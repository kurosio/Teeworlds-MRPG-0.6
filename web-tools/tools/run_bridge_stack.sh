#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SKINS_DIR="$ROOT_DIR/skins_api"
DISCORD_DIR="$ROOT_DIR/discord"
VENV_DIR="$ROOT_DIR/.venv"

SKINS_PID=""

cleanup() {
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

if [ ! -d "$VENV_DIR" ]; then
  echo "[bridge] creating virtualenv..."
  python3 -m venv "$VENV_DIR"
fi

PYTHON="$VENV_DIR/bin/python"
PIP="$VENV_DIR/bin/pip"

echo "[bridge] installing dependencies..."
[ -f "$SKINS_DIR/requires.txt" ] && "$PIP" install -r "$SKINS_DIR/requires.txt"
[ -f "$DISCORD_DIR/requires.txt" ] && "$PIP" install -r "$DISCORD_DIR/requires.txt"

echo "[bridge] starting skins_api..."
(
  cd "$SKINS_DIR"
  exec "$PYTHON" -m uvicorn main:app --host 127.0.0.1 --port 8000
) &
SKINS_PID=$!

sleep 2
kill -0 "$SKINS_PID" 2>/dev/null || { echo "skins_api failed to start"; exit 1; }

echo "[bridge] starting discord..."
cd "$DISCORD_DIR"
exec "$PYTHON" main.py