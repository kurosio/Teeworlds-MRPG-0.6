<?php
// api/db-crud.php
// Generic CRUD endpoint for DB-backed editors.

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('X-Content-Type-Options: nosniff');

$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';

function json_out(array $payload, int $code = 200): void {
  http_response_code($code);
  echo json_encode($payload, JSON_UNESCAPED_UNICODE);
  exit;
}

$rootDir = dirname(__DIR__); // /web-tools
$dataDir = $rootDir . DIRECTORY_SEPARATOR . 'data';
$configPath = $dataDir . DIRECTORY_SEPARATOR . 'db-config.json';

function read_config(string $configPath): array {
  if (!file_exists($configPath)) return [];
  $raw = file_get_contents($configPath);
  if (!$raw) return [];
  $cfg = json_decode($raw, true);
  if (json_last_error() !== JSON_ERROR_NONE || !is_array($cfg)) return [];
  return $cfg;
}

function read_body_json(): array {
  $raw = file_get_contents('php://input');
  if (!$raw) return [];
  $data = json_decode($raw, true);
  if (json_last_error() !== JSON_ERROR_NONE || !is_array($data)) {
    json_out(['ok' => false, 'error' => 'Некорректный JSON'], 400);
  }
  return $data;
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
    return new PDO($dsn, $user, $pass, [
      PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
      PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
      PDO::ATTR_EMULATE_PREPARES => false,
    ]);
  } catch (Throwable $e) {
    json_out(['ok' => false, 'error' => 'Ошибка подключения к БД', 'details' => $e->getMessage()], 500);
  }
}

// Resource map: define DB-backed entities here.
// Add new DB editors by adding a resource entry.
$RESOURCES = [
  'vouchers' => [
    'table' => 'tw_voucher',
    'pk' => 'ID',
    'columns' => ['Code', 'Data', 'Multiple', 'ValidUntil'],
    'search' => 'Code',
    // per-column type coercion
    'types' => [
      'ID' => 'int',
      'Multiple' => 'bool',
      'ValidUntil' => 'int',
    ],
    // JSON column transformation
    'json_columns' => ['Data'],
  ],
];

$action = (string)($_GET['action'] ?? '');
$resource = (string)($_GET['resource'] ?? '');

if (!isset($RESOURCES[$resource])) {
  json_out(['ok' => false, 'error' => 'Неизвестный resource'], 400);
}

$cfg = read_config($configPath);
if (!$cfg) json_out(['ok' => false, 'error' => 'Конфиг БД не настроен'], 400);
$pdo = get_pdo($cfg);

$def = $RESOURCES[$resource];
$table = $def['table'];
$pk = $def['pk'];
$cols = $def['columns'];
$searchCol = $def['search'] ?? null;
$types = $def['types'] ?? [];
$jsonCols = $def['json_columns'] ?? [];

function coerce_row(array $row, array $types, array $jsonCols): array {
  foreach ($row as $k => $v) {
    if (isset($types[$k])) {
      $t = $types[$k];
      if ($t === 'int') $row[$k] = (int)$v;
      if ($t === 'bool') $row[$k] = (int)$v ? 1 : 0;
    }
    if (in_array($k, $jsonCols, true)) {
      $decoded = json_decode((string)$v, true);
      $row[$k] = (json_last_error() === JSON_ERROR_NONE) ? $decoded : (string)$v;
    }
  }
  return $row;
}

