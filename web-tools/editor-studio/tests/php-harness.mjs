// Runs the PHP parts of the studio in a wasm PHP runtime.
//
//   node tests/php-harness.mjs            # syntax check of every api/*.php + helper tests
//
// The browser studio is checked by node:test; the server keeps its value rules
// (NULL / '' / 0 comparison) and the DB-password config rules in plain PHP
// functions, so they are exercised here instead of being reviewed by eye.
// Requires the optional `php-wasm` package:
//   npm install --no-save php-wasm
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

let PhpNode;
try {
  ({ PhpNode } = await import('php-wasm/PhpNode.mjs'));
} catch {
  console.log('skip: php-wasm is not installed (npm install --no-save php-wasm)');
  process.exit(0);
}

const php = new PhpNode();
const errors = [];

/** Runs PHP code and returns everything it printed. */
async function run(code) {
  let output = '';
  const listener = (event) => { output += event.detail.join(''); };
  php.addEventListener('output', listener);
  const exitCode = await php.run(code);
  php.removeEventListener('output', listener);
  return { output, exitCode };
}

// php-wasm has its own virtual file system: copy the sources into it once.
const VFS_ROOT = '/studio';
await php.mkdir('/studio');
for (const dir of ['api']) {
  const target = `${VFS_ROOT}/${dir}`;
  await php.mkdir(target);
  for (const name of fs.readdirSync(path.join(ROOT, dir))) {
    if (!name.endsWith('.php')) continue;
    await php.writeFile(`${target}/${name}`, fs.readFileSync(path.join(ROOT, dir, name), 'utf8'), { encoding: 'utf8' });
  }
}

await php.mkdir(`${VFS_ROOT}/editor-core`);
await php.writeFile(`${VFS_ROOT}/editor-core/reference-schema.json`, fs.readFileSync(path.join(ROOT, 'editor-core/reference-schema.json'), 'utf8'), { encoding: 'utf8' });

async function syntaxCheck(file) {
  // token_get_all() compiles the source: a parse error shows up here.
  const { output } = await run(`<?php token_get_all(file_get_contents(${JSON.stringify(`${VFS_ROOT}/${file}`)})); echo 'ok';`);
  if (!output.includes('ok')) errors.push(`${file}: ${output.trim() || 'parse error'}`);
  return output.includes('ok');
}

const phpFiles = fs.readdirSync(path.join(ROOT, 'api'))
  .filter((name) => name.endsWith('.php'))
  .map((name) => `api/${name}`);

let checked = 0;
for (const file of phpFiles) {
  if (await syntaxCheck(file)) checked += 1;
}
console.log(`php syntax: ${checked}/${phpFiles.length} files`);

// ── Value comparison (NULL / '' / 0) ────────────────────────────────────────
const cases = [
  // a, b, json, expected — «нет значения» с обеих сторон считается равенством
  [null, '', false, true],
  [null, '0', false, true],
  [null, 0, false, true],
  ['', 0, false, true],
  ['', '0', false, true],
  [null, 'Material', false, false],
  [7, '7', false, true],
  [7, 0, false, false],
  ['7.00', 7, false, true],
  ['Text', 'text', false, false],
  [null, '[]', true, true],
  [null, [], true, true],
  ['{"a":1}', '{"a":1}', true, true],
  ['{"a":1}', '{"a":2}', true, false],
  ['{"a":1}', null, true, false],
];

const phpCaseList = JSON.stringify(cases);
const { output: valueOutput } = await run(`<?php
require '${VFS_ROOT}/api/db-resources.php';
$cases = json_decode(${JSON.stringify(phpCaseList)}, true);
$failed = 0;
foreach ($cases as [$a, $b, $isJson, $expected]) {
    $actual = editor_values_equal($a, $b, (bool)$isJson);
    if ($actual !== $expected) {
        $failed++;
        echo "FAIL editor_values_equal(" . var_export($a, true) . ', ' . var_export($b, true) . '): '
            . var_export($actual, true) . " expected " . var_export($expected, true) . "\\n";
    }
}
echo $failed === 0 ? "values: ok\\n" : "values: {$failed} failed\\n";
`);
if (!valueOutput.includes('values: ok')) errors.push(valueOutput.trim());
process.stdout.write(valueOutput);

