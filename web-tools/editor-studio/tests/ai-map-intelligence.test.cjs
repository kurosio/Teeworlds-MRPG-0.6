'use strict';

/**
 * tests/ai-map-intelligence.test.cjs — сквозная проверка расширенного чтения,
 * структурного/семантического анализа карт (здания, дома, замки/гильддома,
 * магазины, пещеры, рельеф, визуальные слои, зоны, объекты БД, контекст точки)
 * и серверного рендера PNG-скриншотов (ext-gd + pure-PHP fallback) в REST AI API
 * (api/ai.php) и MCP (api/mcp.php).
 */

const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const ROOT = path.join(__dirname, '..');
const REPO_ROOT = path.resolve(ROOT, '../..');
const read = (rel) => fs.readFileSync(path.join(ROOT, rel), 'utf8');

let loadNodeRuntime = null;
let createNodeFsMountHandler = null;
let PHP = null;
try {
  ({ loadNodeRuntime, createNodeFsMountHandler } = require('@php-wasm/node'));
  ({ PHP } = require('@php-wasm/universal'));
} catch {
  // Опциональные пакеты @php-wasm/node/@php-wasm/universal
}

const PNG_SIG = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);

function assertValidPng(buf, label) {
  assert.ok(Buffer.isBuffer(buf) && buf.length > 64, `${label}: PNG должен быть непустым буфером`);
  assert.ok(buf.subarray(0, 8).equals(PNG_SIG), `${label}: некорректная PNG-сигнатура`);
  assert.equal(buf.subarray(12, 16).toString('ascii'), 'IHDR', `${label}: отсутствует чанк IHDR`);
  const width = buf.readUInt32BE(16);
  const height = buf.readUInt32BE(20);
  assert.ok(width >= 64 && height >= 64, `${label}: некорректные размеры PNG ${width}x${height}`);
  assert.ok(buf.includes(Buffer.from('IEND', 'ascii')), `${label}: отсутствует чанк IEND`);
  return { width, height };
}

test('статическая согласованность map-analysis, map-render, OpenAPI, onboarding и безопасность', () => {
  const openApi = JSON.parse(read('api/ai-openapi.json'));
  const actionEnum = openApi.paths['/api/ai.php'].get.parameters.find((p) => p.name === 'action').schema.enum;
  assert.ok(actionEnum.includes('analyze'), 'OpenAPI должен содержать action=analyze');
  assert.ok(actionEnum.includes('map_screenshot'), 'OpenAPI должен содержать action=map_screenshot');
  assert.ok(openApi.paths['/api/ai.php'].get.responses['200'].content['image/png'], 'OpenAPI должен описывать image/png для map_screenshot');

  const onboarding = read('ai/index.php');
  assert.ok(onboarding.includes('analyze_map'), 'ai/index.php должен упоминать analyze_map');
  assert.ok(onboarding.includes('get_map_screenshot'), 'ai/index.php должен упоминать get_map_screenshot');

  const docker = read('Dockerfile');
  assert.ok(docker.includes('libpng-dev') && docker.includes(' gd'), 'Dockerfile должен устанавливать PHP gd для рендера скриншотов');

  // Проверка отсутствия утечки внутренних настроек и секретов
  for (const rel of ['api/map-analysis.php', 'api/map-render.php']) {
    const src = read(rel);
    assert.ok(!src.includes("'secret' =>") && !src.includes('"secret" =>'), `${rel} не должен формировать секретные поля`);
    assert.ok(!src.includes('maps_dir\' =>'), `${rel} не должен возвращать maps_dir в JSON`);
    assert.ok(!src.includes('mapres_dir\' =>'), `${rel} не должен возвращать mapres_dir в JSON`);
  }
});

