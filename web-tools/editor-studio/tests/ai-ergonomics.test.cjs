'use strict';

/**
 * tests/ai-ergonomics.test.cjs — End-to-End verification of AI DX / Ergonomics
 * improvements across REST (api/ai.php, /llms.txt, /ai), MCP v2.1.0 (api/mcp.php,
 * tools/mcp-stdio.mjs), OpenAPI 3.1.0, and Admin UI (guests-editor.html).
 *
 * Covers:
 *  1. Static parity (OpenAPI, router.php, apache-config.conf, ai/index.php, guests-editor.html)
 *  2. Public /llms.txt & api/ai.php?action=llms_txt (zero secret/path leaks)
 *  3. Global cross-resource search (REST action=search + MCP search_editor) + ACL isolation
 *  4. 360-degree entity context (REST action=context + MCP get_context + mrpg://context/{res}/{id})
 *  5. Rich filter operators (eq/ne/gt/gte/lt/lte/like/in/is_null/not_null), sorting & did_you_mean
 *  6. Batch record retrieval (REST ids=... + MCP get_record ids:[...])
 *  7. Symbolic ref / $alias chaining across operations (REST + MCP + error handling)
 *  8. Self-healing validation errors (did_you_mean, allowed, expected_type, hint)
 *  9. MCP v2.1.0 protocol negotiation, tool annotations, resources/templates/list, cookbook & 5 prompts
 * 10. End-to-end Change Request lifecycle via AI REST & MCP manage_change_request (with ACL & scope checks)
 * 11. Real child process tools/mcp-stdio.mjs --check (success & failure) + --timeout -32002 handling
 */

const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { spawn } = require('node:child_process');

const ROOT = path.join(__dirname, '..');
const read = (rel) => fs.readFileSync(path.join(ROOT, rel), 'utf8');

let loadNodeRuntime = null;
let PHP = null;
try {
  ({ loadNodeRuntime } = require('@php-wasm/node'));
  ({ PHP } = require('@php-wasm/universal'));
} catch {
  // Handled via skip below if optional php-wasm isn't installed.
}

