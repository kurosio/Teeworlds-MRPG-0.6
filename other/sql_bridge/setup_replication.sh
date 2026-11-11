#!/bin/sh
# =============================================================================
# setup_replication.sh — interactive setup of the MariaDB primary (master)
# and read replicas for Teeworlds-MRPG.
#
# Automates steps 1-2 of other/sql_bridge/README.md:
#   1) master  — Step 1 (MariaDB, binary log, mrpg_shared database, dump
#                import, mrpg_replica / mrpg_bridge / mrpg_user accounts);
#   2) replica — Step 2 (MariaDB, read-only shared replica, snapshot from
#                the primary, CHANGE MASTER, local mrpg_bridge account, plus
#                a writable mrpg_local_<location> database and direct local
#                game user);
#   3) status  — replication state on this host;
#   4) reset   — remove the configuration this script creates (stop/reset
#                replication, move configs aside, optionally drop the
#                accounts and databases it added).
#
# The script is fully interactive — it accepts no arguments. Answer the
# questions one by one (Enter accepts the [default]); passwords are not
# echoed and an empty answer generates a random one. After the plan is
# shown you can apply it [Y], print the commands without running them
# (dry-run) [t], or abort [n].
#
# Target: Debian/Ubuntu, MariaDB 10.6+. Idempotent: old configs are backed
# up next to the new ones, a dump is not re-imported into a non-empty
# database without asking, accounts are created with CREATE USER IF NOT EXISTS.
#
# Generated passwords are saved to /root/mrpg-replication-<date>.cred (0600).
# Passwords must not contain ' " or \
#
# Usage: sudo sh setup_replication.sh
# =============================================================================
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" 2>/dev/null && pwd) || SCRIPT_DIR=.
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." 2>/dev/null && pwd) || REPO_ROOT="$SCRIPT_DIR"

MASTER_CONF=/etc/mysql/mariadb.conf.d/61-mrpg-replication.cnf
REPLICA_CONF=/etc/mysql/mariadb.conf.d/61-mrpg-replica.cnf

# ---------- state ----------
DRY_RUN=0
SHARED_DB=mrpg_shared
IS_ROOT=1
MARIADB=mariadb
# SCRIPT_DIR is computed at the top (with fallbacks) — used as the default for the bridge steps
# cross-step state (the final "ready to work" verification reads these)
GAME_SHARED_LOGIN=""
GAME_SHARED_PW=""
GAME_LOCAL_LOGIN=""
GAME_LOCAL_PW=""
GAME_LOCAL_DB=""
BRIDGE_CFG_USER=""
BRIDGE_CFG_PASSWORD=""
BRIDGE_CFG_HOST=""
BRIDGE_CFG_PORT=""
BRIDGE_CFG_DB=""
BRIDGE_LISTEN_HOST=""
BRIDGE_LISTEN_PORT=""
BRIDGE_SERVICE_UP=0

# ---------- logging ----------
info() { printf '%s\n' "$*"; }
ok()   { printf '✅ %s\n' "$*"; }
warn() { printf '⚠️  %s\n' "$*" >&2; }
die()  { printf '❌ %s\n' "$*" >&2; exit 1; }

# ---------- input ----------
REPLY=""

# prompt <question> <default> — answer in $REPLY
prompt() {
    _p_def=${2:-}
    if [ -n "$_p_def" ]; then
        printf '%s [%s]: ' "$1" "$_p_def"
    else
        printf '%s: ' "$1"
    fi
    if ! read -r _p_ans; then
        _p_ans=""
        [ -z "$_p_def" ] && die "input closed (EOF) — a value is required: $1"
    fi
    [ -z "$_p_ans" ] && _p_ans=$_p_def
    REPLY=$_p_ans
}

# prompt_required <question> [default] — asks until a non-empty answer is given
prompt_required() {
    while :; do
        prompt "$1" "${2:-}"
        [ -n "$REPLY" ] && return 0
        info "A value is required, please repeat."
    done
}

# prompt_secret <question> — hidden input; empty answer = autogenerate, result in $REPLY
prompt_secret() {
    printf '%s' "$1"
    if [ -t 0 ]; then
        stty -echo
        read -r _p_ans || _p_ans=""
        stty echo
        printf '\n'
    else
        read -r _p_ans || _p_ans=""
    fi
    REPLY=$_p_ans
}

# ask_yn <question> <y|n> — y/n in $REPLY
ask_yn() {
    _q=$1
    _d=${2:-y}
    if [ "$_d" = n ]; then _h='[y/N]'; else _h='[Y/n]'; fi
    while :; do
        printf '%s %s: ' "$_q" "$_h"
        read -r _a || _a=$_d
        [ -z "$_a" ] && _a=$_d
        case "$_a" in
            y|Y|yes|YES|Yes) REPLY=y; return 0 ;;
            n|N|no|NO|No)    REPLY=n; return 0 ;;
            *) info "Please answer y or n." ;;
        esac
    done
}

# confirm_plan [y|n] — asks what to do with the plan that was just shown
confirm_plan() {
    _cd=${1:-y}
    if [ "$_cd" = n ]; then _hint='[y] apply / [t] dry-run (print only) / [N] abort'
    else _hint='[Y] apply / [t] dry-run (print only) / [n] abort'; fi
    while :; do
        printf 'Apply the changes shown above? %s: ' "$_hint"
        # EOF must never apply changes — always abort
        read -r _a || die "input closed (EOF) — aborting for safety."
        [ -z "$_a" ] && _a=$_cd
        case "$_a" in
            y|Y|yes|YES|Yes)
                if [ "$IS_ROOT" != 1 ]; then
                    die "root is required — rerun: sudo sh $0"
                fi
                return 0 ;;
            t|T|dry|dry-run)
                DRY_RUN=1
                info "Dry-run mode: commands are only printed, nothing is changed."
                return 0 ;;
            n|N|no|NO|No) die "Aborted by the user." ;;
            *) info "Please answer y, t or n." ;;
        esac
    done
}

# confirm_typed_yes <what will happen> — requires typing "yes" (for destructive actions)
confirm_typed_yes() {
    info "$1"
    printf 'This cannot be undone. Type "yes" to confirm: '
    read -r _a || _a=""
    [ "$_a" = yes ] || die "Aborted."
}

# ---------- validation ----------
is_number() { case "$1" in ''|*[!0-9]*) return 1 ;; *) return 0 ;; esac; }

