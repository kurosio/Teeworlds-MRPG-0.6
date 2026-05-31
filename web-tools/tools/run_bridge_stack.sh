#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SKINS_DIR="$ROOT_DIR/skins_api"
DISCORD_DIR="$ROOT_DIR/discord"
WEB_EDITOR_TRANSLATE_DIR="$ROOT_DIR/web-translate-editor"
WEB_EDITOR_TRANSLATE_SCRIPT="$WEB_EDITOR_TRANSLATE_DIR/start-web-editor.sh"
VENV_DIR="$ROOT_DIR/.venv"
RUN_DIR="${RUN_DIR:-$ROOT_DIR/.run}"

HOST="${BRIDGE_HOST:-192.124.189.87}"
PORT="${BRIDGE_PORT:-8000}"
HEALTH_HOST="${BRIDGE_HEALTH_HOST:-$HOST}"
[ "$HEALTH_HOST" = "0.0.0.0" ] && HEALTH_HOST="127.0.0.1"

WEB_EDITOR_TRANSLATE_HOST="${WEB_EDITOR_TRANSLATE_HOST:-0.0.0.0}"
WEB_EDITOR_TRANSLATE_PORT="${WEB_EDITOR_TRANSLATE_PORT:-3001}"
TRANSLATION_ROOT="${TRANSLATION_ROOT:-/root/mmorpg/server_lang}"
WEB_EDITOR_TRANSLATE_HEALTH_HOST="${WEB_EDITOR_TRANSLATE_HEALTH_HOST:-127.0.0.1}"
PUBLIC_HOST="${BRIDGE_PUBLIC_HOST:-}"
if [ -z "$PUBLIC_HOST" ]; then
  if [ "$HOST" != "0.0.0.0" ] && [ "$HOST" != "127.0.0.1" ]; then
    PUBLIC_HOST="$HOST"
  else
    PUBLIC_HOST="$(hostname -I 2>/dev/null | awk '{print $1}')"
    PUBLIC_HOST="${PUBLIC_HOST:-127.0.0.1}"
  fi
fi

SKINS_LOG_FILE="${SKINS_LOG_FILE:-$ROOT_DIR/skins_api.log}"
DISCORD_LOG_FILE="${DISCORD_LOG_FILE:-$ROOT_DIR/discord_bridge.log}"
WEB_EDITOR_TRANSLATE_LOG_FILE="${WEB_EDITOR_TRANSLATE_LOG_FILE:-$ROOT_DIR/web_translate_editor.log}"

SKIP_INSTALL="${SKIP_INSTALL:-0}"

SKINS_PID=""
DISCORD_PID=""
WEB_EDITOR_TRANSLATE_PID=""

SKINS_PID_FILE="$RUN_DIR/skins_api.pid"
DISCORD_PID_FILE="$RUN_DIR/discord_bridge.pid"
WEB_EDITOR_TRANSLATE_PID_FILE="$RUN_DIR/web_translate_editor.pid"

ensure_run_dir() {
  mkdir -p "$RUN_DIR"
}

is_running() {
  _pid="$1"
  [ -n "$_pid" ] && kill -0 "$_pid" 2>/dev/null
}

stop_pid() {
  _name="$1"
  _pid="$2"

  if is_running "$_pid"; then
    echo "[bridge] stopping $_name pid=$_pid ..."
    kill "$_pid" 2>/dev/null || true

    _i=0
    while is_running "$_pid" && [ "$_i" -lt 20 ]; do
      sleep 1
      _i=$(( _i + 1 ))
    done

    if is_running "$_pid"; then
      echo "[bridge] $_name did not stop gracefully; killing pid=$_pid ..."
      kill -9 "$_pid" 2>/dev/null || true
    fi
  fi
}

stop_pid_file() {
  _name="$1"
  _pid_file="$2"

  if [ -f "$_pid_file" ]; then
    _pid="$(cat "$_pid_file" 2>/dev/null || true)"
    stop_pid "$_name" "$_pid"
    rm -f "$_pid_file"
  fi
}

stop_by_pattern() {
  _name="$1"
  _pattern="$2"

  command -v pgrep >/dev/null 2>&1 || return 0

  _pids="$(pgrep -f "$_pattern" 2>/dev/null || true)"
  [ -n "$_pids" ] || return 0

  echo "[bridge] stopping already running $_name process(es): $_pids"
  for _pid in $_pids; do
    [ "$_pid" = "$$" ] && continue
    stop_pid "$_name" "$_pid"
  done
}