// ── Resource / request helpers still load together ──────────────────────────
const { output: libOutput } = await run(`<?php
require '${VFS_ROOT}/api/change-requests-lib.php';
$checks = [
    cr_values_equal(null, '', false),
    cr_values_equal(null, 0, false),
    !cr_values_equal(5, 0, false),
    cr_values_equal('7', 7, false),
];
echo in_array(false, $checks, true) ? "cr_values_equal: FAIL\\n" : "cr_values_equal: ok\\n";
`);
if (!libOutput.includes('cr_values_equal: ok')) errors.push(libOutput.trim());
process.stdout.write(libOutput);

// ── Временные ID ингредиентов крафта заменяются ID из БД ────────────────────
{
  const { output } = await run(`<?php
require_once '${VFS_ROOT}/api/change-requests-lib.php';
$itemTemp = 2000000123;
$craftTemp = 2000000456;
$raw = [
    ['type'=>'create', 'resource'=>'crafts', 'tempId'=>$craftTemp,
        'data'=>['ItemID'=>0, 'ItemValue'=>1, 'RequiredItems'=>"[{$itemTemp}/2]"]],
    ['type'=>'create', 'resource'=>'items', 'tempId'=>$itemTemp, 'data'=>['Name'=>'Request item']],
];
$ordered = cr_normalize_ops($raw, 10);
$orderedCorrectly = count($ordered) === 2 && $ordered[0]['resource'] === 'items' && $ordered[1]['resource'] === 'crafts';
$canonical = cr_remap_craft_required_items("[{$itemTemp}/2],[17/1]", [$itemTemp => 84]);
$json = cr_remap_craft_required_items(json_encode([[$itemTemp, 2], [17, 1]]), [$itemTemp => 84]);
$jsonPairs = json_decode($json, true);
$checks = [
    $orderedCorrectly,
    cr_craft_required_item_temp_refs("[{$itemTemp}/2]") === [$itemTemp],
    $canonical === '[84/2],[17/1]',
    is_array($jsonPairs) && $jsonPairs[0][0] === 84 && $jsonPairs[1][0] === 17,
];
echo in_array(false, $checks, true) ? "craft ID remapping: FAIL\n" : "craft ID remapping: ok\n";
`);
  if (!output.includes('craft ID remapping: ok')) errors.push(output.trim() || 'craft ID remapping failed');
  process.stdout.write(output);
}

// ── Общие типы ссылок: PHP заменяет ровно те ID, которые описаны схемой ─────
{
  const { fixtures, idMap } = await import('./reference-fixtures.mjs');
  await php.writeFile(`${VFS_ROOT}/ref-fixtures.json`, JSON.stringify({ fixtures, idMap }), { encoding: 'utf8' });
  const { output } = await run(`<?php
require_once '${VFS_ROOT}/api/change-requests-lib.php';
$f = json_decode(file_get_contents('${VFS_ROOT}/ref-fixtures.json'), true, 512, JSON_THROW_ON_ERROR);
$idMap = $f['idMap'];
$failed = 0;
foreach ($f['fixtures'] as $n => $case) {
    $actual = editor_remap_references($case['resource'], $case['data'], $idMap);
    if (json_encode($actual, JSON_UNESCAPED_UNICODE) !== json_encode($case['expected'], JSON_UNESCAPED_UNICODE)) {
        $failed++;
        echo "FAIL case " . ($n + 1) . ' (' . $case['resource'] . "):\\n"
            . json_encode($actual, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT) . "\\n";
    }
    // Повторная замена уже реальных ID не меняет результат.
    $again = editor_remap_references($case['resource'], $actual, $idMap);
    if (json_encode($again) !== json_encode($actual)) { $failed++; echo "FAIL idempotent case " . ($n + 1) . "\\n"; }
}
// Ссылка на новую запись вне черновика и несовместимый тип раздела отклоняются.
$missing = null; $wrongTarget = null;
try {
    cr_normalize_ops([['type'=>'create','resource'=>'crafts','tempId'=>2000000001,
        'data'=>['ItemID'=>2000000009, 'RequiredItems'=>"[2000000009/1]"]]], 10);
} catch (CrInputError $e) { $missing = $e->getMessage(); }
try {
    cr_normalize_ops([
        ['type'=>'create','resource'=>'items','tempId'=>2000000001, 'data'=>['Name'=>'X']],
        ['type'=>'create','resource'=>'crafts','tempId'=>2000000002, 'data'=>['WorldID'=>2000000001]],
    ], 10);
} catch (CrInputError $e) { $wrongTarget = $e->getMessage(); }
// Ссылка на запись, удаляемую этим же запросом.
$deleted = null;
try {
    cr_normalize_ops([
        ['type'=>'create','resource'=>'crafts','tempId'=>2000000001, 'data'=>['ItemID'=>7]],
        ['type'=>'delete','resource'=>'items','id'=>7],
    ], 10);
} catch (CrInputError $e) { $deleted = $e->getMessage(); }
if ($missing === null || strpos($missing, 'которой нет в этом запросе') === false) { $failed++; echo "FAIL missing ref\\n"; }
if ($wrongTarget === null || strpos($wrongTarget, 'требуется worlds') === false) { $failed++; echo "FAIL wrong target\\n"; }
if ($deleted === null || strpos($deleted, 'удаляемую в этом же запросе') === false) { $failed++; echo "FAIL deleted ref\\n"; }
echo $failed === 0 ? "reference remapping: ok\\n" : "reference remapping: {$failed} failed\\n";
`);
  if (!output.includes('reference remapping: ok')) errors.push(output.trim() || 'reference remapping failed');
  process.stdout.write(output);
}

