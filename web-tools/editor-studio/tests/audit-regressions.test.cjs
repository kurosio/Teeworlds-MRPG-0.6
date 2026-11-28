'use strict';

/**
 * tests/audit-regressions.test.cjs — регрессионные проверки по итогам аудита:
 *  - отсутствие невалидных CSS-токенов/классов, смешанных кавычек и битых вызовов UI;
 *  - корректный перевод новых и исправленных шаблонов `{0}` в RU и обратный откат в EN;
 *  - безопасный teardown таймеров ChangeMarkers при закрытии окна;
 *  - совместимость TestMysqli + api/db-resources.php и api/db.php lookup ACL в PHP-WASM.
 */

const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { JSDOM } = require('jsdom');

const ROOT = path.join(__dirname, '..');
const read = (rel) => fs.readFileSync(path.join(ROOT, rel), 'utf8');

let loadNodeRuntime = null;
let PHP = null;
try {
  ({ loadNodeRuntime } = require('@php-wasm/node'));
  ({ PHP } = require('@php-wasm/universal'));
} catch {
  // Опциональные пакеты @php-wasm/node/@php-wasm/universal.
}

test('HTML/CSS/JS контракты: без смешанных кавычек, потерянных токенов и copyToClipboard', () => {
  const htmlFiles = fs.readdirSync(ROOT).filter((f) => f.endsWith('.html'));
  const jsFiles = fs.readdirSync(path.join(ROOT, 'editor-core')).filter((f) => f.endsWith('.js')).map((f) => `editor-core/${f}`);
  for (const rel of [...htmlFiles, ...jsFiles]) {
    const src = read(rel);
    assert.ok(!/“[^”\n]*»/.test(src), `${rel}: не должно оставаться смешанных кавычек “...»`);
    assert.ok(!src.includes('--editor-primary-soft'), `${rel}: неопределённая переменная --editor-primary-soft`);
    assert.ok(!src.includes('editor-section-divider'), `${rel}: неопределённый класс editor-section-divider`);
    assert.ok(!src.includes('copyToClipboard'), `${rel}: устаревший вызов copyToClipboard (нужно EditorCore.Page.copyText)`);
  }

  const questsHtml = read('quests-editor.html');
  assert.ok(questsHtml.includes('EditorCore.Page.copyText('), 'quests-editor.html должен копировать через EditorCore.Page.copyText');

  const dungeonsHtml = read('dungeons-editor.html');
  assert.ok(!/sec cooldown\./.test(dungeonsHtml), 'dungeons-editor.html не должен путать TimeLimit с cooldown');

  const sshHtml = read('ssh-servers.html');
  assert.ok(!/\blet busy\b/.test(sshHtml), 'ssh-servers.html не должен содержать неиспользуемую переменную busy');

  const tailwindCss = read('editor-core/tailwind.css');
  for (const cls of ['.mr-auto', '.pt-1', '.text-green-600', '.text-red-500']) {
    assert.ok(tailwindCss.includes(cls), `editor-core/tailwind.css должен включать ${cls}`);
  }
});

test('ChangeMarkers teardown: запланированный refreshNow не падает после window.close()', async () => {
  const dom = new JSDOM('<!DOCTYPE html><body><div data-resource="items" data-id="1"></div></body>', {
    url: 'https://studio.test/items-editor.html',
    runScripts: 'outside-only',
  });
  const uncaught = [];
  const onErr = (err) => uncaught.push(err);
  process.on('uncaughtException', onErr);
  try {
    dom.window.requestAnimationFrame = (cb) => dom.window.setTimeout(cb, 20);
    dom.window.eval(read('editor-core/change-markers.js'));
    dom.window.EditorCore.ChangeMarkers.setRequest({ id: 'r1', operations: [] });
    // Закрываем окно до срабатывания запланированного refreshNow()
    dom.window.close();
    await new Promise((resolve) => setTimeout(resolve, 60));
    assert.deepEqual(uncaught, [], 'таймер ChangeMarkers не должен падать после teardown');
  } finally {
    process.off('uncaughtException', onErr);
  }
});

