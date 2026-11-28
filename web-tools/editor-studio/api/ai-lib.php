<?php
// Библиотека общего AI API: проверка прав гостя/админа, расширенный поиск и
// чтение, разбор карт DDNet v4, анализ входящих/исходящих связей, комплексная
// валидация операций (БД + файлы сценариев) и построение предпросмотра с diff.
//
// Все операции здесь не изменяют игровую БД и не пишут файлы на диск.

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';
require_once __DIR__ . '/map-datafile.php';
require_once __DIR__ . '/ai-schema.php';
require_once __DIR__ . '/change-requests-lib.php';

const AI_MAP_READ_TABS = [
    'quests', 'bots_npc', 'bots_mobs', 'warehouses', 'worlds',
    'crafts', 'dungeons', 'aethers', 'nodes_collecting',
];

const AI_MAP_MAX_BYTES = 50331648;
const AI_MAP_MAX_UNCOMPRESSED = 536870912;
const AI_DEFAULT_RESPONSE_MAX_BYTES = 524288;
const AI_MAX_RESPONSE_MAX_BYTES = 2097152;

/** Карта прав справочников (согласована с api/db.php SOURCE_TABS). */
function ai_lookup_tabs(): array
{
    return [
        'quests' => ['quests', 'bots_npc', 'quest_boards', 'scenarios'],
        'quest_boards' => ['quests', 'quest_boards'],
        'items' => ['items', 'crafts', 'warehouses', 'quests', 'bots_info', 'bots_mobs', 'nodes_collecting', 'scenarios'],
        'worlds' => ['worlds', 'crafts', 'warehouses', 'quests', 'quest_boards', 'bots_mobs', 'bots_npc', 'nodes_collecting', 'dungeons', 'aethers', 'items', 'scenarios'],
        'skills' => ['items', 'bots_mobs', 'quests'],
        'bots' => ['bots_info', 'bots_mobs', 'bots_npc', 'quests', 'dungeons', 'scenarios'],
        'attributes' => ['items', 'bots_mobs'],
    ];
}

function ai_effective_guest_pages(?array $override = null): array
{
    return $override !== null ? array_values($override) : editor_guest_pages();
}

function ai_can_read_resource(string $resource, string $role, ?array $guestPages = null): bool
{
    if ($role === EDITOR_ROLE_ADMIN) {
        if ($resource === 'scenarios' || $resource === 'maps') {
            return true;
        }
        if (isset(ai_schema_lookups()[$resource])) {
            return true;
        }
        return isset(editor_resources()[$resource]);
    }
    if ($role !== EDITOR_ROLE_GUEST) {
        return false;
    }

    $pages = ai_effective_guest_pages($guestPages);
    if ($resource === 'scenarios') {
        return in_array('scenarios', $pages, true);
    }
    if ($resource === 'maps') {
        return (bool)array_intersect(AI_MAP_READ_TABS, $pages);
    }
    $lookups = ai_schema_lookups();
    if (isset($lookups[$resource])) {
        return (bool)array_intersect($lookups[$resource]['tabs'] ?? [], $pages);
    }
    $R = editor_resources()[$resource] ?? null;
    if ($R === null || empty($R['guest'])) {
        return false;
    }
    return in_array($resource, editor_guest_read_resources($pages), true);
}

function ai_can_write_resource(string $resource, string $role, ?array $guestPages = null): bool
{
    if ($role === EDITOR_ROLE_ADMIN) {
        return $resource === 'scenarios' || isset(editor_resources()[$resource]);
    }
    if ($role !== EDITOR_ROLE_GUEST) {
        return false;
    }
    $pages = ai_effective_guest_pages($guestPages);
    if ($resource === 'scenarios') {
        return in_array('scenarios', $pages, true);
    }
    if ($resource === 'maps' || isset(ai_schema_lookups()[$resource])) {
        return false;
    }
    $R = editor_resources()[$resource] ?? null;
    if ($R === null || empty($R['guest'])) {
        return false;
    }
    return in_array($resource, editor_guest_write_resources($pages), true);
}

function ai_can_read_lookup_source(string $source, string $role, ?array $guestPages = null): bool
{
    if ($role === EDITOR_ROLE_ADMIN) {
        return isset(ai_lookup_tabs()[$source]);
    }
    if ($role !== EDITOR_ROLE_GUEST) {
        return false;
    }
    $tabs = ai_lookup_tabs()[$source] ?? null;
    if ($tabs === null) {
        return false;
    }
    return (bool)array_intersect($tabs, ai_effective_guest_pages($guestPages));
}

function ai_table_to_resource_map(): array
{
    static $map = null;
    if ($map === null) {
        $map = [];
        foreach (editor_resources() as $key => $R) {
            $map[$R['table']] = $key;
        }
        foreach (ai_schema_lookups() as $key => $L) {
            $map[$L['table']] = $key;
        }
    }
    return $map;
}

/** Формирование каталога ресурсов (только разрешённых текущей сессии). */
function ai_allowed_resources(string $role, ?array $guestPages = null, bool $includeExamples = true): array
{
    $refSchema = editor_reference_schema();
    $out = [];

    foreach (editor_resources() as $key => $R) {
        if (!ai_can_read_resource($key, $role, $guestPages)) {
            continue;
        }
        $meta = ai_schema_for_resource($key) ?? [];
        $entry = [
            'kind' => 'database',
            'resource' => $key,
            'title' => (string)$R['title'],
            'table' => (string)$R['table'],
            'pk' => (string)$R['pk'],
            'label_column' => $R['label'] ?? null,
            'allow_zero_id' => !empty($meta['allow_zero_id']),
            'auto_discovered' => !empty($meta['auto_discovered']),
            'permissions' => [
                'read' => true,
                'propose' => ai_can_write_resource($key, $role, $guestPages),
            ],
            'writable_columns' => $R['columns'],
            'search_columns' => $R['search'] ?? [],
            'json_columns' => $R['json'] ?? [],
            'direct_refs' => $R['refs'] ?? [],
            'reference_rules' => $refSchema[$key] ?? [],
            'required_on_create' => $meta['required_on_create'] ?? [],
            'unique_keys' => $meta['unique_keys'] ?? [],
            'columns_schema' => $meta['columns'] ?? [],
        ];
        if ($includeExamples) {
            $entry['examples'] = ai_schema_example_for($key, $R, $meta);
        }
        $out[$key] = $entry;
    }

    foreach (ai_schema_lookups() as $key => $L) {
        if (!ai_can_read_resource($key, $role, $guestPages)) {
            continue;
        }
        $out[$key] = [
            'kind' => 'lookup',
            'resource' => $key,
            'title' => (string)$L['title'],
            'table' => (string)$L['table'],
            'pk' => (string)$L['pk'],
            'label_column' => (string)$L['label'],
            'permissions' => ['read' => true, 'propose' => false],
            'columns' => $L['columns'],
            'search_columns' => $L['search'],
        ];
    }

    if (ai_can_read_resource('scenarios', $role, $guestPages)) {
        $scEntry = [
            'kind' => 'file',
            'resource' => 'scenarios',
            'title' => 'Сценарии (файлы)',
            'pk' => 'filename',
            'label_column' => 'filename',
            'permissions' => [
                'read' => true,
                'propose' => ai_can_write_resource('scenarios', $role, $guestPages),
            ],
            'max_bytes' => SCENARIO_MAX_BYTES,
            'filename_pattern' => '^[A-Za-z0-9_\\p{L}][A-Za-z0-9\\p{L}._ -]*\\.json$',
            'supports_base_hash' => true,
            'supports_patch_json' => true,
        ];
        if ($includeExamples) {
            $scEntry['examples'] = ai_schema_examples()['scenarios'] ?? [];
        }
        $out['scenarios'] = $scEntry;
    }

    if (ai_can_read_resource('maps', $role, $guestPages)) {
        $out['maps'] = [
            'kind' => 'map_metadata',
            'resource' => 'maps',
            'title' => 'Карты миров (только инспекция)',
            'pk' => 'world_id',
            'permissions' => ['read' => true, 'propose' => false],
            'tile_size_px' => 32,
        ];
    }

    return $out;
}

