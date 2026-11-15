#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/mariadb" <<'MOCK'
#!/bin/sh
case "$*" in
 *SHOW*) printf '%s\n' "${STATUS:-}" ;;
 *log_bin*) echo "${BINLOG:-1}" ;;
 *) exit 1 ;;
esac
MOCK
chmod +x "$tmp/mariadb"
export MARIADB="$tmp/mariadb" WATCHDOG_CONF="$tmp/no-conf"
check() { expected=$1; shift; set +e; "$base/replication_watchdog.sh" > /dev/null 2>&1; actual=$?; set -e; [ "$actual" = "$expected" ] || { echo "expected $expected got $actual" >&2; exit 1; }; }
ROLE=replica; export ROLE
STATUS='Slave_IO_Running: Yes
Slave_SQL_Running: Yes
Seconds_Behind_Master: 0'; export STATUS
check 0
STATUS='Slave_IO_Running: Yes
Slave_SQL_Running: No
Seconds_Behind_Master: NULL
Last_SQL_Error: Duplicate entry'; export STATUS
check 1
STATUS=''; export STATUS; check 1
STATUS='Slave_IO_Running: Yes
Slave_SQL_Running: Yes
Seconds_Behind_Master: NULL'; export STATUS; check 1
STATUS='Slave_IO_Running: Yes
Slave_SQL_Running: Yes
Seconds_Behind_Master: 301'; export STATUS; check 1
STATUS='Slave_SQL_Running: Yes
Slave_SQL_Running: No'; export STATUS; check 1
ROLE=primary; export ROLE; check 0
BINLOG=0; export BINLOG; check 1
ROLE=invalid; export ROLE; check 2
echo 'watchdog tests passed'
