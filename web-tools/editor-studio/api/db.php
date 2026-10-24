<?php
// DB list API for DB Select components + settings page.
// Endpoint: api/db.php
// Actions:
//  - get_config (GET)
//  - save_config (POST)
//  - test (POST)
//  - test_skins (POST)
//  - list (GET) : source, search, limit, offset, with_total
//  - get_one (GET) : source, id
//
// Guest role: list, get_one, get_config (skins_api only).

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';

// Admin: everything. Guest: only read-only lookups (list/get_one) and the public
// part of the config (skins_api). Connection settings are never shown to guests.
$ROLE = bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN, EDITOR_ROLE_GUEST]);

// ── Source registry ──────────────────────────────────────────────────────────
// Whitelisted sources for DBSelect.
// Each source returns items {value, label}

const SOURCES = [
    'quests'     => ['table' => 'tw_quests_list', 'id' => 'ID', 'label' => 'Name'],
    'items'      => ['table' => 'tw_items_list',  'id' => 'ID', 'label' => 'Name', 'filters' => ['Type', 'Group']],
    'worlds'     => ['table' => 'tw_worlds',      'id' => 'ID', 'label' => 'Name'],
    'skills'     => ['table' => 'tw_skills_list', 'id' => 'ID', 'label' => 'Name'],
    'bots'       => ['table' => 'tw_bots_info',   'id' => 'ID', 'label' => 'Name'],
    'attributes' => ['table' => 'tw_attributes',  'id' => 'ID', 'label' => 'Name'],
];

// Which editor tabs need each lookup source. A guest may only read lookups of the
// tabs the administrator granted (otherwise the dropdowns would leak data from
// editors the account cannot open).
const SOURCE_TABS = [
    'quests'     => ['quests'],
    'items'      => ['items', 'crafts', 'warehouses', 'quests'],
    'worlds'     => ['worlds', 'crafts', 'quests', 'dungeons', 'aethers'],
    'skills'     => ['items', 'bots_mobs', 'quests'],
    'bots'       => ['bots_info', 'bots_mobs', 'bots_npc', 'quests'],
    'attributes' => ['items', 'bots_mobs'],
];

/** Guest gate for lookup sources. Admins always pass. */
function assert_source_access(string $source): void
{
    global $ROLE;
    if ($ROLE === EDITOR_ROLE_ADMIN) {
        return;
    }
    $tabs = SOURCE_TABS[$source] ?? [];
    $pages = editor_guest_pages();
    if (!array_intersect($tabs, $pages)) {
        respond(['ok' => false, 'error' => 'Доступ запрещён: справочник не входит в ваши вкладки.', 'code' => 'guest_forbidden'], 403);
    }
}

// ── Validation helpers ───────────────────────────────────────────────────────

function resolve_source(string $source): array
{
    if (!isset(SOURCES[$source])) {
        respond(['ok' => false, 'error' => 'Неизвестный справочник'], 400);
    }
    return SOURCES[$source];
}

/**
 * Validate that a column name is a simple identifier (letters, digits, underscore).
 * Prevents any SQL injection through column names coming from the source registry.
 */
function validate_column_name(string $name): string
{
    if ($name === '' || !preg_match('/^[a-zA-Z_][a-zA-Z0-9_]*$/', $name)) {
        throw new RuntimeException('Invalid column name: ' . $name);
    }
    return $name;
}

/**
 * Validate and sanitize a table name.
 */
function validate_table_name(string $name): string
{
    if ($name === '' || !preg_match('/^[a-zA-Z_][a-zA-Z0-9_]*$/', $name)) {
        throw new RuntimeException('Invalid table name: ' . $name);
    }
    return $name;
}

// ── Action handlers ──────────────────────────────────────────────────────────

function handle_get_config(): never
{
    global $ROLE;
    $c = load_cfg();

    if ($ROLE !== EDITOR_ROLE_ADMIN) {
        respond(['ok' => true, 'config' => ['skins_api' => (string)($c['skins_api'] ?? '')]]);
    }

    $safe = [
        'host'                => (string)($c['host'] ?? ''),
        'port'                => (int)($c['port'] ?? 3306),
        'database'            => (string)($c['database'] ?? ''),
        'user'                => (string)($c['user'] ?? ''),
        'skins_api'           => (string)($c['skins_api'] ?? ''),
        // Мост к картам игрового сервера (см. api/map.php).
        // Токен наружу не отдаём: только признак, что он сохранён.
        'map_api'             => (string)($c['map_api'] ?? ''),
        'map_api_token_set'   => ((string)($c['map_api_token'] ?? '')) !== '',
        'maps_dir'            => (string)($c['maps_dir'] ?? ''),
        'mapres_dir'          => (string)($c['mapres_dir'] ?? ''),
        'editor_user'         => (string)($c['editor_user'] ?? 'admin'),
        'allow_remote_editor' => (bool)($c['allow_remote_editor'] ?? false),
        'allowed_editor_ips'  => is_array($c['allowed_editor_ips'] ?? null)
            ? array_values($c['allowed_editor_ips'])
            : [],
    ];

    respond(['ok' => true, 'config' => $safe]);
}