test('I18n переводит новые и составные шаблоны {0} на русский и возвращает английский', () => {
  const dom = new JSDOM('<!DOCTYPE html><body></body>', {
    url: 'https://studio.test/quests-editor.html',
    runScripts: 'outside-only',
  });
  try {
    const w = dom.window;
    w.EditorCoreLocalization = {};
    w.eval(read('localization/index.js'));
    for (const f of w.EditorCoreLocalization.files) {
      w.eval(read(`localization/${f}`));
    }
    w.eval(read('editor-core/i18n.js'));

    const samples = [
      'ID 7 · Expires: 2026-12-31',
      'ID 7 · Next: #8',
      'Quest: Dragon Hunt',
      'Step 2 of 5',
      'Steps: 5',
      'Shown: 10 of 42',
      'Under review: 2; total requests: 9',
      'No requests under review; total requests: 9',
      'Request updated: version 3',
      'Request applied: 2 operations, skipped 1',
      'Data to delete (4 fields)',
      '5 server(s)',
      'Run server update over SSH on 3 server(s)?',
      'Link ignored: «A» → «B» are different events (+2 more).',
      'Event: OnStart (on_start) — its steps never link outside this event',
      'Remove field from operation',
      'Dungeon #4 — Forest',
      'Limit: 600 sec.',
      'show (350 chars)',
      'Fields: Name, Price',
      '· sent 2026-11-27',
      'Could not verify against the current database: connection refused',
      'Expires',
      'Limits',
      'Next: #8',
      'Change in request req_1 — awaiting administrator review',
      'Editing request',
      'intro ⚠ other event',
    ];

    const nodes = samples.map((text) => {
      const span = w.document.createElement('span');
      span.textContent = text;
      w.document.body.appendChild(span);
      return [text, span];
    });

    w.EditorCore.I18n.set('ru', { force: true });
    for (const [en, span] of nodes) {
      assert.ok(/[А-Яа-яЁё]/.test(span.textContent), `ожидался русский перевод для "${en}", получено "${span.textContent}"`);
      assert.ok(!/\b(?:updated|total requests|skipped|server\(s\)|step\(s\)|different events|sec cooldown|chars|other event|awaiting administrator)\b/i.test(span.textContent),
        `в русском переводе "${en}" остались непереведённые английские фрагменты: "${span.textContent}"`);
    }

    w.EditorCore.I18n.set('en');
    for (const [en, span] of nodes) {
      assert.equal(span.textContent, en);
    }
  } finally {
    dom.window.close();
  }
});

test('PHP-WASM: crud_update_plan + api/db.php lookup ACL для quest_boards/scenarios/dungeons', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const rtId = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(rtId);
  try {
    php.mkdir('/studio');
    php.mkdir('/studio/api');
    php.mkdir('/studio/data');
    php.mkdir('/studio/editor-core');
    for (const f of fs.readdirSync(path.join(ROOT, 'api'))) {
      if (f.endsWith('.php')) php.writeFile(`/studio/api/${f}`, read(`api/${f}`));
    }
    php.writeFile('/studio/editor-core/reference-schema.json', read('editor-core/reference-schema.json'));
    php.writeFile('/studio/api/db-smoke.php', `<?php
require_once '/studio/api/db-resources.php';
require_once '/studio/api/ai-lib.php';
$pdo = new PDO('sqlite::memory:');
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$pdo->exec('CREATE TABLE tw_quest_boards (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT, WorldID INTEGER, PosX INTEGER, PosY INTEGER)');
$pdo->exec("INSERT INTO tw_quest_boards (Name, WorldID, PosX, PosY) VALUES ('Town Board', 0, 100, 200)");
$stmt = $pdo->query('SELECT * FROM tw_quest_boards WHERE ID = 1');
$row = $stmt->fetch(PDO::FETCH_ASSOC);
$plan = crud_update_plan($row, ['Name' => 'Town Board v2', 'WorldID' => 0], []);
$tabs = ai_lookup_tabs();
echo json_encode([
  'ok' => $plan !== null && $plan['data'] === ['Name' => 'Town Board v2'] && $plan['before'] === ['Name' => 'Town Board'],
  'quests_has_scenarios' => in_array('scenarios', $tabs['quests'], true),
  'quests_has_boards' => in_array('quest_boards', $tabs['quests'], true),
  'bots_has_dungeons' => in_array('dungeons', $tabs['bots'], true),
]);
`);
    const { text } = await php.run({ scriptPath: '/studio/api/db-smoke.php' });
    const res = JSON.parse(text);
    assert.deepEqual(res, {
      ok: true,
      quests_has_scenarios: true,
      quests_has_boards: true,
      bots_has_dungeons: true,
    });
  } finally {
    try { php.exit(0); } catch {}
  }
});