restart_previous_components() {
  echo "[bridge] restarting components if they are already running..."

  stop_pid_file "discord bridge" "$DISCORD_PID_FILE"
  stop_pid_file "web translate editor" "$WEB_EDITOR_TRANSLATE_PID_FILE"
  stop_pid_file "skins_api" "$SKINS_PID_FILE"

  if [ -x "$WEB_EDITOR_TRANSLATE_SCRIPT" ]; then
    (
      cd "$WEB_EDITOR_TRANSLATE_DIR"
      ./start-web-editor.sh stop >/dev/null 2>&1 || true
    )
  fi

  stop_by_pattern "discord bridge" "$DISCORD_DIR/main.py"
  stop_by_pattern "skins_api" "uvicorn main:app --host $HOST --port $PORT"
}

cleanup() {
  stop_pid "discord bridge" "${DISCORD_PID:-}"
  stop_pid "web translate editor launcher" "${WEB_EDITOR_TRANSLATE_PID:-}"
  stop_pid "skins_api" "${SKINS_PID:-}"

  rm -f "$DISCORD_PID_FILE" "$WEB_EDITOR_TRANSLATE_PID_FILE" "$SKINS_PID_FILE"

  if [ -x "$WEB_EDITOR_TRANSLATE_SCRIPT" ]; then
    (
      cd "$WEB_EDITOR_TRANSLATE_DIR"
      ./start-web-editor.sh stop >/dev/null 2>&1 || true
    )
  fi
}
trap cleanup EXIT INT TERM

wait_for_http() {
  _name="$1"
  _url="$2"
  _log_file="$3"
  _tries="${4:-20}"

  _i=1
  while [ "$_i" -le "$_tries" ]; do
    if curl --silent --fail --max-time 2 "$_url" >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
    _i=$(( _i + 1 ))
  done

  echo "[bridge] $_name did not become ready in time: $_url"
  [ -f "$_log_file" ] && tail -n 80 "$_log_file"
  return 1
}


show_listeners() {
  _port="$1"

  if command -v ss >/dev/null 2>&1; then
    ss -ltnp 2>/dev/null | grep -E "(:|\\*)$_port[[:space:]]" || true
    return 0
  fi

  if command -v netstat >/dev/null 2>&1; then
    netstat -ltnp 2>/dev/null | grep -E "(:|\\*)$_port[[:space:]]" || true
    return 0
  fi
}

check_network_bind() {
  _name="$1"
  _port="$2"
  _required_host="$3"
  _log_file="$4"

  _listeners="$(show_listeners "$_port")"

  if [ -z "$_listeners" ]; then
    echo "[bridge] $_name is not listening on port $_port."
    [ -f "$_log_file" ] && tail -n 80 "$_log_file"
    return 1
  fi

  echo "[bridge] $_name listeners on port $_port:"
  echo "$_listeners"

  if [ "$_required_host" = "0.0.0.0" ]; then
    if echo "$_listeners" | grep -Eq "(0\.0\.0\.0|\*)[:.]$_port|\[::\]:$_port|:::$_port"; then
      return 0
    fi

    echo "[bridge] ERROR: $_name is reachable locally, but it is not bound to 0.0.0.0:$_port."
    echo "[bridge] It is probably listening only on 127.0.0.1, so it will not open from the network."
    echo "[bridge] Check $WEB_EDITOR_TRANSLATE_SCRIPT and make sure the dev/server command uses --host 0.0.0.0 or HOST=0.0.0.0."
    [ -f "$_log_file" ] && tail -n 80 "$_log_file"
    return 1
  fi

  return 0
}

echo "[bridge] checking directories..."
[ -d "$SKINS_DIR" ] || { echo "Missing directory: $SKINS_DIR"; exit 1; }
[ -d "$DISCORD_DIR" ] || { echo "Missing directory: $DISCORD_DIR"; exit 1; }
[ -d "$WEB_EDITOR_TRANSLATE_DIR" ] || { echo "Missing directory: $WEB_EDITOR_TRANSLATE_DIR"; exit 1; }
[ -f "$WEB_EDITOR_TRANSLATE_SCRIPT" ] || { echo "Missing script: $WEB_EDITOR_TRANSLATE_SCRIPT"; exit 1; }

command -v python3 >/dev/null 2>&1 || { echo "python3 not found"; exit 1; }
command -v curl >/dev/null 2>&1 || { echo "curl not found"; exit 1; }
command -v npm >/dev/null 2>&1 || { echo "npm not found"; exit 1; }

ensure_run_dir
restart_previous_components

if [ ! -d "$VENV_DIR" ]; then
  echo "[bridge] creating virtualenv..."
  python3 -m venv "$VENV_DIR"
fi

PYTHON="$VENV_DIR/bin/python"

if [ "$SKIP_INSTALL" != "1" ]; then
  echo "[bridge] installing dependencies..."
  [ -f "$SKINS_DIR/requires.txt" ] && "$PYTHON" -m pip install -r "$SKINS_DIR/requires.txt"
  [ -f "$DISCORD_DIR/requires.txt" ] && "$PYTHON" -m pip install -r "$DISCORD_DIR/requires.txt"
else
  echo "[bridge] SKIP_INSTALL=1 -> skipping dependency installation."
