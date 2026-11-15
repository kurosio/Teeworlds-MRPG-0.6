#!/bin/sh
# Bounded, read-only diagnostics and explicitly confirmed single-channel restart.
set -u
LC_ALL=C
export LC_ALL
MARIADB=${MARIADB:-mariadb}
usage() { echo "Usage: $0 auto|primary|replica|restart-replica" >&2; exit 2; }
[ "$#" -eq 1 ] || usage
case $1 in auto|primary|replica|restart-replica) ;; *) usage ;; esac
command -v "$MARIADB" >/dev/null 2>&1 || { echo 'MariaDB client not found' >&2; exit 2; }
command -v timeout >/dev/null 2>&1 || { echo 'timeout is required' >&2; exit 2; }
query() { timeout -k 2 10 "$MARIADB" --protocol=socket --connect-timeout=5 "$@"; }
status=$(query -e 'SHOW ALL SLAVES STATUS\G') || { echo 'Cannot query replication status' >&2; exit 2; }
count=$(printf '%s\n' "$status" | grep -c '^[[:space:]]*Slave_SQL_Running:' || :)
if [ "$1" = restart-replica ]; then
    [ "$count" = 1 ] || { echo 'Restart refused: expected exactly one replication channel' >&2; exit 2; }
    names=$(printf '%s\n' "$status" | grep -c '^[[:space:]]*Connection_name:' || :)
    channel=$(printf '%s\n' "$status" | sed -n 's/^[[:space:]]*Connection_name:[[:space:]]*//p')
    [ "$names" = 1 ] && [ -z "$channel" ] || { echo 'Restart refused: channel is named or unknown' >&2; exit 2; }
    echo 'This only starts the default channel; it does not repair divergent data.'
    echo 'First save SHOW ALL SLAVES STATUS, disable replica reads, and diagnose Last_SQL_Error.'
    printf 'Type START to continue: '
    read -r answer || exit 2
    [ "$answer" = START ] || { echo 'Cancelled.'; exit 1; }
    query -e 'START SLAVE;' || exit 2
    status=$(query -e 'SHOW ALL SLAVES STATUS\G') || exit 2
fi
printf '%s\n' '--- Server variables ---'
query -e "SHOW VARIABLES WHERE Variable_name IN ('server_id','log_bin','binlog_format','binlog_row_image','gtid_strict_mode','expire_logs_days','read_only','replicate_wild_ignore_table','replicate_wild_do_table');" || exit 2
if [ "$1" = primary ]; then
    printf '%s\n' '--- Primary binlog status ---'
    query -e 'SHOW MASTER STATUS\G' || exit 2
elif [ "$count" = 0 ]; then
    if [ "$1" = auto ]; then
        echo 'No replication channel found: this may be a primary OR an unconfigured replica.'
        echo 'Confirm the topology before taking any action.'
    else
        echo 'No replication channel configured.' >&2
        exit 2
    fi
else
    printf '%s\n' "--- Replication status ($count channel(s)) ---" "$status"
    printf '%s\n' 'Inspect Last_SQL_Errno/Last_SQL_Error and Last_IO_Errno/Last_IO_Error; see REPLICATION_FIX.md.'
fi
