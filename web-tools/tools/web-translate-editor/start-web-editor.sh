#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$APP_DIR"

TRANSLATION_ROOT="${TRANSLATION_ROOT:-/root/mmorpg/server_lang}"
HOST="${HOST:-0.0.0.0}"
PORT="${PORT:-${API_PORT:-3001}}"
PID_FILE="${PID_FILE:-$APP_DIR/.web-editor.pid}"
LOG_FILE="${LOG_FILE:-$APP_DIR/web-editor.log}"

is_port_busy() {
  local port="$1"
  if command -v ss >/dev/null 2>&1; then
    ss -ltn | awk '{print $4}' | grep -Eq "(^|:)${port}$"
  elif command -v lsof >/dev/null 2>&1; then
    lsof -iTCP:"$port" -sTCP:LISTEN >/dev/null 2>&1
  else
    node -e "const net=require('net'); const s=net.createServer(); s.once('error',()=>process.exit(0)); s.once('listening',()=>s.close(()=>process.exit(1))); s.listen(Number(process.argv[1]), '0.0.0.0');" "$port"
  fi
}

find_free_port() {
  local port="$1"
  while is_port_busy "$port"; do
    port=$((port + 1))
  done
  echo "$port"
}

server_ip() {
  if command -v hostname >/dev/null 2>&1; then
    hostname -I 2>/dev/null | awk '{print $1}'
  fi
}

install_deps() {
  if [ ! -d node_modules ]; then
    echo "Installing npm dependencies..."
    npm install
  fi
}

build_app() {
  if [ ! -d dist ]; then
    echo "Building frontend..."
    npm run build
  fi
}

start_app() {
  install_deps
  build_app

  if [ -f "$PID_FILE" ]; then
    local old_pid
    old_pid="$(cat "$PID_FILE" || true)"
    if [ -n "$old_pid" ] && kill -0 "$old_pid" >/dev/null 2>&1; then
      echo "Web editor is already running. PID: $old_pid"
      echo "Stop it first: $0 stop"
      exit 0
    fi
    rm -f "$PID_FILE"
  fi

  local selected_port
  selected_port="$(find_free_port "$PORT")"
  if [ "$selected_port" != "$PORT" ]; then
    echo "Port $PORT is busy. Using free port $selected_port instead."
  fi

  echo "Starting web editor..."
  echo "TRANSLATION_ROOT=$TRANSLATION_ROOT"
  echo "HOST=$HOST"
  echo "PORT=$selected_port"

  HOST="$HOST" PORT="$selected_port" TRANSLATION_ROOT="$TRANSLATION_ROOT" \
    nohup node server/index.mjs > "$LOG_FILE" 2>&1 &

  local pid=$!
  echo "$pid" > "$PID_FILE"
  sleep 1

  if ! kill -0 "$pid" >/dev/null 2>&1; then
    echo "Failed to start. Log: $LOG_FILE"
    tail -n 80 "$LOG_FILE" || true
    rm -f "$PID_FILE"
    exit 1
  fi

  local ip
  ip="$(server_ip || true)"
  echo "Started. PID: $pid"
  echo "Local URL:  http://127.0.0.1:$selected_port"
  if [ -n "${ip:-}" ]; then
    echo "Remote URL: http://$ip:$selected_port"
  else
    echo "Remote URL: http://YOUR_SERVER_IP:$selected_port"
  fi
  echo "Log file: $LOG_FILE"
}

stop_app() {
  if [ ! -f "$PID_FILE" ]; then
    echo "No PID file found. Nothing to stop."
    exit 0
  fi

  local pid
  pid="$(cat "$PID_FILE" || true)"
  if [ -n "$pid" ] && kill -0 "$pid" >/dev/null 2>&1; then
    echo "Stopping PID $pid..."
    kill "$pid"
    sleep 1
    if kill -0 "$pid" >/dev/null 2>&1; then
      echo "Process did not stop gracefully. Killing..."
      kill -9 "$pid" || true
    fi
  else
    echo "Process is not running."
  fi

  rm -f "$PID_FILE"
}

status_app() {
  if [ -f "$PID_FILE" ]; then
    local pid
    pid="$(cat "$PID_FILE" || true)"
    if [ -n "$pid" ] && kill -0 "$pid" >/dev/null 2>&1; then
      echo "Running. PID: $pid"
      echo "Log file: $LOG_FILE"
      exit 0
    fi
  fi
  echo "Not running."
}

logs_app() {
  touch "$LOG_FILE"
  tail -f "$LOG_FILE"
}

case "${1:-start}" in
  start)
    start_app
    ;;
  stop)
    stop_app
    ;;
  restart)
    stop_app || true
    start_app
    ;;
  status)
    status_app
    ;;
  logs)
    logs_app
    ;;
  rebuild)
    install_deps
    npm run build
    ;;
  *)
    echo "Usage: $0 {start|stop|restart|status|logs|rebuild}"
    echo "Env: TRANSLATION_ROOT=/root/mmorpg/server_lang HOST=0.0.0.0 PORT=3001"
    exit 1
    ;;
esac
