'use strict';

/**
 * tests/ai-modernization.test.cjs — End-to-End 12-scenario suite for
 * universal model-agnostic AI & MCP integration in Teeworlds-MRPG-0.6 Editor Studio.
 *
 * Scenarios covered end-to-end (via real PHP 8.3 in @php-wasm/node, SQLite,
 * router.php, ai/index.php, api/ai.php, api/mcp.php, api/guests.php,
 * api/change-requests.php, api/db-history.php, and real child process mcp-stdio.mjs):
 *   1. Single-link /ai onboarding + /.well-known/ai-plugin.json + /openapi.json + route protection
 *   2. Auto-discovery of newly added editor_resources() tables without duplicating code
 *   3. API & schema discovery + examples + OpenAPI 3.1.0 + capability flags
 *   4. MCP Server (JSON-RPC 2.0 over HTTP /mcp) + real stdio bridge process (tools/mcp-stdio.mjs)
 *   5. Read & analyze objects via Bearer token (list, get, references, DDNet map point, history)
 *   6. Create item (action alias, auto tempId, preview, Bearer propose, approve, db_history)
 *   7. Create world scenario + cross-referenced item via MCP + approve tempId remapping
 *   8. Safely edit/improve existing objects via RFC 7396 patch_json & copy without data loss
 *   9. Validate & safe save + confirm_dangerous gate + reversible rollback via db-history
 *  10. Invalid input, stale baseHash, expected_before conflict, invalid Bearer token handling
 *  11. Scoped AI tokens (read/create/update/delete), tab narrowing, revoke, expiry & guest limits
 *  12. Teeworlds MRPG 0.6 server compatibility rules + real ctf1.map checks
 */

const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { spawn } = require('node:child_process');

const ROOT = path.join(__dirname, '..');
const REPO_ROOT = path.resolve(ROOT, '../..');
const read = (rel) => fs.readFileSync(path.join(ROOT, rel), 'utf8');

