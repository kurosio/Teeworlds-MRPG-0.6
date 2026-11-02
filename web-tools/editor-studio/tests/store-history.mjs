// Проверка хранилища истории изменений на настоящем SQLite.
//
//   node tests/store-history.mjs
//
// `tests/php-harness.mjs` работает в php-wasm без расширения pdo_sqlite, поэтому
// SQL хранилища там проверить нельзя: он проверяется здесь. Нужен пакет
// @php-wasm/node (сборка WordPress playground с SQLite и PDO):
//
//   npm install --no-save @php-wasm/node @php-wasm/universal
//
// Скрипт создаёт временную базу в виртуальной файловой системе и проходит по
// тем же запросам, что и api/store.php: схема 5, вставка, фильтры, поиск,
// постраничная навигация, отметки об отмене, ограничение размера журнала и
// обновление существующей базы схемы 4.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

let loadNodeRuntime;
let PHP;
try {
  ({ loadNodeRuntime } = await import('@php-wasm/node'));
  ({ PHP } = await import('@php-wasm/universal'));
} catch {
  console.log('skip: @php-wasm/node is not installed (npm install --no-save @php-wasm/node @php-wasm/universal)');
  process.exit(0);
}

// processId обязателен: без него php-wasm не может поднять файловые блокировки.
const php = new PHP(await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } }));
php.mkdir('/studio');
php.mkdir('/studio/api');
php.mkdir('/studio/data');
php.writeFile('/studio/api/store.php', fs.readFileSync(path.join(ROOT, 'api/store.php'), 'utf8'));

