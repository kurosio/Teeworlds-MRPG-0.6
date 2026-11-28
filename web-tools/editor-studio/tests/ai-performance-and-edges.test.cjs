'use strict';

/**
 * tests/ai-performance-and-edges.test.cjs — Regression suite for AI subsystem
 * optimizations, SQL query budgets, caching & ACL isolation, BFS geometry edges,
 * scenario file cache invalidation, renderer robustness, and mcp-stdio framing.
 *
 * Covers:
 *  1. SQL query budgets & batching across AI API hot paths (anti-N+1 regression guard)
 *  2. Scope-aware ai_allowed_resources() cache & live-mutation freshness of ai_resolve_targets_batch()
 *  3. Map static analysis RAM + DISK cache equivalence, speed budget, invalidation & ACL isolation
 *  4. Regression test for BFS horizontal wrap-around on synthetic DDNet v4 map + RLE helpers
 *  5. scenario-files.php file parse cache invalidation (create/update/delete/rollback) & malformed inputs
 *  6. map-render.php edge viewports & tools/mcp-stdio.mjs malformed/non-JSON response safety
 */

const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { spawn } = require('node:child_process');

const ROOT = path.join(__dirname, '..');
const REPO = path.join(ROOT, '..', '..');

let loadNodeRuntime = null;
let PHP = null;
try {
  ({ loadNodeRuntime } = require('@php-wasm/node'));
  ({ PHP } = require('@php-wasm/universal'));
} catch {
  // Optional php-wasm dependency handled by skip below.
}

function mountStudio(php, extraMaps = []) {
  php.mkdir('/studio');
  php.mkdir('/studio/api');
  php.mkdir('/studio/editor-core');
  php.mkdir('/studio/scenarios');
  php.mkdir('/studio/maps');
  php.writeFile(
    '/studio/editor-core/reference-schema.json',
    fs.readFileSync(path.join(ROOT, 'editor-core/reference-schema.json'))
  );
  for (const name of fs.readdirSync(path.join(ROOT, 'api'))) {
    const full = path.join(ROOT, 'api', name);
    if (fs.statSync(full).isFile()) {
      php.writeFile('/studio/api/' + name, fs.readFileSync(full));
    }
  }
  for (const [alias, relPath] of extraMaps) {
    const abs = path.join(REPO, relPath);
    if (fs.existsSync(abs)) {
      php.writeFile('/studio/maps/' + alias, fs.readFileSync(abs));
    }
  }
}

const COUNTING_MYSQLI_PHP = `
if (!class_exists('CountingMysqliResult')) {
class CountingMysqliResult extends mysqli_result {
    private array $rows;
    private int $pos = 0;
    public function __construct(array $rows) { $this->rows = array_values($rows); }
    public function fetch_assoc(): ?array { return $this->rows[$this->pos++] ?? null; }
    public function fetch_row(): ?array {
        $r = $this->fetch_assoc();
        return $r === null ? null : array_values($r);
    }
}
class CountingMysqliStmt extends mysqli_stmt {
    private PDO $pdo;
    private string $sql;
    private array $params = [];
    private CountingMysqli $db;
    public function __construct(PDO $pdo, string $sql, CountingMysqli $db) {
        $this->pdo = $pdo;
        $this->sql = $sql;
        $this->db = $db;
    }
    public function bind_param(string $types, mixed &...$vars): bool {
        $this->params = $vars;
        return true;
    }
    public function execute(?array $params = null): bool {
        if ($params !== null) $this->params = $params;
        $this->db->queryCount++;
        $this->db->queries[] = $this->sql;
        return true;
    }
    public function get_result(): mysqli_result|false {
        $s = $this->sql;
        if (stripos($s, 'information_schema.tables') !== false) {
            $st = $this->pdo->prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name = ? LIMIT 1");
            $st->execute([$this->params[0] ?? '']);
            return new CountingMysqliResult($st->fetchAll(PDO::FETCH_ASSOC));
        }
        $s = str_replace(chr(96), '"', $s);
        $s = preg_replace('/CAST\\(([^)]+) AS CHAR\\)/i', 'CAST($1 AS TEXT)', $s);
        $st = $this->pdo->prepare($s);
        $st->execute($this->params);
        return new CountingMysqliResult($st->fetchAll(PDO::FETCH_ASSOC));
    }
    public function close(): true { return true; }
}
class CountingMysqli extends mysqli {
    public PDO $pdo;
    public int $queryCount = 0;
    public array $queries = [];
    public function __construct(PDO $pdo) { $this->pdo = $pdo; }
    public function prepare(string $query): mysqli_stmt|false {
        return new CountingMysqliStmt($this->pdo, $query, $this);
    }
    public function query(string $query, int $result_mode = MYSQLI_STORE_RESULT): mysqli_result|bool {
        $this->queryCount++;
        $this->queries[] = $query;
        $s = str_replace(chr(96), '"', $query);
        $s = preg_replace('/CAST\\(([^)]+) AS CHAR\\)/i', 'CAST($1 AS TEXT)', $s);
        $st = $this->pdo->query($s);
        return new CountingMysqliResult($st ? $st->fetchAll(PDO::FETCH_ASSOC) : []);
    }
}
}
`;