function handle_save_config(): never
{
    $body = read_json_body();

    // Atomic read-modify-write under a lock: guest accounts / guest settings are
    // managed by other endpoints and must never be lost by a config save here.
    $saved = mutate_cfg(function (array $prev) use ($body) {
        // Resolve allowed_editor_ips: prefer body, fallback to previous
        $allowedIps = [];
        if (is_array($body['allowed_editor_ips'] ?? null)) {
            $allowedIps = array_values($body['allowed_editor_ips']);
        } elseif (is_array($prev['allowed_editor_ips'] ?? null)) {
            $allowedIps = array_values($prev['allowed_editor_ips']);
        }

        // Handle password: special marker to clear it
        $passwordValue = (string)($body['password'] ?? '');
        $clearPassword = ($body['clear_password'] === true || $passwordValue === '__CLEAR_PASSWORD__');

        $cfg = [
            'host'                => (string)($body['host']     ?? ($prev['host']     ?? '127.0.0.1')),
            'port'                => (int)($body['port']        ?? ($prev['port']     ?? 3306)),
            'database'            => (string)($body['database'] ?? ($prev['database'] ?? '')),
            'user'                => (string)($body['user']     ?? ($prev['user']     ?? 'root')),
            'password'            => $clearPassword ? '' : $passwordValue,
            'skins_api'           => trim((string)($body['skins_api']    ?? ($prev['skins_api']    ?? ''))),
            'map_api'             => trim((string)($body['map_api']      ?? ($prev['map_api']      ?? ''))),
            'maps_dir'            => trim((string)($body['maps_dir']     ?? ($prev['maps_dir']     ?? ''))),
            'mapres_dir'          => trim((string)($body['mapres_dir']   ?? ($prev['mapres_dir']   ?? ''))),
            'editor_user'         => trim((string)($body['editor_user']  ?? ($prev['editor_user']  ?? 'admin'))),
            'editor_password'     => (string)($body['editor_password'] ?? ''),
            'allow_remote_editor' => (bool)($body['allow_remote_editor'] ?? ($prev['allow_remote_editor'] ?? false)),
            'allowed_editor_ips'  => $allowedIps,
        ];

        // Keep existing password if new one is empty AND not explicitly clearing
        if (!$clearPassword && $cfg['password'] === '' && isset($prev['password'])) {
            $cfg['password'] = (string)$prev['password'];
        }

        // Ensure editor_user is never empty
        if ($cfg['editor_user'] === '') {
            $cfg['editor_user'] = (string)($prev['editor_user'] ?? 'admin');
            if ($cfg['editor_user'] === '') {
                $cfg['editor_user'] = 'admin';
            }
        }

        // Keep existing editor password if new one is empty
        if ($cfg['editor_password'] === '' && isset($prev['editor_password'])) {
            $cfg['editor_password'] = (string)$prev['editor_password'];
        }

        // Токен map-api: пустое поле = «не менять», явная очистка — флагом.
        $mapToken = (string)($body['map_api_token'] ?? '');
        if ($body['clear_map_api_token'] === true || $mapToken === '__CLEAR_TOKEN__') {
            $cfg['map_api_token'] = '';
        } elseif ($mapToken !== '') {
            $cfg['map_api_token'] = $mapToken;
        } else {
            $cfg['map_api_token'] = (string)($prev['map_api_token'] ?? '');
        }

        // Адрес моста — только http(s) или пусто.
        if ($cfg['map_api'] !== '' && !preg_match('#^https?://#i', $cfg['map_api'])) {
            throw new EditorUserError('Адрес map-api должен начинаться с http:// или https://');
        }

        // Preserve settings managed elsewhere (guest access, guest accounts, etc.).
        return array_merge($prev, $cfg);
    });

    if (!$saved) {
        respond(['ok' => false, 'error' => 'Не удалось сохранить настройки'], 500);
    }

    respond(['ok' => true]);
}

function handle_test(): never
{
    $mysqli = db_connect();
    $server = $mysqli->server_info;
    $mysqli->close();
    respond(['ok' => true, 'server' => $server]);
}