check_ident() { case "$1" in ''|*[!a-zA-Z0-9_]*) die "invalid database/identifier name: '$1'" ;; esac; }
check_password() { case "$1" in *\'*|*\\*|*\"*) die "password must not contain ', \" or \\" ;; esac; }
check_host() { case "$1" in ''|*\'*|*\\*|*\;*|*\|*|*\`*|*\$*|*\ *) die "invalid characters in the address: '$1'" ;; esac; }
check_netmask() { case "$1" in ''|*\'*|*\\*|*\;*|*\ *|*[!a-zA-Z0-9_.%:-]*) die "invalid host mask: '$1'" ;; esac; }
check_cidr() { case "$1" in ''|*[!0-9./]*) die "invalid CIDR network (expected e.g. 10.0.0.0/24): '$1'" ;; esac; }

ask_server_id() { # $1 — default
    while :; do
        prompt "server_id of this host (must be unique in the cluster)" "$1"
        if is_number "$REPLY" && [ "$REPLY" -ge 1 ]; then SERVER_ID=$REPLY; return 0; fi
        info "server_id must be a number >= 1."
    done
}

ask_shared_db() {
    while :; do
        prompt "Name of the shared database" "mrpg_shared"
        if ( check_ident "$REPLY" 2>/dev/null ); then SHARED_DB=$REPLY; return 0; fi
        info "Database name: only letters, digits and _."
    done
}

# ---------- helpers ----------
run() { # run a command (or print it in dry-run mode)
    if [ "$DRY_RUN" = 1 ]; then
        printf '+'
        for _arg in "$@"; do printf ' %s' "$_arg"; done
        printf '\n'
    else
        "$@"
    fi
}

sql_exec() { # $1 — one-line SQL; a live failure stops the script with a clear error
    if [ "$DRY_RUN" = 1 ]; then
        printf "+ %s -u root -e '%s'\n" "$MARIADB" "$1"
    else
        "$MARIADB" -u root -e "$1" || die "SQL failed: $1"
    fi
}

sql_stdin() { # $1 (optional) — die with this message if the batch fails; when omitted, the caller gets the exit status back
    _sql_msg=${1:-}
    if [ "$DRY_RUN" = 1 ]; then
        printf '+ %s -u root <<EOF\n' "$MARIADB"
        cat
        printf 'EOF\n'
        return 0
    fi
    if [ -n "$_sql_msg" ]; then
        if ! "$MARIADB" -u root; then die "$_sql_msg"; fi
    else
        "$MARIADB" -u root
    fi
}

write_conf() { # $1 — config path; content via stdin
    _path=$1
    if [ "$DRY_RUN" = 1 ]; then
        info "+ write $_path (previous file, if any, is saved to *.bak.<date>):"
        sed 's/^/    | /'
        return 0
    fi
    _bak=""
    if [ -f "$_path" ]; then
        _bak="$_path.bak.$(date +%Y%m%d_%H%M%S)"
        if ! cp -a "$_path" "$_bak"; then
            die "could not back up $_path to $_bak — NOT overwriting it (fix the backup path/permissions first)"
        fi
    fi
    _tmp="$_path.new.$$"
    if ! cat > "$_tmp"; then
        rm -f "$_tmp" || true
        die "could not write $_path — the previous config was left untouched${_bak:+ (backup: $_bak)}"
    fi
    if ! mv "$_tmp" "$_path"; then
        rm -f "$_tmp" || true
        die "could not replace $_path${_bak:+ — restore it manually: cp -a $_bak $_path}"
    fi
    [ -n "$_bak" ] && info "Previous config saved to: $_bak"
    ok "Written $_path"
}

remove_conf() { # $1 — config path; moves it aside instead of deleting
    _path=$1
    if [ ! -f "$_path" ]; then
        info "Not present: $_path"
        return 0
    fi
    _bak="$_path.removed.$(date +%Y%m%d_%H%M%S)"
    if [ "$DRY_RUN" = 1 ]; then
        info "+ mv $_path $_bak"
    else
        if ! mv "$_path" "$_bak"; then
            die "could not move $_path to $_bak — fix permissions; NOT continuing (the config is still active and MariaDB will keep using it)"
        fi
        ok "Moved $_path → $_bak"
    fi
}

detect_bins() {
    if command -v mariadb >/dev/null 2>&1; then MARIADB=mariadb
    elif command -v mysql >/dev/null 2>&1; then MARIADB=mysql
    elif [ "$DRY_RUN" != 1 ]; then die "MariaDB/MySQL client not found"
    fi
}

mariadb_preinstalled() {
    command -v mariadbd >/dev/null 2>&1 && return 0
    command -v mysqld   >/dev/null 2>&1 && return 0
    command -v dpkg     >/dev/null 2>&1 && dpkg -s mariadb-server >/dev/null 2>&1
}

install_mariadb() {
    if [ "$DRY_RUN" != 1 ]; then
        command -v apt-get >/dev/null 2>&1 || die "this script targets Debian/Ubuntu (apt-get not found)"
        if mariadb_preinstalled; then
            ask_yn "MariaDB already appears to be installed — skip package installation?" y
            if [ "$REPLY" = y ]; then
                info "Skipping package installation."
                return 0
            fi
        fi
    fi
    if ! run apt-get update; then
        die "apt-get update failed — check the network and the APT sources, then rerun."
    fi
    if ! run env DEBIAN_FRONTEND=noninteractive apt-get install -y mariadb-server mariadb-client; then
        die "apt-get install mariadb-server failed — see the errors above (fix APT sources/mirror or install MariaDB manually), then rerun."
    fi
}

mariadb_health() {
    [ "$DRY_RUN" = 1 ] && return 0
    if command -v systemctl >/dev/null 2>&1; then
        if systemctl is-active --quiet mariadb; then
            ok "MariaDB is active"
        else
            warn "MariaDB is NOT active after the restart — check: journalctl -xeu mariadb"
        fi
    fi
}

restart_mariadb() { # $1 — what was just changed (for the error message)
    _what=${1:-environment change}
    if command -v systemctl >/dev/null 2>&1; then
        _rc=0
        run systemctl restart mariadb || _rc=$?
        if [ "$DRY_RUN" != 1 ] && [ "$_rc" != 0 ]; then
            die "MariaDB failed to restart ($_what) — check: systemctl status mariadb; journalctl -xeu mariadb"
        fi
    elif command -v service >/dev/null 2>&1; then
        _rc=0
        run service mariadb restart || _rc=$?
        if [ "$DRY_RUN" != 1 ] && [ "$_rc" != 0 ]; then
            die "MariaDB failed to restart ($_what) — check: service mariadb status; tail -100 /var/log/mysql/error.log"
        fi
    else
        warn "could not detect the init system — restart MariaDB manually"
        return 0
    fi
    mariadb_health
}

gen_password() {
    if command -v openssl >/dev/null 2>&1; then
        openssl rand -hex 18
    else
        head -c 18 /dev/urandom | od -An -tx1 | tr -d ' \n'
    fi
}

# local_db_names / local_user_names — discover what already exists on THIS host
# (used to offer sensible defaults for the local-side answers instead of guessing)
local_db_names() {
    "$MARIADB" -u root -N -e "SELECT schema_name FROM information_schema.schemata WHERE schema_name LIKE 'mrpg_local%' ORDER BY 1" 2>/dev/null || true
}
local_user_names() {
    "$MARIADB" -u root -N -e "SELECT DISTINCT user FROM mysql.user WHERE user LIKE 'mrpg_local%' ORDER BY 1" 2>/dev/null || true
}

table_count() { # $1 — database; prints the number of tables
    if [ "$DRY_RUN" = 1 ]; then echo 0; return 0; fi
    "$MARIADB" -u root -N -e \
        "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='$1'" 2>/dev/null || echo 0
}

import_dump() { # $1 — database, $2 — dump file, $3 — allow recreating the db (1 = yes; default 0)
    _allow_recreate=${3:-0}
    _tables=$(table_count "$1") || die "could not read the table count of $1 — is MariaDB up and root access working?"
    if [ "$_tables" != 0 ]; then
        warn "database $1 already has tables ($_tables)"
        if [ "$_allow_recreate" = 1 ]; then
            ask_yn "Drop and recreate $1 from scratch using $2? (ALL ITS DATA WILL BE DELETED)" n
            if [ "$REPLY" = y ]; then
                if [ "$DRY_RUN" != 1 ]; then
                    confirm_typed_yes "Database $1 will be dropped and re-imported from $2."
                fi
                sql_drop_db "$1"
                sql_exec "CREATE DATABASE IF NOT EXISTS $1 CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
            else
                info "Import skipped — database kept as is."
                return 0
            fi
        else
            ask_yn "Import the dump anyway? (rows may be duplicated / 1050 errors)" n
            [ "$REPLY" = n ] && { info "Import skipped."; return 0; }
        fi
    fi
    if [ "$DRY_RUN" = 1 ]; then
        info "+ $MARIADB -u root $1 < $2"
        return 0
    fi
    if ! "$MARIADB" -u root "$1" < "$2"; then
        warn "import of $2 into $1 failed"
        if [ "$_allow_recreate" = 1 ]; then
            ask_yn "Drop and recreate $1 from scratch and import again? (ALL ITS DATA WILL BE DELETED)" n
            if [ "$REPLY" = y ]; then
                if [ "$DRY_RUN" != 1 ]; then
                    confirm_typed_yes "Database $1 will be dropped and re-imported from $2."
                fi
                sql_drop_db "$1"
                sql_exec "CREATE DATABASE IF NOT EXISTS $1 CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
                if "$MARIADB" -u root "$1" < "$2"; then
                    ok "Database $1 recreated and imported from $2"
                    return 0
                fi
                die "import failed again — see the errors above"
            fi
        fi
        die "import failed — fix the errors above or drop $1 manually, then rerun"
    fi
    ok "Dump $2 imported into $1"
}

# pre_import_clean_shared_db — the replica's copy of the shared database is
# ALWAYS stale before a fresh snapshot import: drop and re-create it so the
# import lands on a clean DB and the dump's GTID becomes the exact replication
# start position (no ERROR 1050, no duplicated rows, no stale gtid_binlog_pos).
# The snapshot is the source of truth — default y; typed-yes when replication
# is currently running (dropping would break it). Refusing keeps the old
# behavior (the import then asks about recreating the DB with data in it).
pre_import_clean_shared_db() {
    if [ "$DRY_RUN" = 1 ]; then
        info "+ STOP/RESET SLAVE; drop and re-create $SHARED_DB on this replica, then import into a clean DB"
        return 0
    fi
    _exists=$("$MARIADB" -u root -N -e "SELECT COUNT(*) FROM information_schema.schemata WHERE schema_name='$SHARED_DB'" 2>/dev/null || printf 0)
    [ "$_exists" = "1" ] || return 0
    _ntables=$(table_count "$SHARED_DB" 2>/dev/null) || _ntables=unknown
    _st=$("$MARIADB" -u root -e "SHOW SLAVE STATUS\G" 2>/dev/null || true)
    _io=$(printf '%s\n' "$_st" | sed -n 's/^ *Slave_IO_Running: //p' | head -1)
    _sql=$(printf '%s\n' "$_st" | sed -n 's/^ *Slave_SQL_Running: //p' | head -1)
    if [ "$_io" = "Yes" ] || [ "$_sql" = "Yes" ]; then
        warn "replication is CURRENTLY RUNNING on this host (IO: ${_io:-?}, SQL: ${_sql:-?})."
        warn "dropping $SHARED_DB will break it — the replica will have to be re-provisioned from scratch."
        ask_yn "Drop and re-create $SHARED_DB before importing anyway?" y
        [ "$REPLY" = y ] && confirm_typed_yes "Replication will be stopped and $SHARED_DB dropped, then re-imported from the snapshot."
    else
        info "Existing copy of $SHARED_DB: ${_ntables} table(s) — stale (a previous setup attempt)."
        ask_yn "Drop and re-create $SHARED_DB before importing? (the snapshot is the source of truth)" y
    fi
    [ "$REPLY" = y ] || { info "Keeping the existing $SHARED_DB — the import will go on top of it (not recommended)."; return 0; }
    "$MARIADB" -u root -e "STOP SLAVE" >/dev/null 2>&1 || true
    "$MARIADB" -u root -e "RESET SLAVE" >/dev/null 2>&1 || true
    sql_drop_db "$SHARED_DB"
    sql_exec "CREATE DATABASE IF NOT EXISTS $SHARED_DB CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
    ok "$SHARED_DB re-created empty — the import binds the snapshot's GTID as the position."
}

save_credentials() { # $1 — text; prints ONLY the file path (everything else goes to stderr)
    _cred="/root/mrpg-replication-$(date +%Y%m%d_%H%M%S).cred"
    if [ "$DRY_RUN" = 1 ]; then
        info "+ saving credentials to $_cred (chmod 600):" >&2
        printf '%s\n' "$1" | sed 's/^/    | /' >&2
        printf '%s\n' "$_cred"
        return 0
    fi
    if ! ( umask 077; printf '%s\n' "$1" > "$_cred" ); then
        warn "could not save credentials to $_cred — THE PASSWORDS BELOW ARE THE ONLY RECORD, copy them now:"
        printf '%s\n' "$1" >&2
        die "credentials were not persisted to $_cred"
    fi
    if ! chmod 600 "$_cred"; then
        warn "could not chmod 600 $_cred — restrict it: chmod 600 $_cred"
    fi
    printf '%s\n' "$_cred"
}

# _log_bin_on — prints ON when this server has binary logging enabled (always ON in dry-run so the printed plan keeps the SET sql_log_bin lines)
_log_bin_on() {
    if [ "$DRY_RUN" = 1 ]; then
        printf 'ON\n'
        return 0
    fi
    _lb=$("$MARIADB" -u root -N -e "SELECT @@global.log_bin" 2>/dev/null || printf 'OFF')
    { [ "$_lb" = "ON" ] || [ "$_lb" = "1" ]; } && printf 'ON\n' || printf 'OFF\n'
}

# sql_drop_db <db> — DROP DATABASE. Two session guards keep it from failing
# halfway: foreign_key_checks=0 (the shared database is FK-coupled — a plain
# DROP fails with 1451 depending on table order) and sql_log_bin=0, applied
# only when this server actually has binary logging (binlog-less replicas do
# not know the variable).
sql_drop_db() {
    if [ "$DRY_RUN" = 1 ]; then
        info "+ DROP DATABASE IF EXISTS $1 (foreign_key_checks=0; sql_log_bin=0 first if this server has binlog)"
        return 0
    fi
    if [ "$(_log_bin_on)" = "ON" ]; then
        sql_stdin "the DROP of $1 failed — see the errors above" <<EOF
SET SESSION sql_log_bin = 0;
SET SESSION foreign_key_checks = 0;
DROP DATABASE IF EXISTS $1;
SET SESSION foreign_key_checks = 1;
SET SESSION sql_log_bin = 1;
EOF
    else
        sql_stdin "the DROP of $1 failed — see the errors above" <<EOF
SET SESSION foreign_key_checks = 0;
DROP DATABASE IF EXISTS $1;
SET SESSION foreign_key_checks = 1;
EOF
    fi
}

# ---------- snapshot fetch (shared by the replica setup and the 1236 recovery) ----------

# dump_remote_command <db> — the shell command run on the primary over ssh; detects
# mariadb-dump vs mysqldump and --source-data vs --master-data (MySQL 8 vs MariaDB 10.x)
dump_remote_command() { # $1 — database
    printf "D=\$(command -v mariadb-dump || command -v mysqldump); if [ \$(id -u) = 0 ]; then SUDO=; else SUDO='sudo -n'; fi; if \$D --help 2>&1 | grep -q -- --source-data; then OPT=--source-data=2; else OPT=--master-data=2; fi; if ! \$D --help 2>&1 | grep -q -- --gtid; then echo 'mariadb-dump on the primary does not support --gtid (MariaDB 10.4+ required for a GTID snapshot)' >&2; exit 3; fi; \$SUDO \"\$D\" -u root --single-transaction --quick --routines --triggers --events \$OPT --gtid '%s'" "$1"
}

snapshot_fail_hints() { # $1 — user@host
    info "Things to check:"
    info "  • the primary is reachable: ping ${1#*@}; ssh $1 true"
    info "  • ssh asked for a password but got no terminal? run the script from an interactive shell (not via cron/pipe)"
    info "  • host key changed on the primary? remove the stale entry: ssh-keygen -R ${1#*@} — then rerun"
    info "  • password login for root may be disabled (PermitRootLogin): use an ssh key: ssh-keygen; ssh-copy-id $1"
    info "  • non-root SSH user: passwordless sudo for mariadb-dump is required, e.g.: '$1' ALL=(root) NOPASSWD: /usr/bin/mariadb-dump"
    info "  • verify the fingerprint yourself: on the primary run: ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub"
}

# tmp_snapshot_arm / tmp_snapshot_disarm — always remove the temp dump, even on Ctrl+C
tmp_snapshot_arm() { # $1 — path of the temp dump
    _tmpdump=$1
    trap '_t=$_tmpdump; [ -n "$_t" ] && rm -f "$_t"' EXIT
    trap '_t=$_tmpdump; [ -n "$_t" ] && rm -f "$_t"; exit 130' INT TERM
}
tmp_snapshot_disarm() {
    trap - EXIT INT TERM
    if [ -n "${_tmpdump:-}" ]; then rm -f "$_tmpdump"; _tmpdump=""; fi
}

# fetch_snapshot <user@host> <db> <dest file> — 0 = success, 1 = failure (already logged)
fetch_snapshot() {
    if [ -r /dev/tty ] && [ -w /dev/tty ] && ( : </dev/tty ) 2>/dev/null; then _ssh_stdin=/dev/tty; else _ssh_stdin=/dev/null; fi
    ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=4 "$1" "$(dump_remote_command "$2")" < "$_ssh_stdin" > "$3" &
    _ssh_pid=$!
    _shown=0
    _tick=0
    while kill -0 "$_ssh_pid" 2>/dev/null; do
        sleep 3
        _tick=$((_tick + 1))
        [ $((_tick % 5)) -eq 0 ] || continue
        _sz=$(wc -c < "$3" 2>/dev/null | tr -d ' ')
        [ -n "$_sz" ] || continue
        _mb=$(( _sz / 1048576 ))
        if [ "$_mb" != "$_shown" ]; then
            _shown=$_mb
            info "… downloaded ${_mb} MB so far (Ctrl+C to abort)"
        fi
    done
    _rc=0
    wait "$_ssh_pid" || _rc=$?
    if [ "$_rc" != 0 ]; then
        warn "could not fetch the dump over SSH."
        snapshot_fail_hints "$1"
        return 1
    fi
    if [ ! -s "$3" ]; then
        warn "the downloaded dump is empty — import aborted"
        return 1
    fi
    return 0
}

# ---------- master ----------
do_master() {
    info
    info "=== Configure primary (master) ==="
    info

    SERVER_ID=""
    ask_server_id 1
    ask_shared_db

    # dump for import
    _dump_default="$REPO_ROOT/mrpg_shared.sql"
    if [ -f "$_dump_default" ]; then
        prompt "Path to the dump to import ('-' = skip)" "$_dump_default"
    else
        prompt "Path to the dump to import (empty = skip)" ""
    fi
    DUMP=$REPLY
    [ "$DUMP" = "-" ] && DUMP=""
    if [ -n "$DUMP" ] && [ ! -f "$DUMP" ]; then
        die "dump file not found: $DUMP"
    fi

    GAME_HOST_MASK=""
    while :; do
        prompt "Host mask of mrpg_user on the primary (e.g. 10.0.0.% or %)" "%"
        if ( check_netmask "$REPLY" 2>/dev/null ); then GAME_HOST_MASK=$REPLY; break; fi
        info "Host mask: letters, digits, . _ % - : with no spaces or quotes."
    done

    BIND_ADDRESS=""
    prompt "bind-address for MariaDB (empty = keep as is; set the private/VPN IP to allow replicas to connect)" ""
    BIND_ADDRESS=$REPLY
    [ -n "$BIND_ADDRESS" ] && check_host "$BIND_ADDRESS"

    prompt_secret "mrpg_replica password — used by replicas to connect (Enter = generate): "
    REPLICA_REPL_PASSWORD=$REPLY
    check_password "$REPLICA_REPL_PASSWORD"
    [ -z "$REPLICA_REPL_PASSWORD" ] && REPLICA_REPL_PASSWORD=$(gen_password)

    prompt_secret "mrpg_bridge password — SELECT-only account for the bridge/replicas (Enter = generate): "
    BRIDGE_READ_PASSWORD=$REPLY
    check_password "$BRIDGE_READ_PASSWORD"
    [ -z "$BRIDGE_READ_PASSWORD" ] && BRIDGE_READ_PASSWORD=$(gen_password)

    prompt_secret "mrpg_user password — game server account (Enter = generate): "
    GAME_PASSWORD=$REPLY
    check_password "$GAME_PASSWORD"
    [ -z "$GAME_PASSWORD" ] && GAME_PASSWORD=$(gen_password)

    UFW_NETWORK=""
    prompt "Allow 3306/tcp only from this CIDR via ufw (empty = skip; e.g. 10.0.0.0/24)" ""
    UFW_NETWORK=$REPLY
    [ -n "$UFW_NETWORK" ] && check_cidr "$UFW_NETWORK"

    info
    info "Plan for this host (primary/master):"
    info "  • install MariaDB, server_id=$SERVER_ID"
    info "  • binary log (ROW) + gtid_strict_mode → $MASTER_CONF"
    info "  • database $SHARED_DB (utf8mb4); dump import: ${DUMP:-skipped}"
    info "  • accounts: mrpg_replica (REPLICATION SLAVE), mrpg_bridge (SELECT), mrpg_user (shared game database)"
    info "  • mrpg_user grants: $SHARED_DB.* (host '$GAME_HOST_MASK'); local game accounts are created on each location host"
    [ -n "$BIND_ADDRESS" ] && info "  • bind-address = $BIND_ADDRESS"
    [ -n "$UFW_NETWORK" ]   && info "  • ufw: allow 3306/tcp only from $UFW_NETWORK"
    confirm_plan y

    install_mariadb
    detect_bins

    info "→ MariaDB configuration (binary log)"
    {
        printf '[mysqld]\nserver_id = %s\nlog_bin = /var/log/mysql/mariadb-bin\nbinlog_format = ROW\ngtid_strict_mode = 1\n' "$SERVER_ID"
        [ -n "$BIND_ADDRESS" ] && printf 'bind-address = %s\n' "$BIND_ADDRESS"
    } | write_conf "$MASTER_CONF"
    restart_mariadb "$MASTER_CONF"
    if [ "$DRY_RUN" != 1 ]; then
        _lb=$("$MARIADB" -u root -N -e "SELECT @@global.log_bin" 2>/dev/null || echo unknown)
        _ser=$("$MARIADB" -u root -N -e "SELECT @@global.server_id" 2>/dev/null || echo unknown)
        [ "$_ser" = "$SERVER_ID" ] || die "MariaDB reports server_id=$_ser, expected $SERVER_ID — the $MASTER_CONF was not loaded (check the include order in /etc/mysql/mariadb.conf.d/)"
        [ "$_lb" = "ON" ] || die "MariaDB is up but log_bin is OFF — the $MASTER_CONF was not loaded (check the include order in /etc/mysql/mariadb.conf.d/); replication cannot work"
        info "MariaDB is up: server_id=$_ser, log_bin=$_lb"
    fi

    info "→ Database $SHARED_DB"
    sql_exec "CREATE DATABASE IF NOT EXISTS $SHARED_DB CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
    if [ -n "$DUMP" ]; then
        import_dump "$SHARED_DB" "$DUMP" 1
    else
        info "Dump import skipped."
    fi

    info "→ Accounts"
    # sql_log_bin=0: the accounts must not be replicated to the replicas
    sql_stdin "the primary accounts were NOT created — see the errors above; fix the cause and rerun menu 1" <<EOF
SET SESSION sql_log_bin = 0;
CREATE USER IF NOT EXISTS 'mrpg_replica'@'%' IDENTIFIED BY '$REPLICA_REPL_PASSWORD';
GRANT REPLICATION SLAVE, REPLICATION CLIENT ON *.* TO 'mrpg_replica'@'%';
CREATE USER IF NOT EXISTS 'mrpg_bridge'@'%' IDENTIFIED BY '$BRIDGE_READ_PASSWORD';
GRANT SELECT ON $SHARED_DB.* TO 'mrpg_bridge'@'%';
CREATE USER IF NOT EXISTS 'mrpg_user'@'$GAME_HOST_MASK' IDENTIFIED BY '$GAME_PASSWORD';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON $SHARED_DB.* TO 'mrpg_user'@'$GAME_HOST_MASK';
FLUSH PRIVILEGES;
SET SESSION sql_log_bin = 1;
EOF
    info "Accounts created (existing accounts are kept — passwords are not changed; see CREATE USER IF NOT EXISTS)"

    info "→ Verification (the same TCP logins the replicas/bridge will use)"
    _mfail=0
    if [ "$DRY_RUN" = 1 ]; then
        info "+ SELECT 1 with mrpg_replica and mrpg_bridge over TCP"
        info "+ mrpg_bridge: a SELECT on $SHARED_DB (checks the GRANT, not just the login)"
        case "$GAME_HOST_MASK" in
            %|127.0.0.%|127.0.0.1|localhost) info "+ SELECT 1 with mrpg_user (host mask $GAME_HOST_MASK allows local TCP)" ;;
            *) info "+ mrpg_user test skipped (host mask $GAME_HOST_MASK does not cover this host)" ;;
        esac
    else
        if MYSQL_PWD="$REPLICA_REPL_PASSWORD" "$MARIADB" --connect-timeout=5 -h 127.0.0.1 -P 3306 -u mrpg_replica -e 'SELECT 1 LIMIT 1' >/dev/null 2>&1; then
            ok "mrpg_replica connects (TCP)"
        else
            warn "mrpg_replica cannot connect over TCP — replicas would fail identically (password? bind-address? skip-networking?)"
            _mfail=$((_mfail + 1))
        fi
        if MYSQL_PWD="$BRIDGE_READ_PASSWORD" "$MARIADB" --connect-timeout=5 -h 127.0.0.1 -P 3306 -u mrpg_bridge "$SHARED_DB" -e 'SELECT 1 LIMIT 1' >/dev/null 2>&1; then
            ok "mrpg_bridge connects and SELECTs on $SHARED_DB"
        else
            warn "mrpg_bridge cannot connect/SELECT on $SHARED_DB — the bridge would fail identically"
            _mfail=$((_mfail + 1))
        fi
        _first_table=$("$MARIADB" -u root -N -e "SELECT table_name FROM information_schema.tables WHERE table_schema='$SHARED_DB' LIMIT 1" 2>/dev/null || true)
        if [ -n "$_first_table" ] && MYSQL_PWD="$BRIDGE_READ_PASSWORD" "$MARIADB" --connect-timeout=5 -h 127.0.0.1 -P 3306 -u mrpg_bridge "$SHARED_DB" -e "SELECT COUNT(*) FROM $_first_table WHERE 1=0" >/dev/null 2>&1; then
            ok "mrpg_bridge has SELECT on $SHARED_DB (checked on $_first_table)"
        elif [ -n "$_first_table" ]; then
            warn "mrpg_bridge lacks SELECT on $SHARED_DB — the bridge would fail on real queries"
            _mfail=$((_mfail + 1))
        fi
        case "$GAME_HOST_MASK" in
            %|127.0.0.%|127.0.0.1|localhost)
                if MYSQL_PWD="$GAME_PASSWORD" "$MARIADB" --connect-timeout=5 -h 127.0.0.1 -P 3306 -u mrpg_user "$SHARED_DB" -e 'SELECT 1 LIMIT 1' >/dev/null 2>&1; then
                    ok "mrpg_user connects (TCP)"
                else
                    warn "mrpg_user cannot connect over TCP — the game server would fail identically"
                    _mfail=$((_mfail + 1))
                fi ;;
            *)  info "mrpg_user test skipped (host mask '$GAME_HOST_MASK' does not cover this host)" ;;
        esac
        [ "$_mfail" = 0 ] && ok "accounts verified" || warn "$_mfail account check(s) failed — fix them before pointing replicas/game servers here"
    fi

    if [ -n "$UFW_NETWORK" ]; then
        if command -v ufw >/dev/null 2>&1; then
            run ufw allow from "$UFW_NETWORK" to any port 3306 proto tcp || warn "ufw rule failed — add it manually: ufw allow from $UFW_NETWORK to any port 3306 proto tcp"
        else
            warn "ufw not found — configure the firewall manually (3306 should be reachable only from the private network)"
        fi
    fi

    _cred=$(save_credentials "primary (master): $(hostname 2>/dev/null || echo unknown)
date: $(date)
shared database: $SHARED_DB
server_id: $SERVER_ID

mrpg_replica (replication, used by the replicas): $REPLICA_REPL_PASSWORD
mrpg_bridge  (SELECT-only account for the bridge/replicas): $BRIDGE_READ_PASSWORD
mrpg_user    (shared game database, via bridge): $GAME_PASSWORD
mrpg_user host mask: $GAME_HOST_MASK

Setting up a replica (on the location host): sudo sh setup_replication.sh
  → menu: 2) replica; answer the questions with the same passwords.