let loadNodeRuntime = null;
let PHP = null;
try {
  ({ loadNodeRuntime } = require('@php-wasm/node'));
  ({ PHP } = require('@php-wasm/universal'));
} catch {
  // Handled cleanly via test skip below when optional php-wasm isn't installed.
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
        // Copy values out of by-ref bind_param slots so PDOStatement::execute cannot mutate caller types (e.g. int -> string).
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

function create_test_game_pdo(string $dsn = 'sqlite:/studio/data/game.sqlite'): PDO {
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
        'CREATE TABLE IF NOT EXISTS tw_warehouses (ID INTEGER PRIMARY KEY, Name TEXT, Type TEXT, Trades TEXT, PosX INTEGER, PosY INTEGER, StorageData TEXT, Currency INTEGER, WorldID INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_nodes_collecting (ID INTEGER PRIMARY KEY, SwitchID INTEGER, Level INTEGER, Health INTEGER, Name TEXT, Properties TEXT, Type TEXT, WorldID INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_aethers (ID INTEGER PRIMARY KEY, Name TEXT, WorldID INTEGER, TeleX INTEGER, TeleY INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_dungeons (ID INTEGER PRIMARY KEY, Level INTEGER, DoorX INTEGER, DoorY INTEGER, Scenario TEXT, WorldID INTEGER, TimeLimit INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_vouchers (ID INTEGER PRIMARY KEY, Code TEXT, ValidUntil INTEGER, Multiple INTEGER, Data TEXT)',
        'CREATE TABLE IF NOT EXISTS tw_attributes (ID INTEGER PRIMARY KEY, Name TEXT, Price INTEGER, "Group" INTEGER)',
        'CREATE TABLE IF NOT EXISTS tw_skills_list (ID INTEGER PRIMARY KEY, Name TEXT, Description TEXT, ManaCostPct INTEGER, PriceSP INTEGER, Passive INTEGER, ProfessionID INTEGER)',
    ];
    foreach ($ddls as $sql) $pdo->exec($sql);
    $pdo->exec("DELETE FROM tw_worlds");
    $pdo->exec("INSERT INTO tw_worlds VALUES (0, 'Main World', 'ctf1.map', 'default', '', 0, 0, 1)");
    $pdo->exec("DELETE FROM tw_items_list");
    $item1Data = json_encode(['bonus' => ['strength' => 5, 'crit' => 2], 'rarity' => 'rare', 'use_scenario' => ['name' => 'tutorial_data.json', 'world_id' => 0]]);
    $st = $pdo->prepare('INSERT INTO tw_items_list VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)');
    $st->execute(['', 1, 'Iron Ore', 'Raw ore', 'Resource', 'Resource mineable', '', null, 'universal', 10, 0, 0, 0, 0, 0, $item1Data]);
    $pdo->exec("INSERT INTO tw_items_list VALUES ('', 2, 'Gold Coin', 'Currency', 'Currency', 'Default', '', NULL, 'universal', 1, 0, 0, 0, 0, 0, NULL)");
    $pdo->exec("INSERT INTO tw_quests_list VALUES (10, 0, 'Start Quest', 100, 50, 'Type main')");
    $pdo->exec("INSERT INTO tw_bots_info VALUES (7, 'Blacksmith', '{}', '0', 0, 0, 0, 0, 0, 0)");
    $pdo->exec("INSERT INTO tw_nodes_collecting VALUES (2, 10, 1, 3, 'Iron Vein', '[1/100.0]', 'ore', 0)");
    $pdo->exec("INSERT INTO tw_attributes VALUES (1, 'Strength', 1, 1)");
    $pdo->exec("INSERT INTO tw_skills_list VALUES (1, 'Dash', 'Quick dash', 5, 1, 0, 1)");
    return $pdo;
}
}
`;

async function createModernSandbox() {
  const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(runtime);
  php.mkdir('/studio');
  php.mkdir('/studio/ai');
  php.mkdir('/studio/api');
  php.mkdir('/studio/data');
  php.mkdir('/studio/editor-core');
  php.mkdir('/studio/scenarios');
  php.mkdir('/studio/maps');

  for (const f of fs.readdirSync(path.join(ROOT, 'api'))) {
    if (!f.endsWith('.php')) continue;
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
  php.writeFile('/studio/api/ai-openapi.json', read('api/ai-openapi.json'));
  php.writeFile('/studio/api.php', read('api.php'));
  php.writeFile('/studio/router.php', read('router.php'));
  php.writeFile('/studio/index.html', read('index.html'));
  php.writeFile('/studio/editor-core/reference-schema.json', read('editor-core/reference-schema.json'));
  for (const sc of fs.readdirSync(path.join(ROOT, 'scenarios'))) {
    if (!sc.endsWith('.json')) continue;
    php.writeFile(`/studio/scenarios/${sc}`, fs.readFileSync(path.join(ROOT, 'scenarios', sc)));
  }
  const ctfMap = path.join(REPO_ROOT, 'data/maps/ctf1.map');
  if (fs.existsSync(ctfMap)) {
    php.writeFile('/studio/maps/ctf1.map', fs.readFileSync(ctfMap));
  }

  // Initialize SQLite store (schema v6) + Game SQLite DB + 2 guests
  const initOut = await php.run({
    code: `<?php
      ${TEST_HARNESS_PHP}
      putenv('EDITOR_STUDIO_SQLITE=/studio/data/studio.sqlite');
      create_test_game_pdo();
      require '/studio/api/store.php';
      store_settings_save([
          'host' => 'mock', 'port' => 3306, 'database' => 'mrpg', 'user' => 'root', 'password' => '',
          'editor_password' => password_hash('adminpass1234', PASSWORD_BCRYPT),
          'guest_enabled' => true, 'guest_allow_remote' => true,
          'guest_max_pending' => 4, 'guest_submit_cooldown' => 0, 'guest_max_operations' => 25,
          'maps_dir' => '/studio/maps', 'mapres_dir' => '/studio/maps',
          'map_api' => '', 'map_api_token' => 'TOP_SECRET_MAP_TOKEN',
      ]);
      $gFull = [
          'id' => 'g0000010', 'login' => 'ai_full', 'name' => 'AI Full Author', 'enabled' => true,
          'pages' => ['items', 'worlds', 'quests', 'bots_npc', 'bots_mobs', 'nodes_collecting', 'warehouses', 'crafts', 'scenarios'],
          'passwordHash' => password_hash('guestpass1234', PASSWORD_BCRYPT), 'sessionVersion' => 1,
          'createdAt' => gmdate('c'), 'updatedAt' => gmdate('c'), 'lastLoginAt' => '',
          'limits' => ['max_pending' => 4, 'cooldown' => 0, 'max_operations' => 25],
          'expiresAt' => '',
      ];
      $gItems = [
          'id' => 'g0000011', 'login' => 'ai_items', 'name' => 'AI Items Only', 'enabled' => true,
          'pages' => ['items'],
          'passwordHash' => password_hash('guestpass5678', PASSWORD_BCRYPT), 'sessionVersion' => 1,
          'createdAt' => gmdate('c'), 'updatedAt' => gmdate('c'), 'lastLoginAt' => '',
          'limits' => ['max_pending' => 2, 'cooldown' => 0, 'max_operations' => 2],
          'expiresAt' => '',
      ];
      store_guest_user_insert($gFull);
      store_guest_user_insert($gItems);
      echo json_encode(['ok' => true]);
    `,
  });
  assert.equal(JSON.parse(initOut.text).ok, true);
  return php;
}

async function execRoute(php, {
  urlPath = '/ai',
  method = 'GET',
  query = {},
  body = null,
  headers = {},
  role = null, // 'admin' | null
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

    $uriPath = (string)parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);
    if (str_starts_with($uriPath, '/api/')) {
        $_SERVER['SCRIPT_NAME'] = $uriPath;
        require '/studio' . $uriPath;
    } elseif ($uriPath === '/api.php') {
        $_SERVER['SCRIPT_NAME'] = '/api.php';
        require '/studio/api.php';
    } else {
        $handled = require '/studio/router.php';
        if (!$handled) {
            http_response_code(404);
            echo json_encode(['static_fallthrough' => true]);
        }
    }
  `;

  const res = await php.run({ code: script });
  let json = null;
  try {
    json = JSON.parse(res.text);
  } catch {
    // HTML or plain text responses
  }
  return { status: res.httpStatusCode, text: res.text, json };
}

