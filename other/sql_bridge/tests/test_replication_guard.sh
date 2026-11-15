#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/client" <<'MOCK'
#!/bin/sh
case " $* " in *' --protocol=socket '*) ;; *) echo 'socket protocol required' >&2; exit 3 ;; esac
case "$*" in
 *server_id*) [ "${FAIL_ID:-0}" = 0 ] || exit 1; echo "${ID:-2}" ;;
 *log_bin*) echo "${BINLOG:-1}" ;;
 *'SHOW ALL SLAVES STATUS'*) printf '%s\n' "${STATUS:-}" ;;
 *START*) echo "$*" >> "$CALLS" ;;
 *) exit 1 ;;
esac
MOCK
chmod +x "$tmp/client"
export MARIADB="$tmp/client" GUARD_CONF="$tmp/missing" CALLS="$tmp/calls" PRIMARY_IDS=1 REPLICA_IDS=2
run_once() {
    expected=$1
    set +e
    sh "$base/replication_guard.sh" --once 2>"$tmp/out"
    rc=$?
    set -e
    [ "$rc" = "$expected" ] || { echo "expected $expected, got $rc: $(cat "$tmp/out")" >&2; exit 1; }
}
run_loop() {
    set +e
    timeout 2 sh "$base/replication_guard.sh" 2>"$tmp/out"
    rc=$?
    set -e
    [ "$rc" = 124 ] || { echo "unexpected loop exit: $rc" >&2; exit 1; }
}
STATUS='Slave_IO_Running: Yes
Slave_SQL_Running: Yes
Seconds_Behind_Master: 0'; export STATUS
run_once 0; grep -q 'OK: replica' "$tmp/out"
STATUS='Slave_IO_Running: Yes
Slave_SQL_Running: No
Last_SQL_Errno: 1062'; export STATUS
run_once 1; grep -q 'SQL=No' "$tmp/out"; [ ! -e "$CALLS" ]
STATUS='Connection_name:
Slave_IO_Running: No
Slave_SQL_Running: Yes
Last_IO_Errno: 2003'; export STATUS
run_once 1; [ ! -e "$CALLS" ]
run_loop; grep -q 'START SLAVE IO_THREAD' "$CALLS"; rm "$CALLS"
# Even persistent network errors have a bounded retry budget.
INTERVAL=1 IO_RETRY_SECONDS=1 MAX_IO_RETRIES=2; export INTERVAL IO_RETRY_SECONDS MAX_IO_RETRIES
set +e
timeout 5 sh "$base/replication_guard.sh" 2>"$tmp/out"
rc=$?
set -e
[ "$rc" = 124 ] && [ "$(wc -l < "$CALLS")" -eq 2 ]
grep -q 'retry budget exhausted' "$tmp/out"
rm "$CALLS"
unset INTERVAL IO_RETRY_SECONDS MAX_IO_RETRIES
STATUS='Connection_name: named
Slave_IO_Running: No
Slave_SQL_Running: Yes
Last_IO_Errno: 2003'; export STATUS
run_loop; [ ! -e "$CALLS" ]
STATUS='Slave_IO_Running: No
Slave_SQL_Running: Yes
Last_IO_Errno: 1236'; export STATUS
run_loop; [ ! -e "$CALLS" ]
STATUS='Slave_SQL_Running: Yes
Slave_SQL_Running: No'; export STATUS
run_once 1; grep -q 'found 2' "$tmp/out"
STATUS='Slave_IO_Running: Yes
Slave_SQL_Running: Yes
Seconds_Behind_Master: NULL'; export STATUS
run_once 1
ID=5; export ID; run_once 1; grep -q 'not mapped' "$tmp/out"
ID=1; STATUS='unrecognized output'; export ID STATUS; run_once 1; grep -q 'unrecognized replication status' "$tmp/out"
STATUS=''; export STATUS; run_once 0; grep -q 'OK: primary' "$tmp/out"
FAIL_ID=1; export FAIL_ID; run_once 1
unset FAIL_ID
INTERVAL=0; export INTERVAL; run_once 2
printf '%s\n' 'guard tests passed'