// ── Одобрение запроса: временные ID черновика становятся реальными ID БД ────
{
  const { output } = await run(`<?php
require_once '${VFS_ROOT}/api/change-requests-lib.php';
$I = 2000000001; $W = 2000000002; $B = 2000000003; $Q = 2000000004; $C = 2000000005;
$raw = [
    ['type'=>'create','resource'=>'crafts','tempId'=>$C,
        'data'=>['GroupName'=>'Recipe','ItemID'=>$I,'ItemValue'=>1,'RequiredItems'=>"[$I/2],[17/3]",'WorldID'=>$W]],
    ['type'=>'create','resource'=>'quests','tempId'=>$Q,
        'data'=>['Name'=>'Loop','NextQuestID'=>$Q,'Money'=>0,'Exp'=>0]],
    ['type'=>'create','resource'=>'bots_info','tempId'=>$B,
        'data'=>['Name'=>'Bot','SlotHammer'=>$I,'EquippedModules'=>"17,$I"]],
    ['type'=>'create','resource'=>'worlds','tempId'=>$W, 'data'=>['Name'=>'World','Path'=>'w','Type'=>'default']],
    ['type'=>'create','resource'=>'items','tempId'=>$I, 'data'=>['Name'=>'Sword']],
    ['type'=>'update','resource'=>'bots_npc','id'=>4,
        'data'=>['BotID'=>$B,'DialogData'=>json_encode([['left_speaker_id'=>$B,'text'=>"Take <item_$I> in <world_$W>"]])]],
    ['type'=>'update','resource'=>'items','id'=>12,
        'data'=>['Data'=>json_encode(['random_box'=>[['item_id'=>$I,'value'=>3]],'use_scenario'=>['world_id'=>$W,'duration'=>5]])]],
    ['type'=>'update','resource'=>'nodes_collecting','id'=>3, 'data'=>['Properties'=>"[$I/2/50]"]],
];
$ops = cr_normalize_ops($raw, 50);
// Порядок: сначала записи без зависимостей, затем ссылающиеся на них.
$order = array_map(fn($o) => $o['resource'], $ops);
$next = 80; $rows = []; $updates = [];
$result = cr_apply_sequence(
    $ops,
    function ($R, $data) use (&$next, &$rows) { $rows[$R['key'] . ':' . ++$next] = $data; return $next; },
    function ($R, $id, $data) use (&$rows, &$updates) {
        $key = $R['key'] . ':' . $id;
        $rows[$key] = array_replace($rows[$key] ?? [], $data);
        $updates[] = $key;
    },
    function ($R, $id) use (&$updates) { $updates[] = 'delete:' . $R['key'] . ':' . $id; },
    fn($R, $id) => null,
);
$idMap = $result['idMap'];
$stored = json_encode($rows, JSON_UNESCAPED_UNICODE);
$failed = 0;
$check = function ($label, $ok) use (&$failed) { if (!$ok) { $failed++; echo "FAIL $label\\n"; } };
$check('ordering items before crafts', array_search('items', $order, true) < array_search('crafts', $order, true));
$check('ordering worlds before crafts', array_search('worlds', $order, true) < array_search('crafts', $order, true));
$check('idMap complete', count($idMap) === 5 && !array_intersect(array_values($idMap), [$I,$W,$B,$Q,$C]));
$check('no temp id survives in stored rows', preg_match('/200\\d{7}/', $stored) !== 1);
$craft = $rows['crafts:' . $idMap[$C]];
$check('craft scalar FK', $craft['ItemID'] === $idMap[$I] && $craft['WorldID'] === $idMap[$W]);
$check('craft pair list', $craft['RequiredItems'] === '[' . $idMap[$I] . '/2],[17/3]');
$quest = $rows['quests:' . $idMap[$Q]];
$check('self reference deferred to real id', $quest['NextQuestID'] === $idMap[$Q]);
$check('self reference written after insert', in_array('quests:' . $idMap[$Q], $updates, true));
$bot = $rows['bots_info:' . $idMap[$B]];
$check('bot FK and csv modules', $bot['SlotHammer'] === $idMap[$I] && $bot['EquippedModules'] === '17,' . $idMap[$I]);
$npc = json_decode($rows['bots_npc:4']['DialogData'], true);
$check('dialog speaker id', $npc[0]['left_speaker_id'] === $idMap[$B]);
$check('dialog tokens', $npc[0]['text'] === 'Take <item_' . $idMap[$I] . '> in <world_' . $idMap[$W] . '>');
$itemData = json_decode($rows['items:12']['Data'], true);
$check('item json list', $itemData['random_box'][0]['item_id'] === $idMap[$I] && $itemData['random_box'][0]['value'] === 3);
$check('item json world', $itemData['use_scenario']['world_id'] === $idMap[$W] && $itemData['use_scenario']['duration'] === 5);
$check('collecting properties', $rows['nodes_collecting:3']['Properties'] === '[' . $idMap[$I] . '/2/50]');
echo $failed === 0 ? "apply remapping: ok\\n" : "apply remapping: {$failed} failed\\n";
`);
  if (!output.includes('apply remapping: ok')) errors.push(output.trim() || 'apply remapping failed');
  process.stdout.write(output);
}

