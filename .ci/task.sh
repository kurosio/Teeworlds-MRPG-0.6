#!/usr/bin/env bash
# Temporary remote driver script (runs on a GitHub runner, which can reach
# editor.mmotee.online whereas the development sandbox cannot).
set -uo pipefail

BASE="${EDITOR_BASE:-https://editor.mmotee.online}"
JAR="$(mktemp)"
OUT=".ci/results"
MODE="$(head -n 1 .ci/mode 2>/dev/null || echo preview)"
NICKNAME="${GUEST_NICKNAME:-Arena Agent}"
mkdir -p "$OUT"

req() {
  local path="$1"; shift
  curl -sS -m 120 -b "$JAR" -c "$JAR" \
    -H "Origin: $BASE" -H "Referer: $BASE/index.html" \
    -H 'Accept: application/json' \
    "$@" "$BASE/$path"
  echo
}

post_json() {
  local path="$1"; local body="$2"
  req "$path" -H 'Content-Type: application/json' -X POST --data "$body"
}

body_for() {
  python3 - "$1" "$2" <<'PY'
import json, sys
path, nickname = sys.argv[1], sys.argv[2]
ops = json.load(open(path))
msg = {
    '.ci/content/cave.json': (
        "Cave (world 16, level 50): цепочка из трёх сайд-квестов у Торговца в пещере "
        "(NPC 31, bot 1087): медь и уголь -> аметистовые призмы -> сера и корни. "
        "Награды 250/450/900 золота и 200/400/800 опыта, все шаги с диалогами и задачами."
    ),
    '.ci/content/forest.json': (
        "Forest (world 6, level 15): два сайд-квеста у шахтёра Джексона (NPC 4, bot 26) — "
        "зачистка леса от категеров/кутал и охота на ведьм с поставкой железной руды. "
        "Лесная локация до сих пор без квестов, это первый контент для неё."
    ),
}.get(path, "Новые квесты и шаги квестов (гостевой черновик).")
print(json.dumps({"nickname": nickname, "message": msg, "operations": ops}, ensure_ascii=False))
PY
}

summarize() {
  python3 - "$1" <<'PY'
import json, sys
raw = open(sys.argv[1]).read().strip()
try:
    d = json.loads(raw)
except Exception:
    print("  (not json):", raw[:2000]); raise SystemExit
print("  ok:", d.get("ok"), "| error:", d.get("error"), "| code:", d.get("code"))
for k in ("warnings", "notes"):
    if d.get(k):
        print(f"  {k}:", json.dumps(d[k], ensure_ascii=False)[:3000])
for k in ("analysis", "summary", "stats"):
    if d.get(k):
        print(f"  {k}:", json.dumps(d[k], ensure_ascii=False)[:4000])
if d.get("id"):
    print("  request id:", d.get("id"), "status:", d.get("status"))
if d.get("request"):
    r = d["request"]
    print("  request id:", r.get("id"), "status:", r.get("status"))
PY
}

echo "== guest_login =="
post_json 'api/auth.php?action=guest_login' \
  "{\"login\":\"${GUEST_LOGIN:-test}\",\"password\":\"${GUEST_PASSWORD:-123123}\"}" > "$OUT/login.json"
python3 -c "import json;d=json.load(open('$OUT/login.json'));print(' role:',d.get('role'),'user:',d.get('user'),'ok:',d.get('ok'),'err:',d.get('error'))"

echo "== mode: $MODE =="

if [ "$MODE" = "submit" ]; then
  ACTION_LIST="cave forest"
else
  ACTION_LIST="cave forest"
fi

for pack in $ACTION_LIST; do
  file=".ci/content/$pack.json"
  [ -f "$file" ] || continue
  body_for "$file" "$NICKNAME" > "$OUT/$pack-body.json"
  echo "== $pack: $(wc -c < "$file") bytes of ops =="

  if [ "$MODE" = "submit" ]; then
    req 'api/change-requests.php?action=preview' -H 'Content-Type: application/json' -X POST \
      --data @"$OUT/$pack-body.json" > "$OUT/$pack-preview.json"
    echo "-- preview --"; summarize "$OUT/$pack-preview.json"

    if python3 -c "import json,sys;sys.exit(0 if json.load(open('$OUT/$pack-preview.json')).get('ok') else 1)"; then
      echo "-- submit --"
      req 'api/change-requests.php?action=submit' -H 'Content-Type: application/json' -X POST \
        --data @"$OUT/$pack-body.json" > "$OUT/$pack-submit.json"
      summarize "$OUT/$pack-submit.json"
      sleep 65
    else
      echo "-- submit skipped: preview not ok --"
    fi
  else
    req 'api/change-requests.php?action=preview' -H 'Content-Type: application/json' -X POST \
      --data @"$OUT/$pack-body.json" > "$OUT/$pack-preview.json"
    echo "-- preview --"; summarize "$OUT/$pack-preview.json"
  fi
done

echo "== my requests =="
req 'api/change-requests.php?action=mine' > "$OUT/mine.json"
python3 - <<'PY'
import json
d = json.load(open('.ci/results/mine.json'))
rows = d.get('requests') or d.get('rows') or []
print("  total:", len(rows))
for r in rows:
    print("  ", r.get('id'), r.get('status'), '|', (r.get('author') or {}).get('nickname'), '|', (r.get('message') or '')[:60])
PY

echo "== request details =="
for id in CR-20261025-162749-5b4f86 CR-20261025-162855-70c47e; do
  req "api/change-requests.php?action=get&id=$id&peek=1" > "$OUT/req-$id.json"
  python3 - "$OUT/req-$id.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
r = d.get('request') or d
print('  id:', r.get('id'), '| status:', r.get('status'), '| ops:', len(r.get('operations') or []), '| version:', r.get('version'))
for o in (r.get('operations') or []):
    print('   -', o.get('type'), o.get('resource'), o.get('id') or o.get('tempId'))
print('  message:', (r.get('message') or '')[:120])
PY
done