test('AI API SQL budgets, batching (anti-N+1), scope-aware cache & live mutation freshness', async (t) => {
  if (!loadNodeRuntime || !PHP) {
    t.skip('@php-wasm/node is not installed');
    return;
  }

  const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(runtime);
  try {
    mountStudio(php);
    const res = await php.run({
      code: `<?php
require_once '/studio/api/db-core.php';
require_once '/studio/api/db-resources.php';
require_once '/studio/api/change-requests-lib.php';
require_once '/studio/api/scenario-files.php';
require_once '/studio/api/ai-schema.php';
require_once '/studio/api/ai-lib.php';
${COUNTING_MYSQLI_PHP}

$pdo = new PDO('sqlite::memory:');
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$pdo->exec('CREATE TABLE tw_worlds (ID INTEGER PRIMARY KEY, Name TEXT, Path TEXT, Type TEXT, Flags TEXT, RespawnWorldID INT, JailWorldID INT, RequiredLevel INT)');
$pdo->exec('CREATE TABLE tw_items_list (ID INTEGER PRIMARY KEY, Name TEXT, Description TEXT, "Group" TEXT, Type TEXT, InitialPrice INT, Data TEXT)');
$pdo->exec('CREATE TABLE tw_quests_list (ID INTEGER PRIMARY KEY, Name TEXT, Money INT, Gold INT, Exp INT, Flags TEXT, StoryLine TEXT, NextQuestID INT, Data TEXT)');
$pdo->exec('CREATE TABLE tw_bots_info (ID INTEGER PRIMARY KEY, Name TEXT, SlotWeapon INT, SlotHammer INT, SlotGun INT, SlotShotgun INT, SlotGrenade INT, SlotRifle INT, SlotArmor INT, SlotEidolon INT)');
$pdo->exec('CREATE TABLE tw_bots_npc (ID INTEGER PRIMARY KEY, BotID INT, WorldID INT, PosX INT, PosY INT, GiveQuestID INT, Function INT, Static INT, Emote INT, DialogData TEXT)');
$pdo->exec('CREATE TABLE tw_bots_quest (ID INTEGER PRIMARY KEY, BotID INT, QuestID INT, Step INT, WorldID INT, PosX INT, PosY INT, AutoFinish INT, DialogData TEXT, ScenarioData TEXT, TasksData TEXT)');
$pdo->exec('CREATE TABLE tw_bots_mobs (ID INTEGER PRIMARY KEY, BotID INT, WorldID INT, PositionX INT, PositionY INT, Level INT, Power INT, Number INT, Boss INT, Radius INT, it_drop_0 INT, it_drop_1 INT, it_drop_2 INT, it_drop_3 INT, it_drop_4 INT, it_drop_count TEXT, it_drop_chance TEXT)');
$pdo->exec('CREATE TABLE tw_crafts_list (ID INTEGER PRIMARY KEY, GroupName TEXT, ItemID INT, ItemValue INT, RequiredItems TEXT, Price INT, WorldID INT)');
$pdo->exec('CREATE TABLE tw_warehouses (ID INTEGER PRIMARY KEY, Name TEXT, Type TEXT, WorldID INT, PosX INT, PosY INT, Currency INT, Trades TEXT)');
$pdo->exec('CREATE TABLE tw_nodes_collecting (ID INTEGER PRIMARY KEY, SwitchID INT, Level INT, Health INT, Name TEXT, Properties TEXT, Type TEXT, WorldID INT)');
$pdo->exec('CREATE TABLE tw_quest_boards (ID INTEGER PRIMARY KEY, Name TEXT, WorldID INT, PosX INT, PosY INT)');
$pdo->exec('CREATE TABLE tw_quests_board_list (ID INTEGER PRIMARY KEY, DailyBoardID INT, QuestID INT)');
$pdo->exec('CREATE TABLE tw_aethers (ID INTEGER PRIMARY KEY, Name TEXT, WorldID INT, TeleX INT, TeleY INT)');
$pdo->exec('CREATE TABLE tw_dungeons (ID INTEGER PRIMARY KEY, Level INT, WorldID INT, DoorX INT, DoorY INT, TimeLimit INT)');
$pdo->exec('CREATE TABLE tw_attributes (ID INTEGER PRIMARY KEY, Name TEXT, Price INT, "Group" TEXT)');
$pdo->exec('CREATE TABLE tw_skills_list (ID INTEGER PRIMARY KEY, Name TEXT)');

for ($i = 0; $i < 5; $i++) $pdo->exec("INSERT INTO tw_worlds VALUES ($i, 'World $i', 'main.map', 'default', '', 0, 0, 1)");
for ($i = 1; $i <= 300; $i++) {
    $n = ($i % 5 === 0) ? "Iron Sword $i" : "Mystic Relic $i";
    $d = ($i === 1) ? '{"use_scenario":{"name":"sc_1.json"}}' : '{}';
    $pdo->exec("INSERT INTO tw_items_list VALUES ($i, '$n', 'Desc $i', 'Resource', 'Default', " . ($i * 10) . ", '$d')");
}
for ($i = 1; $i <= 60; $i++) $pdo->exec("INSERT INTO tw_quests_list VALUES ($i, 'Quest Iron $i', 100, 0, 50, '', 'Main', " . ($i < 60 ? $i + 1 : 0) . ", '{}')");
for ($i = 1; $i <= 80; $i++) $pdo->exec("INSERT INTO tw_bots_info VALUES ($i, 'Bot $i', " . (($i % 10) + 1) . ", 0, 0, 0, 0, 0, 0, 0)");
for ($i = 1; $i <= 80; $i++) $pdo->exec("INSERT INTO tw_bots_npc VALUES ($i, $i, " . ($i % 5) . ", 100, 100, " . (($i % 60) + 1) . ", 0, 0, 0, '[]')");
for ($i = 1; $i <= 120; $i++) $pdo->exec("INSERT INTO tw_bots_quest VALUES ($i, " . (($i % 80) + 1) . ", " . (($i % 60) + 1) . ", " . (($i % 3) + 1) . ", " . ($i % 5) . ", 120, 120, 0, '[]', '', '{}')");
for ($i = 1; $i <= 100; $i++) $pdo->exec("INSERT INTO tw_crafts_list VALUES ($i, 'Weapons', " . (($i % 300) + 1) . ", 1, '[1/2],[2/3]', 50, 0)");
for ($i = 1; $i <= 30; $i++) $pdo->exec("INSERT INTO tw_bots_mobs VALUES ($i, " . (($i % 80) + 1) . ", 0, 200, 200, 5, 10, 3, 0, 200, 1, 2, 0, 0, 0, '|1|1|0|0|0|', '|10|5|0|0|0|')");
for ($i = 1; $i <= 10; $i++) $pdo->exec("INSERT INTO tw_attributes VALUES ($i, 'Attr $i', 10, 'Main')");
for ($i = 1; $i <= 10; $i++) $pdo->exec("INSERT INTO tw_skills_list VALUES ($i, 'Skill $i')");

for ($s = 1; $s <= 10; $s++) {
    $comps = [
        ['type' => 'message', 'text' => 'Hi', 'bot_id' => $s],
        ['type' => 'check_has_item', 'item_id' => $s],
        ['type' => 'quest_condition', 'quest_id' => $s],
        ['type' => 'teleport', 'world_id' => 0],
    ];
    if ($s === 10) {
        $comps[] = ['type' => 'check_has_item', 'item_id' => 99999]; // intentional broken ref
    }
    file_put_contents("/studio/scenarios/sc_$s.json", json_encode([
        'scenario_mode' => 'default',
        'steps' => [['id' => 'start', 'components' => $comps]],
    ]));
}

$db = new CountingMysqli($pdo);
// Warm ai_table_exists static cache once (as happens on first request).
foreach ([
    'tw_worlds','tw_items_list','tw_quests_list','tw_bots_info','tw_bots_npc',
    'tw_bots_quest','tw_bots_mobs','tw_crafts_list','tw_warehouses','tw_nodes_collecting',
    'tw_quest_boards','tw_quests_board_list','tw_aethers','tw_dungeons','tw_attributes',
    'tw_skills_list','tw_vouchers','tw_houses','tw_guilds_houses','tw_worlds_swaps','tw_quests_daily_board'
] as $tbl) {
    ai_table_exists($db, $tbl);
}

$measure = static function (callable $fn) use ($db): array {
    $q0 = $db->queryCount;
    $out = $fn();
    return ['sql' => $db->queryCount - $q0, 'out' => $out];
};

// 1. Global search (uses skip_count + LIMIT+1 instead of COUNT(*)+SELECT).
$gs = $measure(fn() => ai_global_search($db, 'Iron', 'admin', null));

// 2. References inspect (item + scenario).
$refItem = $measure(fn() => ai_references_inspect($db, 'items', 1, 'admin', null, '/studio/scenarios'));
$refScen = $measure(fn() => ai_references_inspect($db, 'scenarios', 'sc_1.json', 'admin', null, '/studio/scenarios'));

// 3. Entity context (reuses prefetched row inside ai_references_inspect).
$ctxItem = $measure(fn() => ai_build_entity_context($db, 'items', 1, 'admin', null));

// 4. Scenario analysis (single + 10-file corpus with 41 refs).
$scOne = $measure(fn() => ai_analyze_scenario($db, 'sc_1.json', 'admin', null, '/studio/scenarios'));
$scCorpus = $measure(fn() => ai_analyze_scenario_corpus($db, 'admin', null, '/studio/scenarios'));

// 5. Batch validation: 25 item creates (preindexed similarity check -> 1 SQL).
$opsItems = [];
for ($i = 1; $i <= 25; $i++) {
    $opsItems[] = [
        'type' => 'create',
        'resource' => 'items',
        'tempId' => 2000000000 + $i,
        'data' => ['Name' => "Iron Sword $i", 'Description' => 'd', 'Group' => 'Resource', 'Type' => 'Default', 'InitialPrice' => 50],
    ];
}
$valItems = $measure(fn() => ai_validate_operations($db, $opsItems, 'admin', null, '/studio/scenarios', 100));

// 6. Batch validation: 25 crafts with 100 FKs including 1 broken FK -> batched IN(...) per table.
$opsCrafts = [];
for ($i = 1; $i <= 25; $i++) {
    $itemRef = ($i === 25) ? 99999 : $i;
    $opsCrafts[] = [
        'type' => 'create',
        'resource' => 'crafts',
        'tempId' => 2000000100 + $i,
        'data' => [
            'GroupName' => 'Weapons',
            'ItemID' => $itemRef,
            'ItemValue' => 1,
            'RequiredItems' => '[' . ($i + 10) . '/2],[' . ($i + 20) . '/1]',
            'Price' => 100,
            'WorldID' => ($i % 5),
        ],
    ];
}
$valCrafts = $measure(fn() => ai_validate_operations($db, $opsCrafts, 'admin', null, '/studio/scenarios', 100));

// 7. scenario_validate_content with 30 item FKs (2 broken) -> 1 batched IN(...) query!
$scComps = [];
for ($i = 1; $i <= 28; $i++) $scComps[] = ['type' => 'check_has_item', 'item_id' => $i];
$scComps[] = ['type' => 'check_has_item', 'item_id' => 88881];
$scComps[] = ['type' => 'check_has_item', 'item_id' => 88882];
$valScen = $measure(fn() => scenario_validate_content([
    'scenario_mode' => 'default',
    'steps' => [['id' => 's1', 'components' => $scComps]],
], $db));

// 8. Role/guestPages/scope isolation of ai_allowed_resources() & ai_build_manifest() caches.
$resGuestItems = ai_allowed_resources('guest', ['items'], false);
$resGuestQuests = ai_allowed_resources('guest', ['quests'], false);
$GLOBALS['EDITOR_TOKEN_ACTOR'] = ['scopes' => ['read']];
$manRead = ai_build_manifest('admin', null, [], null, false);
$GLOBALS['EDITOR_TOKEN_ACTOR'] = ['scopes' => ['read', 'propose']];
$manPropose = ai_build_manifest('admin', null, [], null, false);
unset($GLOBALS['EDITOR_TOKEN_ACTOR']);

// 9. Freshness of ai_resolve_targets_batch() across DB mutations (no stale static cache).
$beforeMut = ai_resolve_targets_batch($db, [['target' => 'tw_items_list', 'id' => 301]], 'admin', null);
$pdo->exec("INSERT INTO tw_items_list VALUES (301, 'Fresh Sword', 'New', 'Resource', 'Default', 100, '{}')");
$afterInsert = ai_resolve_targets_batch($db, [['target' => 'tw_items_list', 'id' => 301]], 'admin', null);
$pdo->exec('DELETE FROM tw_items_list WHERE ID = 301');
$afterDelete = ai_resolve_targets_batch($db, [['target' => 'tw_items_list', 'id' => 301]], 'admin', null);

// 10. ai_schema_* memoization stability.
$m1 = ai_schema_tables();
$m2 = ai_schema_tables();
$l1 = ai_schema_lookups();
$l2 = ai_schema_lookups();

echo json_encode([
    'gs_sql' => $gs['sql'],
    'gs_matches' => count($gs['out']['top_matches'] ?? []),
    'ref_item_sql' => $refItem['sql'],
    'ref_item_inbound' => count($refItem['out']['inbound'] ?? []),
    'ref_scen_sql' => $refScen['sql'],
    'ref_scen_outbound' => count($refScen['out']['outbound'] ?? []),
    'ref_scen_inbound_ids' => array_map(fn($r) => (int)($r['record_id'] ?? 0), $refScen['out']['inbound'] ?? []),
    'ctx_item_sql' => $ctxItem['sql'],
    'ctx_item_ok' => (($ctxItem['out']['resource'] ?? '') === 'items' && (int)($ctxItem['out']['id'] ?? 0) === 1),
    'sc_one_sql' => $scOne['sql'],
    'sc_one_broken' => count($scOne['out']['missing_references'] ?? []),
    'sc_corpus_sql' => $scCorpus['sql'],
    'sc_corpus_files' => (int)($scCorpus['out']['total_files'] ?? 0),
    'sc_corpus_broken' => (int)($scCorpus['out']['missing_total'] ?? 0),
    'val_items_sql' => $valItems['sql'],
    'val_items_valid' => $valItems['out']['valid'],
    'val_items_warns' => count($valItems['out']['warnings']),
    'val_crafts_sql' => $valCrafts['sql'],
    'val_crafts_valid' => $valCrafts['out']['valid'],
    'val_crafts_err_codes' => array_map(fn($e) => $e['code'], $valCrafts['out']['errors']),
    'val_scen_sql' => $valScen['sql'],
    'val_scen_err_codes' => array_map(fn($e) => $e['code'], $valScen['out']['errors']),
    'guest_items_has_items' => isset($resGuestItems['items']),
    'guest_items_has_quests' => isset($resGuestItems['quests']),
    'guest_quests_has_quests' => isset($resGuestQuests['quests']),
    'guest_quests_has_items' => isset($resGuestQuests['items']),
    'man_read_scopes' => $manRead['auth']['token_scopes'] ?? [],
    'man_propose_scopes' => $manPropose['auth']['token_scopes'] ?? [],
    'mut_before' => $beforeMut['tw_items_list:301']['exists'] ?? null,
    'mut_insert' => $afterInsert['tw_items_list:301']['exists'] ?? null,
    'mut_insert_label' => $afterInsert['tw_items_list:301']['label'] ?? null,
    'mut_delete' => $afterDelete['tw_items_list:301']['exists'] ?? null,
    'schema_stable' => ($m1 === $m2 && $l1 === $l2 && count($m1) >= 15),
]);
`,
    });

    const text = res.text.trim();
    assert.ok(text.startsWith('{'), `PHP output was not JSON: ${text.slice(0, 600)}`);
    const data = JSON.parse(text);
    // SQL budget assertions (strict guards against N+1 regressions)
    assert.ok(data.gs_sql <= 22, `global_search SQL budget exceeded: ${data.gs_sql}`);
    assert.ok(data.gs_matches >= 10, 'global_search must still return real matches');
    assert.ok(data.ref_item_sql <= 16, `references(items) SQL budget exceeded: ${data.ref_item_sql}`);
    assert.ok(data.ref_item_inbound > 0, 'references(items) must find inbound refs via grouped OR queries');
    assert.ok(data.ref_scen_sql <= 8, `references(scenarios) SQL budget exceeded: ${data.ref_scen_sql}`);
    assert.equal(data.ref_scen_outbound, 4);
    assert.deepEqual(data.ref_scen_inbound_ids, [1]);
    assert.ok(data.ctx_item_sql <= 22, `context(items) SQL budget exceeded: ${data.ctx_item_sql}`);
    assert.equal(data.ctx_item_ok, true);
    assert.ok(data.sc_one_sql <= 7, `analyze_scenario(single) SQL budget exceeded: ${data.sc_one_sql}`);
    assert.equal(data.sc_one_broken, 0);
    assert.ok(data.sc_corpus_sql <= 6, `analyze_scenario(corpus 10 files / 41 refs) SQL budget exceeded: ${data.sc_corpus_sql}`);
    assert.equal(data.sc_corpus_files, 10);
    assert.equal(data.sc_corpus_broken, 1, 'corpus analysis must detect the 1 broken reference');
    assert.ok(data.val_items_sql <= 3, `validate(25 item creates) SQL budget exceeded: ${data.val_items_sql}`);
    assert.equal(data.val_items_valid, true);
    assert.equal(data.val_items_warns, 25, 'preindexed similarity check must warn on all 25 near-duplicate items');
    assert.ok(data.val_crafts_sql <= 5, `validate(25 crafts w/ 100 FKs) SQL budget exceeded: ${data.val_crafts_sql}`);
    assert.equal(data.val_crafts_valid, false);
    assert.deepEqual(data.val_crafts_err_codes, ['missing_reference']);
    assert.ok(data.val_scen_sql <= 2, `scenario_validate_content(30 FKs) SQL budget exceeded: ${data.val_scen_sql}`);
    assert.deepEqual(data.val_scen_err_codes, ['missing_reference', 'missing_reference']);

    // Role / guestPages / scope cache isolation
    assert.equal(data.guest_items_has_items, true);
    assert.equal(data.guest_items_has_quests, false);
    assert.equal(data.guest_quests_has_quests, true);
    assert.equal(data.guest_quests_has_items, false);
    assert.deepEqual(data.man_read_scopes, ['read']);
    assert.deepEqual(data.man_propose_scopes, ['read', 'propose']);

    // Live mutation freshness
    assert.equal(data.mut_before, false);
    assert.equal(data.mut_insert, true);
    assert.equal(data.mut_insert_label, 'Fresh Sword');
    assert.equal(data.mut_delete, false);
    assert.equal(data.schema_stable, true);
  } finally {
    php.exit(0);
  }
});