/** Манифест возможностей API для AI-агента. */
function ai_build_manifest(string $role, ?array $account, array $limits, ?array $guestPages = null, bool $includeSchemas = true): array
{
    $resources = ai_allowed_resources($role, $guestPages, $includeSchemas);
    $scenarioAware = isset($resources['scenarios']) || isset($resources['items']) || isset($resources['quest_bots']) || isset($resources['dungeons']);

    $lookupsAllowed = [];
    foreach (array_keys(ai_lookup_tabs()) as $src) {
        if (ai_can_read_lookup_source($src, $role, $guestPages)) {
            $lookupsAllowed[] = $src;
        }
    }

    $tokenActor = editor_ai_token_actor();
    $manifest = [
        'api_version' => '2.0',
        'service' => 'Teeworlds-MRPG-0.6 Editor Studio Universal AI API',
        'openapi' => 'api/ai.php?action=openapi',
        'discovery_urls' => [
            'landing' => 'ai/',
            'plugin_manifest' => '.well-known/ai-plugin.json',
            'openapi' => 'openapi.json',
            'mcp_http' => 'mcp',
            'rest_api' => 'api/ai.php',
        ],
        'capabilities' => [
            'model_agnostic' => true,
            'auto_resource_discovery' => true,
            'openapi_3_1' => true,
            'mcp_jsonrpc_2_0' => true,
            'bearer_tokens' => true,
            'scoped_rbac' => true,
            'patch_json_merge' => true,
            'copy_records' => true,
            'dangerous_confirmation_gate' => true,
            'optimistic_concurrency' => true,
            'idempotent_propose' => true,
        ],
        'auth' => [
            'role' => $role,
            'auth_method' => $tokenActor !== null ? 'bearer_token' : 'session',
            'token_scopes' => ai_actor_scopes(),
            'account_login' => $account['login'] ?? null,
            'account_name' => $account['name'] ?? null,
            'granted_pages' => $role === EDITOR_ROLE_ADMIN ? array_keys(editor_guest_page_catalog()) : ai_effective_guest_pages($guestPages),
            'direct_db_writes_allowed' => false,
            'submission_workflow' => 'guest_change_request',
        ],
        'limits' => $limits,
        'pagination' => [
            'default_limit' => 50,
            'max_limit' => 500,
            'default_max_bytes' => AI_DEFAULT_RESPONSE_MAX_BYTES,
            'max_response_bytes' => AI_MAX_RESPONSE_MAX_BYTES,
        ],
        'error_format' => [
            'ok' => false,
            'code' => 'string_machine_code',
            'error' => 'string_human_message',
            'optional_fields' => ['field', 'errors', 'warnings', 'allowed', 'conflicts', 'dangerous_actions', 'requires_confirmation', 'version', 'retry_after'],
        ],
        'operation_types' => ['create', 'update', 'copy', 'delete'],
        'operation_aliases' => ['action' => 'Optional alias for type (create|update|copy|delete)'],
        'endpoints' => [
            'discovery' => ['method' => 'GET', 'params' => [], 'auth' => 'public'],
            'openapi' => ['method' => 'GET', 'params' => [], 'auth' => 'public'],
            'manifest' => ['method' => 'GET', 'params' => ['include_schemas?']],
            'resources' => ['method' => 'GET', 'params' => ['resource?', 'include_examples?']],
            'list' => ['method' => 'GET', 'params' => ['resource', 'search?', 'filter?', 'filter_key?', 'filter_value?', 'filter_values?', 'fields?', 'limit?', 'offset?', 'max_bytes?', 'decode?']],
            'get' => ['method' => 'GET', 'params' => ['resource', 'id|file|world|path', 'fields?', 'summary_only?', 'include_references?', 'include_entities?', 'x?', 'y?']],
            'references' => ['method' => 'GET', 'params' => ['resource? & id?', 'source? & search?', 'limit?', 'offset?', 'max_bytes?']],
            'history' => ['method' => 'GET', 'params' => ['resource?', 'id?', 'limit?', 'offset?']],
            'validate' => ['method' => 'POST', 'body' => ['operations' => 'array<op>'], 'mutates' => false],
            'preview' => ['method' => 'POST', 'body' => ['operations' => 'array<op>'], 'mutates' => false],
            'propose' => ['method' => 'POST', 'body' => ['message', 'nickname?', 'operations', 'confirm_dangerous?', 'idempotency_key?', 'request_id?', 'version?'], 'mutates' => 'sqlite_change_requests_only'],
        ],
        'temp_id_convention' => [
            'min' => GUEST_TEMP_ID_BASE,
            'max' => GUEST_TEMP_ID_MAX,
            'auto_assign_when_omitted' => true,
            'note' => 'Database create operations can specify explicit tempId (2000000000..2000999999) for cross-referencing within the same proposal, or omit it for automatic sequential assignment.',
        ],
        'resources_summary' => array_map(static fn(array $r): array => [
            'kind' => $r['kind'],
            'title' => $r['title'],
            'pk' => $r['pk'],
            'permissions' => $r['permissions'],
        ], $resources),
        'lookup_sources' => $lookupsAllowed,
    ];

    if ($includeSchemas) {
        $manifest['resources'] = $resources;
    }

    if ($scenarioAware) {
        $manifest['scenario_schema'] = ai_schema_scenario_definitions();
    }
    if (isset($resources['maps'])) {
        $manifest['map_schema'] = ai_schema_map_definitions();
    }

    return $manifest;
}

// ── Чтение, проекция и пагинация с контролем размера ──────────────────────────

/** Проверка и нормализация списка запрашиваемых колонок ресурса БД. */
function ai_resolve_select_fields(array $R, mixed $fields): array
{
    $pk = editor_identifier($R['pk'], 'pk');
    $allowedCols = array_values(array_unique(array_merge([$pk], $R['columns'])));
    if ($fields === null || $fields === '' || $fields === []) {
        return $allowedCols;
    }
    $allowedSet = array_flip($allowedCols);
    $requested = is_array($fields)
        ? $fields
        : array_values(array_filter(array_map('trim', explode(',', (string)$fields)), static fn(string $s): bool => $s !== ''));
    if ($requested === []) {
        return $allowedCols;
    }
    $valid = [$pk => true];
    foreach ($requested as $col) {
        if (!is_string($col) || !isset($allowedSet[$col])) {
            throw new CrInputError("Неизвестная колонка «{$col}» для ресурса «{$R['key']}».", 400, [
                'code' => 'unknown_field',
                'field' => (string)$col,
                'allowed' => $allowedCols,
            ]);
        }
        $valid[$col] = true;
    }
    return array_keys($valid);
}

/** Проекция одной записи БД по запрошенным полям (?fields=...). */
function ai_project_record_fields(array $row, array $R, mixed $fields): array
{
    if ($fields === null || $fields === '' || $fields === []) {
        return $row;
    }
    $cols = ai_resolve_select_fields($R, $fields);
    $out = [];
    foreach ($cols as $col) {
        if (array_key_exists($col, $row)) {
            $out[$col] = $row[$col];
        }
    }
    return $out;
}

/**
 * Единая пагинация и отсечение списка по бюджету max_bytes.
 * Гарантирует хотя бы 1 элемент при непустом срезе, чтобы offset всегда продвигался.
 */
