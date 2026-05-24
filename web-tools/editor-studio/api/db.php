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

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';

bootstrap_editor_api(true);

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

// ── Validation helpers ───────────────────────────────────────────────────────

function resolve_source(string $source): array
{
    if (!isset(SOURCES[$source])) {
        respond(['ok' => false, 'error' => 'Unknown source'], 400);
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
    $c = load_cfg();

    $safe = [
        'host'                => (string)($c['host'] ?? ''),
        'port'                => (int)($c['port'] ?? 3306),
        'database'            => (string)($c['database'] ?? ''),
        'user'                => (string)($c['user'] ?? ''),
        'skins_api'           => (string)($c['skins_api'] ?? ''),
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
    $prev = load_cfg();

    // Resolve allowed_editor_ips: prefer body, fallback to previous
    $allowedIps = [];
    if (is_array($body['allowed_editor_ips'] ?? null)) {
        $allowedIps = array_values($body['allowed_editor_ips']);
    } elseif (is_array($prev['allowed_editor_ips'] ?? null)) {
        $allowedIps = array_values($prev['allowed_editor_ips']);
    }

    $cfg = [
        'host'                => (string)($body['host']     ?? ($prev['host']     ?? '127.0.0.1')),
        'port'                => (int)($body['port']        ?? ($prev['port']     ?? 3306)),
        'database'            => (string)($body['database'] ?? ($prev['database'] ?? '')),
        'user'                => (string)($body['user']     ?? ($prev['user']     ?? 'root')),
        'password'            => (string)($body['password'] ?? ''),
        'skins_api'           => trim((string)($body['skins_api']    ?? ($prev['skins_api']    ?? ''))),
        'editor_user'         => trim((string)($body['editor_user']  ?? ($prev['editor_user']  ?? 'admin'))),
        'editor_password'     => (string)($body['editor_password'] ?? ''),
        'allow_remote_editor' => (bool)($body['allow_remote_editor'] ?? ($prev['allow_remote_editor'] ?? false)),
        'allowed_editor_ips'  => $allowedIps,
    ];

    // Keep existing password if new one is empty
    if ($cfg['password'] === '' && isset($prev['password'])) {
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

    if (!save_cfg($cfg)) {
        respond(['ok' => false, 'error' => 'Failed to save config'], 500);
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

function handle_list(): never
{
    $source = (string)($_GET['source'] ?? '');
    $meta   = resolve_source($source);

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
            throw new RuntimeException('Prepare failed: ' . $mysqli->error);
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

    if ($id === '' || !ctype_digit($id)) {
        respond(['ok' => false, 'error' => 'Invalid id'], 400);
    }

    $table    = validate_table_name($meta['table']);
    $idCol    = validate_column_name($meta['id']);
    $labelCol = validate_column_name($meta['label']);

    $mysqli = db_connect();

    try {
        $sql  = "SELECT `{$idCol}` AS id, `{$labelCol}` AS label FROM `{$table}` WHERE `{$idCol}` = ? LIMIT 1";
        $stmt = $mysqli->prepare($sql);
        if ($stmt === false) {
            throw new RuntimeException('Prepare failed: ' . $mysqli->error);
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
        throw new RuntimeException('Prepare failed: ' . $mysqli->error);
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
    'test'        => 'handle_test',
    'test_skins'  => 'handle_test_skins',
    'list'        => 'handle_list',
    'get_one'     => 'handle_get_one',
];

try {
    if (isset($handlers[$action])) {
        $handlers[$action]();
    } else {
        respond(['ok' => false, 'error' => 'Unknown action'], 400);
    }
} catch (Throwable $e) {
    respond(['ok' => false, 'error' => $e->getMessage()], 500);
}