")

    echo
    if [ "${_mfail:-0}" = 0 ]; then
        ok "Primary (master) is ready."
    else
        warn "Primary is configured, but ${_mfail} verification check(s) failed — fix them; replicas and the game servers would fail the same way."
    fi
    [ -n "$BIND_ADDRESS" ] || info "Note: bind-address was not changed — to let replicas connect from the network rerun this script with a private/VPN IP or configure the firewall."
    info "Credentials saved to: ${_cred:-(see above)}"
    info "Next steps:"
    info "  1. MariaDB tuning — docs/mariadb.md (innodb_buffer_pool_size etc.)"
    info "  2. On every location: sudo sh setup_replication.sh → 2) replica"
    info "  3. On the locations — the bridge (step 3 in other/sql_bridge/README.md) and the game server (step 4)"
    print_final_card master
}

# ---------- replica ----------
do_replica() {
    info
    info "=== Configure replica ==="
    info

    SERVER_ID=""
    ask_server_id 2
    ask_shared_db

    prompt_required "Address of the primary (host or IP)"
    PRIMARY_HOST=$REPLY
    check_host "$PRIMARY_HOST"

    PRIMARY_PORT=""
    while :; do
        prompt "Port of the primary" "3306"
        if is_number "$REPLY"; then PRIMARY_PORT=$REPLY; break; fi
        info "The port must be a number."
    done

    info
    info "Where to get the snapshot of $SHARED_DB?"
    info "  1) a snapshot file on this host"
    info "  2) pull it from the primary over SSH (sudo mariadb-dump on the primary)"
    info "  3) skip (the database has already been imported)"
    SNAPSHOT=""
    FROM_PRIMARY=""
    # Position recorded by the fresh --gtid snapshot. mariadb-dump writes the
    # SET GLOBAL gtid_slave_pos statement commented when --master-data=2 is
    # used, so the script applies this position explicitly after the import.
    SNAPSHOT_GTID=""
    SKIP_SNAPSHOT=0
    while :; do
        prompt "Choice" "2"
        case "$REPLY" in
            1)
                prompt_required "Path to the snapshot file"
                [ -f "$REPLY" ] || die "file not found: $REPLY"
                SNAPSHOT=$REPLY
                break ;;
            2)
                prompt "SSH address of the primary (user@host)" "root@$PRIMARY_HOST"
                check_host "$REPLY"
                FROM_PRIMARY=$REPLY
                break ;;
            3)
                SKIP_SNAPSHOT=1
                break ;;
            *) info "Choose 1, 2 or 3." ;;
        esac
    done

    while :; do
        prompt_secret "mrpg_replica password (set on the primary; required): "
        [ -n "$REPLY" ] && break
        info "Required — it cannot be generated locally (it was created on the primary)."
    done
    REPLICA_REPL_PASSWORD=$REPLY
    check_password "$REPLICA_REPL_PASSWORD"

    prompt_secret "Local mrpg_bridge password (Enter = generate): "
    BRIDGE_READ_PASSWORD=$REPLY
    check_password "$BRIDGE_READ_PASSWORD"
    [ -z "$BRIDGE_READ_PASSWORD" ] && BRIDGE_READ_PASSWORD=$(gen_password)

    # --- local database mrpg_local_<location> and a dedicated local user ---
    LOCAL_DB_YN=n
    LOCAL_DB_NAME=""
    LOCAL_USER=mrpg_local
    LOCAL_USER_HOST=127.0.0.1
    LOCAL_USER_PASSWORD=""
    LOCAL_IMPORT=""
    ask_yn "Create the local database (mrpg_local_<location>) and a local user on this host?" y
    if [ "$REPLY" = y ]; then
        LOCAL_DB_YN=y
        prompt_required "Location name for the local database (e.g. eu, us, msk)"
        _loc=$REPLY
        case "$_loc" in *[!a-zA-Z0-9_]*) die "location name: only letters, digits and _" ;; esac
        LOCAL_DB_NAME="mrpg_local_$_loc"
        check_ident "$LOCAL_DB_NAME"

        prompt "Name of the local user for $LOCAL_DB_NAME" "mrpg_local"
        LOCAL_USER=$REPLY
        check_ident "$LOCAL_USER"

        prompt "Host for the local user (127.0.0.1 = local game server; 10.0.0.% = LAN; % = any)" "127.0.0.1"
        LOCAL_USER_HOST=$REPLY
        check_netmask "$LOCAL_USER_HOST"

        prompt_secret "Local user password (Enter = generate): "
        LOCAL_USER_PASSWORD=$REPLY
        check_password "$LOCAL_USER_PASSWORD"
        [ -z "$LOCAL_USER_PASSWORD" ] && LOCAL_USER_PASSWORD=$(gen_password)

        _local_dump_default="$REPO_ROOT/mrpg_local.sql"
        if [ -f "$_local_dump_default" ]; then
            prompt "Path to mrpg_local.sql to import ('-' = skip)" "$_local_dump_default"
        else
            prompt "Path to mrpg_local.sql to import (empty = skip)" ""
        fi
        LOCAL_IMPORT=$REPLY
        [ "$LOCAL_IMPORT" = "-" ] && LOCAL_IMPORT=""
        if [ -n "$LOCAL_IMPORT" ] && [ ! -f "$LOCAL_IMPORT" ]; then
            die "dump file not found: $LOCAL_IMPORT"
        fi
    fi

    _snap_desc="skipped"
    [ -n "$SNAPSHOT" ]     && _snap_desc="file $SNAPSHOT"
    [ -n "$FROM_PRIMARY" ] && _snap_desc="over SSH from $FROM_PRIMARY"

    info
    info "Plan for this host (replica):"
    info "  • install MariaDB, server_id=$SERVER_ID, read_only=1"
    info "  • replicate_wild_ignore_table = mrpg_local%.% (local databases are not replicated)"
    info "  • snapshot from the primary: $_snap_desc"
    info "  • replication from $PRIMARY_HOST:$PRIMARY_PORT (GTID, MASTER_SSL=1)"
    info "  • local account mrpg_bridge@127.0.0.1 (SELECT on $SHARED_DB)"
    if [ "$LOCAL_DB_YN" = y ]; then
        info "  • local database $LOCAL_DB_NAME + direct game account $LOCAL_USER@'$LOCAL_USER_HOST' (ALL PRIVILEGES on this database)"
        info "  • $LOCAL_USER also gets READ ONLY ADMIN so it can write this local database while the shared replica stays read_only=1"
        info "  • local dump import: ${LOCAL_IMPORT:-skipped}"
    else
        info "  • local database: created manually later (README, step 2.5)"
    fi
    [ "$SKIP_SNAPSHOT" = 1 ] || info "  • an existing copy of $SHARED_DB on this replica is dropped and re-created before the import (confirmed interactively)"
    confirm_plan y

    install_mariadb
    detect_bins
    if [ -n "$FROM_PRIMARY" ]; then
        command -v ssh >/dev/null 2>&1 || die "pulling the snapshot over SSH requires an ssh client"
    fi

    info "→ MariaDB configuration (replica)"
    write_conf "$REPLICA_CONF" <<EOF