// ── Гостевой черновик: мир 0 допустим, остальные разделы — нет ──────────────
{
  const { output } = await run(`<?php
require_once '${VFS_ROOT}/api/change-requests-lib.php';
$world = cr_normalize_ops([['type'=>'update','resource'=>'worlds','id'=>0,'data'=>['Name'=>'Main']]], 10);
$failed = 0;
if (count($world) !== 1 || $world[0]['id'] !== 0) { $failed++; echo "FAIL world 0 update\\n"; }
$worldString = cr_normalize_ops([['type'=>'update','resource'=>'worlds','id'=>'0','data'=>['Name'=>'Main']]], 10);
if (($worldString[0]['id'] ?? null) !== 0) { $failed++; echo "FAIL world '0' string\\n"; }
foreach ([['items', 0], ['items', -3], ['worlds', -1], ['worlds', ''], ['worlds', null], ['worlds', 1.5]] as [$resource, $id]) {
    try {
        cr_normalize_ops([['type'=>'update','resource'=>$resource,'id'=>$id,'data'=>['Name'=>'x']]], 10);
        $failed++; echo "FAIL accepted " . var_export($id, true) . " for $resource\\n";
    } catch (CrInputError) { /* ожидаемый отказ */ }
}
// Новая запись не может быть одновременно создана и изменена отдельной операцией.
try {
    cr_normalize_ops([
        ['type'=>'create','resource'=>'items','tempId'=>2000000001,'data'=>['Name'=>'x']],
        ['type'=>'update','resource'=>'items','id'=>2000000001,'data'=>['Name'=>'y']],
    ], 10);
    $failed++; echo "FAIL temp id accepted as an existing record\\n";
} catch (CrInputError) { /* ожидаемый отказ */ }
// Повторяющийся временный ID отклоняется.
try {
    cr_normalize_ops([
        ['type'=>'create','resource'=>'items','tempId'=>2000000001,'data'=>['Name'=>'x']],
        ['type'=>'create','resource'=>'items','tempId'=>2000000001,'data'=>['Name'=>'y']],
    ], 10);
    $failed++; echo "FAIL duplicate temp id\\n";
} catch (CrInputError) { /* ожидаемый отказ */ }
echo $failed === 0 ? "guest draft ids: ok\\n" : "guest draft ids: {$failed} failed\\n";
`);
  if (!output.includes('guest draft ids: ok')) errors.push(output.trim() || 'guest draft ids failed');
  process.stdout.write(output);
}