function encode_json_columns(array $data, array $jsonCols): array {
  foreach ($jsonCols as $k) {
    if (array_key_exists($k, $data) && is_array($data[$k])) {
      $data[$k] = json_encode($data[$k], JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
    }
  }
  return $data;
}

if ($action === 'list' && $method === 'GET') {
  $search = trim((string)($_GET['search'] ?? ''));
  $limit = (int)($_GET['limit'] ?? 100);
  $offset = (int)($_GET['offset'] ?? 0);
  if ($limit < 1) $limit = 1;
  if ($limit > 500) $limit = 500;
  if ($offset < 0) $offset = 0;
  if (mb_strlen($search) > 100) $search = mb_substr($search, 0, 100);

  $selectCols = array_merge([$pk], $cols);
  $selectList = implode(', ', array_map(fn($c) => "$c", $selectCols));
  $sql = "SELECT $selectList FROM $table";
  $params = [];
  if ($search !== '' && $searchCol) {
    $sql .= " WHERE $searchCol LIKE :q";
    $params[':q'] = '%' . $search . '%';
  }
  $sql .= " ORDER BY $pk DESC LIMIT :limit OFFSET :offset";

  try {
    $stmt = $pdo->prepare($sql);
    foreach ($params as $k => $v) $stmt->bindValue($k, $v);
    $stmt->bindValue(':limit', $limit, PDO::PARAM_INT);
    $stmt->bindValue(':offset', $offset, PDO::PARAM_INT);
    $stmt->execute();
    $rows = $stmt->fetchAll() ?: [];
    $rows = array_map(fn($r) => coerce_row($r, $types, $jsonCols), $rows);
    json_out(['ok' => true, 'rows' => $rows]);
  } catch (Throwable $e) {
    json_out(['ok' => false, 'error' => 'Ошибка запроса к БД', 'details' => $e->getMessage()], 500);
  }
}

if ($action === 'get' && $method === 'GET') {
  $id = (int)($_GET['id'] ?? 0);
  if ($id <= 0) json_out(['ok' => false, 'error' => 'Некорректный ID'], 400);
  $selectCols = array_merge([$pk], $cols);
  $selectList = implode(', ', array_map(fn($c) => "$c", $selectCols));
  try {
    $stmt = $pdo->prepare("SELECT $selectList FROM $table WHERE $pk = :id LIMIT 1");
    $stmt->execute([':id' => $id]);
    $row = $stmt->fetch();
    if (!$row) json_out(['ok' => false, 'error' => 'Не найдено'], 404);
    json_out(['ok' => true, 'row' => coerce_row($row, $types, $jsonCols)]);
  } catch (Throwable $e) {
    json_out(['ok' => false, 'error' => 'Ошибка запроса к БД', 'details' => $e->getMessage()], 500);
  }
}

if (($action === 'create' || $action === 'update') && $method === 'POST') {
  $body = read_body_json();
  $data = $body['data'] ?? null;
  if (!is_array($data)) json_out(['ok' => false, 'error' => 'Нет data'], 400);

  // Only allow declared columns
  $filtered = [];
  foreach ($cols as $c) {
    if (array_key_exists($c, $data)) $filtered[$c] = $data[$c];
  }
  $filtered = encode_json_columns($filtered, $jsonCols);

  try {
    if ($action === 'create') {
      $names = array_keys($filtered);
      $placeholders = array_map(fn($n) => ':' . $n, $names);
      $sql = sprintf(
        'INSERT INTO %s (%s) VALUES (%s)',
        $table,
        implode(', ', $names),
        implode(', ', $placeholders)
      );
      $stmt = $pdo->prepare($sql);
      foreach ($filtered as $k => $v) $stmt->bindValue(':' . $k, $v);
      $stmt->execute();
      $newId = (int)$pdo->lastInsertId();
      json_out(['ok' => true, 'id' => $newId]);
    }

    // update
    $id = (int)($_GET['id'] ?? 0);
    if ($id <= 0) json_out(['ok' => false, 'error' => 'Некорректный ID'], 400);
    $pairs = implode(', ', array_map(fn($n) => "$n = :$n", array_keys($filtered)));
    $sql = sprintf('UPDATE %s SET %s WHERE %s = :id', $table, $pairs, $pk);
    $stmt = $pdo->prepare($sql);
    foreach ($filtered as $k => $v) $stmt->bindValue(':' . $k, $v);
    $stmt->bindValue(':id', $id, PDO::PARAM_INT);
    $stmt->execute();
    json_out(['ok' => true]);
  } catch (Throwable $e) {
    json_out(['ok' => false, 'error' => 'Ошибка записи в БД', 'details' => $e->getMessage()], 500);
  }
}

if ($action === 'delete' && $method === 'POST') {
  $id = (int)($_GET['id'] ?? 0);
  if ($id <= 0) json_out(['ok' => false, 'error' => 'Некорректный ID'], 400);
  try {
    $stmt = $pdo->prepare("DELETE FROM $table WHERE $pk = :id");
    $stmt->execute([':id' => $id]);
    json_out(['ok' => true]);
  } catch (Throwable $e) {
    json_out(['ok' => false, 'error' => 'Ошибка удаления', 'details' => $e->getMessage()], 500);
  }
}

json_out(['ok' => false, 'error' => 'Unknown action'], 404);