fi

echo "[bridge] starting skins_api on http://$HOST:$PORT ..."
(
  cd "$SKINS_DIR"
  exec "$PYTHON" -m uvicorn main:app --host "$HOST" --port "$PORT"
) >"$SKINS_LOG_FILE" 2>&1 &
SKINS_PID=$!
echo "$SKINS_PID" > "$SKINS_PID_FILE"

sleep 1
if ! is_running "$SKINS_PID"; then
  echo "[bridge] skins_api failed to start (process exited)."
  [ -f "$SKINS_LOG_FILE" ] && tail -n 80 "$SKINS_LOG_FILE"
  exit 1
fi

wait_for_http "skins_api" "http://$HEALTH_HOST:$PORT/docs" "$SKINS_LOG_FILE" 20

echo "[bridge] starting web translate editor on http://$WEB_EDITOR_TRANSLATE_HOST:$WEB_EDITOR_TRANSLATE_PORT ..."
chmod +x "$WEB_EDITOR_TRANSLATE_SCRIPT" 2>/dev/null || true

(
  cd "$WEB_EDITOR_TRANSLATE_DIR"
  HOST="$WEB_EDITOR_TRANSLATE_HOST" \
  HOSTNAME="$WEB_EDITOR_TRANSLATE_HOST" \
  SERVER_HOST="$WEB_EDITOR_TRANSLATE_HOST" \
  VITE_HOST="$WEB_EDITOR_TRANSLATE_HOST" \
  PORT="$WEB_EDITOR_TRANSLATE_PORT" \
  TRANSLATION_ROOT="$TRANSLATION_ROOT" \
  ./start-web-editor.sh start
) >"$WEB_EDITOR_TRANSLATE_LOG_FILE" 2>&1 &
WEB_EDITOR_TRANSLATE_PID=$!
echo "$WEB_EDITOR_TRANSLATE_PID" > "$WEB_EDITOR_TRANSLATE_PID_FILE"

WEB_TRANSLATE_READY=0
for _url in \
  "http://$WEB_EDITOR_TRANSLATE_HEALTH_HOST:$WEB_EDITOR_TRANSLATE_PORT/api/health" \
  "http://$WEB_EDITOR_TRANSLATE_HEALTH_HOST:$WEB_EDITOR_TRANSLATE_PORT/"
do
  if wait_for_http "web translate editor" "$_url" "$WEB_EDITOR_TRANSLATE_LOG_FILE" 10; then
    WEB_TRANSLATE_READY=1
    break
  fi

  if ! is_running "$WEB_EDITOR_TRANSLATE_PID"; then
    # start-web-editor.sh may daemonize and exit normally; readiness is checked by HTTP.
    WEB_EDITOR_TRANSLATE_PID=""
    rm -f "$WEB_EDITOR_TRANSLATE_PID_FILE"
  fi
done

if [ "$WEB_TRANSLATE_READY" -ne 1 ]; then
  echo "[bridge] web translate editor failed to start."
  [ -f "$WEB_EDITOR_TRANSLATE_LOG_FILE" ] && tail -n 80 "$WEB_EDITOR_TRANSLATE_LOG_FILE"
  exit 1
fi

check_network_bind "web translate editor" "$WEB_EDITOR_TRANSLATE_PORT" "$WEB_EDITOR_TRANSLATE_HOST" "$WEB_EDITOR_TRANSLATE_LOG_FILE"

echo "[bridge] starting discord..."
(
  cd "$DISCORD_DIR"
  exec "$PYTHON" main.py
) >"$DISCORD_LOG_FILE" 2>&1 &
DISCORD_PID=$!
echo "$DISCORD_PID" > "$DISCORD_PID_FILE"

sleep 1
if ! is_running "$DISCORD_PID"; then
  echo "[bridge] discord bridge failed to start (process exited)."
  [ -f "$DISCORD_LOG_FILE" ] && tail -n 80 "$DISCORD_LOG_FILE"
  exit 1
fi

echo "[bridge] all services started."
echo "[bridge] skins_api          pid=$SKINS_PID log=$SKINS_LOG_FILE"
echo "[bridge] web translate edit pid=${WEB_EDITOR_TRANSLATE_PID:-managed-by-start-script} log=$WEB_EDITOR_TRANSLATE_LOG_FILE"
echo "[bridge] discord            pid=$DISCORD_PID log=$DISCORD_LOG_FILE"
echo "[bridge] skins_api URL:      http://$HOST:$PORT"
echo "[bridge] web editor local:   http://127.0.0.1:$WEB_EDITOR_TRANSLATE_PORT"
echo "[bridge] web editor network: http://$PUBLIC_HOST:$WEB_EDITOR_TRANSLATE_PORT"
echo "[bridge] translation root:   $TRANSLATION_ROOT"
echo "[bridge] press Ctrl+C to stop."

wait "$DISCORD_PID"
