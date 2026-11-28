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
require_once __DIR__ . '/map-analysis.php';
require_once __DIR__ . '/map-render.php';
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
    static $cache = [];
    $scopes = ai_actor_scopes();
    $cacheKey = $role . '|' . ($guestPages === null ? '*' : implode(',', $guestPages)) . '|' . implode(',', $scopes) . '|' . ($includeExamples ? '1' : '0');
    if (isset($cache[$cacheKey])) {
        return $cache[$cacheKey];
    }

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
            'title' => 'Карты миров (визуальный, структурный и семантический анализ + скриншоты)',
            'pk' => 'world_id',
            'permissions' => ['read' => true, 'propose' => false],
            'tile_size_px' => 32,
            'supports_semantic_analysis' => true,
            'supports_structure_recognition' => true,
            'supports_screenshot' => true,
            'screenshot_modes' => array_keys(ai_map_screenshot_modes()),
            'structure_kinds' => array_keys(ai_map_structure_kinds_catalog()),
        ];
    }

    if (count($cache) >= 16) {
        array_shift($cache);
    }
    return $cache[$cacheKey] = $out;
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
            'llms_txt' => 'llms.txt',
            'llms_txt_api' => 'api/ai.php?action=llms_txt',
            'plugin_manifest' => 'api/ai.php?action=discovery',
            'openapi' => 'api/ai.php?action=openapi',
            'openapi_direct' => 'openapi.json',
            'mcp_http' => 'api/mcp.php',
            'mcp_direct' => 'mcp',
            'rest_api' => 'api/ai.php',
        ],
        'capabilities' => [
            'model_agnostic' => true,
            'auto_resource_discovery' => true,
            'openapi_3_1' => true,
            'mcp_jsonrpc_2_0' => true,
            'llms_txt' => true,
            'bearer_tokens' => true,
            'scoped_rbac' => true,
            'global_search' => true,
            'composite_context' => true,
            'batch_get' => true,
            'advanced_filtering_and_sorting' => true,
            'symbolic_refs' => true,
            'self_healing_validation_hints' => true,
            'patch_json_merge' => true,
            'copy_records' => true,
            'dangerous_confirmation_gate' => true,
            'optimistic_concurrency' => true,
            'idempotent_propose' => true,
            'change_request_lifecycle' => true,
            'scenario_analysis' => true,
            'scenario_corpus_dependency_check' => true,
            'similar_records_search' => true,
            'similar_records_create_warning' => true,
            'verify_before_propose' => true,
            'map_semantic_analysis' => true,
            'map_structure_recognition' => true,
            'map_screenshot_rendering' => true,
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
            'llms_txt' => ['method' => 'GET', 'params' => [], 'auth' => 'public'],
            'manifest' => ['method' => 'GET', 'params' => ['include_schemas?']],
            'resources' => ['method' => 'GET', 'params' => ['resource?', 'include_examples?']],
            'search' => ['method' => 'GET', 'params' => ['q|search', 'resources?', 'limit_per_resource?'], 'auth' => 'read', 'mutates' => false],
            'context' => ['method' => 'GET', 'params' => ['resource', 'id|file'], 'auth' => 'read', 'mutates' => false],
            'list' => ['method' => 'GET', 'params' => ['resource', 'search?', 'filter?', 'filter_key?', 'filter_value?', 'filter_values?', 'sort?', 'fields?', 'limit?', 'offset?', 'max_bytes?', 'decode?']],
            'get' => ['method' => 'GET', 'params' => ['resource', 'id|ids|file|world|path', 'fields?', 'summary_only?', 'include_references?', 'include_entities?', 'include_minimap?', 'target?', 'x?', 'y?']],
            'references' => ['method' => 'GET', 'params' => ['resource? & id?', 'source? & search?', 'limit?', 'offset?', 'max_bytes?']],
            'history' => ['method' => 'GET', 'params' => ['resource?', 'id?', 'limit?', 'offset?']],
            'request' => ['method' => 'GET', 'params' => ['id'], 'auth' => 'read', 'mutates' => false],
            'request_reply' => ['method' => 'POST', 'body' => ['id', 'body'], 'auth' => 'create|update', 'mutates' => 'sqlite_change_requests_only'],
            'request_withdraw' => ['method' => 'POST', 'body' => ['id'], 'auth' => 'update|delete', 'mutates' => 'sqlite_change_requests_only'],
            'analyze' => ['method' => 'GET', 'params' => ['resource=scenarios|maps', 'id?|file?|world?|path?', 'target?', 'structure_kind?', 'x?', 'y?', 'radius?', 'search?', 'compact?'], 'auth' => 'read', 'mutates' => false],
            'map_screenshot' => ['method' => 'GET', 'params' => ['world|world_id|path', 'mode=hybrid|visual|schematic?', 'target?', 'format=json|png?', 'x?', 'y?', 'radius?', 'width?', 'height?', 'highlight_x?', 'highlight_y?'], 'auth' => 'read', 'mutates' => false],
            'similar' => ['method' => 'GET', 'params' => ['resource', 'name|q', 'limit?'], 'auth' => 'read', 'mutates' => false],
            'validate' => ['method' => 'POST', 'body' => ['operations' => 'array<op>'], 'mutates' => false],
            'preview' => ['method' => 'POST', 'body' => ['operations' => 'array<op>'], 'mutates' => false],
            'apply_patch' => ['method' => 'POST', 'body' => ['resource', 'id|filename', 'patch_json', 'data?', 'baseHash?', 'expected_before?'], 'auth' => 'read', 'mutates' => false],
            'propose' => ['method' => 'POST', 'body' => ['message', 'nickname?', 'operations', 'confirm_dangerous?', 'idempotency_key?', 'request_id?', 'version?'], 'mutates' => 'sqlite_change_requests_only'],
        ],
        'temp_id_convention' => [
            'min' => GUEST_TEMP_ID_BASE,
            'max' => GUEST_TEMP_ID_MAX,
            'auto_assign_when_omitted' => true,
            'symbolic_refs' => true,
            'note' => 'Database create/copy operations can declare "ref": "my_alias" and reference it anywhere in the same batch as "$my_alias" (in FK columns, DSL strings like "[$my_ore/3]", or JSON payloads). Explicit numeric tempId (2000000000..2000999999) or auto-assignment also remain fully supported.',
        ],
        'recommended_workflow' => [
            'orient' => 'Orient in 1 call via GET action=search&q=<term> (cross-resource search) and GET action=context&resource=worlds|quests|items|bots_info&id=<id> (complete related snapshot).',
            'analyze' => 'Inspect scenarios (GET action=analyze&resource=scenarios[&id=file.json]) and world maps (GET action=analyze&resource=maps&world=<id> + GET action=map_screenshot&world=<id>&target=overview) before authoring.',
            'similar' => 'Before creating any object, find likely duplicates: GET api/ai.php?action=similar&resource=<resource>&name=<Name>. A create operation also returns warning similar_records_found when similar rows exist.',
            'read' => 'Read one or multiple records in one call: GET action=get&resource=<res>&id=<id> or &ids=1,2,3 (with include_references=1; for scenarios keep sha256 as baseHash).',
            'verify' => 'Apply patch_json server-side without writing: POST action=apply_patch {resource, id|filename, patch_json, baseHash?}. It returns before/after/diff plus the same errors action=propose would produce.',
            'submit' => 'Validate -> preview -> propose (using ref/"$alias" to link new rows in one batch). Track admin feedback, reply, revise, or withdraw pending requests via GET action=request / POST action=request_reply / POST action=request_withdraw.',
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
    $allowedFilterOps = ['eq', 'ne', 'gt', 'gte', 'lt', 'lte', 'contains', 'prefix', 'like', 'in', 'is_null', 'not_null'];
    if (is_array($filters)) {
        foreach ($filters as $col => $val) {
            if (!is_string($col) || !isset($allowedSet[$col])) {
                $suggest = ai_schema_suggest_closest((string)$col, $allowedCols, 3);
                $errExtra = [
                    'code' => 'unknown_filter_field',
                    'field' => (string)$col,
                    'allowed' => $allowedCols,
                ];
                if ($suggest !== []) {
                    $errExtra['did_you_mean'] = $suggest;
                    $errExtra['hint'] = 'Возможно, вы имели в виду: ' . implode(', ', $suggest) . '.';
                }
                throw new CrInputError("Неизвестное поле фильтрации «{$col}» для ресурса «{$R['key']}».", 400, $errExtra);
            }
            $colId = editor_identifier($col, 'filter column');
            if ($val === null) {
                $where[] = "`$colId` IS NULL";
            } elseif (is_array($val) && ($val === [] || array_is_list($val))) {
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
            } elseif (is_array($val)) {
                // Объект операторов сравнения: {"gte": 5, "lte": 15, "contains": "Iron", "not_null": true}
                foreach ($val as $op => $opVal) {
                    $opKey = strtolower(trim((string)$op));
                    if (!in_array($opKey, $allowedFilterOps, true)) {
                        throw new CrInputError("Неизвестный оператор фильтрации «{$op}» для поля «{$col}».", 400, [
                            'code' => 'invalid_filter_operator',
                            'field' => $col,
                            'allowed' => $allowedFilterOps,
                            'did_you_mean' => ai_schema_suggest_closest($opKey, $allowedFilterOps, 3),
                        ]);
                    }
                    if ($opKey === 'is_null') {
                        $where[] = $opVal ? "`$colId` IS NULL" : "`$colId` IS NOT NULL";
                    } elseif ($opKey === 'not_null') {
                        $where[] = $opVal ? "`$colId` IS NOT NULL" : "`$colId` IS NULL";
                    } elseif ($opKey === 'in') {
                        if (!is_array($opVal) || $opVal === []) {
                            $where[] = '1 = 0';
                        } else {
                            $vals = array_values($opVal);
                            $placeholders = implode(', ', array_fill(0, count($vals), '?'));
                            $where[] = "`$colId` IN ($placeholders)";
                            foreach ($vals as $item) {
                                $types .= is_int($item) ? 'i' : 's';
                                $params[] = $item;
                            }
                        }
                    } elseif ($opKey === 'contains' || $opKey === 'prefix' || $opKey === 'like') {
                        $rawPat = (string)$opVal;
                        if ($opKey === 'prefix') {
                            $pat = $rawPat . '%';
                        } elseif ($opKey === 'like' && (str_contains($rawPat, '%') || str_contains($rawPat, '_'))) {
                            $pat = $rawPat;
                        } else {
                            $pat = '%' . $rawPat . '%';
                        }
                        $where[] = "CAST(`$colId` AS CHAR) LIKE ?";
                        $types .= 's';
                        $params[] = $pat;
                    } else {
                        $sqlOp = match ($opKey) {
                            'eq' => '=',
                            'ne' => '<>',
                            'gt' => '>',
                            'gte' => '>=',
                            'lt' => '<',
                            'lte' => '<=',
                        };
                        if ($opVal === null && ($opKey === 'eq' || $opKey === 'ne')) {
                            $where[] = $opKey === 'eq' ? "`$colId` IS NULL" : "`$colId` IS NOT NULL";
                        } else {
                            $where[] = "`$colId` {$sqlOp} ?";
                            if (is_bool($opVal)) {
                                $types .= 'i';
                                $params[] = $opVal ? 1 : 0;
                            } elseif (is_int($opVal)) {
                                $types .= 'i';
                                $params[] = $opVal;
                            } elseif (is_float($opVal)) {
                                $types .= 'd';
                                $params[] = $opVal;
                            } else {
                                $types .= 's';
                                $params[] = (string)$opVal;
                            }
                        }
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

    $sortRaw = $opts['sort'] ?? null;
    if (is_string($sortRaw) && trim($sortRaw) !== '') {
        $sortClauses = [];
        foreach (explode(',', $sortRaw) as $part) {
            $part = trim($part);
            if ($part === '') {
                continue;
            }
            $dir = 'ASC';
            $colName = $part;
            if (str_starts_with($part, '-')) {
                $dir = 'DESC';
                $colName = substr($part, 1);
            } elseif (str_starts_with($part, '+')) {
                $colName = substr($part, 1);
            } elseif (str_contains($part, ':')) {
                [$colName, $dirRaw] = explode(':', $part, 2);
                $dirLower = strtolower(trim($dirRaw));
                if (!in_array($dirLower, ['asc', 'desc'], true)) {
                    throw new CrInputError("Некорректное направление сортировки «{$dirRaw}» (используйте asc или desc).", 400, [
                        'code' => 'invalid_sort',
                        'field' => 'sort',
                        'allowed' => ['asc', 'desc'],
                    ]);
                }
                $dir = strtoupper($dirLower);
            }
            $colName = trim($colName);
            if (!isset($allowedSet[$colName])) {
                $suggest = ai_schema_suggest_closest($colName, $allowedCols, 3);
                $errExtra = [
                    'code' => 'unknown_sort_field',
                    'field' => 'sort',
                    'allowed' => $allowedCols,
                ];
                if ($suggest !== []) {
                    $errExtra['did_you_mean'] = $suggest;
                }
                throw new CrInputError("Неизвестное поле сортировки «{$colName}» для ресурса «{$R['key']}».", 400, $errExtra);
            }
            $sortClauses[] = '`' . editor_identifier($colName, 'sort column') . '` ' . $dir;
            if (count($sortClauses) >= 4) {
                break;
            }
        }
        if ($sortClauses !== []) {
            $order = implode(', ', $sortClauses);
        }
    }

    $limit = clamp_int($opts['limit'] ?? 50, 1, 500, 50);
    $offset = clamp_int($opts['offset'] ?? 0, 0, 10000000, 0);
    $skipCount = !empty($opts['skip_count']);
    $fetchLimit = $skipCount ? ($limit + 1) : $limit;

    $total = null;
    if (!$skipCount) {
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
    }

    $sql = "SELECT $colSql FROM `$table`$whereSql ORDER BY $order LIMIT ? OFFSET ?";
    $stmt = $db->prepare($sql);
    if ($stmt === false) {
        throw new RuntimeException($db->error ?: 'Ошибка подготовки выборки');
    }
    $allTypes = $types . 'ii';
    $allParams = array_merge($params, [$fetchLimit, $offset]);
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

    if ($total === null) {
        if (count($rows) > $limit) {
            array_pop($rows);
            $countSql = "SELECT COUNT(*) FROM `$table`$whereSql";
            $stmtCount = $db->prepare($countSql);
            if ($stmtCount !== false) {
                if ($types !== '') {
                    $stmtCount->bind_param($types, ...$params);
                }
                $stmtCount->execute();
                $resCount = $stmtCount->get_result();
                $total = (int)($resCount ? ($resCount->fetch_row()[0] ?? 0) : ($offset + count($rows) + 1));
                $stmtCount->close();
            } else {
                $total = $offset + count($rows) + 1;
            }
        } else {
            $total = $offset + count($rows);
        }
    }

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
    $limit = clamp_int($opts['limit'] ?? 200, 1, 1000, 200);
    $offset = clamp_int($opts['offset'] ?? 0, 0, 10000000, 0);
    $skipCount = !empty($opts['skip_count']);
    $fetchLimit = $skipCount ? ($limit + 1) : $limit;

    $total = null;
    if (!$skipCount) {
        $stmtCount = $db->prepare("SELECT COUNT(*) FROM `$table`$whereSql");
        if ($types !== '') {
            $stmtCount->bind_param($types, ...$params);
        }
        $stmtCount->execute();
        $total = (int)($stmtCount->get_result()->fetch_row()[0] ?? 0);
        $stmtCount->close();
    }

    $stmt = $db->prepare("SELECT $colSql FROM `$table`$whereSql ORDER BY `$idCol` ASC LIMIT ? OFFSET ?");
    $stmt->bind_param($types . 'ii', ...array_merge($params, [$fetchLimit, $offset]));
    $stmt->execute();
    $rows = [];
    $items = [];
    $res = $stmt->get_result();
    while ($row = $res->fetch_assoc()) {
        $rows[] = $row;
        $items[] = ['value' => (int)$row[$idCol], 'label' => (string)$row[$labelCol]];
    }
    $stmt->close();

    if ($total === null) {
        if (count($rows) > $limit) {
            array_pop($rows);
            array_pop($items);
            $stmtCount = $db->prepare("SELECT COUNT(*) FROM `$table`$whereSql");
            if ($types !== '') {
                $stmtCount->bind_param($types, ...$params);
            }
            $stmtCount->execute();
            $total = (int)($stmtCount->get_result()->fetch_row()[0] ?? ($offset + count($rows) + 1));
            $stmtCount->close();
        } else {
            $total = $offset + count($rows);
        }
    }

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
 * (backwards-compatible game/switch stats + rich visual, structural,
 * terrain, zone, and semantic building recognition) without leaking paths.
 *
 * @param array<string, mixed> $extraOptions
 * @return array<string, mixed>
 */
function ai_map_parse_datafile(
    string $bytes,
    bool $includeEntities = false,
    ?int $pointX = null,
    ?int $pointY = null,
    ?mysqli $db = null,
    ?int $worldId = null,
    string $role = EDITOR_ROLE_ADMIN,
    ?array $guestPages = null,
    array $extraOptions = []
): array {
    $opts = array_merge([
        'include_entities' => $includeEntities,
        'x' => $pointX,
        'y' => $pointY,
    ], $extraOptions);

    $full = ai_map_analyze_full($bytes, $db, $worldId, $role, $guestPages, $opts);

    // Keep legacy key aliases intact so existing callers & tests remain 100% compatible.
    $full['game_layer']['unhookable_tiles'] = (int)($full['game_layer']['nohook_tiles'] ?? 0);
    if ($includeEntities) {
        $full['game_entities'] = $full['game_layer']['entities'] ?? [];
        $full['entities_truncated'] = (bool)($full['game_layer']['entities_truncated'] ?? false);
    }

    // Populate legacy switch_layer fields (entries_count, switch_numbers, switch_types).
    $swNumbers = [];
    foreach ($full['zones']['named_zones'] ?? [] as $z) {
        $swNumbers[(int)$z['number']] = true;
    }
    foreach ($full['zones']['action_zones'] ?? [] as $az) {
        $swNumbers[(int)$az['number']] = true;
    }
    foreach ($full['structures']['items'] ?? [] as $st) {
        if (isset($st['house_zone_number'])) {
            $swNumbers[(int)$st['house_zone_number']] = true;
        }
    }
    $swNumKeys = array_keys($swNumbers);
    sort($swNumKeys, SORT_NUMERIC);
    $full['switch_layer']['entries_count'] = (int)($full['switch_layer']['non_empty_tiles'] ?? 0);
    $full['switch_layer']['switch_numbers'] = $swNumKeys;
    $full['switch_layer']['switch_types'] = $full['switch_layer']['type_counts'] ?? [];

    if (isset($full['point_check']) && is_array($full['point_check'])) {
        $pc = &$full['point_check'];
        $pc['game_tile_id'] = $pc['in_bounds'] ? ($pc['game_tile'] ?? null) : null;
        $pc['collision_blocked'] = !empty($pc['solid']) || !empty($pc['death']);
        $pc['switch_tile'] = $pc['switch_at_point'] ?? null;
        unset($pc);
    }

    return $full;
}

/**
 * Shared handler for full/region map analysis (used by REST action=analyze&resource=maps
 * and MCP tool analyze_map).
 *
 * @param array<string, mixed> $options
 * @return array<string, mixed>
 */
function ai_map_analyze_request(
    ?mysqli $db,
    ?int $worldId,
    ?string $rawPath,
    string $role,
    ?array $guestPages,
    array $options = []
): array {
    $loaded = ai_map_load_bytes($db, $worldId, $rawPath);
    if (!$loaded['available'] || !is_string($loaded['bytes'])) {
        return [
            'resource' => 'maps',
            'world' => $loaded['world'],
            'available' => false,
            'reason' => $loaded['reason'] ?? 'Файл карты недоступен',
        ];
    }

    $effectiveWorldId = $worldId ?? (isset($loaded['world']['ID']) ? (int)$loaded['world']['ID'] : null);
    $hasExplicitRegion = isset($options['target']) || isset($options['tx0']) || isset($options['x0'])
        || (isset($options['x'], $options['y']) && (isset($options['radius']) || isset($options['radius_tiles'])));

    $extraOpts = [
        'include_minimap' => !array_key_exists('include_minimap', $options) || !empty($options['include_minimap']),
    ];
    if ($hasExplicitRegion) {
        $extraOpts['viewport_params'] = $options;
        if (isset($options['pad_tiles'])) {
            $extraOpts['pad_tiles'] = (int)$options['pad_tiles'];
        }
    }

    $full = ai_map_parse_datafile(
        $loaded['bytes'],
        !empty($options['include_entities']),
        isset($options['x']) && $options['x'] !== null ? (int)$options['x'] : null,
        isset($options['y']) && $options['y'] !== null ? (int)$options['y'] : null,
        $db,
        $effectiveWorldId,
        $role,
        $guestPages,
        $extraOpts
    );

    // Optional structure kind filter so AI can ask specifically for castles or houses.
    $filterKind = isset($options['structure_kind']) ? trim((string)$options['structure_kind']) : '';
    if ($filterKind !== '') {
        $filtered = array_values(array_filter(
            $full['structures']['items'] ?? [],
            static fn(array $s): bool => ($s['kind'] ?? '') === $filterKind
        ));
        $full['structures']['filtered_kind'] = $filterKind;
        $full['structures']['filtered_total'] = count($filtered);
        $full['structures']['items'] = $filtered;
    }

    // Compact mode trims verbose per-layer arrays when AI wants focused structure/terrain overview.
    if (!empty($options['compact'])) {
        $full['visual_layers']['tile_layers'] = array_slice($full['visual_layers']['tile_layers'] ?? [], 0, 15);
        $full['visual_layers']['quad_layers'] = array_slice($full['visual_layers']['quad_layers'] ?? [], 0, 10);
        $full['structures']['items'] = array_slice($full['structures']['items'] ?? [], 0, 40);
    }

    return [
        'resource' => 'maps',
        'world' => $loaded['world'],
        'available' => true,
        'source' => $loaded['source'],
        'basename' => $loaded['basename'] ?? null,
        'analysis' => $full,
    ];
}

/**
 * Shared handler for rendering a visual/hybrid/schematic map screenshot PNG
 * (used by REST action=map_screenshot and MCP tool get_map_screenshot).
 *
 * @param array<string, mixed> $options
 * @return array<string, mixed>
 */
function ai_map_screenshot_request(
    ?mysqli $db,
    ?int $worldId,
    ?string $rawPath,
    string $role,
    ?array $guestPages,
    array $options = []
): array {
    $loaded = ai_map_load_bytes($db, $worldId, $rawPath);
    if (!$loaded['available'] || !is_string($loaded['bytes'])) {
        throw new CrInputError($loaded['reason'] ?? 'Файл карты не найден на сервере.', 404, ['code' => 'map_not_found']);
    }

    $effectiveWorldId = $worldId ?? (isset($loaded['world']['ID']) ? (int)$loaded['world']['ID'] : null);
    $analysis = ai_map_analyze_full($loaded['bytes'], $db, $effectiveWorldId, $role, $guestPages, [
        'include_entities' => false,
    ]);
    $rendered = ai_map_render_screenshot($loaded['bytes'], $analysis, $options);

    return [
        'resource' => 'maps',
        'world' => $loaded['world'],
        'source' => $loaded['source'],
        'basename' => $loaded['basename'] ?? null,
        'map_dimensions' => [
            'width_tiles' => $analysis['width_tiles'],
            'height_tiles' => $analysis['height_tiles'],
            'width_px' => $analysis['width_px'],
            'height_px' => $analysis['height_px'],
        ],
        'overview_text' => $analysis['overview_text'],
        'screenshot' => $rendered,
    ];
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

/**
 * Пакетное разрешение списка целей `[['target' => ..., 'id' => int], ...]` за 1 SQL-запрос на таблицу
 * вместо N одиночных SELECT-ов. Полностью соблюдает гостевой/lookup ACL.
 *
 * @param array<int, array{target:string, id:int}> $pairs
 * @return array<string, array<string, mixed>> Ключ: "$tableOrResource:$id"
 */
function ai_resolve_targets_batch(mysqli $db, array $pairs, string $role, ?array $guestPages = null): array
{
    if ($pairs === []) {
        return [];
    }
    $resources = editor_resources();
    $lookups = ai_schema_lookups();
    $tableMap = ai_table_to_resource_map();

    $out = [];
    // Группируем разрешённые уникальные ID по таблице.
    $byTable = [];
    foreach ($pairs as $pair) {
        $tableOrResource = (string)$pair['target'];
        $id = (int)$pair['id'];
        $outKey = $tableOrResource . ':' . $id;
        if (isset($out[$outKey])) {
            continue;
        }
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
            $out[$outKey] = ['target_table' => $table, 'target_resource' => $targetRes, 'id' => $id, 'accessible' => false];
            continue;
        }

        if (!isset($byTable[$table])) {
            $R = $targetRes !== null ? ($resources[$targetRes] ?? null) : null;
            $pkCol = $R ? editor_identifier($R['pk'], 'pk') : 'ID';
            $labelCol = $R && !empty($R['label']) ? $R['label'] : (in_array($table, ['tw_attributes', 'tw_skills_list'], true) ? 'Name' : null);
            $byTable[$table] = [
                'table' => $table,
                'target_resource' => $targetRes,
                'pk' => $pkCol,
                'label' => $labelCol,
                'ids' => [],
                'keys' => [],
            ];
        }
        $byTable[$table]['ids'][$id] = true;
        $byTable[$table]['keys'][$outKey] = [$targetRes, $id];
    }

    foreach ($byTable as $group) {
        $table = $group['table'];
        $pkCol = $group['pk'];
        $labelCol = $group['label'];
        $ids = array_keys($group['ids']);
        $found = [];

        if (ai_table_exists($db, $table) && $ids !== []) {
            $tableId = editor_identifier($table, 'table');
            $cols = $labelCol ? "`$pkCol`, `" . editor_identifier($labelCol, 'label') . '`' : "`$pkCol`";
            foreach (array_chunk($ids, 200) as $chunk) {
                $ph = implode(', ', array_fill(0, count($chunk), '?'));
                $stmt = $db->prepare("SELECT $cols FROM `$tableId` WHERE `$pkCol` IN ($ph)");
                if ($stmt) {
                    $stmt->bind_param(str_repeat('i', count($chunk)), ...$chunk);
                    $stmt->execute();
                    $res = $stmt->get_result();
                    while ($row = $res->fetch_assoc()) {
                        $rid = (int)$row[$pkCol];
                        $found[$rid] = ($labelCol !== null && isset($row[$labelCol])) ? (string)$row[$labelCol] : null;
                    }
                    $stmt->close();
                }
            }
        }

        foreach ($group['keys'] as $outKey => [$targetRes, $id]) {
            $exists = array_key_exists($id, $found);
            $out[$outKey] = [
                'target_table' => $table,
                'target_resource' => $targetRes,
                'id' => $id,
                'accessible' => true,
                'exists' => $exists,
                'label' => $exists ? $found[$id] : null,
            ];
        }
    }

    return $out;
}

function ai_references_inspect(
    mysqli $db,
    string $resource,
    int|string $id,
    string $role,
    ?array $guestPages = null,
    ?string $scenariosDir = null,
    ?array $prefetchedRow = null
): array {
    if ($resource === 'scenarios') {
        $read = scenario_read_file((string)$id, $scenariosDir);
        if (!$read['ok']) {
            throw new CrInputError((string)$read['error'], ($read['code'] ?? '') === 'scenario_not_found' ? 404 : 400, ['code' => $read['code'] ?? 'invalid_filename']);
        }
        $batchPairs = [];
        foreach ($read['references'] as $ref) {
            $batchPairs[] = ['target' => (string)$ref['target'], 'id' => (int)$ref['id']];
        }
        $resolvedMap = ai_resolve_targets_batch($db, $batchPairs, $role, $guestPages);
        $outbound = [];
        foreach ($read['references'] as $ref) {
            $key = $ref['target'] . ':' . (int)$ref['id'];
            $resolved = $resolvedMap[$key] ?? ai_resolve_target_summary($db, $ref['target'], (int)$ref['id'], $role, $guestPages);
            $outbound[] = array_merge($resolved, ['source' => 'scenario_json', 'path' => $ref['path']]);
        }

        // Поиск в items.Data (use_scenario.name) с SQL-префильтром по имени файла.
        $inbound = [];
        if (ai_can_read_resource('items', $role, $guestPages) && ai_table_exists($db, 'tw_items_list')) {
            $base = preg_replace('/\.json$/i', '', (string)$read['filename']);
            $likePat = '%' . $base . '%';
            $stmt = $db->prepare("SELECT `ID`, `Name`, `Data` FROM `tw_items_list` WHERE `Data` IS NOT NULL AND `Data` <> '' AND `Data` LIKE ?");
            if ($stmt) {
                $stmt->bind_param('s', $likePat);
                $stmt->execute();
                $res = $stmt->get_result();
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
                $stmt->close();
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
    if ($prefetchedRow !== null) {
        $row = $prefetchedRow;
        $decoded = $R !== null ? decode_json_cols($row, $R['json'] ?? []) : $row;
    } elseif ($R !== null) {
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
        $pendingOut = [];
        $batchPairs = [];
        foreach (editor_data_references($resource, $row) as $ref) {
            $refTarget = (string)$ref['target'];
            $k = $ref['column'] . '|' . $refTarget . '|' . $ref['id'];
            if (isset($seenOut[$k])) {
                continue;
            }
            $seenOut[$k] = true;
            $pendingOut[] = $ref;
            $batchPairs[] = ['target' => $refTarget, 'id' => (int)$ref['id']];
        }
        $resolvedMap = ai_resolve_targets_batch($db, $batchPairs, $role, $guestPages);
        foreach ($pendingOut as $ref) {
            $refTarget = (string)$ref['target'];
            $key = $refTarget . ':' . (int)$ref['id'];
            $resolved = $resolvedMap[$key] ?? ai_resolve_target_summary($db, $refTarget, (int)$ref['id'], $role, $guestPages);
            $outbound[] = array_merge($resolved, [
                'column' => $ref['column'],
                'codec' => $schemaByCol[$ref['column']] ?? 'id',
            ]);
        }
    }

    // 1. Входящие прямые SQL-ссылки: группируем по исходной таблице (1 SQL вместо 5–8 для bots_info/bots_mobs).
    $inbound = [];
    $tableMap = ai_table_to_resource_map();
    $revByTable = [];
    foreach (editor_reverse_refs($targetTable) as $rev) {
        $srcTable = $rev['table'];
        $srcRes = $tableMap[$srcTable] ?? null;
        if ($srcRes === null || !ai_can_read_resource($srcRes, $role, $guestPages)) {
            continue;
        }
        $revByTable[$srcTable]['resource'] = $srcRes;
        $revByTable[$srcTable]['cols'][] = $rev;
    }

    foreach ($revByTable as $srcTable => $grp) {
        if (!ai_table_exists($db, $srcTable)) {
            continue;
        }
        $srcRes = $grp['resource'];
        $srcR = $resources[$srcRes];
        $srcPk = editor_identifier($srcR['pk'], 'pk');
        $srcLabelCol = !empty($srcR['label']) ? editor_identifier($srcR['label'], 'label') : null;
        $tblSql = editor_identifier($srcTable, 'table');

        $revCols = $grp['cols'];
        $selCols = [$srcPk];
        if ($srcLabelCol !== null && !in_array($srcLabelCol, $selCols, true)) {
            $selCols[] = $srcLabelCol;
        }
        $whereParts = [];
        $types = '';
        $params = [];
        foreach ($revCols as $rev) {
            $cSql = editor_identifier($rev['column'], 'col');
            if (!in_array($rev['column'], $selCols, true)) {
                $selCols[] = $rev['column'];
            }
            $whereParts[] = "`$cSql` = ?";
            $types .= 'i';
            $params[] = $recId;
        }
        $selSql = implode(', ', array_map(static fn(string $c): string => '`' . editor_identifier($c, 'c') . '`', $selCols));
        $limit = min(200, 50 * count($revCols));
        $stmt = $db->prepare("SELECT $selSql FROM `$tblSql` WHERE " . implode(' OR ', $whereParts) . " ORDER BY `$srcPk` ASC LIMIT $limit");
        if ($stmt) {
            $stmt->bind_param($types, ...$params);
            $stmt->execute();
            $res = $stmt->get_result();
            $perColCount = [];
            while ($hit = $res->fetch_assoc()) {
                foreach ($revCols as $rev) {
                    $colName = $rev['column'];
                    if (isset($hit[$colName]) && (int)$hit[$colName] === $recId) {
                        $perColCount[$colName] = ($perColCount[$colName] ?? 0) + 1;
                        if ($perColCount[$colName] > 50) {
                            continue;
                        }
                        $inbound[] = [
                            'source_resource' => $srcRes,
                            'source_table' => $srcTable,
                            'record_id' => (int)$hit[$srcPk],
                            'record_label' => $srcLabelCol ? (string)($hit[$srcLabelCol] ?? '') : null,
                            'field' => $colName,
                            'codec' => 'id',
                            'fk_action' => $rev['action'],
                        ];
                    }
                }
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
 * Разрешает символические ссылки (`"ref": "my_sword"` -> `"$my_sword"`) внутри
 * пакета операций до валидации: подставляет детерминированные tempId (2000000001..),
 * работая в FK-полях, JSON-полях (Data, TasksData, ScenarioData, scenarios) и MRPG DSL
 * (`RequiredItems: "[$ore/3]"`, `Trades`, `Properties`, `EquippedModules`).
 *
 * @return array{ops: array<int, mixed>, resolved_refs: array<string, int>, errors: array<int, array<string, mixed>>}
 */
function ai_resolve_symbolic_refs(array $rawOps): array
{
    $refMap = [];
    $errors = [];
    $usedTemps = [];

    foreach ($rawOps as $op) {
        if (is_array($op) && isset($op['tempId']) && is_numeric($op['tempId'])) {
            $usedTemps[(int)$op['tempId']] = true;
        }
    }
    $nextTemp = GUEST_TEMP_ID_BASE + 1;

    // 1. Собираем объявления "ref" на create/copy операциях БД.
    $ops = $rawOps;
    foreach ($ops as $idx => $op) {
        if (!is_array($op) || !array_key_exists('ref', $op) || $op['ref'] === null || $op['ref'] === '') {
            continue;
        }
        $resKey = (string)($op['resource'] ?? '');
        $origType = (string)($op['type'] ?? ($op['action'] ?? ''));
        if (!is_string($op['ref']) || preg_match('/^[A-Za-z_][A-Za-z0-9_-]{0,47}$/', trim($op['ref'])) !== 1) {
            $errors[] = [
                'code' => 'invalid_symbolic_ref',
                'operationIndex' => $idx,
                'resource' => $resKey,
                'field' => 'ref',
                'message' => 'Некорректное имя символической ссылки ref (ожидается латиница/цифры/_/-, 1..48 симв., начинается с буквы или _).',
            ];
            continue;
        }
        $alias = trim($op['ref']);
        if (!in_array($origType, ['create', 'copy'], true) || $resKey === 'scenarios') {
            $errors[] = [
                'code' => 'invalid_symbolic_ref',
                'operationIndex' => $idx,
                'resource' => $resKey,
                'field' => 'ref',
                'message' => "Символическая ссылка ref=«{$alias}» поддерживается только для create/copy операций БД.",
            ];
            continue;
        }
        if (isset($refMap[$alias])) {
            $errors[] = [
                'code' => 'duplicate_symbolic_ref',
                'operationIndex' => $idx,
                'resource' => $resKey,
                'field' => 'ref',
                'message' => "Дублирующееся имя символической ссылки ref=«{$alias}» в одном пакете.",
            ];
            continue;
        }
        if (!isset($op['tempId']) || !is_numeric($op['tempId'])) {
            while (isset($usedTemps[$nextTemp]) && $nextTemp <= GUEST_TEMP_ID_MAX) {
                $nextTemp++;
            }
            $usedTemps[$nextTemp] = true;
            $ops[$idx]['tempId'] = $nextTemp;
            $nextTemp++;
        }
        $refMap[$alias] = (int)$ops[$idx]['tempId'];
    }

    $proseKeys = ['Name' => true, 'Description' => true, 'Comment' => true, 'text' => true, 'message' => true, 'GroupName' => true];
    $availableRefs = array_keys($refMap);

    $walk = static function (mixed $val, string $fieldPath, string $leafKey, int $opIdx, string $resKey) use (&$walk, $refMap, $availableRefs, $proseKeys, &$errors): mixed {
        if (is_array($val)) {
            $out = [];
            foreach ($val as $k => $v) {
                $kStr = (string)$k;
                $nextPath = $fieldPath === '' ? $kStr : "{$fieldPath}.{$kStr}";
                $out[$k] = $walk($v, $nextPath, $kStr, $opIdx, $resKey);
            }
            return $out;
        }
        if (!is_string($val) || !str_contains($val, '$')) {
            return $val;
        }
        $trimmed = trim($val);
        // Точное совпадение "$alias" -> числовой tempId
        if (preg_match('/^\$([A-Za-z_][A-Za-z0-9_-]{0,47})$/', $trimmed, $m) === 1) {
            $alias = $m[1];
            if (isset($refMap[$alias])) {
                return $refMap[$alias];
            }
            if (!isset($proseKeys[$leafKey])) {
                $sug = ai_schema_suggest_closest($alias, $availableRefs, 3);
                $err = [
                    'code' => 'unresolved_symbolic_ref',
                    'operationIndex' => $opIdx,
                    'resource' => $resKey,
                    'field' => $fieldPath,
                    'message' => "Неизвестная символическая ссылка «\${$alias}» в поле «{$fieldPath}».",
                    'available_refs' => $availableRefs,
                ];
                if ($sug !== []) {
                    $err['did_you_mean'] = array_map(static fn(string $s): string => '$' . $s, $sug);
                }
                $errors[] = $err;
            }
            return $val;
        }

        // Подстановка внутри DSL / не-прозаических строк (например "[$ore/3]", "*Shop\n[$sword/1(50)]", "$mod1,$mod2").
        if (isset($proseKeys[$leafKey])) {
            return $val;
        }
        return preg_replace_callback('/\$([A-Za-z_][A-Za-z0-9_-]{0,47})/', static function (array $m) use ($refMap, $availableRefs, $fieldPath, $opIdx, $resKey, &$errors, $val): string {
            $alias = $m[1];
            if (isset($refMap[$alias])) {
                return (string)$refMap[$alias];
            }
            // Внутри DSL-скобок [$unknown/...] или если в пакете вообще объявлены ref — сообщаем об ошибке.
            if ($availableRefs !== [] || str_contains($val, '[$' . $alias)) {
                $sug = ai_schema_suggest_closest($alias, $availableRefs, 3);
                $err = [
                    'code' => 'unresolved_symbolic_ref',
                    'operationIndex' => $opIdx,
                    'resource' => $resKey,
                    'field' => $fieldPath,
                    'message' => "Неизвестная символическая ссылка «\${$alias}» в поле «{$fieldPath}».",
                    'available_refs' => $availableRefs,
                ];
                if ($sug !== []) {
                    $err['did_you_mean'] = array_map(static fn(string $s): string => '$' . $s, $sug);
                }
                $errors[] = $err;
            }
            return $m[0];
        }, $val) ?? $val;
    };

    // 2. Подставляем ссылки в data / patch_json / content всех операций.
    foreach ($ops as $idx => $op) {
        if (!is_array($op)) {
            continue;
        }
        $resKey = (string)($op['resource'] ?? '');
        foreach (['data', 'patch_json', 'content'] as $section) {
            if (isset($op[$section])) {
                $ops[$idx][$section] = $walk($op[$section], $section, $section, $idx, $resKey);
            }
        }
    }

    return [
        'ops' => $ops,
        'resolved_refs' => $refMap,
        'errors' => $errors,
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
        return ['valid' => false, 'errors' => $errors, 'warnings' => $warnings, 'normalized_operations' => [], 'resolved_refs' => []];
    }

    $symResolved = ai_resolve_symbolic_refs($rawOps);
    $rawOps = $symResolved['ops'];
    $resolvedRefs = $symResolved['resolved_refs'];
    foreach ($symResolved['errors'] as $symErr) {
        $errors[] = $symErr;
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
                    $addErr('base_hash_mismatch', $idx, 'scenarios', 'baseHash', "Файл сценария «{$filename}» изменился с момента чтения (baseHash не совпадает).", [
                        'current_sha256' => $curHash,
                        'hint' => 'Перечитайте файл через get_record (resource=scenarios) и используйте актуальный sha256 в качестве baseHash.',
                    ]);
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
            $knownRes = array_values(array_filter(array_keys($resources), static fn(string $k): bool => ai_can_write_resource($k, $role, $guestPages)));
            if (ai_can_write_resource('scenarios', $role, $guestPages)) {
                $knownRes[] = 'scenarios';
            }
            $sug = ai_schema_suggest_closest($resKey, $knownRes, 3);
            $extra = ['allowed' => $knownRes];
            if ($sug !== []) {
                $extra['did_you_mean'] = $sug;
                $extra['hint'] = 'Возможно, имелся в виду ресурс: ' . implode(', ', $sug) . '.';
            }
            $addErr('unknown_resource', $idx, $resKey, 'resource', "Неизвестный ресурс «{$resKey}».", $extra);
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
        $preValidated[$idx] = [
            'type' => 'update',
            'resource' => $resKey,
            'R' => $R,
            'id' => $id,
            'rawData' => $rawData,
            'expected_before' => $expectedBefore,
            'existingRow' => isset($existingRow) ? $existingRow : null,
        ];
        unset($existingRow);
    }

    // 2-й проход: проверка неизвестных полей, типов, enum/set, DSL, JSON, существования записей и ссылок.
    $normalizedOps = [];
    $tableToRes = ai_table_to_resource_map();
    $refExistCache = [];
    $pendingDbRefs = [];
    $similarCreateCounts = [];
    $similarCreateQueries = [];
    if ($db !== null) {
        foreach ($preValidated as $pv) {
            if ($pv['type'] === 'create' && ($pv['resource'] ?? '') !== 'scenarios' && !empty($pv['R']['label'])) {
                $rk = (string)$pv['resource'];
                $lv = $pv['rawData'][$pv['R']['label']] ?? null;
                if (is_string($lv) && mb_strlen(trim($lv), 'UTF-8') >= 3) {
                    $similarCreateCounts[$rk] = ($similarCreateCounts[$rk] ?? 0) + 1;
                    $similarCreateQueries[$rk][] = $lv;
                }
            }
        }
    }
    $similarBatchCache = [];

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
                $sug = ai_schema_suggest_closest((string)$uCol, $R['columns'], 3);
                $extra = ['allowed' => $R['columns']];
                if ($sug !== []) {
                    $extra['did_you_mean'] = $sug;
                    $extra['hint'] = 'Возможно, вы имели в виду: ' . implode(', ', $sug) . '.';
                }
                $addErr('unknown_field', $idx, $resKey, (string)$uCol, "Поле «{$uCol}» не существует в ресурсе «{$resKey}».", $extra);
            }
            continue;
        }

        // Проверка существования целевой записи и expected_before для update/delete.
        if ($db !== null && $item['type'] !== 'create') {
            $existing = $item['existingRow'] ?? crud_fetch_row($db, $R, (int)$item['id']);
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

            // Предупреждение о похожих существующих записях при создании (анти-дубли).
            // Проверка read-only: не блокирует заявку, а подсказывает AI перепроверить.
            // При нескольких create в одном ресурсе загружаем таблицу 1 раз и переиспользуем биграммы.
            if ($db !== null && $item['type'] === 'create' && !empty($R['label'])) {
                $labelVal = $rawData[$R['label']] ?? null;
                if (is_string($labelVal) && mb_strlen(trim($labelVal), 'UTF-8') >= 3) {
                    try {
                        if (($similarCreateCounts[$resKey] ?? 0) >= 2) {
                            if (!array_key_exists($resKey, $similarBatchCache)) {
                                $similarBatchCache[$resKey] = ai_find_similar_prepare_index(
                                    $db,
                                    $resKey,
                                    $similarCreateQueries[$resKey] ?? [],
                                    $role,
                                    $guestPages
                                );
                            }
                            $similar = ai_find_similar_from_index($similarBatchCache[$resKey], $labelVal, 3, 0.62);
                        } else {
                            $similar = ai_find_similar_records($db, $resKey, $labelVal, 3, $role, $guestPages, 0.62);
                        }
                        if (is_array($similar) && $similar['candidates'] !== []) {
                            $list = [];
                            foreach ($similar['candidates'] as $cand) {
                                $list[] = $resKey . '#' . $cand['id'] . ' «' . $cand['label'] . '» (' . $cand['score_percent'] . '%)';
                            }
                            $warnings[] = [
                                'code' => 'similar_records_found',
                                'operationIndex' => $idx,
                                'resource' => $resKey,
                                'field' => (string)$R['label'],
                                'message' => 'Похожи на уже существующие записи: ' . implode('; ', $list) . '. Проверьте дубликаты (action=similar).',
                                'similar' => $similar['candidates'],
                            ];
                        }
                    } catch (Throwable) {
                        // Поиск похожих — необязательная подсказка: сбой запроса не должен ломать валидацию.
                    }
                }
            }

            // Проверка всех ссылок (прямых и вложенных JSON/DSL); DB-существование проверяется пакетом ниже.
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
                    $pendingDbRefs[] = [
                        'idx' => $idx,
                        'res' => $resKey,
                        'col' => (string)$ref['column'],
                        'table' => $targetTable,
                        'id' => $refId,
                    ];
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

    // Пакетная проверка существования всех внешних ссылок: 1 SELECT ... WHERE ID IN (...) на таблицу вместо N.
    if ($db !== null && $pendingDbRefs !== []) {
        $idsByTable = [];
        foreach ($pendingDbRefs as $pr) {
            $t = $pr['table'];
            $idVal = $pr['id'];
            $cKey = "{$t}:{$idVal}";
            if (!array_key_exists($cKey, $refExistCache)) {
                $idsByTable[$t][$idVal] = true;
            }
        }
        foreach ($idsByTable as $targetTable => $idSet) {
            $ids = array_keys($idSet);
            if (!ai_table_exists($db, $targetTable)) {
                foreach ($ids as $idVal) {
                    $refExistCache["{$targetTable}:{$idVal}"] = true;
                }
                continue;
            }
            $tblSql = editor_identifier($targetTable, 'table');
            $foundIds = [];
            foreach (array_chunk($ids, 200) as $chunk) {
                $ph = implode(', ', array_fill(0, count($chunk), '?'));
                $stmt = $db->prepare("SELECT `ID` FROM `$tblSql` WHERE `ID` IN ($ph)");
                if ($stmt) {
                    $stmt->bind_param(str_repeat('i', count($chunk)), ...$chunk);
                    $stmt->execute();
                    $res = $stmt->get_result();
                    while ($r = $res->fetch_row()) {
                        $foundIds[(int)$r[0]] = true;
                    }
                    $stmt->close();
                } else {
                    foreach ($chunk as $idVal) {
                        $foundIds[$idVal] = true;
                    }
                }
            }
            foreach ($ids as $idVal) {
                $refExistCache["{$targetTable}:{$idVal}"] = isset($foundIds[$idVal]);
            }
        }
        foreach ($pendingDbRefs as $pr) {
            $cKey = "{$pr['table']}:{$pr['id']}";
            if (empty($refExistCache[$cKey])) {
                $addErr('missing_reference', $pr['idx'], $pr['res'], $pr['col'], "Поле «{$pr['col']}» ссылается на несуществующую запись {$pr['table']} #{$pr['id']}.");
            }
        }
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
        'resolved_refs' => $resolvedRefs,
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

// ════════════════════════════════════════════════════════════════════════════
//  Search & Verify (v2): анализ сценариев, похожие записи и серверная проверка
//  patch_json перед отправкой заявки (verify-before-propose).
//
//  Канонический безопасный цикл AI-агента:
//    1) GET  action=analyze&resource=scenarios[&id=file.json]  — структура сценария,
//       зависимости (боты/предметы/миры) и места переиспользования компонентов;
//    2) GET  action=similar&resource=<res>&name=<…>            — поиск похожих
//       существующих записей до создания новой (анти-дубли);
//    3) GET  action=get …                                      — исходная запись + baseHash;
//    4) POST action=apply_patch {patch_json}                   — серверное слияние,
//       расшифровка diff и повторная проверка результата БЕЗ записи в БД/файлы;
//    5) POST action=propose                                     — только чистого пакета.
//
//  Все функции в этом разделе read-only: они ничего не пишут в игровую БД и
//  на диск, не расходуют cooldown и не требуют confirm_dangerous.
// ════════════════════════════════════════════════════════════════════════════

/** Нормализация текста для сравнения похожести: lowercase, «ё»→«е», пунктуация → пробел. */
function ai_normalize_search_text(string $s): string
{
    $s = mb_strtolower($s, 'UTF-8');
    $s = str_replace('ё', 'е', $s);
    $s = preg_replace('/[^\p{L}\p{N}]+/u', ' ', $s) ?? '';
    $s = preg_replace('/\s+/u', ' ', $s) ?? '';
    return trim($s);
}

/** Бигграммы строки с обрамляющими пробелами (для Dice-коэффициента). */
function ai_text_bigrams(string $s): array
{
    $padded = ' ' . $s . ' ';
    $plen = mb_strlen($padded, 'UTF-8');
    $out = [];
    for ($i = 0; $i + 2 <= $plen; $i++) {
        $out[] = mb_substr($padded, $i, 2, 'UTF-8');
    }
    return $out;
}

/**
 * Похожесть двух строк (0..1): Dice-коэффициент по биграммам с бонусом за
 * вхождение одной строки в другую. Регистр, пунктуация и «ё/е» игнорируются.
 */
function ai_similarity_score(string $a, string $b): float
{
    $na = ai_normalize_search_text($a);
    $nb = ai_normalize_search_text($b);
    if ($na === '' || $nb === '') {
        return 0.0;
    }
    if ($na === $nb) {
        return 1.0;
    }
    if (str_contains($na, $nb) || str_contains($nb, $na)) {
        $shorter = (float)min(mb_strlen($na, 'UTF-8'), mb_strlen($nb, 'UTF-8'));
        $longer = (float)max(mb_strlen($na, 'UTF-8'), mb_strlen($nb, 'UTF-8'));
        return round(0.7 + 0.3 * ($shorter / $longer), 4);
    }
    $g1 = array_count_values(ai_text_bigrams($na));
    $g2 = array_count_values(ai_text_bigrams($nb));
    $inter = 0;
    foreach ($g1 as $gram => $n) {
        if (isset($g2[$gram])) {
            $inter += min($n, $g2[$gram]);
        }
    }
    $total = array_sum($g1) + array_sum($g2);
    return $total > 0 ? round((2 * $inter) / $total, 4) : 0.0;
}

/** Быстрый расчёт Dice + substring similarity по предвычисленным нормализованным строкам и биграммам. */
function ai_similarity_score_precomputed(
    string $na,
    int $lenA,
    array $g1,
    int $sumG1,
    string $nb,
    int $lenB,
    array $g2,
    int $sumG2
): float {
    if ($na === '' || $nb === '') {
        return 0.0;
    }
    if ($na === $nb) {
        return 1.0;
    }
    if (str_contains($na, $nb) || str_contains($nb, $na)) {
        $shorter = (float)min($lenA, $lenB);
        $longer = (float)max($lenA, $lenB);
        return round(0.7 + 0.3 * ($shorter / $longer), 4);
    }
    $inter = 0;
    foreach ($g1 as $gram => $n) {
        if (isset($g2[$gram])) {
            $inter += min($n, $g2[$gram]);
        }
    }
    $total = $sumG1 + $sumG2;
    return $total > 0 ? round((2 * $inter) / $total, 4) : 0.0;
}

/**
 * Предзагрузка и индексирование кандидатов для пакетной проверки дублей (1 SQL на ресурс вместо N).
 *
 * @param string[] $queries
 * @return array<int, array{id:int, label:string, norm:string, len:int, grams:array<string, int>, sum:int}>|null
 */
function ai_find_similar_prepare_index(
    mysqli $db,
    string $resource,
    array $queries,
    string $role = EDITOR_ROLE_GUEST,
    ?array $guestPages = null
): ?array {
    $resources = editor_resources();
    $lookups = ai_schema_lookups();
    $table = null;
    $pk = 'ID';
    $label = null;
    if (isset($resources[$resource])) {
        if (!ai_can_read_resource($resource, $role, $guestPages)) {
            return null;
        }
        $table = (string)$resources[$resource]['table'];
        $pk = (string)$resources[$resource]['pk'];
        $label = $resources[$resource]['label'] ?? null;
    } elseif (isset($lookups[$resource])) {
        if (!ai_can_read_resource($resource, $role, $guestPages)) {
            return null;
        }
        $table = (string)$lookups[$resource]['table'];
        $pk = (string)($lookups[$resource]['pk'] ?? 'ID');
        $label = $lookups[$resource]['label'] ?? null;
    } else {
        return null;
    }
    if ($label === null) {
        return [];
    }

    $tableId = editor_identifier($table, 'table');
    $pkId = editor_identifier($pk, 'pk');
    $labelId = editor_identifier((string)$label, 'label');

    // Объединяем поисковые токены всех создаваемых записей; при <=20 токенах фильтруем в SQL.
    $allTokens = [];
    $allHaveTokens = true;
    foreach ($queries as $q) {
        $nq = ai_normalize_search_text($q);
        $toks = preg_split('/\s+/u', $nq) ?: [];
        $toks = array_values(array_unique(array_filter($toks, static fn(string $t): bool => mb_strlen($t, 'UTF-8') >= 3)));
        if ($toks === []) {
            $allHaveTokens = false;
            break;
        }
        usort($toks, static fn(string $a, string $b): int => mb_strlen($b, 'UTF-8') <=> mb_strlen($a, 'UTF-8'));
        foreach (array_slice($toks, 0, 4) as $tok) {
            $allTokens[$tok] = true;
        }
    }

    $where = '';
    $types = '';
    $params = [];
    if ($allHaveTokens && $allTokens !== [] && count($allTokens) <= 20) {
        $parts = [];
        foreach (array_keys($allTokens) as $token) {
            $parts[] = "LOWER(CAST(`{$labelId}` AS CHAR)) LIKE ?";
            $types .= 's';
            $params[] = '%' . $token . '%';
        }
        $where = ' WHERE (' . implode(' OR ', $parts) . ')';
    }

    $sql = "SELECT `{$pkId}`, `{$labelId}` FROM `{$tableId}`{$where} ORDER BY `{$pkId}` ASC LIMIT 1500";
    $stmt = $db->prepare($sql);
    if ($stmt === false) {
        return [];
    }
    if ($types !== '') {
        $stmt->bind_param($types, ...$params);
    }
    $stmt->execute();
    $res = $stmt->get_result();
    $index = [];
    if ($res) {
        while ($row = $res->fetch_assoc()) {
            $labelValue = (string)($row[$label] ?? '');
            if (trim($labelValue) === '') {
                continue;
            }
            $id = (int)($row[$pk] ?? 0);
            if (is_guest_temp_id($id)) {
                continue;
            }
            $norm = ai_normalize_search_text($labelValue);
            if ($norm === '') {
                continue;
            }
            $grams = array_count_values(ai_text_bigrams($norm));
            $index[] = [
                'id' => $id,
                'label' => $labelValue,
                'norm' => $norm,
                'len' => mb_strlen($norm, 'UTF-8'),
                'grams' => $grams,
                'sum' => array_sum($grams),
            ];
        }
    }
    $stmt->close();
    return $index;
}

/**
 * Поиск похожих записей по предвычисленному индексу `ai_find_similar_prepare_index()`.
 *
 * @param array<int, array{id:int, label:string, norm:string, len:int, grams:array<string, int>, sum:int}>|null $index
 */
function ai_find_similar_from_index(?array $index, string $query, int $limit = 5, float $threshold = 0.35): ?array
{
    if ($index === null) {
        return null;
    }
    $limit = clamp_int($limit, 1, 25, 5);
    $normQuery = ai_normalize_search_text($query);
    $lenQ = mb_strlen($normQuery, 'UTF-8');
    if ($lenQ < 2) {
        return ['candidates' => [], 'scanned' => 0, 'threshold' => $threshold];
    }
    $g1 = array_count_values(ai_text_bigrams($normQuery));
    $sumG1 = array_sum($g1);

    $candidates = [];
    foreach ($index as $entry) {
        $score = ai_similarity_score_precomputed(
            $normQuery,
            $lenQ,
            $g1,
            $sumG1,
            $entry['norm'],
            $entry['len'],
            $entry['grams'],
            $entry['sum']
        );
        if ($score < $threshold) {
            continue;
        }
        $candidates[] = [
            'id' => $entry['id'],
            'label' => $entry['label'],
            'score' => $score,
            'score_percent' => (int)round($score * 100),
        ];
    }
    usort($candidates, static fn(array $a, array $b): int => $b['score'] <=> $a['score'] ?: $a['id'] <=> $b['id']);
    return ['candidates' => array_slice($candidates, 0, $limit), 'scanned' => count($index), 'threshold' => $threshold];
}

/**
 * Поиск похожих существующих записей по названию (анти-дубли).
 * Используется endpoint action=similar и автоматическим предупреждением
 * similar_records_found при создании записей.
 *
 * Токены предфильтра содержат только буквы и цифры после нормализации,
 * поэтому LIKE без ESCAPE здесь безопасен (диких символов %/_ в них нет).
 *
 * @return array{candidates: array<int, array{id:int, label:string, score:float, score_percent:int}>, scanned: int, threshold: float}|null
 *         null — ресурс неизвестен или запрещён читающему актору.
 */
function ai_find_similar_records(mysqli $db, string $resource, string $query, int $limit = 5, string $role = EDITOR_ROLE_GUEST, ?array $guestPages = null, float $threshold = 0.35): ?array
{
    $resources = editor_resources();
    $lookups = ai_schema_lookups();
    $table = null;
    $pk = 'ID';
    $label = null;
    if (isset($resources[$resource])) {
        if (!ai_can_read_resource($resource, $role, $guestPages)) {
            return null;
        }
        $table = (string)$resources[$resource]['table'];
        $pk = (string)$resources[$resource]['pk'];
        $label = $resources[$resource]['label'] ?? null;
    } elseif (isset($lookups[$resource])) {
        if (!ai_can_read_resource($resource, $role, $guestPages)) {
            return null;
        }
        $table = (string)$lookups[$resource]['table'];
        $pk = (string)($lookups[$resource]['pk'] ?? 'ID');
        $label = $lookups[$resource]['label'] ?? null;
    } else {
        return null;
    }

    $limit = clamp_int($limit, 1, 25, 5);
    if ($label === null) {
        return ['candidates' => [], 'scanned' => 0, 'threshold' => $threshold];
    }
    $normQuery = ai_normalize_search_text($query);
    $lenQ = mb_strlen($normQuery, 'UTF-8');
    if ($lenQ < 2) {
        return ['candidates' => [], 'scanned' => 0, 'threshold' => $threshold];
    }

    $tableId = editor_identifier($table, 'table');
    $pkId = editor_identifier($pk, 'pk');
    $labelId = editor_identifier((string)$label, 'label');

    $tokens = preg_split('/\s+/u', $normQuery) ?: [];
    $tokens = array_values(array_unique(array_filter($tokens, static fn(string $t): bool => mb_strlen($t, 'UTF-8') >= 3)));
    usort($tokens, static fn(string $a, string $b): int => mb_strlen($b, 'UTF-8') <=> mb_strlen($a, 'UTF-8'));
    $tokens = array_slice($tokens, 0, 6);

    $where = '';
    $types = '';
    $params = [];
    if ($tokens !== []) {
        $parts = [];
        foreach ($tokens as $token) {
            $parts[] = "LOWER(CAST(`{$labelId}` AS CHAR)) LIKE ?";
            $types .= 's';
            $params[] = '%' . $token . '%';
        }
        $where = ' WHERE (' . implode(' OR ', $parts) . ')';
    }

    $sql = "SELECT `{$pkId}`, `{$labelId}` FROM `{$tableId}`{$where} ORDER BY `{$pkId}` ASC LIMIT 300";
    $stmt = $db->prepare($sql);
    if ($stmt === false) {
        return ['candidates' => [], 'scanned' => 0, 'threshold' => $threshold];
    }
    if ($types !== '') {
        $stmt->bind_param($types, ...$params);
    }
    $stmt->execute();
    $res = $stmt->get_result();
    $rows = [];
    if ($res) {
        while ($row = $res->fetch_assoc()) {
            $rows[] = $row;
        }
    }
    $stmt->close();

    // Предвычисляем биграммы запроса один раз вместо пересчёта на каждой строке.
    $g1 = array_count_values(ai_text_bigrams($normQuery));
    $sumG1 = array_sum($g1);

    $candidates = [];
    foreach ($rows as $row) {
        $labelValue = (string)($row[$label] ?? '');
        if (trim($labelValue) === '') {
            continue;
        }
        $id = (int)($row[$pk] ?? 0);
        if (is_guest_temp_id($id)) {
            continue;
        }
        $nb = ai_normalize_search_text($labelValue);
        if ($nb === '') {
            continue;
        }
        $lenB = mb_strlen($nb, 'UTF-8');
        if ($normQuery === $nb) {
            $score = 1.0;
        } elseif (str_contains($normQuery, $nb) || str_contains($nb, $normQuery)) {
            $shorter = (float)min($lenQ, $lenB);
            $longer = (float)max($lenQ, $lenB);
            $score = round(0.7 + 0.3 * ($shorter / $longer), 4);
        } else {
            $g2 = array_count_values(ai_text_bigrams($nb));
            $score = ai_similarity_score_precomputed($normQuery, $lenQ, $g1, $sumG1, $nb, $lenB, $g2, array_sum($g2));
        }
        if ($score < $threshold) {
            continue;
        }
        $candidates[] = ['id' => $id, 'label' => $labelValue, 'score' => $score, 'score_percent' => (int)round($score * 100)];
    }
    usort($candidates, static fn(array $a, array $b): int => $b['score'] <=> $a['score'] ?: $a['id'] <=> $b['id']);
    return ['candidates' => array_slice($candidates, 0, $limit), 'scanned' => count($rows), 'threshold' => $threshold];
}

/** Инвентаризация типов компонентов шагов сценария (частоты и шаги появления). */
function ai_scenario_component_inventory(array $content): array
{
    $known = array_flip(scenario_known_component_types());
    $legacy = array_flip(scenario_legacy_component_types());
    $inventory = [];
    $total = 0;
    $steps = $content['steps'] ?? [];
    if (is_array($steps)) {
        foreach ($steps as $idx => $step) {
            if (!is_array($step)) {
                continue;
            }
            $sid = isset($step['id']) && is_scalar($step['id']) && trim((string)$step['id']) !== ''
                ? (string)$step['id']
                : ('step_' . $idx);
            $comps = $step['components'] ?? [];
            if (!is_array($comps)) {
                continue;
            }
            foreach ($comps as $comp) {
                if (!is_array($comp)) {
                    continue;
                }
                $type = isset($comp['type']) && is_string($comp['type']) ? trim($comp['type']) : '';
                if ($type === '') {
                    continue;
                }
                $total++;
                if (!isset($inventory[$type])) {
                    $inventory[$type] = [
                        'type' => $type,
                        'count' => 0,
                        'steps' => [],
                        'status' => isset($known[$type]) ? 'known' : (isset($legacy[$type]) ? 'legacy' : 'unknown'),
                    ];
                }
                $inventory[$type]['count']++;
                if (!in_array($sid, $inventory[$type]['steps'], true)) {
                    $inventory[$type]['steps'][] = $sid;
                }
            }
        }
    }
    $items = array_values($inventory);
    usort($items, static fn(array $a, array $b): int => $b['count'] <=> $a['count'] ?: strcmp($a['type'], $b['type']));
    $unknown = array_values(array_map(
        static fn(array $item): string => $item['type'],
        array_filter($items, static fn(array $item): bool => $item['status'] === 'unknown')
    ));
    return ['items' => $items, 'total' => $total, 'unknown_types' => $unknown];
}

/** Поиск входящих ссылок на сценарий в tw_items_list.Data (use_scenario.name) с SQL-префильтром. */
function ai_scenario_find_inbound_items(mysqli $db, string $filename, string $role, ?array $guestPages = null): array
{
    $inbound = [];
    if (!ai_can_read_resource('items', $role, $guestPages) || !ai_table_exists($db, 'tw_items_list')) {
        return $inbound;
    }
    $base = preg_replace('/\.json$/i', '', $filename);
    $likePat = '%' . $base . '%';
    $stmt = $db->prepare("SELECT `ID`, `Name`, `Data` FROM `tw_items_list` WHERE `Data` IS NOT NULL AND `Data` <> '' AND `Data` LIKE ?");
    if ($stmt) {
        $stmt->bind_param('s', $likePat);
        $stmt->execute();
        $res = $stmt->get_result();
        while ($row = $res->fetch_assoc()) {
            $decoded = json_decode((string)$row['Data'], true);
            $scName = is_array($decoded) ? (string)($decoded['use_scenario']['name'] ?? '') : '';
            if ($scName !== '' && ($scName === $filename || $scName === $base)) {
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
        $stmt->close();
    }
    return $inbound;
}

/** Анализ одного файла сценария: компоненты, зависимости (боты/предметы/миры), входящие связи и переиспользование. */
function ai_analyze_scenario(mysqli $db, string $filename, string $role, ?array $guestPages = null, ?string $scenariosDir = null): array
{
    $read = scenario_read_file($filename, $scenariosDir);
    if (!$read['ok']) {
        throw new CrInputError(
            (string)($read['error'] ?? 'Файл сценария не найден.'),
            ($read['code'] ?? '') === 'scenario_not_found' ? 404 : 400,
            ['code' => (string)($read['code'] ?? 'scenario_not_found')]
        );
    }
    $content = $read['content'];
    $inventory = ai_scenario_component_inventory($content);

    $rawRefs = $read['references'] ?? scenario_extract_references($content);
    $batchPairs = [];
    foreach ($rawRefs as $ref) {
        $batchPairs[] = ['target' => (string)$ref['target'], 'id' => (int)$ref['id']];
    }
    $resolvedMap = ai_resolve_targets_batch($db, $batchPairs, $role, $guestPages);

    $resolved = [];
    $missing = [];
    $byTable = [];
    foreach ($rawRefs as $ref) {
        $target = (string)$ref['target'];
        $id = (int)$ref['id'];
        $t = $resolvedMap[$target . ':' . $id] ?? ai_resolve_target_summary($db, $target, $id, $role, $guestPages);
        $entry = [
            'path' => (string)$ref['path'],
            'target' => $target,
            'resource' => $t['target_resource'] ?? null,
            'id' => $id,
            'accessible' => (bool)($t['accessible'] ?? false),
            'exists' => $t['exists'] ?? null,
            'label' => $t['label'] ?? null,
        ];
        $resolved[] = $entry;
        if ($entry['accessible'] && $entry['exists'] === false) {
            $missing[] = $entry;
        }
        $byTable[$target][] = $id;
    }
    $dependencies = [];
    foreach ($byTable as $table => $ids) {
        $missingCount = 0;
        foreach ($resolved as $entry) {
            if ($entry['target'] === $table && $entry['accessible'] && $entry['exists'] === false) {
                $missingCount++;
            }
        }
        $dependencies[] = [
            'table' => $table,
            'resource' => ai_table_to_resource_map()[$table] ?? null,
            'references' => count($ids),
            'unique_ids' => count(array_unique($ids)),
            'missing' => $missingCount,
        ];
    }
    usort($dependencies, static fn(array $a, array $b): int => strcmp($a['table'], $b['table']));

    $inbound = [];
    try {
        $inbound = ai_scenario_find_inbound_items($db, (string)$read['filename'], $role, $guestPages);
    } catch (Throwable) {
        $inbound = [];
    }

    $usedTypes = array_column($inventory['items'], 'type');
    $reuse = [];
    if ($usedTypes !== []) {
        foreach (scenario_list_files('', $scenariosDir) as $meta) {
            if (($meta['filename'] ?? '') === $read['filename']) {
                continue;
            }
            $metaSummary = is_array($meta['summary'] ?? null) ? $meta['summary'] : [];
            $otherTypes = $metaSummary['component_types'] ?? [];
            $shared = array_values(array_intersect($usedTypes, is_array($otherTypes) ? $otherTypes : []));
            if ($shared === []) {
                continue;
            }
            $reuse[] = [
                'filename' => (string)$meta['filename'],
                'shared_components' => $shared,
                'shared_count' => count($shared),
                'steps_count' => $metaSummary['steps_count'] ?? null,
            ];
        }
        usort($reuse, static fn(array $a, array $b): int => $b['shared_count'] <=> $a['shared_count'] ?: strcmp($a['filename'], $b['filename']));
        $reuse = array_slice($reuse, 0, 10);
    }

    return [
        'filename' => $read['filename'],
        'bytes' => $read['bytes'],
        'sha256' => $read['sha256'],
        'baseHash' => $read['sha256'],
        'updated_at' => $read['updated_at'],
        'summary' => $read['summary'],
        'components' => $inventory['items'],
        'component_total' => $inventory['total'],
        'unknown_component_types' => $inventory['unknown_types'],
        'references' => $resolved,
        'missing_references' => $missing,
        'dependencies' => $dependencies,
        'dependency_ok' => $missing === [],
        'inbound' => $inbound,
        'reuse' => $reuse,
    ];
}

/**
 * Сводный анализ всего каталога сценариев (до 200 файлов за вызов):
 * гистограмма компонентов, невалидные JSON-файлы и отчёт по отсутствующим
 * зависимостям (боты/предметы/миры/квесты) с пакетной проверкой существования в БД.
 */
function ai_analyze_scenario_corpus(mysqli $db, string $role, ?array $guestPages = null, ?string $scenariosDir = null, string $search = ''): array
{
    $files = scenario_list_files($search, $scenariosDir);
    $totalFiles = count($files);
    $MAX_SCANNED = 200;
    $scanned = 0;
    $truncated = false;
    $invalid = [];
    $componentUsage = [];
    $refTotal = 0;
    $missingRefs = [];
    $missingTotal = 0;
    $tableStats = [];

    $collectedRefs = []; // [['filename' => ..., 'path' => ..., 'target' => ..., 'id' => int], ...]
    $batchPairs = [];

    foreach ($files as $meta) {
        if ($scanned >= $MAX_SCANNED) {
            $truncated = true;
            break;
        }
        $scanned++;
        if (!($meta['valid_json'] ?? false)) {
            $invalid[] = ['filename' => (string)$meta['filename'], 'parse_error' => (string)($meta['parse_error'] ?? 'invalid JSON')];
            continue;
        }
        $summary = is_array($meta['summary'] ?? null) ? $meta['summary'] : [];
        foreach (($summary['component_types'] ?? []) as $type) {
            $componentUsage[$type] = ($componentUsage[$type] ?? 0) + 1;
        }
        $read = scenario_read_file((string)$meta['filename'], $scenariosDir);
        if (!$read['ok']) {
            continue;
        }
        foreach (($read['references'] ?? scenario_extract_references($read['content'])) as $ref) {
            $refTotal++;
            $target = (string)$ref['target'];
            $id = (int)$ref['id'];
            if (!isset($tableStats[$target])) {
                $tableStats[$target] = ['references' => 0, 'unique' => [], 'missing' => 0];
            }
            $tableStats[$target]['references']++;
            $tableStats[$target]['unique'][$id] = true;
            $collectedRefs[] = [
                'filename' => (string)$read['filename'],
                'path' => (string)$ref['path'],
                'target' => $target,
                'id' => $id,
            ];
            $batchPairs[] = ['target' => $target, 'id' => $id];
        }
    }

    // Проверяем все ссылки по всем файлам за 1 SQL-запрос на таблицу!
    $resolvedMap = ai_resolve_targets_batch($db, $batchPairs, $role, $guestPages);
    foreach ($collectedRefs as $cr) {
        $chk = $resolvedMap[$cr['target'] . ':' . $cr['id']] ?? null;
        if ($chk !== null && !empty($chk['accessible']) && ($chk['exists'] ?? null) === false) {
            $missingTotal++;
            $tableStats[$cr['target']]['missing']++;
            if (count($missingRefs) < 100) {
                $missingRefs[] = $cr;
            }
        }
    }

    $knownTypes = scenario_known_component_types();
    $legacyTypes = scenario_legacy_component_types();
    $components = [];
    foreach ($componentUsage as $type => $scenariosCount) {
        $components[] = [
            'type' => $type,
            'scenarios' => $scenariosCount,
            'status' => in_array($type, $knownTypes, true) ? 'known' : (in_array($type, $legacyTypes, true) ? 'legacy' : 'unknown'),
        ];
    }
    usort($components, static fn(array $a, array $b): int => $b['scenarios'] <=> $a['scenarios'] ?: strcmp($a['type'], $b['type']));

    $dependencies = [];
    foreach ($tableStats as $table => $stats) {
        $dependencies[] = [
            'table' => $table,
            'resource' => ai_table_to_resource_map()[$table] ?? null,
            'references' => (int)$stats['references'],
            'unique_ids' => count($stats['unique']),
            'missing' => (int)$stats['missing'],
        ];
    }
    usort($dependencies, static fn(array $a, array $b): int => strcmp($a['table'], $b['table']));

    return [
        'scanned' => $scanned,
        'total_files' => $totalFiles,
        'truncated' => $truncated,
        'invalid' => $invalid,
        'components' => $components,
        'reference_total' => $refTotal,
        'missing_references' => $missingRefs,
        'missing_total' => $missingTotal,
        'dependencies' => $dependencies,
        'dependency_ok' => $missingTotal === 0,
    ];
}

/**
 * Серверная проверка patch_json перед отправкой заявки (шаг «verify»).
 * Полностью read-only: прогоняет ту же валидацию, что и action=propose,
 * затем возвращает исходную запись/файл, объединённый результат и diff.
 *
 * @param array{resource?:mixed, id?:mixed, filename?:mixed, file?:mixed, patch_json?:mixed, data?:mixed, baseHash?:mixed, expected_before?:mixed} $input
 * @return array{status:int, payload:array<string, mixed>}
 */
function ai_apply_patch_verify(mysqli $db, array $input, string $role): array
{
    $resource = trim((string)($input['resource'] ?? ''));
    if ($resource === '') {
        throw new CrInputError('Укажите параметр resource.', 400, ['code' => 'missing_required_field', 'field' => 'resource']);
    }
    $known = isset(editor_resources()[$resource]) || $resource === 'scenarios';
    if (!$known) {
        throw new CrInputError("Неизвестный ресурс «{$resource}».", 404, ['code' => 'unknown_resource', 'field' => 'resource']);
    }
    if (!ai_can_read_resource($resource, $role)) {
        throw new CrInputError("Доступ к ресурсу «{$resource}» запрещён вашей учётной записи.", 403, ['code' => 'resource_forbidden', 'field' => 'resource']);
    }

    $patch = $input['patch_json'] ?? null;
    if (!is_array($patch) || ($patch !== [] && array_is_list($patch))) {
        throw new CrInputError('Поле patch_json должно быть JSON-объектом.', 400, ['code' => 'invalid_patch_json', 'field' => 'patch_json']);
    }

    $op = ['type' => 'update', 'resource' => $resource, 'patch_json' => $patch];
    if (isset($input['data'])) {
        if (!is_array($input['data'])) {
            throw new CrInputError('Поле data должно быть JSON-объектом.', 400, ['code' => 'invalid_data', 'field' => 'data']);
        }
        $op['data'] = $input['data'];
    }
    if (array_key_exists('expected_before', $input)) {
        $op['expected_before'] = $input['expected_before'];
    }
    if (array_key_exists('baseHash', $input)) {
        $op['baseHash'] = $input['baseHash'];
    }

    if ($resource === 'scenarios') {
        $fn = $input['filename'] ?? ($input['id'] ?? ($input['file'] ?? null));
        if (!is_string($fn) || trim($fn) === '') {
            throw new CrInputError('Укажите filename (или id) сценария для проверки патча.', 400, ['code' => 'missing_required_field', 'field' => 'filename']);
        }
        $op['filename'] = $fn;
    } else {
        $idRaw = $input['id'] ?? null;
        if ($idRaw === null || $idRaw === '') {
            throw new CrInputError('Укажите id записи для проверки патча.', 400, ['code' => 'missing_required_field', 'field' => 'id']);
        }
        $op['id'] = $idRaw;
    }

    $val = ai_validate_operations($db, [$op], $role, null, null, 1);
    if (!$val['valid']) {
        $first = $val['errors'][0];
        return [
            'status' => ai_http_status_for_validation_errors($val['errors']),
            'payload' => [
                'ok' => false,
                'valid' => false,
                'code' => (string)($first['code'] ?? 'validation_failed'),
                'error' => (string)($first['message'] ?? 'Проверка патча завершилась с ошибками.'),
                'field' => $first['field'] ?? null,
                'errors' => $val['errors'],
                'warnings' => $val['warnings'],
            ],
        ];
    }

    $norm = $val['normalized_operations'][0] ?? null;
    $warnings = $val['warnings'];

    if ($resource === 'scenarios') {
        $curRead = scenario_read_file((string)$norm['filename']);
        $before = !empty($curRead['ok']) ? $curRead['content'] : null;
        $after = $norm['data']['content'] ?? null;
        $diff = scenario_json_diff($before, $after);
        return [
            'status' => 200,
            'payload' => [
                'ok' => true,
                'valid' => true,
                'resource' => 'scenarios',
                'id' => (string)$norm['filename'],
                'filename' => (string)$norm['filename'],
                'before' => $before,
                'after' => $after,
                'diff' => $diff,
                'changed_fields' => count($diff),
                'noop' => $diff === [],
                'sha256_before' => !empty($curRead['ok']) ? (string)$curRead['sha256'] : null,
                'summary_after' => is_array($after) ? scenario_extract_summary($after) : null,
                'normalized_operation' => $norm,
                'warnings' => $warnings,
                'note' => 'Read-only серверная проверка patch_json. Ничего не изменено; для отправки используйте action=propose.',
            ],
        ];
    }

    $R = editor_resources()[$resource] + ['key' => $resource];
    $recId = (int)($norm['id'] ?? 0);
    $cur = crud_fetch_row($db, $R, $recId);
    $before = $cur !== null ? decode_json_cols($cur, $R['json'] ?? []) : null;
    $jsonCols = array_flip($R['json'] ?? []);
    $after = is_array($before) ? $before : [];
    foreach (($norm['data'] ?? []) as $col => $value) {
        if (isset($jsonCols[(string)$col]) && is_string($value)) {
            $decoded = json_decode($value, true);
            $after[$col] = $decoded;
        } else {
            $after[$col] = $value;
        }
    }

    $diff = [];
    foreach (($norm['data'] ?? []) as $col => $value) {
        $isJson = isset($jsonCols[(string)$col]);
        $beforeRaw = $cur !== null ? ($cur[$col] ?? null) : null;
        $beforeVal = $isJson && is_array($before) ? ($before[$col] ?? null) : $beforeRaw;
        $afterVal = $after[$col] ?? null;
        if (editor_values_equal($beforeRaw, $afterVal, $isJson)) {
            continue;
        }
        $diff[] = [
            'column' => (string)$col,
            'json' => $isJson,
            'before' => cr_display_value($beforeVal ?? $beforeRaw, $isJson),
            'after' => cr_display_value($afterVal, $isJson),
        ];
    }

    return [
        'status' => 200,
        'payload' => [
            'ok' => true,
            'valid' => true,
            'resource' => $resource,
            'id' => $recId,
            'record' => $after,
            'before' => $before,
            'diff' => $diff,
            'changed_fields' => count($diff),
            'noop' => $diff === [],
            'normalized_operation' => $norm,
            'warnings' => $warnings,
            'note' => 'Read-only серверная проверка patch_json. Ничего не изменено; для отправки используйте action=propose.',
        ],
    ];
}

/** HTTP-статус для ответа с ошибками валидации: 403/409 по first-match, иначе 400. */
function ai_http_status_for_validation_errors(array $errors): int
{
    foreach ($errors as $e) {
        if (($e['code'] ?? '') === 'resource_forbidden') {
            return 403;
        }
    }
    foreach ($errors as $e) {
        if (in_array((string)($e['code'] ?? ''), ['record_not_found', 'scenario_exists', 'scenario_not_found', 'base_hash_mismatch', 'version_conflict'], true)) {
            return 409;
        }
    }
    return 400;
}

/**
 * Пакетное чтение до 50 записей одного ресурса БД за один SQL-запрос (`ids=1,2,3`).
 * Сохраняет порядок запрошенных ID и возвращает `missing_ids` для отсутствующих.
 */
function ai_query_db_batch_get(mysqli $db, array $R, array $ids, array $opts = []): array
{
    $cleanIds = [];
    $seen = [];
    $allowZero = (($R['key'] ?? '') === 'worlds');
    foreach ($ids as $raw) {
        $id = crud_record_id((string)$raw, (string)($R['key'] ?? ''));
        if ($id === null || (!$allowZero && $id < 1)) {
            throw new CrInputError('Некорректный ID в списке ids.', 400, ['code' => 'invalid_id', 'field' => 'ids']);
        }
        if (!isset($seen[$id])) {
            $seen[$id] = true;
            $cleanIds[] = $id;
        }
        if (count($cleanIds) > 50) {
            throw new CrInputError('За один запрос можно прочитать не более 50 ID.', 400, ['code' => 'too_many_ids', 'field' => 'ids', 'max' => 50]);
        }
    }
    if ($cleanIds === []) {
        return [
            'rows' => [],
            'found_ids' => [],
            'missing_ids' => [],
            'requested' => 0,
            'found' => 0,
            'returned' => 0,
            'truncated' => false,
        ];
    }

    $table = editor_identifier($R['table'], 'table');
    $pk = editor_identifier($R['pk'], 'pk');
    $selectCols = ai_resolve_select_fields($R, $opts['fields'] ?? null);
    if (!in_array($pk, $selectCols, true)) {
        array_unshift($selectCols, $pk);
    }
    $colSql = implode(', ', array_map(static fn(string $c): string => '`' . editor_identifier($c, 'select') . '`', $selectCols));
    $placeholders = implode(', ', array_fill(0, count($cleanIds), '?'));
    $sql = "SELECT $colSql FROM `$table` WHERE `$pk` IN ($placeholders)";
    $stmt = $db->prepare($sql);
    if ($stmt === false) {
        throw new RuntimeException($db->error ?: 'Ошибка подготовки пакетного чтения');
    }
    $types = str_repeat('i', count($cleanIds));
    $stmt->bind_param($types, ...$cleanIds);
    $stmt->execute();
    $res = $stmt->get_result();
    $byId = [];
    $decode = (bool)($opts['decode'] ?? true);
    $jsonCols = $R['json'] ?? [];
    while ($row = $res->fetch_assoc()) {
        $rec = $decode ? decode_json_cols($row, $jsonCols) : $row;
        $rec = ai_project_record_fields($rec, $R, $opts['fields'] ?? null);
        $byId[(int)$row[$pk]] = $rec;
    }
    $stmt->close();

    $ordered = [];
    $foundIds = [];
    $missing = [];
    foreach ($cleanIds as $id) {
        if (isset($byId[$id])) {
            $ordered[] = $byId[$id];
            $foundIds[] = $id;
        } else {
            $missing[] = $id;
        }
    }

    $maxBytes = clamp_int($opts['max_bytes'] ?? null, 4096, AI_MAX_RESPONSE_MAX_BYTES, AI_DEFAULT_RESPONSE_MAX_BYTES);
    $page = ai_paginate_slice($ordered, count($ordered), count($ordered) ?: 1, 0, $maxBytes);

    return [
        'rows' => $page['rows'],
        'found_ids' => $foundIds,
        'missing_ids' => $missing,
        'requested' => count($cleanIds),
        'found' => count($ordered),
        'returned' => count($page['rows']),
        'truncated' => (bool)$page['truncated'],
        'max_bytes' => (int)$page['max_bytes'],
        'bytes' => (int)$page['bytes'],
    ];
}

/**
 * Сквозной поиск по всем разрешённым ресурсам БД, сценариям и справочникам за один вызов.
 * Строго соблюдает гостевой ACL (vouchers и закрытые вкладки никогда не запрашиваются).
 */
function ai_global_search(mysqli $db, string $query, string $role, ?array $guestPages = null, array $opts = []): array
{
    $q = trim($query);
    if ($q === '') {
        throw new CrInputError('Укажите строку поиска q.', 400, ['code' => 'missing_required_field', 'field' => 'q']);
    }
    $qLower = mb_strtolower($q, 'UTF-8');
    $isNumeric = preg_match('/^\d+$/', $q) === 1;
    $limitPerRes = clamp_int($opts['limit_per_resource'] ?? null, 1, 20, 5);
    $maxMatches = clamp_int($opts['max_matches'] ?? null, 5, 100, 40);

    $requestedResources = null;
    if (isset($opts['resources']) && $opts['resources'] !== null && $opts['resources'] !== '') {
        $rawList = is_array($opts['resources']) ? $opts['resources'] : explode(',', (string)$opts['resources']);
        $requestedResources = array_values(array_filter(array_map('trim', array_map('strval', $rawList)), static fn(string $s): bool => $s !== ''));
    }

    $allResources = editor_resources();
    $lookups = ai_schema_lookups();
    $candidates = array_merge(array_keys($allResources), array_keys($lookups), ['scenarios']);
    if ($requestedResources !== null) {
        $allowedKnown = array_flip($candidates);
        foreach ($requestedResources as $rk) {
            if (!isset($allowedKnown[$rk])) {
                throw new CrInputError("Неизвестный ресурс «{$rk}» в параметре resources.", 400, [
                    'code' => 'unknown_resource',
                    'field' => 'resources',
                    'did_you_mean' => ai_schema_suggest_closest($rk, $candidates, 3),
                ]);
            }
        }
        $candidates = $requestedResources;
    }

    $groups = [];
    $flat = [];
    $searchedResources = [];

    foreach ($candidates as $resKey) {
        if (!ai_can_read_resource($resKey, $role, $guestPages)) {
            continue;
        }
        $searchedResources[] = $resKey;

        if ($resKey === 'scenarios') {
            $files = scenario_list_files($q);
            if ($files !== []) {
                $items = [];
                foreach (array_slice($files, 0, $limitPerRes) as $f) {
                    $fn = (string)$f['filename'];
                    $score = strtolower($fn) === $qLower ? 100 : (str_contains(strtolower($fn), $qLower) ? 80 : 60);
                    $entry = [
                        'resource' => 'scenarios',
                        'id' => $fn,
                        'label' => $fn,
                        'preview' => [
                            'valid_json' => (bool)$f['valid_json'],
                            'steps_count' => $f['summary']['steps_count'] ?? 0,
                            'scenario_mode' => $f['summary']['scenario_mode'] ?? null,
                        ],
                        '_score' => $score,
                    ];
                    $items[] = array_diff_key($entry, ['_score' => 1]);
                    $flat[] = $entry;
                }
                $groups['scenarios'] = ['title' => 'Сценарии (файлы)', 'total' => count($files), 'items' => $items];
            }
            continue;
        }

        if (isset($lookups[$resKey])) {
            $L = $lookups[$resKey];
            if (!ai_table_exists($db, (string)$L['table'])) {
                continue;
            }
            $res = ai_query_lookup($db, $resKey, ['search' => $q, 'limit' => $limitPerRes, 'offset' => 0, 'skip_count' => true]);
            if ($res['rows'] !== []) {
                $items = [];
                foreach ($res['rows'] as $row) {
                    $id = $row[$L['pk']] ?? null;
                    $label = (string)($row[$L['label']] ?? ('#' . $id));
                    $score = ($isNumeric && (string)$id === $q) ? 100
                        : (mb_strtolower($label, 'UTF-8') === $qLower ? 95
                        : (str_starts_with(mb_strtolower($label, 'UTF-8'), $qLower) ? 82 : 65));
                    $entry = [
                        'resource' => $resKey,
                        'id' => is_numeric($id) ? (int)$id : $id,
                        'label' => $label,
                        'preview' => $row,
                        '_score' => $score,
                    ];
                    $items[] = array_diff_key($entry, ['_score' => 1]);
                    $flat[] = $entry;
                }
                $groups[$resKey] = ['title' => (string)$L['title'], 'total' => (int)$res['total'], 'items' => $items];
            }
            continue;
        }

        $R = $allResources[$resKey] + ['key' => $resKey];
        if (!ai_table_exists($db, (string)$R['table'])) {
            continue;
        }
        $pk = (string)$R['pk'];
        $labelCol = $R['label'] ?? null;
        $compactCols = [$pk];
        if ($labelCol !== null && !in_array($labelCol, $compactCols, true)) {
            $compactCols[] = (string)$labelCol;
        }
        foreach (['WorldID', 'BotID', 'QuestID', 'ItemID', 'Type', 'Group', 'Level', 'Path', 'SwitchID', 'Step'] as $extraCol) {
            if (in_array($extraCol, $R['columns'], true) && !in_array($extraCol, $compactCols, true)) {
                $compactCols[] = $extraCol;
            }
        }
        $res = ai_query_db_list($db, $R, [
            'search' => $q,
            'fields' => $compactCols,
            'limit' => $limitPerRes,
            'offset' => 0,
            'decode' => false,
            'skip_count' => true,
        ]);
        if ($res['rows'] !== []) {
            $items = [];
            foreach ($res['rows'] as $row) {
                $id = isset($row[$pk]) ? (int)$row[$pk] : null;
                $label = ($labelCol !== null && isset($row[$labelCol]) && trim((string)$row[$labelCol]) !== '')
                    ? (string)$row[$labelCol]
                    : ($R['title'] . ' #' . $id);
                $lblLower = mb_strtolower($label, 'UTF-8');
                $score = ($isNumeric && (string)$id === $q) ? 100
                    : ($lblLower === $qLower ? 96
                    : (str_starts_with($lblLower, $qLower) ? 85
                    : (str_contains($lblLower, $qLower) ? 72 : 55)));
                $entry = [
                    'resource' => $resKey,
                    'id' => $id,
                    'label' => $label,
                    'preview' => $row,
                    '_score' => $score,
                ];
                $items[] = array_diff_key($entry, ['_score' => 1]);
                $flat[] = $entry;
            }
            $groups[$resKey] = ['title' => (string)$R['title'], 'total' => (int)$res['total'], 'items' => $items];
        }
    }

    usort($flat, static function (array $a, array $b): int {
        if ($a['_score'] !== $b['_score']) {
            return $b['_score'] <=> $a['_score'];
        }
        return strcmp((string)$a['resource'] . ':' . $a['id'], (string)$b['resource'] . ':' . $b['id']);
    });
    $topMatches = array_map(
        static fn(array $e): array => array_diff_key($e, ['_score' => 1]),
        array_slice($flat, 0, $maxMatches)
    );

    return [
        'query' => $q,
        'searched_resources' => $searchedResources,
        'matched_resources_count' => count($groups),
        'top_matches' => $topMatches,
        'groups' => $groups,
    ];
}

/** Вспомогательный запрос списка строк с привязкой одного целочисленного параметра. */
function ai_ctx_fetch_rows(mysqli $db, string $sql, string $types = '', array $params = []): array
{
    $stmt = $db->prepare($sql);
    if ($stmt === false) {
        return [];
    }
    if ($types !== '' && $params !== []) {
        $stmt->bind_param($types, ...$params);
    }
    $stmt->execute();
    $res = $stmt->get_result();
    $rows = [];
    while ($row = $res->fetch_assoc()) {
        $rows[] = $row;
    }
    $stmt->close();
    return $rows;
}

/** Карта `BotID => Name` из `tw_bots_info` (только если ресурс разрешён). */
function ai_ctx_bot_names(mysqli $db, array $botIds, string $role, ?array $guestPages = null): array
{
    if (!ai_can_read_resource('bots_info', $role, $guestPages) || !ai_table_exists($db, 'tw_bots_info')) {
        return [];
    }
    $ids = array_values(array_unique(array_filter(array_map('intval', $botIds), static fn(int $v): bool => $v > 0)));
    if ($ids === []) {
        return [];
    }
    $ph = implode(', ', array_fill(0, count($ids), '?'));
    $rows = ai_ctx_fetch_rows($db, "SELECT `ID`, `Name` FROM `tw_bots_info` WHERE `ID` IN ($ph)", str_repeat('i', count($ids)), $ids);
    $map = [];
    foreach ($rows as $r) {
        $map[(int)$r['ID']] = (string)($r['Name'] ?? '');
    }
    return $map;
}

/**
 * Композитный снимок контекста сущности за один запрос:
 *  - worlds    -> мир + NPC, мобы, шаги квестов, склады, телепорты, доски, узлы, крафты, данжи, доступность карты
 *  - quests    -> квест + упорядоченные шаги (quest_bots) + выдающие NPC + доски + цепочка prev/next + references
 *  - items     -> предмет + крафты (результат/ингредиент) + дроп мобов + узлы сбора + экипировка ботов + магазины + references
 *  - bots_info -> бот + все точки спавна NPC/мобов/квестовых шагов + references
 *  - scenarios -> файл + analysis + references
 *  - прочие ресурсы -> запись + references
 */
function ai_build_entity_context(mysqli $db, string $resource, int|string $id, string $role, ?array $guestPages = null): array
{
    if (!ai_can_read_resource($resource, $role, $guestPages)) {
        throw new CrInputError("Доступ к ресурсу «{$resource}» запрещён вашей учётной записи.", 403, ['code' => 'resource_forbidden', 'field' => 'resource']);
    }

    if ($resource === 'scenarios') {
        $fn = (string)$id;
        $read = scenario_read_file($fn);
        if (!$read['ok']) {
            $status = ($read['code'] ?? '') === 'scenario_not_found' ? 404 : 400;
            throw new CrInputError((string)$read['error'], $status, ['code' => (string)($read['code'] ?? 'scenario_not_found'), 'field' => 'id']);
        }
        return [
            'resource' => 'scenarios',
            'id' => $read['filename'],
            'summary' => $read['summary'],
            'sha256' => $read['sha256'],
            'analysis' => ai_analyze_scenario($db, $read['filename'], $role, $guestPages),
            'references' => ai_references_inspect($db, 'scenarios', $read['filename'], $role, $guestPages),
        ];
    }

    $resources = editor_resources();
    if (!isset($resources[$resource])) {
        throw new CrInputError("Неизвестный ресурс «{$resource}».", 404, [
            'code' => 'unknown_resource',
            'field' => 'resource',
            'did_you_mean' => ai_schema_suggest_closest($resource, array_merge(array_keys($resources), ['scenarios']), 3),
        ]);
    }

    $R = $resources[$resource] + ['key' => $resource];
    $recId = crud_record_id((string)$id, $resource);
    if ($recId === null) {
        throw new CrInputError('Некорректный ID записи.', 400, ['code' => 'invalid_id', 'field' => 'id']);
    }
    $row = crud_fetch_row($db, $R, $recId);
    if ($row === null) {
        throw new CrInputError("Запись {$R['title']} #{$recId} не найдена.", 404, ['code' => 'record_not_found', 'field' => 'id']);
    }
    $decoded = decode_json_cols($row, $R['json'] ?? []);

    if ($resource === 'worlds') {
        $ctx = [
            'resource' => 'worlds',
            'id' => $recId,
            'record' => $decoded,
            'counts' => [],
            'entities' => [],
        ];
        if (ai_can_read_resource('maps', $role, $guestPages)) {
            $loaded = ai_map_load_bytes($db, $recId, null);
            $ctx['map'] = [
                'available' => (bool)$loaded['available'],
                'source' => $loaded['source'],
                'path' => (string)($decoded['Path'] ?? ''),
                'bytes' => is_string($loaded['bytes']) ? strlen($loaded['bytes']) : null,
                'hint' => 'Используйте analyze_map и get_map_screenshot с world_id=' . $recId . ' для анализа построек и визуальной проверки.',
            ];
        }

        $botIds = [];
        $npcs = (ai_can_read_resource('bots_npc', $role, $guestPages) && ai_table_exists($db, 'tw_bots_npc'))
            ? ai_ctx_fetch_rows($db, 'SELECT `ID`, `BotID`, `PosX`, `PosY`, `GiveQuestID`, `Function`, `Static`, `Emote` FROM `tw_bots_npc` WHERE `WorldID` = ? ORDER BY `ID` ASC LIMIT 120', 'i', [$recId])
            : null;
        $mobs = (ai_can_read_resource('bots_mobs', $role, $guestPages) && ai_table_exists($db, 'tw_bots_mobs'))
            ? ai_ctx_fetch_rows($db, 'SELECT `ID`, `BotID`, `PositionX`, `PositionY`, `Level`, `Power`, `Number`, `Boss`, `Radius` FROM `tw_bots_mobs` WHERE `WorldID` = ? ORDER BY `ID` ASC LIMIT 120', 'i', [$recId])
            : null;
        $qbots = (ai_can_read_resource('quest_bots', $role, $guestPages) && ai_table_exists($db, 'tw_bots_quest'))
            ? ai_ctx_fetch_rows($db, 'SELECT `ID`, `BotID`, `QuestID`, `Step`, `PosX`, `PosY` FROM `tw_bots_quest` WHERE `WorldID` = ? ORDER BY `QuestID` ASC, `Step` ASC LIMIT 120', 'i', [$recId])
            : null;

        foreach ([$npcs, $mobs, $qbots] as $arr) {
            if (is_array($arr)) {
                foreach ($arr as $r) {
                    if (isset($r['BotID'])) {
                        $botIds[] = (int)$r['BotID'];
                    }
                }
            }
        }
        $botNames = ai_ctx_bot_names($db, $botIds, $role, $guestPages);

        if ($npcs !== null) {
            foreach ($npcs as &$r) {
                $r['BotName'] = $botNames[(int)$r['BotID']] ?? null;
            }
            unset($r);
            $ctx['counts']['bots_npc'] = count($npcs);
            $ctx['entities']['bots_npc'] = $npcs;
        }
        if ($mobs !== null) {
            foreach ($mobs as &$r) {
                $r['BotName'] = $botNames[(int)$r['BotID']] ?? null;
            }
            unset($r);
            $ctx['counts']['bots_mobs'] = count($mobs);
            $ctx['entities']['bots_mobs'] = $mobs;
        }
        if ($qbots !== null) {
            foreach ($qbots as &$r) {
                $r['BotName'] = $botNames[(int)$r['BotID']] ?? null;
            }
            unset($r);
            $ctx['counts']['quest_bots'] = count($qbots);
            $ctx['entities']['quest_bots'] = $qbots;
        }
        if (ai_can_read_resource('warehouses', $role, $guestPages) && ai_table_exists($db, 'tw_warehouses')) {
            $rows = ai_ctx_fetch_rows($db, 'SELECT `ID`, `Name`, `Type`, `PosX`, `PosY`, `Currency` FROM `tw_warehouses` WHERE `WorldID` = ? ORDER BY `ID` ASC LIMIT 80', 'i', [$recId]);
            $ctx['counts']['warehouses'] = count($rows);
            $ctx['entities']['warehouses'] = $rows;
        }
        if (ai_can_read_resource('aethers', $role, $guestPages) && ai_table_exists($db, 'tw_aethers')) {
            $rows = ai_ctx_fetch_rows($db, 'SELECT `ID`, `Name`, `TeleX`, `TeleY` FROM `tw_aethers` WHERE `WorldID` = ? ORDER BY `ID` ASC LIMIT 80', 'i', [$recId]);
            $ctx['counts']['aethers'] = count($rows);
            $ctx['entities']['aethers'] = $rows;
        }
        if (ai_can_read_resource('quest_boards', $role, $guestPages) && ai_table_exists($db, 'tw_quest_boards')) {
            $rows = ai_ctx_fetch_rows($db, 'SELECT `ID`, `Name`, `PosX`, `PosY` FROM `tw_quest_boards` WHERE `WorldID` = ? ORDER BY `ID` ASC LIMIT 80', 'i', [$recId]);
            $ctx['counts']['quest_boards'] = count($rows);
            $ctx['entities']['quest_boards'] = $rows;
        }
        if (ai_can_read_resource('nodes_collecting', $role, $guestPages) && ai_table_exists($db, 'tw_nodes_collecting')) {
            $rows = ai_ctx_fetch_rows($db, 'SELECT `ID`, `SwitchID`, `Name`, `Type`, `Level`, `Health` FROM `tw_nodes_collecting` WHERE `WorldID` = ? ORDER BY `Type` ASC, `SwitchID` ASC LIMIT 120', 'i', [$recId]);
            $ctx['counts']['nodes_collecting'] = count($rows);
            $ctx['entities']['nodes_collecting'] = $rows;
        }
        if (ai_can_read_resource('crafts', $role, $guestPages) && ai_table_exists($db, 'tw_crafts_list')) {
            $rows = ai_ctx_fetch_rows($db, 'SELECT `ID`, `GroupName`, `ItemID`, `ItemValue`, `Price` FROM `tw_crafts_list` WHERE `WorldID` = ? ORDER BY `ID` ASC LIMIT 120', 'i', [$recId]);
            $ctx['counts']['crafts'] = count($rows);
            $ctx['entities']['crafts'] = $rows;
        }
        if (ai_can_read_resource('dungeons', $role, $guestPages) && ai_table_exists($db, 'tw_dungeons')) {
            $rows = ai_ctx_fetch_rows($db, 'SELECT `ID`, `Level`, `DoorX`, `DoorY`, `TimeLimit` FROM `tw_dungeons` WHERE `WorldID` = ? ORDER BY `ID` ASC LIMIT 40', 'i', [$recId]);
            $ctx['counts']['dungeons'] = count($rows);
            $ctx['entities']['dungeons'] = $rows;
        }
        return $ctx;
    }

    if ($resource === 'quests') {
        $steps = [];
        if (ai_can_read_resource('quest_bots', $role, $guestPages) && ai_table_exists($db, 'tw_bots_quest')) {
            $rawSteps = ai_ctx_fetch_rows($db, 'SELECT `ID`, `BotID`, `Step`, `WorldID`, `PosX`, `PosY`, `AutoFinish`, `DialogData`, `ScenarioData`, `TasksData` FROM `tw_bots_quest` WHERE `QuestID` = ? ORDER BY `Step` ASC, `ID` ASC LIMIT 100', 'i', [$recId]);
            $botNames = ai_ctx_bot_names($db, array_column($rawSteps, 'BotID'), $role, $guestPages);
            foreach ($rawSteps as $s) {
                $decStep = decode_json_cols($s, ['DialogData', 'TasksData']);
                $steps[] = [
                    'id' => (int)$s['ID'],
                    'step' => (int)$s['Step'],
                    'bot_id' => (int)$s['BotID'],
                    'bot_name' => $botNames[(int)$s['BotID']] ?? null,
                    'world_id' => $s['WorldID'] !== null ? (int)$s['WorldID'] : null,
                    'x' => (int)$s['PosX'],
                    'y' => (int)$s['PosY'],
                    'auto_finish' => $s['AutoFinish'],
                    'dialog_lines' => is_array($decStep['DialogData'] ?? null) ? count($decStep['DialogData']) : 0,
                    'has_scenario' => trim((string)($s['ScenarioData'] ?? '')) !== '',
                    'tasks' => $decStep['TasksData'] ?? null,
                ];
            }
        }
        $givers = [];
        if (ai_can_read_resource('bots_npc', $role, $guestPages) && ai_table_exists($db, 'tw_bots_npc')) {
            $rawGivers = ai_ctx_fetch_rows($db, 'SELECT `ID`, `BotID`, `WorldID`, `PosX`, `PosY`, `Function` FROM `tw_bots_npc` WHERE `GiveQuestID` = ? ORDER BY `ID` ASC LIMIT 30', 'i', [$recId]);
            $botNames = ai_ctx_bot_names($db, array_column($rawGivers, 'BotID'), $role, $guestPages);
            foreach ($rawGivers as $g) {
                $g['BotName'] = $botNames[(int)$g['BotID']] ?? null;
                $givers[] = $g;
            }
        }
        $prevQuests = ai_ctx_fetch_rows($db, 'SELECT `ID`, `Name`, `Flags` FROM `tw_quests_list` WHERE `NextQuestID` = ? ORDER BY `ID` ASC LIMIT 20', 'i', [$recId]);
        $nextQuest = null;
        if (!empty($decoded['NextQuestID']) && (int)$decoded['NextQuestID'] > 0) {
            $nq = ai_ctx_fetch_rows($db, 'SELECT `ID`, `Name`, `Flags` FROM `tw_quests_list` WHERE `ID` = ? LIMIT 1', 'i', [(int)$decoded['NextQuestID']]);
            $nextQuest = $nq[0] ?? null;
        }
        $boards = [];
        if (ai_can_read_resource('quest_board_links', $role, $guestPages) && ai_table_exists($db, 'tw_quests_board_list')) {
            $boards = ai_ctx_fetch_rows($db, 'SELECT `ID`, `DailyBoardID` FROM `tw_quests_board_list` WHERE `QuestID` = ? ORDER BY `ID` ASC LIMIT 20', 'i', [$recId]);
        }
        return [
            'resource' => 'quests',
            'id' => $recId,
            'record' => $decoded,
            'steps' => $steps,
            'givers' => $givers,
            'board_links' => $boards,
            'chain' => ['prev_quests' => $prevQuests, 'next_quest' => $nextQuest],
            'references' => ai_references_inspect($db, 'quests', $recId, $role, $guestPages, null, $row),
        ];
    }

    if ($resource === 'items') {
        $craftedBy = [];
        $usedInCrafts = [];
        if (ai_can_read_resource('crafts', $role, $guestPages) && ai_table_exists($db, 'tw_crafts_list')) {
            $craftedBy = ai_ctx_fetch_rows($db, 'SELECT `ID`, `GroupName`, `ItemValue`, `RequiredItems`, `Price`, `WorldID` FROM `tw_crafts_list` WHERE `ItemID` = ? ORDER BY `ID` ASC LIMIT 40', 'i', [$recId]);
            $likePat = '%[' . $recId . '/%';
            $usedInCrafts = ai_ctx_fetch_rows($db, 'SELECT `ID`, `GroupName`, `ItemID`, `ItemValue`, `RequiredItems`, `WorldID` FROM `tw_crafts_list` WHERE `RequiredItems` LIKE ? ORDER BY `ID` ASC LIMIT 40', 's', [$likePat]);
        }
        $droppedBy = [];
        if (ai_can_read_resource('bots_mobs', $role, $guestPages) && ai_table_exists($db, 'tw_bots_mobs')) {
            $rawMobs = ai_ctx_fetch_rows(
                $db,
                'SELECT `ID`, `BotID`, `WorldID`, `Level`, `Boss`, `it_drop_count`, `it_drop_chance` FROM `tw_bots_mobs` WHERE `it_drop_0` = ? OR `it_drop_1` = ? OR `it_drop_2` = ? OR `it_drop_3` = ? OR `it_drop_4` = ? ORDER BY `ID` ASC LIMIT 40',
                'iiiii',
                [$recId, $recId, $recId, $recId, $recId]
            );
            $botNames = ai_ctx_bot_names($db, array_column($rawMobs, 'BotID'), $role, $guestPages);
            foreach ($rawMobs as $m) {
                $m['BotName'] = $botNames[(int)$m['BotID']] ?? null;
                $droppedBy[] = $m;
            }
        }
        $harvestNodes = [];
        if (ai_can_read_resource('nodes_collecting', $role, $guestPages) && ai_table_exists($db, 'tw_nodes_collecting')) {
            $likePat = '%[' . $recId . '/%';
            $harvestNodes = ai_ctx_fetch_rows($db, 'SELECT `ID`, `Name`, `Type`, `SwitchID`, `WorldID`, `Properties` FROM `tw_nodes_collecting` WHERE `Properties` LIKE ? ORDER BY `ID` ASC LIMIT 40', 's', [$likePat]);
        }
        $soldIn = [];
        if (ai_can_read_resource('warehouses', $role, $guestPages) && ai_table_exists($db, 'tw_warehouses')) {
            $likePat = '%[' . $recId . '/%';
            $soldIn = ai_ctx_fetch_rows($db, 'SELECT `ID`, `Name`, `Type`, `WorldID`, `Currency` FROM `tw_warehouses` WHERE `Currency` = ? OR `Trades` LIKE ? ORDER BY `ID` ASC LIMIT 40', 'is', [$recId, $likePat]);
        }
        return [
            'resource' => 'items',
            'id' => $recId,
            'record' => $decoded,
            'crafted_by' => $craftedBy,
            'used_in_crafts' => $usedInCrafts,
            'dropped_by_mobs' => $droppedBy,
            'harvested_from_nodes' => $harvestNodes,
            'warehouses' => $soldIn,
            'references' => ai_references_inspect($db, 'items', $recId, $role, $guestPages, null, $row),
        ];
    }

    if ($resource === 'bots_info') {
        $npcSpawns = (ai_can_read_resource('bots_npc', $role, $guestPages) && ai_table_exists($db, 'tw_bots_npc'))
            ? ai_ctx_fetch_rows($db, 'SELECT `ID`, `WorldID`, `PosX`, `PosY`, `GiveQuestID`, `Function`, `Static`, `Emote` FROM `tw_bots_npc` WHERE `BotID` = ? ORDER BY `ID` ASC LIMIT 40', 'i', [$recId])
            : [];
        $mobSpawns = (ai_can_read_resource('bots_mobs', $role, $guestPages) && ai_table_exists($db, 'tw_bots_mobs'))
            ? ai_ctx_fetch_rows($db, 'SELECT `ID`, `WorldID`, `PositionX`, `PositionY`, `Level`, `Power`, `Number`, `Boss` FROM `tw_bots_mobs` WHERE `BotID` = ? ORDER BY `ID` ASC LIMIT 40', 'i', [$recId])
            : [];
        $questSteps = (ai_can_read_resource('quest_bots', $role, $guestPages) && ai_table_exists($db, 'tw_bots_quest'))
            ? ai_ctx_fetch_rows($db, 'SELECT `ID`, `QuestID`, `Step`, `WorldID`, `PosX`, `PosY` FROM `tw_bots_quest` WHERE `BotID` = ? ORDER BY `QuestID` ASC, `Step` ASC LIMIT 40', 'i', [$recId])
            : [];
        return [
            'resource' => 'bots_info',
            'id' => $recId,
            'record' => $decoded,
            'npc_spawns' => $npcSpawns,
            'mob_spawns' => $mobSpawns,
            'quest_steps' => $questSteps,
            'references' => ai_references_inspect($db, 'bots_info', $recId, $role, $guestPages, null, $row),
        ];
    }

    return [
        'resource' => $resource,
        'id' => $recId,
        'record' => $decoded,
        'references' => ai_references_inspect($db, $resource, $recId, $role, $guestPages, null, $row),
    ];
}

/**
 * Публичный машиночитаемый документ /llms.txt (без секретов, путей и данных БД).
 */
function ai_build_llms_txt(string $baseUrl): string
{
    $base = rtrim($baseUrl, '/');
    return implode("\n", [
        '# Teeworlds-MRPG-0.6 Editor Studio — AI & MCP Guide',
        '',
        '> Model-agnostic REST & MCP JSON-RPC 2.0 platform for inspecting MRPG 0.6 worlds, maps, items, quests, NPCs, mobs, shops, and JSON scenarios, and submitting safe human-reviewed Change Requests.',
        '',
        '## Discovery Endpoints (Public)',
        "- Onboarding (HTML / JSON): {$base}/ai/ and {$base}/ai/?format=json",
        "- Plugin Descriptor: {$base}/api/ai.php?action=discovery (alias: {$base}/.well-known/ai-plugin.json)",
        "- OpenAPI 3.1.0 Spec: {$base}/api/ai.php?action=openapi (alias: {$base}/openapi.json)",
        "- MCP HTTP Endpoint (JSON-RPC 2.0): {$base}/api/mcp.php (alias: {$base}/mcp)",
        "- REST API Entrypoint: {$base}/api/ai.php",
        '',
        '## Authentication & Safety Boundary',
        '- Header: `Authorization: Bearer esai_<token>` (or `X-AI-Token: esai_<token>`)',
        '- Scopes: `read`, `create`, `update`, `delete` (enforced server-side per tool/operation).',
        '- Direct database/disk mutations are impossible: every write creates a Change Request in SQLite reviewed by a Studio administrator.',
        '- Deletions, clearing JSON fields, >10 operations, or modifying `worlds`/`dungeons` require explicit `confirm_dangerous: true` after reviewing `dangerous_actions`.',
        '',
        '## Recommended AI Workflow',
        '1. **Capabilities & Schema**: `GET /api/ai.php?action=manifest` (MCP: `get_manifest`, `get_schema`, resource `mrpg://cookbook`).',
        '2. **Fast Orientation**: `GET /api/ai.php?action=search&q=<term>` (MCP: `search_editor`) and `GET /api/ai.php?action=context&resource=worlds|quests|items|bots_info&id=<id>` (MCP: `get_context`).',
        '3. **Maps & Visual Verification**: `GET /api/ai.php?action=analyze&resource=maps&world=<id>` (MCP: `analyze_map`) + `GET /api/ai.php?action=map_screenshot&world=<id>&target=overview|guild_house:1|house:1` (MCP: `get_map_screenshot` returning native PNG).',
        '4. **Anti-Duplicates & Scenarios**: `GET /api/ai.php?action=similar&resource=<res>&name=<Name>` (MCP: `find_similar`) and `GET /api/ai.php?action=analyze&resource=scenarios` (MCP: `analyze_scenario`).',
        '5. **Batch Reads & Filtering**: `GET /api/ai.php?action=get&resource=<res>&ids=1,2,3` (MCP: `get_record` with `ids`), plus `list` operator filters (`{"Level":{"gte":5}}`) and `sort=-ID`.',
        '6. **Atomic Multi-Object Authoring (`ref` / `$alias`)**: Add `"ref": "my_item"` on any DB `create`/`copy` op and reference it as `"$my_item"` in FK fields, DSL (`"[$my_item/3]"`), or JSON in the same batch.',
        '7. **Safe JSON Edits**: Prefer `patch_json` (RFC 7396); verify read-only via `POST /api/ai.php?action=apply_patch` (MCP: `apply_patch`), then `validate` -> `preview` -> `propose`.',
        '8. **Review Loop**: Check admin feedback, reply, or withdraw pending requests via `GET /api/ai.php?action=request&id=<id>`, `POST action=request_reply`, `POST action=request_withdraw` (MCP: `manage_change_request`).',
        '',
    ]);
}

