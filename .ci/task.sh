#!/usr/bin/env bash
# Temporary remote driver script (runs on a GitHub runner, which can reach
# editor.mmotee.online whereas the development sandbox cannot).
set -uo pipefail

BASE="${EDITOR_BASE:-https://editor.mmotee.online}"
JAR="$(mktemp)"
OUT=".ci/results"
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

echo "== guest_login =="
post_json 'api/auth.php?action=guest_login' \
  "{\"login\":\"${GUEST_LOGIN:-test}\",\"password\":\"${GUEST_PASSWORD:-123123}\"}" > "$OUT/login.json"
head -c 400 "$OUT/login.json"; echo

echo "== my requests =="
req 'api/change-requests.php?action=mine' > "$OUT/mine.json"
cat "$OUT/mine.json" | head -c 2000; echo

echo "== mobs =="
req 'api/db-crud.php?action=list&resource=bots_mobs&limit=5000' > "$OUT/bots_mobs.json"
wc -c "$OUT/bots_mobs.json"

echo "== npc 31 =="
req 'api/db-crud.php?action=get&resource=bots_npc&id=31' > "$OUT/npc31.json"
cat "$OUT/npc31.json"; echo

echo "== crafts (count only) =="
req 'api/db-crud.php?action=list&resource=crafts&limit=5' > "$OUT/crafts.json"
wc -c "$OUT/crafts.json"