function handle_test_skins(): never
{
    $body = read_json_body();
    $cfg  = load_cfg();
    $url  = trim((string)($body['url'] ?? ($cfg['skins_api'] ?? '')));

    if ($url === '') {
        respond(['ok' => false, 'error' => 'Skins API URL is empty'], 400);
    }

    $res = test_http_endpoint($url);
    if (!$res['ok']) {
        respond([
            'ok'     => false,
            'error'  => $res['error'] ?? 'Skins API error',
            'status' => $res['status'] ?? null,
        ], 502);
    }

    respond(['ok' => true, 'status' => $res['status'] ?? 200]);
}

/**
 * Проверка моста к картам игрового сервера.
 * Запрос: { url?: string, token?: string } — можно проверить адрес до сохранения.
 * Ответ: { ok, status } или { ok: false, error }.
 */
function handle_test_map_api(): never
{
    $body = read_json_body();
    $cfg  = load_cfg();

    $url = trim((string)($body['url'] ?? ($cfg['map_api'] ?? '')));
    $token = (string)($body['token'] ?? ($cfg['map_api_token'] ?? ''));

    if ($url === '') {
        respond(['ok' => false, 'error' => 'Адрес map-api не задан'], 400);
    }

    $target = $url . (str_contains($url, '?') ? '&' : '?') . 'action=status';
    $res = map_api_request($target, $token);

    if (!$res['ok']) {
        respond([
            'ok'     => false,
            'error'  => $res['error'] ?? 'map-api недоступен',
            'status' => $res['status'] ?? null,
        ], 502);
    }

    $data = json_decode((string)$res['body'], true);
    respond([
        'ok'     => true,
        'status' => $res['status'] ?? 200,
        'source' => is_array($data) ? (string)($data['source'] ?? '') : '',
    ]);
}

function handle_list(): never
{
    $source = (string)($_GET['source'] ?? '');
    $meta   = resolve_source($source);
    assert_source_access($source);

    $search         = trim((string)($_GET['search'] ?? ''));
    $filterKey      = trim((string)($_GET['filter_key'] ?? ''));
    $filterValuesRaw = trim((string)($_GET['filter_values'] ?? ''));
    $filterValue    = trim((string)($_GET['filter_value'] ?? ''));
    $limit          = max(1, min(5000, (int)($_GET['limit'] ?? 1000)));
    $fetchLimit     = $limit + 1; // fetch one extra to detect "has_more"
    $offset         = max(0, (int)($_GET['offset'] ?? 0));
    $withTotal      = (string)($_GET['with_total'] ?? '') === '1';

    $table    = validate_table_name($meta['table']);
    $idCol    = validate_column_name($meta['id']);
    $labelCol = validate_column_name($meta['label']);

    // Validate filter key against whitelist
    if ($filterKey !== '') {
        $allowed = $meta['filters'] ?? [];
        if (!in_array($filterKey, $allowed, true)) {
            respond(['ok' => false, 'error' => 'Invalid filter key'], 400);
        }
        validate_column_name($filterKey);
    }

    // Build WHERE clause
    $whereParts = [];
    $types      = '';
    $params     = [];

    if ($search !== '') {
        if (ctype_digit($search)) {
            $whereParts[] = "(`{$idCol}` = ? OR `{$labelCol}` LIKE ?)";
            $types  .= 'is';
            $params[] = (int)$search;
            $params[] = '%' . $search . '%';
        } else {
            $whereParts[] = "`{$labelCol}` LIKE ?";
            $types  .= 's';
            $params[] = '%' . $search . '%';
        }
    }

    // Parse filter values
    $filterValues = parse_filter_values($filterValuesRaw, $filterValue);

    if ($filterKey !== '' && $filterValues !== []) {
        $placeholders = implode(',', array_fill(0, count($filterValues), '?'));
        $whereParts[] = "`{$filterKey}` IN ({$placeholders})";
        $types .= str_repeat('s', count($filterValues));
        array_push($params, ...$filterValues);
    }

    $whereClause = $whereParts !== [] ? ('WHERE ' . implode(' AND ', $whereParts)) : '';

    $mysqli = db_connect();

    try {
        // Optional total count
        $total = null;
        if ($withTotal) {
            $total = query_total_count($mysqli, $table, $whereClause, $types, $params);
        }

        // Main query
        $sql  = "SELECT `{$idCol}` AS id, `{$labelCol}` AS label FROM `{$table}` {$whereClause} ORDER BY `{$idCol}` ASC LIMIT ? OFFSET ?";
        $stmt = $mysqli->prepare($sql);
        if ($stmt === false) {
            throw new RuntimeException('Ошибка подготовки запроса к базе: ' . $mysqli->error);
        }

        $bindTypes  = $types . 'ii';
        $bindParams = array_merge($params, [$fetchLimit, $offset]);
        $stmt->bind_param($bindTypes, ...$bindParams);
        $stmt->execute();

        $res   = $stmt->get_result();
        $items = [];
        while ($r = $res->fetch_assoc()) {
            $items[] = [
                'value' => (string)$r['id'],
                'label' => (string)($r['label'] ?? ''),
            ];
        }
        $stmt->close();
    } finally {
        $mysqli->close();
    }

    $hasMore = count($items) > $limit;
    if ($hasMore) {
        $items = array_slice($items, 0, $limit);
    }

    respond([
        'ok'       => true,
        'items'    => $items,
        'has_more' => $hasMore,
        'total'    => $withTotal ? $total : null,
    ]);
}