const code = `<?php
putenv('EDITOR_STUDIO_SQLITE=/studio/data/test.sqlite');
require '/studio/api/store.php';
$fail = [];
$ok = function (bool $cond, string $what) use (&$fail) { if (!$cond) { $fail[] = $what; } };
$ok(extension_loaded('pdo_sqlite'), 'pdo_sqlite');
$ok((int)store_db()->query('PRAGMA user_version')->fetchColumn() === EDITOR_STORE_SCHEMA, 'schema version');
$cols = [];
foreach (store_db()->query('PRAGMA table_info(db_history)') as $c) { $cols[] = $c['name']; }
$ok(count($cols) === 21, 'db_history columns: ' . count($cols));

$id = store_history_add([
  'actor_role' => 'admin', 'actor_id' => 'admin', 'actor_name' => 'admin',
  'source' => 'editor', 'resource' => 'worlds', 'table_name' => 'tw_worlds',
  'record_id' => 7, 'op' => 'update', 'label' => 'Forest',
  'before' => ['Name' => 'Old'], 'after' => ['Name' => 'New'],
]);
$ok($id > 0, 'insert id');
$id2 = store_history_add([
  'actor_role' => 'guest', 'actor_id' => 'g1', 'actor_name' => 'Vasya', 'approved_by' => 'admin',
  'source' => 'request', 'request_id' => 'CR-0001', 'resource' => 'items',
  'table_name' => 'tw_items_list', 'record_id' => 12, 'op' => 'create', 'label' => 'Sword',
  'after' => ['Name' => 'Sword', 'Data' => ['a' => 1]],
]);
$id3 = store_history_add([
  'resource' => 'vouchers', 'table_name' => 'tw_vouchers', 'record_id' => 3, 'op' => 'delete',
  'before' => ['Code' => 'PROMO'], 'revertable' => false, 'source' => 'revert', 'revert_of' => $id,
]);
$ok($id3 > 0, 'insert revert entry');

$row = store_history_get($id);
$ok($row['before'] === ['Name' => 'Old'] && $row['after'] === ['Name' => 'New'], 'before/after roundtrip');
$ok($row['revertable'] === true, 'revertable decoded');
$ok(store_history_get(9999) === null, 'missing row null');

$list = store_history_list([], 10, 0);
$ok($list['total'] === 3, 'list total: ' . $list['total']);
$ok($list['rows'][0]['id'] === $id3, 'list newest first');
$ok(store_history_list(['resource' => 'items'], 10, 0)['total'] === 1, 'filter by resource');
$ok(store_history_list(['source' => 'request', 'state' => 'revertible'], 10, 0)['total'] === 1, 'filter source+state');
$ok(store_history_list(['actor_role' => 'guest'], 10, 0)['total'] === 1, 'filter by actor');
$ok(store_history_list(['q' => 'sword'], 10, 0)['total'] === 1, 'search label');
$ok(store_history_list(['q' => '12'], 10, 0)['total'] === 1, 'search record id');
$ok(store_history_list(['q' => 'CR-0001'], 10, 0)['total'] === 1, 'search request id');
$page = store_history_list([], 2, 0);
$ok(count($page['rows']) === 2 && $page['total'] === 3, 'pagination');
$ok(count(store_history_list([], 2, 2)['rows']) === 1, 'pagination offset');
$ok(store_history_list(['state' => 'reverted'], 10, 0)['total'] === 0, 'state reverted (none yet)');

$counts = store_history_counts();
$ok($counts['total'] === 3 && $counts['revertible'] === 2, 'counts: ' . json_encode($counts));

store_history_mark_reverted($id, $id3);
$ok(store_history_counts()['revertible'] === 1, 'counts after mark');
$ok(store_history_list(['state' => 'reverted'], 10, 0)['total'] === 1, 'state reverted after mark');
store_history_unmark_reverted($id);
$ok(store_history_get($id)['reverted_at'] === '', 'unmark');

// Обрезка: переполняем журнал и проверяем, что остаются самые новые записи.
for ($i = 0; $i < 1100; $i++) {
  store_history_add(['resource' => 'worlds', 'op' => 'update', 'record_id' => $i,
    'before' => ['Name' => 'a'], 'after' => ['Name' => 'b']]);
}
$pdo0 = store_db();
$pdo0->exec('DELETE FROM db_history WHERE id > 3');
for ($i = 0; $i < 5700; $i++) {
  $pdo0->prepare('INSERT INTO db_history(at, resource, op, record_id, before, after) VALUES(?, ?, ?, ?, ?, ?)')
    ->execute(['2026-01-01T00:00:00Z', 'worlds', 'update', $i, '{}', '{}']);
}
store_history_prune();
$left = (int)$pdo0->query('SELECT COUNT(*) FROM db_history')->fetchColumn();
$ok($left === 5000, 'prune trims to 5000, got ' . $left);
$min = (int)$pdo0->query('SELECT MIN(id) FROM db_history')->fetchColumn();
$max = (int)$pdo0->query('SELECT MAX(id) FROM db_history')->fetchColumn();
$ok($min === $max - 4999, 'prune drops oldest only');
$keepId = store_history_add(['resource' => 'worlds', 'op' => 'update', 'record_id' => 999999, 'before' => ['Name' => 'a'], 'after' => ['Name' => 'b']]);
store_history_prune();
$ok((int)$pdo0->query('SELECT COUNT(*) FROM db_history')->fetchColumn() === 5001, 'prune keeps under limit + 1');
$ok(store_history_get($keepId)['id'] === $keepId, 'prune keeps newest entry');

$pdo = store_db();
$pdo->exec('DROP TABLE db_history');
$pdo->exec('PRAGMA user_version = 4');
store_schema(store_db());
$ok((int)store_db()->query('PRAGMA user_version')->fetchColumn() === 5, 'migrated to 5');
$ok((bool)store_db()->query("SELECT name FROM sqlite_master WHERE type='table' AND name='db_history'")->fetchColumn(), 'history table recreated');

echo $fail ? ('FAIL: ' . implode(' | ', $fail) . "\\n") : "store history: ok\\n";
`;
const out = await php.run({ code });
console.log(out.text.trim());
if (out.errors) console.error(out.errors);
// php-wasm держит worker живым: без явного выхода процесс не завершается.
process.exit(out.text.includes('store history: ok') ? 0 : 1);