const TEST_HARNESS_PHP = `
if (!class_exists('TestMysqliResult')) {
class TestMysqliResult extends mysqli_result {
    private array $rows;
    private int $pos = 0;
    public function __construct(array $rows) { $this->rows = array_values($rows); }
    public function fetch_assoc(): ?array { return $this->rows[$this->pos++] ?? null; }
    public function fetch_row(): ?array {
        $r = $this->fetch_assoc();
        return $r === null ? null : array_values($r);
    }
    public function fetch_all(int $mode = MYSQLI_NUM): array {
        return $mode === MYSQLI_ASSOC ? $this->rows : array_map('array_values', $this->rows);
    }
    public function free(): void {}
}

class TestMysqliStmt extends mysqli_stmt {
    private array $params = [];
    private array $rows = [];
    private int $affected = 0;
    public function __construct(private PDO $pdo, private string $sql) {}
    public function bind_param(string $types, mixed &...$vars): bool { $this->params = $vars; return true; }
    public function execute(?array $params = null): bool {
        $bind = $params ?? $this->params;
        $s = str_replace(chr(96), '"', $this->sql);
        if (preg_match('/information_schema\\.tables/i', $s)) {
            $tbl = (string)($bind[0] ?? '');
            $st = $this->pdo->prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name = ? LIMIT 1");
            $st->execute([$tbl]);
            $this->rows = $st->fetchAll(PDO::FETCH_ASSOC);
            $this->affected = 0;
            return true;
        }
        $s = preg_replace('/CAST\\(([^\\s)]+)\\s+AS\\s+CHAR\\)/i', 'CAST($1 AS TEXT)', $s);
        $s = preg_replace('/\\s+FOR\\s+UPDATE\\s*$/i', '', $s);
        if (preg_match('/^\\s*(UPDATE|DELETE)\\b/i', $s)) {
            $s = preg_replace('/\\s+LIMIT\\s+\\d+\\s*$/i', '', $s);
        }
        $st = $this->pdo->prepare($s);
        $st->execute(array_map(static fn($v) => $v, $bind));
        $this->affected = $st->rowCount();
        $this->rows = preg_match('/^\\s*SELECT\\b/i', $s) ? $st->fetchAll(PDO::FETCH_ASSOC) : [];
        return true;
    }
    public function get_result(): TestMysqliResult|false { return new TestMysqliResult($this->rows); }
    public function store_result(): bool { return true; }
    public function testNumRows(): int { return count($this->rows); }
    public function testAffectedRows(): int { return $this->affected; }
    public function close(): true { return true; }
}

class TestMysqli extends mysqli {
    public function __construct(public PDO $pdo) {}
    public function testInsertId(): int { return (int)$this->pdo->lastInsertId(); }
    public function prepare(string $query): mysqli_stmt|false { return new TestMysqliStmt($this->pdo, $query); }
    public function query(string $query, int $result_mode = MYSQLI_STORE_RESULT): TestMysqliResult|bool {
        $s = str_replace(chr(96), '"', $query);
        $s = preg_replace('/CAST\\(([^\\s)]+)\\s+AS\\s+CHAR\\)/i', 'CAST($1 AS TEXT)', $s);
        $st = $this->pdo->query($s);
        if ($st === false) return false;
        if (preg_match('/^\\s*SELECT\\b/i', $s)) {
            return new TestMysqliResult($st->fetchAll(PDO::FETCH_ASSOC));
        }
        return true;
    }
    public function begin_transaction(int $flags = 0, ?string $name = null): bool { return $this->pdo->beginTransaction(); }
    public function commit(int $flags = 0, ?string $name = null): bool { return $this->pdo->commit(); }
    public function rollback(int $flags = 0, ?string $name = null): bool { return $this->pdo->inTransaction() ? $this->pdo->rollBack() : true; }
    public function close(): true { return true; }
}

function create_ergonomics_game_pdo(string $dsn = 'sqlite:/studio/data/game.sqlite'): PDO {
    $pdo = new PDO($dsn);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $ddls = [
        'CREATE TABLE IF NOT EXISTS tw_worlds (ID INTEGER PRIMARY KEY, Name TEXT, Path TEXT, Type TEXT, Flags TEXT, RespawnWorldID INTEGER, JailWorldID INTEGER, RequiredLevel INTEGER)',
        "CREATE TABLE IF NOT EXISTS tw_items_list (Comment TEXT, ID INTEGER PRIMARY KEY, Name TEXT, Description TEXT, \\"Group\\" TEXT, Type TEXT, Flags TEXT, ScenarioData TEXT, ScenarioMode TEXT NOT NULL DEFAULT 'universal', InitialPrice INTEGER, RequiresProducts INTEGER, AT1 INTEGER, AT2 INTEGER, ATValue1 INTEGER, ATValue2 INTEGER, Data TEXT)",
        'CREATE TABLE IF NOT EXISTS tw_quests_list (ID INTEGER PRIMARY KEY, NextQuestID INTEGER, Name TEXT, Money INTEGER, Exp INTEGER, Flags TEXT)',
        'CREATE TABLE IF NOT EXISTS tw_quest_boards (ID INTEGER PRIMARY KEY, Name TEXT, PosX INTEGER, PosY INTEGER, WorldID INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_quests_board_list (ID INTEGER PRIMARY KEY, QuestID INTEGER, DailyBoardID INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_bots_info (ID INTEGER PRIMARY KEY, Name TEXT, JsonTeeInfo TEXT, EquippedModules TEXT, SlotHammer INTEGER, SlotGun INTEGER, SlotShotgun INTEGER, SlotGrenade INTEGER, SlotRifle INTEGER, SlotArmor INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_bots_npc (ID INTEGER PRIMARY KEY, BotID INTEGER, PosX INTEGER, PosY INTEGER, GiveQuestID INTEGER, DialogData TEXT, Function INTEGER, Static INTEGER, Emote TEXT, WorldID INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_bots_mobs (ID INTEGER PRIMARY KEY, BotID INTEGER, WorldID INTEGER, PositionX INTEGER, PositionY INTEGER, Debuffs TEXT, Behavior TEXT, Level INTEGER, Power INTEGER, Number INTEGER, Respawn INTEGER, Radius INTEGER, ActiveRadius INTEGER, WanderRadius INTEGER, Boss INTEGER, it_drop_0 INTEGER, it_drop_1 INTEGER, it_drop_2 INTEGER, it_drop_3 INTEGER, it_drop_4 INTEGER, it_drop_count TEXT, it_drop_chance TEXT)',
        'CREATE TABLE IF NOT EXISTS tw_bots_quest (ID INTEGER PRIMARY KEY, BotID INTEGER, QuestID INTEGER, Step INTEGER, WorldID INTEGER, PosX INTEGER, PosY INTEGER, AutoFinish TEXT, DialogData TEXT, ScenarioData TEXT, TasksData TEXT)',
        'CREATE TABLE IF NOT EXISTS tw_crafts_list (ID INTEGER PRIMARY KEY, GroupName TEXT, ItemID INTEGER, ItemValue INTEGER, RequiredItems TEXT, Price INTEGER, WorldID INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_vouchers (ID INTEGER PRIMARY KEY, Code TEXT, ValidUntil TEXT, Data TEXT, Multiple INTEGER)',
    ];
    foreach ($ddls as $sql) $pdo->exec($sql);

    $pdo->exec("INSERT OR REPLACE INTO tw_worlds (ID, Name, Path, Type, Flags, RespawnWorldID, JailWorldID, RequiredLevel) VALUES (1, 'Sunspire Citadel', 'main.map', 'Default', '', 1, 1, 1)");
    $pdo->exec("INSERT OR REPLACE INTO tw_items_list (ID, Name, Description, \\"Group\\", Type, Flags, InitialPrice, Data) VALUES
        (10, 'Sunfire Blade', 'Forged in solar steel', 'Equipment', 'Equip hammer', '', 250, '{\\"bonus\\":{\\"strength\\":6}}'),
        (11, 'Frost Shard', 'Cold crystal fragment', 'Resource', 'Resource mineable', '', 80, '{}'),
        (12, 'Golden Coin', NULL, 'Currency', 'Default', '', 1, '{}'),
        (13, 'Citadel Blade', 'Guard standard blade', 'Equipment', 'Equip hammer', '', 420, '{\\"bonus\\":{\\"strength\\":9}}')
    ");
    $pdo->exec("INSERT OR REPLACE INTO tw_quests_list (ID, NextQuestID, Name, Money, Exp, Flags) VALUES (20, 0, 'Sunfire Awakening', 500, 1200, 'Type main')");
    $pdo->exec("INSERT OR REPLACE INTO tw_bots_info (ID, Name) VALUES (30, 'Captain Solis'), (31, 'Sunfire Golem')");
    $pdo->exec("INSERT OR REPLACE INTO tw_bots_npc (ID, BotID, PosX, PosY, GiveQuestID, DialogData, Function, Static, Emote, WorldID) VALUES (40, 30, 1024, 512, 20, '[]', 0, 1, 'Normal', 1)");
    $pdo->exec("INSERT OR REPLACE INTO tw_bots_quest (ID, BotID, QuestID, Step, WorldID, PosX, PosY, AutoFinish, DialogData, ScenarioData, TasksData) VALUES (50, 30, 20, 1, 1, 1056, 512, '', '[]', '{}', '[]')");
    $pdo->exec("INSERT OR REPLACE INTO tw_crafts_list (ID, GroupName, ItemID, ItemValue, RequiredItems, Price, WorldID) VALUES (60, 'Weapons', 10, 1, '[{\\"id\\":11,\\"value\\":3}]', 150, 1)");
    $pdo->exec("INSERT OR REPLACE INTO tw_vouchers (ID, Code, ValidUntil, Data, Multiple) VALUES (99, 'TOP-SECRET-VOUCHER', '2099-01-01', '{}', 0)");
    return $pdo;
}
}
`;

