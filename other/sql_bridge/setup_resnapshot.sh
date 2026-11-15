#!/usr/bin/env bash
# One-time interactive SSH authorization and opt-in configuration, run on replica.
set -Eeuo pipefail
export LC_ALL=C
umask 077
DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
CONFIG_DIR=${MRPG_CONFIG_DIR:-/etc/mrpg}
[[ "$CONFIG_DIR" =~ ^/[A-Za-z0-9_./-]+$ && "/$CONFIG_DIR/" != *"/../"* ]] || { echo "Invalid configuration directory" >&2; exit 2; }
CONFIG=$CONFIG_DIR/replication-guard.conf
KEY=$CONFIG_DIR/resnapshot_ed25519
KNOWN=$CONFIG_DIR/resnapshot_known_hosts
GUARD_SERVICE=${GUARD_SERVICE_NAME:-mmoteeworlds-replication-guard.service}

die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
[[ $EUID -eq 0 ]] || die 'Run this script as root on the replica.'
[[ -t 0 ]] || die 'An interactive terminal is required for identity verification.'
for cmd in mariadb ssh ssh-keygen ssh-keyscan ssh-copy-id python3 timeout awk stat; do
    command -v "$cmd" >/dev/null || die "Missing dependency: $cmd"
done
[[ -f "$DIR/replica_resnapshot.py" ]] || die 'replica_resnapshot.py not found'
if [[ ! -e "$CONFIG_DIR" && ! -L "$CONFIG_DIR" ]]; then install -d -m 700 "$CONFIG_DIR"; fi
[[ -d "$CONFIG_DIR" && ! -L "$CONFIG_DIR" && $(stat -c %u "$CONFIG_DIR") == 0 && $(stat -c %a "$CONFIG_DIR") == 700 ]] || die 'Configuration directory must be root-owned mode 700'
if [[ -e "$CONFIG" || -L "$CONFIG" ]]; then
    [[ -f "$CONFIG" && ! -L "$CONFIG" && $(stat -c %u "$CONFIG") == 0 ]] || die 'Guard config must be a root-owned regular file'
    [[ $(stat -c %a "$CONFIG") == 600 ]] || die 'Guard config must have mode 600'
fi