test('Map static cache (RAM + DISK equivalence, speed, invalidation, ACL isolation) & BFS horizontal wrap-around regression', async (t) => {
  if (!loadNodeRuntime || !PHP) {
    t.skip('@php-wasm/node is not installed');
    return;
  }

  const mainMapAbs = path.join(REPO, 'maps/main.map');
  if (!fs.existsSync(mainMapAbs)) {
    t.skip('maps/main.map not present');
    return;
  }

  const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(runtime);
  try {
    mountStudio(php, [
      ['main.map', 'maps/main.map'],
      ['ctf1.map', 'data/maps/ctf1.map'],
    ]);

    // Step A: cold vs RAM cache hit + ACL isolation + synthetic BFS wrap-around + RLE helper parity
    const res1 = await php.run({
      code: `<?php
require_once '/studio/api/db-core.php';
require_once '/studio/api/db-resources.php';
require_once '/studio/api/change-requests-lib.php';
require_once '/studio/api/scenario-files.php';
require_once '/studio/api/ai-schema.php';
require_once '/studio/api/ai-lib.php';
require_once '/studio/api/map-datafile.php';
require_once '/studio/api/map-analysis.php';
${COUNTING_MYSQLI_PHP}

// 1. Cold vs RAM cache hit exact equivalence on real main.map
$mainBytes = file_get_contents('/studio/maps/main.map');
$anCold = ai_map_analyze_full($mainBytes, null, null, 'admin', null, ['include_minimap' => true, 'no_cache' => true]);
$anPopulate = ai_map_analyze_full($mainBytes, null, null, 'admin', null, ['include_minimap' => true]);
$t0 = microtime(true);
$anRam = ai_map_analyze_full($mainBytes, null, null, 'admin', null, ['include_minimap' => true]);
$ramMs = (microtime(true) - $t0) * 1000;

$exactEq = (
    $anCold['sha256'] === $anRam['sha256']
    && $anCold['game_layer'] === $anRam['game_layer']
    && $anCold['terrain'] === $anRam['terrain']
    && $anCold['structures'] === $anRam['structures']
    && $anCold['zones'] === $anRam['zones']
    && $anCold['region_analysis']['features'] === $anRam['region_analysis']['features']
    && $anCold['region_analysis']['semantic_minimap']['grid'] === $anRam['region_analysis']['semantic_minimap']['grid']
);

// 2. ACL isolation with warm cache + WorldID=0 support: admin sees DB houses/warehouses/NPCs, restricted guest does NOT
$pdo = new PDO('sqlite::memory:');
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$pdo->exec('CREATE TABLE tw_houses (ID INTEGER PRIMARY KEY, InitialFee INT, RentDays INT, Class TEXT, Doors TEXT, Farmzones TEXT, Properties TEXT, WorldID INT)');
$pdo->exec('CREATE TABLE tw_warehouses (ID INTEGER PRIMARY KEY, Name TEXT, Type TEXT, WorldID INT, PosX INT, PosY INT)');
$pdo->exec('CREATE TABLE tw_bots_info (ID INTEGER PRIMARY KEY, Name TEXT)');
$pdo->exec('CREATE TABLE tw_bots_npc (ID INTEGER PRIMARY KEY, BotID INT, WorldID INT, PosX INT, PosY INT, Function INT, GiveQuestID INT)');
$stH = $pdo->prepare('INSERT INTO tw_houses VALUES (10, 500, 3, ?, ?, NULL, ?, 0)');
$stH->execute(['Cottage', json_encode([['name' => 'Door', 'position' => ['x' => 1600, 'y' => 1600]]]), json_encode(['position' => ['x' => 1600, 'y' => 1600]])]);
$pdo->exec("INSERT INTO tw_warehouses VALUES (20, 'Secret Vault', 'trade', 0, 1700, 1600)");
$pdo->exec("INSERT INTO tw_bots_info VALUES (30, 'Secret NPC')");
$pdo->exec("INSERT INTO tw_bots_npc VALUES (31, 30, 0, 1800, 1600, 0, 0)");
$db = new CountingMysqli($pdo);

$anAdmin = ai_map_analyze_full($mainBytes, $db, 0, EDITOR_ROLE_ADMIN, null, ['include_minimap' => false]);
$anGuest = ai_map_analyze_full($mainBytes, $db, 0, EDITOR_ROLE_GUEST, ['quests'], ['include_minimap' => false]);

// 3. Synthetic DDNet v4 map builder for BFS horizontal wrap-around regression test
//    Build a 48x24 map with TWO isolated enclosed rooms at x=0..6 and x=41..47 (same rows y=4..12),
//    separated by wide open sky (x=7..40). With the old 1D BFS bug, x=0 connected to x=47 via cell-1!
$buildSynthMap = static function (int $w, int $h, array $gameGrid, ?array $visGrid = null, string $author = 'test'): string {
    $packTiles = static function (array $grid, int $w, int $h): string {
        $raw = '';
        for ($y = 0; $y < $h; $y++) {
            for ($x = 0; $x < $w; $x++) {
                $raw .= chr($grid[$y][$x] ?? 0) . "\\x00\\x00\\x00";
            }
        }
        return $raw;
    };
    $dataRaw = [];
    $dataRaw[] = $author . "\\x00"; // data 0: author
    $dataRaw[] = $packTiles($gameGrid, $w, $h); // data 1: game tiles
    if ($visGrid !== null) {
        $dataRaw[] = $packTiles($visGrid, $w, $h); // data 2: visual tiles
    }
    $dataComp = array_map(static fn(string $d): string => gzcompress($d, 1), $dataRaw);

    // Items:
    //  type 1 (INFO): 1 item
    //  type 4 (GROUP): 1 item
    //  type 5 (LAYER): 1 or 2 items
    $items = [];
    // INFO item (type 1, id 0): version=1, author=0, map_ver=-1, credits=-1, license=-1, settings=-1
    $items[] = [1, 0, pack('VVVVVV', 1, 0, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)];
    // GROUP item (type 4, id 0): version=3, offX=0, offY=0, paraX=100, paraY=100, startL=0, numL=(vis?2:1), useClip=0, clip(4), name(3)
    $numLayers = $visGrid !== null ? 2 : 1;
    $items[] = [4, 0, pack('V15', 3, 0, 0, 100, 100, 0, $numLayers, 0, 0, 0, 0, 0, 0, 0, 0)];
    // Put visual background layer BEFORE game layer so it gets depth='background' -> AI_VIS_BG_VISUAL!
    $layerId = 0;
    if ($visGrid !== null) {
        $items[] = [5, $layerId++, pack('V18', 0, 2, 0, 3, $w, $h, 0, 255, 255, 255, 255, 0xffffffff, 0, 0xffffffff, 2, 0, 0, 0)];
    }
    // Game layer (type 5): ver=0, ltype=2, lflags=0, tileVer=3, w, h, flags=1 (GAME), rgba(4), colorEnv=-1, colorEnvOff=0, img=-1, data=1, name(3)
    $items[] = [5, $layerId++, pack('V18', 0, 2, 0, 3, $w, $h, 1, 255, 255, 255, 255, 0xffffffff, 0, 0xffffffff, 1, 0, 0, 0)];

    // Group items by type for type table
    $typeTable = '';
    $typeOrder = [];
    foreach ($items as $idx => [$tp]) {
        if (!isset($typeOrder[$tp])) $typeOrder[$tp] = ['first' => $idx, 'count' => 0];
        $typeOrder[$tp]['count']++;
    }
    foreach ($typeOrder as $tp => $meta) {
        $typeTable .= pack('VVV', $tp, $meta['first'], $meta['count']);
    }

    $itemOffsets = '';
    $itemData = '';
    foreach ($items as [$tp, $id, $payload]) {
        $itemOffsets .= pack('V', strlen($itemData));
        $itemData .= pack('VV', ($tp << 16) | $id, strlen($payload)) . $payload;
    }

    $dataOffsets = '';
    $dataSizes = '';
    $dataBlob = '';
    foreach ($dataComp as $idx => $comp) {
        $dataOffsets .= pack('V', strlen($dataBlob));
        $dataSizes .= pack('V', strlen($dataRaw[$idx]));
        $dataBlob .= $comp;
    }

    $numTypes = count($typeOrder);
    $numItems = count($items);
    $numData = count($dataComp);
    $itemSize = strlen($itemData);
    $dataSize = strlen($dataBlob);
    $tablesBin = $typeTable . $itemOffsets . $dataOffsets . $dataSizes;
    $itemStart = 36 + strlen($tablesBin);
    $dataStart = $itemStart + $itemSize;
    $totalLen = $dataStart + $dataSize;
    $header = 'DATA' . pack('VVVVVVVV', 4, $totalLen - 16, $dataStart - 16, $numTypes, $numItems, $numData, $itemSize, $dataSize);
    return $header . $tablesBin . $itemData . $dataBlob;
};

$w = 48; $h = 24;
$grid = array_fill(0, $h, array_fill(0, $w, 0));
$visGrid = array_fill(0, $h, array_fill(0, $w, 0));
// Floor across the bottom
for ($x = 0; $x < $w; $x++) $grid[13][$x] = 1;
// Left enclosed room: x=0..6, y=4..13 (roof y=4, right wall x=6, interior x=0..5, y=5..12)
for ($x = 0; $x <= 6; $x++) $grid[4][$x] = 1;
for ($y = 4; $y <= 13; $y++) $grid[$y][6] = 1;
for ($y = 5; $y <= 12; $y++) {
    for ($x = 0; $x <= 5; $x++) $visGrid[$y][$x] = 1;
}
// Right enclosed room: x=41..47, y=4..13 (roof y=4, left wall x=41, interior x=42..47, y=5..12)
for ($x = 41; $x <= 47; $x++) $grid[4][$x] = 1;
for ($y = 4; $y <= 13; $y++) $grid[$y][41] = 1;
for ($y = 5; $y <= 12; $y++) {
    for ($x = 42; $x <= 47; $x++) $visGrid[$y][$x] = 1;
}

$synthA = $buildSynthMap($w, $h, $grid, $visGrid, 'authorA');
$synthB = $buildSynthMap($w, $h, $grid, $visGrid, 'authorB'); // differs in 1 byte -> must invalidate cache!
$anSynthA = ai_map_analyze_full($synthA, null, null, 'admin', null, ['include_minimap' => false]);
$anSynthB = ai_map_analyze_full($synthB, null, null, 'admin', null, ['include_minimap' => false]);

$synthBoxes = array_map(fn($s) => $s['bbox_tiles'], $anSynthA['structures']['items']);
$wrappedAcrossMap = false;
foreach ($synthBoxes as $b) {
    if (($b['tx1'] - $b['tx0']) > 20) $wrappedAcrossMap = true;
}

// 4. Test ddnet_tile_layer_has_nonzero_in_rect() & ddnet_expand_tile_indices() on v3 and v4 RLE
$plainV3 = '';
for ($y = 0; $y < 10; $y++) {
    for ($x = 0; $x < 10; $x++) {
        $idx = ($x === 7 && $y === 3) ? 42 : 0;
        $plainV3 .= chr($idx) . "\\x00\\x00\\x00";
    }
}
$expV3 = ddnet_expand_tile_indices($plainV3, 100, 3);
// v4 RLE: 37 zeros (skip=36), 1 tile idx=42 (skip=0), 62 zeros (skip=61)
$plainV4 = "\\x00\\x00\\x24\\x00" . chr(42) . "\\x00\\x00\\x00" . "\\x00\\x00\\x3d\\x00";
$expV4 = ddnet_expand_tile_indices($plainV4, 100, 4);

echo json_encode([
    'exact_eq' => $exactEq,
    'ram_ms' => round($ramMs, 1),
    'main_structs' => count($anCold['structures']['items']),
    'admin_houses' => count($anAdmin['world_objects']['houses'] ?? []),
    'admin_warehouses' => count($anAdmin['world_objects']['warehouses'] ?? []),
    'admin_npcs' => count($anAdmin['world_objects']['npcs'] ?? []),
    'guest_houses' => count($anGuest['world_objects']['houses'] ?? []),
    'guest_warehouses' => count($anGuest['world_objects']['warehouses'] ?? []),
    'guest_npcs' => count($anGuest['world_objects']['npcs'] ?? []),
    'synth_structs_count' => count($synthBoxes),
    'synth_wrapped' => $wrappedAcrossMap,
    'synth_author_a' => $anSynthA['map_info']['author'],
    'synth_author_b' => $anSynthB['map_info']['author'],
    'rle_eq' => ($expV3 === $expV4 && ord($expV4[37]) === 42),
    'rect_hit_v3' => ddnet_tile_layer_has_nonzero_in_rect($plainV3, 10, 10, 3, 7, 3, 7, 3),
    'rect_miss_v3' => ddnet_tile_layer_has_nonzero_in_rect($plainV3, 10, 10, 3, 0, 0, 6, 9),
    'rect_hit_v4' => ddnet_tile_layer_has_nonzero_in_rect($plainV4, 10, 10, 4, 6, 2, 8, 4),
    'rect_miss_v4' => ddnet_tile_layer_has_nonzero_in_rect($plainV4, 10, 10, 4, 0, 4, 9, 9),
    'cold_hash' => hash('sha256', json_encode($anCold['structures']) . json_encode($anCold['region_analysis']['features'])),
]);
`,
    });

    const d1 = JSON.parse(res1.text);
    assert.equal(d1.exact_eq, true, 'RAM cache hit must match cold no_cache analysis exactly');
    assert.ok(d1.ram_ms < 250, `main.map RAM cache hit took too long: ${d1.ram_ms}ms`);
    assert.ok(d1.main_structs >= 20, 'main.map must detect structures');

    // ACL isolation with warm static cache + WorldID=0 support
    assert.equal(d1.admin_houses, 1, 'WorldID=0 must load DB houses for admin');
    assert.equal(d1.admin_warehouses, 1, 'WorldID=0 must load DB warehouses for admin');
    assert.equal(d1.admin_npcs, 1, 'WorldID=0 must load DB NPCs for admin');
    assert.equal(d1.guest_houses, 1, 'public map geometry tw_houses remains visible on readable map');
    assert.equal(d1.guest_warehouses, 0, 'guest without warehouses access must not see admin warehouses via cache');
    assert.equal(d1.guest_npcs, 0, 'guest without npcs access must not see admin NPCs via cache');

    // BFS horizontal wrap-around regression guard
    assert.equal(d1.synth_structs_count, 2, 'left and right edge rooms must be 2 separate structures');
    assert.equal(d1.synth_wrapped, false, 'BFS must NEVER wrap horizontally across x=0 and x=w-1');
    assert.equal(d1.synth_author_a, 'authorA');
    assert.equal(d1.synth_author_b, 'authorB');

    // RLE expansion & zero-alloc rect check parity
    assert.equal(d1.rle_eq, true);
    assert.equal(d1.rect_hit_v3, true);
    assert.equal(d1.rect_miss_v3, false);
    assert.equal(d1.rect_hit_v4, true);
    assert.equal(d1.rect_miss_v4, false);

    // Step B: Fresh PHP run reading DISK cache written in Step A!
    const res2 = await php.run({
      code: `<?php
if (!defined('EDITOR_ROLE_ADMIN')) define('EDITOR_ROLE_ADMIN', 'admin');
require_once '/studio/api/ai-schema.php';
require_once '/studio/api/map-datafile.php';
require_once '/studio/api/map-analysis.php';

$mainBytes = file_get_contents('/studio/maps/main.map');
$t0 = microtime(true);
$anDisk = ai_map_analyze_full($mainBytes, null, null, 'admin', null, ['include_minimap' => true]);
$diskMs = (microtime(true) - $t0) * 1000;
echo json_encode([
    'disk_ms' => round($diskMs, 1),
    'disk_hash' => hash('sha256', json_encode($anDisk['structures']) . json_encode($anDisk['region_analysis']['features'])),
]);
`,
    });
    const d2 = JSON.parse(res2.text);
    assert.equal(d2.disk_hash, d1.cold_hash, 'Fresh-process DISK cache hit must match cold analysis bit-for-bit');
    assert.ok(d2.disk_ms < 300, `main.map DISK cache hit took too long: ${d2.disk_ms}ms`);
  } finally {
    php.exit(0);
  }
});