async function bootStudioPhp() {
  const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(runtime);
  php.mkdir('/studio');
  php.mkdir('/studio/api');
  php.mkdir('/studio/ai');
  php.mkdir('/studio/data');
  php.mkdir('/studio/editor-core');
  php.mkdir('/studio/scenarios');
  php.mkdir('/studio/maps');

  for (const f of fs.readdirSync(path.join(ROOT, 'api'))) {
    const full = path.join(ROOT, 'api', f);
    if (!fs.statSync(full).isFile()) continue;
    if (!f.endsWith('.php')) {
      php.writeFile(`/studio/api/${f}`, fs.readFileSync(full));
      continue;
    }
    let src = read(`api/${f}`);
    if (f === 'db-core.php') {
      src = src.replace(
        'function db_connect(): mysqli\n{',
        'function db_connect(): mysqli\n{\n    if (isset($GLOBALS[\'__TEST_MYSQLI\']) && $GLOBALS[\'__TEST_MYSQLI\'] instanceof mysqli) { return $GLOBALS[\'__TEST_MYSQLI\']; }'
      );
    }
    if (f === 'db-resources.php') {
      src = src.replaceAll('(int)$mysqli->insert_id', '($mysqli instanceof TestMysqli ? $mysqli->testInsertId() : (int)$mysqli->insert_id)');
      src = src.replaceAll('$stmt->affected_rows', '($stmt instanceof TestMysqliStmt ? $stmt->testAffectedRows() : $stmt->affected_rows)');
      src = src.replaceAll('$stmt->num_rows', '($stmt instanceof TestMysqliStmt ? $stmt->testNumRows() : $stmt->num_rows)');
    }
    php.writeFile(`/studio/api/${f}`, src);
  }
  php.writeFile('/studio/ai/index.php', read('ai/index.php'));
  php.writeFile('/studio/router.php', read('router.php'));
  php.writeFile('/studio/editor-core/reference-schema.json', read('editor-core/reference-schema.json'));

  php.writeFile('/studio/scenarios/sunfire_intro.json', JSON.stringify({
    scenario_mode: 'default',
    steps: [
      {
        id: 'start',
        msg: [{ text: 'Seek the Sunfire Blade in the Citadel!', delay: 0 }],
        components: [
          { type: 'player_give_item', item_id: 10, value: 1 },
          { type: 'quest_action', action: 'accept', quest_id: 20 },
        ],
      },
    ],
  }, null, 2));

  const init = await php.run({
    code: `<?php
      ${TEST_HARNESS_PHP}
      putenv('EDITOR_STUDIO_SQLITE=/studio/data/studio.sqlite');
      create_ergonomics_game_pdo();
      require '/studio/api/store.php';
      store_settings_save([
          'host' => 'mock', 'port' => 3306, 'database' => 'mrpg', 'user' => 'root', 'password' => '',
          'editor_password' => password_hash('adminpass1234', PASSWORD_BCRYPT),
          'guest_enabled' => true, 'guest_allow_remote' => true,
          'guest_max_pending' => 20, 'guest_submit_cooldown' => 0, 'guest_max_operations' => 50,
          'maps_dir' => '/studio/maps', 'mapres_dir' => '/studio/maps',
          'map_api' => '', 'map_api_token' => 'SUPER_SECRET_MAP_API_TOKEN',
      ]);
      store_guest_user_insert([
          'id' => 'g0000020', 'login' => 'ergo_guest', 'name' => 'Ergo AI', 'enabled' => true,
          'pages' => ['items', 'quests', 'bots_info', 'bots_npc', 'bots_mobs', 'crafts', 'worlds', 'scenarios'],
          'passwordHash' => password_hash('guestpass1234', PASSWORD_BCRYPT), 'sessionVersion' => 1,
          'createdAt' => gmdate('c'), 'updatedAt' => gmdate('c'), 'lastLoginAt' => '',
          'limits' => ['max_pending' => 20, 'cooldown' => 0, 'max_operations' => 50],
          'expiresAt' => '',
      ]);
      store_guest_user_insert([
          'id' => 'g0000021', 'login' => 'other_guest', 'name' => 'Other AI', 'enabled' => true,
          'pages' => ['items'],
          'passwordHash' => password_hash('guestpass5678', PASSWORD_BCRYPT), 'sessionVersion' => 1,
          'createdAt' => gmdate('c'), 'updatedAt' => gmdate('c'), 'lastLoginAt' => '',
          'limits' => ['max_pending' => 5, 'cooldown' => 0, 'max_operations' => 10],
          'expiresAt' => '',
      ]);
      echo 'OK';
    `,
  });
  assert.equal(init.text.trim(), 'OK', `init failed: ${init.errors}`);
  return php;
}

