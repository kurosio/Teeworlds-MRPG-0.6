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

if (errors.length) {
  console.log(`\n${errors.length} problem(s):`);
  errors.forEach((line) => console.log(' - ' + line));
  process.exit(1);
}
console.log('php checks passed');