[mysqld]
server_id = $SERVER_ID
bind-address = 127.0.0.1
read_only = 1
gtid_strict_mode = 1
# only mrpg_shared is replicated; local databases are excluded
replicate_wild_ignore_table = mrpg_local%.%
EOF
    restart_mariadb "$REPLICA_CONF"

    info "→ Database $SHARED_DB and snapshot"
    sql_exec "CREATE DATABASE IF NOT EXISTS $SHARED_DB CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
    if [ "$SKIP_SNAPSHOT" != 1 ]; then
        _snap=$SNAPSHOT
        _tmpdump=""
        if [ -z "$_snap" ] && [ -n "$FROM_PRIMARY" ]; then
            if [ "$DRY_RUN" = 1 ]; then
                info "+ ssh (background, progress every 15 s) -o StrictHostKeyChecking=accept-new -o ConnectTimeout=15 $FROM_PRIMARY \"<dump with --source-data=2 or --master-data=2 auto-detected, plus --gtid>\" > <tmp>"
                _snap="<tmp>"
            else
                _tmpdump=$(mktemp /tmp/mrpg_shared_snapshot.XXXXXX) || die "cannot create a temp file in /tmp — free some space and retry"
                tmp_snapshot_arm "$_tmpdump"
                info "Pulling the snapshot from $FROM_PRIMARY (this may take a while)."
                info "SSH is interactive: a password/passphrase may be asked; the primary's host key is added automatically if new."
                if ! fetch_snapshot "$FROM_PRIMARY" "$SHARED_DB" "$_tmpdump"; then
                    tmp_snapshot_disarm
                    die "could not fetch the dump over SSH. Alternative: create it manually (README, step 2.2) and choose option 1 with the ready file"
                fi
                _snap=$_tmpdump
            fi
        fi
        if [ "$DRY_RUN" != 1 ]; then
            SNAPSHOT_GTID=$(extract_gtid_pos "$_snap")
            if [ -z "$SNAPSHOT_GTID" ]; then
                [ -n "$_tmpdump" ] && rm -f "$_tmpdump"
                die "the snapshot does not record SET GLOBAL gtid_slave_pos (dump without --gtid) — aborting because starting GTID replication from it would cause error 1236. Re-create it with --master-data=2 --gtid (snapshot option 2 over SSH)."
            fi
            check_gtid_pos "$SNAPSHOT_GTID"
            info "Snapshot records GTID position: $SNAPSHOT_GTID"
            # probe the replication master (never prompts): the explicit SSH address when
            # one was given (snapshot option 2), otherwise try root@PRIMARY_HOST for option 1
            if [ -n "$FROM_PRIMARY" ]; then _probe="$FROM_PRIMARY"; else _probe="root@$PRIMARY_HOST"; fi
            _pri_pos=$(ssh -o BatchMode=yes -o ConnectTimeout=8 "$_probe" "if [ \$(id -u) = 0 ]; then mariadb -u root -N -e 'SELECT @@global.gtid_binlog_pos'; else sudo -n mariadb -u root -N -e 'SELECT @@global.gtid_binlog_pos'; fi" 2>/dev/null || true)
            if [ -n "$_pri_pos" ]; then
                info "primary gtid_binlog_pos now:    $_pri_pos"
                _cov=$(gtid_end_compare "$SNAPSHOT_GTID" "$_pri_pos")
                case "$_cov" in
                    fresh)
                        ok "the snapshot position matches the primary's current position." ;;
                    behind)
                        warn "the snapshot was taken before the primary's current position —"
                        info "  the primary may have already purged those binlogs (expire_logs_days) → Last_IO_Error 1236."
                        info "  the safest option is a fresh dump (snapshot option 2 — SSH pull)." ;;
                    foreign)
                        die "the snapshot is FOREIGN to this primary — it contains a position this server never had (or an ahead one). This is guaranteed Last_IO_Error 1236. Re-create the dump ON the primary (menu 2, snapshot option 2), or use a dump created after the primary was rebuilt/reset." ;;
                esac
            elif [ -n "$FROM_PRIMARY" ]; then
                info "note: could not query the primary's gtid_binlog_pos (BatchMode ssh failed) — skipping the comparison"
            fi
        else
            SNAPSHOT_GTID="<GTID from snapshot>"
        fi
        pre_import_clean_shared_db
        import_dump "$SHARED_DB" "$_snap"
        [ -n "$_tmpdump" ] && tmp_snapshot_disarm
    else
        info "Snapshot import skipped."
        if [ "$SKIP_SNAPSHOT" = 1 ] && [ "$DRY_RUN" != 1 ]; then
            warn "no import happened — replication will start from this server's current gtid_binlog_pos."
            info "  make sure mrpg_shared on this host holds the right data and its GTID position matches the primary,"
            info "  otherwise replication cannot converge (see the 1236 note in the README)."
        fi
    fi

    info "→ Replication setup"
    if [ "$DRY_RUN" = 1 ]; then
        info "+ $MARIADB -u root -e 'STOP SLAVE; RESET SLAVE' (errors ignored)"
    else
        "$MARIADB" -u root -e "STOP SLAVE" >/dev/null 2>&1 || true
        "$MARIADB" -u root -e "RESET SLAVE" >/dev/null 2>&1 || true
    fi
    if [ "$SKIP_SNAPSHOT" != 1 ]; then
        # --master-data=2 comments the dump's SET GLOBAL gtid_slave_pos line.
        # Do this after the final RESET SLAVE, otherwise the replica can ask the
        # primary for its stale pre-import GTID and immediately get error 1236.
        set_gtid_slave_pos "$SNAPSHOT_GTID"
        _gtid_bootstrap=slave_pos
    else
        # No snapshot was imported, so preserve the legacy explicit choice:
        # CURRENT_POS is only valid when the existing data and GTID state match.
        _gtid_bootstrap=CURRENT_POS
    fi
    if ! sql_stdin <<EOF
CHANGE MASTER TO
  MASTER_HOST='$PRIMARY_HOST',
  MASTER_PORT=$PRIMARY_PORT,
  MASTER_USER='mrpg_replica',
  MASTER_PASSWORD='$REPLICA_REPL_PASSWORD',
  MASTER_USE_GTID=$_gtid_bootstrap,
  MASTER_SSL=1;
START SLAVE;
EOF
    then
        die "CHANGE MASTER/START SLAVE failed — see the SQL errors above. Check: primary reachable ($PRIMARY_HOST:$PRIMARY_PORT), the mrpg_replica password, MASTER_SSL, and the snapshot GTID position. The replica is NOT configured; fix the cause and rerun menu 2."
    fi

    info "→ Local mrpg_bridge account"
    sql_stdin "the local mrpg_bridge was NOT created — see the errors above; the bridge cannot read the replica" <<EOF
CREATE USER IF NOT EXISTS 'mrpg_bridge'@'127.0.0.1' IDENTIFIED BY '$BRIDGE_READ_PASSWORD';
GRANT SELECT ON $SHARED_DB.* TO 'mrpg_bridge'@'127.0.0.1';
FLUSH PRIVILEGES;
EOF

    if [ "$LOCAL_DB_YN" = y ]; then
        info "→ Local database $LOCAL_DB_NAME and direct game user $LOCAL_USER@'$LOCAL_USER_HOST'"
        sql_exec "CREATE DATABASE IF NOT EXISTS $LOCAL_DB_NAME CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
        # The MariaDB instance remains read_only=1 to protect mrpg_shared. The
        # local database is deliberately excluded from replication, therefore
        # its game account must be allowed to bypass that global guard. READ
        # ONLY ADMIN only bypasses read_only; ALL PRIVILEGES below is still
        # limited to this one local schema.
        sql_stdin "the local user $LOCAL_USER was NOT created/granted write access — see the errors above" <<EOF
CREATE USER IF NOT EXISTS '$LOCAL_USER'@'$LOCAL_USER_HOST' IDENTIFIED BY '$LOCAL_USER_PASSWORD';
GRANT ALL PRIVILEGES ON $LOCAL_DB_NAME.* TO '$LOCAL_USER'@'$LOCAL_USER_HOST';
GRANT READ ONLY ADMIN ON *.* TO '$LOCAL_USER'@'$LOCAL_USER_HOST';
FLUSH PRIVILEGES;
EOF
        if [ -n "$LOCAL_IMPORT" ]; then
            import_dump "$LOCAL_DB_NAME" "$LOCAL_IMPORT" 1
            if [ "$DRY_RUN" = 1 ]; then
                info "+ $MARIADB -u root -N -e \"SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='$LOCAL_DB_NAME'\""
            else
                _n=$(table_count "$LOCAL_DB_NAME") || die "could not count the tables of $LOCAL_DB_NAME — is MariaDB up?"
                if [ "$_n" = 10 ]; then
                    ok "Local database has the expected 10 tables"
                else
                    warn "local database has $_n tables (expected 10) — check the import of $LOCAL_IMPORT"
                fi
            fi
        fi
    fi

    info "→ Verification"
    if [ "$DRY_RUN" = 1 ]; then
        info "+ $MARIADB -u root -e 'SHOW SLAVE STATUS\\G'"
        info "+ $MARIADB -u root -e \"SHOW DATABASES LIKE 'mrpg%';\""
    else
        _tries=0
        _ok=0
        while [ "$_tries" -lt 5 ]; do
            _status=$("$MARIADB" -u root -e "SHOW SLAVE STATUS\G" 2>/dev/null || true)
            _io=$(printf '%s\n' "$_status" | sed -n 's/^ *Slave_IO_Running: //p' | head -1)
            _sql=$(printf '%s\n' "$_status" | sed -n 's/^ *Slave_SQL_Running: //p' | head -1)
            if [ "$_io" = "Yes" ] && [ "$_sql" = "Yes" ]; then
                _ok=1
                break
            fi
            _tries=$((_tries + 1))
            sleep 2
        done
        if [ "$_ok" = 1 ]; then
            _lag=$(printf '%s\n' "$_status" | sed -n 's/^ *Seconds_Behind_Master: //p' | head -1)
            ok "Replication is running (IO/SQL threads: Yes; Seconds_Behind_Master: ${_lag:-?}; GTID mode: $_gtid_bootstrap)"
        else
            warn "replication did not come up — details:"
            if [ -z "$(printf '%s' "$_status" | grep -E 'Slave_IO_Running|Slave_SQL_Running')" ]; then
                info "  SHOW SLAVE STATUS returned nothing — replication was never configured (did CHANGE MASTER succeed?)"
            else
                printf '%s\n' "$_status" | grep -E 'Master_Host|Slave_IO_Running|Slave_SQL_Running|Seconds_Behind_Master|Last_IO_Error|Last_SQL_Error|Master_Server_Id' | sed 's/^/  /'
            fi
            info "  full output: $MARIADB -u root -e \"SHOW SLAVE STATUS\\G\""
            info "  if the snapshot import was redone or replication broke: menu 4 (reset) and set up again (menu 2)"
            _last_io=$(printf '%s\n' "$_status" | sed -n 's/^ *Last_IO_Error: //p' | head -1)
            case "$_last_io" in
                *1236*|*"not in the master's binlog"*)
                    warn "The replica asked for a GTID the primary no longer has in its binlog."
                    info "  Cause: stale snapshot (binlog purged or dump from a different/rebuilt primary)."
                    info "  Skipping the gap is NOT safe — a FRESH snapshot is required."
                    ask_yn "Re-provision now? (drop $SHARED_DB, fresh snapshot over SSH, reimport, redo CHANGE MASTER)" y
                    if [ "$REPLY" = y ]; then
                        reprovision_from_ssh
                    else
                        info "  Manual recovery: menu 4 (reset replica), then menu 2 with snapshot option 2 (fresh SSH pull)."
                    fi
                    ;;
            esac
        fi
        "$MARIADB" -u root -e "SHOW DATABASES LIKE 'mrpg%';"
    fi

    # ---- post-setup: game server config, bridge, systemd unit, verification ----
    if [ "$IS_ROOT" = 1 ] || [ "$DRY_RUN" = 1 ]; then
        info
        info "=== Final steps: game server, bridge, service, verification ==="
        info "  1) game server config (autoexec_server.cfg)  2) bridge.cfg  3) systemd service  4) verification"
        configure_game_server
        configure_bridge
        install_bridge_service
        final_ready_check
    else
        info "Skipping the game-server/bridge/systemd steps (root required) — rerun with sudo."
    fi

    _cred_extra=""
    if [ "$LOCAL_DB_YN" = y ]; then
        _cred_extra="