async function execRoute(php, {
  urlPath = '/api/ai.php',
  method = 'GET',
  query = {},
  body = null,
  headers = {},
  role = null,
} = {}) {
  const qs = new URLSearchParams(query).toString();
  const fullUri = qs ? `${urlPath}?${qs}` : urlPath;
  const qHex = Buffer.from(JSON.stringify(query), 'utf8').toString('hex');
  const bHex = Buffer.from(body !== null ? JSON.stringify(body) : '', 'utf8').toString('hex');
  const hHex = Buffer.from(JSON.stringify(headers), 'utf8').toString('hex');
  const validCsrf = 'c'.repeat(64);

  const script = `<?php
    ${TEST_HARNESS_PHP}
    putenv('EDITOR_STUDIO_SQLITE=/studio/data/studio.sqlite');
    $GLOBALS['__TEST_GAME_PDO'] ??= new PDO('sqlite:/studio/data/game.sqlite');
    $GLOBALS['__TEST_GAME_PDO']->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $GLOBALS['__TEST_MYSQLI'] = new TestMysqli($GLOBALS['__TEST_GAME_PDO']);

    $_SERVER['REQUEST_METHOD'] = ${JSON.stringify(method)};
    $_SERVER['REQUEST_URI'] = ${JSON.stringify(fullUri)};
    $_SERVER['REMOTE_ADDR'] = '127.0.0.1';
    $_SERVER['HTTP_HOST'] = 'studio.test';
    $_SERVER['HTTP_ORIGIN'] = 'http://studio.test';
    $_SERVER['CONTENT_TYPE'] = ${body !== null ? "'application/json'" : "''"};
    $_SERVER['HTTP_X_EDITOR_CSRF'] = ${JSON.stringify(validCsrf)};
    foreach (array_keys($_SERVER) as $k) {
        if (str_starts_with($k, 'HTTP_AUTHORIZATION') || str_starts_with($k, 'HTTP_ACCEPT')) {
            unset($_SERVER[$k]);
        }
    }
    $hdrs = json_decode(hex2bin('${hHex}'), true) ?: [];
    foreach ($hdrs as $hk => $hv) {
        $normKey = 'HTTP_' . strtoupper(str_replace('-', '_', (string)$hk));
        $_SERVER[$normKey] = (string)$hv;
    }
    $_GET = json_decode(hex2bin('${qHex}'), true) ?: [];
    $_SESSION = [];
    if (${JSON.stringify(role)} === 'admin') {
        if (session_status() !== PHP_SESSION_ACTIVE) @session_start();
        $_SESSION['editor_studio_csrf'] = str_repeat('c', 64);
        $_SESSION['editor_studio_auth'] = true;
        $_SESSION['editor_studio_user'] = 'admin';
    }
    $rawBody = hex2bin('${bHex}');
    putenv('EDITOR_STUDIO_TEST=1');
    putenv('EDITOR_STUDIO_BODY=' . ($rawBody === false ? '' : $rawBody));
    http_response_code(200);

    $uriPath = (string)parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);
    if ($uriPath === '/llms.txt') {
        $_GET['action'] = 'llms_txt';
        $_SERVER['SCRIPT_NAME'] = '/api/ai.php';
        require '/studio/api/ai.php';
    } elseif ($uriPath === '/mcp') {
        $_SERVER['SCRIPT_NAME'] = '/api/mcp.php';
        require '/studio/api/mcp.php';
    } elseif ($uriPath === '/ai' || $uriPath === '/ai/') {
        $_SERVER['SCRIPT_NAME'] = '/ai/index.php';
        require '/studio/ai/index.php';
    } elseif (str_starts_with($uriPath, '/api/')) {
        $_SERVER['SCRIPT_NAME'] = $uriPath;
        require '/studio' . $uriPath;
    } else {
        http_response_code(404);
        echo json_encode(['ok' => false, 'error' => 'Not found']);
    }
  `;

  const res = await php.run({ code: script });
  let json = null;
  try { json = JSON.parse(res.text); } catch { /* text/markdown or html */ }
  return { status: res.httpStatusCode || 200, text: res.text, json, errors: res.errors };
}

test('статическая согласованность AI ergonomics: OpenAPI, router/apache, /ai и guests-editor.html', () => {
  const openapi = JSON.parse(read('api/ai-openapi.json'));
  assert.ok(openapi.paths['/llms.txt'], 'OpenAPI должен описывать /llms.txt');
  const getActions = openapi.paths['/api/ai.php'].get.parameters.find((p) => p.name === 'action').schema.enum;
  const postActions = openapi.paths['/api/ai.php'].post.parameters.find((p) => p.name === 'action').schema.enum;
  for (const act of ['llms_txt', 'search', 'context', 'request']) {
    assert.ok(getActions.includes(act), `OpenAPI GET action должен включать ${act}`);
  }
  for (const act of ['request_reply', 'request_withdraw']) {
    assert.ok(postActions.includes(act), `OpenAPI POST action должен включать ${act}`);
  }
  assert.ok(openapi.components.schemas.DbOperation.properties.ref, 'DbOperation должен описывать символическую ссылку ref');
  assert.ok(openapi.components.schemas.RequestReplyRequest);
  assert.ok(openapi.components.schemas.RequestWithdrawRequest);

  const router = read('router.php');
  const apache = read('apache-config.conf');
  assert.ok(router.includes('llms\\.txt') && router.includes('llms_txt'), 'router.php должен маршрутизировать /llms.txt');
  assert.ok(apache.includes('llms\\.txt') && apache.includes('action=llms_txt'), 'apache-config.conf должен маршрутизировать /llms.txt');

  const aiIndex = read('ai/index.php');
  assert.ok(!aiIndex.includes('stdioFallback'), 'ai/index.php не должен содержать невалидное поле stdioFallback в MCP JSON');
  assert.ok(aiIndex.includes('llms_txt'), 'ai/index.php должен ссылаться на llms.txt');
  for (const tab of ['mcp-http', 'mcp-stdio', 'chat-prompt', 'curl']) {
    assert.ok(aiIndex.includes(`data-tab="${tab}"`), `ai/index.php должен содержать вкладку ${tab}`);
  }

  const guestsHtml = read('guests-editor.html');
  for (const preset of ['readonly', 'author', 'full']) {
    assert.ok(guestsHtml.includes(`data-preset="${preset}"`), `guests-editor.html должен содержать пресет ${preset}`);
  }
  assert.ok(guestsHtml.includes('id="t-pages"'), 'guests-editor.html должен поддерживать сужение вкладок токена');
  for (const snippet of ['mcp-http', 'mcp-stdio', 'prompt']) {
    assert.ok(guestsHtml.includes(`data-snippet="${snippet}"`), `guests-editor.html должен показывать конфиг ${snippet}`);
  }
});