test('End-to-End 12 AI Modernization Scenarios (Onboarding, Auto-Discovery, MCP/Stdio, Scoped Tokens, patch_json, copy, confirm_dangerous, Rollback & MRPG Compat)', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const php = await createModernSandbox();

  try {
    // ── SCENARIO 1: Single-link /ai onboarding, discovery, OpenAPI & route protection ─
    const aiHtml = await execRoute(php, { urlPath: '/ai/' });
    assert.equal(aiHtml.status, 200);
    assert.ok(aiHtml.text.includes('ai-discovery-manifest'), 'HTML onboarding must embed machine-readable JSON manifest');
    assert.ok(aiHtml.text.includes('rel="openapi"') || aiHtml.text.includes('rel="service-desc"'), 'HTML must expose discovery links');
    assert.ok(!aiHtml.text.includes('TOP_SECRET_MAP_TOKEN') && !aiHtml.text.includes('/studio/maps'), 'public /ai must never expose secrets');

    const aiJson = await execRoute(php, { urlPath: '/ai', query: { format: 'json' } });
    assert.equal(aiJson.status, 200);
    assert.equal(aiJson.json.api_version, '2.0');
    assert.equal(aiJson.json.model_agnostic, true);
    // OpenAPI primary URL must be the API fallback (works without rewrite rules;
    // /openapi.json is kept as a documented alias for rewritten deployments).
    assert.ok(aiJson.json.urls.openapi_3_1.endsWith('/api/ai.php?action=openapi'));
    assert.ok(aiJson.json.urls.openapi_direct.endsWith('/openapi.json'));
    assert.ok(aiJson.json.urls.mcp_api.endsWith('/api/mcp.php'));
    assert.ok(aiJson.json.urls.mcp_http.endsWith('/mcp'));
    assert.ok(aiJson.json.urls.discovery_api.endsWith('/api/ai.php?action=discovery'));

    const aiAccept = await execRoute(php, { urlPath: '/ai/', headers: { Accept: 'application/json' } });
    assert.equal(aiAccept.status, 200);
    assert.equal(aiAccept.json.service, aiJson.json.service);

    const plugin = await execRoute(php, { urlPath: '/.well-known/ai-plugin.json' });
    assert.equal(plugin.status, 200);
    assert.equal(plugin.json.name_for_model, 'mrpg_editor_studio');
    assert.equal(plugin.json.capabilities.patch_json, true);
    assert.equal(plugin.json.capabilities.dangerous_confirm_gate, true);

    const openapiRoot = await execRoute(php, { urlPath: '/openapi.json' });
    assert.equal(openapiRoot.status, 200);
    assert.equal(openapiRoot.json.openapi, '3.1.0');
    assert.equal(openapiRoot.json.info.version, '2.0.0');
    assert.ok(openapiRoot.json.components.securitySchemes.BearerAuth);
    assert.ok(openapiRoot.json.paths['/ai'] && openapiRoot.json.paths['/mcp']);

    // Sensitive paths stay strictly blocked by router.php
    for (const secretPath of ['/data/studio.sqlite', '/.env', '/.git/config', '/scenarios/tutorial_data.json']) {
      const blocked = await execRoute(php, { urlPath: secretPath });
      assert.equal(blocked.status, 404, `router.php must block ${secretPath}`);
    }
    const indexHtml = read('index.html');
    assert.ok(indexHtml.includes('href="ai/?format=json"') && indexHtml.includes('href="api/ai.php?action=openapi"'));
    const apacheConf = read('apache-config.conf');
    assert.ok(apacheConf.includes('RewriteRule ^/\\.well-known/ai-plugin\\.json$') && apacheConf.includes('RewriteRule ^/mcp/?$'));

    // ── Mint scoped AI Bearer tokens via Admin API (guests.php) ─────────────────────
    const tokFullRes = await execRoute(php, {
      urlPath: '/api/guests.php', method: 'POST', role: 'admin',
      query: { action: 'token_create' },
      body: { guest_id: 'g0000010', name: 'Full AI Agent', scopes: ['read', 'create', 'update', 'delete'] },
    });
    assert.equal(tokFullRes.status, 200, tokFullRes.text);
    assert.ok(tokFullRes.json.secret.startsWith('esai_'));
    const fullToken = tokFullRes.json.secret;
    const fullTokenId = tokFullRes.json.token.id;

    // Verify token_list never leaks raw secret or sha256 hash
    const tokList = await execRoute(php, { urlPath: '/api/guests.php', role: 'admin', query: { action: 'token_list' } });
    assert.equal(tokList.status, 200);
    assert.ok(!JSON.stringify(tokList.json).includes(fullToken), 'token_list must never return raw secret');
    assert.ok(!JSON.stringify(tokList.json).includes('tokenHash'));

    const tokReadOnlyRes = await execRoute(php, {
      urlPath: '/api/guests.php', method: 'POST', role: 'admin',
      query: { action: 'token_create' },
      body: { guest_id: 'g0000010', name: 'Reader Bot', scopes: ['read'] },
    });
    const readOnlyToken = tokReadOnlyRes.json.secret;

    const tokCreateOnlyRes = await execRoute(php, {
      urlPath: '/api/guests.php', method: 'POST', role: 'admin',
      query: { action: 'token_create' },
      body: { guest_id: 'g0000011', name: 'Item Creator', scopes: ['read', 'create'] },
    });
    const itemCreateToken = tokCreateOnlyRes.json.secret;

    const tokExpiredRes = await execRoute(php, {
      urlPath: '/api/guests.php', method: 'POST', role: 'admin',
      query: { action: 'token_create' },
      body: { guest_id: 'g0000010', name: 'Expired Token', scopes: ['read'], expires_at: '2020-01-01' },
    });
    const expiredToken = tokExpiredRes.json.secret;

    const tokNarrowPagesRes = await execRoute(php, {
      urlPath: '/api/guests.php', method: 'POST', role: 'admin',
      query: { action: 'token_create' },
      body: { guest_id: 'g0000010', name: 'Items Only Token', scopes: ['read', 'create'], pages: ['items'] },
    });
    const narrowPagesToken = tokNarrowPagesRes.json.secret;

    // ── SCENARIO 2: Auto-discovery of a new editor_resources() table ────────────────
    const autoDiscoveryOut = await php.run({
      code: `<?php
        ${TEST_HARNESS_PHP}
        require '/studio/api/ai-lib.php';
        $fakeRes = [
            'table' => 'tw_custom_relics', 'pk' => 'ID', 'title' => 'Relics', 'label' => 'Title',
            'columns' => ['Title', 'WorldID', 'PowerLevel', 'MetaJson'],
            'required' => ['Title', 'WorldID'],
            'json' => ['MetaJson'],
            'refs' => ['WorldID' => 'tw_worlds'],
            'guest' => true,
        ];
        $autoSchema = ai_schema_auto_resource('custom_relics', $fakeRes);
        $autoExample = ai_schema_example_for('custom_relics', $fakeRes, $autoSchema);
        echo json_encode(['schema' => $autoSchema, 'example' => $autoExample], JSON_UNESCAPED_UNICODE);
      `,
    });
    const autoParsed = JSON.parse(autoDiscoveryOut.text);
    assert.equal(autoParsed.schema.auto_discovered, true);
    assert.equal(autoParsed.schema.columns.MetaJson.type, 'json');
    assert.equal(autoParsed.schema.columns.WorldID.ref, 'tw_worlds');
    assert.equal(autoParsed.example.create.resource, 'custom_relics');

    // ── SCENARIO 3: Manifest v2.0 + Resources + Examples via Bearer Token ───────────
    const authHeaders = { Authorization: `Bearer ${fullToken}` };
    const statusBearer = await execRoute(php, { urlPath: '/api/auth.php', query: { action: 'status' }, headers: authHeaders });
    assert.equal(statusBearer.status, 200);
    assert.equal(statusBearer.json.authorized, true);
    assert.equal(statusBearer.json.auth_method, 'bearer_token');
    assert.deepEqual(statusBearer.json.scopes, ['read', 'create', 'update', 'delete']);

    const man = await execRoute(php, { urlPath: '/api/ai.php', query: { action: 'manifest' }, headers: authHeaders });
    assert.equal(man.status, 200);
    assert.equal(man.json.manifest.api_version, '2.0');
    assert.equal(man.json.manifest.capabilities.auto_resource_discovery, true);
    assert.equal(man.json.manifest.capabilities.patch_json_merge, true);
    assert.ok(man.json.manifest.resources.items.examples.create);
    assert.ok(man.json.manifest.resources.items.examples.update_patch_json);
    assert.ok(man.json.manifest.resources.scenarios.examples.create);
    assert.equal(man.json.manifest.resources_summary.vouchers, undefined, 'vouchers must stay hidden');

    // ── SCENARIO 4: MCP HTTP JSON-RPC 2.0 + Real Child Process tools/mcp-stdio.mjs ──
    const mcpGet = await execRoute(php, { urlPath: '/mcp', method: 'GET' });
    assert.equal(mcpGet.status, 200);
    assert.equal(mcpGet.json.protocol, 'mcp');
    for (const tool of ['patch_json_record', 'copy_record', 'analyze_scenario', 'find_similar', 'apply_patch']) {
      assert.ok(mcpGet.json.tools.includes(tool), `MCP GET tools list должен включать ${tool}`);
    }

    const mcpInit = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: { jsonrpc: '2.0', id: 1, method: 'initialize', params: {} },
    });
    assert.equal(mcpInit.status, 200);
    assert.equal(mcpInit.json.result.protocolVersion, '2024-11-05');

    const mcpTools = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: { jsonrpc: '2.0', id: 2, method: 'tools/list' },
    });
    assert.equal(mcpTools.json.result.tools.length, 14);
    const mcpToolNames = mcpTools.json.result.tools.map((t) => t.name);
    for (const tool of ['analyze_scenario', 'find_similar', 'apply_patch']) {
      assert.ok(mcpToolNames.includes(tool), `tools/list должен включать ${tool}`);
    }

    const mcpResList = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: { jsonrpc: '2.0', id: 3, method: 'resources/list' },
    });
    assert.ok(mcpResList.json.result.resources.some((r) => r.uri === 'mrpg://schema/items'));

    const mcpResRead = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: { jsonrpc: '2.0', id: 4, method: 'resources/read', params: { uri: 'mrpg://schema/items' } },
    });
    assert.ok(JSON.parse(mcpResRead.json.result.contents[0].text).columns_schema.Data);

    // Real stdio bridge test spawning tools/mcp-stdio.mjs against live local HTTP server wrapping execRoute
    const bridgeServer = http.createServer((req, res) => {
      const chunks = [];
      req.on('data', (c) => chunks.push(c));
      req.on('end', async () => {
        const raw = Buffer.concat(chunks).toString('utf8');
        let parsedBody = null;
        try { parsedBody = raw ? JSON.parse(raw) : null; } catch {}
        const result = await execRoute(php, {
          urlPath: req.url.split('?')[0],
          method: req.method,
          body: parsedBody,
          headers: req.headers,
        });
        res.writeHead(result.status, { 'Content-Type': 'application/json' });
        res.end(result.text);
      });
    });
    await new Promise((resolve) => bridgeServer.listen(0, '127.0.0.1', resolve));
    const port = bridgeServer.address().port;

    try {
      const stdioOutput = await new Promise((resolve, reject) => {
        const child = spawn(process.execPath, [
          path.join(ROOT, 'tools/mcp-stdio.mjs'),
          '--url', `http://127.0.0.1:${port}`,
          '--token', fullToken,
        ], { stdio: ['pipe', 'pipe', 'pipe'] });
        let out = '';
        let err = '';
        child.stdout.on('data', (d) => { out += d.toString('utf8'); });
        child.stderr.on('data', (d) => { err += d.toString('utf8'); });
        child.on('error', reject);
        child.on('close', (code) => {
          if (code !== 0) reject(new Error(`mcp-stdio exited ${code}: ${err}`));
          else resolve(out.trim().split('\n').map((l) => JSON.parse(l)));
        });
        child.stdin.write(JSON.stringify({ jsonrpc: '2.0', id: 10, method: 'initialize', params: {} }) + '\n');
        child.stdin.write(JSON.stringify({ jsonrpc: '2.0', id: 11, method: 'tools/call', params: { name: 'get_manifest', arguments: { include_schemas: false } } }) + '\n');
        child.stdin.end();
      });
      assert.equal(stdioOutput.length, 2);
      assert.equal(stdioOutput[0].result.serverInfo.name, 'mrpg-editor-studio-mcp');
      assert.equal(stdioOutput[1].result.structuredContent.manifest.api_version, '2.0');
    } finally {
      bridgeServer.close();
    }

    // ── SCENARIO 5: Read & analyze objects + DDNet map point check ─────────────────
    const getItem1 = await execRoute(php, {
      urlPath: '/api/ai.php', headers: authHeaders,
      query: { action: 'get', resource: 'items', id: '1', include_references: '1' },
    });
    assert.equal(getItem1.status, 200);
    assert.equal(getItem1.json.record.Data.bonus.strength, 5);
    assert.ok(
      getItem1.json.references.inbound.some((r) => r.source_resource === 'nodes_collecting' && r.record_id === 2),
      `Expected nodes_collecting inbound in: ${JSON.stringify(getItem1.json.references)}`
    );

    const refScen = await execRoute(php, {
      urlPath: '/api/ai.php', headers: authHeaders,
      query: { action: 'references', resource: 'scenarios', id: 'tutorial_data.json' },
    });
    assert.equal(refScen.status, 200);
    assert.ok(refScen.json.references.inbound.some((r) => r.source_resource === 'items' && r.record_id === 1));

    const mapCheck = await execRoute(php, {
      urlPath: '/api/ai.php', headers: authHeaders,
      query: { action: 'get', resource: 'maps', world: '0', x: '160', y: '160' },
    });
    assert.equal(mapCheck.status, 200);
    assert.equal(mapCheck.json.map.format, 'DDNet v4');

    // ── SCENARIO 6: Create item with `action` alias + auto tempId + admin approve ─
    const itemCreateBody = {
      message: 'Create Emerald Shard via Bearer token + action alias',
      idempotency_key: 'e2e-create-item-01',
      operations: [
        {
          action: 'create',
          resource: 'items',
          data: {
            Name: 'Emerald Shard',
            Description: 'Mined in deep caves',
            Group: 'Resource',
            Type: 'Resource mineable',
            InitialPrice: 120,
            Data: { bonus: { mining_fortune: 2 } },
          },
        },
      ],
    };
    const itemPrev = await execRoute(php, { urlPath: '/api/ai.php', method: 'POST', headers: authHeaders, query: { action: 'preview' }, body: itemCreateBody });
    assert.equal(itemPrev.status, 200, itemPrev.text);
    assert.equal(itemPrev.json.requires_confirmation, false);

    const itemProp = await execRoute(php, { urlPath: '/api/ai.php', method: 'POST', headers: authHeaders, query: { action: 'propose' }, body: itemCreateBody });
    assert.equal(itemProp.status, 200, itemProp.text);
    assert.equal(itemProp.json.request.author, undefined, 'guest view must strip raw author object');
    const itemReqId = itemProp.json.request.id;

    const adminItemReq = await execRoute(php, { urlPath: '/api/change-requests.php', role: 'admin', query: { action: 'get', id: itemReqId } });
    assert.equal(adminItemReq.json.request.author.via, 'ai_token');

    const approveItem = await execRoute(php, {
      urlPath: '/api/change-requests.php', method: 'POST', role: 'admin',
      query: { action: 'approve' }, body: { id: itemReqId, version: 1, comment: 'Approved Emerald Shard' },
    });
    assert.equal(approveItem.status, 200, approveItem.text);
    const createdItemRow = await execRoute(php, {
      urlPath: '/api/ai.php', headers: authHeaders,
      query: { action: 'list', resource: 'items', search: 'Emerald Shard' },
    });
    assert.equal(createdItemRow.json.total, 1);
    const newItemId = createdItemRow.json.rows[0].ID;

    // ── SCENARIO 7: Create world scenario + cross-referenced item via MCP ───────────
    const mcpProposeScene = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: {
        jsonrpc: '2.0', id: 20, method: 'tools/call',
        params: {
          name: 'propose_changes',
          arguments: {
            message: 'Create world welcoming quest item & linked scenario',
            idempotency_key: 'mcp-scene-combo-01',
            operations: [
              { type: 'create', resource: 'items', tempId: 2000000123, data: { Name: 'Valley Pass', Group: 'Quest', Type: 'Default' } },
              {
                type: 'create', resource: 'scenarios', filename: 'valley_welcome.json',
                data: {
                  content: {
                    scenario_mode: 'default',
                    steps: [
                      { id: 'intro', components: [{ type: 'message', text: 'Welcome!', delay: 0, next_step_id: 'reward' }] },
                      { id: 'reward', components: [{ type: 'check_has_item', item_id: 2000000123, count: 1 }] },
                    ],
                  },
                },
              },
            ],
          },
        },
      },
    });
    assert.equal(mcpProposeScene.status, 200);
    assert.equal(mcpProposeScene.json.result.isError, false, JSON.stringify(mcpProposeScene.json));
    const sceneReqId = mcpProposeScene.json.result.structuredContent.request.id;

    const approveScene = await execRoute(php, {
      urlPath: '/api/change-requests.php', method: 'POST', role: 'admin',
      query: { action: 'approve' }, body: { id: sceneReqId, version: 1, comment: 'Ship scenario' },
    });
    assert.equal(approveScene.status, 200, approveScene.text);
    const writtenScene = JSON.parse(php.readFileAsText('/studio/scenarios/valley_welcome.json'));
    const remappedPassId = writtenScene.steps[1].components[0].item_id;
    assert.ok(remappedPassId > 0 && remappedPassId < 2000000000, `tempId must remap to real ID, got ${remappedPassId}`);

    // ── SCENARIO 7c: MCP read-only verify tools (analyze_scenario, find_similar, apply_patch) ─
    const mcpAnalyze = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: { jsonrpc: '2.0', id: 21, method: 'tools/call', params: { name: 'analyze_scenario', arguments: { id: 'valley_welcome.json' } } },
    });
    assert.equal(mcpAnalyze.json.result.isError, false, JSON.stringify(mcpAnalyze.json));
    assert.equal(mcpAnalyze.json.result.structuredContent.filename, 'valley_welcome.json');
    assert.ok(mcpAnalyze.json.result.structuredContent.component_total >= 2);
    assert.ok(Array.isArray(mcpAnalyze.json.result.structuredContent.dependencies));

    const mcpCorpus = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: { jsonrpc: '2.0', id: 22, method: 'tools/call', params: { name: 'analyze_scenario', arguments: {} } },
    });
    assert.equal(mcpCorpus.json.result.isError, false);
    assert.equal(mcpCorpus.json.result.structuredContent.mode, 'corpus');
    assert.ok(mcpCorpus.json.result.structuredContent.scanned >= 2);
    assert.ok(mcpCorpus.json.result.structuredContent.components.some((c) => c.type === 'message'));

    const mcpSimilar = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: { jsonrpc: '2.0', id: 23, method: 'tools/call', params: { name: 'find_similar', arguments: { resource: 'items', name: 'Valley Pass' } } },
    });
    assert.equal(mcpSimilar.json.result.isError, false, JSON.stringify(mcpSimilar.json));
    assert.ok(mcpSimilar.json.result.structuredContent.candidates.some((c) => c.label === 'Valley Pass'),
      JSON.stringify(mcpSimilar.json.result.structuredContent));

    const mcpApplyPatch = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: {
        jsonrpc: '2.0', id: 24, method: 'tools/call',
        params: { name: 'apply_patch', arguments: { resource: 'items', id: 1, patch_json: { Data: { bonus: { strength: 7 } } } } },
      },
    });
    assert.equal(mcpApplyPatch.json.result.isError, false, JSON.stringify(mcpApplyPatch.json));
    assert.equal(mcpApplyPatch.json.result.structuredContent.changed_fields, 1);
    assert.equal(mcpApplyPatch.json.result.structuredContent.record.Data.bonus.strength, 7);

    const itemStillUnpatched = await execRoute(php, {
      urlPath: '/api/ai.php', headers: authHeaders,
      query: { action: 'get', resource: 'items', id: '1' },
    });
    const bonusStrength = itemStillUnpatched.json.record.Data?.bonus?.strength;
    assert.equal(bonusStrength, 5, 'apply_patch не должен изменять БД (SCENARIO 8 позднее поставит 9 через propose)');

    // ── SCENARIO 8: Safe improvement via RFC 7396 patch_json & copy ──────────────────
    const patchRes = await execRoute(php, {
      urlPath: '/mcp', method: 'POST', headers: authHeaders,
      body: {
        jsonrpc: '2.0', id: 30, method: 'tools/call',
        params: {
          name: 'patch_json_record',
          arguments: {
            resource: 'items', id: 1,
            patch_json: { Data: { bonus: { strength: 9 } } },
            propose: true,
            message: 'Buff Iron Ore bonus strength safely without erasing crit or rarity',
          },
        },
      },
    });
    assert.equal(patchRes.json.result.isError, false, JSON.stringify(patchRes.json));
    const patchReqId = patchRes.json.result.structuredContent.request.id;
    await execRoute(php, {
      urlPath: '/api/change-requests.php', method: 'POST', role: 'admin',
      query: { action: 'approve' }, body: { id: patchReqId, version: 1, comment: 'Buff OK' },
    });
    const patchedItem = await execRoute(php, {
      urlPath: '/api/ai.php', headers: authHeaders,
      query: { action: 'get', resource: 'items', id: '1' },
    });
    assert.equal(patchedItem.json.record.Data.bonus.strength, 9);
    assert.equal(patchedItem.json.record.Data.bonus.crit, 2, 'unrelated nested field must remain untouched');
    assert.equal(patchedItem.json.record.Data.rarity, 'rare', 'unrelated top-level Data field must remain untouched');

    // copy DB item + copy scenario via REST & MCP
    const copyPreview = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: authHeaders,
      query: { action: 'preview' },
      body: {
        operations: [
          { action: 'copy', resource: 'items', source_id: newItemId, data: { Name: 'Pure Emerald Shard' } },
          { action: 'copy', resource: 'scenarios', source_filename: 'valley_welcome.json', filename: 'valley_welcome_hard.json', patch_json: { scenario_mode: 'universal' } },
        ],
      },
    });
    assert.equal(copyPreview.status, 200, copyPreview.text);
    assert.equal(copyPreview.json.valid, true);
    assert.equal(copyPreview.json.normalized_operations[0].data.InitialPrice, 120);
    assert.equal(copyPreview.json.normalized_operations[0].data.Name, 'Pure Emerald Shard');
    assert.equal(copyPreview.json.normalized_operations[1].data.content.scenario_mode, 'universal');
    assert.equal(copyPreview.json.normalized_operations[1].data.content.steps.length, 2);

    // ── SCENARIO 9: confirm_dangerous gate + safe delete + db-history rollback ───────
    const unsafeDelete = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: authHeaders,
      query: { action: 'propose' },
      body: {
        message: 'Delete item without confirmation flag',
        operations: [{ type: 'delete', resource: 'items', id: newItemId }],
      },
    });
    assert.equal(unsafeDelete.status, 400);
    assert.equal(unsafeDelete.json.code, 'confirmation_required');
    assert.equal(unsafeDelete.json.requires_confirmation, true);
    assert.equal(unsafeDelete.json.dangerous_actions[0].code, 'delete_operation');

    const unsafeJsonClear = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: authHeaders,
      query: { action: 'propose' },
      body: {
        message: 'Wipe Data JSON on item #1',
        operations: [{ type: 'update', resource: 'items', id: 1, data: { Data: [] } }],
      },
    });
    assert.equal(unsafeJsonClear.status, 400);
    assert.equal(unsafeJsonClear.json.code, 'confirmation_required');
    assert.equal(unsafeJsonClear.json.dangerous_actions[0].code, 'clearing_json_field');

    const confirmedDelete = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: authHeaders,
      query: { action: 'propose' },
      body: {
        message: 'Confirmed removal of test Emerald Shard',
        confirm_dangerous: true,
        operations: [{ type: 'delete', resource: 'items', id: newItemId }],
      },
    });
    assert.equal(confirmedDelete.status, 200, confirmedDelete.text);
    const delReqId = confirmedDelete.json.request.id;

    const approveDel = await execRoute(php, {
      urlPath: '/api/change-requests.php', method: 'POST', role: 'admin',
      query: { action: 'approve' }, body: { id: delReqId, version: 1, comment: 'Delete test item' },
    });
    assert.equal(approveDel.status, 200);
    const gone = await execRoute(php, { urlPath: '/api/ai.php', headers: authHeaders, query: { action: 'get', resource: 'items', id: String(newItemId) } });
    assert.equal(gone.status, 404);

    // Rollback deletion via admin db-history.php?action=revert!
    const histAdmin = await execRoute(php, { urlPath: '/api/db-history.php', role: 'admin', query: { action: 'list', resource: 'items' } });
    const delHistEntry = histAdmin.json.rows.find((r) => r.op === 'delete' && r.record_id === newItemId);
    assert.ok(delHistEntry && delHistEntry.can_revert);
    const revertRes = await execRoute(php, {
      urlPath: '/api/db-history.php', method: 'POST', role: 'admin',
      query: { action: 'revert' }, body: { id: delHistEntry.id },
    });
    assert.equal(revertRes.status, 200, revertRes.text);
    const restored = await execRoute(php, { urlPath: '/api/ai.php', headers: authHeaders, query: { action: 'get', resource: 'items', id: String(newItemId) } });
    assert.equal(restored.status, 200);
    assert.equal(restored.json.record.Name, 'Emerald Shard');

    // ── SCENARIO 10: Invalid inputs, baseHash conflict, expected_before & bad token ─
    const staleBaseHash = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: authHeaders,
      query: { action: 'preview' },
      body: {
        operations: [{
          type: 'update', resource: 'scenarios', filename: 'valley_welcome.json',
          baseHash: '0'.repeat(64),
          patch_json: { scenario_mode: 'universal' },
        }],
      },
    });
    assert.equal(staleBaseHash.status, 409);
    assert.equal(staleBaseHash.json.code, 'base_hash_mismatch');

    const staleExpectedBefore = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: authHeaders,
      query: { action: 'preview' },
      body: {
        operations: [{
          type: 'update', resource: 'items', id: 1,
          expected_before: { Name: 'Wrong Stale Name' },
          data: { Name: 'Updated Ore' },
        }],
      },
    });
    assert.equal(staleExpectedBefore.status, 409);
    assert.equal(staleExpectedBefore.json.code, 'version_conflict');

    const badTokenRes = await execRoute(php, {
      urlPath: '/api/ai.php', query: { action: 'manifest' },
      headers: { Authorization: 'Bearer esai_0000000000000000000000000000' },
    });
    assert.equal(badTokenRes.status, 401);
    assert.equal(badTokenRes.json.code, 'invalid_token');

    // Invalid Bearer token sent WITH an admin cookie must NEVER fall back to admin session
    const noFallbackRes = await execRoute(php, {
      urlPath: '/api/ai.php', role: 'admin', query: { action: 'manifest' },
      headers: { Authorization: 'Bearer invalid_short' },
    });
    assert.equal(noFallbackRes.status, 401);

    // ── SCENARIO 11: Scoped RBAC, limits, tab narrowing, expiry, revoke & audit ──────
    // Read-only token: reads OK, validate OK, propose -> 403 scope_forbidden
    const roList = await execRoute(php, { urlPath: '/api/ai.php', query: { action: 'list', resource: 'items' }, headers: { Authorization: `Bearer ${readOnlyToken}` } });
    assert.equal(roList.status, 200);
    const roPropose = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: { Authorization: `Bearer ${readOnlyToken}` },
      query: { action: 'propose' },
      body: { message: 'Should fail scope check', operations: [{ type: 'create', resource: 'items', data: { Name: 'Blocked' } }] },
    });
    assert.equal(roPropose.status, 403);
    assert.equal(roPropose.json.code, 'scope_forbidden');
    assert.equal(roPropose.json.required_scope, 'create');

    // Create-only token: create OK, update -> 403 scope_forbidden, over max_operations (limit=2) -> 400 too_many_operations
    const crOnlyUpd = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: { Authorization: `Bearer ${itemCreateToken}` },
      query: { action: 'propose' },
      body: { message: 'Update not in create scope', operations: [{ type: 'update', resource: 'items', id: 1, data: { Name: 'Nope' } }] },
    });
    assert.equal(crOnlyUpd.status, 403);
    assert.equal(crOnlyUpd.json.code, 'scope_forbidden');

    const crTooMany = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: { Authorization: `Bearer ${itemCreateToken}` },
      query: { action: 'propose' },
      body: {
        message: 'Over limit of 2 ops',
        operations: [
          { type: 'create', resource: 'items', data: { Name: 'A' } },
          { type: 'create', resource: 'items', data: { Name: 'B' } },
          { type: 'create', resource: 'items', data: { Name: 'C' } },
        ],
      },
    });
    assert.equal(crTooMany.status, 400);
    assert.equal(crTooMany.json.code, 'too_many_operations');

    // Tab narrowing token (pages=['items'] only) blocks 'scenarios' even though guest g0000010 has scenarios
    const narrowFail = await execRoute(php, {
      urlPath: '/api/ai.php', headers: { Authorization: `Bearer ${narrowPagesToken}` },
      query: { action: 'list', resource: 'scenarios' },
    });
    assert.equal(narrowFail.status, 403);
    assert.equal(narrowFail.json.code, 'resource_forbidden');

    // Expired token -> 401 token_expired
    const expiredFail = await execRoute(php, {
      urlPath: '/api/ai.php', headers: { Authorization: `Bearer ${expiredToken}` },
      query: { action: 'manifest' },
    });
    assert.equal(expiredFail.status, 401);
    assert.equal(expiredFail.json.code, 'token_expired');

    // AI token cannot reach admin-only endpoints (api.php or guests.php)
    const adminApiBlocked = await execRoute(php, { urlPath: '/api.php', headers: authHeaders, query: { action: 'status' } });
    assert.ok(adminApiBlocked.status === 401 || adminApiBlocked.status === 403);
    const guestsApiBlocked = await execRoute(php, { urlPath: '/api/guests.php', headers: authHeaders, query: { action: 'list' } });
    assert.ok(guestsApiBlocked.status === 401 || guestsApiBlocked.status === 403);

    // AI action history returns actor audit entries without IP leaks
    const aiHistory = await execRoute(php, { urlPath: '/api/ai.php', headers: authHeaders, query: { action: 'history' } });
    assert.equal(aiHistory.status, 200);
    assert.ok(aiHistory.json.audit.length >= 2);
    assert.equal(aiHistory.json.audit[0].ip, undefined, 'guest history must strip raw IP');

    // Revoking token immediately blocks further requests
    const revTok = await execRoute(php, {
      urlPath: '/api/guests.php', method: 'POST', role: 'admin',
      query: { action: 'token_revoke' }, body: { id: fullTokenId },
    });
    assert.equal(revTok.status, 200);
    const afterRevoke = await execRoute(php, { urlPath: '/api/ai.php', headers: authHeaders, query: { action: 'manifest' } });
    assert.equal(afterRevoke.status, 401);
    assert.equal(afterRevoke.json.code, 'token_revoked');

    // ── SCENARIO 12: Teeworlds MRPG 0.6 server compatibility invariants ──────────────
    const roHeaders = { Authorization: `Bearer ${readOnlyToken}` };
    const mrpgVal = await execRoute(php, {
      urlPath: '/api/ai.php', method: 'POST', headers: roHeaders,
      query: { action: 'validate' },
      body: {
        operations: [
          // Valid MRPG NPC Guardian (Function=2), Mining Node, Craft, Warehouse Shop
          { type: 'create', resource: 'bots_npc', data: { BotID: 7, WorldID: 0, PosX: 320, PosY: 320, Function: 2, Static: 1, Emote: 'Happy' } },
          { type: 'create', resource: 'nodes_collecting', data: { SwitchID: 15, Level: 2, Health: 5, Name: 'Ruby Vein', Properties: '[1/100]', Type: 'ore', WorldID: 0 } },
          { type: 'create', resource: 'crafts', data: { GroupName: '|Smithing|Gems', ItemID: 1, ItemValue: 1, RequiredItems: '[1/2]', Price: 50, WorldID: 0 } },
          { type: 'create', resource: 'warehouses', data: { Name: 'Town Store', Type: 'selling', PosX: 400, PosY: 320, Currency: 2, WorldID: 0, Trades: "*Potions\n[1/1(50)]" } },
          // Invalid Function=5 (>2), invalid SwitchID=300 (>255), conflicting Quest Flags
          { type: 'create', resource: 'bots_npc', data: { BotID: 7, WorldID: 0, PosX: 320, PosY: 320, Function: 5, Static: 1 } },
          { type: 'create', resource: 'nodes_collecting', data: { SwitchID: 300, Level: 1, Health: 1, Name: 'Bad Switch', Properties: '[1/50]', Type: 'ore', WorldID: 0 } },
          { type: 'create', resource: 'quests', data: { Name: 'Conflict', Money: 10, Exp: 10, Flags: 'Type main,Type daily' } },
        ],
      },
    });
    assert.equal(mrpgVal.status, 200);
    assert.equal(mrpgVal.json.valid, false);
    // First 4 valid ops produce 0 errors; ops 4, 5, 6 fail with exact domain checks
    assert.ok(mrpgVal.json.errors.every((e) => e.operationIndex >= 4), JSON.stringify(mrpgVal.json.errors));
    const mrpgErrCodes = mrpgVal.json.errors.map((e) => `${e.operationIndex}:${e.code}`);
    assert.ok(mrpgErrCodes.includes('4:out_of_range'), 'bots_npc Function=5 must fail out_of_range');
    assert.ok(mrpgErrCodes.includes('5:collecting_node_invalid') || mrpgErrCodes.includes('5:out_of_range'));
    assert.ok(mrpgErrCodes.includes('6:quest_flags_conflict'));
  } finally {
    try { php.exit(0); } catch {}
  }
});