local database: $LOCAL_DB_NAME
$LOCAL_USER (game server, sv_sql_local_*): $LOCAL_USER_PASSWORD
game server config:
  sv_sql_local_host \"127.0.0.1\"
  sv_sql_local_port 3306
  sv_sql_local_login \"$LOCAL_USER\"
  sv_sql_local_password \"$LOCAL_USER_PASSWORD\"
  sv_sql_local_database \"$LOCAL_DB_NAME\"
"
    fi
    _cred_bridge=${BRIDGE_DIR:+$BRIDGE_DIR/bridge.cfg}
    _cred_bridge=${_cred_bridge:-(not configured)}
    _cred=$(save_credentials "replica: $(hostname 2>/dev/null || echo unknown)
date: $(date)
shared database: $SHARED_DB
server_id: $SERVER_ID
primary: $PRIMARY_HOST:$PRIMARY_PORT

mrpg_bridge (SELECT, local): $BRIDGE_READ_PASSWORD
bridge.cfg: ${_cred_bridge}
${_cred_extra}
Configure the bridge on this host (step 3 in other/sql_bridge/README.md):
  [replica] host = 127.0.0.1, port = 3306, user = mrpg_bridge,
            password = <password above>, database = $SHARED_DB
")

    echo
    ok "Replica is ready."
    info "Credentials saved to: ${_cred:-(see above)}"
    info "Next steps:"
    if [ -n "$BRIDGE_DIR" ]; then
        info "  • bridge: check $BRIDGE_DIR/bridge.cfg; logs: journalctl -u mrpg-mysql-bridge -f"
    else
        info "  • bridge: not configured — rerun and choose the bridge steps (or see other/sql_bridge/README.md, step 3)"
    fi
    info "  • game server: sv_sql_* → 127.0.0.1:33060 (bridge); sv_sql_local_* → 127.0.0.1:3306"
    info "  • if autoexec_server.cfg was edited in this run, restart the game server and check its [SQL] startup line"
    info "  • replication health: $MARIADB -u root -e \"SHOW SLAVE STATUS\G\" | grep -E 'Slave_IO_Running|Slave_SQL_Running|Seconds_Behind'"
    info "  • need to rebuild the local database? menu 4 (reset), then menu 2 (replica) again"
    print_final_card replica
}

# print_final_card <master|replica> — the last thing each flow prints: exactly
# what must be configured where for the game server to work.  Nobody should
# have to guess sv_sql_host or which password is which.
print_final_card() {
    info
    info "=========================================================="
    info "  FINAL CARD — what must be configured where"
    info "=========================================================="
    if [ "$1" = master ]; then
        info "  THIS host (primary): serves replicas and bridges."
        info "  The game NEVER connects here directly; the game goes"
        info "  through the bridge on ITS location host."
        info
        info "  Accounts on THIS host (passwords: /root/mrpg-replication-*.cred):"
        info "    mrpg_replica  REPLICATION SLAVE — replicas pull from here"
        info "    mrpg_bridge   SELECT only      — bridges read through this"
        info "    mrpg_user     game account     — the game (via the bridge)"
        info
        info "  On every LOCATION host (menu 2):"
        info "    sv_sql_host '127.0.0.1'   sv_sql_port 33060   # the bridge THERE"
        info "    sv_sql_login 'mrpg_user'  sv_sql_password '<mrpg_user password>'"
        info "    sv_sql_database_shared '$SHARED_DB'   sv_sql_use_tls 0"
        info "    # local part (if any): sv_sql_local_* -> 127.0.0.1:3306"
        info
        info "  Replicas need the network route to this host on 3306"
        info "  (private/VPN IP; bind-address + firewall)."
        return 0
    fi
    info "  THIS host: game server + local replica + bridge."
    info
    info "  autoexec_server.cfg (Step 1 wrote it; the same values):"
    info "    #sql"
    info "    sv_sql_host '127.0.0.1'                 # the bridge ON THIS HOST"
    info "    sv_sql_port 33060"
    info "    sv_sql_login 'mrpg_user'                # the game account on the PRIMARY"
    info "    sv_sql_password '<mrpg_user password>'    # same as on the primary:"
    info "                                          # see /root/mrpg-replication-*.cred THERE"
    info "    sv_sql_database_shared '$SHARED_DB'"
    info "    sv_sql_use_tls 0"
    info "    # (sv_sql_login is NEVER root / mrpg_bridge / mrpg_replica)"
    if [ "${LOCAL_DB_YN:-n}" = y ]; then
        info "    # local database — direct, no bridge:"
        info "    sv_sql_local_host '127.0.0.1'  sv_sql_local_port 3306"
        info "    sv_sql_local_login '$LOCAL_USER'  sv_sql_local_database '$LOCAL_DB_NAME'"
    else
        info "    # no local database: leave sv_sql_local_database empty"
    fi
    info
    info "  bridge.cfg (${BRIDGE_DIR:-<bridge directory>}/bridge.cfg):"
    info "    [primary] host = $PRIMARY_HOST  port = $PRIMARY_PORT   # writes, fallback"
    info "    [replica] host = 127.0.0.1  port = 3306  user = mrpg_bridge  database = $SHARED_DB"
    info
    info "  service: systemctl status mrpg-mysql-bridge"
    info "  logs:    journalctl -u mrpg-mysql-bridge -f"
    info "           (look for the 'stats replica=N primary=M' lines; replica grows)"
    info
    info "  Restart the game server after changing autoexec_server.cfg."
}

# ---------- status ----------
do_status() {
    [ "$IS_ROOT" = 1 ] || die "root is required for the status check — rerun: sudo sh $0"
    detect_bins
    sql_exec "SHOW VARIABLES WHERE Variable_name IN ('server_id','log_bin','read_only','gtid_strict_mode','binlog_format','replicate_wild_ignore_table');"
    "$MARIADB" -u root -e "SHOW SLAVE STATUS\G" 2>/dev/null || info "(this server is not configured as a replica)"
    sql_exec "SHOW DATABASES LIKE 'mrpg%';"
}

# ---------- reset ----------
do_reset() {
    info
    info "=== Reset the configuration created by this script ==="
    info "Configs are never deleted silently — they are moved aside to *.removed.<date>."
    info

    _have_master=0; [ -f "$MASTER_CONF" ]  && _have_master=1
    _have_replica=0; [ -f "$REPLICA_CONF" ] && _have_replica=1

    if [ "$_have_master" = 1 ] && [ "$_have_replica" = 0 ]; then
        _role=master
    elif [ "$_have_replica" = 1 ] && [ "$_have_master" = 0 ]; then
        _role=replica
    else
        if [ "$_have_master" = 1 ]; then
            info "Found both $MASTER_CONF and $REPLICA_CONF on this host."
        else
            info "None of the script configs were found on this host."
        fi
        while :; do
            prompt "Reset which role? [master/replica]" "replica"
            case "$REPLY" in
                master|replica) _role=$REPLY; break ;;
                *) info "Answer master or replica." ;;
            esac
        done
    fi
    info "Selected role: $_role"

    if [ "$_role" = master ]; then
        reset_master
    else
        reset_replica
    fi
}

reset_master() {
    ask_shared_db

    ask_yn "Remove $MASTER_CONF (binary log / GTID)? Moved to *.removed.<date>." y
    _rm_conf=$REPLY
    ask_yn "Drop the accounts created by this script? (mrpg_replica, mrpg_bridge, mrpg_user)" n
    _rm_users=$REPLY
    _mask="%"
    if [ "$_rm_users" = y ]; then
        prompt "Host part of the mrpg_user account to drop" "%"
        _mask=$REPLY
        check_netmask "$_mask"
    fi
    ask_yn "Drop database $SHARED_DB and ALL DATA in it?" n
    _rm_db=$REPLY
    if [ "$_rm_db" = y ]; then
        confirm_typed_yes "Database $SHARED_DB on the PRIMARY will be dropped — this destroys the only full copy of the data (replicas keep stale copies)."
    fi
    ask_yn "Restart MariaDB now?" y
    _restart=$REPLY

    info
    info "Plan (reset master):"
    info "  • config: $([ "$_rm_conf" = y ] && printf 'remove %s' "$MASTER_CONF" || printf 'keep %s' "$MASTER_CONF")"
    info "  • accounts: $([ "$_rm_users" = y ] && printf 'drop mrpg_replica, mrpg_bridge, mrpg_user@%s' "$_mask" || printf 'keep')"
    info "  • database $SHARED_DB: $([ "$_rm_db" = y ] && printf 'DROP (all data lost)' || printf 'keep')"
    info "  • restart MariaDB: $([ "$_restart" = y ] && printf 'yes' || printf 'no')"
    confirm_plan n

    detect_bins
    if [ "$_rm_conf" = y ]; then remove_conf "$MASTER_CONF"; fi
    if [ "$_rm_users" = y ]; then
        # sql_log_bin=0: the DROPs must not replicate to the replicas
        sql_stdin "the reset failed partway — see the errors above; the host may be half-reset (check menu 3 — status)" <<EOF
SET SESSION sql_log_bin = 0;
DROP USER IF EXISTS 'mrpg_replica'@'%';
DROP USER IF EXISTS 'mrpg_bridge'@'%';
DROP USER IF EXISTS 'mrpg_user'@'$_mask';
FLUSH PRIVILEGES;
SET SESSION sql_log_bin = 1;
EOF
    fi
    if [ "$_rm_db" = y ]; then
        sql_drop_db "$SHARED_DB"
    fi
    if [ "$_restart" = y ]; then restart_mariadb; fi

    echo
    ok "Master configuration reset."
    info "Notes:"
    info "  • Existing replicas keep their stale copies and stay read-only — reset them too (menu 4 on each location)."
    info "  • Old binary logs in /var/log/mysql are kept — purge them manually if needed."
    info "  • Removed config is backed up next to the original; to configure again use menu 1."
}

