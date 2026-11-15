#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/client" <<'MOCK'
#!/bin/sh
case " $* " in *' --protocol=socket '*) ;; *) echo 'socket protocol required' >&2; exit 3 ;; esac
case "$*" in
 *'SHOW ALL SLAVES STATUS'*) printf '%s\n' "${STATUS:-}" ;;
 *'START SLAVE'*) printf 'start\n' >> "$CALLS" ;;
 *'SHOW VARIABLES'*|*'SHOW MASTER STATUS'*) echo ok ;;
 *) exit 1 ;;
esac
MOCK
chmod +x "$tmp/client"
export MARIADB="$tmp/client" CALLS="$tmp/calls"
STATUS='Connection_name:
Slave_SQL_Running: No
Last_SQL_Errno: 1062'; export STATUS
sh "$base/fix_replication.sh" auto >"$tmp/out"
grep -q 'Last_SQL_Errno: 1062' "$tmp/out"
[ ! -e "$CALLS" ]
printf 'START\n' | sh "$base/fix_replication.sh" restart-replica >"$tmp/out"
[ -f "$CALLS" ]; rm "$CALLS"
STATUS='Connection_name: named
Slave_SQL_Running: No'; export STATUS
if printf 'START\n' | sh "$base/fix_replication.sh" restart-replica >"$tmp/out" 2>&1; then
    echo 'named-channel restart unexpectedly accepted' >&2; exit 1
fi
[ ! -e "$CALLS" ]
STATUS=''; export STATUS
sh "$base/fix_replication.sh" auto >"$tmp/out"
grep -q 'unconfigured replica' "$tmp/out"
if sh "$base/fix_replication.sh" replica >"$tmp/out" 2>&1; then exit 1; fi
echo 'fix diagnostics tests passed'
