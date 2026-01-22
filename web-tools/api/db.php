<?php
// api/db.php
// Secure, editor-agnostic DB API used by UI components (DB Select, Settings page)

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('X-Content-Type-Options: nosniff');

$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';

// Preflight (optional). In production prefer same-origin and remove '*'.
if ($method === 'OPTIONS') {
  header('Access-Control-Allow-Origin: *');
  header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
  header('Access-Control-Allow-Headers: Content-Type');
  exit(0);
}

function json_out(array $payload, int $code = 200): void {
  http_response_code($code);
  echo json_encode($payload, JSON_UNESCAPED_UNICODE);
  exit;
}

$rootDir = dirname(__DIR__); // /web-tools
$dataDir = $rootDir . DIRECTORY_SEPARATOR . 'data';
$configPath = $dataDir . DIRECTORY_SEPARATOR . 'db-config.json';

function read_body_json(): array {
  $raw = file_get_contents('php://input');
  if (!$raw) return [];
  $data = json_decode($raw, true);
  if (json_last_error() !== JSON_ERROR_NONE || !is_array($data)) {
    json_out(['ok' => false, 'error' => 'Некорректный JSON'], 400);
  }
  return $data;
}

function read_config(string $configPath): array {
  if (!file_exists($configPath)) return [];
  $raw = file_get_contents($configPath);
  if (!$raw) return [];
  $cfg = json_decode($raw, true);
  if (json_last_error() !== JSON_ERROR_NONE || !is_array($cfg)) return [];
  return $cfg;
}

function ensure_data_dir(string $dataDir): void {
  if (!is_dir($dataDir)) {
    if (!mkdir($dataDir, 0750, true) && !is_dir($dataDir)) {
      json_out(['ok' => false, 'error' => 'Не удалось создать директорию data/'], 500);
    }
  }
}

function write_config(string $dataDir, string $configPath, array $cfg): void {
  ensure_data_dir($dataDir);
  $json = json_encode($cfg, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE);
  if ($json === false) {
    json_out(['ok' => false, 'error' => 'Не удалось сериализовать конфиг'], 500);
  }
  $ok = file_put_contents($configPath, $json, LOCK_EX);
  if ($ok === false) {
    json_out(['ok' => false, 'error' => 'Не удалось сохранить конфиг'], 500);
  }
  @chmod($configPath, 0600);
}

function build_dsn(array $cfg): string {
  $host = $cfg['host'] ?? '';
  $port = $cfg['port'] ?? 3306;
  $db = $cfg['database'] ?? '';
  if (!$host || !$db) {
    json_out(['ok' => false, 'error' => 'Конфиг БД не заполнен'], 400);
  }
  $port = is_numeric($port) ? (int)$port : 3306;
  return sprintf('mysql:host=%s;port=%d;dbname=%s;charset=utf8mb4', $host, $port, $db);
}

function get_pdo(array $cfg): PDO {
  $dsn = build_dsn($cfg);
  $user = $cfg['user'] ?? '';
  $pass = $cfg['password'] ?? '';
  try {
    $pdo = new PDO($dsn, $user, $pass, [
      PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
      PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
      PDO::ATTR_EMULATE_PREPARES => false,
    ]);
    return $pdo;
  } catch (Throwable $e) {
    json_out(['ok' => false, 'error' => 'Ошибка подключения к БД', 'details' => $e->getMessage()], 500);
  }
}


function assert_ident(string $name, string $what = 'identifier'): string {
  // allow `schema.table` (rare) but block everything else
  if (!preg_match('/^[A-Za-z0-9_]+(\.[A-Za-z0-9_]+)?$/', $name)) {
    json_out(['ok' => false, 'error' => "Недопустимый {$what}"], 400);
  }
  return $name;
}

function q_ident(string $name): string {
  // Quote identifiers with backticks. Supports optional schema prefix: a.b
  $name = assert_ident($name);
  $parts = explode('.', $name);
  $parts = array_map(fn($p) => '`' . $p . '`', $parts);
  return implode('.', $parts);
}

$action = $_GET['action'] ?? '';

