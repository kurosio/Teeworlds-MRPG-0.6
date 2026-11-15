#!/bin/sh
# Read-only health check for a single MariaDB replication channel.
# Exit 0 healthy, 1 unhealthy, 2 check/configuration error.
set -u
ROLE=${ROLE:-replica}
MARIADB=${MARIADB:-mariadb}
LAG_WARN_SEC=${LAG_WARN_SEC:-300}
CONF=${WATCHDOG_CONF:-/etc/mrpg/replication_watchdog.conf}
[ ! -r "$CONF" ] || . "$CONF"
case "$ROLE" in primary|replica) ;; *) echo 'Invalid ROLE' >&2; exit 2 ;; esac
case "$LAG_WARN_SEC" in ''|*[!0-9]*|0|0*) echo 'Invalid LAG_WARN_SEC' >&2; exit 2 ;; esac
[ ${#LAG_WARN_SEC} -le 7 ] || { echo 'Invalid LAG_WARN_SEC' >&2; exit 2; }
command -v timeout >/dev/null 2>&1 || { echo 'timeout command required' >&2; exit 2; }
query() { timeout 10 "$MARIADB" --connect-timeout=5 "$@"; }
fail() { echo "CRITICAL: $*" >&2; command -v logger >/dev/null 2>&1 && logger -t mrpg-replication "CRITICAL: $*" || :; exit 1; }
if [ "$ROLE" = primary ]; then
    value=$(query -N -e 'SELECT @@global.log_bin') || fail 'Cannot query primary MariaDB'
    [ "$value" = 1 ] || fail 'Primary binary logging is disabled'
    echo 'OK: primary binary logging enabled'
    exit 0
fi
status=$(query -e 'SHOW ALL SLAVES STATUS\G') || fail 'Cannot query replica MariaDB'
[ -n "$status" ] || fail 'Replica has no replication channel'
# Reject multichannel output: one good channel must not mask another failed one.
count=$(printf '%s\n' "$status" | grep -c '^[[:space:]]*Slave_SQL_Running:' || :)
[ "$count" = 1 ] || fail 'Expected exactly one replication channel; inspect SHOW ALL SLAVES STATUS'
field() { printf '%s\n' "$status" | sed -n "s/^[[:space:]]*$1:[[:space:]]*//p"; }
io=$(field Slave_IO_Running)
sql=$(field Slave_SQL_Running)
lag=$(field Seconds_Behind_Master)
[ "$io" = Yes ] && [ "$sql" = Yes ] || fail "IO=$io SQL=$sql; SQL error: $(field Last_SQL_Error); IO error: $(field Last_IO_Error)"
case "$lag" in ''|*[!0-9]*) fail "Replication lag unknown: $lag" ;; esac
[ ${#lag} -le 9 ] || fail "Replication lag exceeds supported range"
[ "$lag" -lt "$LAG_WARN_SEC" ] || fail "Replication lag ${lag}s >= ${LAG_WARN_SEC}s"
echo "OK: IO=$io SQL=$sql lag=${lag}s"