test('scenario-files.php parse cache invalidation (create/update/delete/rollback) & malformed edges', async (t) => {
  if (!loadNodeRuntime || !PHP) {
    t.skip('@php-wasm/node is not installed');
    return;
  }

  const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(runtime);
  try {
    mountStudio(php);
    const res = await php.run({
      code: `<?php
require_once '/studio/api/scenario-files.php';

$dir = '/studio/scenarios';
$validV1 = ['scenario_mode' => 'default', 'steps' => [['id' => 'intro', 'components' => [['type' => 'message', 'text' => 'v1']]]]];
$validV2 = ['scenario_mode' => 'default', 'steps' => [['id' => 'intro', 'components' => [['type' => 'message', 'text' => 'v2-updated']]]]];

// 1. Create via scenario_apply_file_ops -> list -> read (cache populated)
$tx1 = scenario_apply_file_ops([[
    'type' => 'create', 'resource' => 'scenarios', 'filename' => 'quest_cache.json', 'data' => ['content' => $validV1],
]], [], $dir);
($tx1['commit'])();

$list1 = scenario_list_files('', $dir);
$read1 = scenario_read_file('quest_cache.json', $dir);

// 2. Update -> cache must invalidate immediately
$tx2 = scenario_apply_file_ops([[
    'type' => 'update', 'resource' => 'scenarios', 'filename' => 'quest_cache.json', 'data' => ['content' => $validV2],
]], [], $dir);
($tx2['commit'])();
$read2 = scenario_read_file('quest_cache.json', $dir);

// 3. Update then ROLLBACK -> cache must invalidate and restore v2
$tx3 = scenario_apply_file_ops([[
    'type' => 'update', 'resource' => 'scenarios', 'filename' => 'quest_cache.json',
    'data' => ['content' => ['scenario_mode' => 'default', 'steps' => [['id' => 'intro', 'components' => [['type' => 'message', 'text' => 'rolled-back']]]]]],
]], [], $dir);
($tx3['rollback'])();
$readAfterRollback = scenario_read_file('quest_cache.json', $dir);

// 4. Delete -> cache must drop entry
$tx4 = scenario_apply_file_ops([[
    'type' => 'delete', 'resource' => 'scenarios', 'filename' => 'quest_cache.json',
]], [], $dir);
($tx4['commit'])();
$readAfterDelete = scenario_read_file('quest_cache.json', $dir);

// 5. Malformed JSON & path traversal edge cases
file_put_contents($dir . '/broken.json', '{not valid json');
$listBroken = scenario_list_files('', $dir);
$readBroken = scenario_read_file('broken.json', $dir);
$readTraversal = scenario_read_file('../api/ai.php', $dir);

echo json_encode([
    'list1_count' => count($list1),
    'read1_text' => $read1['content']['steps'][0]['components'][0]['text'] ?? null,
    'read2_text' => $read2['content']['steps'][0]['components'][0]['text'] ?? null,
    'rollback_text' => $readAfterRollback['content']['steps'][0]['components'][0]['text'] ?? null,
    'after_delete_ok' => $readAfterDelete['ok'],
    'broken_listed_valid' => $listBroken[0]['valid_json'] ?? null,
    'broken_read_code' => $readBroken['code'] ?? null,
    'traversal_ok' => $readTraversal['ok'],
]);
`,
    });

    const d = JSON.parse(res.text);
    assert.equal(d.list1_count, 1);
    assert.equal(d.read1_text, 'v1');
    assert.equal(d.read2_text, 'v2-updated');
    assert.equal(d.rollback_text, 'v2-updated');
    assert.equal(d.after_delete_ok, false);
    assert.equal(d.broken_listed_valid, false);
    assert.equal(d.broken_read_code, 'invalid_scenario_json');
    assert.equal(d.traversal_ok, false);
  } finally {
    php.exit(0);
  }
});