reset_replica() {
    ask_shared_db

    ask_yn "Stop and reset replication (STOP SLAVE; RESET SLAVE ALL)?" y
    _rst_repl=$REPLY
    ask_yn "Remove $REPLICA_CONF (read_only / ignore mrpg_local%.%)? Moved to *.removed.<date>." y
    _rm_conf=$REPLY
    ask_yn "Drop the local read-only account mrpg_bridge@127.0.0.1?" n
    _rm_user=$REPLY
    ask_yn "Drop the replicated database $SHARED_DB on this replica?" n
    _rm_db=$REPLY
    if [ "$_rm_db" = y ]; then
        confirm_typed_yes "Database $SHARED_DB (the replica's copy) will be dropped. The primary keeps the data."
    fi
    prompt "Local user to drop (empty = skip)" ""
    _rm_local_user=$REPLY
    _rm_local_user_host=""
    if [ -n "$_rm_local_user" ]; then
        check_ident "$_rm_local_user"
        prompt "Host of that user" "127.0.0.1"
        _rm_local_user_host=$REPLY
        check_netmask "$_rm_local_user_host"
    fi
    prompt "Local database to drop, e.g. mrpg_local_eu (empty = skip)" ""
    _rm_local_db=$REPLY
    if [ -n "$_rm_local_db" ]; then
        check_ident "$_rm_local_db"
        confirm_typed_yes "Database $_rm_local_db on this host will be dropped. This deletes LOCAL game state (guilds, groups, auction, house owners) — it is not replicated anywhere."
    fi
    ask_yn "Restart MariaDB now?" y
    _restart=$REPLY

    info
    info "Plan (reset replica):"
    info "  • replication: $([ "$_rst_repl" = y ] && printf 'STOP SLAVE; RESET SLAVE ALL' || printf 'keep')"
    info "  • config: $([ "$_rm_conf" = y ] && printf 'remove %s' "$REPLICA_CONF" || printf 'keep %s' "$REPLICA_CONF")"
    info "  • account mrpg_bridge@127.0.0.1: $([ "$_rm_user" = y ] && printf 'drop' || printf 'keep')"
    info "  • database $SHARED_DB: $([ "$_rm_db" = y ] && printf 'DROP (stale replica copy)' || printf 'keep')"
    info "  • local user: $([ -n "$_rm_local_user" ] && printf 'drop %s@%s' "$_rm_local_user" "$_rm_local_user_host" || printf 'keep')"
    info "  • local database: $([ -n "$_rm_local_db" ] && printf 'DROP %s' "$_rm_local_db" || printf 'keep')"
    info "  • restart MariaDB: $([ "$_restart" = y ] && printf 'yes' || printf 'no')"
    confirm_plan n

    detect_bins
    if [ "$_rst_repl" = y ]; then
        if [ "$DRY_RUN" = 1 ]; then
            info "+ $MARIADB -u root -e 'STOP SLAVE; RESET SLAVE ALL' (errors ignored)"
        else
            "$MARIADB" -u root -e "STOP SLAVE" >/dev/null 2>&1 || true
            if ! "$MARIADB" -u root -e "RESET SLAVE ALL"; then
                die "RESET SLAVE ALL failed — replication metadata was NOT cleared; fix the error above and rerun"
            fi
        fi
    fi
    if [ "$_rm_conf" = y ]; then remove_conf "$REPLICA_CONF"; fi
    if [ "$_rm_user" = y ] || [ -n "$_rm_local_user" ] || [ "$_rm_db" = y ] || [ -n "$_rm_local_db" ]; then
        # nothing dropped here may ever propagate anywhere — sql_log_bin=0 applies only when this server has binlog
        [ "$DRY_RUN" = 1 ] && _reset_lb=ON || _reset_lb=$(_log_bin_on)
        _drops=""
        [ "$_rm_user" = y ] && _drops="${_drops}DROP USER IF EXISTS 'mrpg_bridge'@'127.0.0.1';
"
        [ -n "$_rm_local_user" ] && _drops="${_drops}DROP USER IF EXISTS '$_rm_local_user'@'$_rm_local_user_host';
"
        [ "$_rm_db" = y ] && _drops="${_drops}DROP DATABASE IF EXISTS $SHARED_DB;
"
        [ -n "$_rm_local_db" ] && _drops="${_drops}DROP DATABASE IF EXISTS $_rm_local_db;
"
        _drops="${_drops}FLUSH PRIVILEGES;"
        if [ "$_reset_lb" = "ON" ]; then
            _drops="SET SESSION sql_log_bin = 0;
SET SESSION foreign_key_checks = 0;
${_drops}
SET SESSION foreign_key_checks = 1;
SET SESSION sql_log_bin = 1;"
        else
            _drops="SET SESSION foreign_key_checks = 0;
${_drops}
SET SESSION foreign_key_checks = 1;"
        fi
        printf '%s\n' "$_drops" | sql_stdin "the reset failed partway — see the errors above; the host may be half-reset (check menu 3 — status)"
    fi
    if [ "$_restart" = y ]; then restart_mariadb; fi

    echo
    ok "Replica configuration reset."
    info "Notes:"
    info "  • Replication metadata is gone (RESET SLAVE ALL); to replicate again the primary must be reachable — use menu 2."
    info "  • Relay logs are kept — purge them manually if needed."
    info "  • Removed config is backed up next to the original."
}

# ---------- GTID snapshots & recovery (1236) ----------

# extract_gtid_pos <dump> — print the primary GTID recorded by
# mariadb-dump --master-data=2 --gtid. The resulting SET GLOBAL
# gtid_slave_pos line is deliberately commented in a --master-data=2 dump,
# but it is still the exact point from which this replica must start.
extract_gtid_pos() {
    grep -aiEo "gtid_(slave|binlog)_pos[[:space:]]*=[[:space:]]*'[^']*'" "$1" 2>/dev/null |
        tail -1 | sed -e "s/^.*'\([^']*\)'.*$/\1/"
}

# check_gtid_pos <position> — dump content is data, never interpolate an
# unchecked value into SET GLOBAL. MariaDB GTID sets contain only digits,
# commas and hyphens (domain-server-sequence[/range]).
check_gtid_pos() {
    _checked_gtid=$(printf '%s' "$1" | tr -d '[:space:]')
    case "$_checked_gtid" in
        ''|*[!0-9,-]*) die "invalid GTID position in snapshot: '$1'" ;;
    esac
}

# set_gtid_slave_pos <position> — persist the fresh snapshot position before
# CHANGE MASTER ... MASTER_USE_GTID=slave_pos. mariadb-dump comments this SET
# when --master-data=2 is used, so importing the dump alone leaves the stale
# replica position in place and causes error 1236.
set_gtid_slave_pos() {
    if [ "$DRY_RUN" = 1 ]; then
        info "+ $MARIADB -u root -e \"SET GLOBAL gtid_slave_pos = '<GTID parsed from snapshot>';\""
        return 0
    fi
    check_gtid_pos "$1"
    sql_exec "SET GLOBAL gtid_slave_pos = '$_checked_gtid';"
}

# gtid_end_compare <dump-set> <primary-set> — decide whether the dump's end
# position can be served by the primary:
#   fresh    — every component of the dump ends exactly at the primary's current end (fresh --gtid dump)
#   behind   — the dump ends at or below the primary's current end (may already be purged — a stale-dump risk note, not a hard error)
#   foreign  — the dump references a position the primary never had (or an ahead one) — guaranteed 1236
# Unparseable input is reported as "behind" (inconclusive — never blocks the import).
gtid_end_compare() {
    printf '%s\n%s\n' "$1" "$2" | awk '
function comp_end(c,   p, n) {
    n = split(c, p, "-")
    if (n == 3 && p[1] ~ /^[0-9]+$/ && p[2] ~ /^[0-9]+$/) return p[3] + 0
    if (n == 4 && p[1] ~ /^[0-9]+$/ && p[2] ~ /^[0-9]+$/) return p[4] + 0
    return -1
}
function comp_key(c,   p) { split(c, p, "-"); return p[1] "-" p[2] }
NR == 1 { dn = split($0, dumps, ","); next }
NR == 2 {
    split($0, prims, ",")
    for (i in prims) {
        c = prims[i]; gsub(/ /, "", c); e = comp_end(c)
        if (e >= 0) { k = comp_key(c); if (!(k in pmap) || e > pmap[k]) pmap[k] = e }
    }
    foreign = 0; behind = 0; useful = 0
    for (i = 1; i <= dn; i++) {
        c = dumps[i]; gsub(/ /, "", c); e = comp_end(c)
        if (e < 0) continue
        useful = 1
        k = comp_key(c)
        if (!(k in pmap)) { foreign = 1; continue }
        if (e > pmap[k]) foreign = 1
        else if (e < pmap[k]) behind = 1
    }
    if (foreign) print "foreign"
    else if (!useful) print "behind"
    else if (behind) print "behind"
    else print "fresh"
}'
}

# reprovision_from_ssh — the ONLY correct recovery from
# 1236 "connecting slave requested to start from GTID ... which is not in the master's binlog":
# the snapshot is stale — the primary purged those events (or the dump came from a
# different/rebuilt primary). Skipping the gap would silently diverge the data, so we
# drop mrpg_shared, pull a FRESH snapshot and re-run CHANGE MASTER + START SLAVE.
reprovision_from_ssh() {
    warn "The snapshot is stale: the primary no longer has the binlog events the replica needs."
    prompt "SSH address of the primary (user@host)" "${FROM_PRIMARY:-root@$PRIMARY_HOST}"
    check_host "$REPLY"
    FROM_PRIMARY=$REPLY
    if [ "$DRY_RUN" = 1 ]; then
        info "+ ssh (background, progress) $FROM_PRIMARY '<fresh dump --gtid>' > <tmp>"
        info "+ DROP/CREATE $SHARED_DB; import; SET GLOBAL gtid_slave_pos=<snapshot>; CHANGE MASTER ... MASTER_USE_GTID=slave_pos; START SLAVE"
        return 0
    fi
    info "This will DROP mrpg_shared on this host and re-import it from a FRESH snapshot."
    confirm_typed_yes "mrpg_shared on THIS host will be dropped and re-imported."
    command -v ssh >/dev/null 2>&1 || die "an ssh client is required"
    _tmpdump=$(mktemp /tmp/mrpg_shared_snapshot.XXXXXX) || die "cannot create a temp file in /tmp — free some space and retry"
    tmp_snapshot_arm "$_tmpdump"
    info "Pulling a FRESH snapshot from $FROM_PRIMARY (this may take a while)."
    if ! fetch_snapshot "$FROM_PRIMARY" "$SHARED_DB" "$_tmpdump"; then
        tmp_snapshot_disarm
        die "fresh snapshot failed — see the messages above; fix the cause and retry (menu 2)"
    fi
    _fresh_gtid=$(extract_gtid_pos "$_tmpdump")
    if [ -z "$_fresh_gtid" ]; then
        tmp_snapshot_disarm
        die "the fresh snapshot has no SET GLOBAL gtid_slave_pos (it was not made with --gtid); refusing to start replication from an unknown position."
    fi
    check_gtid_pos "$_fresh_gtid"
    info "Fresh snapshot GTID position: $_fresh_gtid"
    "$MARIADB" -u root -e "STOP SLAVE" >/dev/null 2>&1 || true
    "$MARIADB" -u root -e "RESET SLAVE" >/dev/null 2>&1 || true
    sql_drop_db "$SHARED_DB"
    sql_exec "CREATE DATABASE IF NOT EXISTS $SHARED_DB CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
    import_dump "$SHARED_DB" "$_tmpdump" 1
    # See the equivalent setup path above: --master-data=2 comments this line
    # in the dump, so it must be applied explicitly after the import.
    set_gtid_slave_pos "$_fresh_gtid"
    tmp_snapshot_disarm
    if ! sql_stdin <<EOF
CHANGE MASTER TO
  MASTER_HOST='$PRIMARY_HOST',
  MASTER_PORT=$PRIMARY_PORT,
  MASTER_USER='mrpg_replica',
  MASTER_PASSWORD='$REPLICA_REPL_PASSWORD',
  MASTER_USE_GTID=slave_pos,
  MASTER_SSL=1;
START SLAVE;
EOF
    then
        die "CHANGE MASTER failed while re-provisioning — see the errors above. The fresh snapshot is imported, but replication was not configured; fix the cause and rerun menu 2."
    fi
    _t=0; _ok2=0; _st=""
    while [ "$_t" -lt 5 ]; do
        _st=$("$MARIADB" -u root -e "SHOW SLAVE STATUS\G" 2>/dev/null || true)
        _io2=$(printf '%s\n' "$_st" | sed -n 's/^ *Slave_IO_Running: //p' | head -1)
        _sql2=$(printf '%s\n' "$_st" | sed -n 's/^ *Slave_SQL_Running: //p' | head -1)
        if [ "$_io2" = "Yes" ] && [ "$_sql2" = "Yes" ]; then _ok2=1; break; fi
        _t=$((_t + 1)); sleep 2
    done
    if [ "$_ok2" = 1 ]; then
        ok "Replication is running again in slave_pos mode."
    else
        warn "still not running — details:"
        printf '%s\n' "$_st" | grep -E 'Slave_IO_Running|Slave_SQL_Running|Seconds_Behind_Master|Last_IO_Error|Last_SQL_Error' | sed 's/^/  /'
    fi
}

# ---------- post-setup: game server config, bridge, systemd unit ----------
# These use the values gathered earlier in the replica flow (bridge host/port,
# local credentials) plus a few extra questions, and never touch anything
# without showing the plan or without the user's explicit answer.

# print_game_block — the fresh #sql block for autoexec_server.cfg
print_game_block() {
    cat <<EOF

#sql
# shared database: through the bridge (setup_replication.sh)
sv_sql_host "127.0.0.1"
sv_sql_port 33060
sv_sql_login "$_shared_login"
sv_sql_password "$_shared_pw"
sv_sql_database_shared "$SHARED_DB"
sv_sql_pool_size $_shared_pool
sv_sql_use_tls 0

# local database: direct to the local MariaDB (setup_replication.sh)
sv_sql_local_host "127.0.0.1"
sv_sql_local_port 3306
sv_sql_local_login "$_local_login"
sv_sql_local_password "$_local_pw"
sv_sql_local_database "$_local_db"
sv_sql_local_pool_size $_local_pool
sv_sql_local_use_tls 0
EOF
}

