#!/bin/sh
# Isolated root-only mock integration test. Never contacts SSH or MariaDB.
set -eu
[ "$(id -u)" = 0 ] || { echo 'SKIP: mocked setup test requires root'; exit 0; }
command -v script >/dev/null || { echo 'SKIP: script (util-linux) not available'; exit 0; }
base=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -m 700 "$tmp/bin"
cat > "$tmp/bin/mariadb" <<'MOCK'
#!/bin/sh
case "$*" in
 *'SHOW ALL SLAVES STATUS'*) cat <<'STATUS'
Connection_name:
Master_Host: db-primary
Master_Server_Id: 1
Slave_IO_Running: No
Slave_SQL_Running: No
Replicate_Wild_Do_Table: mrpg_shared.%
STATUS
 ;;
 *server_id*) printf '2\t0\t1\tOFF\n' ;;
 *) exit 1 ;;
esac
MOCK
cat > "$tmp/bin/ssh-keyscan" <<'MOCK'
#!/bin/sh
printf 'db-primary ssh-ed25519 dummykey\n'
MOCK
cat > "$tmp/bin/ssh-keygen" <<'MOCK'
#!/bin/sh
case "$1" in
 -lf) printf '256 SHA256:testfingerprint db-primary (ED25519)\n' ;;
 *) while [ "$#" -gt 0 ]; do if [ "$1" = -f ]; then shift; key=$1; break; fi; shift; done
    printf 'private\n' > "$key"; chmod 600 "$key"; printf 'public\n' > "$key.pub" ;;
esac
MOCK
cat > "$tmp/bin/ssh" <<'MOCK'
#!/bin/sh
case "$*" in
 *'SELECT @@global.server_id'*) echo 1 ;;
 *'SELECT @@global.log_bin'*) printf '1\t2\t0\n/usr/bin/mariadb-dump\n' ;;
 *' true') exit 0 ;;
 *) exit 1 ;;
esac
MOCK
cat > "$tmp/bin/ssh-copy-id" <<'MOCK'
#!/bin/sh
exit 99
MOCK
cat > "$tmp/bin/systemctl" <<'MOCK'
#!/bin/sh
exit 3
MOCK
chmod 755 "$tmp/bin"/*
# Wrong fingerprints must abort before generating or installing anything.
if printf 'backup@db-primary\nSHA256:wrong\n' | script -q -e -c "env PATH='$tmp/bin':\"\$PATH\" MRPG_CONFIG_DIR='$tmp/bad' bash '$base/setup_resnapshot.sh'" /dev/null > "$tmp/bad-output"; then
    echo 'Expected fingerprint mismatch to fail' >&2; exit 1
fi
[ ! -e "$tmp/bad/resnapshot_ed25519" ]
[ ! -e "$tmp/bad/replication-guard.conf" ]
# The target and fingerprint are verified by fake tools; no network is used.
printf 'backup@db-primary\nSHA256:testfingerprint\nENABLE\n' | script -q -e -c "env PATH='$tmp/bin':\"\$PATH\" MRPG_CONFIG_DIR='$tmp/config' bash '$base/setup_resnapshot.sh'" /dev/null > "$tmp/output" || { cat "$tmp/output"; exit 1; }
grep -q '^AUTO_RESNAPSHOT=1$' "$tmp/config/replication-guard.conf"
grep -q '^RESYNC_PRIMARY_ID=1$' "$tmp/config/replication-guard.conf"
grep -q '^RESYNC_REPLICA_ID=2$' "$tmp/config/replication-guard.conf"
[ "$(stat -c %a "$tmp/config/replication-guard.conf")" = 600 ]
[ -f "$tmp/config/resnapshot_known_hosts" ]
printf 'mocked setup test passed\n'