info=$(timeout -k 2 10 mariadb --protocol=socket -N -e 'SELECT @@global.server_id, @@global.log_bin, @@global.read_only, @@global.event_scheduler') || die 'Cannot query local MariaDB'
IFS=$'\t' read -r rid binlog readonly scheduler <<< "$info"
[[ "$rid" =~ ^[1-9][0-9]{0,9}$ && "$binlog" == 0 && "$readonly" == 1 && ( "$scheduler" == OFF || "$scheduler" == DISABLED || "$scheduler" == 0 ) ]] || die 'Expected replica: unique nonzero ID, log_bin=0, read_only=1, event_scheduler=OFF.'
status=$(timeout -k 2 10 mariadb --protocol=socket -N -e 'SHOW ALL SLAVES STATUS\G') || die 'Cannot read local replication status'
count=$(printf '%s\n' "$status" | grep -c '^[[:space:]]*Slave_SQL_Running:' || :)
[[ $count == 1 ]] || die 'Expected exactly one replica channel.'
master=$(printf '%s\n' "$status" | sed -n 's/^[[:space:]]*Master_Host:[[:space:]]*//p')
[[ "$master" =~ ^[A-Za-z0-9_][A-Za-z0-9_.:-]*$ ]] || die 'Invalid or missing replication Master_Host.'
printf 'Replica ID: %s; replication Master_Host: %s\n' "$rid" "$master"
printf '%s\n' 'Use a dedicated restricted SSH account on the primary, with local MariaDB read/dump access.'
read -r -p "Primary SSH user@host [backup@$master]: " target
target=${target:-backup@$master}
[[ "$target" =~ ^[A-Za-z0-9_][A-Za-z0-9_.-]*@[A-Za-z0-9_][A-Za-z0-9_.-]*$ ]] || die 'Use a valid user@hostname or user@IPv4 address (SSH port 22).'
host=${target#*@}
[[ "$host" == "$master" ]] || die 'SSH hostname must exactly match Master_Host; use the same verified hostname.'

tmp=$(mktemp -d "$CONFIG_DIR/.setup-resnapshot.XXXXXXXX")
trap 'rm -rf -- "$tmp"' EXIT
ssh-keyscan -T 10 -t ed25519 "$host" >"$tmp/known_hosts" 2>/dev/null || die 'Could not retrieve primary SSH host key.'
[[ -s "$tmp/known_hosts" ]] || die 'Empty host key scan.'
ssh-keygen -lf "$tmp/known_hosts" -E sha256 | awk '{print $2}' | sort -u > "$tmp/fingerprints"
[[ $(wc -l < "$tmp/fingerprints") == 1 ]] || die 'Host scan returned multiple distinct host keys; verify the topology manually.'
fingerprint=$(cat "$tmp/fingerprints")
[[ "$fingerprint" == SHA256:* ]] || die 'Could not read host key fingerprint.'
printf '\n%s\n' 'Verify the primary ED25519 host-key fingerprint through a trusted, independent channel.'
printf 'Scanned fingerprint: %s\n' "$fingerprint"
read -r -p 'Enter the independently verified fingerprint: ' confirmed
[[ "$confirmed" == "$fingerprint" ]] || die 'Fingerprint mismatch. Nothing was installed.'

if [[ -e "$KEY" || -L "$KEY" || -e "$KEY.pub" || -L "$KEY.pub" ]]; then
    [[ -f "$KEY" && ! -L "$KEY" && -f "$KEY.pub" && ! -L "$KEY.pub" && $(stat -c %u "$KEY") == 0 && $(stat -c %a "$KEY") == 600 ]] || die 'Existing identity is incomplete or insecure; inspect it before retrying.'
else
    ssh-keygen -q -t ed25519 -N '' -f "$KEY" -C 'mrpg-replica-resnapshot'
fi
ssh_opts=(-o StrictHostKeyChecking=yes -o "UserKnownHostsFile=$tmp/known_hosts" -o GlobalKnownHostsFile=/dev/null -o IdentitiesOnly=yes -o ConnectTimeout=10 -i "$KEY")
if ! ssh "${ssh_opts[@]}" -o BatchMode=yes "$target" true </dev/null; then
    printf '\n%s\n' 'Key is not authorized. The next step modifies authorized_keys on the primary.' \
      'ssh-copy-id may ask for the SSH account password locally; this script does not save it.'
    read -r -p 'Type INSTALL to install this public key on the primary, or Enter to cancel: ' answer
    [[ "$answer" == INSTALL ]] || die 'Authorization cancelled; no database changes were made.'
    ssh-copy-id -i "$KEY.pub" -o StrictHostKeyChecking=yes -o "UserKnownHostsFile=$tmp/known_hosts" -o GlobalKnownHostsFile=/dev/null -o ConnectTimeout=10 "$target" || die 'Public-key installation failed.'
fi
ssh "${ssh_opts[@]}" -o BatchMode=yes "$target" true </dev/null || die 'Key authentication failed.'
primary_id=$(timeout -k 2 30 ssh "${ssh_opts[@]}" -o BatchMode=yes "$target" "mariadb --protocol=socket -N -e 'SELECT @@global.server_id'" </dev/null) || die 'Cannot query the primary database with this SSH account.'
[[ "$primary_id" =~ ^[1-9][0-9]{0,9}$ && "$primary_id" != "$rid" ]] || die 'Invalid primary server_id or IDs are equal.'
# Reuse the exact replica preflight used immediately before resnapshot.
python3 - "$DIR/replica_resnapshot.py" "$rid" "$primary_id" "$master" <<'PY'
import importlib.util, sys
spec = importlib.util.spec_from_file_location('resnapshot', sys.argv[1])
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
try:
    module.check_replica(*sys.argv[2:])
except Exception as exc:
    print('Replica preflight failed:', exc, file=sys.stderr)
    sys.exit(1)
PY
# Test identity, binlog and table engines; this does not test full dump privileges.
primary_probe=$(timeout -k 2 30 ssh "${ssh_opts[@]}" -o BatchMode=yes "$target" \
  "mariadb --protocol=socket -N -e \"SELECT @@global.log_bin, COUNT(*), COALESCE(SUM(ENGINE IS NULL OR ENGINE <> 'InnoDB'),0) FROM information_schema.tables WHERE table_schema='mrpg_shared' AND table_type='BASE TABLE'\" && command -v mariadb-dump" </dev/null) || die 'Primary preflight failed.'
IFS=$'\n' read -r -d '' -a lines <<< "$primary_probe" || :
[[ ${#lines[@]} == 2 ]] || die 'Unexpected primary preflight response.'
IFS=$'\t' read -r source_binlog table_count other_engines <<< "${lines[0]}"
[[ "$source_binlog" == 1 && "$table_count" =~ ^[1-9][0-9]*$ && "$other_engines" == 0 && -n "${lines[1]}" ]] || die 'Primary requires binlog and nonempty InnoDB shared tables.'
printf '\n%s\n' 'WARNING: automatic resnapshot can DROP replica mrpg_shared and change its global GTID.' \
  'It is experimental and must first be tested on a staging replica. It does not merge local changes.'
read -r -p 'Type ENABLE to install configuration and enable automatic resnapshot: ' answer
[[ "$answer" == ENABLE ]] || die 'Not enabled. The key may already have been installed on the primary.'
# Install the verified host key and atomically replace the local root-only config.
install -m 600 "$tmp/known_hosts" "$tmp/final-hosts"
mv -f "$tmp/final-hosts" "$KNOWN"
new=$(mktemp "$CONFIG_DIR/.guard-conf.XXXXXXXX")
if [[ -e "$CONFIG" ]]; then
    backup=$(mktemp "$CONFIG_DIR/replication-guard.conf.backup.XXXXXXXX")
    cp -p "$CONFIG" "$backup"
    awk '!/^[[:space:]]*(export[[:space:]]+)?(PRIMARY_IDS|REPLICA_IDS|AUTO_RESNAPSHOT|RESYNC_REPLICA_ID|RESYNC_PRIMARY_ID|RESYNC_MASTER_HOST|RESYNC_SSH_TARGET|RESYNC_SSH_IDENTITY|RESYNC_SSH_KNOWN_HOSTS)[[:space:]]*=/' "$CONFIG" > "$new"
    printf 'Previous configuration saved to %s\n' "$backup"
fi
{
    printf '\nPRIMARY_IDS=\nREPLICA_IDS=%s\nAUTO_RESNAPSHOT=1\n' "$rid"
    printf 'RESYNC_REPLICA_ID=%s\nRESYNC_PRIMARY_ID=%s\n' "$rid" "$primary_id"
    printf 'RESYNC_MASTER_HOST=%s\nRESYNC_SSH_TARGET=%s\n' "$master" "$target"
    printf 'RESYNC_SSH_IDENTITY=%s\nRESYNC_SSH_KNOWN_HOSTS=%s\n' "$KEY" "$KNOWN"
} >> "$new"
chmod 600 "$new"
mv -f "$new" "$CONFIG"
if command -v systemctl >/dev/null && systemctl is-active --quiet "$GUARD_SERVICE"; then
    systemctl restart "$GUARD_SERVICE" || die 'Configuration saved, but guard restart failed.'
    printf 'Configuration installed and running guard restarted: %s\n' "$GUARD_SERVICE"
else
    printf 'Configuration installed; guard is not active. Start the game service and check %s.\n' "$GUARD_SERVICE"
fi
printf '%s\n' 'Verify the guard journal and perform a full restore drill before production.'