test('семантический анализ реальных карт MRPG (main.map, Forest.map, tutorial.map, ctf1.map) и рендер PNG (GD + PHP fallback)', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(runtime);
  try {
    php.mkdir('/repo');
    php.mount('/repo', createNodeFsMountHandler(REPO_ROOT));

    const { text } = await php.run({
      code: `<?php
        putenv('EDITOR_MAPRES_DIR=/repo/data/mapres');
        require_once '/repo/web-tools/editor-studio/api/db-core.php';
        require_once '/repo/web-tools/editor-studio/api/ai-schema.php';
        require_once '/repo/web-tools/editor-studio/api/map-render.php';

        $mainBytes = file_get_contents('/repo/maps/main.map');
        $main = ai_map_analyze_full($mainBytes, null, null, EDITOR_ROLE_ADMIN, null, [
          'include_minimap' => true,
        ]);

        // Пробы контрольных точек на main.map (интерьеры зданий vs открытое небо vs уличная земля)
        $probes = [];
        foreach ([
          'guild_hall_1' => [585 * 32 + 16, 49 * 32 + 16],
          'player_house_1' => [560 * 32 + 16, 164 * 32 + 16],
          'tavern' => [704 * 32 + 16, 90 * 32 + 16],
          'bank' => [550 * 32 + 16, 72 * 32 + 16],
          'spawn' => [612 * 32 + 16, 134 * 32 + 16],
          'open_sky' => [100 * 32 + 16, 10 * 32 + 16],
        ] as $k => [$px, $py]) {
          $probes[$k] = ai_map_analyze_full($mainBytes, null, null, EDITOR_ROLE_ADMIN, null, ['x' => $px, 'y' => $py])['point_check'];
        }

        // Остальные реальные карты
        $forest = ai_map_analyze_full(file_get_contents('/repo/maps/worlds/Forest.map'));
        $tutorial = ai_map_analyze_full(file_get_contents('/repo/maps/tutorial.map'));
        $ctf1 = ai_map_analyze_full(file_get_contents('/repo/data/maps/ctf1.map'));

        // Рендер PNG в 3 режимах + pure-PHP fallback
        $shotOverview = ai_map_render_screenshot($mainBytes, $main, ['target' => 'overview', 'mode' => 'hybrid', 'width' => 800]);
        $shotCastle = ai_map_render_screenshot($mainBytes, $main, ['target' => 'guild_house:1', 'mode' => 'hybrid', 'width' => 768]);
        $shotVisual = ai_map_render_screenshot($mainBytes, $main, ['target' => 'house:1_2', 'mode' => 'visual', 'width' => 640]);
        $shotFallback = ai_map_render_screenshot($mainBytes, $main, ['target' => 'guild_house:1', 'mode' => 'schematic', 'width' => 512, '_force_php_fallback' => true]);

        echo json_encode([
          'main' => [
            'width_tiles' => $main['width_tiles'],
            'height_tiles' => $main['height_tiles'],
            'visual_tile_layers' => $main['visual_layers']['tile_layers_count'],
            'visual_quad_layers' => $main['visual_layers']['quad_layers_count'],
            'images_count' => count($main['images']),
            'groups_count' => count($main['groups']),
            'spawns_count' => count($main['game_layer']['spawn_points']),
            'structures_total' => $main['structures']['total'],
            'structures_by_kind' => $main['structures']['by_kind'],
            'structures_sample' => array_slice($main['structures']['items'], 0, 12),
            'zones_count' => count($main['zones']['named_zones']),
            'minimap_rows' => count($main['region_analysis']['semantic_minimap']['grid'] ?? []),
            'elevation_bands' => count($main['terrain']['elevation_profile']),
          ],
          'probes' => $probes,
          'forest' => [
            'structures_total' => $forest['structures']['total'],
            'by_kind' => $forest['structures']['by_kind'],
            'spawns' => count($forest['game_layer']['spawn_points']),
          ],
          'tutorial' => [
            'structures_total' => $tutorial['structures']['total'],
            'spawns' => count($tutorial['game_layer']['spawn_points']),
          ],
          'ctf1' => [
            'format' => $ctf1['format'],
            'structures_total' => $ctf1['structures']['total'],
          ],
          'shots' => [
            'overview' => [
              'renderer' => $shotOverview['renderer'],
              'missing' => $shotOverview['missing_external_tilesets'],
              'viewport' => $shotOverview['viewport'],
              'mapping' => $shotOverview['coordinate_mapping'],
              'b64' => base64_encode($shotOverview['png_bytes']),
            ],
            'castle' => [
              'renderer' => $shotCastle['renderer'],
              'missing' => $shotCastle['missing_external_tilesets'],
              'viewport' => $shotCastle['viewport'],
              'visible_structures' => $shotCastle['visible_structures'],
              'b64' => base64_encode($shotCastle['png_bytes']),
            ],
            'visual' => [
              'renderer' => $shotVisual['renderer'],
              'mode' => $shotVisual['mode'],
              'b64' => base64_encode($shotVisual['png_bytes']),
            ],
            'fallback' => [
              'renderer' => $shotFallback['renderer'],
              'b64' => base64_encode($shotFallback['png_bytes']),
            ],
          ],
        ], JSON_UNESCAPED_UNICODE);
      `,
    });

    const data = JSON.parse(text);

    // 1. Полное чтение слоёв, групп, картинок и построек main.map
    assert.equal(data.main.width_tiles, 1000);
    assert.equal(data.main.height_tiles, 250);
    assert.ok(data.main.visual_tile_layers >= 30, 'должны читаться визуальные тайловые слои');
    assert.ok(data.main.visual_quad_layers >= 10, 'должны читаться quad-слои');
    assert.ok(data.main.images_count >= 15, 'должен читаться каталог тайлсетов/изображений');
    assert.ok(data.main.groups_count >= 5, 'должны читаться группы с parallax');
    assert.ok(data.main.spawns_count >= 1, 'должны находиться точки спавна (tile 208)');
    assert.ok((data.main.structures_by_kind.castle_or_guild_hall || 0) >= 6, 'должны распознаваться замки/гильддома');
    assert.ok((data.main.structures_by_kind.house || 0) >= 6, 'должны распознаваться жилые дома игроков');
    assert.ok((data.main.structures_by_kind.shop_or_service || 0) >= 3, 'должны распознаваться магазины/сервисы');
    assert.ok(data.main.zones_count >= 10, 'должны читаться именованные #zone');
    assert.ok(data.main.minimap_rows >= 6, 'должна строиться ASCII-миникарта');
    assert.ok(data.main.elevation_bands >= 10, 'должен строиться профиль высот рельефа');

    // Проверка пространственной кластеризации: SW_HOUSE_ZONE #1 разделён на Guild Hall #1 и Player House #1
    const gh1 = data.main.structures_sample.find((s) => s.id === 'guild_house:1');
    const ph1 = data.main.structures_sample.find((s) => s.id === 'house:1_2');
    assert.ok(gh1 && gh1.kind === 'castle_or_guild_hall', 'Guild Hall #1 должен классифицироваться как castle_or_guild_hall');
    assert.ok(ph1 && ph1.kind === 'house', 'Player House #1 должен классифицироваться как house');
    assert.ok(Math.abs(gh1.center.tile_y - ph1.center.tile_y) > 80, 'Guild Hall #1 и Player House #1 не должны сливаться в один bbox');

    // 2. Контрольные точки: интерьеры зданий vs открытое небо
    for (const interiorKey of ['guild_hall_1', 'player_house_1', 'tavern', 'bank', 'spawn']) {
      const pc = data.probes[interiorKey];
      assert.equal(pc.terrain_context.environment, 'interior_building', `${interiorKey} должен определяться как interior_building`);
      assert.equal(pc.terrain_context.has_roof_above, true, `${interiorKey} должен иметь крышу сверху`);
      assert.equal(pc.terrain_context.has_floor_below, true, `${interiorKey} должен иметь пол снизу`);
      assert.ok(pc.inside_structure && pc.inside_structure.name, `${interiorKey} должен указывать inside_structure`);
      assert.ok(pc.terrain_context.nearest_standable_point, `${interiorKey} должен вычислять nearest_standable_point`);
    }
    const sky = data.probes.open_sky;
    assert.equal(sky.terrain_context.environment, 'outdoor_air', 'точка в небе должна определяться как outdoor_air');
    assert.equal(sky.terrain_context.has_roof_above, false, 'верхняя граница карты ty=0 не должна считаться крышей');
    assert.equal(sky.inside_structure, null, 'в открытом небе не должно быть inside_structure');

    // 3. Другие реальные карты
    assert.ok(data.forest.spawns >= 1 && data.forest.structures_total >= 5);
    assert.ok(data.tutorial.spawns >= 1 && data.tutorial.structures_total >= 1);
    assert.equal(data.ctf1.format, 'DDNet v4');

    // 4. Валидные PNG-скриншоты (GD + внешние mapres + pure-PHP fallback)
    assert.equal(data.shots.overview.renderer, 'gd');
    assert.deepEqual(data.shots.overview.missing, [], 'все внешние mapres-тайлсеты main.map должны загружаться');
    assertValidPng(Buffer.from(data.shots.overview.b64, 'base64'), 'overview hybrid');

    assert.equal(data.shots.castle.renderer, 'gd');
    assert.equal(data.shots.castle.viewport.target_resolved, 'structure:guild_house:1');
    assert.ok(data.shots.castle.visible_structures.some((s) => s.id === 'guild_house:1'));
    const castlePng = assertValidPng(Buffer.from(data.shots.castle.b64, 'base64'), 'castle hybrid');
    assert.ok(castlePng.width <= 768 && castlePng.width >= 400);

    assert.equal(data.shots.visual.mode, 'visual');
    assertValidPng(Buffer.from(data.shots.visual.b64, 'base64'), 'player house visual');

    assert.equal(data.shots.fallback.renderer, 'php-fallback');
    assertValidPng(Buffer.from(data.shots.fallback.b64, 'base64'), 'pure-PHP fallback schematic');
  } finally {
    try { php.exit(0); } catch {}
  }
});