test('End-to-End AI Ergonomics: llms.txt, search, 360° context, filters/sort, batch get, $ref, self-healing hints, MCP v2.1 & CR lifecycle', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const php = await bootStudioPhp();
  try {

  // Issue 3 tokens: full guest token, read-only token, and other guest token
  const fullTokRes = await execRoute(php, {
    urlPath: '/api/guests.php', method: 'POST', query: { action: 'token_create' }, role: 'admin',
    body: { guest_id: 'g0000020', name: 'Ergo Full', scopes: ['read', 'create', 'update', 'delete'] },
  });
  assert.equal(fullTokRes.status, 200, fullTokRes.text);
  const fullHeaders = { Authorization: `Bearer ${fullTokRes.json.secret}` };

  const roTokRes = await execRoute(php, {
    urlPath: '/api/guests.php', method: 'POST', query: { action: 'token_create' }, role: 'admin',
    body: { guest_id: 'g0000020', name: 'Ergo RO', scopes: ['read'] },
  });
  assert.equal(roTokRes.status, 200, roTokRes.text);
  const roHeaders = { Authorization: `Bearer ${roTokRes.json.secret}` };

  const otherTokRes = await execRoute(php, {
    urlPath: '/api/guests.php', method: 'POST', query: { action: 'token_create' }, role: 'admin',
    body: { guest_id: 'g0000021', name: 'Other Guest', scopes: ['read', 'create', 'update', 'delete'] },
  });
  assert.equal(otherTokRes.status, 200, otherTokRes.text);
  const otherHeaders = { Authorization: `Bearer ${otherTokRes.json.secret}` };

  // ── 1. Public /llms.txt & /ai?format=json (no auth, no secret/path leaks) ──
  const llms = await execRoute(php, { urlPath: '/llms.txt', method: 'GET' });
  assert.equal(llms.status, 200);
  assert.ok(llms.text.includes('Teeworlds-MRPG-0.6 Editor Studio'), 'llms.txt должен содержать заголовок Studio');
  assert.ok(llms.text.includes('search_editor') && llms.text.includes('get_context') && llms.text.includes('manage_change_request'));
  assert.ok(llms.text.includes('$alias') || llms.text.includes('"$'));
  assert.ok(!llms.text.includes('SUPER_SECRET_MAP_API_TOKEN') && !llms.text.includes('/studio/data/'), 'llms.txt не должен раскрывать секреты или пути');

  const aiJson = await execRoute(php, { urlPath: '/ai', method: 'GET', query: { format: 'json' } });
  assert.equal(aiJson.status, 200);
  assert.equal(aiJson.json.mcp.tools.length, 19);
  assert.ok(aiJson.json.urls.llms_txt_api.includes('llms_txt'));
  assert.ok(aiJson.json.mcp.supportedVersions.includes('2025-06-18'));

  // ── 2. Global Cross-Resource Search + ACL Isolation ──
  const searchRes = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: { action: 'search', q: 'Sunfire' },
  });
  assert.equal(searchRes.status, 200, searchRes.text);
  assert.equal(searchRes.json.ok, true);
  assert.ok(searchRes.json.top_matches.length >= 3, `expected hits across items/quests/scenarios, got ${JSON.stringify(searchRes.json)}`);
  const hitResources = Object.keys(searchRes.json.groups || {});
  assert.ok(hitResources.includes('items'));
  assert.ok(hitResources.includes('quests'));
  assert.ok(hitResources.includes('scenarios'));
  assert.ok(!hitResources.includes('vouchers'), 'vouchers никогда не должны попадать в поиск гостя');

  // Searching for a secret voucher code must yield 0 hits
  const secretSearch = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: { action: 'search', q: 'TOP-SECRET-VOUCHER' },
  });
  assert.equal(secretSearch.status, 200);
  assert.equal(secretSearch.json.top_matches.length, 0);

  // Numeric search finds exact primary key matches
  const numSearch = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: { action: 'search', q: '20' },
  });
  assert.equal(numSearch.status, 200);
  assert.ok(numSearch.json.top_matches.some((m) => m.resource === 'quests' && Number(m.id) === 20), JSON.stringify(numSearch.json));

  // ── 3. 360-Degree Entity Context (REST + MCP tool + MCP resource template) ──
  const ctxRes = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: { action: 'context', resource: 'quests', id: '20' },
  });
  assert.equal(ctxRes.status, 200, ctxRes.text);
  assert.equal(Number(ctxRes.json.record.ID), 20);
  assert.equal(ctxRes.json.record.Name, 'Sunfire Awakening');
  assert.ok(Array.isArray(ctxRes.json.steps) && ctxRes.json.steps.length === 1, 'context квеста должен возвращать шаги steps');
  assert.equal(ctxRes.json.steps[0].bot_name, 'Captain Solis', 'steps должен обогащаться именем бота');
  assert.ok(Array.isArray(ctxRes.json.givers) && ctxRes.json.givers.length === 1 && ctxRes.json.givers[0].BotName === 'Captain Solis');
  const inboundScenarios = ctxRes.json.references?.inbound?.scenarios || ctxRes.json.references?.inbound || [];
  assert.ok(JSON.stringify(inboundScenarios).includes('sunfire_intro.json'), JSON.stringify(ctxRes.json.references));

  // Forbidden resource context must fail with 403
  const forbiddenCtx = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: { action: 'context', resource: 'vouchers', id: '99' },
  });
  assert.equal(forbiddenCtx.status, 403);

  // ── 4. Rich Filter Operators, Sorting & did_you_mean ──
  const filterRange = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: {
      action: 'list', resource: 'items',
      filter: JSON.stringify({ InitialPrice: { gte: 100, lte: 500 }, Name: { like: 'Blade' } }),
      sort: '-InitialPrice',
    },
  });
  assert.equal(filterRange.status, 200, filterRange.text);
  assert.deepEqual(filterRange.json.rows.map((r) => Number(r.ID)), [13, 10]);

  const filterNull = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: {
      action: 'list', resource: 'items',
      filter: JSON.stringify({ Description: { is_null: true } }),
    },
  });
  assert.equal(filterNull.status, 200, filterNull.text);
  assert.deepEqual(filterNull.json.rows.map((r) => Number(r.ID)), [12]);

  const filterNotNullNe = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: {
      action: 'list', resource: 'items',
      filter: JSON.stringify({ Description: { not_null: true }, Group: { ne: 'Resource' } }),
      sort: 'Name',
    },
  });
  assert.equal(filterNotNullNe.status, 200, filterNotNullNe.text);
  assert.deepEqual(filterNotNullNe.json.rows.map((r) => r.Name), ['Citadel Blade', 'Sunfire Blade']);

  // Typo in filter column -> did_you_mean hint
  const badFilter = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: { action: 'list', resource: 'items', filter: JSON.stringify({ Nmae: 'Sunfire Blade' }) },
  });
  assert.equal(badFilter.status, 400);
  assert.equal(badFilter.json.code, 'unknown_filter_field');
  assert.ok(Array.isArray(badFilter.json.did_you_mean) && badFilter.json.did_you_mean.includes('Name'));

  // SQL injection in sort -> 400 unknown_sort_field
  const badSort = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: { action: 'list', resource: 'items', sort: 'ID; DROP TABLE tw_items_list--' },
  });
  assert.equal(badSort.status, 400);
  assert.equal(badSort.json.code, 'unknown_sort_field');

  // ── 5. Batch Get (REST ids=... + MCP get_record ids:[...]) ──
  const batchGet = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: fullHeaders,
    query: { action: 'get', resource: 'items', ids: '10,12,999', fields: 'ID,Name' },
  });
  assert.equal(batchGet.status, 200, batchGet.text);
  assert.equal(batchGet.json.batch, true);
  assert.deepEqual(batchGet.json.found_ids, [10, 12]);
  assert.deepEqual(batchGet.json.missing_ids, [999]);
  assert.equal(batchGet.json.rows.length, 2);

  // ── 6. Symbolic ref / $alias Chaining + Self-Healing Validation Hints ──
  const refValidate = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'POST', headers: fullHeaders,
    query: { action: 'validate' },
    body: {
      operations: [
        {
          resource: 'items',
          action: 'create',
          ref: 'moon_key',
          data: { Name: 'Moonlight Key', Description: 'Opens lunar vault', Group: 'Quest', Type: 'Default' },
        },
        {
          resource: 'scenarios',
          action: 'create',
          filename: 'lunar_vault.json',
          data: {
            content: {
              scenario_mode: 'default',
              steps: [
                {
                  id: 'start',
                  msg: [{ text: 'Take this Moonlight Key!', delay: 0 }],
                  components: [{ type: 'player_give_item', item_id: '$moon_key', value: 1 }],
                },
              ],
            },
          },
        },
      ],
    },
  });
  assert.equal(refValidate.status, 200, refValidate.text);
  assert.equal(refValidate.json.valid, true, JSON.stringify(refValidate.json));
  assert.ok(refValidate.json.resolved_refs && Number.isInteger(refValidate.json.resolved_refs.moon_key));
  const assignedTempId = refValidate.json.resolved_refs.moon_key;
  assert.ok(assignedTempId >= 2000000000 && assignedTempId <= 2000999999);
  assert.equal(refValidate.json.normalized_operations[0].tempId, assignedTempId);
  assert.strictEqual(refValidate.json.normalized_operations[1].data.content.steps[0].components[0].item_id, assignedTempId);

  // Unresolved symbolic ref -> clear error code + did_you_mean + available_refs
  const badRef = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'POST', headers: fullHeaders,
    query: { action: 'validate' },
    body: {
      operations: [
        {
          resource: 'items',
          action: 'create',
          ref: 'moon_ore',
          data: { Name: 'Moon Ore', Description: 'Lunar metal', Group: 'Resource', Type: 'Resource mineable' },
        },
        {
          resource: 'crafts',
          action: 'create',
          data: { GroupName: 'Misc', ItemID: '$moon_or', ItemValue: 1, RequiredItems: '[$moon_ore/3]', Price: 10, WorldID: 1 },
        },
      ],
    },
  });
  assert.equal(badRef.status, 200);
  assert.equal(badRef.json.valid, false);
  const badRefErr = badRef.json.errors.find((e) => e.code === 'unresolved_symbolic_ref');
  assert.ok(badRefErr && badRefErr.available_refs.includes('moon_ore') && badRefErr.did_you_mean.includes('$moon_ore'), JSON.stringify(badRef.json.errors));

  // Self-healing DB validation errors: op 0 tests unknown_field typo; op 1 tests invalid_enum_value + invalid_json_field
  const selfHeal = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'POST', headers: fullHeaders,
    query: { action: 'validate' },
    body: {
      operations: [
        {
          resource: 'items',
          action: 'create',
          data: { Nmae: 'Typo Sword' },
        },
        {
          resource: 'items',
          action: 'create',
          data: { Name: 'Broken Item', Group: 'Equipmnt', Type: 'Defualt', Data: '{broken json' },
        },
      ],
    },
  });
  assert.equal(selfHeal.status, 200);
  assert.equal(selfHeal.json.valid, false);
  const errs = selfHeal.json.errors;
  const typoErr = errs.find((e) => e.code === 'unknown_field' && e.field === 'Nmae');
  assert.ok(typoErr && Array.isArray(typoErr.did_you_mean) && typoErr.did_you_mean.includes('Name'), JSON.stringify(errs));
  const enumErr = errs.find((e) => e.code === 'invalid_enum_value' && e.field === 'Group');
  assert.ok(enumErr && Array.isArray(enumErr.allowed) && enumErr.did_you_mean.includes('Equipment'), JSON.stringify(errs));
  const jsonErr = errs.find((e) => e.code === 'invalid_json' && e.field === 'Data');
  assert.ok(jsonErr && typeof jsonErr.hint === 'string' && jsonErr.hint.length > 0, JSON.stringify(errs));

  // ── 7. MCP v2.1.0 Protocol Negotiation, Annotations, Templates, Cookbook & Prompts ──
  const init2025 = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 101, method: 'initialize', params: { protocolVersion: '2025-06-18' } },
  });
  assert.equal(init2025.json.result.protocolVersion, '2025-06-18');
  assert.equal(init2025.json.result.serverInfo.version, '2.1.0');
  assert.ok(init2025.json.result.instructions.includes('search_editor'));

  const initFuture = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 102, method: 'initialize', params: { protocolVersion: '2099-01-01' } },
  });
  assert.equal(initFuture.json.result.protocolVersion, '2025-06-18');

  const toolsList = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 103, method: 'tools/list' },
  });
  assert.equal(toolsList.json.result.tools.length, 19);
  for (const t of toolsList.json.result.tools) {
    assert.ok(t.annotations && typeof t.annotations.readOnlyHint === 'boolean', `tool ${t.name} must have annotations`);
  }
  const proposeTool = toolsList.json.result.tools.find((t) => t.name === 'propose_changes');
  assert.equal(proposeTool.annotations.readOnlyHint, false);
  const searchTool = toolsList.json.result.tools.find((t) => t.name === 'search_editor');
  assert.equal(searchTool.annotations.readOnlyHint, true);

  const resList = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 104, method: 'resources/list' },
  });
  const uris = resList.json.result.resources.map((r) => r.uri);
  assert.ok(uris.includes('mrpg://cookbook') && uris.includes('mrpg://llms.txt'));

  const tplList = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 105, method: 'resources/templates/list' },
  });
  const tpls = tplList.json.result.resourceTemplates.map((t) => t.uriTemplate);
  assert.ok(tpls.includes('mrpg://schema/{resource}') && tpls.includes('mrpg://context/{resource}/{id}'));

  const readCookbook = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 106, method: 'resources/read', params: { uri: 'mrpg://cookbook' } },
  });
  const cookbook = JSON.parse(readCookbook.json.result.contents[0].text);
  assert.ok(cookbook.coordinates && cookbook.symbolic_refs && cookbook.dsl_cheatsheet && cookbook.filtering_and_sorting && cookbook.safe_editing_rules);

  const readCtxUri = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 107, method: 'resources/read', params: { uri: 'mrpg://context/quests/20' } },
  });
  const ctxViaUri = JSON.parse(readCtxUri.json.result.contents[0].text);
  assert.equal(Number(ctxViaUri.record.ID), 20);

  const promptsList = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 108, method: 'prompts/list' },
  });
  assert.equal(promptsList.json.result.prompts.length, 5);

  const promptGet = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 109, method: 'prompts/get', params: { name: 'create_mrpg_quest_scene', arguments: { goal: 'Lunar Eclipse', world_id: '1' } } },
  });
  const promptText = promptGet.json.result.messages[0].content.text;
  assert.ok(promptText.includes('Lunar Eclipse') && promptText.includes('get_context') && promptText.includes('ref'));

  // Verify all 5 prompts render cleanly without PHP warnings
  for (const pName of ['create_mrpg_item', 'create_mrpg_quest_scene', 'safe_json_patch_edit', 'inspect_map_and_place_object', 'review_and_revise_request']) {
    const pRes = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: fullHeaders,
      body: { jsonrpc: '2.0', id: 109, method: 'prompts/get', params: { name: pName, arguments: { concept: 'Test', world_id: '1', goal: 'Test', resource: 'items', id: '10' } } },
    });
    assert.ok(pRes.json?.result?.messages?.[0]?.content?.text?.length > 40, `prompt ${pName} failed: ${pRes.text}`);
  }

  // MCP tool calls: search_editor, get_context, get_record batch
  const mcpSearch = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 110, method: 'tools/call', params: { name: 'search_editor', arguments: { q: 'Sunfire' } } },
  });
  assert.equal(mcpSearch.json.result.isError, false);
  assert.ok(JSON.parse(mcpSearch.json.result.content[0].text).top_matches.length >= 3);

  const mcpBatchGet = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 111, method: 'tools/call', params: { name: 'get_record', arguments: { resource: 'items', ids: [10, 11], fields: ['ID', 'Name'] } } },
  });
  assert.equal(mcpBatchGet.json.result.isError, false);
  assert.deepEqual(JSON.parse(mcpBatchGet.json.result.content[0].text).found_ids, [10, 11]);

  // ── 8. Change Request Lifecycle via AI REST & MCP manage_change_request + ACL ──
  const proposeRes = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'POST', headers: fullHeaders,
    query: { action: 'propose' },
    body: {
      message: 'Add Moonlight Key via symbolic ref',
      operations: [
        { resource: 'items', action: 'create', ref: 'moon_key', data: { Name: 'Moonlight Key', Description: 'Opens lunar vault', Group: 'Quest', Type: 'Default' } },
      ],
    },
  });
  assert.equal(proposeRes.status, 200, proposeRes.text);
  const crId = proposeRes.json.request.id;
  assert.ok(proposeRes.json.resolved_refs && proposeRes.json.resolved_refs.moon_key >= 2000000000);

  // Read-only token can inspect the CR, cannot reply or withdraw
  const roGetCr = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: roHeaders,
    query: { action: 'request', id: crId },
  });
  assert.equal(roGetCr.status, 200, roGetCr.text);
  assert.equal(roGetCr.json.request.id, crId);

  const roReply = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'POST', headers: roHeaders,
    query: { action: 'request_reply' },
    body: { id: crId, text: 'Should be blocked by scope' },
  });
  assert.equal(roReply.status, 403);

  const roWithdraw = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'POST', headers: roHeaders,
    query: { action: 'request_withdraw' },
    body: { id: crId },
  });
  assert.equal(roWithdraw.status, 403);

  // Another guest cannot view, reply to, or withdraw g_ergo's CR
  const otherGetCr = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'GET', headers: otherHeaders,
    query: { action: 'request', id: crId },
  });
  assert.ok([403, 404].includes(otherGetCr.status), `other guest must not inspect CR, got ${otherGetCr.status}`);

  const otherWithdraw = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'POST', headers: otherHeaders,
    query: { action: 'request_withdraw' },
    body: { id: crId },
  });
  assert.ok([403, 404].includes(otherWithdraw.status));

  // Owner replies via MCP manage_change_request(reply), verifies via get, then withdraws via MCP
  const mcpReply = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 120, method: 'tools/call', params: { name: 'manage_change_request', arguments: { action: 'reply', id: crId, text: 'Ready for admin review!' } } },
  });
  assert.equal(mcpReply.json.result.isError, false, JSON.stringify(mcpReply.json));
  assert.equal(JSON.parse(mcpReply.json.result.content[0].text).message.body, 'Ready for admin review!');

  const mcpGetCr = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 121, method: 'tools/call', params: { name: 'manage_change_request', arguments: { action: 'get', request_id: crId } } },
  });
  const crDetails = JSON.parse(mcpGetCr.json.result.content[0].text);
  assert.ok(crDetails.request.messages.some((m) => m.body === 'Ready for admin review!'), JSON.stringify(crDetails));

  const mcpWithdraw = await execRoute(php, {
    urlPath: '/mcp', method: 'POST', headers: fullHeaders,
    body: { jsonrpc: '2.0', id: 122, method: 'tools/call', params: { name: 'manage_change_request', arguments: { action: 'withdraw', request_id: crId } } },
  });
  assert.equal(mcpWithdraw.json.result.isError, false, JSON.stringify(mcpWithdraw.json));
  assert.equal(JSON.parse(mcpWithdraw.json.result.content[0].text).request.status, 'withdrawn');

  // Withdrawing a second time fails cleanly with request_not_pending
  const withdrawAgain = await execRoute(php, {
    urlPath: '/api/ai.php', method: 'POST', headers: fullHeaders,
    query: { action: 'request_withdraw' },
    body: { id: crId },
  });
  assert.equal(withdrawAgain.status, 409);
  assert.equal(withdrawAgain.json.code, 'request_not_pending');
  } finally {
    try { php.exit(0); } catch {}
  }
});