function ai_paginate_slice(array $rows, int $total, int $limit, int $offset, int $maxBytes = AI_DEFAULT_RESPONSE_MAX_BYTES): array
{
    $budget = clamp_int($maxBytes, 4096, AI_MAX_RESPONSE_MAX_BYTES, AI_DEFAULT_RESPONSE_MAX_BYTES);
    $kept = [];
    $used = 2; // []
    $truncated = false;

    foreach ($rows as $row) {
        $encoded = json_encode($row, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
        $len = is_string($encoded) ? strlen($encoded) : 0;
        $nextUsed = $used + ($kept === [] ? 0 : 1) + $len;
        if ($kept !== [] && $nextUsed > $budget) {
            $truncated = true;
            break;
        }
        $kept[] = $row;
        $used = $nextUsed;
    }

    $count = count($kept);
    $hasMore = ($offset + $count) < $total;

    return [
        'rows' => $kept,
        'total' => $total,
        'limit' => $limit,
        'offset' => $offset,
        'has_more' => $hasMore,
        'next_offset' => $hasMore ? ($offset + $count) : null,
        'truncated' => $truncated,
        'max_bytes' => $budget,
        'bytes' => $used,
    ];
}

/** Безопасная выборка списка из БД с фильтрами и проекцией колонок. */
function ai_query_db_list(mysqli $db, array $R, array $opts): array
{
    $table = editor_identifier($R['table'], 'table');
    $pk = editor_identifier($R['pk'], 'pk');
    $allowedCols = array_values(array_unique(array_merge([$pk], $R['columns'])));
    $allowedSet = array_flip($allowedCols);

    $selectCols = ai_resolve_select_fields($R, $opts['fields'] ?? null);

    $where = [];
    $types = '';
    $params = [];

    $search = trim((string)($opts['search'] ?? ''));
    if ($search !== '' && !empty($R['search'])) {
        $like = '%' . $search . '%';
        $parts = [];
        foreach ($R['search'] as $col) {
            $colId = editor_identifier($col, 'column');
            $parts[] = "CAST(`$colId` AS CHAR) LIKE ?";
            $types .= 's';
            $params[] = $like;
        }
        $where[] = '(' . implode(' OR ', $parts) . ')';
    }

    $filters = $opts['filter'] ?? [];
    if (is_array($filters)) {
        foreach ($filters as $col => $val) {
            if (!is_string($col) || !isset($allowedSet[$col])) {
                throw new CrInputError("Неизвестное поле фильтрации «{$col}» для ресурса «{$R['key']}».", 400, [
                    'code' => 'unknown_filter_field',
                    'field' => (string)$col,
                    'allowed' => $allowedCols,
                ]);
            }
            $colId = editor_identifier($col, 'filter column');
            if ($val === null) {
                $where[] = "`$colId` IS NULL";
            } elseif (is_array($val)) {
                $vals = array_values($val);
                if ($vals === []) {
                    $where[] = '1 = 0';
                } else {
                    $placeholders = implode(', ', array_fill(0, count($vals), '?'));
                    $where[] = "`$colId` IN ($placeholders)";
                    foreach ($vals as $item) {
                        $types .= is_int($item) ? 'i' : 's';
                        $params[] = $item;
                    }
                }
            } elseif (is_bool($val)) {
                $where[] = "`$colId` = ?";
                $types .= 'i';
                $params[] = $val ? 1 : 0;
            } elseif (is_int($val)) {
                $where[] = "`$colId` = ?";
                $types .= 'i';
                $params[] = $val;
            } else {
                $where[] = "`$colId` = ?";
                $types .= 's';
                $params[] = (string)$val;
            }
        }
    }

    $whereSql = $where ? ' WHERE ' . implode(' AND ', $where) : '';
    $colSql = implode(', ', array_map(static fn(string $c): string => '`' . editor_identifier($c, 'select') . '`', $selectCols));
    $order = $R['order'] ?? "`$pk` ASC";

    $countSql = "SELECT COUNT(*) FROM `$table`$whereSql";
    $stmtCount = $db->prepare($countSql);
    if ($stmtCount === false) {
        throw new RuntimeException($db->error ?: 'Ошибка подготовки запроса подсчёта');
    }
    if ($types !== '') {
        $stmtCount->bind_param($types, ...$params);
    }
    $stmtCount->execute();
    $resCount = $stmtCount->get_result();
    $total = (int)($resCount ? ($resCount->fetch_row()[0] ?? 0) : 0);
    $stmtCount->close();

    $limit = clamp_int($opts['limit'] ?? 50, 1, 500, 50);
    $offset = clamp_int($opts['offset'] ?? 0, 0, 10000000, 0);

    $sql = "SELECT $colSql FROM `$table`$whereSql ORDER BY $order LIMIT ? OFFSET ?";
    $stmt = $db->prepare($sql);
    if ($stmt === false) {
        throw new RuntimeException($db->error ?: 'Ошибка подготовки выборки');
    }
    $allTypes = $types . 'ii';
    $allParams = array_merge($params, [$limit, $offset]);
    $stmt->bind_param($allTypes, ...$allParams);
    $stmt->execute();
    $res = $stmt->get_result();
    $rows = [];
    $jsonCols = $R['json'] ?? [];
    $decode = (bool)($opts['decode'] ?? true);
    while ($row = $res->fetch_assoc()) {
        $rows[] = $decode ? decode_json_cols($row, $jsonCols) : $row;
    }
    $stmt->close();

    return ai_paginate_slice($rows, $total, $limit, $offset, (int)($opts['max_bytes'] ?? AI_DEFAULT_RESPONSE_MAX_BYTES));
}

/** Чтение справочника (attributes / skills / lookup sources). */
function ai_query_lookup(mysqli $db, string $source, array $opts): array
{
    $sources = [
        'quests' => ['table' => 'tw_quests_list', 'id' => 'ID', 'label' => 'Name'],
        'quest_boards' => ['table' => 'tw_quest_boards', 'id' => 'ID', 'label' => 'Name'],
        'items' => ['table' => 'tw_items_list', 'id' => 'ID', 'label' => 'Name'],
        'worlds' => ['table' => 'tw_worlds', 'id' => 'ID', 'label' => 'Name'],
        'skills' => ['table' => 'tw_skills_list', 'id' => 'ID', 'label' => 'Name'],
        'bots' => ['table' => 'tw_bots_info', 'id' => 'ID', 'label' => 'Name'],
        'attributes' => ['table' => 'tw_attributes', 'id' => 'ID', 'label' => 'Name'],
    ];
    if (!isset($sources[$source])) {
        throw new CrInputError("Неизвестный справочник «{$source}».", 400, ['code' => 'unknown_source', 'source' => $source]);
    }
    $S = $sources[$source];
    $table = editor_identifier($S['table'], 'table');
    $idCol = editor_identifier($S['id'], 'id');
    $labelCol = editor_identifier($S['label'], 'label');

    $lookups = ai_schema_lookups();
    $cols = isset($lookups[$source]) ? $lookups[$source]['columns'] : [$idCol, $labelCol];
    $colSql = implode(', ', array_map(static fn(string $c): string => '`' . editor_identifier($c, 'col') . '`', $cols));

    $where = [];
    $types = '';
    $params = [];
    $search = trim((string)($opts['search'] ?? ''));
    if ($search !== '') {
        $like = '%' . $search . '%';
        $where[] = "(CAST(`$idCol` AS CHAR) LIKE ? OR `$labelCol` LIKE ?)";
        $types .= 'ss';
        $params[] = $like;
        $params[] = $like;
    }

    $whereSql = $where ? ' WHERE ' . implode(' AND ', $where) : '';
    $stmtCount = $db->prepare("SELECT COUNT(*) FROM `$table`$whereSql");
    if ($types !== '') {
        $stmtCount->bind_param($types, ...$params);
    }
    $stmtCount->execute();
    $total = (int)($stmtCount->get_result()->fetch_row()[0] ?? 0);
    $stmtCount->close();

    $limit = clamp_int($opts['limit'] ?? 200, 1, 1000, 200);
    $offset = clamp_int($opts['offset'] ?? 0, 0, 10000000, 0);
    $stmt = $db->prepare("SELECT $colSql FROM `$table`$whereSql ORDER BY `$idCol` ASC LIMIT ? OFFSET ?");
    $stmt->bind_param($types . 'ii', ...array_merge($params, [$limit, $offset]));
    $stmt->execute();
    $rows = [];
    $items = [];
    $res = $stmt->get_result();
    while ($row = $res->fetch_assoc()) {
        $rows[] = $row;
        $items[] = ['value' => (int)$row[$idCol], 'label' => (string)$row[$labelCol]];
    }
    $stmt->close();

    $page = ai_paginate_slice($rows, $total, $limit, $offset, (int)($opts['max_bytes'] ?? AI_DEFAULT_RESPONSE_MAX_BYTES));
    $page['items'] = array_slice($items, 0, count($page['rows']));
    return $page;
}

// ── DDNet v4 map inspection (delegates binary parsing to api/map-datafile.php) ──

function ai_map_u32(string $bytes, int $offset): int
{
    return ddnet_u32($bytes, $offset);
}

function ai_map_i32(string $bytes, int $offset): int
{
    return ddnet_i32($bytes, $offset);
}

function ai_map_expand_tiles(string $plain, int $count, int $version): string
{
    return ddnet_expand_tiles($plain, $count, $version);
}

/**
 * Parse a DDNet v4 binary map file and build an AI-friendly summary
 * without leaking server filesystem paths.
 *
 * @return array<string, mixed>
 */
function ai_map_parse_datafile(string $bytes, bool $includeEntities = false, ?int $pointX = null, ?int $pointY = null): array
{
    $parsed = ddnet_parse_datafile($bytes, AI_MAP_MAX_BYTES, AI_MAP_MAX_UNCOMPRESSED, false);
    $length = (int)$parsed['bytes'];
    $layerCount = (int)$parsed['layers_count'];
    $game = $parsed['game'];
    $gameTiles = (string)$parsed['game_tiles'];
    $switch = $parsed['switch'];
    $switchTiles = $parsed['switch_tiles'];

    $w = (int)$game['width'];
    $h = (int)$game['height'];
    $cellCount = $w * $h;

    $defs = ai_schema_map_definitions();
    $entityNames = $defs['game_entity_tiles'];
    $nonEmpty = 0;
    $solidCount = 0;
    $deathCount = 0;
    $unhookCount = 0;
    $entityCounts = [];
    $entities = [];
    $entitiesTruncated = false;

    for ($idx = 0; $idx < $cellCount; $idx++) {
        $tileId = ord($gameTiles[$idx * 3]);
        if ($tileId === 0) {
            continue;
        }
        $nonEmpty++;
        if ($tileId === 1) {
            $solidCount++;
        } elseif ($tileId === 2) {
            $deathCount++;
        } elseif ($tileId === 3) {
            $unhookCount++;
        } else {
            $key = $entityNames[$tileId] ?? ("tile_{$tileId}");
            $entityCounts[$key] = ($entityCounts[$key] ?? 0) + 1;
            if ($includeEntities && $tileId >= 191) {
                if (count($entities) < 500) {
                    $tx = $idx % $w;
                    $ty = intdiv($idx, $w);
                    $entities[] = [
                        'tile' => $tileId,
                        'name' => $key,
                        'tile_x' => $tx,
                        'tile_y' => $ty,
                        'x' => $tx * 32 + 16,
                        'y' => $ty * 32 + 16,
                    ];
                } else {
                    $entitiesTruncated = true;
                }
            }
        }
    }

    $switchInfo = ['present' => false, 'entries_count' => 0, 'switch_numbers' => [], 'switch_types' => []];
    $switchTilesOut = [];
    $switchTilesTruncated = false;
    if ($switch !== null && is_string($switchTiles)) {
        $swW = (int)$switch['width'];
        $swH = (int)$switch['height'];
        $swCells = $swW * $swH;
        $swTypeNames = $defs['switch_tiles'];
        $numbers = [];
        $types = [];
        $entriesCount = 0;
        for ($idx = 0; $idx < $swCells; $idx++) {
            $off = $idx * 4;
            $num = ord($switchTiles[$off]);
            $type = ord($switchTiles[$off + 1]);
            if ($type === 0 && $num === 0) {
                continue;
            }
            $entriesCount++;
            if ($num > 0) {
                $numbers[$num] = true;
            }
            $tName = $swTypeNames[$type] ?? ("type_{$type}");
            $types[$tName] = ($types[$tName] ?? 0) + 1;
            if ($includeEntities && $num > 0) {
                if (count($switchTilesOut) < 500) {
                    $tx = $idx % $swW;
                    $ty = intdiv($idx, $swW);
                    $switchTilesOut[] = [
                        'number' => $num,
                        'type' => $type,
                        'name' => $tName,
                        'tile_x' => $tx,
                        'tile_y' => $ty,
                        'x' => $tx * 32 + 16,
                        'y' => $ty * 32 + 16,
                    ];
                } else {
                    $switchTilesTruncated = true;
                }
            }
        }
        $numKeys = array_keys($numbers);
        sort($numKeys, SORT_NUMERIC);
        $switchInfo = [
            'present' => true,
            'width' => $swW,
            'height' => $swH,
            'entries_count' => $entriesCount,
            'switch_numbers' => $numKeys,
            'switch_types' => $types,
        ];
        if ($includeEntities) {
            $switchInfo['tiles'] = $switchTilesOut;
            $switchInfo['tiles_truncated'] = $switchTilesTruncated;
        }
    }

    $summary = [
        'format' => 'DDNet v4',
        'bytes' => $length,
        'sha256' => hash('sha256', $bytes),
        'width_tiles' => $w,
        'height_tiles' => $h,
        'width_px' => $w * 32,
        'height_px' => $h * 32,
        'layers_count' => $layerCount,
        'game_layer' => [
            'non_empty_tiles' => $nonEmpty,
            'solid_tiles' => $solidCount,
            'death_tiles' => $deathCount,
            'unhookable_tiles' => $unhookCount,
            'entity_counts' => $entityCounts,
        ],
        'switch_layer' => $switchInfo,
    ];
    if ($includeEntities) {
        $summary['game_entities'] = $entities;
        $summary['entities_truncated'] = $entitiesTruncated;
    }

    if ($pointX !== null && $pointY !== null) {
        $tx = (int)floor($pointX / 32);
        $ty = (int)floor($pointY / 32);
        $inBounds = $tx >= 0 && $ty >= 0 && $tx < $w && $ty < $h;
        $tileId = $inBounds ? ord($gameTiles[($ty * $w + $tx) * 3]) : null;
        $collisionBlocked = in_array($tileId, [1, 2, 3], true);
        $swAtPoint = null;
        if ($switch !== null && is_string($switchTiles) && $tx >= 0 && $ty >= 0 && $tx < $switch['width'] && $ty < $switch['height']) {
            $sOff = ($ty * $switch['width'] + $tx) * 4;
            $sNum = ord($switchTiles[$sOff]);
            $sType = ord($switchTiles[$sOff + 1]);
            if ($sNum > 0 || $sType > 0) {
                $swAtPoint = ['number' => $sNum, 'type' => $sType, 'delay' => ord($switchTiles[$sOff + 3])];
            }
        }
        $summary['point_check'] = [
            'x' => $pointX,
            'y' => $pointY,
            'tile_x' => $tx,
            'tile_y' => $ty,
            'in_bounds' => $inBounds,
            'game_tile_id' => $tileId,
            'safe_for_placement' => $inBounds && !$collisionBlocked,
            'collision_blocked' => $collisionBlocked,
            'switch_tile' => $swAtPoint,
        ];
    }

    return $summary;
}

/** Locate readable maps directories without exposing paths to callers. */
function ai_map_find_dirs(): array
{
    return ddnet_map_existing_dirs(load_cfg(), 'EDITOR_MAPS_DIR', ['maps', 'data/maps'], 'maps_dir');
}

function ai_map_resolve_relative(string $dir, string $relative): ?string
{
    return ddnet_map_resolve($dir, $relative);
}

function ai_map_load_bytes(?mysqli $db, ?int $worldId, ?string $rawPath): array
{
    $world = null;
    $path = trim((string)$rawPath);
    if ($worldId !== null) {
        if ($db === null) {
            throw new CrInputError('База данных недоступна для получения пути карты.', 500, ['code' => 'db_unavailable']);
        }
        $R = editor_resources()['worlds'];
        $world = crud_fetch_row($db, $R, $worldId);
        if ($world === null) {
            throw new CrInputError("Мир #{$worldId} не найден в таблице tw_worlds.", 404, ['code' => 'world_not_found']);
        }
        $path = trim((string)($world['Path'] ?? ''));
    }

    if ($path === '') {
        return ['world' => $world, 'available' => false, 'source' => null, 'bytes' => null, 'reason' => 'Путь к карте не указан'];
    }

    $cleanPath = str_replace('\\', '/', $path);
    if ($cleanPath[0] === '/' || str_contains($cleanPath, '..')) {
        throw new CrInputError('Недопустимый относительный путь карты.', 400, ['code' => 'invalid_map_path']);
    }

    $candidates = [$cleanPath];
    if (!str_ends_with(strtolower($cleanPath), '.map')) {
        $candidates[] = $cleanPath . '.map';
    }
    foreach (ai_map_find_dirs() as $dir) {
        foreach ($candidates as $cand) {
            $file = ai_map_resolve_relative($dir, $cand);
            if ($file !== null && filesize($file) <= AI_MAP_MAX_BYTES) {
                $bytes = @file_get_contents($file);
                if (is_string($bytes)) {
                    return ['world' => $world, 'available' => true, 'source' => 'local', 'bytes' => $bytes, 'basename' => basename($cand)];
                }
            }
        }
    }

    $cfg = load_cfg();
    $mapApi = trim((string)($cfg['map_api'] ?? ''));
    if ($mapApi !== '') {
        $sep = str_contains($mapApi, '?') ? '&' : '?';
        $query = http_build_query([
            'action' => 'file',
            'world' => $worldId !== null ? (string)$worldId : '',
            'path' => $cleanPath,
        ]);
        $resp = map_api_request($mapApi . $sep . $query, (string)($cfg['map_api_token'] ?? ''), AI_MAP_MAX_BYTES);
        if (!empty($resp['ok']) && is_string($resp['body']) && $resp['body'] !== '') {
            return ['world' => $world, 'available' => true, 'source' => 'map_api', 'bytes' => $resp['body'], 'basename' => basename($cleanPath)];
        }
    }

    return ['world' => $world, 'available' => false, 'source' => null, 'bytes' => null, 'reason' => 'Файл карты не найден на сервере', 'basename' => basename($cleanPath)];
}

// ── Анализ входящих и исходящих связей (references) ──────────────────────────

/**
 * Проверка, существует ли таблица в текущей БД (защита для локальных таблиц).
 */
function ai_table_exists(mysqli $db, string $table): bool
{
    static $cache = [];
    $ck = spl_object_id($db) . '|' . $table;
    if (array_key_exists($ck, $cache)) {
        return $cache[$ck];
    }
    $stmt = $db->prepare('SELECT 1 FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = ? LIMIT 1');
    if ($stmt === false) {
        return $cache[$ck] = false;
    }
    $stmt->bind_param('s', $table);
    $stmt->execute();
    $exists = ($stmt->get_result()->fetch_row() !== null);
    $stmt->close();
    return $cache[$ck] = $exists;
}

function ai_resolve_target_summary(mysqli $db, string $tableOrResource, int $id, string $role, ?array $guestPages = null): array
{
    $resources = editor_resources();
    $lookups = ai_schema_lookups();
    $tableMap = ai_table_to_resource_map();
    if (isset($resources[$tableOrResource])) {
        $targetRes = $tableOrResource;
        $table = $resources[$tableOrResource]['table'];
    } elseif (isset($lookups[$tableOrResource])) {
        $targetRes = $tableOrResource;
        $table = $lookups[$tableOrResource]['table'];
    } else {
        $table = $tableOrResource;
        $targetRes = $tableMap[$table] ?? null;
    }

    $accessible = $targetRes !== null && ai_can_read_resource($targetRes, $role, $guestPages);
    $lookupSource = match ($table) {
        'tw_items_list' => 'items',
        'tw_worlds' => 'worlds',
        'tw_bots_info' => 'bots',
        'tw_quests_list' => 'quests',
        'tw_quest_boards' => 'quest_boards',
        'tw_skills_list' => 'skills',
        'tw_attributes' => 'attributes',
        default => null,
    };
    $lookupAccessible = $lookupSource !== null && ai_can_read_lookup_source($lookupSource, $role, $guestPages);

    if (!$accessible && !$lookupAccessible) {
        return ['target_table' => $table, 'target_resource' => $targetRes, 'id' => $id, 'accessible' => false];
    }

    $tableId = editor_identifier($table, 'table');
    $R = $targetRes !== null ? ($resources[$targetRes] ?? null) : null;
    $pkCol = $R ? editor_identifier($R['pk'], 'pk') : 'ID';
    $labelCol = $R && !empty($R['label']) ? $R['label'] : (in_array($table, ['tw_attributes', 'tw_skills_list'], true) ? 'Name' : null);
    $cols = $labelCol ? "`$pkCol`, `" . editor_identifier($labelCol, 'label') . '`' : "`$pkCol`";

    $stmt = $db->prepare("SELECT $cols FROM `$tableId` WHERE `$pkCol` = ? LIMIT 1");
    $exists = false;
    $label = null;
    if ($stmt) {
        $stmt->bind_param('i', $id);
        $stmt->execute();
        $row = $stmt->get_result()->fetch_assoc();
        $stmt->close();
        if ($row) {
            $exists = true;
            if ($labelCol !== null && isset($row[$labelCol])) {
                $label = (string)$row[$labelCol];
            }
        }
    }
    return [
        'target_table' => $table,
        'target_resource' => $targetRes,
        'id' => $id,
        'accessible' => true,
        'exists' => $exists,
        'label' => $label,
    ];
}

function ai_references_inspect(
    mysqli $db,
    string $resource,
    int|string $id,
    string $role,
    ?array $guestPages = null,
    ?string $scenariosDir = null
): array {
    if ($resource === 'scenarios') {
        $read = scenario_read_file((string)$id, $scenariosDir);
        if (!$read['ok']) {
            throw new CrInputError((string)$read['error'], ($read['code'] ?? '') === 'scenario_not_found' ? 404 : 400, ['code' => $read['code'] ?? 'invalid_filename']);
        }
        $outbound = [];
        foreach ($read['references'] as $ref) {
            $resolved = ai_resolve_target_summary($db, $ref['target'], (int)$ref['id'], $role, $guestPages);
            $outbound[] = array_merge($resolved, ['source' => 'scenario_json', 'path' => $ref['path']]);
        }

        // Поиск в items.Data (use_scenario.name) если items доступен.
        $inbound = [];
        if (ai_can_read_resource('items', $role, $guestPages) && ai_table_exists($db, 'tw_items_list')) {
            $base = preg_replace('/\.json$/i', '', (string)$read['filename']);
            $res = $db->query("SELECT `ID`, `Name`, `Data` FROM `tw_items_list` WHERE `Data` IS NOT NULL AND `Data` <> ''");
            if ($res) {
                while ($row = $res->fetch_assoc()) {
                    $decoded = json_decode((string)$row['Data'], true);
                    $scName = is_array($decoded) ? (string)($decoded['use_scenario']['name'] ?? '') : '';
                    if ($scName !== '' && ($scName === $read['filename'] || $scName === $base)) {
                        $inbound[] = [
                            'source_resource' => 'items',
                            'source_table' => 'tw_items_list',
                            'record_id' => (int)$row['ID'],
                            'record_label' => (string)$row['Name'],
                            'field' => 'Data',
                            'path' => 'use_scenario.name',
                        ];
                    }
                }
            }
        }
        return [
            'resource' => 'scenarios',
            'id' => $read['filename'],
            'outbound' => $outbound,
            'inbound' => $inbound,
        ];
    }

    $resources = editor_resources();
    $lookups = ai_schema_lookups();
    $R = $resources[$resource] ?? null;
    $targetTable = $R ? $R['table'] : ($lookups[$resource]['table'] ?? null);
    if ($targetTable === null) {
        throw new CrInputError("Неизвестный ресурс «{$resource}».", 400, ['code' => 'unknown_resource']);
    }

    $recId = (int)$id;
    if ($R !== null) {
        $row = crud_fetch_row($db, $R, $recId);
        if ($row === null) {
            throw new CrInputError("Запись {$R['title']} #{$recId} не найдена.", 404, ['code' => 'record_not_found']);
        }
        $decoded = decode_json_cols($row, $R['json'] ?? []);
    } else {
        $tableSql = editor_identifier($targetTable, 'table');
        $stmt = $db->prepare("SELECT * FROM `$tableSql` WHERE `ID` = ? LIMIT 1");
        $stmt->bind_param('i', $recId);
        $stmt->execute();
        $row = $stmt->get_result()->fetch_assoc();
        $stmt->close();
        if (!$row) {
            throw new CrInputError("Запись {$resource} #{$recId} не найдена.", 404, ['code' => 'record_not_found']);
        }
        $decoded = $row;
    }

    $outbound = [];
    $seenOut = [];
    if ($R !== null) {
        $schemaByCol = [];
        foreach (editor_reference_schema()[$resource] ?? [] as $rule) {
            $schemaByCol[$rule['column']] ??= $rule['codec'];
        }
        foreach (editor_data_references($resource, $row) as $ref) {
            $refTarget = (string)$ref['target'];
            $k = $ref['column'] . '|' . $refTarget . '|' . $ref['id'];
            if (isset($seenOut[$k])) {
                continue;
            }
            $seenOut[$k] = true;
            $resolved = ai_resolve_target_summary($db, $refTarget, (int)$ref['id'], $role, $guestPages);
            $outbound[] = array_merge($resolved, [
                'column' => $ref['column'],
                'codec' => $schemaByCol[$ref['column']] ?? 'id',
            ]);
        }
    }

    // 1. Входящие прямые SQL-ссылки (только по доступным текущему пользователю ресурсам).
    $inbound = [];
    $tableMap = ai_table_to_resource_map();
    foreach (editor_reverse_refs($targetTable) as $rev) {
        $srcTable = $rev['table'];
        $srcRes = $tableMap[$srcTable] ?? null;
        if ($srcRes === null || !ai_can_read_resource($srcRes, $role, $guestPages)) {
            continue;
        }
        if (!ai_table_exists($db, $srcTable)) {
            continue;
        }
        $srcR = $resources[$srcRes];
        $srcPk = editor_identifier($srcR['pk'], 'pk');
        $srcCol = editor_identifier($rev['column'], 'col');
        $srcLabelCol = !empty($srcR['label']) ? editor_identifier($srcR['label'], 'label') : null;
        $sel = $srcLabelCol ? "`$srcPk`, `$srcLabelCol`" : "`$srcPk`";
        $tblSql = editor_identifier($srcTable, 'table');
        $stmt = $db->prepare("SELECT $sel FROM `$tblSql` WHERE `$srcCol` = ? ORDER BY `$srcPk` ASC LIMIT 50");
        if ($stmt) {
            $stmt->bind_param('i', $recId);
            $stmt->execute();
            $res = $stmt->get_result();
            while ($hit = $res->fetch_assoc()) {
                $inbound[] = [
                    'source_resource' => $srcRes,
                    'source_table' => $srcTable,
                    'record_id' => (int)$hit[$srcPk],
                    'record_label' => $srcLabelCol ? (string)($hit[$srcLabelCol] ?? '') : null,
                    'field' => $rev['column'],
                    'codec' => 'id',
                    'fk_action' => $rev['action'],
                ];
            }
            $stmt->close();
        }
    }

    // 2. Входящие DSL / JSON / токен ссылки по схеме reference-schema.json.
    $refSchema = editor_reference_schema();
    foreach ($refSchema as $srcRes => $rules) {
        if (!ai_can_read_resource($srcRes, $role, $guestPages)) {
            continue;
        }
        $srcR = $resources[$srcRes] ?? null;
        if ($srcR === null || !ai_table_exists($db, $srcR['table'])) {
            continue;
        }
        $nonIdCols = [];
        $codecByCol = [];
        foreach ($rules as $rule) {
            $ruleTarget = (string)($rule['target'] ?? '');
            $ruleCodec = (string)($rule['codec'] ?? '');
            if ($ruleCodec !== 'id' && ($ruleTarget === $resource || $ruleTarget === $targetTable || $ruleCodec === 'json')) {
                $nonIdCols[$rule['column']] = true;
                $codecByCol[$rule['column']] = $ruleCodec;
            }
        }
        if (!$nonIdCols) {
            continue;
        }
        $srcPk = editor_identifier($srcR['pk'], 'pk');
        $srcLabelCol = !empty($srcR['label']) ? editor_identifier($srcR['label'], 'label') : null;
        $cols = array_values(array_unique(array_merge([$srcPk], $srcLabelCol ? [$srcLabelCol] : [], array_keys($nonIdCols))));
        $colSql = implode(', ', array_map(static fn(string $c): string => '`' . editor_identifier($c, 'c') . '`', $cols));
        $tblSql = editor_identifier($srcR['table'], 'table');

        $likeParts = [];
        $types = '';
        $params = [];
        foreach (array_keys($nonIdCols) as $col) {
            $cSql = editor_identifier($col, 'c');
            $likeParts[] = "CAST(`$cSql` AS CHAR) LIKE ?";
            $types .= 's';
            $params[] = '%' . $recId . '%';
        }
        $stmt = $db->prepare("SELECT $colSql FROM `$tblSql` WHERE " . implode(' OR ', $likeParts) . ' LIMIT 200');
        if (!$stmt) {
            continue;
        }
        $stmt->bind_param($types, ...$params);
        $stmt->execute();
        $res = $stmt->get_result();
        while ($hit = $res->fetch_assoc()) {
            foreach (editor_data_references($srcRes, $hit) as $ref) {
                $rt = (string)$ref['target'];
                if (($rt === $resource || $rt === $targetTable) && (int)$ref['id'] === $recId) {
                    $inbound[] = [
                        'source_resource' => $srcRes,
                        'source_table' => $srcR['table'],
                        'record_id' => (int)$hit[$srcPk],
                        'record_label' => $srcLabelCol ? (string)($hit[$srcLabelCol] ?? '') : null,
                        'field' => $ref['column'],
                        'codec' => $codecByCol[$ref['column']] ?? 'embedded',
                    ];
                }
            }
        }
        $stmt->close();
    }

    // 3. Входящие ссылки из файлов сценариев (если scenarios доступны).
    if (ai_can_read_resource('scenarios', $role, $guestPages)) {
        foreach (scenario_list_files('', $scenariosDir) as $fileMeta) {
            $read = scenario_read_file($fileMeta['filename'], $scenariosDir);
            if (!$read['ok']) {
                continue;
            }
            foreach ($read['references'] as $ref) {
                if ($ref['target'] === $targetTable && (int)$ref['id'] === $recId) {
                    $inbound[] = [
                        'source_resource' => 'scenarios',
                        'source_table' => 'scenarios',
                        'filename' => $read['filename'],
                        'field' => 'content',
                        'codec' => 'scenario_json',
                        'path' => $ref['path'],
                    ];
                }
            }
        }
    }

    return [
        'resource' => $resource,
        'table' => $targetTable,
        'id' => $recId,
        'label' => $R && !empty($R['label']) ? ($decoded[$R['label']] ?? null) : ($decoded['Name'] ?? null),
        'outbound' => $outbound,
        'inbound' => $inbound,
    ];
}

// ── Валидация и предпросмотр операций (validate / preview) ───────────────────

/** RFC 7396 JSON Merge Patch: безопасно объединяет патч с существующим объектом. */
function ai_json_merge_patch(mixed $target, mixed $patch): mixed
{
    if (!is_array($patch) || ($patch !== [] && array_is_list($patch))) {
        return $patch;
    }
    $out = (is_array($target) && ($target === [] || !array_is_list($target))) ? $target : [];
    foreach ($patch as $key => $val) {
        if ($val === null) {
            unset($out[$key]);
        } else {
            $out[$key] = ai_json_merge_patch($out[$key] ?? null, $val);
        }
    }
    return $out;
}

function ai_require_scope(string $scope): void
{
    if (!ai_actor_has_scope($scope)) {
        throw new CrInputError(
            "AI-токен не имеет права «{$scope}» (выданы права: " . implode(', ', ai_actor_scopes()) . ').',
            403,
            [
                'code' => 'scope_forbidden',
                'required_scope' => $scope,
                'granted_scopes' => ai_actor_scopes(),
            ]
        );
    }
}

/** Проверяет, что у текущего актора (токена/сессии) есть все нужные права на операции. */
function ai_assert_operation_scopes(array $requiredScopes): void
{
    foreach ($requiredScopes as $scope) {
        ai_require_scope((string)$scope);
    }
}

/**
 * Извлечение массива операций из JSON-тела запроса и авто-назначение tempId
 * для операций create/copy БД, где tempId не указан явно.
 * Принимает синонимы поля типа: `type` и `action`.
 *
 * @return array<int, array<string, mixed>>
 */
function ai_extract_operations_payload(array $body): array
{
    if (array_key_exists('operations', $body)) {
        $ops = $body['operations'];
    } elseif (array_key_exists('operation', $body) && is_array($body['operation'])) {
        $ops = [$body['operation']];
    } else {
        throw new CrInputError('Тело запроса должно содержать массив "operations".', 400, [
            'code' => 'missing_operations',
            'field' => 'operations',
        ]);
    }
    if (!is_array($ops) || !array_is_list($ops) || $ops === []) {
        throw new CrInputError('Поле "operations" должно быть непустым массивом операций.', 400, [
            'code' => 'empty_operations',
            'field' => 'operations',
        ]);
    }

    $usedTemps = [];
    foreach ($ops as $op) {
        if (is_array($op) && isset($op['tempId']) && is_numeric($op['tempId'])) {
            $usedTemps[(int)$op['tempId']] = true;
        }
    }
    $nextTemp = GUEST_TEMP_ID_BASE + 1;
    $out = [];
    foreach ($ops as $op) {
        if (is_array($op)) {
            if (!isset($op['type']) && isset($op['action'])) {
                $op['type'] = $op['action'];
                unset($op['action']);
            }
            $opType = (string)($op['type'] ?? '');
            if (in_array($opType, ['create', 'copy'], true) && ($op['resource'] ?? '') !== 'scenarios' && (!isset($op['tempId']) || $op['tempId'] === null || $op['tempId'] === '')) {
                while (isset($usedTemps[$nextTemp]) && $nextTemp <= GUEST_TEMP_ID_MAX) {
                    $nextTemp++;
                }
                $usedTemps[$nextTemp] = true;
                $op['tempId'] = $nextTemp;
                $nextTemp++;
            }
        }
        $out[] = $op;
    }
    return $out;
}

/** Определение опасных изменений, требующих явного подтверждения confirm_dangerous. */
function ai_detect_dangerous_operations(?mysqli $db, array $normalizedOps, ?string $scenariosDir = null): array
{
    $reasons = [];
    $resources = editor_resources();
    $highImpact = ['worlds' => true, 'dungeons' => true, 'vouchers' => true];

    if (count($normalizedOps) > 10) {
        $reasons[] = [
            'code' => 'bulk_operations',
            'severity' => 'high',
            'message' => 'Пакет содержит более 10 операций (' . count($normalizedOps) . ') — требуется подтверждение.',
        ];
    }

    foreach ($normalizedOps as $idx => $op) {
        $type = (string)($op['type'] ?? '');
        $resKey = (string)($op['resource'] ?? '');

        if ($type === 'delete') {
            $reasons[] = [
                'code' => 'delete_operation',
                'operationIndex' => $idx,
                'resource' => $resKey,
                'id' => $op['id'] ?? ($op['filename'] ?? null),
                'severity' => 'high',
                'message' => $resKey === 'scenarios'
                    ? 'Удаление файла сценария «' . (string)($op['filename'] ?? '') . '» необратимо после одобрения.'
                    : "Удаление записи {$resKey} #" . (string)($op['id'] ?? '') . ' может нарушить игровые ссылки.',
            ];
            continue;
        }

        if ($type === 'update' && isset($highImpact[$resKey])) {
            $reasons[] = [
                'code' => 'high_impact_resource',
                'operationIndex' => $idx,
                'resource' => $resKey,
                'id' => $op['id'] ?? null,
                'severity' => 'medium',
                'message' => "Изменение критического ресурса «{$resKey}» требует подтверждения администратора и агента.",
            ];
        }

        if ($type === 'update' && $resKey === 'scenarios') {
            $fn = (string)($op['filename'] ?? '');
            $cur = scenario_read_file($fn, $scenariosDir);
            $nextContent = $op['data']['content'] ?? null;
            if (!empty($cur['ok']) && is_array($cur['content']) && is_array($nextContent)) {
                $removedKeys = [];
                foreach (array_keys($cur['content']) as $k) {
                    if (!array_key_exists($k, $nextContent) && !empty($cur['content'][$k])) {
                        $removedKeys[] = (string)$k;
                    }
                }
                if ($removedKeys !== []) {
                    $reasons[] = [
                        'code' => 'scenario_section_removed',
                        'operationIndex' => $idx,
                        'resource' => 'scenarios',
                        'filename' => $fn,
                        'keys' => $removedKeys,
                        'severity' => 'high',
                        'message' => "Обновление сценария «{$fn}» удаляет непустые разделы: " . implode(', ', $removedKeys),
                    ];
                }
            }
        }

        if ($type === 'update' && $resKey !== 'scenarios' && $db !== null && isset($resources[$resKey], $op['id'])) {
            $R = $resources[$resKey];
            $jsonCols = $R['json'] ?? [];
            $data = $op['data'] ?? [];
            $existing = null;
            foreach ($jsonCols as $jc) {
                if (!array_key_exists($jc, $data)) {
                    continue;
                }
                $existing ??= crud_fetch_row($db, $R, (int)$op['id']);
                if (!$existing) {
                    break;
                }
                $oldVal = decode_json_cols($existing, [$jc])[$jc] ?? null;
                $newVal = $data[$jc];
                if (is_string($newVal) && trim($newVal) !== '') {
                    $dec = json_decode($newVal, true);
                    if (is_array($dec)) {
                        $newVal = $dec;
                    }
                }
                $oldNonEmpty = is_array($oldVal) && $oldVal !== [];
                $newEmpty = ($newVal === null || $newVal === '' || $newVal === []);
                if ($oldNonEmpty && $newEmpty) {
                    $reasons[] = [
                        'code' => 'clearing_json_field',
                        'operationIndex' => $idx,
                        'resource' => $resKey,
                        'id' => (int)$op['id'],
                        'field' => $jc,
                        'severity' => 'high',
                        'message' => "Операция очищает непустое JSON-поле «{$jc}» у {$resKey} #{$op['id']}.",
                    ];
                }
            }
        }
    }

    return [
        'requires_confirmation' => ($reasons !== []),
        'dangerous_actions' => $reasons,
    ];
}

/**
 * Комплексная проверка пакета операций (БД + сценарии) без изменения БД/диска.
 * Возвращает структурированные ошибки с полями code, operationIndex, resource, field, message.
 */
function ai_validate_operations(
    ?mysqli $db,
    array $rawOps,
    string $role,
    ?array $guestPages = null,
    ?string $scenariosDir = null,
    int $maxOps = 200
): array {
    $errors = [];
    $warnings = [];
    $resources = editor_resources();
    $addErr = static function (string $code, ?int $idx, string $resource, ?string $field, string $message, array $extra = []) use (&$errors): void {
        $errors[] = array_merge([
            'code' => $code,
            'operationIndex' => $idx,
            'resource' => $resource,
            'field' => $field,
            'message' => $message,
        ], $extra);
    };

    if (count($rawOps) > $maxOps) {
        $addErr('too_many_operations', null, '', 'operations', "Слишком много операций ({$maxOps} максимум).");
        return ['valid' => false, 'errors' => $errors, 'warnings' => $warnings, 'normalized_operations' => []];
    }

    $tempKnown = [];
    $deletedMap = [];
    $rowTypeSeen = [];
    $scenarioFilesSeen = [];
    $requiredScopes = [];

    // 1-й проход: структурная проверка операций, copy/patch_json резолв, прав доступа и сбор tempId / deletedMap.
    $preValidated = [];
    foreach ($rawOps as $idx => $raw) {
        if (!is_array($raw)) {
            $addErr('invalid_operation', $idx, '', null, 'Каждая операция должна быть JSON-объектом.');
            continue;
        }
        $origType = (string)($raw['type'] ?? ($raw['action'] ?? ''));
        $resKey = (string)($raw['resource'] ?? '');

        if (!in_array($origType, ['create', 'update', 'copy', 'delete'], true)) {
            $addErr('invalid_operation_type', $idx, $resKey, 'type', "Неизвестный тип операции «{$origType}»; допустимо: create, update, copy, delete.", ['allowed' => ['create', 'update', 'copy', 'delete']]);
            continue;
        }
        $scopeForOp = $origType === 'copy' ? 'create' : $origType;
        $requiredScopes[$scopeForOp] = true;
        $type = $origType === 'copy' ? 'create' : $origType;

        if ($resKey === '') {
            $addErr('missing_required_field', $idx, '', 'resource', 'Не указан ресурс операции.');
            continue;
        }

        if ($resKey === 'scenarios') {
            if (!ai_can_write_resource('scenarios', $role, $guestPages)) {
                $addErr('resource_forbidden', $idx, 'scenarios', 'resource', 'Доступ на изменение файлов сценариев не выдан вашей учётной записи.');
                continue;
            }
            $fnRaw = $raw['filename'] ?? ($raw['target_filename'] ?? ($raw['id'] ?? null));
            if ($origType === 'copy') {
                $srcRaw = $raw['source_filename'] ?? ($raw['source_id'] ?? ($raw['from'] ?? null));
                if ($srcRaw === null || $srcRaw === '') {
                    $addErr('missing_required_field', $idx, 'scenarios', 'source_filename', 'Операция copy для сценария требует source_filename.');
                    continue;
                }
                $srcRead = scenario_read_file($srcRaw, $scenariosDir);
                if (!$srcRead['ok']) {
                    $addErr((string)($srcRead['code'] ?? 'scenario_not_found'), $idx, 'scenarios', 'source_filename', (string)$srcRead['error']);
                    continue;
                }
                if ($fnRaw === null || $fnRaw === '') {
                    $addErr('missing_required_field', $idx, 'scenarios', 'filename', 'Укажите новое имя filename для копии сценария.');
                    continue;
                }
            }

            $fnCheck = scenario_resolve_path($fnRaw, $scenariosDir, false);
            if (!$fnCheck['ok']) {
                $addErr((string)($fnCheck['code'] ?? 'invalid_filename'), $idx, 'scenarios', 'filename', (string)$fnCheck['error']);
                continue;
            }
            $filename = (string)$fnCheck['filename'];
            $scKey = 'scenarios:' . strtolower($filename);
            if (isset($scenarioFilesSeen[$scKey])) {
                $addErr('duplicate_scenario_target', $idx, 'scenarios', 'filename', "Повторяющаяся операция над файлом сценария «{$filename}» в одном пакете.");
                continue;
            }
            $scenarioFilesSeen[$scKey] = $type;

            if ($type === 'create' && $fnCheck['exists']) {
                $addErr('scenario_exists', $idx, 'scenarios', 'filename', "Файл сценария «{$filename}» уже существует; используйте type=update.");
            } elseif ($type !== 'create' && !$fnCheck['exists']) {
                $addErr('scenario_not_found', $idx, 'scenarios', 'filename', "Файл сценария «{$filename}» не найден.");
            }

            if ($type !== 'create' && $fnCheck['exists'] && !empty($raw['baseHash'])) {
                $curRaw = @file_get_contents((string)$fnCheck['path']);
                $curHash = is_string($curRaw) ? hash('sha256', $curRaw) : '';
                if ($curHash !== (string)$raw['baseHash']) {
                    $addErr('base_hash_mismatch', $idx, 'scenarios', 'baseHash', "Файл сценария «{$filename}» изменился с момента чтения (baseHash не совпадает).");
                }
            }

            $rawContent = $raw['content'] ?? ($raw['data']['content'] ?? ($raw['data'] ?? null));
            if ($origType === 'copy' && isset($srcRead) && !empty($srcRead['ok'])) {
                $baseContent = $srcRead['content'];
                if (isset($raw['patch_json'])) {
                    if (!is_array($raw['patch_json']) || ($raw['patch_json'] !== [] && array_is_list($raw['patch_json']))) {
                        $addErr('invalid_patch_json', $idx, 'scenarios', 'patch_json', 'Поле patch_json должно быть JSON-объектом.');
                        continue;
                    }
                    $rawContent = ai_json_merge_patch($baseContent, $raw['patch_json']);
                } elseif ($rawContent === null) {
                    $rawContent = $baseContent;
                }
            } elseif ($type === 'update' && isset($raw['patch_json'])) {
                if (!is_array($raw['patch_json']) || ($raw['patch_json'] !== [] && array_is_list($raw['patch_json']))) {
                    $addErr('invalid_patch_json', $idx, 'scenarios', 'patch_json', 'Поле patch_json должно быть JSON-объектом.');
                    continue;
                }
                if ($fnCheck['exists']) {
                    $curRead = scenario_read_file($filename, $scenariosDir);
                    if (!$curRead['ok']) {
                        $addErr((string)($curRead['code'] ?? 'scenario_not_found'), $idx, 'scenarios', 'filename', (string)$curRead['error']);
                        continue;
                    }
                    $rawContent = ai_json_merge_patch($curRead['content'], $raw['patch_json']);
                }
            }

            $preValidated[$idx] = [
                'type' => $type,
                'resource' => 'scenarios',
                'filename' => $filename,
                'baseHash' => isset($raw['baseHash']) && is_string($raw['baseHash']) ? $raw['baseHash'] : null,
                'content' => $rawContent,
            ];
            continue;
        }

        $R = $resources[$resKey] ?? null;
        if ($R === null) {
            $addErr('unknown_resource', $idx, $resKey, 'resource', "Неизвестный ресурс «{$resKey}».");
            continue;
        }
        $R['key'] = $resKey;
        if (!ai_can_write_resource($resKey, $role, $guestPages)) {
            $addErr('resource_forbidden', $idx, $resKey, 'resource', "Ресурс «{$resKey}» недоступен для изменения вашей учётной записи.");
            continue;
        }

        if ($type === 'create') {
            $tempIdRaw = $raw['tempId'] ?? null;
            if (!is_numeric($tempIdRaw) || (float)$tempIdRaw !== floor((float)$tempIdRaw)) {
                $addErr('invalid_temp_id', $idx, $resKey, 'tempId', 'Некорректный временный ID (tempId).');
                continue;
            }
            $tempId = (int)$tempIdRaw;
            if (!is_guest_temp_id($tempId)) {
                $addErr('invalid_temp_id', $idx, $resKey, 'tempId', 'Временный ID должен быть в диапазоне ' . GUEST_TEMP_ID_BASE . '..' . GUEST_TEMP_ID_MAX . '.');
                continue;
            }
            if (isset($tempKnown[$tempId])) {
                $addErr('duplicate_temp_id', $idx, $resKey, 'tempId', "Повторяющийся временный ID #{$tempId}.");
                continue;
            }
            $rawData = is_array($raw['data'] ?? null) ? $raw['data'] : [];

            if ($origType === 'copy') {
                $srcRawId = $raw['source_id'] ?? ($raw['from'] ?? ($raw['id'] ?? null));
                $allowZero = ($resKey === 'worlds');
                if ((!is_int($srcRawId) && !(is_string($srcRawId) && preg_match('/^-?\d+$/', trim($srcRawId)) === 1))
                    || (int)$srcRawId < ($allowZero ? 0 : 1) || is_guest_temp_id($srcRawId)) {
                    $addErr('invalid_id', $idx, $resKey, 'source_id', 'Операция copy требует корректный source_id существующей записи.');
                    continue;
                }
                if ($db === null) {
                    $addErr('db_required', $idx, $resKey, 'source_id', 'Для операции copy требуется подключение к базе данных.');
                    continue;
                }
                $srcRow = crud_fetch_row($db, $R, (int)$srcRawId);
                if ($srcRow === null) {
                    $addErr('record_not_found', $idx, $resKey, 'source_id', "Исходная запись {$R['title']} #{$srcRawId} не найдена для копирования.");
                    continue;
                }
                $decodedSrc = decode_json_cols($srcRow, $R['json'] ?? []);
                $cloned = [];
                foreach ($R['columns'] as $colName) {
                    if (array_key_exists($colName, $decodedSrc) && $decodedSrc[$colName] !== null) {
                        $cloned[$colName] = $decodedSrc[$colName];
                    }
                }
                $rawData = array_merge($cloned, $rawData);
            }

            if (isset($raw['patch_json'])) {
                if (!is_array($raw['patch_json']) || ($raw['patch_json'] !== [] && array_is_list($raw['patch_json']))) {
                    $addErr('invalid_patch_json', $idx, $resKey, 'patch_json', 'Поле patch_json должно быть объектом вида {"ColumnName": {...patch...}}.');
                    continue;
                }
                $jsonColSet = array_flip($R['json'] ?? []);
                foreach ($raw['patch_json'] as $pCol => $pVal) {
                    if (!isset($jsonColSet[(string)$pCol])) {
                        $addErr('invalid_patch_column', $idx, $resKey, (string)$pCol, "Поле «{$pCol}» не является JSON-колонкой ресурса «{$resKey}».", ['allowed' => $R['json'] ?? []]);
                        continue 2;
                    }
                    $curVal = $rawData[$pCol] ?? null;
                    if (is_string($curVal) && trim($curVal) !== '') {
                        $curVal = json_decode($curVal, true);
                    }
                    $rawData[$pCol] = ai_json_merge_patch(is_array($curVal) ? $curVal : [], $pVal);
                }
            }

            if ($rawData === [] || (array_is_list($rawData) && $rawData !== [])) {
                $addErr('empty_data', $idx, $resKey, 'data', 'Операция create требует непустой объект data.');
                continue;
            }
            $tempKnown[$tempId] = $resKey;
            $preValidated[$idx] = ['type' => 'create', 'resource' => $resKey, 'R' => $R, 'tempId' => $tempId, 'rawData' => $rawData];
            continue;
        }

        // update / delete
        $rawId = $raw['id'] ?? null;
        $allowZero = ($resKey === 'worlds');
        if ((!is_int($rawId) && !(is_string($rawId) && preg_match('/^-?\d+$/', trim($rawId)) === 1))
            || (int)$rawId < ($allowZero ? 0 : 1) || is_guest_temp_id($rawId)) {
            $addErr('invalid_id', $idx, $resKey, 'id', $allowZero
                ? "Поле id для «{$resKey}» должно быть целым числом >= 0."
                : "Поле id для «{$resKey}» должно быть положительным целым числом.");
            continue;
        }
        $id = (int)$rawId;
        $rk = "{$resKey}:{$id}";
        if (isset($rowTypeSeen[$rk]) && ($rowTypeSeen[$rk] === 'delete' || $type === 'delete')) {
            $addErr('conflicting_operations', $idx, $resKey, 'id', "Запись {$resKey} #{$id} не может одновременно изменяться и удаляться в одном пакете.");
            continue;
        }
        $rowTypeSeen[$rk] = $type;

        $expectedBefore = $raw['expected_before'] ?? null;
        if ($expectedBefore !== null && (!is_array($expectedBefore) || ($expectedBefore !== [] && array_is_list($expectedBefore)))) {
            $addErr('invalid_expected_before', $idx, $resKey, 'expected_before', 'Поле expected_before должно быть объектом ожидаемых текущих значений.');
            continue;
        }

        if ($type === 'delete') {
            $deletedMap[$rk] = true;
            $preValidated[$idx] = ['type' => 'delete', 'resource' => $resKey, 'R' => $R, 'id' => $id, 'rawData' => [], 'expected_before' => $expectedBefore];
            continue;
        }

        $rawData = is_array($raw['data'] ?? null) ? $raw['data'] : [];
        if (isset($raw['patch_json'])) {
            if (!is_array($raw['patch_json']) || ($raw['patch_json'] !== [] && array_is_list($raw['patch_json']))) {
                $addErr('invalid_patch_json', $idx, $resKey, 'patch_json', 'Поле patch_json должно быть объектом вида {"ColumnName": {...patch...}}.');
                continue;
            }
            $jsonColSet = array_flip($R['json'] ?? []);
            foreach (array_keys($raw['patch_json']) as $pCol) {
                if (!isset($jsonColSet[(string)$pCol])) {
                    $addErr('invalid_patch_column', $idx, $resKey, (string)$pCol, "Поле «{$pCol}» не является JSON-колонкой ресурса «{$resKey}».", ['allowed' => $R['json'] ?? []]);
                    continue 2;
                }
            }
            $existingRow = $db !== null ? crud_fetch_row($db, $R, $id) : null;
            if ($db !== null && $existingRow === null) {
                $addErr('record_not_found', $idx, $resKey, 'id', "Запись {$R['title']} #{$id} не найдена в базе данных.");
                continue;
            }
            $decodedExisting = $existingRow ? decode_json_cols($existingRow, $R['json'] ?? []) : [];
            foreach ($raw['patch_json'] as $pCol => $pVal) {
                $baseVal = $rawData[$pCol] ?? ($decodedExisting[$pCol] ?? []);
                if (is_string($baseVal) && trim($baseVal) !== '') {
                    $decBase = json_decode($baseVal, true);
                    $baseVal = is_array($decBase) ? $decBase : [];
                }
                $rawData[$pCol] = ai_json_merge_patch(is_array($baseVal) ? $baseVal : [], $pVal);
            }
        }

        if ($rawData === [] || (array_is_list($rawData) && $rawData !== [])) {
            $addErr('empty_data', $idx, $resKey, 'data', 'Операция update требует непустой объект data или patch_json.');
            continue;
        }
        $preValidated[$idx] = ['type' => 'update', 'resource' => $resKey, 'R' => $R, 'id' => $id, 'rawData' => $rawData, 'expected_before' => $expectedBefore];
    }

    // 2-й проход: проверка неизвестных полей, типов, enum/set, DSL, JSON, существования записей и ссылок.
    $normalizedOps = [];
    $tableToRes = ai_table_to_resource_map();
    $refExistCache = [];

    foreach ($preValidated as $idx => $item) {
        if ($item['resource'] === 'scenarios') {
            if ($item['type'] !== 'delete') {
                $scCheck = scenario_validate_content($item['content'], $db, $tempKnown, $deletedMap);
                foreach ($scCheck['errors'] as $e) {
                    $addErr((string)$e['code'], $idx, 'scenarios', (string)$e['field'], (string)$e['message'], isset($e['allowed']) ? ['allowed' => $e['allowed']] : []);
                }
                foreach ($scCheck['warnings'] as $w) {
                    $warnings[] = array_merge($w, ['operationIndex' => $idx, 'resource' => 'scenarios']);
                }
                $normContent = scenario_normalize_content($item['content']);
                if ($normContent['ok']) {
                    $normalizedOps[] = [
                        'type' => $item['type'],
                        'resource' => 'scenarios',
                        'filename' => $item['filename'],
                        'baseHash' => $item['baseHash'],
                        'data' => ['content' => $normContent['content']],
                    ];
                }
            } else {
                $normalizedOps[] = [
                    'type' => 'delete',
                    'resource' => 'scenarios',
                    'filename' => $item['filename'],
                    'baseHash' => $item['baseHash'],
                    'data' => [],
                ];
            }
            continue;
        }

        $R = $item['R'];
        $resKey = $item['resource'];
        $rawData = $item['rawData'];
        $unknownCols = array_values(array_diff(array_keys($rawData), $R['columns'], [$R['pk']]));
        if ($unknownCols) {
            foreach ($unknownCols as $uCol) {
                $addErr('unknown_field', $idx, $resKey, (string)$uCol, "Поле «{$uCol}» не существует в ресурсе «{$resKey}».", ['allowed' => $R['columns']]);
            }
            continue;
        }

        // Проверка существования целевой записи и expected_before для update/delete.
        if ($db !== null && $item['type'] !== 'create') {
            $existing = crud_fetch_row($db, $R, (int)$item['id']);
            if ($existing === null) {
                $addErr('record_not_found', $idx, $resKey, 'id', "Запись {$R['title']} #{$item['id']} не найдена в базе данных.");
                continue;
            }
            if (!empty($item['expected_before']) && is_array($item['expected_before'])) {
                $decodedCur = decode_json_cols($existing, $R['json'] ?? []);
                foreach ($item['expected_before'] as $eCol => $eVal) {
                    $curVal = $decodedCur[$eCol] ?? null;
                    if (is_array($eVal) || is_array($curVal)) {
                        $same = json_encode($curVal) === json_encode($eVal);
                    } else {
                        $same = (string)$curVal === (string)$eVal;
                    }
                    if (!$same) {
                        $addErr('version_conflict', $idx, $resKey, (string)$eCol, "Запись {$R['title']} #{$item['id']} изменилась в БД: поле «{$eCol}» больше не совпадает с expected_before.");
                        continue 2;
                    }
                }
            }
        }

        $dbData = [];
        if ($item['type'] !== 'delete') {
            try {
                $dbData = cr_normalize_data($R, $rawData);
            } catch (Throwable $e) {
                $addErr('invalid_data', $idx, $resKey, 'data', editor_safe_message($e));
                continue;
            }

            $fieldCheck = ai_schema_validate_db_record(
                $resKey,
                $item['type'],
                $rawData,
                $dbData,
                $idx,
                $db,
                $tempKnown,
                $deletedMap,
                $item['id'] ?? null
            );
            foreach ($fieldCheck['errors'] as $e) {
                $errors[] = $e;
            }
            foreach ($fieldCheck['warnings'] as $w) {
                $warnings[] = $w;
            }

            // Проверка всех ссылок (прямых и вложенных JSON/DSL).
            $lookups = ai_schema_lookups();
            foreach (editor_data_references($resKey, $dbData) as $ref) {
                $rawTarget = (string)$ref['target'];
                if (isset($resources[$rawTarget])) {
                    $targetRes = $rawTarget;
                    $targetTable = $resources[$rawTarget]['table'];
                } elseif (isset($lookups[$rawTarget])) {
                    $targetRes = $rawTarget;
                    $targetTable = $lookups[$rawTarget]['table'];
                } else {
                    $targetTable = $rawTarget;
                    $targetRes = $tableToRes[$targetTable] ?? '';
                }
                $refId = (int)$ref['id'];

                if ($targetRes !== '' && isset($deletedMap["{$targetRes}:{$refId}"])) {
                    $addErr('deleted_reference_target', $idx, $resKey, $ref['column'], "Поле «{$ref['column']}» ссылается на {$targetTable} #{$refId}, удаляемую в этом же запросе.");
                    continue;
                }
                if (is_guest_temp_id($refId)) {
                    if (!isset($tempKnown[$refId])) {
                        $addErr('missing_temp_reference', $idx, $resKey, $ref['column'], "Временный ID #{$refId} в поле «{$ref['column']}» отсутствует среди создаваемых записей этого запроса.");
                    } elseif ($targetRes !== '' && $tempKnown[$refId] !== $targetRes) {
                        $addErr('invalid_reference_target', $idx, $resKey, $ref['column'], "Поле «{$ref['column']}»: временный ID #{$refId} относится к «{$tempKnown[$refId]}», а требуется «{$targetRes}».");
                    }
                    continue;
                }
                if ($db !== null && ($targetTable === 'tw_worlds' ? $refId >= 0 : $refId > 0)) {
                    $cKey = "{$targetTable}:{$refId}";
                    if (!array_key_exists($cKey, $refExistCache)) {
                        if (!ai_table_exists($db, $targetTable)) {
                            $refExistCache[$cKey] = true;
                        } else {
                            $tblSql = editor_identifier($targetTable, 'table');
                            $stmt = $db->prepare("SELECT 1 FROM `$tblSql` WHERE `ID` = ? LIMIT 1");
                            if ($stmt) {
                                $stmt->bind_param('i', $refId);
                                $stmt->execute();
                                $refExistCache[$cKey] = ($stmt->get_result()->fetch_row() !== null);
                                $stmt->close();
                            } else {
                                $refExistCache[$cKey] = true;
                            }
                        }
                    }
                    if (!$refExistCache[$cKey]) {
                        $addErr('missing_reference', $idx, $resKey, $ref['column'], "Поле «{$ref['column']}» ссылается на несуществующую запись {$targetTable} #{$refId}.");
                    }
                }
            }
        }

        $normalizedOps[] = [
            'type' => $item['type'],
            'resource' => $resKey,
            'id' => $item['id'] ?? null,
            'tempId' => $item['tempId'] ?? null,
            'data' => $rawData,
        ];
    }

    // Проверяем нормализацию и порядок зависимостей (вкл. циклы FK) через change-requests-lib.
    if ($errors === [] && $normalizedOps !== []) {
        try {
            cr_normalize_ops($normalizedOps, $maxOps);
        } catch (Throwable $e) {
            $addErr('dependency_or_order_error', null, '', null, editor_safe_message($e));
        }
    }

    $danger = $normalizedOps !== []
        ? ai_detect_dangerous_operations($db, $normalizedOps, $scenariosDir)
        : ['requires_confirmation' => false, 'dangerous_actions' => []];

    return [
        'valid' => ($errors === []),
        'errors' => $errors,
        'warnings' => $warnings,
        'normalized_operations' => $normalizedOps,
        'required_scopes' => array_keys($requiredScopes),
        'requires_confirmation' => $danger['requires_confirmation'],
        'dangerous_actions' => $danger['dangerous_actions'],
    ];
}

/** Добавление компактного path-level JSON diff в результат анализа cr_analyze. */
function ai_enrich_analysis_diffs(array $analysis): array
{
    if (!isset($analysis['operations']) || !is_array($analysis['operations'])) {
        return $analysis;
    }
    foreach ($analysis['operations'] as &$op) {
        if (!isset($op['changes']) || !is_array($op['changes'])) {
            continue;
        }
        foreach ($op['changes'] as &$change) {
            if (empty($change['json'])) {
                continue;
            }
            $beforeVal = is_string($change['before'] ?? null) ? json_decode($change['before'], true) : ($change['before'] ?? null);
            $afterVal = is_string($change['after'] ?? null) ? json_decode($change['after'], true) : ($change['after'] ?? null);
            if ($beforeVal !== null || $afterVal !== null) {
                $change['json_diff'] = scenario_json_diff($beforeVal, $afterVal);
            }
        }
        unset($change);
    }
    unset($op);
    return $analysis;
}