// ── Анализ: какие временные ID заменятся, а какие нужно проверить руками ────
{
  const { output } = await run(`<?php
require_once '${VFS_ROOT}/api/change-requests-lib.php';
$I = 2000000001;
$notes = cr_temp_id_notes('crafts', ['ItemID' => $I, 'RequiredItems' => "[$I/2]"], ['ItemID' => 'tw_items_list']);
$failed = 0;
$check = function ($label, $ok) use (&$failed) { if (!$ok) { $failed++; echo "FAIL $label\\n"; } };
$check('scalar FK is not reported twice', count(array_filter($notes, fn($n) => $n['severity'] === 'info')) === 1);
$check('pair list replacement announced', (bool)array_filter($notes, fn($n) => $n['severity'] === 'info' && str_contains($n['text'], 'RequiredItems')));
$text = cr_temp_id_notes('items', ['Name' => "copy of item $I"]);
$check('free text warns', count($text) === 1 && $text[0]['severity'] === 'warning');
$broken = cr_temp_id_notes('items', ['Data' => "{ not json $I"]);
$check('unparsable JSON warns', count($broken) === 1 && $broken[0]['severity'] === 'warning');
$quantity = cr_temp_id_notes('items', ['Data' => json_encode(['random_box' => [['item_id' => 12, 'value' => $I]]])]);
$check('a quantity that looks like a draft ID warns', count($quantity) === 1 && $quantity[0]['severity'] === 'warning');
$clean = cr_temp_id_notes('items', ['Data' => json_encode(['random_box' => [['item_id' => $I, 'value' => 3]]])]);
$check('typed JSON reference only informs', count($clean) === 1 && $clean[0]['severity'] === 'info');
echo $failed === 0 ? "temp id analysis: ok\\n" : "temp id analysis: {$failed} failed\\n";
`);
  if (!output.includes('temp id analysis: ok')) errors.push(output.trim() || 'temp id analysis failed');
  process.stdout.write(output);
}