configure_game_server() {
    info
    info "=== Step 1 of 4 — game server config (autoexec_server.cfg) ==="
    ask_yn "Configure the game server config now? Removes old sv_sql_* entries, writes the new ones." y
    [ "$REPLY" = y ] || { info "Skipping the game server config."; return 0; }

    _dir=""
    while :; do
        prompt_required "Directory containing autoexec_server.cfg"
        if [ -d "$REPLY" ]; then _dir=$REPLY; break; fi
        info "No such directory: $REPLY"
    done
    _cfg="$_dir/autoexec_server.cfg"
    [ -f "$_cfg" ] || die "file not found: $_cfg"

    prompt "Shared database login for the game server (through the bridge)" "mrpg_user"
    _shared_login=$REPLY
    check_ident "$_shared_login"
    prompt_secret "Password of $_shared_login (created on the primary during the master setup; also saved on the primary in /root/mrpg-replication-*.cred): "
    [ -z "$REPLY" ] && die "the shared password is required"
    _shared_pw=$REPLY
    check_password "$_shared_pw"

    if [ "${LOCAL_DB_YN:-n}" = y ]; then
        _local_login=$LOCAL_USER
        _local_pw=$LOCAL_USER_PASSWORD
        _local_db=$LOCAL_DB_NAME
        info "Local connection uses the database/login created earlier in this run: $_local_db / $_local_login"
    else
        # nothing was created in this run — offer what already exists on this host instead of a guessed name
        _all_dbs=""
        _all_users=""
        if [ "$DRY_RUN" != 1 ]; then
            _all_dbs=$(local_db_names)
            _all_users=$(local_user_names)
            [ -z "$_all_dbs" ] || info "Existing local databases: $(printf '%s' "$_all_dbs" | tr '\n' ' ')"
            [ -z "$_all_users" ] || info "Existing local users:     $(printf '%s' "$_all_users" | tr '\n' ' ')"
        fi
        _found_db=$(printf '%s\n' "$_all_dbs" | head -1)
        _found_user=$(printf '%s\n' "$_all_users" | head -1)
        prompt "Local database login" "${_found_user:-mrpg_local}"
        _local_login=$REPLY
        check_ident "$_local_login"
        prompt_secret "Password of $_local_login (created in an earlier run; see /root/mrpg-replication-*.cred on this host): "
        [ -z "$REPLY" ] && die "the local password is required"
        _local_pw=$REPLY
        check_password "$_local_pw"
        prompt "Local database name" "${_found_db:-mrpg_local_eu}"
        _local_db=$REPLY
        check_ident "$_local_db"
    fi

    prompt "sv_sql_pool_size (shared pool)" "8"
    is_number "$REPLY" || die "pool size must be a number"
    _shared_pool=$REPLY
    prompt "sv_sql_local_pool_size (local pool)" "2"
    is_number "$REPLY" || die "pool size must be a number"
    _local_pool=$REPLY

    GAME_SHARED_LOGIN=$_shared_login
    GAME_SHARED_PW=$_shared_pw
    GAME_LOCAL_LOGIN=$_local_login
    GAME_LOCAL_PW=$_local_pw
    GAME_LOCAL_DB=$_local_db

    info "Plan for $_cfg:"
    info "  • remove every line matching sv_sql_* (including commented-out ones) and the '#sql' marker"
    info "  • append a fresh #sql block (shared → 127.0.0.1:33060, local → 127.0.0.1:3306)"

    if [ "$DRY_RUN" = 1 ]; then
        info "+ cp -a $_cfg $_cfg.bak.<date>"
        info "+ filter out sv_sql_* and #sql lines, then append:"
        print_game_block | sed -e 's/^\(sv_sql_password \).*/\1"<hidden>"/' -e 's/^\(sv_sql_local_password \).*/\1"<hidden>"/' | sed 's/^/    | /'
        return 0
    fi

    _bak="$_cfg.bak.$(date +%Y%m%d_%H%M%S)"
    if ! cp -a "$_cfg" "$_bak"; then
        die "could not back up $_cfg to $_bak — check permissions; the config was NOT modified"
    fi
    _re_svsql='^[[:space:]]*#*[[:space:]]*sv_sql_'
    _re_sqlmarker='^[[:space:]]*#[[:space:]]*sql[[:space:]]*$'
    _removed=$(grep -cE "$_re_svsql|$_re_sqlmarker" "$_cfg" 2>/dev/null || true)
    _removed=$(printf '%s' "$_removed" | tr -d ' ')
    _new="$_cfg.new.$$"
    { grep -vE "$_re_svsql|$_re_sqlmarker" "$_cfg" || true; print_game_block; } > "$_new" || { rm -f "$_new"; die "could not build the new config (in $_new) — the original was not modified"; }
    if ! mv "$_new" "$_cfg"; then
        rm -f "$_new" || true
        die "could not replace $_cfg — restore it manually from the backup: cp -a $_bak $_cfg"
    fi
    # keep the previous owner/mode (the game server may run as a dedicated user)
    chown --reference="$_bak" "$_cfg" 2>/dev/null || true
    chmod --reference="$_bak" "$_cfg" 2>/dev/null || true
    ok "Removed $_removed old sv_sql entries from $_cfg (backup: $_bak) and wrote the new block"
}

BRIDGE_DIR=""

configure_bridge() {
    info
    info "=== Step 2 of 4 — bridge (bridge.cfg): local replica reads, primary relay ==="
    ask_yn "Configure the bridge (bridge.cfg) now?" y
    [ "$REPLY" = y ] || { info "Skipping the bridge configuration."; return 0; }

    BRIDGE_DIR=""
    _default_dir=""
    [ -f "${SCRIPT_DIR:-.}/mysql_bridge.py" ] && _default_dir=${SCRIPT_DIR:-.}
    while :; do
        prompt_required "Directory containing mysql_bridge.py" "$_default_dir"
        if [ -f "$REPLY/mysql_bridge.py" ]; then BRIDGE_DIR=$REPLY; break; fi
        info "mysql_bridge.py not found in: $REPLY"
    done
    _cfg="$BRIDGE_DIR/bridge.cfg"
    if [ -f "$_cfg" ]; then
        ask_yn "bridge.cfg already exists — overwrite it? (it will be backed up)" y
        [ "$REPLY" = y ] || { info "Keeping the existing bridge.cfg."; return 0; }
    fi

    prompt "Bridge listen host (the game server connects here)" "127.0.0.1"
    _listen_host=$REPLY
    check_host "$_listen_host"
    prompt "Bridge listen port" "33060"
    is_number "$REPLY" || die "port must be a number"
    _listen_port=$REPLY
    [ "$_listen_port" -ge 1 ] 2>/dev/null && [ "$_listen_port" -le 65535 ] 2>/dev/null || die "the port must be between 1 and 65535"
    prompt "Primary (master) host" "${PRIMARY_HOST:-db-central.example.com}"
    check_host "$REPLY"
    _ph=$REPLY
    prompt "Primary port" "${PRIMARY_PORT:-3306}"
    is_number "$REPLY" || die "port must be a number"
    _pp=$REPLY
    ask_yn "Use TLS for the primary connection?" n
    _ptls=$REPLY
    prompt "Local replica host" "127.0.0.1"
    _rh=$REPLY
    check_host "$_rh"
    prompt "Local replica port" "3306"
    is_number "$REPLY" || die "port must be a number"
    _rp=$REPLY
    prompt "Bridge user on the replica (SELECT only)" "mrpg_bridge"
    _ru=$REPLY
    check_ident "$_ru"
    prompt "Replica database" "$SHARED_DB"
    _rdb=$REPLY
    check_ident "$_rdb"
    ask_yn "Use TLS for the replica connection?" n
    _rtls=$REPLY
    prompt "read_your_writes_grace_sec (keep above the replication lag)" "2.0"
    case "$REPLY" in *[!0-9.]*|'') die "must be a number like 2.0" ;; *) _grace=$REPLY ;; esac

    if [ "$_ru" = "mrpg_bridge" ] && [ -n "${BRIDGE_READ_PASSWORD:-}" ]; then
        _replica_password=$BRIDGE_READ_PASSWORD
        info "Using the local mrpg_bridge created earlier in this run (password is not echoed again)."
    else
        prompt_secret "Password of $_ru: "
        [ -z "$REPLY" ] && die "the replica password is required"
        _replica_password=$REPLY
        check_password "$_replica_password"
    fi
    BRIDGE_CFG_USER=$_ru
    BRIDGE_CFG_PASSWORD=$_replica_password
    BRIDGE_CFG_HOST=$_rh
    BRIDGE_CFG_PORT=$_rp
    BRIDGE_CFG_DB=$_rdb
    BRIDGE_LISTEN_HOST=$_listen_host
    BRIDGE_LISTEN_PORT=$_listen_port

    print_bridge_cfg() {
        cat <<EOF
# mysql_bridge.py config (generated by setup_replication.sh on $(date))
[bridge]
listen_host = $_listen_host
listen_port = $_listen_port
max_clients = 64
replica_pool_size = 16
route_selects_to_replica = true
read_your_writes_grace_sec = $_grace
stats_interval_sec = 30

[primary]
host = $_ph
port = $_pp
use_tls = $([ "$_ptls" = y ] && printf true || printf false)

[replica]
host = $_rh
port = $_rp
user = $_ru
password = $_replica_password
database = $_rdb
use_tls = $([ "$_rtls" = y ] && printf true || printf false)
EOF
    }

    if [ "$DRY_RUN" = 1 ]; then
        info "+ write $_cfg (chmod 600) with:"
        print_bridge_cfg | sed -e 's/^\(password = \).*/\1***/' | sed 's/^/    | /'
        return 0
    fi

    if [ -f "$_cfg" ]; then
        _bak="$_cfg.bak.$(date +%Y%m%d_%H%M%S)"
        if ! cp -a "$_cfg" "$_bak"; then
            die "could not back up $_cfg to $_bak — NOT overwriting it (fix the backup path first)"
        fi
        info "Previous bridge.cfg saved to: $_bak"
    fi
    _bnew="$_cfg.new.$$"
    if ! ( umask 077; print_bridge_cfg > "$_bnew" ); then
        rm -f "$_bnew" || true
        die "could not write $_cfg — the previous file (if any) is untouched${_bak:+ (backup: $_bak)}"
    fi
    if ! chmod 600 "$_bnew"; then
        rm -f "$_bnew" || true
        die "could not chmod 600 bridge.cfg — the previous file (if any) is untouched"
    fi
    if ! mv "$_bnew" "$_cfg"; then
        rm -f "$_bnew" || true
        die "could not replace $_cfg${_bak:+ — restore it manually: cp -a $_bak $_cfg}"
    fi
    ok "Written $_cfg (chmod 600)"
    if command -v python3 >/dev/null 2>&1; then
        if python3 "$BRIDGE_DIR/mysql_bridge.py" "$_cfg" --check-config; then
            ok "bridge.cfg passed mysql_bridge.py --check-config"
        else
            warn "mysql_bridge.py rejected the config — check $_cfg (see other/sql_bridge/README.md, step 3)"
        fi
    else
        warn "python3 not found — cannot validate bridge.cfg (check it manually)"
    fi

    # the bridge logs into the LOCAL MariaDB with this account — prove it works now;
    # if not (or a custom account name was chosen), offer to create/fix it
    if [ "$DRY_RUN" = 1 ]; then
        info "+ verify the replica login: MYSQL_PWD=*** $MARIADB -h $_rh -P $_rp -u $_ru $_rdb -e 'SELECT 1 LIMIT 1'"
        info "+ if it fails: CREATE USER IF NOT EXISTS '$_ru'@'$_rh' (or ALTER USER) + GRANT SELECT ON $_rdb.*"
    elif [ "$_rh" = "127.0.0.1" ] || [ "$_rh" = "localhost" ] || [ "$_rh" = "::1" ]; then
        if ! MYSQL_PWD="$_replica_password" "$MARIADB" -h "$_rh" -P "$_rp" -u "$_ru" "$_rdb" -e 'SELECT 1 LIMIT 1' >/dev/null 2>&1; then
            warn "the account '$_ru'@'$_rh' cannot SELECT from $_rdb with this password — the bridge would not work."
            ask_yn "Create/fix '$_ru'@'$_rh' now (SELECT-only on $_rdb, password as entered)?" y
            if [ "$REPLY" = y ]; then
                sql_stdin "could not create/fix '$_ru'@'$_rh' — see the errors above" <<EOF
SET SESSION sql_log_bin = 0;
CREATE USER IF NOT EXISTS '$_ru'@'$_rh' IDENTIFIED BY '$_replica_password';
ALTER USER '$_ru'@'$_rh' IDENTIFIED BY '$_replica_password';
GRANT SELECT ON $_rdb.* TO '$_ru'@'$_rh';
FLUSH PRIVILEGES;
SET SESSION sql_log_bin = 1;
EOF
                if MYSQL_PWD="$_replica_password" "$MARIADB" -h "$_rh" -P "$_rp" -u "$_ru" "$_rdb" -e 'SELECT 1 LIMIT 1' >/dev/null 2>&1; then
                    ok "the account '$_ru'@'$_rh' works (SELECT-only on $_rdb)"
                else
                    warn "still cannot connect with '$_ru'@'$_rh' — check the password and the local MariaDB manually"
                fi
            fi
        else
            ok "the bridge's replica login works ('$_ru'@'$_rh', SELECT on $_rdb)"
        fi
    else
        info "note: the replica host is remote ($_rh) — its account must exist there; verify with:"
        info "  MYSQL_PWD=*** $MARIADB -h $_rh -P $_rp -u $_ru $_rdb -e 'SELECT 1'"
    fi
}