test('REST api/ai.php и MCP api/mcp.php: analyze_map, map_screenshot (JSON/PNG/MCP image block), DB-корреляция и гостевой ACL', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(runtime);
  try {
    php.mkdir('/studio');
    php.mkdir('/studio/api');
    php.mkdir('/studio/data');
    php.mkdir('/repo');
    php.mount('/repo', createNodeFsMountHandler(REPO_ROOT));

    // Копируем api/*.php с лёгким хуком db_connect() под in-memory SQLite mock (как в ai-api.test.cjs)
    for (const f of fs.readdirSync(path.join(ROOT, 'api'))) {
      if (!f.endsWith('.php') && !f.endsWith('.json')) continue;
      let src = read(`api/${f}`);
      if (f === 'db-core.php') {
        src = src.replace(
          'function db_connect(): mysqli\n{',
          'function db_connect(): mysqli\n{\n    if (isset($GLOBALS[\'__TEST_MYSQLI\']) && $GLOBALS[\'__TEST_MYSQLI\'] instanceof mysqli) { return $GLOBALS[\'__TEST_MYSQLI\']; }'
        );
      }
      php.writeFile(`/studio/api/${f}`, src);
    }

    const { text } = await php.run({
      code: `<?php
        putenv('EDITOR_MAPS_DIR=/repo/maps');
        putenv('EDITOR_MAPRES_DIR=/repo/data/mapres');

        class MapTestMysqliResult extends mysqli_result {
            private array $rows;
            private int $i = 0;
            public function __construct(array $rows) { $this->rows = array_values($rows); }
            public function fetch_assoc(): ?array { return $this->rows[$this->i++] ?? null; }
            public function fetch_row(): ?array {
                $r = $this->fetch_assoc();
                return $r === null ? null : array_values($r);
            }
            public function fetch_all(int $mode = MYSQLI_NUM): array {
                return $mode === MYSQLI_ASSOC ? $this->rows : array_map('array_values', $this->rows);
            }
            public function free(): void {}
        }
        class MapTestMysqliStmt extends mysqli_stmt {
            private PDO $pdo;
            private string $sql;
            private array $params = [];
            private array $rows = [];
            public function __construct(PDO $pdo, string $sql) { $this->pdo = $pdo; $this->sql = $sql; }
            public function bind_param(string $types, mixed &...$vars): bool { $this->params = $vars; return true; }
            public function execute(?array $params = null): bool {
                $bind = $params ?? $this->params;
                $s = str_replace(chr(96), '"', $this->sql);
                if (preg_match('/information_schema\\.tables/i', $s)) {
                    $st = $this->pdo->prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name = ? LIMIT 1");
                    $st->execute([(string)($bind[0] ?? '')]);
                    $this->rows = $st->fetchAll(PDO::FETCH_ASSOC);
                    return true;
                }
                $st = $this->pdo->prepare($s);
                $st->execute(array_map(static fn($v) => $v, $bind));
                $this->rows = preg_match('/^\\s*SELECT\\b/i', $s) ? $st->fetchAll(PDO::FETCH_ASSOC) : [];
                return true;
            }
            public function get_result(): MapTestMysqliResult|false { return new MapTestMysqliResult($this->rows); }
            public function close(): true { return true; }
        }
        class MapTestMysqli extends mysqli {
            public PDO $pdo;
            public function __construct(PDO $pdo) { $this->pdo = $pdo; }
            public function prepare(string $query): MapTestMysqliStmt|false {
                return new MapTestMysqliStmt($this->pdo, $query);
            }
            public function query(string $query, int $result_mode = MYSQLI_STORE_RESULT): MapTestMysqliResult|bool {
                $s = str_replace(chr(96), '"', $query);
                $st = $this->pdo->query($s);
                return $st ? new MapTestMysqliResult($st->fetchAll(PDO::FETCH_ASSOC)) : false;
            }
            public function close(): true { return true; }
        }

        $pdo = new PDO('sqlite::memory:');
        $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        $pdo->exec('CREATE TABLE tw_worlds (ID INTEGER PRIMARY KEY, Name TEXT, Path TEXT, Type TEXT, Flags TEXT, RespawnWorldID INTEGER, JailWorldID INTEGER, RequiredLevel INTEGER)');
        $pdo->exec('CREATE TABLE tw_houses (ID INTEGER PRIMARY KEY, InitialFee INTEGER, RentDays INTEGER, Class TEXT, Bank TEXT, Doors TEXT, Farmzones TEXT, Properties TEXT, WorldID INTEGER)');
        $pdo->exec('CREATE TABLE tw_guilds_houses (ID INTEGER PRIMARY KEY, InitialFee INTEGER, RentDays INTEGER, Doors TEXT, Farmzones TEXT, Properties TEXT, WorldID INTEGER, KickPos TEXT)');
        $pdo->exec('CREATE TABLE tw_warehouses (ID INTEGER PRIMARY KEY, Name TEXT, Type TEXT, Trades TEXT, PosX INTEGER, PosY INTEGER, StorageData TEXT, Currency INTEGER, WorldID INTEGER)');
        $pdo->exec('CREATE TABLE tw_bots_info (ID INTEGER PRIMARY KEY, Name TEXT)');
        $pdo->exec('CREATE TABLE tw_bots_npc (ID INTEGER PRIMARY KEY, BotID INTEGER, PosX INTEGER, PosY INTEGER, GiveQuestID INTEGER, DialogData TEXT, Function INTEGER, Static INTEGER, Emote TEXT, WorldID INTEGER)');
        $pdo->exec('CREATE TABLE tw_world_swap (ID INTEGER PRIMARY KEY, WorldID INTEGER, PositionX INTEGER, PositionY INTEGER, TwoWorldID INTEGER, TwoPositionX INTEGER, TwoPositionY INTEGER)');

        $pdo->exec("INSERT INTO tw_worlds VALUES (1, 'Gridania', 'main.map', 'default', '', 1, 1, 1)");
        $pdo->exec("INSERT INTO tw_houses VALUES (1, 10000, 3, 'Starter Cottage', '0', '[{\\\"name\\\":\\\"Main door\\\",\\\"position\\\":{\\\"x\\\":17736,\\\"y\\\":5329}}]', NULL, '{\\\"position\\\":{\\\"x\\\":17933,\\\"y\\\":5265}}', 1)");
        $pdo->exec("INSERT INTO tw_guilds_houses VALUES (1, 10000, 3, '[{\\\"name\\\":\\\"Front Door\\\",\\\"position\\\":{\\\"x\\\":17827,\\\"y\\\":1617}}]', NULL, '{\\\"position\\\":{\\\"x\\\":18720,\\\"y\\\":1568},\\\"radius\\\":1376}', 1, '{}')");
        $pdo->exec("INSERT INTO tw_warehouses VALUES (10, 'Gridania Bank Storage', 'storage', '', 17600, 2304, '', 2, 1)");
        $pdo->exec("INSERT INTO tw_bots_info VALUES (7, 'Guild Steward')");
        $pdo->exec("INSERT INTO tw_bots_npc VALUES (21, 7, 18720, 1568, 0, '[]', 0, 1, '', 1)");
        $pdo->exec("INSERT INTO tw_world_swap VALUES (3, 1, 19376, 4208, 2, 100, 100)");

        $db = new MapTestMysqli($pdo);
        $GLOBALS['__TEST_MYSQLI'] = $db;

        require_once '/studio/api/ai-lib.php';

        // 1. Админ: полная корреляция объектов БД (гильддома + дома + склады + NPC + world_swap)
        $adminAnalyze = ai_map_analyze_request($db, 1, null, EDITOR_ROLE_ADMIN, null, [
          'target' => 'guild_house:1',
          'structure_kind' => 'castle_or_guild_hall',
        ]);

        // 2. Ограниченный гость (только вкладка worlds, без bots_npc и warehouses):
        //    видит карту и публичную геометрию домов, но НЕ видит NPC и склады!
        $guestAnalyze = ai_map_analyze_request($db, 1, null, EDITOR_ROLE_GUEST, ['worlds'], [
          'compact' => true,
        ]);

        // 3. Проверка ACL: гость только с вкладкой items НЕ может читать карты
        $guestCanReadMaps = ai_can_read_resource('maps', EDITOR_ROLE_GUEST, ['items']);
        $guestWorldsCanReadMaps = ai_can_read_resource('maps', EDITOR_ROLE_GUEST, ['worlds']);

        // 4. Скриншот через общий обработчик + формирование нативного MCP image result
        $shotRes = ai_map_screenshot_request($db, 1, null, EDITOR_ROLE_ADMIN, null, [
          'target' => 'guild_house:1',
          'mode' => 'hybrid',
          'width' => 640,
        ]);
        $pngBytes = (string)$shotRes['screenshot']['png_bytes'];
        unset($shotRes['screenshot']['png_bytes']);

        // Эмулируем mcp_tool_image_result из api/mcp.php
        $mcpMeta = array_merge(['ok' => true], $shotRes);
        $mcpContent = [
          ['type' => 'image', 'data' => base64_encode($pngBytes), 'mimeType' => 'image/png'],
          ['type' => 'text', 'text' => (string)json_encode($mcpMeta, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES)],
        ];

        // 5. Проверка защиты от path traversal
        $traversalBlocked = false;
        try {
          ai_map_load_bytes($db, null, '../etc/passwd');
        } catch (CrInputError $e) {
          $traversalBlocked = ($e->extra['code'] ?? '') === 'invalid_map_path';
        }

        echo json_encode([
          'admin_gh1' => $adminAnalyze['analysis']['structures']['items'][0] ?? null,
          'admin_objects_counts' => $adminAnalyze['analysis']['world_objects']['counts'] ?? [],
          'admin_filtered_kind' => $adminAnalyze['analysis']['structures']['filtered_kind'] ?? null,
          'admin_region_target' => $adminAnalyze['analysis']['region_analysis']['target_resolved'] ?? null,
          'guest_objects_counts' => $guestAnalyze['analysis']['world_objects']['counts'] ?? [],
          'guest_items_can_maps' => $guestCanReadMaps,
          'guest_worlds_can_maps' => $guestWorldsCanReadMaps,
          'mcp_content' => $mcpContent,
          'traversal_blocked' => $traversalBlocked,
        ], JSON_UNESCAPED_UNICODE);
      `,
    });

    const out = JSON.parse(text);

    // Админ видит полную DB-корреляцию, включая Guild Steward внутри Guild Hall #1
    assert.equal(out.admin_filtered_kind, 'castle_or_guild_hall');
    assert.equal(out.admin_region_target, 'structure:guild_house:1');
    assert.equal(out.admin_objects_counts.houses, 1);
    assert.equal(out.admin_objects_counts.guild_houses, 1);
    assert.equal(out.admin_objects_counts.warehouses, 1);
    assert.equal(out.admin_objects_counts.npcs, 1);
    assert.equal(out.admin_objects_counts.world_swaps, 1);
    assert.ok(out.admin_gh1 && out.admin_gh1.db_record && out.admin_gh1.db_record.table === 'tw_guilds_houses');
    assert.ok(Array.isArray(out.admin_gh1.linked_objects) && out.admin_gh1.linked_objects.some((o) => o.name === 'Guild Steward'));

    // ACL гостя: при доступе только к worlds публичные дома видны, а NPC и склады скрыты
    assert.equal(out.guest_objects_counts.houses, 1);
    assert.equal(out.guest_objects_counts.guild_houses, 1);
    assert.equal(out.guest_objects_counts.warehouses, 0, 'склады не должны утекать гостю без вкладки warehouses');
    assert.equal(out.guest_objects_counts.npcs, 0, 'NPC не должны утекать гостю без вкладки bots_npc');
    assert.equal(out.guest_items_can_maps, false, 'гость только с items не должен иметь доступа к maps');
    assert.equal(out.guest_worlds_can_maps, true, 'гость с worlds должен иметь доступ к maps');

    // MCP image block: валидный PNG в content[0], без дублирования base64 в content[1].text
    assert.equal(out.mcp_content[0].type, 'image');
    assert.equal(out.mcp_content[0].mimeType, 'image/png');
    assertValidPng(Buffer.from(out.mcp_content[0].data, 'base64'), 'MCP image content');
    assert.equal(out.mcp_content[1].type, 'text');
    assert.ok(!out.mcp_content[1].text.includes('iVBOR'), 'base64 PNG не должен дублироваться в текстовом блоке MCP');
    const parsedMeta = JSON.parse(out.mcp_content[1].text);
    assert.equal(parsedMeta.screenshot.viewport.target_resolved, 'structure:guild_house:1');
    assert.ok(parsedMeta.screenshot.coordinate_mapping.formula.includes('world_x'));

    // Защита от path traversal
    assert.equal(out.traversal_blocked, true, 'path traversal должен блокироваться');
  } finally {
    try { php.exit(0); } catch {}
  }
});