// ── Пароль БД: страница настроек не должна его стирать ──────────────────────
// Берём настоящие обработчики из api/db.php и подменяем только хранилище:
// при открытии настроек клиент шлёт конфиг без password и clear_password.
const dbSource = fs.readFileSync(path.join(ROOT, 'api', 'db.php'), 'utf8');
const configHandlers = dbSource.slice(
  dbSource.indexOf('function handle_get_config'),
  dbSource.indexOf('function handle_test('),
);
if (!configHandlers.includes('handle_save_config')) {
  errors.push('api/db.php: не удалось выделить обработчики конфига');
} else {
  const testPhp = `<?php
error_reporting(E_ALL);
set_error_handler(function ($severity, $message) { throw new ErrorException($message); });
define('EDITOR_ROLE_ADMIN', 'admin');
define('EDITOR_ROLE_GUEST', 'guest');
class EditorUserError extends Exception {}
class ConfigResponded extends Exception {
    public $data;
    function __construct($d) { $this->data = $d; }
}
$cfg = ['host' => 'db', 'port' => 3306, 'database' => 'game', 'user' => 'root',
        'password' => 'secret', 'editor_password' => 'x', 'skins_api' => ''];
$body = [];
$ROLE = EDITOR_ROLE_ADMIN;
function load_cfg(bool $withGuests = true): array { return $GLOBALS['cfg']; }
function read_json_body(): array { return $GLOBALS['body']; }
function mutate_cfg(callable $fn): bool {
    $next = $fn($GLOBALS['cfg']);
    if (!is_array($next)) return false;
    $GLOBALS['cfg'] = $next;
    return true;
}
function respond($data, int $code = 200): never { throw new ConfigResponded($data); }
${configHandlers}
function call_config(string $fn, array $body) {
    $GLOBALS['body'] = $body;
    try { $fn(); } catch (ConfigResponded $e) { return $e->data; }
    throw new Exception('no response');
}
$checks = [];
// Пустой запрос (открытие настроек) — пароль остаётся, предупреждений PHP нет.
$r = call_config('handle_save_config', ['host' => 'db', 'port' => 3306, 'database' => 'game', 'user' => 'root']);
$checks[] = ($GLOBALS['cfg']['password'] === 'secret' && ($r['password_set'] ?? null) === true) ? 'keep' : 'FAIL keep';
// Явная очистка чекбоксом «Без пароля».
$r = call_config('handle_save_config', ['clear_password' => true]);
$checks[] = ($GLOBALS['cfg']['password'] === '' && ($r['password_set'] ?? null) === false) ? 'clear' : 'FAIL clear';
// Новый пароль из формы.
$r = call_config('handle_save_config', ['password' => 'new-secret']);
$checks[] = ($GLOBALS['cfg']['password'] === 'new-secret' && ($r['password_set'] ?? null) === true) ? 'set' : 'FAIL set';
// Ответ API не содержит сам пароль.
$r = call_config('handle_get_config', []);
$checks[] = (!array_key_exists('password', $r['config']) && $r['config']['password_set'] === true) ? 'admin' : 'FAIL admin';
$ROLE = EDITOR_ROLE_GUEST;
$r = call_config('handle_get_config', []);
$checks[] = (array_keys($r['config']) === ['skins_api']) ? 'guest' : 'FAIL guest';
$failures = array_values(array_filter($checks, function ($c) { return substr($c, 0, 4) === 'FAIL'; }));
echo $failures ? ('password: ' . implode(', ', $failures)) : 'password: ok';
`;
  // Отдельный рантайм: в общем уже подключены db-core.php со своими load_cfg(),
  // read_json_body(), respond() — заглушки теста не должны с ними конфликтовать.
  const configPhp = new PhpNode();
  let pwdOutput = '';
  const onOutput = (event) => { pwdOutput += event.detail.join(''); };
  configPhp.addEventListener('output', onOutput);
  await configPhp.writeFile('/config-password-test.php', testPhp, { encoding: 'utf8' });
  await configPhp.run(`<?php require '/config-password-test.php';`);
  configPhp.removeEventListener('output', onOutput);
  if (!pwdOutput.includes('password: ok')) errors.push(pwdOutput.trim() || 'password: нет вывода');
  process.stdout.write(`${pwdOutput}\n`);
}

// Collecting nodes: the same validator is used by CRUD, approvals and sync.
{
  const { output } = await run(`<?php
require_once '${VFS_ROOT}/api/db-resources.php';
$node = ['SwitchID'=>1, 'Level'=>1, 'Health'=>3, 'Name'=>'Stone / Coal', 'WorldID'=>0, 'Type'=>'ore',
    'Properties'=>'[173/80.0],[50/20.0],[172/0.0001]'];
validate_collecting_node($node);
// LONGTEXT must not retain the old 65535-byte TEXT restriction.
$entries = [];
for ($i = 1; $i <= 7000; $i++) $entries[] = '[' . $i . '/0.0001]';
validate_collecting_node(array_replace($node, ['Properties'=>implode(',', $entries)]));
validate_collecting_node(array_replace($node, ['Properties'=>'[173/3/80],[50/1/20]', 'SwitchID'=>255]));
$bad = [
    ['SwitchID'=>0], ['SwitchID'=>256], ['SwitchID'=>1.5], ['WorldID'=>-1], ['Level'=>0], ['Health'=>0],
    ['Name'=>''], ['Type'=>'ore,plant'], ['Type'=>'wood'], ['Properties'=>''], ['Properties'=>'[173/0]'],
    ['Properties'=>'[173/101]'], ['Properties'=>'[173/0/10]'], ['Properties'=>'[173/1.5/10]'],
    ['Properties'=>'[173/NaN]'], ['Properties'=>'[173/80],[173/20]'], ['Properties'=>'[173/80],'],
    ['Properties'=>'[173/80]garbage'], ['Properties'=>'[0/100]']
];
foreach ($bad as $patch) {
    try { validate_collecting_node(array_replace($node, $patch)); echo 'accepted invalid node'; return; }
    catch (RuntimeException $e) { }
}
$r = editor_resources()['nodes_collecting'];
if ($r['table'] !== 'tw_nodes_collecting' || !$r['guest'] || !in_array('Properties', $r['columns'], true)) {
    echo 'invalid collecting resource registry'; return;
}
echo 'collecting validation: ok';`);
  if (!output.includes('collecting validation: ok')) errors.push(output.trim() || 'collecting validation failed');
  console.log(output.trim());
}


