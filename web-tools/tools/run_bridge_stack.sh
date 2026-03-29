#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SKINS_DIR="$ROOT_DIR/skins_api"
DISCORD_DIR="$ROOT_DIR/discord"
VENV_DIR="$ROOT_DIR/.venv"
HOST="${BRIDGE_HOST:-127.0.0.1}"
PORT="${BRIDGE_PORT:-8000}"
SKINS_LOG_FILE="${SKINS_LOG_FILE:-$ROOT_DIR/skins_api.log}"
DISCORD_LOG_FILE="${DISCORD_LOG_FILE:-$ROOT_DIR/discord_bridge.log}"
SKIP_INSTALL="${SKIP_INSTALL:-0}"

SKINS_PID=""
DISCORD_PID=""

cleanup() {
  if [ -n "${DISCORD_PID:-}" ] && kill -0 "$DISCORD_PID" 2>/dev/null; then
    echo "[bridge] stopping discord bridge..."
    kill "$DISCORD_PID" 2>/dev/null || true
    wait "$DISCORD_PID" 2>/dev/null || true
  fi
  if [ -n "${SKINS_PID:-}" ] && kill -0 "$SKINS_PID" 2>/dev/null; then
    echo "[bridge] stopping skins_api..."
    kill "$SKINS_PID" 2>/dev/null || true
    wait "$SKINS_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

echo "[bridge] checking directories..."
[ -d "$SKINS_DIR" ] || { echo "Missing directory: $SKINS_DIR"; exit 1; }
[ -d "$DISCORD_DIR" ] || { echo "Missing directory: $DISCORD_DIR"; exit 1; }

command -v python3 >/dev/null 2>&1 || { echo "python3 not found"; exit 1; }
command -v curl >/dev/null 2>&1 || { echo "curl not found"; exit 1; }

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

sleep 1
kill -0 "$SKINS_PID" 2>/dev/null || {
  echo "[bridge] skins_api failed to start (process exited)."
  [ -f "$SKINS_LOG_FILE" ] && tail -n 40 "$SKINS_LOG_FILE"
  exit 1
}

READY=0
for _ in 1 2 3 4 5 6 7 8 9 10; do
  if curl --silent --max-time 2 "http://$HOST:$PORT/docs" >/dev/null 2>&1; then
    READY=1
    break
  fi
  sleep 1
done

if [ "$READY" -ne 1 ]; then
  echo "[bridge] skins_api did not become ready in time."
  [ -f "$SKINS_LOG_FILE" ] && tail -n 40 "$SKINS_LOG_FILE"
  exit 1
fi

echo "[bridge] starting discord..."
(
  cd "$DISCORD_DIR"
  exec "$PYTHON" main.py
) >"$DISCORD_LOG_FILE" 2>&1 &
DISCORD_PID=$!

sleep 1
kill -0 "$DISCORD_PID" 2>/dev/null || {
  echo "[bridge] discord bridge failed to start (process exited)."
  [ -f "$DISCORD_LOG_FILE" ] && tail -n 40 "$DISCORD_LOG_FILE"
  exit 1
}

echo "[bridge] all services started."
echo "[bridge] skins_api pid=$SKINS_PID log=$SKINS_LOG_FILE"
echo "[bridge] discord   pid=$DISCORD_PID log=$DISCORD_LOG_FILE"
echo "[bridge] press Ctrl+C to stop."

wait "$DISCORD_PID"