test('tools/mcp-stdio.mjs: --check диагностика (успех и 401) + таймаут с JSON-RPC ошибкой -32002', async () => {
  let hangMode = false;
  const server = http.createServer((req, res) => {
    if (hangMode) {
      // Intentionally never respond so --timeout triggers cleanly
      return;
    }
    const chunks = [];
    req.on('data', (c) => chunks.push(c));
    req.on('end', () => {
      const auth = String(req.headers.authorization || '');
      if (auth !== 'Bearer esai_valid_token') {
        res.writeHead(401, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ ok: false, error: 'Unauthorized' }));
        return;
      }
      const payload = JSON.parse(Buffer.concat(chunks).toString('utf8') || '{}');
      res.writeHead(200, { 'Content-Type': 'application/json' });
      if (payload.method === 'initialize') {
        res.end(JSON.stringify({
          jsonrpc: '2.0', id: payload.id,
          result: { protocolVersion: '2025-06-18', serverInfo: { name: 'mrpg-test', version: '2.1.0' } },
        }));
      } else if (payload.method === 'tools/list') {
        res.end(JSON.stringify({
          jsonrpc: '2.0', id: payload.id,
          result: { tools: [{ name: 'get_manifest' }, { name: 'search_editor' }] },
        }));
      } else if (payload.method === 'tools/call' && payload.params?.name === 'get_manifest') {
        res.end(JSON.stringify({
          jsonrpc: '2.0', id: payload.id,
          result: {
            isError: false,
            content: [{ type: 'text', text: JSON.stringify({ ok: true, manifest: { auth: { role: 'guest' } } }) }],
            structuredContent: { ok: true, manifest: { auth: { role: 'guest' } } },
          },
        }));
      } else {
        res.end(JSON.stringify({ jsonrpc: '2.0', id: payload.id, result: { ok: true } }));
      }
    });
  });

  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const port = server.address().port;
  const baseUrl = `http://127.0.0.1:${port}`;

  const runBridgeOnce = (args) => new Promise((resolve) => {
    const child = spawn(process.execPath, [path.join(ROOT, 'tools/mcp-stdio.mjs'), ...args], {
      stdio: ['pipe', 'pipe', 'pipe'],
    });
    let out = '';
    let err = '';
    child.stdout.on('data', (d) => { out += d.toString('utf8'); });
    child.stderr.on('data', (d) => { err += d.toString('utf8'); });
    child.on('close', (code) => resolve({ code, out, err }));
  });

  try {
    // 1. Successful --check
    const okCheck = await runBridgeOnce(['--url', baseUrl, '--token', 'esai_valid_token', '--check']);
    assert.equal(okCheck.code, 0, `expected exit 0, out=${okCheck.out} stderr=${okCheck.err}`);
    const diag = JSON.parse(okCheck.out.trim());
    assert.equal(diag.ok, true);
    assert.equal(diag.tools_count, 2);
    assert.equal(diag.protocolVersion, '2025-06-18');
    assert.equal(diag.authenticated, true);

    // 2. Unauthorized --check fails cleanly with non-zero code
    const badCheck = await runBridgeOnce(['--url', baseUrl, '--token', 'esai_wrong', '--check']);
    assert.notEqual(badCheck.code, 0);
    const failDiag = JSON.parse(badCheck.out.trim());
    assert.equal(failDiag.ok, false);
    assert.match(String(failDiag.error || ''), /401/);

    // 3. Timeout produces structured -32002 JSON-RPC error instead of crashing
    hangMode = true;
    const timeoutReply = await new Promise((resolve, reject) => {
      const child = spawn(process.execPath, [
        path.join(ROOT, 'tools/mcp-stdio.mjs'),
        '--url', baseUrl, '--token', 'esai_valid_token', '--timeout', '2',
      ], { stdio: ['pipe', 'pipe', 'pipe'] });
      let buf = '';
      const timer = setTimeout(() => { child.kill(); reject(new Error('bridge timeout test hung')); }, 8000);
      child.stdout.on('data', (d) => {
        buf += d.toString('utf8');
        if (buf.includes('\n')) {
          clearTimeout(timer);
          const line = buf.split('\n')[0];
          child.kill();
          resolve(JSON.parse(line));
        }
      });
      child.stdin.write(JSON.stringify({ jsonrpc: '2.0', id: 77, method: 'tools/list' }) + '\n');
    });
    assert.equal(timeoutReply.id, 77);
    assert.equal(timeoutReply.error.code, -32002);
  } finally {
    server.closeAllConnections?.();
    server.close();
  }
});