// Whitelisted list sources (db_select / db_multiselect)
// You can override any source via data/db-sources.json:
//
// {
//   "items":  { "table":"tw_items_list", "value":"ID", "label":"Name" },
//   "quests": { "table":"tw_quests_list", "value":"ID", "label":"Name" }
// }
$SOURCES = [
  'quests' => ['table' => 'tw_quests_list', 'value' => 'ID', 'label' => 'Name'],
  'items'  => ['table' => 'tw_items_list',  'value' => 'ID', 'label' => 'Name'],
  'worlds' => ['table' => 'tw_worlds',      'value' => 'ID', 'label' => 'Name'],
  'skills' => ['table' => 'tw_skills_list', 'value' => 'ID', 'label' => 'Name'],
  'bots'   => ['table' => 'tw_bots_info',   'value' => 'ID', 'label' => 'Name'],
];

function read_sources_override(string $dataDir): array {
  $path = $dataDir . '/db-sources.json';
  if (!is_file($path)) return [];
  $raw = @file_get_contents($path);
  if ($raw === false) return [];
  $j = json_decode($raw, true);
  return is_array($j) ? $j : [];
}

function sanitize_ident(string $s): ?string {
  // allow only simple identifiers: letters/numbers/underscore
  return preg_match('/^[A-Za-z0-9_]+$/', $s) ? $s : null;
}

function resolve_source_def(string $source, array $SOURCES, string $dataDir): array {
  $def = $SOURCES[$source] ?? [];
  $over = read_sources_override($dataDir);

  // Apply overrides from data/db-sources.json if present.
  if (isset($over[$source]) && is_array($over[$source])) {
    $o = $over[$source];
    if (isset($o['table'])) {
      $t = sanitize_ident((string)$o['table']);
      if ($t) $def['table'] = $t;
    }
    if (isset($o['value'])) {
      $v = sanitize_ident((string)$o['value']);
      if ($v) $def['value'] = $v;
    }
    if (isset($o['label'])) {
      $l = sanitize_ident((string)$o['label']);
      if ($l) $def['label'] = $l;
    }
  }

  return $def;
}


if ($action === 'get_config' && $method === 'GET') {
  $cfg = read_config($configPath);
  // Never return password
  $out = $cfg;
  if (isset($out['password'])) unset($out['password']);
  $out['has_password'] = !empty($cfg['password']);
  json_out(['ok' => true, 'config' => $out]);
}

if ($action === 'save_config' && $method === 'POST') {
  $body = read_body_json();
  $prev = read_config($configPath);

  $host = trim((string)($body['host'] ?? ''));
  $db   = trim((string)($body['database'] ?? ''));
  $user = trim((string)($body['user'] ?? ''));
  $port = $body['port'] ?? 3306;

  if ($host === '' || $db === '' || $user === '') {
    json_out(['ok' => false, 'error' => 'Заполните host, database и user'], 400);
  }
  if (!is_numeric($port)) {
    json_out(['ok' => false, 'error' => 'Port должен быть числом'], 400);
  }

  $new = [
    'host' => $host,
    'port' => (int)$port,
    'database' => $db,
    'user' => $user,
  ];

  // Password: keep previous if empty
  $pwd = (string)($body['password'] ?? '');
  if ($pwd !== '') {
    $new['password'] = $pwd;
  } else if (!empty($prev['password'])) {
    $new['password'] = $prev['password'];
  }

  write_config($dataDir, $configPath, $new);

  $out = $new;
  unset($out['password']);
  $out['has_password'] = !empty($new['password']);
  json_out(['ok' => true, 'config' => $out]);
}

if ($action === 'test' && $method === 'POST') {
  $cfg = read_config($configPath);
  if (!$cfg) json_out(['ok' => false, 'error' => 'Конфиг БД не настроен'], 400);
  $pdo = get_pdo($cfg);
  $server = null;
  try {
    $server = $pdo->query('SELECT VERSION() AS v')->fetch()['v'] ?? null;
  } catch (Throwable $e) {
    // ignore
  }
  json_out(['ok' => true, 'server' => $server]);
}


