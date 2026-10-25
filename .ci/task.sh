#!/usr/bin/env bash
# Temporary remote driver script (runs on a GitHub runner, which can reach
# editor.mmotee.online whereas the development sandbox cannot).
set -uo pipefail

BASE="${EDITOR_BASE:-https://editor.mmotee.online}"
JAR="$(mktemp)"
OUT=".ci/out"
mkdir -p "$OUT"

req() {
  local path="$1"; shift
  curl -sS -m 60 -b "$JAR" -c "$JAR" \
    -H "Origin: $BASE" -H "Referer: $BASE/index.html" \
    -H 'Accept: application/json' \
    "$@" "$BASE/$path"
  echo
}

post_json() {
  local path="$1"; local body="$2"
  req "$path" -H 'Content-Type: application/json' -X POST --data "$body"
}

echo "== status (before) =="
req 'api/auth.php?action=status'

echo "== guest_login =="
post_json 'api/auth.php?action=guest_login' \
  "{\"login\":\"${GUEST_LOGIN:-test}\",\"password\":\"${GUEST_PASSWORD:-123123}\"}" | tee "$OUT/login.json"

echo "== status (after) =="
req 'api/auth.php?action=status'

echo "== change-requests config =="
req 'api/change-requests.php?action=config' > "$OUT/cr-config.json"
wc -c "$OUT/cr-config.json"

for res in quests quest_bots bots_info bots_npc worlds items nodes_collecting; do
  echo "== list $res =="
  req "api/db-crud.php?action=list&resource=$res&limit=5000" > "$OUT/$res.json"
  wc -c "$OUT/$res.json"
done