test('map-render.php edge viewports & tools/mcp-stdio.mjs malformed/non-JSON framing protection', async (t) => {
  // Part A: map-render.php edge cases via PHP-WASM
  if (loadNodeRuntime && PHP) {
    const ctfAbs = path.join(REPO, 'data/maps/ctf1.map');
    if (fs.existsSync(ctfAbs)) {
      const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
      const php = new PHP(runtime);
      try {
        mountStudio(php, [['ctf1.map', 'data/maps/ctf1.map']]);
        const res = await php.run({
          code: `<?php
if (!defined('EDITOR_ROLE_ADMIN')) define('EDITOR_ROLE_ADMIN', 'admin');
require_once '/studio/api/ai-schema.php';
require_once '/studio/api/map-datafile.php';
require_once '/studio/api/map-analysis.php';
require_once '/studio/api/map-render.php';

$bytes = file_get_contents('/studio/maps/ctf1.map');
$an = ai_map_analyze_full($bytes, null, null, 'admin', null, ['include_minimap' => false]);

$cases = [
    'hybrid_min' => ai_map_render_screenshot($bytes, $an, ['mode' => 'hybrid', 'width' => 10, 'height' => 10]),
    'hybrid_unknown_target' => ai_map_render_screenshot($bytes, $an, ['mode' => 'hybrid', 'target' => 'structure:does_not_exist', 'width' => 320, 'height' => 240]),
    'schematic_oob' => ai_map_render_screenshot($bytes, $an, ['mode' => 'schematic', 'x' => -99999, 'y' => 99999, 'radius_tiles' => 2, 'width' => 320, 'height' => 240]),
    'pure_php_inverted' => ai_map_render_screenshot($bytes, $an, ['mode' => 'schematic', 'tx0' => 50, 'ty0' => 40, 'tx1' => 10, 'ty1' => 5, 'width' => 320, 'height' => 240, '_force_php_fallback' => true]),
];

$out = [];
foreach ($cases as $name => $r) {
    $png = $r['png_bytes'] ?? '';
    $sigOk = str_starts_with($png, "\\x89PNG\\r\\n\\x1a\\n");
    $ihdr = ($sigOk && strlen($png) >= 24) ? unpack('Nw/Nh', substr($png, 16, 8)) : ['w' => 0, 'h' => 0];
    $out[$name] = [
        'ok' => $sigOk,
        'w' => $ihdr['w'],
        'h' => $ihdr['h'],
        'bytes' => strlen($png),
        'renderer' => $r['renderer'] ?? '',
    ];
}
echo json_encode($out);
`,
        });
        const d = JSON.parse(res.text);
        for (const [name, info] of Object.entries(d)) {
          assert.equal(info.ok, true, `${name} must output valid PNG signature`);
          assert.ok(info.w >= 32 && info.h >= 32 && info.w <= 1600 && info.h <= 1600, `${name} dimensions out of bounds: ${info.w}x${info.h}`);
          assert.ok(info.bytes > 150, `${name} PNG unexpectedly tiny`);
        }
        assert.ok(d.hybrid_min.w >= 160, `hybrid_min must clamp width up to min 160: got ${d.hybrid_min.w}`);
        assert.equal(d.pure_php_inverted.renderer, 'php-fallback');
      } finally {
        php.exit(0);
      }
    }
  }

  // Part B: tools/mcp-stdio.mjs framing safety against malformed inputs & HTML 200 responses
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (c) => { body += c; });
    req.on('end', () => {
      const msg = JSON.parse(body);
      if (msg.method === 'trigger_html') {
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end('<html><body>Proxy Gateway Page</body></html>');
        return;
      }
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ jsonrpc: '2.0', id: msg.id, result: { ok: true } }));
    });
  });

  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const { port } = server.address();

  try {
    const bridge = spawn(process.execPath, [
      path.join(ROOT, 'tools/mcp-stdio.mjs'),
      '--url', `http://127.0.0.1:${port}/api/mcp.php`,
    ], { stdio: ['pipe', 'pipe', 'pipe'] });

    let stdout = '';
    bridge.stdout.on('data', (chunk) => { stdout += chunk.toString(); });

    // Send: 1) invalid JSON, 2) JSON primitive null, 3) request triggering HTML 200, 4) valid request
    bridge.stdin.write('{broken\n');
    bridge.stdin.write('null\n');
    bridge.stdin.write(JSON.stringify({ jsonrpc: '2.0', id: 10, method: 'trigger_html' }) + '\n');
    bridge.stdin.write(JSON.stringify({ jsonrpc: '2.0', id: 11, method: 'ping' }) + '\n');
    bridge.stdin.end();

    const code = await new Promise((resolve) => bridge.on('close', resolve));
    assert.equal(code, 0);

    const lines = stdout.trim().split('\n').map((l) => JSON.parse(l));
    assert.equal(lines.length, 4, 'every line on stdout must be valid JSON-RPC');
    assert.equal(lines[0].error.code, -32700);
    assert.equal(lines[1].error.code, -32600);
    assert.equal(lines[2].id, 10);
    assert.equal(lines[2].error.code, -32000);
    assert.match(lines[2].error.message, /non-JSON/i);
    assert.equal(lines[3].id, 11);
    assert.equal(lines[3].result.ok, true);
  } finally {
    await new Promise((resolve) => server.close(resolve));
  }
});