// ── db-core security helpers: client IP, password policy, guest limits ─────
{
  const { output } = await run(`<?php
require_once '${VFS_ROOT}/api/db-core.php';
$checks = [];
// IP: заголовки прокси верят только доверенным соседям.
$checks['ip-untrusted'] = editor_resolve_client_ip('203.0.113.10', '1.2.3.4', '', []) === '203.0.113.10';
$checks['ip-chain'] = editor_resolve_client_ip('10.0.0.2', '198.51.100.7, 10.0.0.3', '', ['10.0.0.0/8']) === '198.51.100.7';
$checks['ip-all-trusted'] = editor_resolve_client_ip('10.0.0.2', '10.0.0.9, 10.0.0.3', '', ['10.0.0.0/8']) === '10.0.0.9';
$checks['ip-real'] = editor_resolve_client_ip('10.0.0.2', '', '198.51.100.8', ['10.0.0.2']) === '198.51.100.8';
$checks['ip-spoof'] = editor_resolve_client_ip('203.0.113.10', '198.51.100.7', '', ['10.0.0.0/8']) === '203.0.113.10';
$checks['ip-invalid-remote'] = editor_resolve_client_ip('junk', '1.2.3.4', '', []) === '';
$checks['ip-junk-in-chain'] = editor_resolve_client_ip('10.0.0.2', 'junk, 198.51.100.7', '', ['10.0.0.0/8']) === '198.51.100.7';
// Политика паролей: минимум символов, максимум 72 байта (bcrypt), валидный UTF-8.
$checks['pw-empty'] = editor_password_policy_error('') !== null;
$checks['pw-short'] = editor_password_policy_error('short') !== null;
$checks['pw-ok'] = editor_password_policy_error('1234567890') === null;
$checks['pw-min-arg'] = editor_password_policy_error('123456', 6) === null;
$checks['pw-72'] = editor_password_policy_error(str_repeat('a', 72)) === null;
$checks['pw-73'] = editor_password_policy_error(str_repeat('a', 73)) !== null;
$checks['pw-mb-ok'] = editor_password_policy_error(str_repeat('ж', 10)) === null;
$checks['pw-mb-long'] = editor_password_policy_error(str_repeat('ж', 40)) !== null;
$checks['pw-bad-utf8'] = editor_password_policy_error(chr(0xC3) . chr(0x28) . 'abcdefgh') !== null;
// Лимиты гостя: глобальные правила + индивидуальные переопределения.
$cfg = ['guest_users' => [], 'guest_max_pending' => 7, 'guest_submit_cooldown' => 30, 'guest_max_operations' => 55];
$g = guest_effective_limits(null, $cfg);
$checks['lim-global'] = $g === ['max_pending' => 7, 'cooldown' => 30, 'max_operations' => 55, 'custom' => false];
$o = guest_effective_limits(['limits' => ['max_pending' => 2]], $cfg);
$checks['lim-override-one'] = $o['max_pending'] === 2 && $o['cooldown'] === 30 && $o['custom'] === true;
$j = guest_effective_limits(['limits' => '{"cooldown":0}'], $cfg);
$checks['lim-json'] = $j['cooldown'] === 0 && $j['custom'] === true;
$c = guest_effective_limits(['limits' => ['max_pending' => 999, 'max_operations' => 0]], $cfg);
$checks['lim-clamp'] = $c['max_pending'] === 50 && $c['max_operations'] === 1;
$e = guest_effective_limits(['limits' => []], $cfg);
$checks['lim-empty'] = $e['custom'] === false;
$failed = array_keys(array_filter($checks, fn($v) => !$v));
echo $failed ? ('db-core security helpers failed: ' . implode(', ', $failed)) : 'db-core security helpers: ok';`);
  if (!output.includes('db-core security helpers: ok')) errors.push(output.trim() || 'db-core security helpers failed');
  console.log(output.trim());
}

if (errors.length) {
  console.log(`\n${errors.length} problem(s):`);
  errors.forEach((line) => console.log(' - ' + line));
  process.exit(1);
}
console.log('php checks passed');