install_bridge_service() {
    info
    info "=== Step 3 of 4 — systemd service (mrpg-mysql-bridge) ==="
    ask_yn "Install the bridge as a systemd service (mrpg-mysql-bridge)?" y
    [ "$REPLY" = y ] || { info "Skipping the systemd service."; return 0; }
    if ! command -v systemctl >/dev/null 2>&1; then
        warn "systemctl not found — create a systemd unit manually (README, step 3); example in the docs"
        return 0
    fi

    if [ -z "$BRIDGE_DIR" ]; then
        _default_dir=""
        [ -f "${SCRIPT_DIR:-.}/mysql_bridge.py" ] && [ -f "${SCRIPT_DIR:-.}/bridge.cfg" ] && _default_dir=${SCRIPT_DIR:-.}
        while :; do
            prompt_required "Directory containing mysql_bridge.py and bridge.cfg" "$_default_dir"
            if [ -f "$REPLY/mysql_bridge.py" ] && [ -f "$REPLY/bridge.cfg" ]; then BRIDGE_DIR=$REPLY; break; fi
            info "mysql_bridge.py and/or bridge.cfg not found in: $REPLY"
        done
    fi

    ask_yn "Run the bridge as a dedicated 'mrpg' system user?" y
    _dedicated=$REPLY
    _run_user=root
    if [ "$_dedicated" = y ]; then
        if id mrpg >/dev/null 2>&1; then
            info "User 'mrpg' already exists — keeping it."
        elif [ "$DRY_RUN" = 1 ]; then
            info "+ useradd -r -s /usr/sbin/nologin mrpg"
        else
            if useradd -r -s /usr/sbin/nologin mrpg; then
                ok "Created system user 'mrpg'"
            else
                warn "could not create the 'mrpg' system user — the service (User=mrpg) will fail to start; create it manually: useradd -r -s /usr/sbin/nologin mrpg (or rerun and choose 'run as root')"
            fi
        fi
        if [ "$DRY_RUN" = 1 ]; then
            info "+ chown mrpg:mrpg $BRIDGE_DIR/bridge.cfg (600; the rest stays untouched)"
        else
            if chown mrpg:mrpg "$BRIDGE_DIR/bridge.cfg"; then
                ok "bridge.cfg is owned by mrpg:mrpg (the rest of $BRIDGE_DIR is untouched)"
            else
                warn "could not chown bridge.cfg to mrpg — the service (User=mrpg) may be unable to read it: chown mrpg:mrpg $BRIDGE_DIR/bridge.cfg"
            fi
        fi
        _run_user=mrpg
    else
        warn "Running the bridge as root is not recommended — the 'mrpg' system user keeps privileges minimal."
    fi
    _py=$(command -v python3 || printf /usr/bin/python3)

    ask_yn "Enable the service on boot (autostart)?" y
    _enable=$REPLY
    ask_yn "Start the service now?" y
    _start=$REPLY

    _unit_path=/etc/systemd/system/mrpg-mysql-bridge.service
    info "Plan (systemd unit $_unit_path):"
    info "  • User=$_run_user, WorkingDirectory=$BRIDGE_DIR"
    info "  • ExecStart=$_py $BRIDGE_DIR/mysql_bridge.py $BRIDGE_DIR/bridge.cfg"
    info "  • enable on boot: $([ "$_enable" = y ] && printf yes || printf no); start now: $([ "$_start" = y ] && printf yes || printf no)"

    _write_unit() {
        cat <<EOF
[Unit]
Description=Teeworlds-MRPG MySQL bridge
After=network-online.target mariadb.service
Wants=network-online.target

[Service]
Type=simple
User=$_run_user
WorkingDirectory=$BRIDGE_DIR
ExecStart=$_py $BRIDGE_DIR/mysql_bridge.py $BRIDGE_DIR/bridge.cfg
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF
    }

    if [ "$DRY_RUN" = 1 ]; then
        info "+ write $_unit_path (backup if exists):"
        _write_unit | sed 's/^/    | /'
        info "+ systemctl daemon-reload"
        [ "$_enable" = y ] && info "+ systemctl enable mrpg-mysql-bridge"
        [ "$_start" = y ]  && info "+ systemctl start mrpg-mysql-bridge"
        return 0
    fi

    if [ -f "$_unit_path" ]; then
        _bak="$_unit_path.bak.$(date +%Y%m%d_%H%M%S)"
        if ! cp -a "$_unit_path" "$_bak"; then
            die "could not back up $_unit_path to $_bak — NOT overwriting it"
        fi
        info "Previous unit saved to: $_bak"
    fi
    _unew="$_unit_path.new.$$"
    if ! _write_unit > "$_unew"; then
        rm -f "$_unew" || true
        die "could not write $_unit_path — the previous unit (if any) is untouched"
    fi
    if ! mv "$_unew" "$_unit_path"; then
        rm -f "$_unew" || true
        die "could not replace $_unit_path${_bak:+ — restore it manually: cp -a $_bak $_unit_path}"
    fi
    ok "Written $_unit_path"
    if ! systemctl daemon-reload; then
        die "systemctl daemon-reload failed — check systemd (systemctl status)"
    fi
    if [ "$_enable" = y ]; then
        if systemctl enable mrpg-mysql-bridge; then ok "Enabled on boot"; else warn "could not enable the service — do it manually: systemctl enable mrpg-mysql-bridge"; fi
    fi
    if [ "$_start" = y ]; then
        if ! systemctl start mrpg-mysql-bridge; then
            warn "systemctl start failed — check: journalctl -xeu mrpg-mysql-bridge"
        fi
        sleep 2
        if systemctl is-active --quiet mrpg-mysql-bridge; then
            ok "mrpg-mysql-bridge is active (logs: journalctl -u mrpg-mysql-bridge -f)"
            if [ -n "$BRIDGE_LISTEN_PORT" ] && command -v python3 >/dev/null 2>&1; then
                if python3 -c 'import socket,sys; s=socket.socket(); s.settimeout(2); sys.exit(0 if s.connect_ex((sys.argv[1], int(sys.argv[2])))==0 else 1)' "$BRIDGE_LISTEN_HOST" "$BRIDGE_LISTEN_PORT" 2>/dev/null; then
                    ok "the bridge is accepting connections on $BRIDGE_LISTEN_HOST:$BRIDGE_LISTEN_PORT"
                    BRIDGE_SERVICE_UP=1
                else
                    warn "the service is active but $BRIDGE_LISTEN_HOST:$BRIDGE_LISTEN_PORT is not accepting connections yet — check: journalctl -xeu mrpg-mysql-bridge"
                fi
            else
                BRIDGE_SERVICE_UP=1
            fi
        else
            warn "service did not start — check: journalctl -xeu mrpg-mysql-bridge"
        fi
    fi
}

# final_ready_check — the last thing the replica script does: prove (as far as
# possible from this host) what the game server will do: replication running,
# the local DB answering, and the bridge relaying to the primary.
final_ready_check() {
    info
    info "=== Step 4 of 4 — final verification (the same calls the game server makes) ==="
    if [ "$DRY_RUN" = 1 ]; then
        info "+ replication: SHOW SLAVE STATUS (IO/SQL threads, Seconds_Behind_Master)"
        info "+ local database: direct TCP login plus INSERT/ROLLBACK test with the sv_sql_local_* account"
        info "+ shared database through the bridge: TCP login test with the sv_sql_* account"
        info "(no connections are attempted in dry-run mode)"
        return 0
    fi

    _rfail=0

    # 1) replication
    _status=$("$MARIADB" -u root -e "SHOW SLAVE STATUS\G" 2>/dev/null || true)
    _io=$(printf '%s\n' "$_status" | sed -n 's/^ *Slave_IO_Running: //p' | head -1)
    _sql=$(printf '%s\n' "$_status" | sed -n 's/^ *Slave_SQL_Running: //p' | head -1)
    if [ "$_io" = "Yes" ] && [ "$_sql" = "Yes" ]; then
        _lag=$(printf '%s\n' "$_status" | sed -n 's/^ *Seconds_Behind_Master: //p' | head -1)
        ok "replication is running (IO/SQL: Yes/Yes; Seconds_Behind_Master: ${_lag:-?})"
    else
        warn "replication is NOT running (IO: ${_io:-?}, SQL: ${_sql:-?}) — the shared DB on this host is stale"
        printf '%s\n' "$_status" | grep -E 'Last_IO_Error|Last_SQL_Error|Slave_IO_Running|Slave_SQL_Running' | sed 's/^/  /'
        _last_io=$(printf '%s\n' "$_status" | sed -n 's/^ *Last_IO_Error: //p' | head -1)
        case "$_last_io" in
            *1236*|*"not in the master's binlog"*)
                info "  → Last_IO_Error 1236: the snapshot is stale or FOREIGN (from another/rebuilt primary)."
                info "    Pull a fresh one (menu 2, snapshot option 2) — never skip this gap; see the recovery offer above." ;;
        esac
        _rfail=$((_rfail + 1))
    fi

    # 2) The local database is reached directly from the game server, never
    # through the bridge. Test a real write because SELECT alone cannot catch
    # the read_only=1 failure that would reject guild creation at runtime.
    if [ -n "$GAME_LOCAL_LOGIN" ]; then
        _lu=$GAME_LOCAL_LOGIN; _lp=$GAME_LOCAL_PW; _ldb=$GAME_LOCAL_DB
    elif [ "${LOCAL_DB_YN:-n}" = "y" ]; then
        _lu=$LOCAL_USER; _lp=$LOCAL_USER_PASSWORD; _ldb=$LOCAL_DB_NAME
    else
        _lu=""
    fi
    if [ -n "$_lu" ]; then
        # Negative IDs are not generated by the game. The transaction is
        # rolled back, so the probe never changes local game state.
        _probe_id=$(( - ($$ % 2000000000) - 1 ))
        if MYSQL_PWD="$_lp" "$MARIADB" --connect-timeout=5 -h 127.0.0.1 -P 3306 -u "$_lu" "$_ldb" -e "START TRANSACTION; INSERT INTO tw_guilds_ranks (ID, Rights, GuildID, Name) VALUES ($_probe_id, 0, $_probe_id, '__mrpg_write_probe__'); ROLLBACK;" >/dev/null 2>&1; then
            ok "local database is directly readable and writable ($_ldb, login $_lu → 127.0.0.1:3306)"
        else
            warn "local database is not writable with the sv_sql_local_* account ($_lu → $_ldb at 127.0.0.1:3306)"
            info "  The game would receive MariaDB error 1290 on guild writes. Re-run replica setup to grant ALL PRIVILEGES on $_ldb.* plus READ ONLY ADMIN to the configured local account, then check sv_sql_local_host is 127.0.0.1 (not the bridge)."
            _rfail=$((_rfail + 1))
        fi
    else
        info "local database test skipped (the local side was not configured)"
    fi

    # 3) the shared database THROUGH THE BRIDGE (the sv_sql_* connection)
    if [ "$BRIDGE_SERVICE_UP" = 1 ] && [ -n "$GAME_SHARED_LOGIN" ]; then
        if MYSQL_PWD="$GAME_SHARED_PW" "$MARIADB" --connect-timeout=5 -h "$BRIDGE_LISTEN_HOST" -P "$BRIDGE_LISTEN_PORT" -u "$GAME_SHARED_LOGIN" "$SHARED_DB" -e 'SELECT 1 LIMIT 1' >/dev/null 2>&1; then
            ok "shared database works through the bridge ($GAME_SHARED_LOGIN → $BRIDGE_LISTEN_HOST:$BRIDGE_LISTEN_PORT → primary)"
        else
            warn "the shared database does NOT answer through the bridge ($GAME_SHARED_LOGIN)"
            info "  the bridge is up but the login failed — the usual causes:"
            info "  • the password of $GAME_SHARED_LOGIN is not the one from the master setup (it is saved on the PRIMARY in /root/mrpg-replication-*.cred)"
            info "  • the primary is unreachable from this host: port ${PRIMARY_PORT:-3306}, bind-address, ufw"
            info "  • the host mask of $GAME_SHARED_LOGIN on the primary (e.g. '%') does not cover this host"
            _rfail=$((_rfail + 1))
        fi
    elif [ -n "$GAME_SHARED_LOGIN" ]; then
        info "shared database through the bridge not tested (the bridge is not running)"
        info "  once it is: MYSQL_PWD=*** $MARIADB -h ${BRIDGE_LISTEN_HOST:-127.0.0.1} -P ${BRIDGE_LISTEN_PORT:-33060} -u $GAME_SHARED_LOGIN $SHARED_DB -e 'SELECT 1'"
    fi

    echo
    if [ "$_rfail" = 0 ]; then
        ok "The server is ready to work."
        info "  • restart the game server (it reads autoexec_server.cfg at startup)"
        info "  • sv_sql_* → the bridge (127.0.0.1:33060); sv_sql_local_* → the local MariaDB (127.0.0.1:3306)"
        info "  • replication health: $MARIADB -u root -e 'SHOW SLAVE STATUS\G' | grep -E 'Slave_IO_Running|Slave_SQL_Running|Seconds_Behind'"
    else
        warn "The server is NOT fully ready — fix the items marked ⚠ above."
        info "  the game server would see exactly the same errors; each item names its fix."
    fi
}

# ---------- entry point ----------
[ $# -eq 0 ] || die "this script takes no arguments: sudo sh $0 (all values are asked interactively)"

if [ "$(id -u)" = 0 ]; then IS_ROOT=1; else IS_ROOT=0; fi

echo "══════════════════════════════════════════"
echo "  Teeworlds-MRPG — MariaDB setup"
echo "══════════════════════════════════════════"
[ "$IS_ROOT" = 1 ] || warn "You are not root: applying changes requires sudo (the dry-run mode works without root)"
echo "What would you like to do?"
echo "  1) primary (master) — the central database"
echo "  2) replica — read replica in this location"
echo "  3) status — show replication state"
echo "  4) reset — remove the configuration created by this script"
echo "  0) exit"
while :; do
    printf 'Choice [1]: '
    read -r _choice || die "input closed (EOF)"
    [ -z "$_choice" ] && _choice=1
    case "$_choice" in
        1) do_master; exit 0 ;;
        2) do_replica; exit 0 ;;
        3) do_status; exit 0 ;;
        4) do_reset; exit 0 ;;
        0) echo "Bye."; exit 0 ;;
        *) info "Choose 1, 2, 3, 4 or 0." ;;
    esac
done