function handle_get_one(): never
{
    $source = (string)($_GET['source'] ?? '');
    $id     = (string)($_GET['id'] ?? '');
    $meta   = resolve_source($source);
    assert_source_access($source);

    if ($id === '' || !ctype_digit($id)) {
        respond(['ok' => false, 'error' => 'Некорректный идентификатор записи'], 400);
    }

    $table    = validate_table_name($meta['table']);
    $idCol    = validate_column_name($meta['id']);
    $labelCol = validate_column_name($meta['label']);

    $mysqli = db_connect();

    try {
        $sql  = "SELECT `{$idCol}` AS id, `{$labelCol}` AS label FROM `{$table}` WHERE `{$idCol}` = ? LIMIT 1";
        $stmt = $mysqli->prepare($sql);
        if ($stmt === false) {
            throw new RuntimeException('Ошибка подготовки запроса к базе: ' . $mysqli->error);
        }

        $iid = (int)$id;
        $stmt->bind_param('i', $iid);
        $stmt->execute();

        $res = $stmt->get_result();
        $row = $res ? $res->fetch_assoc() : null;
        $stmt->close();
    } finally {
        $mysqli->close();
    }

    if (!$row) {
        respond(['ok' => true, 'item' => null]);
    }

    respond(['ok' => true, 'item' => [
        'value' => (string)$row['id'],
        'label' => (string)($row['label'] ?? ''),
    ]]);
}

// ── Internal helpers ─────────────────────────────────────────────────────────

/**
 * Parse filter values from raw input (JSON array or comma-separated string).
 *
 * @return string[]
 */
function parse_filter_values(string $raw, string $singleValue): array
{
    if ($raw !== '') {
        $decoded = json_decode($raw, true);
        if (is_array($decoded)) {
            return array_values(array_filter(
                array_map('trim', $decoded),
                static fn(string $v): bool => $v !== ''
            ));
        }
        return array_values(array_filter(
            array_map('trim', explode(',', $raw)),
            static fn(string $v): bool => $v !== ''
        ));
    }

    if ($singleValue !== '') {
        return [$singleValue];
    }

    return [];
}

/**
 * Execute a COUNT(*) query with the given WHERE clause and return the total.
 */
function query_total_count(mysqli $mysqli, string $table, string $whereClause, string $types, array $params): int
{
    $sql  = "SELECT COUNT(*) AS c FROM `{$table}` {$whereClause}";
    $stmt = $mysqli->prepare($sql);
    if ($stmt === false) {
        throw new RuntimeException('Ошибка подготовки запроса к базе: ' . $mysqli->error);
    }

    if ($types !== '') {
        $stmt->bind_param($types, ...$params);
    }

    $stmt->execute();
    $res = $stmt->get_result();
    $row = $res ? $res->fetch_assoc() : null;
    $stmt->close();

    return $row ? (int)$row['c'] : 0;
}

// ── Router ───────────────────────────────────────────────────────────────────

$action = (string)($_GET['action'] ?? '');

$handlers = [
    'get_config'  => 'handle_get_config',
    'save_config' => 'handle_save_config',
    'test'         => 'handle_test',
    'test_skins'   => 'handle_test_skins',
    'test_map_api' => 'handle_test_map_api',
    'list'        => 'handle_list',
    'get_one'     => 'handle_get_one',
];

try {
    guest_action_guard($ROLE, $action, ['list', 'get_one', 'get_config']);
    // Запись конфигурации — только JSON-POST с этого же узла (см. db-core.php).
    guard_write_actions($action, ['save_config', 'test_map_api']);
    if (isset($handlers[$action])) {
        $handlers[$action]();
    } else {
        respond(['ok' => false, 'error' => 'Неизвестное действие'], 400);
    }
} catch (Throwable $e) {
    editor_internal_error($e);
}