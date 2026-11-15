#!/usr/bin/env bash
set -Eeuo pipefail

export LANG=C
export LC_ALL=C

# ============================================================
# Configuration
# ============================================================
PROJECT_DIR="${PROJECT_DIR:-/root/Teeworlds-MRPG-0.6}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"

# Directory where this script lives — all artifacts are deployed here.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EXECUTABLE_NAME="${EXECUTABLE_NAME:-mmoteeworlds_srv}"
TESTRUNNER_NAME="${TESTRUNNER_NAME:-testrunner}"
RELAY_EXECUTABLE_NAME="${RELAY_EXECUTABLE_NAME:-mmoteeworlds_relay}"

EXECUTABLE_IN_BUILD_DIR="${EXECUTABLE_IN_BUILD_DIR:-$BUILD_DIR/$EXECUTABLE_NAME}"
TESTRUNNER_IN_BUILD_DIR="${TESTRUNNER_IN_BUILD_DIR:-$BUILD_DIR/$TESTRUNNER_NAME}"
RELAY_IN_BUILD_DIR="${RELAY_IN_BUILD_DIR:-$BUILD_DIR/$RELAY_EXECUTABLE_NAME}"

DEPLOY_EXECUTABLE="${DEPLOY_EXECUTABLE:-$SCRIPT_DIR/$EXECUTABLE_NAME}"
DEPLOY_TESTRUNNER="${DEPLOY_TESTRUNNER:-$SCRIPT_DIR/$TESTRUNNER_NAME}"
DEPLOY_RELAY_EXECUTABLE="${DEPLOY_RELAY_EXECUTABLE:-$SCRIPT_DIR/$RELAY_EXECUTABLE_NAME}"

SERVER_SERVICE_NAME="${SERVER_SERVICE_NAME:-mmoteeworlds-server.service}"
TOOLS_SERVICE_NAME="${TOOLS_SERVICE_NAME:-mmoteeworlds-tools.service}"
GUARD_SERVICE_NAME="${GUARD_SERVICE_NAME:-mmoteeworlds-replication-guard.service}"
GUARD_UNIT_PATH="${GUARD_UNIT_PATH:-/etc/systemd/system/$GUARD_SERVICE_NAME}"
GUARD_SCRIPT="$PROJECT_DIR/other/sql_bridge/replication_guard.sh"
RELAY_SERVICE_NAME="${RELAY_SERVICE_NAME:-mmoteeworlds-relay.service}"
SERVER_SERVICE_UNIT_PATH="${SERVER_SERVICE_UNIT_PATH:-/etc/systemd/system/$SERVER_SERVICE_NAME}"
TOOLS_SERVICE_UNIT_PATH="${TOOLS_SERVICE_UNIT_PATH:-/etc/systemd/system/$TOOLS_SERVICE_NAME}"
RELAY_SERVICE_UNIT_PATH="${RELAY_SERVICE_UNIT_PATH:-/etc/systemd/system/$RELAY_SERVICE_NAME}"

SERVICE_USER="${SERVICE_USER:-root}"
SERVICE_GROUP="${SERVICE_GROUP:-root}"
SERVICE_WORKDIR="${SERVICE_WORKDIR:-$SCRIPT_DIR}"

LOG_DIR="${LOG_DIR:-/root/mmorpg/mmoteeworlds}"
SERVER_LOG_FILE="${SERVER_LOG_FILE:-$LOG_DIR/server.log}"
SERVER_SUPERVISOR_LOG_FILE="${SERVER_SUPERVISOR_LOG_FILE:-$LOG_DIR/server_supervisor.log}"
TOOLS_LOG_FILE="${TOOLS_LOG_FILE:-$LOG_DIR/tools.log}"
TOOLS_SUPERVISOR_LOG_FILE="${TOOLS_SUPERVISOR_LOG_FILE:-$LOG_DIR/tools_supervisor.log}"
RELAY_LOG_FILE="${RELAY_LOG_FILE:-$LOG_DIR/relay.log}"
RELAY_SUPERVISOR_LOG_FILE="${RELAY_SUPERVISOR_LOG_FILE:-$LOG_DIR/relay_supervisor.log}"
LOGROTATE_FILE="${LOGROTATE_FILE:-/etc/logrotate.d/mmoteeworlds}"
SERVER_LAUNCHER_SCRIPT="${SERVER_LAUNCHER_SCRIPT:-$SCRIPT_DIR/run_${EXECUTABLE_NAME}_server.sh}"
TOOLS_LAUNCHER_SCRIPT="${TOOLS_LAUNCHER_SCRIPT:-$SCRIPT_DIR/run_mmoteeworlds_tools.sh}"
RELAY_LAUNCHER_SCRIPT="${RELAY_LAUNCHER_SCRIPT:-$SCRIPT_DIR/run_${RELAY_EXECUTABLE_NAME}_relay.sh}"
BRIDGE_STACK_SCRIPT="${BRIDGE_STACK_SCRIPT:-/root/mmorpg/tools/run_bridge_stack.sh}"

GDB_LOG_DIR="${GDB_LOG_DIR:-/root/mmorpg/mmoteeworlds/gdb}"
ASAN_DEBUG_LOG_FILENAME="${ASAN_DEBUG_LOG_FILENAME:-asan_debug_output.log}"
LAST_BUILD_MODE_FILE="${LAST_BUILD_MODE_FILE:-$BUILD_DIR/.last_build_mode}"

AUTO_RESTART_SERVICE="${AUTO_RESTART_SERVICE:-1}"

# ============================================================
# Common helpers
# ============================================================
print_header() {
    echo
    echo "============================================================"
    echo " $1"
    echo "============================================================"
}

log() {
    echo "[INFO] $(date '+%F %T') $*"
}

warn() {
    echo "[WARN] $*" >&2
}

die() {
    echo "[ERROR] $*" >&2
    exit 1
}

ensure_dir_exists() {
    local dir="$1"
    [[ -d "$dir" ]] || mkdir -p "$dir"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "Command not found: $1"
}