test('сквозной HTTP/Bearer вызов реальных api/ai.php (analyze maps, map_screenshot JSON & PNG) и api/mcp.php (analyze_map, get_map_screenshot)', { skip: !PHP ? '@php-wasm/node не установлен' : false }, async () => {
  const runtime = await loadNodeRuntime('8.3', { emscriptenOptions: { processId: 1 } });
  const php = new PHP(runtime);
  try {
    php.mkdir('/studio');
    php.mkdir('/studio/api');
    php.mkdir('/studio/data');
    php.mkdir('/studio/scenarios');
    php.mkdir('/repo');
    php.mount('/repo', createNodeFsMountHandler(REPO_ROOT));

    for (const f of fs.readdirSync(path.join(ROOT, 'api'))) {
      if (!f.endsWith('.php') && !f.endsWith('.json')) continue;
      let src = read(`api/${f}`);
      if (f === 'db-core.php') {
        src = src.replace(
          'function db_connect(): mysqli\n{',
          'function db_connect(): mysqli\n{\n    if (isset($GLOBALS[\'__TEST_MYSQLI\']) && $GLOBALS[\'__TEST_MYSQLI\'] instanceof mysqli) { return $GLOBALS[\'__TEST_MYSQLI\']; }'
        );
      }
      php.writeFile(`/studio/api/${f}`, src);
    }

    // Пишем лёгкий раннер для запуска реальных api/ai.php и api/mcp.php с Bearer-токеном и SQLite mock.
    php.writeFile('/studio/test_runner.php', `<?php
      putenv('EDITOR_MAPS_DIR=/repo/maps');
      putenv('EDITOR_MAPRES_DIR=/repo/data/mapres');

      class HttpMapRes extends mysqli_result {
          private array $rows;
          private int $pos = 0;
          public function __construct(array $rows) { $this->rows = array_values($rows); }
          public function fetch_assoc(): ?array { return $this->rows[$this->pos++] ?? null; }
          public function fetch_row(): ?array { $r = $this->fetch_assoc(); return $r === null ? null : array_values($r); }
          public function fetch_all(int $mode = MYSQLI_NUM): array { return $mode === MYSQLI_ASSOC ? $this->rows : array_map('array_values', $this->rows); }
          public function free(): void {}
      }
      class HttpMapStmt extends mysqli_stmt {
          private array $params = [];
          private array $rows = [];
          public function __construct(private PDO $pdo, private string $sql) {}
          public function bind_param(string $types, mixed &...$vars): bool { $this->params = $vars; return true; }
          public function execute(?array $params = null): bool {
              $bind = $params ?? $this->params;
              $s = str_replace(chr(96), '"', $this->sql);
              if (preg_match('/information_schema\\.tables/i', $s)) {
                  $st = $this->pdo->prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name = ? LIMIT 1");
                  $st->execute([(string)($bind[0] ?? '')]);
                  $this->rows = $st->fetchAll(PDO::FETCH_ASSOC);
                  return true;
              }
              $st = $this->pdo->prepare($s);
              $st->execute(array_map(static fn($v) => $v, $bind));
              $this->rows = preg_match('/^\\s*SELECT\\b/i', $s) ? $st->fetchAll(PDO::FETCH_ASSOC) : [];
              return true;
          }
          public function get_result(): HttpMapRes|false { return new HttpMapRes($this->rows); }
          public function close(): true { return true; }
      }
      class HttpMapMysqli extends mysqli {
          public function __construct(public PDO $pdo) {}
          public function prepare(string $query): HttpMapStmt|false { return new HttpMapStmt($this->pdo, $query); }
          public function query(string $query, int $result_mode = MYSQLI_STORE_RESULT): HttpMapRes|bool {
              $s = str_replace(chr(96), '"', $query);
              $st = $this->pdo->query($s);
              return $st ? new HttpMapRes($st->fetchAll(PDO::FETCH_ASSOC)) : false;
          }
          public function close(): true { return true; }
      }

      $pdo = new PDO('sqlite::memory:');
      $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
      $pdo->exec('CREATE TABLE tw_worlds (ID INTEGER PRIMARY KEY, Name TEXT, Path TEXT, Type TEXT, Flags TEXT, RespawnWorldID INTEGER, JailWorldID INTEGER, RequiredLevel INTEGER)');
      $pdo->exec('CREATE TABLE tw_houses (ID INTEGER PRIMARY KEY, InitialFee INTEGER, RentDays INTEGER, Class TEXT, Bank TEXT, Doors TEXT, Farmzones TEXT, Properties TEXT, WorldID INTEGER)');
      $pdo->exec('CREATE TABLE tw_guilds_houses (ID INTEGER PRIMARY KEY, InitialFee INTEGER, RentDays INTEGER, Doors TEXT, Farmzones TEXT, Properties TEXT, WorldID INTEGER, KickPos TEXT)');
      $pdo->exec("INSERT INTO tw_worlds VALUES (1, 'Gridania', 'main.map', 'default', '', 1, 1, 1)");
      $pdo->exec("INSERT INTO tw_houses VALUES (1, 10000, 3, 'House 1', '0', '[{\\\"name\\\":\\\"Main door\\\",\\\"position\\\":{\\\"x\\\":17736,\\\"y\\\":5329}}]', NULL, '{\\\"position\\\":{\\\"x\\\":17933,\\\"y\\\":5265}}', 1)");
      $pdo->exec("INSERT INTO tw_guilds_houses VALUES (1, 10000, 3, '[{\\\"name\\\":\\\"Front Door\\\",\\\"position\\\":{\\\"x\\\":17827,\\\"y\\\":1617}}]', NULL, '{\\\"position\\\":{\\\"x\\\":18720,\\\"y\\\":1568},\\\"radius\\\":1376}', 1, '{}')");
      $GLOBALS['__TEST_MYSQLI'] = new HttpMapMysqli($pdo);

      $spec = json_decode((string)file_get_contents('/studio/req.json'), true);
      $_SERVER['REQUEST_METHOD'] = $spec['method'];
      $_SERVER['HTTP_HOST'] = '127.0.0.1';
      $_SERVER['HTTP_AUTHORIZATION'] = 'Bearer ' . $spec['token'];
      $_SERVER['CONTENT_TYPE'] = 'application/json';
      $_SERVER['SCRIPT_NAME'] = $spec['script'];
      $_GET = $spec['query'] ?? [];
      if (isset($spec['body'])) {
          putenv('EDITOR_STUDIO_TEST=1');
          putenv('EDITOR_STUDIO_BODY=' . json_encode($spec['body']));
      } else {
          putenv('EDITOR_STUDIO_TEST=');
          putenv('EDITOR_STUDIO_BODY=');
      }
      require '/studio' . $spec['script'];
    `);

    // Создаём гостевого пользователя с доступом к worlds и Bearer-токен (read) + гостя без доступа к картам (items only)
    const setupRes = await php.run({
      code: `<?php
        require_once '/studio/api/db-core.php';
        store_settings_save([
          'host' => 'mock', 'port' => 3306, 'database' => 'mrpg', 'user' => 'root', 'password' => '',
          'editor_password' => password_hash('adminpass1234', PASSWORD_BCRYPT),
          'guest_enabled' => true, 'guest_allow_remote' => true,
          'guest_max_pending' => 4, 'guest_submit_cooldown' => 0, 'guest_max_operations' => 25,
          'maps_dir' => '/repo/maps', 'mapres_dir' => '/repo/data/mapres',
          'map_api' => '', 'map_api_token' => 'TOP_SECRET_MAP_TOKEN',
        ]);
        store_guest_user_insert([
          'id' => 'gmap0001', 'login' => 'map_reader', 'name' => 'Map AI', 'enabled' => true,
          'pages' => ['worlds'], 'passwordHash' => password_hash('SecretPass123!', PASSWORD_BCRYPT),
          'sessionVersion' => 1, 'createdAt' => gmdate('c'), 'updatedAt' => gmdate('c'),
          'lastLoginAt' => '', 'limits' => ['max_pending' => 4, 'cooldown' => 0, 'max_operations' => 25], 'expiresAt' => '',
        ]);
        store_guest_user_insert([
          'id' => 'gmap0002', 'login' => 'items_only', 'name' => 'Items AI', 'enabled' => true,
          'pages' => ['items'], 'passwordHash' => password_hash('SecretPass123!', PASSWORD_BCRYPT),
          'sessionVersion' => 1, 'createdAt' => gmdate('c'), 'updatedAt' => gmdate('c'),
          'lastLoginAt' => '', 'limits' => ['max_pending' => 4, 'cooldown' => 0, 'max_operations' => 25], 'expiresAt' => '',
        ]);
        $tokMap = 'esai_' . bin2hex(random_bytes(24));
        $tokNoMap = 'esai_' . bin2hex(random_bytes(24));
        store_ai_token_insert([
          'id' => 'tmap0001', 'tokenHash' => hash('sha256', $tokMap), 'tokenPrefix' => substr($tokMap, 0, 12),
          'guestId' => 'gmap0001', 'name' => 'Map Token', 'scopes' => ['read'], 'pages' => ['worlds'],
          'createdAt' => gmdate('c'), 'expiresAt' => '', 'lastUsedAt' => '', 'revokedAt' => '',
        ]);
        store_ai_token_insert([
          'id' => 'tmap0002', 'tokenHash' => hash('sha256', $tokNoMap), 'tokenPrefix' => substr($tokNoMap, 0, 12),
          'guestId' => 'gmap0002', 'name' => 'Items Token', 'scopes' => ['read'], 'pages' => ['items'],
          'createdAt' => gmdate('c'), 'expiresAt' => '', 'lastUsedAt' => '', 'revokedAt' => '',
        ]);
        echo json_encode(['tokMap' => $tokMap, 'tokNoMap' => $tokNoMap]);
      `,
    });
    const { tokMap, tokNoMap } = JSON.parse(setupRes.text);

    async function callEndpoint(script, method, token, query = {}, body = null) {
      const reqObj = { script, method, token, query };
      if (body !== null) reqObj.body = body;
      php.writeFile('/studio/req.json', JSON.stringify(reqObj));
      const r = await php.run({ scriptPath: '/studio/test_runner.php' });
      return {
        status: r.httpStatusCode,
        headers: r.headers,
        bytes: Buffer.from(r.bytes),
        text: r.text,
      };
    }

    // 1. REST GET action=analyze&resource=maps
    const rAnalyze = await callEndpoint('/api/ai.php', 'GET', tokMap, {
      action: 'analyze',
      resource: 'maps',
      world: '1',
      target: 'guild_house:1',
      structure_kind: 'castle_or_guild_hall',
    });
    assert.equal(rAnalyze.status, 200);
    const jAnalyze = JSON.parse(rAnalyze.text);
    assert.equal(jAnalyze.ok, true);
    assert.equal(jAnalyze.mode, 'map');
    assert.equal(jAnalyze.analysis.structures.filtered_kind, 'castle_or_guild_hall');
    assert.ok(jAnalyze.analysis.structures.items.some((s) => s.id === 'guild_house:1'));
    assert.ok(jAnalyze.analysis.region_analysis.semantic_minimap.grid.length >= 6);

    // 2. REST GET action=map_screenshot&format=json
    const rShotJson = await callEndpoint('/api/ai.php', 'GET', tokMap, {
      action: 'map_screenshot',
      world: '1',
      target: 'guild_house:1',
      mode: 'hybrid',
      width: '640',
      format: 'json',
    });
    assert.equal(rShotJson.status, 200);
    const jShot = JSON.parse(rShotJson.text);
    assert.equal(jShot.ok, true);
    assert.ok(jShot.screenshot.data_uri.startsWith('data:image/png;base64,'));
    assertValidPng(Buffer.from(jShot.screenshot.image_base64, 'base64'), 'REST map_screenshot json');

    // 3. REST GET action=map_screenshot&format=png (raw image/png stream)
    const rShotPng = await callEndpoint('/api/ai.php', 'GET', tokMap, {
      action: 'map_screenshot',
      world: '1',
      target: 'house:1_2',
      mode: 'visual',
      width: '512',
      format: 'png',
    });
    assert.equal(rShotPng.status, 200);
    assert.ok((rShotPng.headers['content-type'] || []).some((v) => v.includes('image/png')), 'должен отдаваться Content-Type: image/png');
    assert.ok(rShotPng.headers['x-map-viewport'], 'должен отдаваться заголовок X-Map-Viewport');
    assertValidPng(rShotPng.bytes, 'REST map_screenshot raw png');

    // 4. ACL: токен только с items получает 403 resource_forbidden на map_screenshot и analyze maps
    const rDenied = await callEndpoint('/api/ai.php', 'GET', tokNoMap, {
      action: 'map_screenshot',
      world: '1',
    });
    assert.equal(rDenied.status, 403);
    assert.equal(JSON.parse(rDenied.text).code, 'resource_forbidden');

    // 5. MCP tools/call: analyze_map
    const rMcpAnalyze = await callEndpoint('/api/mcp.php', 'POST', tokMap, {}, {
      jsonrpc: '2.0',
      id: 101,
      method: 'tools/call',
      params: {
        name: 'analyze_map',
        arguments: { world_id: 1, target: 'house:1_2', x: 560 * 32 + 16, y: 164 * 32 + 16, compact: true },
      },
    });
    assert.equal(rMcpAnalyze.status, 200);
    const jMcpAnalyze = JSON.parse(rMcpAnalyze.text);
    assert.equal(jMcpAnalyze.result.isError, false);
    assert.equal(jMcpAnalyze.result.structuredContent.analysis.point_check.terrain_context.environment, 'interior_building');
    assert.equal(jMcpAnalyze.result.structuredContent.analysis.point_check.inside_structure.kind, 'house');

    // 6. MCP tools/call: get_map_screenshot -> native MCP type="image" block + clean metadata
    const rMcpShot = await callEndpoint('/api/mcp.php', 'POST', tokMap, {}, {
      jsonrpc: '2.0',
      id: 102,
      method: 'tools/call',
      params: {
        name: 'get_map_screenshot',
        arguments: { world_id: 1, target: 'guild_house:1', mode: 'hybrid', width: 640 },
      },
    });
    assert.equal(rMcpShot.status, 200);
    const jMcpShot = JSON.parse(rMcpShot.text);
    assert.equal(jMcpShot.result.isError, false);
    assert.equal(jMcpShot.result.content[0].type, 'image');
    assert.equal(jMcpShot.result.content[0].mimeType, 'image/png');
    assertValidPng(Buffer.from(jMcpShot.result.content[0].data, 'base64'), 'MCP get_map_screenshot image block');
    assert.equal(jMcpShot.result.content[1].type, 'text');
    assert.ok(!jMcpShot.result.content[1].text.includes('iVBOR'), 'MCP текстовый блок не должен дублировать base64 PNG');
    assert.equal(jMcpShot.result.structuredContent.screenshot.viewport.target_resolved, 'structure:guild_house:1');

    // 7. Проверка отсутствия утечки внутренних путей и секретов во всех ответах
    for (const bodyTxt of [rAnalyze.text, rShotJson.text, rMcpAnalyze.text, rMcpShot.text]) {
      assert.ok(!bodyTxt.includes('/repo/maps'), 'внутренний путь maps_dir не должен утекать');
      assert.ok(!bodyTxt.includes('/repo/data/mapres'), 'внутренний путь mapres_dir не должен утекать');
      assert.ok(!bodyTxt.includes('TOP_SECRET_MAP_TOKEN'), 'map_api_token не должен утекать');
    }
  } finally {
    try { php.exit(0); } catch {}
  }
});