test('localization/lang_ru.js не содержит дублирующихся ключей в strings и plurals', () => {
  const src = read('localization/lang_ru.js');
  for (const section of ['plurals', 'strings']) {
    const start = src.indexOf(`${section}: {`);
    assert.ok(start !== -1, `не найден блок ${section}`);
    const end = section === 'plurals' ? src.indexOf('strings: {', start) : src.length;
    const block = src.slice(start, end);
    const seen = new Map();
    const dups = [];
    for (const m of block.matchAll(/^\s+"((?:[^"\\]|\\.)+)"\s*:/gm)) {
      const key = JSON.parse(`"${m[1]}"`);
      if (seen.has(key)) dups.push(key);
      seen.set(key, true);
    }
    assert.deepEqual(dups, [], `дублирующиеся ключи в lang_ru.js (${section}): ${dups.join(', ')}`);
  }
});

test('PHP-WASM: api/map-datafile.php даёт идентичный разбор main.map для map-edit.php и ai-lib.php', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const mainMapPath = path.resolve(ROOT, '../../maps/main.map');
  assert.ok(fs.existsSync(mainMapPath), `не найдена карта ${mainMapPath}`);
  const rtId = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(rtId);
  try {
    php.mkdir('/studio');
    php.mkdir('/studio/api');
    php.mkdir('/studio/maps');
    for (const f of fs.readdirSync(path.join(ROOT, 'api'))) {
      if (f.endsWith('.php')) php.writeFile(`/studio/api/${f}`, read(`api/${f}`));
    }
    php.writeFile('/studio/maps/main.map', fs.readFileSync(mainMapPath));
    php.writeFile('/studio/api/map-datafile-smoke.php', `<?php
require_once '/studio/api/map-datafile.php';
require_once '/studio/api/ai-lib.php';
$bytes = file_get_contents('/studio/maps/main.map');
$edit = ddnet_parse_datafile($bytes, 50331648, 512 * 1024 * 1024, true);
$ai = ai_map_parse_datafile($bytes, false, 0, 0);
echo json_encode([
  'same_width' => ($edit['game']['width'] ?? null) === ($ai['width_tiles'] ?? -1),
  'same_height' => ($edit['game']['height'] ?? null) === ($ai['height_tiles'] ?? -1),
  'same_switch' => ($edit['switch'] !== null) === (($ai['switch_layer'] ?? null) !== null),
  'has_hashes' => !empty($edit['types_hash']) && !empty($edit['raw_hashes']),
  'blocks_traversal' => ddnet_map_resolve('/studio/maps', '../api/db-core.php') === null,
  'allows_valid' => ddnet_map_resolve('/studio/maps', 'main.map') === '/studio/maps/main.map',
]);
`);
    const { text } = await php.run({ scriptPath: '/studio/api/map-datafile-smoke.php' });
    assert.deepEqual(JSON.parse(text), {
      same_width: true,
      same_height: true,
      same_switch: true,
      has_hashes: true,
      blocks_traversal: true,
      allows_valid: true,
    });
  } finally {
    try { php.exit(0); } catch {}
  }
});