run_sudo() {
    if [[ $EUID -eq 0 ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

show_config_summary() {
    print_header "CONFIG SUMMARY"
    cat <<EOF2
PROJECT_DIR=$PROJECT_DIR
BUILD_DIR=$BUILD_DIR
SCRIPT_DIR=$SCRIPT_DIR
EXECUTABLE_NAME=$EXECUTABLE_NAME
TESTRUNNER_NAME=$TESTRUNNER_NAME
RELAY_EXECUTABLE_NAME=$RELAY_EXECUTABLE_NAME
SERVER_SERVICE_NAME=$SERVER_SERVICE_NAME
TOOLS_SERVICE_NAME=$TOOLS_SERVICE_NAME
RELAY_SERVICE_NAME=$RELAY_SERVICE_NAME
SERVER_SERVICE_UNIT_PATH=$SERVER_SERVICE_UNIT_PATH
TOOLS_SERVICE_UNIT_PATH=$TOOLS_SERVICE_UNIT_PATH
RELAY_SERVICE_UNIT_PATH=$RELAY_SERVICE_UNIT_PATH
SERVER_LAUNCHER_SCRIPT=$SERVER_LAUNCHER_SCRIPT
TOOLS_LAUNCHER_SCRIPT=$TOOLS_LAUNCHER_SCRIPT
RELAY_LAUNCHER_SCRIPT=$RELAY_LAUNCHER_SCRIPT
BRIDGE_STACK_SCRIPT=$BRIDGE_STACK_SCRIPT
SERVER_LOG_FILE=$SERVER_LOG_FILE
SERVER_SUPERVISOR_LOG_FILE=$SERVER_SUPERVISOR_LOG_FILE
TOOLS_LOG_FILE=$TOOLS_LOG_FILE
TOOLS_SUPERVISOR_LOG_FILE=$TOOLS_SUPERVISOR_LOG_FILE
RELAY_LOG_FILE=$RELAY_LOG_FILE
RELAY_SUPERVISOR_LOG_FILE=$RELAY_SUPERVISOR_LOG_FILE
GDB_LOG_DIR="${GDB_LOG_DIR:-/root/mmorpg/mmoteeworlds/gdb}"
EOF2
}

validate_config() {
    print_header "VALIDATE CONFIG"

    [[ -n "$PROJECT_DIR" ]] || die "PROJECT_DIR is empty"
    [[ -n "$BUILD_DIR" ]] || die "BUILD_DIR is empty"
    [[ -n "$SCRIPT_DIR" ]] || die "SCRIPT_DIR is empty"
    [[ -n "$EXECUTABLE_NAME" ]] || die "EXECUTABLE_NAME is empty"
    [[ -n "$RELAY_EXECUTABLE_NAME" ]] || die "RELAY_EXECUTABLE_NAME is empty"
    [[ -n "$SERVER_SERVICE_NAME" ]] || die "SERVER_SERVICE_NAME is empty"
    [[ -n "$TOOLS_SERVICE_NAME" ]] || die "TOOLS_SERVICE_NAME is empty"
    [[ -n "$RELAY_SERVICE_NAME" ]] || die "RELAY_SERVICE_NAME is empty"
    [[ -n "$SERVER_SERVICE_UNIT_PATH" ]] || die "SERVER_SERVICE_UNIT_PATH is empty"
    [[ -n "$TOOLS_SERVICE_UNIT_PATH" ]] || die "TOOLS_SERVICE_UNIT_PATH is empty"
    [[ -n "$RELAY_SERVICE_UNIT_PATH" ]] || die "RELAY_SERVICE_UNIT_PATH is empty"
    [[ -n "$SERVER_LAUNCHER_SCRIPT" ]] || die "SERVER_LAUNCHER_SCRIPT is empty"
    [[ -n "$RELAY_LAUNCHER_SCRIPT" ]] || die "RELAY_LAUNCHER_SCRIPT is empty"

    [[ -d "$PROJECT_DIR" ]] || die "Project directory not found: $PROJECT_DIR"
    [[ -d "$PROJECT_DIR/.git" ]] || die "Not a git repository: $PROJECT_DIR"

    log "Configuration validation passed."
}

# ============================================================
# Dependency checks
# ============================================================
check_dependencies() {
    local deps=(
        git
        cmake
        systemctl
        find
        grep
        sed
        awk
        date
        tee
        mktemp
    )

    local cmd
    for cmd in "${deps[@]}"; do
        require_command "$cmd"
    done
}

check_optional_dependencies() {
    local optional=(
        curl
        gdb
        valgrind
        logrotate
        chmod
    )

    local cmd
    for cmd in "${optional[@]}"; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            warn "Optional command not found: $cmd"
        fi
    done
}

# ============================================================
# Bootstrap: service, logger, runtime files
# ============================================================
create_server_launcher_script() {
	pkill -9 -f "$DEPLOY_EXECUTABLE" 2>/dev/null || true
	pkill -9 -f "gdb .*${EXECUTABLE_NAME}" 2>/dev/null || true
	sleep 1

    print_header "CREATE SERVER LAUNCHER SCRIPT"
    ensure_dir_exists "$SCRIPT_DIR"

    cat > "$SERVER_LAUNCHER_SCRIPT" <<EOF2
#!/usr/bin/env bash
set -Eeuo pipefail

cd "$SERVICE_WORKDIR"
umask 027

LOG_DIR="$LOG_DIR"
SERVER_LOG_FILE="$SERVER_LOG_FILE"
SERVER_SUPERVISOR_LOG_FILE="$SERVER_SUPERVISOR_LOG_FILE"
GDB_LOG_DIR="$GDB_LOG_DIR"
DEPLOY_EXECUTABLE="$DEPLOY_EXECUTABLE"
EXECUTABLE_NAME="$EXECUTABLE_NAME"

mkdir -p "\$LOG_DIR" "\$GDB_LOG_DIR"
touch "\$SERVER_LOG_FILE" "\$SERVER_SUPERVISOR_LOG_FILE"

supervisor_log() {
    echo "[SERVER-SUPERVISOR] \$(date '+%F %T') \$*" | tee -a "\$SERVER_SUPERVISOR_LOG_FILE"
}

server_log() {
    echo "[SERVER] \$(date '+%F %T') \$*" >> "\$SERVER_LOG_FILE"
}

run_server_once() {
    local started_at
    local stamp
    local gdb_crash_log
    local gdb_commands_file
    local rc=0

    started_at="\$(date '+%F %T')"
    stamp="\$(date +%F_%H%M%S_%N)"
    gdb_crash_log="\$GDB_LOG_DIR/gdb_crash_\${stamp}_${EXECUTABLE_NAME}.log"

    [[ -x "\$DEPLOY_EXECUTABLE" ]] || {
        server_log "Executable not found or not executable: \$DEPLOY_EXECUTABLE"
        return 127
    }

    if ! command -v gdb >/dev/null 2>&1; then
        server_log "gdb not found, running directly without crash backtrace"
        "\$DEPLOY_EXECUTABLE" >> "\$SERVER_LOG_FILE" 2>&1
        return \$?
    fi

    gdb_commands_file="\$(mktemp /tmp/gdb_commands_${EXECUTABLE_NAME}.XXXXXX)"

    {
        printf '%s\n' 'set pagination off'
        printf '%s\n' 'set confirm off'
        printf '%s\n' 'set print thread-events off'
        printf '%s\n' 'set logging overwrite on'
        printf 'set logging file %s\n' "\$gdb_crash_log"
        printf '%s\n' 'set logging enabled on'
        printf '%s\n' 'handle SIGPIPE nostop noprint pass'
        printf '%s\n' 'run'
        printf '%s\n' 'echo \n=== PROGRAM STATE ===\n'
        printf '%s\n' 'info program'
        printf '%s\n' 'if \$_isvoid(\$_exitsignal)'
        printf '%s\n' '  if \$_isvoid(\$_exitcode)'
        printf '%s\n' '    echo Program state is unavailable.\n'
        printf '%s\n' '  else'
        printf '%s\n' '    echo Program exited normally with code:\n'
        printf '%s\n' '    print \$_exitcode'
        printf '%s\n' '  end'
        printf '%s\n' 'else'
        printf '%s\n' '  echo Program terminated by signal:\n'
        printf '%s\n' '  print \$_exitsignal'
        printf '%s\n' '  echo \n=== THREAD BACKTRACE ===\n'
        printf '%s\n' '  thread apply all bt full'
        printf '%s\n' '  echo \n=== REGISTERS ===\n'
        printf '%s\n' '  info registers'
        printf '%s\n' '  echo \n=== SHARED LIBRARIES ===\n'
        printf '%s\n' '  info sharedlibrary'
        printf '%s\n' '  echo \n=== DISASSEMBLY AT PC ===\n'
        printf '%s\n' '  x/16i \$pc'
        printf '%s\n' 'end'
        printf '%s\n' 'quit'
    } > "\$gdb_commands_file"

    server_log "Starting server under gdb at \$started_at"
    set +e
    /usr/bin/gdb -q -batch -x "\$gdb_commands_file" --args "\$DEPLOY_EXECUTABLE" >> "\$SERVER_LOG_FILE" 2>&1
    rc=\$?
    set -e
    rm -f "\$gdb_commands_file"

    if [[ \$rc -eq 0 ]]; then
        server_log "Server process exited normally, gdb log: \$gdb_crash_log"
    else
        server_log "Server crashed or exited with rc=\$rc, detailed gdb log: \$gdb_crash_log"
    fi

    return \$rc
}

trap 'supervisor_log "Stop signal received"; exit 0' TERM INT

supervisor_log "Server launcher started"
set +e
run_server_once
rc=\$?
set -e
supervisor_log "Server run finished with rc=\$rc"
exit "\$rc"
EOF2

    chmod +x "$SERVER_LAUNCHER_SCRIPT"
    log "Server launcher created: $SERVER_LAUNCHER_SCRIPT"
}

create_tools_launcher_script() {
	pkill -9 -f "uvicorn main:app" 2>/dev/null || true
	pkill -9 -f "/root/mmorpg/tools/.venv/bin/python main.py" 2>/dev/null || true
	sleep 1

    print_header "CREATE TOOLS LAUNCHER SCRIPT"
    ensure_dir_exists "$SCRIPT_DIR"

    cat > "$TOOLS_LAUNCHER_SCRIPT" <<EOF2
#!/usr/bin/env bash
set -Eeuo pipefail

cd "$SERVICE_WORKDIR"
umask 027

LOG_DIR="$LOG_DIR"
TOOLS_LOG_FILE="$TOOLS_LOG_FILE"
TOOLS_SUPERVISOR_LOG_FILE="$TOOLS_SUPERVISOR_LOG_FILE"
BRIDGE_STACK_SCRIPT="$BRIDGE_STACK_SCRIPT"

mkdir -p "\$LOG_DIR"
touch "\$TOOLS_LOG_FILE" "\$TOOLS_SUPERVISOR_LOG_FILE"

tools_supervisor_log() {
    echo "[TOOLS-SUPERVISOR] \$(date '+%F %T') \$*" | tee -a "\$TOOLS_SUPERVISOR_LOG_FILE"
}

tools_log() {
    echo "[TOOLS] \$(date '+%F %T') \$*" >> "\$TOOLS_LOG_FILE"
}

run_tools_once() {
    [[ -x "\$BRIDGE_STACK_SCRIPT" ]] || {
        tools_log "Bridge stack script not found or not executable: \$BRIDGE_STACK_SCRIPT"
        return 127
    }

    tools_log "Starting tools stack: \$BRIDGE_STACK_SCRIPT"
    "\$BRIDGE_STACK_SCRIPT" >> "\$TOOLS_LOG_FILE" 2>&1
    local rc=\$?
    tools_log "Tools stack exited with rc=\$rc"
    return \$rc
}

trap 'tools_supervisor_log "Stop signal received"; exit 0' TERM INT

tools_supervisor_log "Tools launcher started"
set +e
run_tools_once
rc=\$?
set -e
tools_supervisor_log "Tools run finished with rc=\$rc"
exit "\$rc"
EOF2

    chmod +x "$TOOLS_LAUNCHER_SCRIPT"
    log "Tools launcher created: $TOOLS_LAUNCHER_SCRIPT"
}

create_relay_launcher_script() {
	pkill -9 -f "$DEPLOY_RELAY_EXECUTABLE" 2>/dev/null || true
	sleep 1

    print_header "CREATE RELAY LAUNCHER SCRIPT"
    ensure_dir_exists "$SCRIPT_DIR"

    cat > "$RELAY_LAUNCHER_SCRIPT" <<EOF2
#!/usr/bin/env bash
set -Eeuo pipefail

cd "$SERVICE_WORKDIR"
umask 027

LOG_DIR="$LOG_DIR"
RELAY_LOG_FILE="$RELAY_LOG_FILE"
RELAY_SUPERVISOR_LOG_FILE="$RELAY_SUPERVISOR_LOG_FILE"
DEPLOY_RELAY_EXECUTABLE="$DEPLOY_RELAY_EXECUTABLE"
RELAY_EXECUTABLE_NAME="$RELAY_EXECUTABLE_NAME"

mkdir -p "\$LOG_DIR"
touch "\$RELAY_LOG_FILE" "\$RELAY_SUPERVISOR_LOG_FILE"

relay_supervisor_log() {
    echo "[RELAY-SUPERVISOR] \$(date '+%F %T') \$*" | tee -a "\$RELAY_SUPERVISOR_LOG_FILE"
}

relay_log() {
    echo "[RELAY] \$(date '+%F %T') \$*" >> "\$RELAY_LOG_FILE"
}

trap 'relay_supervisor_log "Stop signal received"; exit 0' TERM INT

relay_supervisor_log "Relay launcher started"

if [[ ! -x "\$DEPLOY_RELAY_EXECUTABLE" ]]; then
    relay_log "Executable not found or not executable: \$DEPLOY_RELAY_EXECUTABLE"
    exit 127
fi

relay_log "Starting relay at \$(date '+%F %T')"
set +e
"\$DEPLOY_RELAY_EXECUTABLE" >> "\$RELAY_LOG_FILE" 2>&1
rc=\$?
set -e
relay_log "Relay run finished with rc=\$rc"
exit "\$rc"
EOF2

    chmod +x "$RELAY_LAUNCHER_SCRIPT"
    log "Relay launcher created: $RELAY_LAUNCHER_SCRIPT"
}

create_logrotate_config() {
    print_header "CREATE LOGROTATE CONFIG"

    local tmpfile
    tmpfile="$(mktemp)"

    cat > "$tmpfile" <<EOF2
$SERVER_LOG_FILE $SERVER_SUPERVISOR_LOG_FILE $TOOLS_LOG_FILE $TOOLS_SUPERVISOR_LOG_FILE $RELAY_LOG_FILE $RELAY_SUPERVISOR_LOG_FILE $GDB_LOG_DIR/*.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    copytruncate
    create 0640 $SERVICE_USER $SERVICE_GROUP
}
EOF2

    run_sudo cp "$tmpfile" "$LOGROTATE_FILE"
    rm -f "$tmpfile"

    log "Logrotate config installed: $LOGROTATE_FILE"
}

recreate_server_service_unit() {
    print_header "RECREATE SERVER SYSTEMD SERVICE"

    local tmpfile
    tmpfile="$(mktemp)"

    if run_sudo systemctl list-unit-files | grep -Fq "$SERVER_SERVICE_NAME"; then
        run_sudo systemctl stop "$SERVER_SERVICE_NAME" || true
        run_sudo systemctl disable "$SERVER_SERVICE_NAME" || true
        run_sudo systemctl reset-failed "$SERVER_SERVICE_NAME" || true
    fi

    cat > "$tmpfile" <<EOF2
[Unit]
Description=MMO Tee Worlds dedicated server with gdb crash logging
After=network-online.target
Wants=network-online.target $GUARD_SERVICE_NAME

[Service]
Type=simple
User=$SERVICE_USER
Group=$SERVICE_GROUP
WorkingDirectory=$SERVICE_WORKDIR
ExecStartPre=/usr/bin/mkdir -p $LOG_DIR $GDB_LOG_DIR
ExecStartPre=/usr/bin/touch $SERVER_LOG_FILE $SERVER_SUPERVISOR_LOG_FILE
ExecStartPre=/usr/bin/chmod +x $SERVER_LAUNCHER_SCRIPT
ExecStart=$SERVER_LAUNCHER_SCRIPT
Restart=always
RestartSec=5
KillMode=control-group
TimeoutStopSec=45
StandardOutput=append:$SERVER_SUPERVISOR_LOG_FILE
StandardError=append:$SERVER_SUPERVISOR_LOG_FILE

[Install]
WantedBy=multi-user.target
EOF2

    run_sudo cp "$tmpfile" "$SERVER_SERVICE_UNIT_PATH"
    rm -f "$tmpfile"

    run_sudo chmod 0644 "$SERVER_SERVICE_UNIT_PATH"
    log "Server service unit recreated: $SERVER_SERVICE_UNIT_PATH"
}

recreate_tools_service_unit() {
    print_header "RECREATE TOOLS SYSTEMD SERVICE"

    local tmpfile
    tmpfile="$(mktemp)"

    if run_sudo systemctl list-unit-files | grep -Fq "$TOOLS_SERVICE_NAME"; then
        run_sudo systemctl stop "$TOOLS_SERVICE_NAME" || true
        run_sudo systemctl disable "$TOOLS_SERVICE_NAME" || true
        fi

    cat > "$tmpfile" <<EOF2
[Unit]
Description=MMO Tee Worlds tools stack supervisor
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$SERVICE_USER
Group=$SERVICE_GROUP
WorkingDirectory=$SERVICE_WORKDIR
ExecStartPre=/usr/bin/mkdir -p $LOG_DIR
ExecStartPre=/usr/bin/touch $TOOLS_LOG_FILE $TOOLS_SUPERVISOR_LOG_FILE
ExecStartPre=/usr/bin/chmod +x $TOOLS_LAUNCHER_SCRIPT
ExecStart=$TOOLS_LAUNCHER_SCRIPT
Restart=always
RestartSec=5
KillMode=control-group
TimeoutStopSec=45
StandardOutput=append:$TOOLS_SUPERVISOR_LOG_FILE
StandardError=append:$TOOLS_SUPERVISOR_LOG_FILE

[Install]
WantedBy=multi-user.target
EOF2

    run_sudo cp "$tmpfile" "$TOOLS_SERVICE_UNIT_PATH"
    rm -f "$tmpfile"

    run_sudo chmod 0644 "$TOOLS_SERVICE_UNIT_PATH"
    log "Tools service unit recreated: $TOOLS_SERVICE_UNIT_PATH"
}

recreate_relay_service_unit() {
    print_header "RECREATE RELAY SYSTEMD SERVICE"

    local tmpfile
    tmpfile="$(mktemp)"

    if run_sudo systemctl list-unit-files | grep -Fq "$RELAY_SERVICE_NAME"; then
        run_sudo systemctl stop "$RELAY_SERVICE_NAME" || true
        run_sudo systemctl disable "$RELAY_SERVICE_NAME" || true
        run_sudo systemctl reset-failed "$RELAY_SERVICE_NAME" || true
    fi

    cat > "$tmpfile" <<EOF2
[Unit]
Description=MMO Tee Worlds relay server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$SERVICE_USER
Group=$SERVICE_GROUP
WorkingDirectory=$SERVICE_WORKDIR
ExecStartPre=/usr/bin/mkdir -p $LOG_DIR
ExecStartPre=/usr/bin/touch $RELAY_LOG_FILE $RELAY_SUPERVISOR_LOG_FILE
ExecStartPre=/usr/bin/chmod +x $RELAY_LAUNCHER_SCRIPT
ExecStart=$RELAY_LAUNCHER_SCRIPT
Restart=always
RestartSec=5
KillMode=control-group
TimeoutStopSec=45
StandardOutput=append:$RELAY_SUPERVISOR_LOG_FILE
StandardError=append:$RELAY_SUPERVISOR_LOG_FILE

[Install]
WantedBy=multi-user.target
EOF2

    run_sudo cp "$tmpfile" "$RELAY_SERVICE_UNIT_PATH"
    rm -f "$tmpfile"

    run_sudo chmod 0644 "$RELAY_SERVICE_UNIT_PATH"
    log "Relay service unit recreated: $RELAY_SERVICE_UNIT_PATH"
}

bootstrap_runtime() {
    print_header "BOOTSTRAP RUNTIME"

    ensure_dir_exists "$PROJECT_DIR"
    ensure_dir_exists "$BUILD_DIR"
    ensure_dir_exists "$SCRIPT_DIR"
    ensure_dir_exists "$GDB_LOG_DIR"

    run_sudo mkdir -p "$LOG_DIR"
    run_sudo touch "$SERVER_LOG_FILE" "$SERVER_SUPERVISOR_LOG_FILE" "$RELAY_LOG_FILE" "$RELAY_SUPERVISOR_LOG_FILE"
    run_sudo chmod 0640 "$SERVER_LOG_FILE" "$SERVER_SUPERVISOR_LOG_FILE" "$RELAY_LOG_FILE" "$RELAY_SUPERVISOR_LOG_FILE" || true

    create_server_launcher_script
    create_tools_launcher_script
    create_relay_launcher_script
    create_logrotate_config

    log "Bootstrap completed."
}

recreate_replication_guard_unit() {
    [[ -f "$GUARD_SCRIPT" ]] || die "Replication guard not found: $GUARD_SCRIPT"
    local tmpfile
    tmpfile="$(mktemp)"
    cat > "$tmpfile" <<EOF2
[Unit]
Description=MMO replication guard
BindsTo=$SERVER_SERVICE_NAME
After=$SERVER_SERVICE_NAME network-online.target

[Service]
Type=simple
User=root
ExecStart=/bin/sh $GUARD_SCRIPT
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
EOF2
    run_sudo install -m 0644 "$tmpfile" "$GUARD_UNIT_PATH"
    rm -f "$tmpfile"
    # Never overwrite an operator's topology mapping.
    if ! run_sudo test -e /etc/mrpg/replication-guard.conf; then
        run_sudo install -d -m 700 /etc/mrpg
        local primary_ids="" replica_ids="" local_id=""
        case "${GUARD_ROLE:-}" in
            primary|replica)
                local_id="$(run_sudo timeout -k 2 10 mariadb --protocol=socket --connect-timeout=5 -N -e 'SELECT @@global.server_id' 2>/dev/null)" || die "Cannot read local MariaDB server_id for GUARD_ROLE"
                [[ "$local_id" =~ ^[1-9][0-9]*$ ]] || die "Invalid local MariaDB server_id"
                if [[ "$GUARD_ROLE" == primary ]]; then primary_ids="$local_id"; else replica_ids="$local_id"; fi
                ;;
            "") ;;
            *) die "GUARD_ROLE must be primary or replica" ;;
        esac
        run_sudo sh -c 'umask 077; printf "# Root-owned trusted shell configuration.\nPRIMARY_IDS=%s\nREPLICA_IDS=%s\n" "$1" "$2" > /etc/mrpg/replication-guard.conf' sh "$primary_ids" "$replica_ids"
        if [[ -z "$primary_ids$replica_ids" ]]; then
            warn "Configure /etc/mrpg/replication-guard.conf with actual server IDs before starting the server."
        fi
    fi
}

install_or_reinstall_services() {
    print_header "INSTALL OR REINSTALL SERVICES"
    bootstrap_runtime
    recreate_server_service_unit
    recreate_replication_guard_unit
    recreate_tools_service_unit
    recreate_relay_service_unit
    run_sudo systemctl daemon-reload
    run_sudo systemctl enable "$SERVER_SERVICE_NAME"
    run_sudo systemctl enable "$RELAY_SERVICE_NAME"
    run_sudo systemctl reset-failed "$SERVER_SERVICE_NAME" || true
    # CN has no tools unit: reset only a unit that exists.
    systemctl cat "$TOOLS_SERVICE_NAME" >/dev/null 2>&1 && run_sudo systemctl reset-failed "$TOOLS_SERVICE_NAME" || true
    run_sudo systemctl reset-failed "$RELAY_SERVICE_NAME" || true
    log "Services installation completed."
}

# ============================================================
# Relay service lifecycle (ensure on start / restart if exists)
# ============================================================
ensure_relay_service() {
    print_header "ENSURE RELAY SERVICE"

    if [[ ! -f "$RELAY_SERVICE_UNIT_PATH" ]]; then
        log "Relay service unit not found: $RELAY_SERVICE_UNIT_PATH — creating."
        recreate_relay_service_unit
        run_sudo systemctl daemon-reload
        run_sudo systemctl enable "$RELAY_SERVICE_NAME"
        run_sudo systemctl start "$RELAY_SERVICE_NAME"
        log "Relay service installed, enabled for autostart and started."
    else
        log "Relay service unit already exists: $RELAY_SERVICE_UNIT_PATH — restarting."
        run_sudo systemctl enable "$RELAY_SERVICE_NAME" || true
        run_sudo systemctl daemon-reload || true
        if ! run_sudo systemctl restart "$RELAY_SERVICE_NAME"; then
            warn "Relay service restart failed, trying plain start."
            run_sudo systemctl start "$RELAY_SERVICE_NAME" || die "Failed to start relay service: $RELAY_SERVICE_NAME"
        fi
        log "Relay service restarted and enabled for autostart."
    fi
}

# ============================================================
# Build cleanup
# ============================================================
clear_build_dir() {
    print_header "CLEAR BUILD DIRECTORY"

    [[ -d "$PROJECT_DIR" ]] || die "Project directory not found: $PROJECT_DIR"

    if [[ -d "$BUILD_DIR" ]]; then
        find "$BUILD_DIR" -mindepth 1 -delete
        log "Build directory cleared: $BUILD_DIR"
    else
        log "Build directory does not exist, nothing to clear."
    fi
}

validate_boolean_flag() {
    local name="$1"
    local value="$2"

    case "$value" in
        0|1) ;;
        *)
            die "$name must be 0 or 1, got: $value"
            ;;
    esac
}

# ============================================================
# Git
# ============================================================
git_update() {
    print_header "UPDATE GIT REPOSITORY" >&2

    [[ -d "$PROJECT_DIR/.git" ]] || die "Not a git repository: $PROJECT_DIR"

    cd "$PROJECT_DIR"

    local old_head=""
    local new_head=""

    old_head="$(git rev-parse HEAD 2>/dev/null || true)"
    git pull --ff-only >&2
    new_head="$(git rev-parse HEAD)"

    log "Git repository updated." >&2

    if [[ -n "$old_head" && "$old_head" != "$new_head" ]]; then
        printf '%s|%s\n' "$old_head" "$new_head"
    fi
}

# ============================================================
# Build helpers
# ============================================================
get_cmake_build_type() {
    case "$1" in
        debug|asan) echo "Debug" ;;
        release) echo "Release" ;;
        *) return 1 ;;
    esac
}

needs_reconfigure() {
    local build_mode="$1"
    local previous_mode=""

    [[ -f "$LAST_BUILD_MODE_FILE" ]] && previous_mode="$(cat "$LAST_BUILD_MODE_FILE")"

    [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]] && return 0
    [[ "$previous_mode" != "$build_mode" ]] && return 0

    return 1
}

configure_build() {
    local build_mode="$1"
    local build_type
    build_type="$(get_cmake_build_type "$build_mode")"

    ensure_dir_exists "$BUILD_DIR"
    cd "$BUILD_DIR"

    if ! needs_reconfigure "$build_mode"; then
        log "CMake configuration is current. Reconfigure skipped."
        return 0
    fi

    log "Reconfiguring CMake for mode: $build_mode"
    rm -f CMakeCache.txt

    case "$build_mode" in
        debug)
            cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=Debug
            ;;
        asan)
            cmake "$PROJECT_DIR" \
                -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_C_FLAGS="-fsanitize=address -g -fno-omit-frame-pointer" \
                -DCMAKE_CXX_FLAGS="-fsanitize=address -g -fno-omit-frame-pointer" \
                -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
                -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address"
            ;;
        release)
            cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=Release
            ;;
        *)
            die "Unknown build mode: $build_mode"
            ;;
    esac

    echo "$build_mode" > "$LAST_BUILD_MODE_FILE"
    log "CMake configured successfully."
}

build_project() {
    local build_mode="$1"
    local build_type
    build_type="$(get_cmake_build_type "$build_mode")"

    cd "$BUILD_DIR"

    rm -f "$EXECUTABLE_IN_BUILD_DIR" "$TESTRUNNER_IN_BUILD_DIR" || true

    log "Building server..."
    cmake --build . --config "$build_type"
    log "Server build completed successfully."
}

build_relay() {
    local build_mode="$1"
    local build_type
    build_type="$(get_cmake_build_type "$build_mode")"

    cd "$BUILD_DIR"

    rm -f "$RELAY_IN_BUILD_DIR" || true

    log "Building relay ($RELAY_EXECUTABLE_NAME)..."
    cmake --build . --config "$build_type" --target "$RELAY_EXECUTABLE_NAME"
    log "Relay build completed successfully."
}

copy_artifacts() {
    print_header "COPY BUILD ARTIFACTS"

    ensure_dir_exists "$SCRIPT_DIR"

    if [[ -f "$EXECUTABLE_IN_BUILD_DIR" ]]; then
        cp -f "$EXECUTABLE_IN_BUILD_DIR" "$DEPLOY_EXECUTABLE"
        chmod +x "$DEPLOY_EXECUTABLE"
        log "Copied executable: $DEPLOY_EXECUTABLE"
    else
        warn "Executable not found after build: $EXECUTABLE_IN_BUILD_DIR"
    fi

    if [[ -f "$TESTRUNNER_IN_BUILD_DIR" ]]; then
        cp -f "$TESTRUNNER_IN_BUILD_DIR" "$DEPLOY_TESTRUNNER"
        chmod +x "$DEPLOY_TESTRUNNER"
        log "Copied test runner: $DEPLOY_TESTRUNNER"
    else
        warn "Test runner not found after build: $TESTRUNNER_IN_BUILD_DIR"
    fi

    if [[ -f "$RELAY_IN_BUILD_DIR" ]]; then
        cp -f "$RELAY_IN_BUILD_DIR" "$DEPLOY_RELAY_EXECUTABLE"
        chmod +x "$DEPLOY_RELAY_EXECUTABLE"
        log "Copied relay executable: $DEPLOY_RELAY_EXECUTABLE"
    else
        warn "Relay executable not found after build: $RELAY_IN_BUILD_DIR"
    fi
}

update_and_build() {
    local build_mode="$1"

    print_header "UPDATE AND BUILD: $build_mode"

    [[ -d "$PROJECT_DIR" ]] || die "Project directory not found: $PROJECT_DIR"

    # --- Обновление кода из git ---
    git_update >/dev/null

    # --- Сборка ---
    configure_build "$build_mode"
    build_project "$build_mode"
    build_relay "$build_mode"
    copy_artifacts
    bootstrap_runtime

    # --- Сервис релея: создать при отсутствии (с автозапуском), иначе перезапустить ---
    ensure_relay_service

    if [[ "$AUTO_RESTART_SERVICE" == "1" && -f "$SERVER_SERVICE_UNIT_PATH" ]]; then
        print_header "AUTO RESTART SERVER SERVICE"
        run_sudo systemctl daemon-reload || true
        run_sudo systemctl reset-failed "$SERVER_SERVICE_NAME" || true
        if run_sudo systemctl restart "$SERVER_SERVICE_NAME"; then
            log "Server service restarted automatically after build."
        else
            warn "Automatic restart failed for server service."
        fi
    fi

    log "Update and build completed for mode: $build_mode"
}

build_only_no_restart() {
    local build_mode="$1"

    print_header "BUILD ONLY (NO RESTART): $build_mode"

    [[ -d "$PROJECT_DIR" ]] || die "Project directory not found: $PROJECT_DIR"

    # --- Обновление кода из git ---
    git_update >/dev/null

    # --- Сборка ---
    configure_build "$build_mode"
    build_project "$build_mode"
    build_relay "$build_mode"
    copy_artifacts

    log "Build only completed for mode: $build_mode (no service restart, no notifications)"
}

update_relay_only() {
    local build_mode="$1"

    print_header "UPDATE AND BUILD RELAY: $build_mode"

    [[ -d "$PROJECT_DIR" ]] || die "Project directory not found: $PROJECT_DIR"

    # --- Обновление кода из git ---
    git_update >/dev/null

    # --- Сборка релея ---
    configure_build "$build_mode"
    build_relay "$build_mode"

    if [[ -f "$RELAY_IN_BUILD_DIR" ]]; then
        ensure_dir_exists "$SCRIPT_DIR"
        cp -f "$RELAY_IN_BUILD_DIR" "$DEPLOY_RELAY_EXECUTABLE"
        chmod +x "$DEPLOY_RELAY_EXECUTABLE"
        log "Copied relay executable: $DEPLOY_RELAY_EXECUTABLE"
    else
        warn "Relay executable not found after build: $RELAY_IN_BUILD_DIR"
    fi

    bootstrap_runtime

    # --- Сервис релея: создать при отсутствии (с автозапуском), иначе перезапустить ---
    ensure_relay_service

    log "Relay update and build completed for mode: $build_mode"
}

# ============================================================
# Debug / test helpers
# ============================================================
prepare_for_debug_run() {
    local tool_name="$1"
    local target_path="$2"
    local hint="$3"

    if [[ ! -f "$target_path" ]]; then
        echo "[ERROR] Executable not found for $tool_name: $target_path"
        echo "[HINT] Build first: $hint"
        return 1
    fi

    cd "$(dirname "$target_path")" || {
        echo "[ERROR] Failed to change directory to $(dirname "$target_path")"
        return 1
    }
}

run_with_asan_deploy() {
    print_header "RUN WITH ASAN"

    prepare_for_debug_run "ASan" "$DEPLOY_EXECUTABLE" "build mode ASan" || return 1

    local asan_log_file="$ASAN_DEBUG_LOG_FILENAME"
    rm -f "$asan_log_file" || true

    export ASAN_OPTIONS="abort_on_error=0:halt_on_error=0:verbosity=1:print_stats=1:log_path=${asan_log_file}:symbolize=1:external_symbolizer_path=/usr/bin/llvm-symbolizer:alloc_dealloc_mismatch=0"

    "./$EXECUTABLE_NAME"

    if compgen -G "${asan_log_file}*" > /dev/null; then
        log "ASan log found near: $(pwd)/$asan_log_file*"
    else
        warn "ASan log file not found."
    fi
}

run_with_valgrind_deploy() {
    print_header "RUN WITH VALGRIND"

    require_command valgrind
    prepare_for_debug_run "Valgrind" "$DEPLOY_EXECUTABLE" "build mode Debug or ASan" || return 1

    local report
    report="valgrind_report_${EXECUTABLE_NAME}_$(date +%F_%H%M%S).txt"

    valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --log-file="$report" \
        "./$EXECUTABLE_NAME"

    log "Valgrind report saved to: $(pwd)/$report"
}

run_with_gdb_deploy() {
    print_header "RUN WITH GDB"

    require_command gdb
    prepare_for_debug_run "GDB" "$DEPLOY_EXECUTABLE" "build mode Debug or ASan" || return 1

    ensure_dir_exists "$GDB_LOG_DIR"

    local stamp
    local gdb_log_file
    local gdb_commands_file

    stamp="$(date +%F_%H%M%S_%N)"
    gdb_log_file="$GDB_LOG_DIR/gdb_log_${stamp}_${EXECUTABLE_NAME}_deploy.log"
    gdb_commands_file="$(mktemp /tmp/gdb_commands.XXXXXX)"

    cat > "$gdb_commands_file" <<EOF2
set pagination off
set logging overwrite on
set logging file $gdb_log_file
set logging enabled on
run
echo \n=== PROGRAM STATE ===\n
info program
if \$_isvoid(\$_exitsignal)
  if \$_isvoid(\$_exitcode)
    echo Program state is unavailable.\n
  else
    echo Program exited normally with code:\n
    print \$_exitcode
  end
else
  echo Program terminated by signal:\n
  print \$_exitsignal
  echo \n=== THREAD BACKTRACE ===\n
  thread apply all bt full
  echo \n=== REGISTERS ===\n
  info registers
  echo \n=== SHARED LIBRARIES ===\n
  info sharedlibrary
  echo \n=== DISASSEMBLY AT PC ===\n
  x/16i \$pc
end
quit
EOF2

    gdb -q --command="$gdb_commands_file" --args "./$EXECUTABLE_NAME"
    rm -f "$gdb_commands_file"

    log "GDB session ended. Log file: $gdb_log_file"
}

run_tests_from_deploy_dir() {
    print_header "RUN TESTS"

    if [[ ! -f "$DEPLOY_TESTRUNNER" ]]; then
        echo "[ERROR] Test runner not found: $DEPLOY_TESTRUNNER"
        echo "[HINT] Build project first and ensure tests are copied."
        return 1
    fi

    cd "$SCRIPT_DIR" || die "Failed to change directory to $SCRIPT_DIR"

    if "./$TESTRUNNER_NAME"; then
        log "Tests completed successfully."
    else
        warn "Tests failed or were interrupted."
    fi
}

# ============================================================
# Service management
# ============================================================
service_is_active() {
    local service_name="$1"
    run_sudo systemctl is-active --quiet "$service_name"
}

get_service_toggle_action() {
    local service_name="$1"
    if service_is_active "$service_name"; then
        echo "stop"
    else
        echo "start"
    fi
}

show_service_toggle_status() {
    local label="$1"
    local service_name="$2"
    local next_action

    next_action="$(get_service_toggle_action "$service_name")"
    if [[ "$next_action" == "stop" ]]; then
        echo "$label: RUNNING (available action: stop)"
    else
        echo "$label: STOPPED (available action: start)"
    fi
}

show_service_status() {
    local service_name="$1"
    print_header "SERVICE STATUS: $service_name"
    run_sudo systemctl status "$service_name" --no-pager || true
}

control_service() {
    local service_name="$1"
    local action="$2"

    case "$action" in
        start|stop|restart)
            ;;
        *)
            die "Unknown service action: $action"
            ;;
    esac

    print_header "SERVICE ACTION: $service_name $action"
    if run_sudo systemctl "$action" "$service_name"; then
        log "Service action completed: $service_name $action"
    else
        warn "Service action failed: $service_name $action"
    fi

    show_service_status "$service_name"
}

show_service_logs() {
    local service_name="$1"
    print_header "SERVICE LOGS: $service_name"
    run_sudo journalctl -u "$service_name" -n 50 --no-pager || true
}

show_all_logs_summary() {
    print_header "LOG FILES"
    echo "Server supervisor log: $SERVER_SUPERVISOR_LOG_FILE"
    echo "Server runtime log:    $SERVER_LOG_FILE"
    echo "Tools supervisor log:  $TOOLS_SUPERVISOR_LOG_FILE"
    echo "Tools runtime log:     $TOOLS_LOG_FILE"
    echo "Relay supervisor log:  $RELAY_SUPERVISOR_LOG_FILE"
    echo "Relay runtime log:     $RELAY_LOG_FILE"
    echo "GDB crash logs dir:    $GDB_LOG_DIR"
}

# ============================================================
# Debug process management
# ============================================================
show_debug_processes() {
    print_header "DEBUG PROCESSES"

    echo "--- GDB ---"
    pgrep -af "gdb .*$EXECUTABLE_NAME" || echo "No GDB processes found."

    echo "--- Valgrind ---"
    pgrep -af "valgrind.*$EXECUTABLE_NAME" || echo "No Valgrind processes found."

    echo "--- Direct executable runs ---"
    pgrep -af "$EXECUTABLE_NAME" | grep -Ev '(gdb|valgrind|systemctl)' || echo "No direct executable processes found."

    echo "--- Relay ---"
    pgrep -af "$RELAY_EXECUTABLE_NAME" | grep -Ev '(gdb|valgrind|systemctl)' || echo "No relay processes found."

    echo "--- Tools ---"
    pgrep -af "$BRIDGE_STACK_SCRIPT" || echo "No tools processes found."
}

kill_debug_processes() {
    print_header "KILL DEBUG PROCESSES"

    pkill -TERM -f "gdb .*$EXECUTABLE_NAME" 2>/dev/null || true
    pkill -TERM -f "valgrind.*$EXECUTABLE_NAME" 2>/dev/null || true
    pkill -TERM -f "$DEPLOY_EXECUTABLE" 2>/dev/null || true
    pkill -TERM -f "$DEPLOY_RELAY_EXECUTABLE" 2>/dev/null || true
    pkill -TERM -f "$BRIDGE_STACK_SCRIPT" 2>/dev/null || true
    pkill -TERM -f 'uvicorn main:app' 2>/dev/null || true
    pkill -TERM -f '/root/mmorpg/tools/.venv/bin/python main.py' 2>/dev/null || true

    sleep 2

    pkill -KILL -f "gdb .*$EXECUTABLE_NAME" 2>/dev/null || true
    pkill -KILL -f "valgrind.*$EXECUTABLE_NAME" 2>/dev/null || true
    pkill -KILL -f "$DEPLOY_EXECUTABLE" 2>/dev/null || true
    pkill -KILL -f "$DEPLOY_RELAY_EXECUTABLE" 2>/dev/null || true
    pkill -KILL -f "$BRIDGE_STACK_SCRIPT" 2>/dev/null || true
    pkill -KILL -f 'uvicorn main:app' 2>/dev/null || true
    pkill -KILL -f '/root/mmorpg/tools/.venv/bin/python main.py' 2>/dev/null || true

    log "Kill requests sent for debug processes."
}

# ============================================================
# CLI
# ============================================================
print_usage() {
    cat <<EOF2
Usage:
  $0 menu
  $0 config
  $0 build <debug|asan|release>
  $0 build-only <debug|asan|release>  # сборка без перезапуска сервисов и уведомлений
  $0 build-relay <debug|asan|release> # обновление и сборка только релея с запуском через сервис
  $0 service <install|reinstall|server|tools|relay|all>
  $0 run <gdb|valgrind|asan>
  $0 test
  $0 clean
  $0 bootstrap
  $0 debug <ps|kill>
EOF2
}

handle_cli() {
    local command="${1:-menu}"

    case "$command" in
        menu)
            main_menu
            ;;
        config)
            show_config_summary
            ;;
        build)
            local mode="${2:-}"
            [[ -n "$mode" ]] || die "Build mode is required: debug | asan | release"
            update_and_build "$mode"
            ;;
        build-only)
            local mode="${2:-}"
            [[ -n "$mode" ]] || die "Build mode is required: debug | asan | release"
            build_only_no_restart "$mode"
            ;;
        build-relay)
            local mode="${2:-}"
            [[ -n "$mode" ]] || die "Build mode is required: debug | asan | release"
            update_relay_only "$mode"
            ;;
        service)
            local target="${2:-all}"
            local action="${3:-}"
            case "$target" in
                install|reinstall|recreate)
                    install_or_reinstall_services
                    ;;
                server)
                    case "$action" in
                        toggle|"") control_service "$SERVER_SERVICE_NAME" "$(get_service_toggle_action "$SERVER_SERVICE_NAME")" ;;
                        start|stop|restart) control_service "$SERVER_SERVICE_NAME" "$action" ;;
                        status) show_service_status "$SERVER_SERVICE_NAME" ;;
                        logs) show_service_logs "$SERVER_SERVICE_NAME"; show_all_logs_summary ;;
                        *) die "Unknown server service action: $action" ;;
                    esac
                    ;;
                tools)
                    case "$action" in
                        toggle|"") control_service "$TOOLS_SERVICE_NAME" "$(get_service_toggle_action "$TOOLS_SERVICE_NAME")" ;;
                        start|stop|restart) control_service "$TOOLS_SERVICE_NAME" "$action" ;;
                        status) show_service_status "$TOOLS_SERVICE_NAME" ;;
                        logs) show_service_logs "$TOOLS_SERVICE_NAME"; show_all_logs_summary ;;
                        *) die "Unknown tools service action: $action" ;;
                    esac
                    ;;
                relay)
                    case "$action" in
                        toggle|"") control_service "$RELAY_SERVICE_NAME" "$(get_service_toggle_action "$RELAY_SERVICE_NAME")" ;;
                        start|stop|restart) control_service "$RELAY_SERVICE_NAME" "$action" ;;
                        status) show_service_status "$RELAY_SERVICE_NAME" ;;
                        logs) show_service_logs "$RELAY_SERVICE_NAME"; show_all_logs_summary ;;
                        *) die "Unknown relay service action: $action" ;;
                    esac
                    ;;
                all)
                    case "$action" in
                        start)
                            control_service "$SERVER_SERVICE_NAME" start
                            control_service "$TOOLS_SERVICE_NAME" start
                            [[ -f "$RELAY_SERVICE_UNIT_PATH" ]] && control_service "$RELAY_SERVICE_NAME" start
                            ;;
                        stop)
                            [[ -f "$RELAY_SERVICE_UNIT_PATH" ]] && control_service "$RELAY_SERVICE_NAME" stop
                            control_service "$TOOLS_SERVICE_NAME" stop
                            control_service "$SERVER_SERVICE_NAME" stop
                            ;;
                        restart)
                            control_service "$SERVER_SERVICE_NAME" restart
                            [[ -f "$RELAY_SERVICE_UNIT_PATH" ]] && control_service "$RELAY_SERVICE_NAME" restart
                            control_service "$TOOLS_SERVICE_NAME" restart
                            ;;
                        status)
                            show_service_toggle_status "Server" "$SERVER_SERVICE_NAME"
                            show_service_status "$SERVER_SERVICE_NAME"
                            echo
                            show_service_toggle_status "Tools" "$TOOLS_SERVICE_NAME"
                            show_service_status "$TOOLS_SERVICE_NAME"
                            echo
                            show_service_toggle_status "Relay" "$RELAY_SERVICE_NAME"
                            show_service_status "$RELAY_SERVICE_NAME"
                            show_all_logs_summary
                            ;;
                        logs)
                            show_service_logs "$SERVER_SERVICE_NAME"
                            show_service_logs "$TOOLS_SERVICE_NAME"
                            [[ -f "$RELAY_SERVICE_UNIT_PATH" ]] && show_service_logs "$RELAY_SERVICE_NAME"
                            show_all_logs_summary
                            ;;
                        *) die "Unknown all-services action: $action" ;;
                    esac
                    ;;
                *)
                    die "Unknown service target: $target"
                    ;;
            esac
            ;;
        run)
            local tool="${2:-}"
            case "$tool" in
                gdb) run_with_gdb_deploy ;;
                valgrind) run_with_valgrind_deploy ;;
                asan) run_with_asan_deploy ;;
                *) die "Unknown run target: $tool" ;;
            esac
            ;;
        test)
            run_tests_from_deploy_dir
            ;;
        clean)
            clear_build_dir
            ;;
        bootstrap)
            bootstrap_runtime
            ;;
        debug)
            local action="${2:-ps}"
            case "$action" in
                ps) show_debug_processes ;;
                kill) kill_debug_processes ;;
                *) die "Unknown debug action: $action" ;;
            esac
            ;;
        help|-h|--help)
            print_usage
            ;;
        *)
            die "Unknown command: $command"
            ;;
    esac
}

# ============================================================
# Menu
# ============================================================
main_menu() {
    while true; do
        print_header "MAIN MENU"

        echo "--- Build ---"
        echo "1.  Update and build (Debug)"
        echo "2.  Update and build (ASan)"
        echo "3.  Update and build (Release)"
        echo "4.  Build only (no restart)"
        echo "5.  Update and build relay (Debug)"
        echo "6.  Update and build relay (ASan)"
        echo "7.  Update and build relay (Release)"

        echo "--- Run / Debug ---"
        echo "8.  Run with GDB"
        echo "9.  Run with Valgrind"
        echo "10. Run with ASan"

        echo "--- Tests ---"
        echo "11. Run tests"

        echo "--- Services ---"
        echo "12. Show all service status"
        echo "13. Toggle server service ($(get_service_toggle_action "$SERVER_SERVICE_NAME"))"
        echo "14. Restart server service"
        echo "15. Toggle tools service ($(get_service_toggle_action "$TOOLS_SERVICE_NAME"))"
        echo "16. Restart tools service"
        echo "17. Toggle relay service ($(get_service_toggle_action "$RELAY_SERVICE_NAME"))"
        echo "18. Restart relay service"
        echo "19. Show all service logs"

        echo "--- Debug processes ---"
        echo "20. Show debug processes"
        echo "21. Kill debug processes"

        echo "--- Other ---"
        echo "22. Clear build directory"
        echo "23. Reinstall services and logger"
        echo "24. Show config summary"
        echo "0.  Exit"

        read -r -p "Enter your choice: " choice

        case "$choice" in
            1) update_and_build "debug" ;;
            2) update_and_build "asan" ;;
            3) update_and_build "release" ;;
            4) prompt_build_only_mode ;;
            5) update_relay_only "debug" ;;
            6) update_relay_only "asan" ;;
            7) update_relay_only "release" ;;
            8) run_with_gdb_deploy ;;
            9) run_with_valgrind_deploy ;;
            10) run_with_asan_deploy ;;
            11) run_tests_from_deploy_dir ;;
            12) show_service_toggle_status "Server" "$SERVER_SERVICE_NAME"; show_service_status "$SERVER_SERVICE_NAME"; echo; show_service_toggle_status "Tools" "$TOOLS_SERVICE_NAME"; show_service_status "$TOOLS_SERVICE_NAME"; echo; show_service_toggle_status "Relay" "$RELAY_SERVICE_NAME"; show_service_status "$RELAY_SERVICE_NAME"; show_all_logs_summary ;;
            13) control_service "$SERVER_SERVICE_NAME" "$(get_service_toggle_action "$SERVER_SERVICE_NAME")" ;;
            14) control_service "$SERVER_SERVICE_NAME" "restart" ;;
            15) control_service "$TOOLS_SERVICE_NAME" "$(get_service_toggle_action "$TOOLS_SERVICE_NAME")" ;;
            16) control_service "$TOOLS_SERVICE_NAME" "restart" ;;
            17) control_service "$RELAY_SERVICE_NAME" "$(get_service_toggle_action "$RELAY_SERVICE_NAME")" ;;
            18) control_service "$RELAY_SERVICE_NAME" "restart" ;;
            19) show_service_logs "$SERVER_SERVICE_NAME"; show_service_logs "$TOOLS_SERVICE_NAME"; show_service_logs "$RELAY_SERVICE_NAME"; show_all_logs_summary ;;
            20) show_debug_processes ;;
            21) kill_debug_processes ;;
            22) clear_build_dir ;;
            23) install_or_reinstall_services ;;
            24) show_config_summary ;;
            0)
                echo "Goodbye."
                break
                ;;
            *)
                echo "[ERROR] Invalid choice: $choice"
                ;;
        esac
    done
}

prompt_build_only_mode() {
    local build_mode=""
    read -r -p "Enter build mode (debug/asan/release): " build_mode

    case "$build_mode" in
        debug|asan|release)
            build_only_no_restart "$build_mode"
            ;;
        *)
            echo "[ERROR] Invalid build mode: $build_mode"
            ;;
    esac
}

# ============================================================
# Main entry point
# ============================================================
main() {
    check_dependencies
    check_optional_dependencies
    validate_config

    validate_boolean_flag "AUTO_RESTART_SERVICE" "$AUTO_RESTART_SERVICE"

    local command="${1:-menu}"
    case "$command" in
        build|build-relay|bootstrap)
            bootstrap_runtime
            ;;
    esac

    handle_cli "$@"
}

main "$@"