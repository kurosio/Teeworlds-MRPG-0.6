'use strict';

/**
 * tests/ai-api.test.cjs — сквозная проверка AI API (api/ai.php, ai-lib.php,
 * ai-schema.php, scenario-files.php), интеграции сценариев в change-requests и
 * db-history, а также паритета схем с mrpg_shared.sql, registry.js и ddnet-map.
 */

const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const ROOT = path.join(__dirname, '..');
const REPO_ROOT = path.resolve(ROOT, '../..');
const read = (rel) => fs.readFileSync(path.join(ROOT, rel), 'utf8');

let loadNodeRuntime = null;
let PHP = null;
try {
  ({ loadNodeRuntime } = require('@php-wasm/node'));
  ({ PHP } = require('@php-wasm/universal'));
} catch {
  // Опциональные пакеты @php-wasm/node/@php-wasm/universal: если не установлены,
  // запускаются только статические и JS-проверки.
}

function parseSharedSqlSchema() {
  const sql = fs.readFileSync(path.join(REPO_ROOT, 'mrpg_shared.sql'), 'utf8');
  const tables = new Map();
  for (const m of sql.matchAll(/CREATE TABLE(?: IF NOT EXISTS)? `([^`]+)` \(([\s\S]*?)\) ENGINE=/g)) {
    const tbl = m[1];
    const cols = new Map();
    for (const line of m[2].split('\n')) {
      const colMatch = line.match(/^\s*`([^`]+)`\s+(.+?),?$/);
      if (!colMatch) continue;
      const col = colMatch[1];
      const def = colMatch[2];
      const enumMatch = def.match(/^enum\(((?:'(?:[^'\\]|''|\\.)*'(?:\s*,\s*)?)+)\)/i);
      const setMatch = def.match(/^set\(((?:'(?:[^'\\]|''|\\.)*'(?:\s*,\s*)?)+)\)/i);
      const varcharMatch = def.match(/^varchar\((\d+)\)/i);
      const parseQuotedList = (raw) => [...raw.matchAll(/'((?:[^'\\]|''|\\.)*)'/g)].map((x) => x[1].replace(/''/g, "'"));
      cols.set(col, {
        raw: def.trim(),
        enumValues: enumMatch ? parseQuotedList(enumMatch[1]) : null,
        setValues: setMatch ? parseQuotedList(setMatch[1]) : null,
        varcharLen: varcharMatch ? Number(varcharMatch[1]) : null,
      });
    }
    tables.set(tbl, cols);
  }
  return tables;
}

function loadRegistryAndEvents() {
  const window = { EditorCore: {} };
  const context = vm.createContext({ window, console, Map, Set });
  context.window.window = context.window;
  vm.runInContext(read('editor-core/registry.js'), context);
  vm.runInContext(read('editor-core/scenario-events.js'), context);
  return {
    Schemas: context.window.EditorCore.schemas,
    ScenarioEvents: context.window.EditorCore.ScenarioEvents,
  };
}

function loadTileCatalog() {
  const context = vm.createContext({ window: {}, console });
  vm.runInContext(read('vendor/ddnet-map/picker-tile-catalog.js'), context);
  return context.window.MapPickerTilesCatalog;
}

// ── 1. Статические контракты безопасности, ACL и UI-интеграции ──────────────

test('безопасность и ACL: api.php остаётся admin-only, vouchers и секреты изолированы от AI/гостей', () => {
  const apiPhp = read('api.php');
  assert.ok(apiPhp.includes('bootstrap_editor_api(true);'), 'api.php обязан оставаться строго admin-only');
  assert.ok(!apiPhp.includes('EDITOR_ROLE_GUEST'), 'api.php не должен принимать гостевые сессии');

  const dbCore = read('api/db-core.php');
  assert.ok(dbCore.includes("'scenarios'") && dbCore.includes("'write' => ['scenarios']"), 'scenarios должен присутствовать в каталоге прав гостя');

  const dbResources = read('api/db-resources.php');
  assert.ok(!dbResources.includes("'scenarios' => ["), 'scenarios — файловый ресурс и не должен попадать в editor_resources()');

  const aiPhp = read('api/ai.php');
  for (const act of ['openapi', 'manifest', 'resources', 'list', 'get', 'references', 'validate', 'preview', 'propose']) {
    assert.ok(aiPhp.includes(`'${act}'`), `ai.php должен поддерживать действие ${act}`);
  }
  assert.ok(aiPhp.includes("guard_write_actions($action, ['validate', 'preview', 'apply_patch', 'propose'])"));
  assert.ok(aiPhp.includes('guest_session_required'), 'propose обязан отклонять администратора кодом guest_session_required');

  const aiLib = read('api/ai-lib.php');
  for (const secret of ['maps_dir', 'mapres_dir', 'map_api_token', 'passwordHash']) {
    assert.ok(!aiLib.includes(`'${secret}' =>`) && !aiLib.includes(`"${secret}" =>`), `ai-lib не должен возвращать секрет ${secret}`);
  }

  // Прямой HTTP-доступ к *.json, *.md и scenarios/ запрещён как в Apache, так и в router.php
  const apacheConf = read('apache-config.conf');
  assert.match(apacheConf, /FilesMatch.*md\|json/i);
  assert.ok(apacheConf.includes('/scenarios'));
  const routerPhp = read('router.php');
  assert.match(routerPhp, /scenarios/i);

  // OpenAPI 3.1.0 спецификация и AI_API.md валидны и согласованы с кодом
  const openapi = JSON.parse(read('api/ai-openapi.json'));
  assert.equal(openapi.openapi, '3.1.0');
  assert.ok(openapi.paths['/api/auth.php'] && openapi.paths['/api/ai.php'] && openapi.paths['/api/change-requests.php']);
  for (const p of Object.keys(openapi.paths)) {
    assert.ok(!p.includes('?'), `OpenAPI path не должен содержать query-строку: ${p}`);
  }
  const getActions = openapi.paths['/api/ai.php'].get.parameters.find((p) => p.name === 'action').schema.enum;
  const postActions = openapi.paths['/api/ai.php'].post.parameters.find((p) => p.name === 'action').schema.enum;
  for (const act of ['openapi', 'manifest', 'resources', 'list', 'get', 'references']) {
    assert.ok(getActions.includes(act), `OpenAPI GET ai.php должен перечислять ${act}`);
  }
  for (const act of ['validate', 'preview', 'propose']) {
    assert.ok(postActions.includes(act), `OpenAPI POST ai.php должен перечислять ${act}`);
  }
  assert.ok(openapi.components.schemas.ErrorResponse.required.includes('code'));
  assert.ok(read('AI_API.md').includes('?action=openapi'));
});

test('клиентская интеграция scenarios: db-history, request-workbench, change-requests, lang_ru', () => {
  // 1) DbHistory: подпись Scenarios (files), без #0, не открывается в БД-редакторах
  const dbHistWin = { addEventListener() {}, dispatchEvent() {} };
  const dbHistCtx = vm.createContext({ window: dbHistWin, console, setTimeout, clearTimeout });
  dbHistCtx.window.window = dbHistCtx.window;
  vm.runInContext(read('editor-core/db-history.js'), dbHistCtx);
  const H = dbHistCtx.window.EditorCore.DbHistory;
  const scEntry = {
    id: 42, resource: 'scenarios', resource_title: 'Сценарии (файлы)', record_id: 0,
    op: 'update', label: 'quest_intro.json', revertable: false, can_revert: false, known_resource: true,
  };
  assert.equal(H.resourceLabel(scEntry), 'Scenarios (files)');
  assert.equal(H.entryTitle(scEntry), 'Scenarios (files) · quest_intro.json');
  assert.equal(H.editorFor(scEntry), '');
  assert.equal(H.canRevert(scEntry), false);

  // 2) RequestWorkbench: без deep link для scenarios + сохранение filename/baseHash при правке
  const wbWin = {
    location: { pathname: '/change-requests.html', search: '' },
    STUDIO_MANIFEST: [{ items: [{ key: 'items', src: 'items-editor.html' }] }],
    EditorCore: {},
  };
  const wbCtx = vm.createContext({ window: wbWin, document: { readyState: 'loading', addEventListener() {} }, URLSearchParams, console });
  vm.runInContext(read('editor-core/utils.js'), wbCtx);
  vm.runInContext(read('editor-core/change-markers.js'), wbCtx);
  vm.runInContext(read('editor-core/request-workbench.js'), wbCtx);
  const RW = wbCtx.window.EditorCore.RequestWorkbench;
  assert.equal(RW.recordLinkAttrs({
    op: { type: 'update', resource: 'scenarios', filename: 'quest_intro.json', id: null },
    requestId: 'CR-1',
  }), '');
  const wbSrc = read('editor-core/request-workbench.js');
  assert.ok(wbSrc.includes('filename: op.filename ?? null'));
  assert.ok(wbSrc.includes('baseHash: op.baseHash ?? null'));

  // 3) change-requests.html: не пишет scenarios в Sync и не рисует #null
  const crHtml = read('change-requests.html');
  assert.ok(crHtml.includes("a.resource !== 'scenarios'"));
  assert.ok(crHtml.includes("op.resource === 'scenarios'"));

  // 4) Русский словарь: все добавленные ключи переведены
  const locWin = { EditorCoreLocalization: {} };
  const locCtx = vm.createContext({ window: locWin, console });
  locCtx.window.window = locCtx.window;
  vm.runInContext(read('localization/index.js'), locCtx);
  for (const f of locCtx.window.EditorCoreLocalization.files) vm.runInContext(read(`localization/${f}`), locCtx);
  const ru = locCtx.window.EditorCoreLocalization.strings('ru');
  assert.equal(ru['Scenarios (files)'], 'Сценарии (файлы)');
  assert.equal(ru['Enter a valid scenario filename.'], 'Укажите корректное имя файла сценария.');
  assert.equal(ru['(new record)'], '(новая запись)');
});

// ── 2. Живое выполнение в PHP 8.3 (scenario-files, ai-schema, ai-lib, ai.php) ─

async function createPhpSandbox(withSqliteMocks = false) {
  const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(runtime);
  php.mkdir('/studio');
  php.mkdir('/studio/api');
  php.mkdir('/studio/data');
  php.mkdir('/studio/editor-core');
  php.mkdir('/studio/scenarios');
  php.mkdir('/studio/maps');

  for (const f of fs.readdirSync(path.join(ROOT, 'api'))) {
    if (!f.endsWith('.php')) continue;
    let src = read(`api/${f}`);
    if (withSqliteMocks && f === 'db-core.php') {
      // В тестовом VFS направляем db_connect() на in-memory PDO-обёртку mysqli.
      src = src.replace(
        'function db_connect(): mysqli\n{',
        'function db_connect(): mysqli\n{\n    if (isset($GLOBALS[\'__TEST_MYSQLI\']) && $GLOBALS[\'__TEST_MYSQLI\'] instanceof mysqli) { return $GLOBALS[\'__TEST_MYSQLI\']; }'
      );
    }
    if (withSqliteMocks && f === 'db-resources.php') {
      // В PHP 8.3 нативные C-свойства mysqli недоступны у подкласса без родного подключения.
      src = src.replaceAll('(int)$mysqli->insert_id', '($mysqli instanceof TestMysqli ? $mysqli->testInsertId() : (int)$mysqli->insert_id)');
      src = src.replaceAll('$stmt->affected_rows', '($stmt instanceof TestMysqliStmt ? $stmt->testAffectedRows() : $stmt->affected_rows)');
      src = src.replaceAll('$stmt->num_rows', '($stmt instanceof TestMysqliStmt ? $stmt->testNumRows() : $stmt->num_rows)');
    }
    php.writeFile(`/studio/api/${f}`, src);
  }
  php.writeFile('/studio/api/ai-openapi.json', read('api/ai-openapi.json'));
  php.writeFile('/studio/api.php', read('api.php'));
  php.writeFile('/studio/router.php', read('router.php'));
  php.writeFile('/studio/editor-core/reference-schema.json', read('editor-core/reference-schema.json'));
  for (const sc of fs.readdirSync(path.join(ROOT, 'scenarios'))) {
    if (!sc.endsWith('.json')) continue;
    php.writeFile(`/studio/scenarios/${sc}`, fs.readFileSync(path.join(ROOT, 'scenarios', sc)));
  }
  const ctfMap = path.join(REPO_ROOT, 'data/maps/ctf1.map');
  if (fs.existsSync(ctfMap)) {
    php.writeFile('/studio/maps/ctf1.map', fs.readFileSync(ctfMap));
  }
  return php;
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
    public bool $failNextCommit = false;
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
    public function commit(int $flags = 0, ?string $name = null): bool {
        if ($this->failNextCommit) {
            $this->failNextCommit = false;
            throw new RuntimeException('Simulated DB commit failure');
        }
        return $this->pdo->commit();
    }
    public function rollback(int $flags = 0, ?string $name = null): bool {
        return $this->pdo->inTransaction() ? $this->pdo->rollBack() : true;
    }
    public function close(): true { return true; }
}

function create_test_game_pdo(string $dsn = 'sqlite::memory:'): PDO {
    $pdo = new PDO($dsn);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $ddls = [
        'CREATE TABLE tw_worlds (ID INTEGER PRIMARY KEY, Name TEXT, Path TEXT, Type TEXT, Flags TEXT, RespawnWorldID INTEGER, JailWorldID INTEGER, RequiredLevel INTEGER)',
        'CREATE TABLE tw_items_list (Comment TEXT, ID INTEGER PRIMARY KEY, Name TEXT, Description TEXT, "Group" TEXT, Type TEXT, Flags TEXT, ScenarioData TEXT, ScenarioMode TEXT, InitialPrice INTEGER, RequiresProducts INTEGER, AT1 INTEGER, AT2 INTEGER, ATValue1 INTEGER, ATValue2 INTEGER, Data TEXT)',
        'CREATE TABLE tw_quests_list (ID INTEGER PRIMARY KEY, NextQuestID INTEGER, Name TEXT, Money INTEGER, Exp INTEGER, Flags TEXT)',
        'CREATE TABLE tw_quest_boards (ID INTEGER PRIMARY KEY, Name TEXT, PosX INTEGER, PosY INTEGER, WorldID INTEGER)',
        'CREATE TABLE tw_quests_board_list (ID INTEGER PRIMARY KEY, QuestID INTEGER, DailyBoardID INTEGER)',
        'CREATE TABLE tw_bots_info (ID INTEGER PRIMARY KEY, Name TEXT, JsonTeeInfo TEXT, EquippedModules TEXT, SlotHammer INTEGER, SlotGun INTEGER, SlotShotgun INTEGER, SlotGrenade INTEGER, SlotRifle INTEGER, SlotArmor INTEGER)',
        'CREATE TABLE tw_bots_npc (ID INTEGER PRIMARY KEY, BotID INTEGER, PosX INTEGER, PosY INTEGER, GiveQuestID INTEGER, DialogData TEXT, Function INTEGER, Static INTEGER, Emote TEXT, WorldID INTEGER)',
        'CREATE TABLE tw_bots_mobs (ID INTEGER PRIMARY KEY, BotID INTEGER, WorldID INTEGER, PositionX INTEGER, PositionY INTEGER, Debuffs TEXT, Behavior TEXT, Level INTEGER, Power INTEGER, Number INTEGER, Respawn INTEGER, Radius INTEGER, ActiveRadius INTEGER, WanderRadius INTEGER, Boss INTEGER, it_drop_0 INTEGER, it_drop_1 INTEGER, it_drop_2 INTEGER, it_drop_3 INTEGER, it_drop_4 INTEGER, it_drop_count TEXT, it_drop_chance TEXT)',
        'CREATE TABLE tw_bots_quest (ID INTEGER PRIMARY KEY, BotID INTEGER, QuestID INTEGER, Step INTEGER, WorldID INTEGER, PosX INTEGER, PosY INTEGER, AutoFinish TEXT, DialogData TEXT, ScenarioData TEXT, TasksData TEXT)',
        'CREATE TABLE tw_crafts_list (ID INTEGER PRIMARY KEY, GroupName TEXT, ItemID INTEGER, ItemValue INTEGER, RequiredItems TEXT, Price INTEGER, WorldID INTEGER)',
        'CREATE TABLE tw_warehouses (ID INTEGER PRIMARY KEY, Name TEXT, Type TEXT, Trades TEXT, PosX INTEGER, PosY INTEGER, StorageData TEXT, Currency INTEGER, WorldID INTEGER)',
        'CREATE TABLE tw_nodes_collecting (ID INTEGER PRIMARY KEY, SwitchID INTEGER, Level INTEGER, Health INTEGER, Name TEXT, Properties TEXT, Type TEXT, WorldID INTEGER)',
        'CREATE TABLE tw_aethers (ID INTEGER PRIMARY KEY, Name TEXT, WorldID INTEGER, TeleX INTEGER, TeleY INTEGER)',
        'CREATE TABLE tw_dungeons (ID INTEGER PRIMARY KEY, Level INTEGER, DoorX INTEGER, DoorY INTEGER, Scenario TEXT, WorldID INTEGER, TimeLimit INTEGER)',
        'CREATE TABLE tw_attributes (ID INTEGER PRIMARY KEY, Name TEXT, Price INTEGER, "Group" INTEGER)',
        'CREATE TABLE tw_skills_list (ID INTEGER PRIMARY KEY, Name TEXT, Description TEXT, ManaCostPct INTEGER, PriceSP INTEGER, Passive INTEGER, ProfessionID INTEGER)',
    ];
    foreach ($ddls as $sql) $pdo->exec($sql);
    $pdo->exec("INSERT INTO tw_worlds VALUES (0, 'Main World', 'ctf1.map', 'default', '', 0, 0, 1), (1, 'Dungeon 1', 'dungeon.map', 'dungeon', '', 0, 0, 5)");
    $item1Data = json_encode(['use_scenario' => ['name' => 'tutorial_data.json', 'world_id' => 0]]);
    $stItem = $pdo->prepare('INSERT INTO tw_items_list VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)');
    $stItem->execute(['', 1, 'Iron Ore', 'Raw ore', 'Resource', 'Default', '', null, 'universal', 5, 0, 0, 0, 0, 0, $item1Data]);
    $pdo->exec("INSERT INTO tw_items_list VALUES ('', 2, 'Gold Coin', 'Currency', 'Currency', 'Default', '', NULL, 'universal', 1, 0, 0, 0, 0, 0, NULL)");
    $pdo->exec("INSERT INTO tw_items_list VALUES ('', 3, 'Iron Sword', 'Blade', 'Equipment', 'Equip hammer', '', NULL, 'universal', 150, 0, 0, 0, 0, 0, NULL)");
    $pdo->exec("INSERT INTO tw_quests_list VALUES (10, 0, 'Start Quest', 100, 50, 'Type main')");
    $pdo->exec("INSERT INTO tw_quest_boards VALUES (5, 'Main Board', 100, 100, 0)");
    $pdo->exec("INSERT INTO tw_quests_board_list VALUES (1, 10, 5)");
    $pdo->exec("INSERT INTO tw_bots_info VALUES (7, 'Blacksmith', '{}', '0', 3, 0, 0, 0, 0, 0)");
    $pdo->exec("INSERT INTO tw_bots_npc VALUES (1, 7, 256, 256, 10, '[]', 0, 1, 'Happy', 0)");
    $pdo->exec("INSERT INTO tw_bots_mobs VALUES (1, 7, 0, 300, 300, '', '', 1, 10, 1, 5, 64, 0, 0, 0, 1, 0, 0, 0, 0, '|1|0|0|0|0|', '|100|0|0|0|0|')");
    $pdo->exec("INSERT INTO tw_crafts_list VALUES (4, 'Weapons', 3, 1, '[1/5]', 50, 0)");
    $pdo->exec("INSERT INTO tw_nodes_collecting VALUES (2, 10, 1, 3, 'Iron Vein', '[1/100.0]', 'ore', 0)");
    $pdo->exec("INSERT INTO tw_attributes VALUES (1, 'Strength', 1, 1)");
    $pdo->exec("INSERT INTO tw_skills_list VALUES (1, 'Dash', 'Quick dash', 5, 1, 0, 1)");
    return $pdo;
}
}
`;

test('паритет ai-schema.php, scenario-files.php и ai-lib.php с mrpg_shared.sql, registry.js, scenario-events.js и ddnet-map', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const php = await createPhpSandbox(false);
  try {
    const { text } = await php.run({
      code: `<?php
        require '/studio/api/ai-schema.php';
        require '/studio/api/ai-lib.php';
        echo json_encode([
          'tables' => ai_schema_tables(),
          'editorResources' => array_map(fn($r) => ['table' => $r['table'], 'columns' => $r['columns']], editor_resources()),
          'components' => scenario_known_component_types(),
          'scenarioDefs' => ai_schema_scenario_definitions(),
          'mapDefs' => ai_schema_map_definitions(),
        ], JSON_UNESCAPED_UNICODE);
      `,
    });
    const phpData = JSON.parse(text);
    const sqlTables = parseSharedSqlSchema();

    // Каждая таблица и колонка из ai_schema_tables должна совпадать с editor_resources() и mrpg_shared.sql
    for (const [resKey, tblSchema] of Object.entries(phpData.tables)) {
      const edRes = phpData.editorResources[resKey];
      assert.ok(edRes, `ресурс ${resKey} отсутствует в editor_resources()`);
      const edCols = new Set(edRes.columns);
      const sqlCols = sqlTables.get(edRes.table);
      if (resKey !== 'nodes_collecting') {
        assert.ok(sqlCols, `таблица ${edRes.table} (${resKey}) не найдена в mrpg_shared.sql`);
      }
      for (const [colName, colDef] of Object.entries(tblSchema.columns)) {
        assert.ok(edCols.has(colName), `колонка ${resKey}.${colName} отсутствует в editor_resources()[${resKey}].columns`);
        if (!sqlCols) continue;
        const sqlCol = sqlCols.get(colName);
        assert.ok(sqlCol, `колонка ${edRes.table}.${colName} отсутствует в mrpg_shared.sql`);
        if (colDef.enum && sqlCol.enumValues) {
          assert.deepEqual([...colDef.enum].sort(), [...sqlCol.enumValues].sort(), `enum несовпадение для ${edRes.table}.${colName}`);
        }
        if (colDef.set && sqlCol.setValues) {
          assert.deepEqual([...colDef.set].sort(), [...sqlCol.setValues].sort(), `set несовпадение для ${edRes.table}.${colName}`);
        }
        if (colDef.max_length && sqlCol.varcharLen) {
          assert.equal(colDef.max_length, sqlCol.varcharLen, `varchar max_length несовпадение для ${edRes.table}.${colName}`);
        }
      }
    }

    // Паритет компонентов и событий сценариев с registry.js и scenario-events.js
    const { Schemas, ScenarioEvents } = loadRegistryAndEvents();
    const jsCompTypes = Object.keys(Schemas.componentSchemas || {}).sort();
    assert.deepEqual([...phpData.components].sort(), jsCompTypes, 'список компонентов scenario_known_component_types() разошёлся с registry.js');
    const jsModes = Object.keys(Schemas.scenarioModeSchemas || {}).sort();
    const phpModes = Object.keys(phpData.scenarioDefs.modes || {}).sort();
    assert.deepEqual(phpModes, jsModes, 'список режимов сценариев разошёлся с registry.js');

    const jsEventTypes = Object.keys(ScenarioEvents.COLORS || {}).sort();
    const phpEventTypes = [...phpData.scenarioDefs.events].sort();
    assert.deepEqual(phpEventTypes, jsEventTypes, 'список событий ai_schema_scenario_definitions() разошёлся с scenario-events.js');

    // Паритет DDNet-тайлов с vendor/ddnet-map/picker-tile-catalog.js
    const tileWin = loadTileCatalog();
    assert.deepEqual(Object.keys(phpData.mapDefs.collision_tiles).map(Number).sort((a, b) => a - b), [...tileWin.COLLISION_IDS].sort((a, b) => a - b));
    assert.deepEqual(phpData.mapDefs.game_entity_tiles, Object.fromEntries(tileWin.GAME_TILES.map((t) => [String(t.id), t.key])));
    assert.deepEqual(phpData.mapDefs.switch_tiles, Object.fromEntries(tileWin.SWITCH_TILES.map((t) => [String(t.id), t.key])));
  } finally {
    try { php.exit(0); } catch {}
  }
});

test('scenario-files.php: валидация имён, симлинки, ссылки, блоки шагов и откат файловых операций', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const php = await createPhpSandbox(false);
  try {
    const { text } = await php.run({
      code: `<?php
        require '/studio/api/scenario-files.php';
        $fail = [];
        $check = function (string $name, bool $ok) use (&$fail) { if (!$ok) $fail[] = $name; };

        // Проверка имён и защиты от traversal / dotfile / control bytes / symlink
        $check('valid simple', scenario_validate_filename('quest_01.json')['ok'] === true);
        $check('valid underscore prefix', scenario_validate_filename('_draft.json')['ok'] === true);
        foreach (['../evil.json', 'sub/a.json', 'sub\\\\a.json', '.htaccess', '.hidden.json', 'bad.txt', "bad\\0.json", ' leading.json', ''] as $bad) {
            $r = scenario_validate_filename($bad);
            $check('reject ' . json_encode($bad), $r['ok'] === false && !empty($r['code']));
        }
        @symlink('/etc/passwd', '/studio/scenarios/sym.json');
        $sym = scenario_resolve_path('sym.json', '/studio/scenarios', false);
        $check('symlink rejected', $sym['ok'] === false && $sym['code'] === 'path_traversal_blocked');
        @unlink('/studio/scenarios/sym.json');

        // Чтение реального файла tutorial_data.json
        $tut = scenario_read_file('tutorial_data.json', '/studio/scenarios');
        $check('read tutorial_data', $tut['ok'] === true && is_array($tut['content']) && !empty($tut['sha256']));

        // Ссылки и ремаппинг tempId (вкл. world_id=0 и строковые step id не трогаются)
        $sc = [
            'scenario_mode' => 'default',
            'steps' => [
                [
                    'id' => '2000000001',
                    'components' => [
                        ['type' => 'message', 'text' => 'hi', 'item_id' => 2000000001, 'next_step_id' => 'step_2'],
                        ['type' => 'quest_condition', 'quest_id' => 2000000002, 'world_id' => 0],
                    ],
                ],
                [
                    'id' => 'step_2',
                    'components' => [
                        ['type' => 'universal_teleport', 'text' => 'hi', 'bot_id' => 7],
                    ],
                ],
            ],
            'on_tick' => [
                'steps' => [
                    ['id' => 'tick_1', 'components' => [['type' => 'message', 'text' => 'loop', 'next_step_id' => 'tick_1']]],
                ],
            ],
        ];
        $refs = scenario_extract_references($sc);
        $check('extract item_id', (bool)array_filter($refs, fn($r) => $r['target'] === 'tw_items_list' && $r['id'] === 2000000001));
        $check('extract world 0', (bool)array_filter($refs, fn($r) => $r['target'] === 'tw_worlds' && $r['id'] === 0));
        $remapped = scenario_remap_references($sc, [2000000001 => 88, 2000000002 => 99]);
        $check('step id untouched', $remapped['steps'][0]['id'] === '2000000001');
        $check('item_id remapped', $remapped['steps'][0]['components'][0]['item_id'] === 88);
        $check('quest_id remapped', $remapped['steps'][0]['components'][1]['quest_id'] === 99);

        // Проверка независимости on_* и поломки next_step_id
        $valOk = scenario_validate_content($sc, null, [2000000001 => 'items', 2000000002 => 'quests'], []);
        $check('valid scenario no errors', $valOk['errors'] === []);
        $check('legacy component warns', (bool)array_filter($valOk['warnings'], fn($w) => $w['code'] === 'legacy_component_type'));

        $badSc = $sc;
        $badSc['on_tick']['steps'][0]['components'][0]['next_step_id'] = 'step_2'; // шаг из main steps не виден в on_tick
        $badSc['steps'][0]['components'][0]['delay'] = -5;
        $valBad = scenario_validate_content($badSc, null, [2000000001 => 'items', 2000000002 => 'quests'], ['bots_info:7' => true]);
        $codes = array_column($valBad['errors'], 'code');
        $check('broken_step_link detected', in_array('broken_step_link', $codes, true));
        $check('out_of_range delay detected', in_array('out_of_range', $codes, true));
        $check('deleted_reference_target detected', in_array('deleted_reference_target', $codes, true));

        // JSON diff
        $diff = scenario_json_diff(['a' => 1, 'b' => [1, 2]], ['a' => 2, 'b' => [1, 2, 3]]);
        $check('json diff replace+add', count($diff) === 2 && $diff[0]['op'] === 'replace' && $diff[1]['op'] === 'add');

        // Атомарное применение и полный rollback (create + update + delete)
        file_put_contents('/studio/scenarios/upd.json', json_encode(['steps' => [['id' => 's1', 'components' => []]]]));
        file_put_contents('/studio/scenarios/del.json', json_encode(['steps' => [['id' => 'd1', 'components' => []]]]));
        $exec = scenario_apply_file_ops([
            ['type' => 'create', 'filename' => 'created.json', 'data' => ['content' => ['steps' => [['id' => 'c1', 'components' => []]]]]],
            ['type' => 'update', 'filename' => 'upd.json', 'data' => ['content' => ['steps' => [['id' => 's2', 'components' => []]]]]],
            ['type' => 'delete', 'filename' => 'del.json'],
        ], [], '/studio/scenarios');
        $check('applied created file exists', is_file('/studio/scenarios/created.json'));
        $check('applied deleted file removed', !is_file('/studio/scenarios/del.json'));
        $check('applied updated file changed', str_contains(file_get_contents('/studio/scenarios/upd.json'), '"s2"'));
        ($exec['rollback'])();
        $check('rollback removes created', !is_file('/studio/scenarios/created.json'));
        $check('rollback restores deleted', is_file('/studio/scenarios/del.json') && str_contains(file_get_contents('/studio/scenarios/del.json'), '"d1"'));
        $check('rollback restores updated', str_contains(file_get_contents('/studio/scenarios/upd.json'), '"s1"'));

        echo json_encode(['ok' => empty($fail), 'fail' => $fail]);
      `,
    });
    const res = JSON.parse(text);
    assert.deepEqual(res.fail, [], `Ошибки scenario-files.php: ${res.fail.join(', ')}`);
  } finally {
    try { php.exit(0); } catch {}
  }
});

test('ai-schema.php + DDNet .map парсер (реальная ctf1.map) + ai-lib.php валидация', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const php = await createPhpSandbox(true);
  try {
    const { text } = await php.run({
      code: `<?php
        ${TEST_HARNESS_PHP}
        require '/studio/api/ai-lib.php';
        $pdo = create_test_game_pdo();
        $db = new TestMysqli($pdo);
        $fail = [];
        $check = function (string $name, bool $ok) use (&$fail) { if (!$ok) $fail[] = $name; };

        // 1. Валидация БД-записей по доменным правилам
        $qConflict = ai_schema_validate_db_record('quests', 'create', ['Name' => 'Q', 'Money' => 10, 'Exp' => 10, 'Flags' => 'Type daily,Type weekly'], ['Name' => 'Q', 'Money' => 10, 'Exp' => 10, 'Flags' => 'Type daily,Type weekly'], 0, $db);
        $check('quest Daily+Weekly conflict', in_array('quest_flags_conflict', array_column($qConflict['errors'], 'code'), true));

        $qbPartial = ai_schema_validate_db_record('quest_bots', 'create', ['BotID' => 7, 'QuestID' => 10, 'PosX' => 100, 'PosY' => 100, 'AutoFinish' => 'Partial', 'ScenarioData' => ['steps' => [['id' => 'a', 'components' => []]]]], ['BotID' => 7, 'QuestID' => 10, 'PosX' => 100, 'PosY' => 100, 'AutoFinish' => 'Partial'], 0, $db);
        $check('quest_bots AutoFinish Partial valid', $qbPartial['errors'] === []);

        $whOk = ai_schema_validate_db_record('warehouses', 'create', ['Name' => 'Shop', 'PosX' => 100, 'PosY' => 100, 'WorldID' => 0, 'Trades' => "*Food\n[1/2(50)]"], ['Name' => 'Shop', 'PosX' => 100, 'PosY' => 100, 'WorldID' => 0, 'Trades' => "*Food\n[1/2(50)]"], 0, $db);
        $check('warehouses Trades DSL valid', $whOk['errors'] === []);
        $whBad = ai_schema_validate_db_record('warehouses', 'create', ['Name' => 'Shop', 'PosX' => 100, 'PosY' => 100, 'WorldID' => 0, 'Trades' => 'broken*dsl'], ['Name' => 'Shop', 'PosX' => 100, 'PosY' => 100, 'WorldID' => 0, 'Trades' => 'broken*dsl'], 0, $db);
        $check('warehouses Trades DSL rejected', in_array('invalid_dsl_format', array_column($whBad['errors'], 'code'), true));

        $mobFloat = ai_schema_validate_db_record('bots_mobs', 'create', ['BotID' => 7, 'PositionX' => 300, 'PositionY' => 300, 'it_drop_chance' => '|12.5|0|0|0|0|'], ['BotID' => 7, 'PositionX' => 300, 'PositionY' => 300, 'it_drop_chance' => '|12.5|0|0|0|0|'], 0, $db);
        $check('bots_mobs pipe float Drops valid', $mobFloat['errors'] === []);
        $mobBad = ai_schema_validate_db_record('bots_mobs', 'create', ['BotID' => 7, 'PositionX' => 300, 'PositionY' => 300, 'it_drop_chance' => '|12.5|0|0|'], ['BotID' => 7, 'PositionX' => 300, 'PositionY' => 300, 'it_drop_chance' => '|12.5|0|0|'], 0, $db);
        $check('bots_mobs 4-part pipe rejected', in_array('invalid_pipe_vector', array_column($mobBad['errors'], 'code'), true));

        $nodeDup = ai_schema_validate_db_record('nodes_collecting', 'create', ['SwitchID' => 10, 'Level' => 1, 'Health' => 3, 'Name' => 'Dup', 'WorldID' => 0, 'Type' => 'ore', 'Properties' => '[1/50]'], ['SwitchID' => 10, 'Level' => 1, 'Health' => 3, 'Name' => 'Dup', 'WorldID' => 0, 'Type' => 'ore', 'Properties' => '[1/50]'], 0, $db);
        $check('nodes_collecting duplicate unique key', in_array('duplicate_unique_key', array_column($nodeDup['errors'], 'code'), true));

        $qblDup = ai_schema_validate_db_record('quest_board_links', 'create', ['QuestID' => 10, 'DailyBoardID' => 5], ['QuestID' => 10, 'DailyBoardID' => 5], 0, $db);
        $check('quest_board_links duplicate QuestID', in_array('duplicate_unique_key', array_column($qblDup['errors'], 'code'), true));

        // 2. Реальный парсинг DDNet v4 .map (ctf1.map)
        $mapBytes = file_get_contents('/studio/maps/ctf1.map');
        $parsed = ai_map_parse_datafile($mapBytes, true, 160, 160);
        $check('ctf1 parsed ok', ($parsed['format'] ?? '') === 'DDNet v4' && !empty($parsed['sha256']));
        $check('ctf1 has dimensions', ($parsed['width_tiles'] ?? 0) > 0 && ($parsed['height_tiles'] ?? 0) > 0);
        $check('ctf1 has layers', ($parsed['layers_count'] ?? 0) > 0 && isset($parsed['game_layer']['non_empty_tiles']));
        $check('ctf1 has point_check', isset($parsed['point_check']['x'], $parsed['point_check']['safe_for_placement']));

        // 3. Проверка ai_validate_operations: tempId авто-подстановка и связь item -> craft + scenario
        $payloadOps = ai_extract_operations_payload(['operations' => [
            ['type' => 'create', 'resource' => 'items', 'data' => ['Name' => 'Crystal', 'Group' => 'Resource', 'Type' => 'Default']],
            ['type' => 'create', 'resource' => 'crafts', 'data' => ['GroupName' => 'Alchemy', 'ItemID' => 2000000001, 'ItemValue' => 1, 'RequiredItems' => '[1/2]', 'WorldID' => 0]],
            ['type' => 'create', 'resource' => 'scenarios', 'filename' => 'crystal.json', 'data' => ['content' => ['steps' => [['id' => 's1', 'components' => [['type' => 'message', 'text' => 'hi', 'item_id' => 2000000001]]]]]]],
        ]]);
        $valRes = ai_validate_operations($db, $payloadOps, EDITOR_ROLE_GUEST, ['items', 'crafts', 'scenarios'], '/studio/scenarios');
        $check('cross-resource tempId valid', $valRes['valid'] === true);

        // Запрет доступа гостя к невыданной вкладке и ошибка target type mismatch
        $badPerm = ai_validate_operations($db, [['type' => 'update', 'resource' => 'worlds', 'id' => 0, 'data' => ['Name' => 'X']]], EDITOR_ROLE_GUEST, ['items'], '/studio/scenarios');
        $check('guest resource_forbidden', !$badPerm['valid'] && $badPerm['errors'][0]['code'] === 'resource_forbidden');

        $badRef = ai_validate_operations($db, [
            ['type' => 'create', 'resource' => 'items', 'tempId' => 2000000001, 'data' => ['Name' => 'NotWorld']],
            ['type' => 'create', 'resource' => 'crafts', 'tempId' => 2000000002, 'data' => ['GroupName' => 'G', 'ItemID' => 3, 'ItemValue' => 1, 'WorldID' => 2000000001]],
        ], EDITOR_ROLE_GUEST, ['items', 'crafts'], '/studio/scenarios');
        $check('wrong target resource caught', !$badRef['valid'] && in_array('invalid_reference_target', array_column($badRef['errors'], 'code'), true));

        echo json_encode(['ok' => empty($fail), 'fail' => $fail]);
      `,
    });
    let res;
    try {
      res = JSON.parse(text);
    } catch (e) {
      assert.fail(`Не удалось распарсить JSON от PHP в тесте ai-schema/ai-lib: ${e.message}\nВывод PHP:\n${text}`);
    }
    assert.deepEqual(res.fail, [], `Ошибки ai-schema/ai-lib: ${res.fail.join(', ')}`);
  } finally {
    try { php.exit(0); } catch {}
  }
});

test('сквозной HTTP/SQLite поток: api/ai.php (manifest..propose), cr_apply откат, approve и db-history', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const php = await createPhpSandbox(true);
  try {
    const runnerPhp = `<?php
      ${TEST_HARNESS_PHP}
      putenv('EDITOR_STUDIO_SQLITE=/studio/data/studio.sqlite');
      require '/studio/api/store.php';

      // 1. Настройка SQLite хранилища, гостевых пользователей и конфигурации карт
      $guestHash = password_hash('guestpass1234', PASSWORD_BCRYPT);
      store_settings_save([
          'host' => 'mock', 'port' => 3306, 'database' => 'mrpg', 'user' => 'root', 'password' => '',
          'editor_password' => password_hash('adminpass1234', PASSWORD_BCRYPT),
          'guest_enabled' => true, 'guest_allow_remote' => true,
          'guest_max_pending' => 2, 'guest_submit_cooldown' => 0, 'guest_max_operations' => 20,
          'maps_dir' => '/studio/maps', 'mapres_dir' => '/studio/maps',
          'map_api' => '', 'map_api_token' => 'TOP_SECRET_TOKEN',
      ]);
      $guest = [
          'id' => 'g0000001', 'login' => 'ai_bot', 'name' => 'AI Helper', 'enabled' => true,
          'pages' => ['items', 'worlds', 'scenarios'],
          'passwordHash' => $guestHash, 'sessionVersion' => 1,
          'createdAt' => gmdate('c'), 'updatedAt' => gmdate('c'), 'lastLoginAt' => '',
          'limits' => ['max_pending' => 2, 'cooldown' => 0, 'max_operations' => 20],
          'expiresAt' => '',
      ];
      store_guest_user_insert($guest);

      require '/studio/api/db-core.php';
      $epoch = guest_user_epoch($guest);
      echo json_encode(['ready' => true, 'epoch' => $epoch]);
    `;
    const initOut = await php.run({ code: runnerPhp });
    const initData = JSON.parse(initOut.text);
    assert.equal(initData.ready, true);

    const validCsrf = 'c'.repeat(64);
    async function callApi(endpoint, { method = 'GET', query = {}, body = null, role = 'guest', origin = 'http://editor.test', csrf = validCsrf } = {}) {
      const qHex = Buffer.from(JSON.stringify(query), 'utf8').toString('hex');
      const bHex = Buffer.from(body !== null ? JSON.stringify(body) : '', 'utf8').toString('hex');
      const script = `<?php
        ${TEST_HARNESS_PHP}
        putenv('EDITOR_STUDIO_SQLITE=/studio/data/studio.sqlite');
        $GLOBALS['__TEST_GAME_PDO'] ??= create_test_game_pdo();
        $GLOBALS['__TEST_MYSQLI'] = new TestMysqli($GLOBALS['__TEST_GAME_PDO']);
        $_SERVER['REQUEST_METHOD'] = ${JSON.stringify(method)};
        $_SERVER['REMOTE_ADDR'] = '127.0.0.1';
        $_SERVER['HTTP_HOST'] = 'editor.test';
        $_SERVER['HTTP_ORIGIN'] = ${JSON.stringify(origin)};
        $_SERVER['CONTENT_TYPE'] = ${body !== null ? "'application/json'" : "''"};
        $_SERVER['HTTP_X_EDITOR_CSRF'] = ${JSON.stringify(csrf)};
        $_GET = json_decode(hex2bin('${qHex}'), true) ?: [];
        $_SESSION = [];
        if (session_status() !== PHP_SESSION_ACTIVE) @session_start();
        $_SESSION['editor_studio_csrf'] = str_repeat('c', 64);
        if (${JSON.stringify(role)} === 'guest') {
            $_SESSION['editor_studio_guest'] = true;
            $_SESSION['editor_studio_guest_user'] = 'g0000001';
            $_SESSION['editor_studio_guest_epoch'] = hex2bin('${Buffer.from(String(initData.epoch), 'utf8').toString('hex')}');
            $_SESSION['editor_studio_guest_key'] = str_repeat('a', 48);
        } elseif (${JSON.stringify(role)} === 'admin') {
            $_SESSION['editor_studio_auth'] = true;
            $_SESSION['editor_studio_user'] = 'admin';
        }
        $rawBody = hex2bin('${bHex}');
        putenv('EDITOR_STUDIO_TEST=1');
        putenv('EDITOR_STUDIO_BODY=' . ($rawBody === false ? '' : $rawBody));
        require '/studio/api/' . ${JSON.stringify(endpoint)};
      `;
      const r = await php.run({ code: script });
      let parsed;
      try {
        parsed = JSON.parse(r.text);
      } catch (e) {
        assert.fail(`Некорректный JSON от ${endpoint} (${method}): ${e.message}\nВывод:\n${r.text}`);
      }
      return { status: r.httpStatusCode, body: parsed };
    }

  // 1. manifest: гость видит только items/worlds/scenarios + без секретов
  const man = await callApi('ai.php', { query: { action: 'manifest' } });
  assert.equal(man.status, 200);
  assert.equal(man.body.ok, true);
  assert.equal(man.body.manifest.auth.role, 'guest');
  assert.ok(man.body.manifest.resources_summary.items);
  assert.ok(man.body.manifest.resources_summary.scenarios);
  assert.equal(man.body.manifest.resources_summary.crafts, undefined);
  assert.equal(man.body.manifest.resources_summary.vouchers, undefined);
  assert.ok(!JSON.stringify(man.body).includes('TOP_SECRET_TOKEN'));
  assert.ok(!JSON.stringify(man.body).includes('/studio/maps'));

  // 2. resources: 403 на запрещённый crafts / vouchers, 200 на items
  const forbCrafts = await callApi('ai.php', { query: { action: 'resources', resource: 'crafts' } });
  assert.equal(forbCrafts.status, 403);
  assert.equal(forbCrafts.body.code, 'resource_forbidden');

  const forbVouchers = await callApi('ai.php', { query: { action: 'resources', resource: 'vouchers' } });
  assert.equal(forbVouchers.status, 403);

  // 3. list & get: фильтрация, проекция полей, чтение сценария + карты
  const listItems = await callApi('ai.php', {
    query: { action: 'list', resource: 'items', search: 'Ore', fields: 'ID,Name' },
  });
  assert.equal(listItems.status, 200);
  assert.equal(listItems.body.total, 1);
  assert.deepEqual(listItems.body.rows[0], { ID: 1, Name: 'Iron Ore' });

  const listSc = await callApi('ai.php', { query: { action: 'list', resource: 'scenarios' } });
  assert.equal(listSc.status, 200);
  assert.ok(listSc.body.rows.some((r) => r.filename === 'tutorial_data.json'));

  const getSc = await callApi('ai.php', {
    query: { action: 'get', resource: 'scenarios', file: 'tutorial_data.json', include_references: '1' },
  });
  assert.equal(getSc.status, 200);
  assert.equal(getSc.body.filename, 'tutorial_data.json');
  assert.ok(getSc.body.references.inbound.some((x) => x.source_resource === 'items' && x.record_id === 1));

  // Чтение реальной .map карты и проверка точки для NPC (без утечки путей)
  const getMap = await callApi('ai.php', {
    query: { action: 'get', resource: 'maps', world: '0', include_entities: '1', x: '160', y: '160' },
  });
  assert.equal(getMap.status, 200);
  assert.equal(getMap.body.available, true);
  assert.equal(getMap.body.map.format, 'DDNet v4');
  assert.ok(!JSON.stringify(getMap.body).includes('/studio/maps'));

  // 4. references: входящие ссылки на item #1 фильтруют недоступный crafts, но видят scenarios & items
  const refsItem = await callApi('ai.php', {
    query: { action: 'references', resource: 'items', id: '1' },
  });
  assert.equal(refsItem.status, 200);
  assert.ok(!refsItem.body.references.inbound.some((r) => r.source_resource === 'crafts'));

  // 5. validate (POST): 200 с деталями ошибок и без мутаций; валидный мульти-пакет проходит
  const badVal = await callApi('ai.php', {
    method: 'POST', query: { action: 'validate' },
    body: { operations: [{ type: 'create', resource: 'scenarios', filename: '../hack.json', data: { content: {} } }] },
  });
  assert.equal(badVal.status, 200);
  assert.equal(badVal.body.valid, false);
  assert.equal(badVal.body.errors[0].code, 'path_traversal_blocked');

  // 6. preview (POST): возвращает diff и json_diff без сохранения
  const createScBody = {
    message: 'Добавлен сценарий новой миссии и тестовый предмет',
    idempotency_key: 'ai_key_0001',
    operations: [
      { type: 'create', resource: 'items', data: { Name: 'Quest Key', Group: 'Quest', Type: 'Default' } },
      {
        type: 'create', resource: 'scenarios', filename: 'quest_key_scene.json',
        data: { content: { scenario_mode: 'default', steps: [{ id: 'start', components: [{ type: 'give_item', item_id: 2000000001 }] }] } },
      },
    ],
  };
  const prev = await callApi('ai.php', { method: 'POST', query: { action: 'preview' }, body: createScBody });
  assert.equal(prev.status, 200, `preview failed: ${JSON.stringify(prev.body)}`);
  assert.equal(prev.body.valid, true);
  assert.equal(prev.body.analysis.summary.create, 2);
  assert.ok(!php.fileExists('/studio/scenarios/quest_key_scene.json'));

  // 7. propose: админу запрещено (403 guest_session_required), гость создаёт + идемпотентность + revise
  const adminProp = await callApi('ai.php', { method: 'POST', role: 'admin', query: { action: 'propose' }, body: createScBody });
  assert.equal(adminProp.status, 403, JSON.stringify(adminProp.body));
  assert.equal(adminProp.body.code, 'guest_session_required');

  const prop1 = await callApi('ai.php', { method: 'POST', query: { action: 'propose' }, body: createScBody });
  assert.equal(prop1.status, 200, `prop1 failed: ${JSON.stringify(prop1.body)}`);
  assert.equal(prop1.body.duplicate, false);
  const reqId = prop1.body.request.id;

  const propDup = await callApi('ai.php', { method: 'POST', query: { action: 'propose' }, body: createScBody });
  assert.equal(propDup.status, 200, `propDup failed: ${JSON.stringify(propDup.body)}`);
  assert.equal(propDup.body.duplicate, true);
  assert.equal(propDup.body.request.id, reqId);

  // Ревизия сценария (только scenarios, чтобы проверить approve + db_history)
  const scOnlyBody = {
    request_id: reqId, version: 1,
    operations: [
      {
        type: 'create', resource: 'scenarios', filename: 'quest_key_scene.json',
        data: { content: { scenario_mode: 'default', steps: [{ id: 'start', components: [{ type: 'give_item', item_id: 1 }] }] } },
      },
    ],
  };
  const revOk = await callApi('ai.php', { method: 'POST', query: { action: 'propose' }, body: scOnlyBody });
  assert.equal(revOk.status, 200, `revOk failed: ${JSON.stringify(revOk.body)}`);
  assert.equal(revOk.body.revised, true);
  assert.equal(revOk.body.request.version, 2);

  // Проверка version_conflict при устаревшем version=1
  const revStale = await callApi('ai.php', { method: 'POST', query: { action: 'propose' }, body: scOnlyBody });
  assert.equal(revStale.status, 409, `revStale failed: ${JSON.stringify(revStale.body)}`);
  assert.equal(revStale.body.code, 'version_conflict');

  // Проверка HTTP 405 на GET-действие вызванное POST-ом
  const wrongMethod = await callApi('ai.php', { method: 'POST', query: { action: 'manifest' }, body: {} });
  assert.equal(wrongMethod.status, 405, JSON.stringify(wrongMethod.body));

  // 8. Одобрение (approve) через api/change-requests.php + запись неотменяемого db_history + проверка 409 при revert
  const approveRes = await callApi('change-requests.php', {
    method: 'POST', role: 'admin', query: { action: 'approve' },
    body: { id: reqId, version: 2, comment: 'Принято' },
  });
  assert.equal(approveRes.status, 200, `approve failed: ${JSON.stringify(approveRes.body)}`);
  assert.equal(approveRes.body.ok, true);
  assert.ok(php.fileExists('/studio/scenarios/quest_key_scene.json'));

  const histList = await callApi('db-history.php', {
    role: 'admin', query: { action: 'list', resource: 'scenarios' },
  });
  assert.equal(histList.status, 200, `histList failed: ${JSON.stringify(histList.body)}`);
  assert.equal(histList.body.rows.length, 1);
  const hEntry = histList.body.rows[0];
  assert.equal(hEntry.resource, 'scenarios');
  assert.equal(hEntry.revertable, false);
  assert.equal(hEntry.can_revert, false);

  const histRevert = await callApi('db-history.php', {
    method: 'POST', role: 'admin', query: { action: 'revert' },
    body: { id: hEntry.id },
  });
  assert.equal(histRevert.status, 409);
  assert.equal(histRevert.body.code, 'history_not_revertable');

  // 9. Проверка отката cr_apply() при сбое транзакции БД (созданный/обновлённый файл должен откатиться)
  const rbOut = await php.run({
    code: `<?php
      ${TEST_HARNESS_PHP}
      putenv('EDITOR_STUDIO_SQLITE=/studio/data/studio.sqlite');
      require '/studio/api/change-requests-lib.php';
      $pdo = create_test_game_pdo();
      $db = new TestMysqli($pdo);
      $db->failNextCommit = true;
      $beforeRaw = file_get_contents('/studio/scenarios/quest_key_scene.json');
      $req = [
          'id' => 'CR-20261127-000001-aaaaaa',
          'operations' => [
              ['type' => 'update', 'resource' => 'scenarios', 'filename' => 'quest_key_scene.json', 'data' => ['content' => ['steps' => [['id' => 'broken', 'components' => []]]]]],
              ['type' => 'create', 'resource' => 'scenarios', 'filename' => 'temp_should_vanish.json', 'data' => ['content' => ['steps' => []]]],
          ],
      ];
      $threw = false;
      try {
          cr_apply($db, $req, true);
      } catch (Throwable $e) {
          $threw = true;
      }
      echo json_encode([
          'threw' => $threw,
          'updated_restored' => file_get_contents('/studio/scenarios/quest_key_scene.json') === $beforeRaw,
          'created_removed' => !file_exists('/studio/scenarios/temp_should_vanish.json'),
      ]);
    `,
  });
    const rbRes = JSON.parse(rbOut.text);
    assert.equal(rbRes.threw, true);
    assert.equal(rbRes.updated_restored, true);
    assert.equal(rbRes.created_removed, true);
  } finally {
    try { php.exit(0); } catch {}
  }
});

test('безопасность AI API: OpenAPI, полные схемы manifest, изоляция записи/сценариев, SQLi, traversal, конфликты, пагинация max_bytes и bulk tempId approve', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const php = await createPhpSandbox(true);
  try {
    const guestHash = await php.run({
      code: `<?php
        ${TEST_HARNESS_PHP}
        putenv('EDITOR_STUDIO_SQLITE=/studio/data/studio.sqlite');
        require '/studio/api/store.php';
        $hash = password_hash('guestpass1234', PASSWORD_BCRYPT);
        store_settings_save([
            'host' => 'mock', 'port' => 3306, 'database' => 'mrpg', 'user' => 'root', 'password' => '',
            'editor_password' => password_hash('adminpass1234', PASSWORD_BCRYPT),
            'guest_enabled' => true, 'guest_allow_remote' => true,
            'guest_max_pending' => 5, 'guest_submit_cooldown' => 0, 'guest_max_operations' => 4,
            'maps_dir' => '/studio/maps', 'mapres_dir' => '/studio/maps',
            'map_api' => '', 'map_api_token' => 'SECRET_MAP_TOKEN',
        ]);
        $g1 = [
            'id' => 'g0000010', 'login' => 'ai_full', 'name' => 'AI Full', 'enabled' => true,
            'pages' => ['items', 'quests', 'worlds', 'scenarios'],
            'passwordHash' => $hash, 'sessionVersion' => 1,
            'createdAt' => gmdate('c'), 'updatedAt' => gmdate('c'), 'lastLoginAt' => '',
            'limits' => ['max_pending' => 5, 'cooldown' => 0, 'max_operations' => 4],
            'expiresAt' => '',
        ];
        $g2 = [
            'id' => 'g0000011', 'login' => 'ai_no_sc', 'name' => 'AI Restricted', 'enabled' => true,
            'pages' => ['items'],
            'passwordHash' => $hash, 'sessionVersion' => 1,
            'createdAt' => gmdate('c'), 'updatedAt' => gmdate('c'), 'lastLoginAt' => '',
            'limits' => ['max_pending' => 5, 'cooldown' => 0, 'max_operations' => 4],
            'expiresAt' => '',
        ];
        store_guest_user_insert($g1);
        store_guest_user_insert($g2);
        require '/studio/api/db-core.php';
        create_test_game_pdo('sqlite:/studio/data/game.sqlite');
        echo json_encode([
            'e1' => guest_user_epoch($g1),
            'e2' => guest_user_epoch($g2),
        ]);
      `,
    });
    const epochs = JSON.parse(guestHash.text);
    const validCsrf = 'c'.repeat(64);

    async function callApi(endpoint, {
      method = 'GET',
      query = {},
      body = null,
      rawBody = null,
      role = 'g1',
      origin = 'http://editor.test',
      csrf = validCsrf,
      contentType = body !== null || rawBody !== null ? 'application/json' : '',
    } = {}) {
      const qHex = Buffer.from(JSON.stringify(query), 'utf8').toString('hex');
      const payload = rawBody !== null ? rawBody : (body !== null ? JSON.stringify(body) : '');
      const bHex = Buffer.from(payload, 'utf8').toString('hex');
      const userId = role === 'g2' ? 'g0000011' : 'g0000010';
      const epoch = role === 'g2' ? epochs.e2 : epochs.e1;
      const script = `<?php
        ${TEST_HARNESS_PHP}
        putenv('EDITOR_STUDIO_SQLITE=/studio/data/studio.sqlite');
        $pdoGame = new PDO('sqlite:/studio/data/game.sqlite');
        $pdoGame->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        $GLOBALS['__TEST_MYSQLI'] = new TestMysqli($pdoGame);
        $_SERVER['REQUEST_METHOD'] = ${JSON.stringify(method)};
        $_SERVER['REMOTE_ADDR'] = '127.0.0.1';
        $_SERVER['HTTP_HOST'] = 'editor.test';
        $_SERVER['HTTP_ORIGIN'] = ${JSON.stringify(origin)};
        $_SERVER['CONTENT_TYPE'] = ${JSON.stringify(contentType)};
        $_SERVER['HTTP_X_EDITOR_CSRF'] = ${JSON.stringify(csrf)};
        $_GET = json_decode(hex2bin('${qHex}'), true) ?: [];
        $_SESSION = [];
        if (session_status() !== PHP_SESSION_ACTIVE) @session_start();
        $_SESSION['editor_studio_csrf'] = str_repeat('c', 64);
        if (${JSON.stringify(role)} === 'g1' || ${JSON.stringify(role)} === 'g2') {
            $_SESSION['editor_studio_guest'] = true;
            $_SESSION['editor_studio_guest_user'] = ${JSON.stringify(userId)};
            $_SESSION['editor_studio_guest_epoch'] = hex2bin('${Buffer.from(String(epoch), 'utf8').toString('hex')}');
            $_SESSION['editor_studio_guest_key'] = str_repeat('b', 48);
        } elseif (${JSON.stringify(role)} === 'admin') {
            $_SESSION['editor_studio_auth'] = true;
            $_SESSION['editor_studio_user'] = 'admin';
        }
        $rawIn = hex2bin('${bHex}');
        putenv('EDITOR_STUDIO_TEST=1');
        putenv('EDITOR_STUDIO_BODY=' . ($rawIn === false ? '' : $rawIn));
        require '/studio/' . ${JSON.stringify(endpoint)};
      `;
      const r = await php.run({ code: script });
      let parsed;
      try {
        parsed = JSON.parse(r.text);
      } catch (e) {
        assert.fail(`Некорректный JSON от ${endpoint}: ${e.message}\n${r.text}`);
      }
      return { status: r.httpStatusCode, body: parsed };
    }

    // 1. OpenAPI 3.1 endpoint (доступен без авторизации, POST отклоняется 405 с code)
    const oaAnon = await callApi('api/ai.php', { role: 'anon', query: { action: 'openapi' } });
    assert.equal(oaAnon.status, 200);
    assert.equal(oaAnon.body.openapi, '3.1.0');
    assert.ok(oaAnon.body.paths['/api/ai.php']);
    const oaPost = await callApi('api/ai.php', { role: 'anon', method: 'POST', query: { action: 'openapi' }, body: {} });
    assert.equal(oaPost.status, 405);
    assert.equal(oaPost.body.code, 'method_not_allowed');

    // Неавторизованный запрос к manifest -> 401
    const unauth = await callApi('api/ai.php', { role: 'anon', query: { action: 'manifest' } });
    assert.equal(unauth.status, 401);
    assert.equal(unauth.body.code, 'auth_required');

    // 2. manifest отдаёт полные схемы разрешённых ресурсов и умеет сжиматься при include_schemas=0
    const manFull = await callApi('api/ai.php', { query: { action: 'manifest' } });
    assert.equal(manFull.status, 200);
    assert.equal(manFull.body.manifest.openapi, 'api/ai.php?action=openapi');
    assert.ok(manFull.body.manifest.resources.items.writable_columns.includes('Data'));
    assert.ok(manFull.body.manifest.resources.items.columns_schema.Type);
    assert.ok(manFull.body.manifest.resources.quests);
    assert.ok(manFull.body.manifest.resources.scenarios);
    assert.equal(manFull.body.manifest.resources.vouchers, undefined);
    assert.equal(manFull.body.manifest.resources.crafts, undefined);

    const manCompact = await callApi('api/ai.php', { query: { action: 'manifest', include_schemas: '0' } });
    assert.equal(manCompact.status, 200);
    assert.equal(manCompact.body.manifest.resources, undefined);
    assert.ok(manCompact.body.manifest.resources_summary.items);

    // 3. Невозможность прямой записи гостем в БД (db-crud.php, db.php, api.php, change-requests.php)
    const crudGetAllowed = await callApi('api/db-crud.php', { query: { action: 'list', resource: 'items' } });
    assert.equal(crudGetAllowed.status, 200);
    const crudGetForbidden = await callApi('api/db-crud.php', { query: { action: 'list', resource: 'vouchers' } });
    assert.equal(crudGetForbidden.status, 403);
    assert.equal(crudGetForbidden.body.code, 'guest_forbidden');

    for (const act of ['create', 'update', 'delete', 'rename_group']) {
      const r = await callApi('api/db-crud.php', {
        method: 'POST', query: { action: act, resource: 'items' }, body: { id: 1, data: { Name: 'Hacked' } },
      });
      assert.equal(r.status, 403, `db-crud ${act} должен быть запрещён гостю`);
      assert.equal(r.body.code, 'guest_forbidden');
    }
    const adminApiGuest = await callApi('api.php', { query: { action: 'db_meta' } });
    assert.equal(adminApiGuest.status, 403);
    assert.equal(adminApiGuest.body.code, 'guest_forbidden');

    const dbPhpGuest = await callApi('api/db.php', { query: { action: 'stats' } });
    assert.equal(dbPhpGuest.status, 403);

    const approveGuest = await callApi('api/change-requests.php', {
      method: 'POST', query: { action: 'approve' }, body: { id: 'CR-1', version: 1 },
    });
    assert.equal(approveGuest.status, 403);
    assert.equal(approveGuest.body.code, 'guest_forbidden');

    // 4. Пагинация, max_bytes, фильтрация, fields в get/list, summary_only в scenarios
    const page1 = await callApi('api/ai.php', {
      query: { action: 'list', resource: 'items', limit: '2', offset: '0' },
    });
    assert.equal(page1.status, 200);
    assert.equal(page1.body.rows.length, 2);
    assert.equal(page1.body.total, 3);
    assert.equal(page1.body.has_more, true);
    assert.equal(page1.body.next_offset, 2);
    assert.equal(page1.body.truncated, false);

    const page2 = await callApi('api/ai.php', {
      query: { action: 'list', resource: 'items', limit: '2', offset: String(page1.body.next_offset) },
    });
    assert.equal(page2.status, 200);
    assert.equal(page2.body.rows.length, 1);
    assert.equal(page2.body.has_more, false);
    assert.equal(page2.body.next_offset, null);

    // Принудительное ограничение max_bytes (tutorial_data.json / длинные строки в DB)
    const bigDesc = 'X'.repeat(3000);
    await php.run({
      code: `<?php
        $pdoGame = new PDO('sqlite:/studio/data/game.sqlite');
        $pdoGame->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        $pdoGame->prepare("INSERT INTO tw_items_list VALUES ('', 4, 'Heavy A', ?, 'Resource', 'Default', '', NULL, 'universal', 1, 0, 0, 0, 0, 0, NULL)")->execute([${JSON.stringify(bigDesc)}]);
        $pdoGame->prepare("INSERT INTO tw_items_list VALUES ('', 5, 'Heavy B', ?, 'Resource', 'Default', '', NULL, 'universal', 1, 0, 0, 0, 0, 0, NULL)")->execute([${JSON.stringify(bigDesc)}]);
      `,
    });
    const budgetPage = await callApi('api/ai.php', {
      query: { action: 'list', resource: 'items', search: 'Heavy', limit: '10', offset: '0', max_bytes: '4096' },
    });
    assert.equal(budgetPage.status, 200);
    assert.equal(budgetPage.body.total, 2);
    assert.equal(budgetPage.body.rows.length, 1);
    assert.equal(budgetPage.body.truncated, true);
    assert.equal(budgetPage.body.has_more, true);
    assert.equal(budgetPage.body.next_offset, 1);

    const filteredItems = await callApi('api/ai.php', {
      query: { action: 'list', resource: 'items', filter: JSON.stringify({ Group: 'Equipment' }), fields: 'Name' },
    });
    assert.equal(filteredItems.status, 200);
    assert.deepEqual(filteredItems.body.rows, [{ ID: 3, Name: 'Iron Sword' }]);

    const getProjected = await callApi('api/ai.php', {
      query: { action: 'get', resource: 'items', id: '1', fields: 'Name' },
    });
    assert.equal(getProjected.status, 200);
    assert.deepEqual(getProjected.body.record, { ID: 1, Name: 'Iron Ore' });

    const getScSummaryOnly = await callApi('api/ai.php', {
      query: { action: 'get', resource: 'scenarios', id: 'tutorial_data.json', summary_only: '1' },
    });
    assert.equal(getScSummaryOnly.status, 200);
    assert.equal(getScSummaryOnly.body.content, undefined);
    assert.ok(getScSummaryOnly.body.sha256 && getScSummaryOnly.body.summary);

    // 5. Единые коды ошибок HTTP (400/403/404/405/415) + CSRF/Origin защита
    const badJson = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'validate' }, rawBody: '{invalid-json',
    });
    assert.equal(badJson.status, 400);
    assert.equal(badJson.body.code, 'invalid_json');

    const badMime = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'validate' }, rawBody: '{}', contentType: 'text/plain',
    });
    assert.equal(badMime.status, 415);
    assert.equal(badMime.body.code, 'unsupported_media_type');

    const badOrigin = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'validate' }, body: { operations: [] }, origin: 'https://evil.example',
    });
    assert.equal(badOrigin.status, 403);
    assert.equal(badOrigin.body.code, 'cross_origin_forbidden');

    const unknownRes = await callApi('api/ai.php', { query: { action: 'list', resource: 'no_such_table' } });
    assert.equal(unknownRes.status, 404);
    assert.equal(unknownRes.body.code, 'unknown_resource');

    // 6. SQL-инъекции и path traversal (БД, сценарии, карты)
    for (const sqli of ["' OR 1=1 --", '"; DROP TABLE tw_items_list; --']) {
      const rSearch = await callApi('api/ai.php', { query: { action: 'list', resource: 'items', search: sqli } });
      assert.equal(rSearch.status, 200);
      assert.equal(rSearch.body.total, 0);

      const rField = await callApi('api/ai.php', { query: { action: 'list', resource: 'items', fields: sqli } });
      assert.equal(rField.status, 400);
      assert.equal(rField.body.code, 'unknown_field');

      const rFilter = await callApi('api/ai.php', { query: { action: 'list', resource: 'items', filter_key: sqli, filter_value: 'x' } });
      assert.equal(rFilter.status, 400);
      assert.equal(rFilter.body.code, 'unknown_filter_field');

      const rId = await callApi('api/ai.php', { query: { action: 'get', resource: 'items', id: `1 ${sqli}` } });
      assert.equal(rId.status, 400);
      assert.equal(rId.body.code, 'invalid_id');
    }

    for (const badPath of ['../db-config.json', '..\\evil.json', 'sub/file.json', '.htaccess', "nul\0.json"]) {
      const rSc = await callApi('api/ai.php', { query: { action: 'get', resource: 'scenarios', file: badPath } });
      assert.equal(rSc.status, 400, `traversal scenarios обязан блокироваться: ${badPath}`);
      assert.ok(rSc.body.code);
    }
    const routerCheck = await php.run({
      code: `<?php
        ob_start();
        $_SERVER['REQUEST_URI'] = '/scenarios/tutorial_data.json';
        $blockedSc = require '/studio/router.php';
        $blockedBody = ob_get_clean();
        $blockedCode = http_response_code();
        http_response_code(200);
        $_SERVER['REQUEST_URI'] = '/api/ai.php?action=openapi';
        $allowedApi = require '/studio/router.php';
        echo json_encode([
            'blockedSc' => $blockedSc === true && $blockedCode === 404 && str_contains($blockedBody, 'Not found'),
            'allowedApi' => $allowedApi === false,
        ]);
      `,
    });
    const routerJson = JSON.parse(routerCheck.text);
    assert.deepEqual(routerJson, { blockedSc: true, allowedApi: true });
    const mapTraversal = await callApi('api/ai.php', { query: { action: 'get', resource: 'maps', path: '../../etc/passwd' } });
    assert.equal(mapTraversal.status, 400);
    assert.equal(mapTraversal.body.code, 'invalid_map_path');

    // Убеждаемся, что таблица tw_items_list не пострадала от SQLi-попыток (5 записей)
    const itemsAfterSqli = await callApi('api/ai.php', { query: { action: 'list', resource: 'items' } });
    assert.equal(itemsAfterSqli.body.total, 5);

    // 7. Изоляция доступа к scenarios для гостя БЕЗ вкладки scenarios (g2)
    for (const scCall of [
      { query: { action: 'list', resource: 'scenarios' } },
      { query: { action: 'get', resource: 'scenarios', id: 'tutorial_data.json' } },
      { method: 'POST', query: { action: 'preview' }, body: { operations: [{ type: 'create', resource: 'scenarios', filename: 'x.json', data: { content: { steps: [] } } }] } },
      { method: 'POST', query: { action: 'propose' }, body: { message: 'Нельзя писать сценарии', operations: [{ type: 'create', resource: 'scenarios', filename: 'x.json', data: { content: { steps: [] } } }] } },
    ]) {
      const r = await callApi('api/ai.php', { role: 'g2', ...scCall });
      assert.equal(r.status, 403, `гость без scenarios должен получать 403: ${JSON.stringify(scCall)}`);
      assert.equal(r.body.code, 'resource_forbidden');
    }
    const g2ValSc = await callApi('api/ai.php', {
      role: 'g2', method: 'POST', query: { action: 'validate' },
      body: { operations: [{ type: 'create', resource: 'scenarios', filename: 'x.json', data: { content: { steps: [] } } }] },
    });
    assert.equal(g2ValSc.status, 200);
    assert.equal(g2ValSc.body.valid, false);
    assert.equal(g2ValSc.body.errors[0].code, 'resource_forbidden');

    // 7b. analyze / similar / apply_patch (read-only, ничего не изменяют)
    const corpusAnalyze = await callApi('api/ai.php', { query: { action: 'analyze', resource: 'scenarios' } });
    assert.equal(corpusAnalyze.status, 200, JSON.stringify(corpusAnalyze.body));
    assert.equal(corpusAnalyze.body.ok, true);
    assert.equal(corpusAnalyze.body.mode, 'corpus');
    assert.ok(corpusAnalyze.body.scanned >= 3, 'каталог должен содержать минимум 3 валидных JSON сценариев');
    assert.deepEqual(corpusAnalyze.body.invalid, []);
    const msgCompCorpus = corpusAnalyze.body.components.find((c) => c.type === 'message');
    assert.ok(msgCompCorpus && msgCompCorpus.scenarios >= 2, 'гистограмма компонентов должна учитывать tutorial_data.json и 4errerer.json');
    const legacyComp = corpusAnalyze.body.components.find((c) => c.type === 'universal_teleport');
    assert.ok(legacyComp && legacyComp.status === 'legacy', 'устаревшие типы должны помечаться status=legacy');
    const knownComp = corpusAnalyze.body.components.find((c) => c.type === 'message');
    assert.equal(knownComp.status, 'known');
    assert.ok(corpusAnalyze.body.missing_total >= 3, 'несуществующие item/quest ID должны попасть в missing_references');
    assert.ok(corpusAnalyze.body.missing_references.every((r) => r.filename && r.target && r.id > 0));
    assert.equal(corpusAnalyze.body.dependency_ok, false);

    const singleAnalyze = await callApi('api/ai.php', { query: { action: 'analyze', resource: 'scenarios', id: 'tutorial_data.json' } });
    assert.equal(singleAnalyze.status, 200, JSON.stringify(singleAnalyze.body));
    assert.equal(singleAnalyze.body.mode, 'scenario');
    assert.equal(singleAnalyze.body.filename, 'tutorial_data.json');
    assert.equal(singleAnalyze.body.sha256, singleAnalyze.body.baseHash);
    assert.ok(singleAnalyze.body.component_total > 10);
    const msgComp = singleAnalyze.body.components.find((c) => c.type === 'message');
    assert.ok(msgComp && msgComp.count >= 5 && msgComp.status === 'known');
    assert.ok(singleAnalyze.body.missing_references.some((r) => r.target === 'tw_items_list' && r.id === 40));
    assert.ok(singleAnalyze.body.missing_references.some((r) => r.target === 'tw_quests_list' && r.id === 1));
    const depItems = singleAnalyze.body.dependencies.find((d) => d.table === 'tw_items_list');
    assert.ok(depItems && depItems.missing >= 2 && depItems.references >= 2);
    assert.equal(singleAnalyze.body.dependency_ok, false);
    assert.ok(Array.isArray(singleAnalyze.body.inbound));
    assert.ok(Array.isArray(singleAnalyze.body.reuse));

    const analyzeMissing = await callApi('api/ai.php', { query: { action: 'analyze', resource: 'scenarios', id: 'no_such_file.json' } });
    assert.equal(analyzeMissing.status, 404);
    assert.equal(analyzeMissing.body.code, 'scenario_not_found');

    const analyzeWrongResource = await callApi('api/ai.php', { query: { action: 'analyze', resource: 'items' } });
    assert.equal(analyzeWrongResource.status, 400);
    assert.equal(analyzeWrongResource.body.code, 'unsupported_resource');

    const unknownAnalyze = await callApi('api/ai.php', { role: 'g2', query: { action: 'analyze', resource: 'scenarios' } });
    assert.equal(unknownAnalyze.status, 403);
    assert.equal(unknownAnalyze.body.code, 'resource_forbidden');

    // similar: точное совпадение Iron Ore
    const similarExact = await callApi('api/ai.php', { query: { action: 'similar', resource: 'items', name: 'Iron Ore' } });
    assert.equal(similarExact.status, 200, JSON.stringify(similarExact.body));
    assert.equal(similarExact.body.ok, true);
    assert.equal(similarExact.body.candidates[0].label, 'Iron Ore');
    assert.ok(similarExact.body.candidates[0].score > 0.9);
    assert.equal(similarExact.body.candidates[0].score_percent, Math.round(similarExact.body.candidates[0].score * 100));

    const similarTypo = await callApi('api/ai.php', { query: { action: 'similar', resource: 'items', name: 'Iron Or' } });
    assert.equal(similarTypo.status, 200);
    assert.ok(similarTypo.body.candidates.some((c) => c.label === 'Iron Ore'), JSON.stringify(similarTypo.body));

    const similarAgainstMissing = await callApi('api/ai.php', { query: { action: 'similar', resource: 'items', name: 'Zzyzx Nonexistent Xylophone' } });
    assert.equal(similarAgainstMissing.status, 200);
    assert.deepEqual(similarAgainstMissing.body.candidates, []);

    const similarNoName = await callApi('api/ai.php', { query: { action: 'similar', resource: 'items' } });
    assert.equal(similarNoName.status, 400);
    assert.equal(similarNoName.body.code, 'missing_required_field');

    const similarUnknownRes = await callApi('api/ai.php', { query: { action: 'similar', resource: 'no_such_table', name: 'X' } });
    assert.equal(similarUnknownRes.status, 404);
    assert.equal(similarUnknownRes.body.code, 'unknown_resource');

    // похожие записи предупреждаются автоматически при validate create
    const dupVal = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'validate' },
      body: { operations: [{ type: 'create', resource: 'items', data: { Name: 'Iron Ore', Group: 'Resource', Type: 'Default' } }] },
    });
    assert.equal(dupVal.status, 200);
    assert.equal(dupVal.body.valid, true);
    const dupWarning = dupVal.body.warnings.find((w) => w.code === 'similar_records_found');
    assert.ok(dupWarning, JSON.stringify(dupVal.body.warnings));
    assert.ok(dupWarning.similar.some((c) => c.label === 'Iron Ore'));

    // apply_patch: сценарий — read-only проверка и noop для совпадающего значения
    const emptySc = await callApi('api/ai.php', { query: { action: 'get', resource: 'scenarios', id: 'empty.json', summary_only: '1' } });
    assert.equal(emptySc.status, 200);
    const emptyHash = emptySc.body.sha256;
    const apScenario = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'apply_patch' },
      body: { resource: 'scenarios', filename: 'empty.json', patch_json: { scenario_mode: 'default' }, baseHash: emptyHash },
    });
    assert.equal(apScenario.status, 200, JSON.stringify(apScenario.body));
    assert.equal(apScenario.body.ok, true);
    assert.equal(apScenario.body.noop, true);
    assert.deepEqual(apScenario.body.diff, []);
    assert.equal(apScenario.body.sha256_before, emptyHash);
    assert.ok(!php.fileExists('/studio/scenarios/empty.json.bak'));

    const apScenarioChange = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'apply_patch' },
      body: { resource: 'scenarios', filename: 'empty.json', patch_json: { scenario_mode: 'universal' }, baseHash: emptyHash },
    });
    assert.equal(apScenarioChange.status, 200, JSON.stringify(apScenarioChange.body));
    assert.equal(apScenarioChange.body.noop, false);
    assert.ok(apScenarioChange.body.changed_fields >= 1);
    assert.equal(apScenarioChange.body.after.scenario_mode, 'universal');
    assert.ok(!php.fileExists('/studio/scenarios/empty.json.bak'));
    const emptyScAfter = await callApi('api/ai.php', { query: { action: 'get', resource: 'scenarios', id: 'empty.json' } });
    assert.equal(emptyScAfter.body.content.scenario_mode, 'default', 'apply_patch не должен изменять файл сценария');

    // apply_patch: несовпадающий baseHash -> 409 base_hash_mismatch (как propose)
    const apStale = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'apply_patch' },
      body: { resource: 'scenarios', filename: 'empty.json', patch_json: { scenario_mode: 'universal' }, baseHash: '0'.repeat(64) },
    });
    assert.equal(apStale.status, 409);
    assert.equal(apStale.body.code, 'base_hash_mismatch');

    // apply_patch: сценарий с битыми ссылками отдаёт те же ошибки, что propose (missing_reference)
    const apBroken = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'apply_patch' },
      body: { resource: 'scenarios', filename: 'tutorial_data.json', patch_json: {}, baseHash: getScSummaryOnly.body.sha256 },
    });
    assert.equal(apBroken.status, 400);
    assert.equal(apBroken.body.valid, false);
    assert.equal(apBroken.body.code, 'missing_reference');
    assert.ok(apBroken.body.errors.some((e) => e.field.includes('quest_id')));

    // apply_patch: невалидный сценарный патч -> те же коды ошибок, что и propose
    const apInvalid = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'apply_patch' },
      body: { resource: 'scenarios', filename: 'empty.json', patch_json: { steps: 'not-an-array' }, baseHash: emptyHash },
    });
    assert.equal(apInvalid.status, 400);
    assert.equal(apInvalid.body.valid, false);
    assert.ok(apInvalid.body.code);

    // apply_patch: БД-ресурс — серверный merge + попольный diff, без записи в БД
    const apItem = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'apply_patch' },
      body: { resource: 'items', id: 1, patch_json: { Data: { use_scenario: { name: 'tutorial_data.json', world_id: 0 }, bonus: { strength: 8 } } }, expected_before: { Name: 'Iron Ore' } },
    });
    assert.equal(apItem.status, 200, JSON.stringify(apItem.body));
    assert.equal(apItem.body.ok, true);
    assert.equal(apItem.body.changed_fields, 1);
    const dataDiff = apItem.body.diff.find((d) => d.column === 'Data');
    assert.ok(dataDiff && dataDiff.json === true);
    assert.equal(apItem.body.record.Data.bonus.strength, 8);
    const itemAfter = await callApi('api/ai.php', { query: { action: 'get', resource: 'items', id: '1' } });
    assert.equal(itemAfter.body.record.Data.bonus, undefined, 'apply_patch не должен изменять БД');

    // apply_patch: expected_before mismatch -> 409
    const apConflict = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'apply_patch' },
      body: { resource: 'items', id: 1, patch_json: { Data: { bonus: { strength: 1 } } }, expected_before: { Name: 'Someone Else' } },
    });
    assert.equal(apConflict.status, 409, JSON.stringify(apConflict.body));
    assert.equal(apConflict.body.code, 'version_conflict');

    // apply_patch: невалидный вход
    const apBadPatch = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'apply_patch' },
      body: { resource: 'items', id: 1, patch_json: 'not-an-object' },
    });
    assert.equal(apBadPatch.status, 400);
    assert.equal(apBadPatch.body.code, 'invalid_patch_json');

    const apReadonlyScenario = await callApi('api/ai.php', { role: 'g2', method: 'POST', query: { action: 'apply_patch' }, body: { resource: 'scenarios', filename: 'tutorial_data.json', patch_json: {} } });
    assert.equal(apReadonlyScenario.status, 403);
    assert.equal(apReadonlyScenario.body.code, 'resource_forbidden');

    // 8. Валидация JSON/структуры объектов + превышение max_operations
    const valDomainErrors = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'validate' },
      body: {
        operations: [
          { type: 'create', resource: 'items', data: { Name: 'Broken JSON', Group: 'Resource', Type: 'Default', Data: '{not_json' } },
          { type: 'create', resource: 'quests', data: { Name: 'Bad Flags', Flags: 'Tutorial,Type main' } },
          { type: 'create', resource: 'scenarios', filename: 'bad_struct.json', data: { content: 'not-an-object' } },
          { type: 'create', resource: 'scenarios', filename: 'bad_delay.json', data: { content: { steps: [{ id: 's1', components: [{ type: 'message', text: 'x', delay: -1 }] }] } } },
        ],
      },
    });
    assert.equal(valDomainErrors.status, 200);
    assert.equal(valDomainErrors.body.valid, false);
    const errCodes = valDomainErrors.body.errors.map((e) => e.code);
    assert.ok(errCodes.includes('invalid_json'), errCodes.join(','));
    assert.ok(errCodes.includes('quest_flags_conflict'), errCodes.join(','));
    assert.ok(errCodes.includes('out_of_range'), errCodes.join(','));

    const tooMany = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'preview' },
      body: {
        operations: Array.from({ length: 5 }, (_, i) => ({
          type: 'create', resource: 'items', data: { Name: `Bulk ${i}` },
        })),
      },
    });
    assert.equal(tooMany.status, 400);
    assert.equal(tooMany.body.code, 'too_many_operations');

    // 9. Bulk-операция (создание items с tempId + создание scenario с этим tempId + обновление существующего scenario по baseHash)
    const tutHash = getScSummaryOnly.body.sha256;
    const bulkBody = {
      message: 'Пакетное создание ключа и привязка сценариев',
      operations: [
        {
          type: 'create', resource: 'items', tempId: 2000000077,
          data: { Name: 'Citadel Key', Description: 'Мастер-ключ', Group: 'Quest', Type: 'Default' },
        },
        {
          type: 'create', resource: 'scenarios', filename: 'bulk_scene.json',
          data: { content: { scenario_mode: 'default', steps: [{ id: 's1', components: [{ type: 'player_give_item', item_id: 2000000077, value: 1 }] }] } },
        },
      ],
    };
    const propBulk = await callApi('api/ai.php', { method: 'POST', query: { action: 'propose' }, body: bulkBody });
    assert.equal(propBulk.status, 200, JSON.stringify(propBulk.body));
    const bulkReqId = propBulk.body.request.id;

    // Проверка устаревшего baseHash при параллельном изменении файла сценария
    const staleBase = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'preview' },
      body: {
        operations: [
          {
            type: 'update', resource: 'scenarios', filename: 'tutorial_data.json',
            baseHash: '0'.repeat(64),
            data: { content: { scenario_mode: 'default', steps: [{ id: 's1', components: [] }] } },
          },
        ],
      },
    });
    assert.equal(staleBase.status, 409, JSON.stringify(staleBase.body));
    assert.equal(staleBase.body.code, 'base_hash_mismatch');
    assert.notEqual('0'.repeat(64), tutHash);

    // Одобрение мульти-ресурсной заявки администратором: реальный ID из tw_items_list должен подставиться в bulk_scene.json!
    const approveBulk = await callApi('api/change-requests.php', {
      method: 'POST', role: 'admin', query: { action: 'approve' },
      body: { id: bulkReqId, version: 1, comment: 'Одобряем пакет' },
    });
    assert.equal(approveBulk.status, 200, JSON.stringify(approveBulk.body));
    assert.ok(php.fileExists('/studio/scenarios/bulk_scene.json'));
    const writtenBulkScene = JSON.parse(php.readFileAsText('/studio/scenarios/bulk_scene.json'));
    const realItemId = writtenBulkScene.steps[0].components[0].item_id;
    assert.equal(realItemId, 6, `tempId 2000000077 должен быть заменён на реальный ID 6, получено: ${realItemId}`);

    // Попытка гостя изменить уже одобренную заявку -> 409 request_not_pending
    const reviseApproved = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'propose' },
      body: { request_id: bulkReqId, version: 1, operations: [{ type: 'update', resource: 'items', id: 1, data: { Name: 'AfterApprove' } }] },
    });
    assert.equal(reviseApproved.status, 409);
    assert.equal(reviseApproved.body.code, 'request_not_pending');

    // Конфликт одновременного редактирования в БД: заявка на update item #1, затем прямое изменение item #1 перед approve
    const updDraft = await callApi('api/ai.php', {
      method: 'POST', query: { action: 'propose' },
      body: {
        message: 'Переименовать предмет 1',
        operations: [{ type: 'update', resource: 'items', id: 1, data: { Name: 'Guest Ore' } }],
      },
    });
    assert.equal(updDraft.status, 200);
    const conflictReqId = updDraft.body.request.id;

    // Параллельное изменение item #1 напрямую админом в БД
    await php.run({
      code: `<?php
        $pdoGame = new PDO('sqlite:/studio/data/game.sqlite');
        $pdoGame->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        $pdoGame->exec("UPDATE tw_items_list SET Name = 'Admin Changed Ore' WHERE ID = 1");
      `,
    });
    const adminViewConflict = await callApi('api/change-requests.php', {
      role: 'admin', query: { action: 'get', id: conflictReqId },
    });
    assert.equal(adminViewConflict.status, 200);
    assert.ok(Array.isArray(adminViewConflict.body.request.conflicts) && adminViewConflict.body.request.conflicts.length > 0);

    // Попытка approve без force -> 409
    const approveConflict = await callApi('api/change-requests.php', {
      method: 'POST', role: 'admin', query: { action: 'approve' },
      body: { id: conflictReqId, version: 1, comment: 'check conflict', force: false },
    });
    assert.equal(approveConflict.status, 409, JSON.stringify(approveConflict.body));

    // Гость может отозвать свою ожидающую заявку (withdraw) и проверить список (mine)
    const mineBefore = await callApi('api/change-requests.php', { query: { action: 'mine' } });
    assert.equal(mineBefore.status, 200);
    assert.ok(mineBefore.body.rows.some((r) => r.id === conflictReqId));

    const withdrawOk = await callApi('api/change-requests.php', {
      method: 'POST', query: { action: 'withdraw' }, body: { id: conflictReqId },
    });
    assert.equal(withdrawOk.status, 200);
    assert.equal(withdrawOk.body.request.status, 'withdrawn');
  } finally {
    try { php.exit(0); } catch {}
  }
});
