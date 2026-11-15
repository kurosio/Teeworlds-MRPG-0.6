#!/bin/sh
# MariaDB single-channel guard. No event skipping, GTID changes or resnapshot.
set -u
LC_ALL=C
export LC_ALL
CONF=${GUARD_CONF:-/etc/mrpg/replication-guard.conf}
PRIMARY_IDS=${PRIMARY_IDS:-}
REPLICA_IDS=${REPLICA_IDS:-}
INTERVAL=${INTERVAL:-15}
MAX_LAG=${MAX_LAG:-300}
IO_RETRY_SECONDS=${IO_RETRY_SECONDS:-120}
REPORT_REPEAT_SECONDS=${REPORT_REPEAT_SECONDS:-300}
AUTO_START_IO=${AUTO_START_IO:-1}
MAX_IO_RETRIES=${MAX_IO_RETRIES:-5}
MARIADB=${MARIADB:-mariadb}
[ ! -r "$CONF" ] || . "$CONF"
for val in "$INTERVAL" "$MAX_LAG" "$IO_RETRY_SECONDS" "$REPORT_REPEAT_SECONDS" "$MAX_IO_RETRIES"; do
    case $val in ''|*[!0-9]*|0|0*) echo 'Invalid guard interval/threshold' >&2; exit 2 ;; esac
    [ ${#val} -le 7 ] || { echo 'Guard interval/threshold too large' >&2; exit 2; }
done
case $AUTO_START_IO in 0|1) ;; *) echo 'Invalid AUTO_START_IO' >&2; exit 2 ;; esac
# Limit execution so an unreachable server cannot block monitoring forever.
# Never issue control statements to a server selected by a client config's host.
# Root's client configuration may still provide credentials and a local socket.
query() { timeout -k 2 10 "$MARIADB" --protocol=socket --connect-timeout=5 "$@"; }
contains_id() { case ",$1," in *",$2,"*) return 0 ;; *) return 1 ;; esac; }
field() { printf '%s\n' "$status" | sed -n "s/^[[:space:]]*$1:[[:space:]]*//p"; }
last_message=''
last_report=0
report() {
    now_report=$(date +%s)
    case $now_report in ''|*[!0-9]*) now_report=0 ;; esac
    if [ "$last_message" != "$1" ] || [ "$now_report" -lt "$last_report" ] ||
       [ $((now_report - last_report)) -ge "$REPORT_REPEAT_SECONDS" ]; then
        printf '%s %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" "$1" >&2
        last_message=$1
        last_report=$now_report
    fi
}
last_retry=0
retry_count=0
check() {
    id=$(query -N -e 'SELECT @@global.server_id' 2>/dev/null) || { report 'CRITICAL: MariaDB unavailable or access denied'; return 1; }
    case $id in ''|*[!0-9]*) report 'CRITICAL: invalid server_id'; return 1 ;; esac
    role=unknown
    if contains_id "$PRIMARY_IDS" "$id"; then role=primary; fi
    if contains_id "$REPLICA_IDS" "$id"; then
        [ "$role" = unknown ] || { report "CRITICAL: server_id=$id is assigned to both roles"; return 1; }
        role=replica
    fi
    [ "$role" != unknown ] || { report "CRITICAL: server_id=$id not mapped; configure PRIMARY_IDS/REPLICA_IDS"; return 1; }
    status=$(query -e 'SHOW ALL SLAVES STATUS\G' 2>/dev/null) || { report "CRITICAL: cannot query replication status on $id"; return 1; }
    count=$(printf '%s\n' "$status" | grep -c '^[[:space:]]*Slave_SQL_Running:' || :)
    if [ "$role" = primary ]; then
        [ "$count" = 0 ] || { report "CRITICAL: primary server_id=$id has a replication channel; topology requires manual review"; return 1; }
        [ -z "$status" ] || { report "CRITICAL: unrecognized replication status on primary server_id=$id"; return 1; }
        binlog=$(query -N -e 'SELECT @@global.log_bin' 2>/dev/null) || { report 'CRITICAL: cannot check primary binlog'; return 1; }
        [ "$binlog" = 1 ] || { report 'CRITICAL: primary binlog disabled'; return 1; }
        report "OK: primary server_id=$id binlog enabled"
        return 0
    fi
    [ "$count" = 1 ] || { report "CRITICAL: replica server_id=$id expected one channel, found $count"; return 1; }
    channel_fields=$(printf '%s\n' "$status" | grep -c '^[[:space:]]*Connection_name:' || :)
    channel=$(field Connection_name)
    io=$(field Slave_IO_Running); sql=$(field Slave_SQL_Running)
    lag=$(field Seconds_Behind_Master); io_errno=$(field Last_IO_Errno)
    sql_errno=$(field Last_SQL_Errno)
    if [ "$sql" != Yes ]; then
        case $sql_errno in
            1062|1032) hint='data mismatch; preserve replica and resnapshot after review' ;;
            1146) hint='missing table or schema mismatch; check filters and schema' ;;
            1205|1213) hint='lock conflict; inspect transaction and lock contention' ;;
            *) hint='inspect Last_SQL_Error; no events will be skipped' ;;
        esac
        report "CRITICAL: replica server_id=$id SQL=$sql errno=$sql_errno; $hint"
        return 1
    fi
    if [ "$io" != Yes ]; then
        case $io_errno in
            1236) hint='binlog or GTID unavailable; inspect Last_IO_Error and retention' ;;
            1045) hint='replication authentication failed; inspect credentials and grants' ;;
            2003|2006|2013|2055) hint='network failure; inspect primary connectivity' ;;
            *) hint='inspect Last_IO_Error' ;;
        esac
        report "CRITICAL: replica server_id=$id IO=$io errno=$io_errno; $hint"
        # Connecting already retries by itself. Only retry documented network
        # failures, never authentication, GTID, or SQL/data errors.
        case $io:$io_errno in
            No:2003|No:2006|No:2013|No:2055)
                now=$(date +%s)
                case $now in ''|*[!0-9]*) now=0 ;; esac
                if [ "$channel_fields" = 1 ] && [ -z "$channel" ] && [ "${CHECK_ONLY:-0}" != 1 ] && [ "$AUTO_START_IO" = 1 ] &&
                   [ "$now" -gt 0 ] && [ "$retry_count" -lt "$MAX_IO_RETRIES" ] &&
                   { [ "$last_retry" -eq 0 ] || [ "$now" -lt "$last_retry" ] || [ $((now - last_retry)) -ge "$IO_RETRY_SECONDS" ]; }; then
                    last_retry=$now
                    retry_count=$((retry_count + 1))
                    if query -e 'START SLAVE IO_THREAD' >/dev/null 2>&1; then
                        echo "Attempted START SLAVE IO_THREAD ($retry_count/$MAX_IO_RETRIES); verifying on next poll" >&2
                    else
                        echo "START SLAVE IO_THREAD failed ($retry_count/$MAX_IO_RETRIES); check privileges and error log" >&2
                    fi
                    if [ "$retry_count" -ge "$MAX_IO_RETRIES" ]; then
                        echo 'IO retry budget exhausted; investigate. Reset after a healthy check or service restart.' >&2
                    fi
                fi ;;
        esac
        return 1
    fi
    case $lag in ''|*[!0-9]*) report "CRITICAL: replica server_id=$id lag unknown"; return 1 ;; esac
    [ ${#lag} -le 9 ] || { report "CRITICAL: replica server_id=$id lag exceeds supported range"; return 1; }
    [ "$lag" -lt "$MAX_LAG" ] || { report "CRITICAL: replica server_id=$id lag=${lag}s (limit ${MAX_LAG}s)"; return 1; }
    retry_count=0
    last_retry=0
    report "OK: replica server_id=$id IO=Yes SQL=Yes lag below ${MAX_LAG}s"
}
command -v timeout >/dev/null 2>&1 || { echo 'timeout command required' >&2; exit 2; }
if [ "$#" -eq 1 ] && [ "$1" = --once ]; then CHECK_ONLY=1; check; exit $?; fi
[ "$#" = 0 ] || { echo "Usage: $0 [--once]" >&2; exit 2; }
while :; do check || :; sleep "$INTERVAL"; done