if ($action === 'get_one' && $method === 'GET') {
  $source = (string)($_GET['source'] ?? '');
  $idRaw = $_GET['id'] ?? null;
  if ($idRaw === null || $idRaw === '') json_out(['ok' => false, 'error' => 'Не указан id'], 400);
  $id = is_numeric($idRaw) ? (int)$idRaw : 0;
  if ($id <= 0) json_out(['ok' => false, 'error' => 'Некорректный id'], 400);

  if (!isset($SOURCES[$source])) {
    json_out(['ok' => false, 'error' => 'Неизвестный source'], 400);
  }

  $cfg = read_config($configPath);
  if (!$cfg) json_out(['ok' => false, 'error' => 'Конфиг БД не настроен'], 400);
  $pdo = get_pdo($cfg);

  $def = resolve_source_def($source, $SOURCES, $dataDir);

  $tbl = q_ident($def['table']);
  $colV = q_ident($def['value']);
  $colL = q_ident($def['label']);

  try {
    $stmt = $pdo->prepare("SELECT {$colV} AS value, {$colL} AS label FROM {$tbl} WHERE {$colV} = :id LIMIT 1");
    $stmt->execute([':id' => $id]);
    $r = $stmt->fetch();
    if (!$r) json_out(['ok' => true, 'item' => null]);
    json_out(['ok' => true, 'item' => ['value' => (int)$r['value'], 'label' => (string)$r['label']]]);
  } catch (Throwable $e) {
    json_out(['ok' => false, 'error' => 'Ошибка запроса к БД', 'details' => $e->getMessage()], 500);
  }
}

if ($action === 'list' && $method === 'GET') {
  $source = (string)($_GET['source'] ?? '');
  if (!isset($SOURCES[$source])) {
    json_out(['ok' => false, 'error' => 'Неизвестный source'], 400);
  }

  $cfg = read_config($configPath);
  if (!$cfg) json_out(['ok' => false, 'error' => 'Конфиг БД не настроен'], 400);
  $pdo = get_pdo($cfg);

  $search = trim((string)($_GET['search'] ?? ''));
  $limit  = (int)($_GET['limit'] ?? 1000);
  $offset = (int)($_GET['offset'] ?? 0);
  $withTotal = (string)($_GET['with_total'] ?? '') === '1';

  if ($limit < 1) $limit = 1;
  if ($limit > 5000) $limit = 5000;
  if ($offset < 0) $offset = 0;
  if ($offset > 2000000) $offset = 2000000;
  if (mb_strlen($search) > 100) $search = mb_substr($search, 0, 100);

  $def = resolve_source_def($source, $SOURCES, $dataDir);

  $tbl = q_ident($def['table']);
  $colV = q_ident($def['value']);
  $colL = q_ident($def['label']);

  $where = '';
  $params = [];
  if ($search !== '') {
    $where = " WHERE {$colL} LIKE :q";
    $params[':q'] = '%' . $search . '%';
  }

  $sql = "SELECT {$colV} AS value, {$colL} AS label FROM {$tbl}{$where} ORDER BY {$colV} ASC LIMIT {$limit} OFFSET {$offset}";

  try {
    $stmt = $pdo->prepare($sql);
    $stmt->execute($params);
    $rows = $stmt->fetchAll();

    $items = array_map(function($r) {
      return [
        'value' => isset($r['value']) ? (int)$r['value'] : 0,
        'label' => isset($r['label']) ? (string)$r['label'] : '',
      ];
    }, $rows ?: []);

    $total = null;
    if ($withTotal) {
      $stmt2 = $pdo->prepare("SELECT COUNT(*) AS c FROM {$tbl}{$where}");
      $stmt2->execute($params);
      $total = (int)($stmt2->fetch()['c'] ?? 0);
    }

    $hasMore = $withTotal ? (($offset + count($items)) < (int)$total) : (count($items) === $limit);

    json_out([
      'ok' => true,
      'items' => $items,
      'limit' => $limit,
      'offset' => $offset,
      'total' => $total,
      'has_more' => $hasMore
    ]);
  } catch (Throwable $e) {
    json_out(['ok' => false, 'error' => 'Ошибка запроса к БД', 'details' => $e->getMessage()], 500);
  }
}

json_out(['ok' => false, 'error' => 'Unknown action'], 404);
