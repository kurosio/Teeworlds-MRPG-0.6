<?php
// DB list API for DB Select components + settings page.
// Endpoint: api/db.php
// Actions:
//  - get_config (GET)
//  - save_config (POST)
//  - test (POST)
//  - list (GET) : source, search, limit, offset, with_total
//  - get_one (GET) : source, id

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');

function respond(array $payload, int $code = 200): void {
  http_response_code($code);
  echo json_encode($payload, JSON_UNESCAPED_UNICODE);
  exit;
}

function read_json_body(): array {
  $raw = file_get_contents('php://input');
  if (!$raw) return [];
  $data = json_decode($raw, true);
  return is_array($data) ? $data : [];
}

function cfg_path(): string {
  return dirname(__DIR__) . '/data/db-config.json';
}

function load_cfg(): array {
  $path = cfg_path();
  if (!file_exists($path)) return [];
  $raw = file_get_contents($path);
  $cfg = json_decode($raw ?: 'null', true);
  return is_array($cfg) ? $cfg : [];
}

function save_cfg(array $cfg): bool {
  $path = cfg_path();
  $dir = dirname($path);
  if (!is_dir($dir)) @mkdir($dir, 0775, true);
  $json = json_encode($cfg, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
  return file_put_contents($path, $json) !== false;
}

function db_connect(): mysqli {
  $cfg = load_cfg();
  $host = (string)($cfg['host'] ?? '127.0.0.1');
  $port = (int)($cfg['port'] ?? 3306);
  $db   = (string)($cfg['database'] ?? '');
  $user = (string)($cfg['user'] ?? 'root');
  $pass = (string)($cfg['password'] ?? '');

  mysqli_report(MYSQLI_REPORT_OFF);
  $mysqli = @new mysqli($host, $user, $pass, $db, $port);
  if ($mysqli->connect_errno) {
    throw new RuntimeException('DB connection failed: ' . $mysqli->connect_error);
  }
  $mysqli->set_charset('utf8mb4');
  return $mysqli;
}

// Whitelisted sources for DBSelect.
// Each source returns items {value,label}
$SOURCES = [
  'quests' => ['table' => 'tw_quests_list', 'id' => 'ID', 'label' => 'Name'],
  'items'  => ['table' => 'tw_items_list',  'id' => 'ID', 'label' => 'Name'],
  'worlds' => ['table' => 'tw_worlds',      'id' => 'ID', 'label' => 'Name'],
  'skills' => ['table' => 'tw_skills_list', 'id' => 'ID', 'label' => 'Name'],
  'bots'   => ['table' => 'tw_bots_info',   'id' => 'ID', 'label' => 'Name'],
];

$action = (string)($_GET['action'] ?? '');

try {
  if ($action === 'get_config') {
    $c = load_cfg();
    $safe = [
      'host' => (string)($c['host'] ?? ''),
      'port' => (int)($c['port'] ?? 3306),
      'database' => (string)($c['database'] ?? ''),
      'user' => (string)($c['user'] ?? ''),
    ];
    respond(['ok' => true, 'config' => $safe]);
  }

  if ($action === 'save_config') {
    $body = read_json_body();
    $prev = load_cfg();
    $cfg = [
      'host' => (string)($body['host'] ?? ($prev['host'] ?? '127.0.0.1')),
      'port' => (int)($body['port'] ?? ($prev['port'] ?? 3306)),
      'database' => (string)($body['database'] ?? ($prev['database'] ?? '')),
      'user' => (string)($body['user'] ?? ($prev['user'] ?? 'root')),
      // if password empty, keep existing
      'password' => (string)($body['password'] ?? ''),
    ];
    if ($cfg['password'] === '' && isset($prev['password'])) {
      $cfg['password'] = (string)$prev['password'];
    }
    if (!save_cfg($cfg)) {
      respond(['ok' => false, 'error' => 'Failed to save config'], 500);
    }
    respond(['ok' => true]);
  }

  if ($action === 'test') {
    $mysqli = db_connect();
    $server = $mysqli->server_info;
    $mysqli->close();
    respond(['ok' => true, 'server' => $server]);
  }

  if ($action === 'list') {
    $source = (string)($_GET['source'] ?? '');
    if (!isset($SOURCES[$source])) {
      respond(['ok' => false, 'error' => 'Unknown source'], 400);
    }
    $meta = $SOURCES[$source];
    $search = trim((string)($_GET['search'] ?? ''));
    $limit = max(1, min(5000, (int)($_GET['limit'] ?? 1000)));
    $offset = max(0, (int)($_GET['offset'] ?? 0));
    $withTotal = (string)($_GET['with_total'] ?? '') === '1';

    $mysqli = db_connect();
    $table = $meta['table'];
    $idCol = $meta['id'];
    $labelCol = $meta['label'];

    $where = '';
    $types = '';
    $params = [];
    if ($search !== '') {
      $isNum = ctype_digit($search);
      if ($isNum) {
        $where = "WHERE `$idCol` = ? OR `$labelCol` LIKE ?";
        $types = 'is';
        $params[] = (int)$search;
        $params[] = '%' . $search . '%';
      } else {
        $where = "WHERE `$labelCol` LIKE ?";
        $types = 's';
        $params[] = '%' . $search . '%';
      }
    }

    $total = null;
    if ($withTotal) {
      $sqlC = "SELECT COUNT(*) AS c FROM `$table` $where";
      $stmtC = $mysqli->prepare($sqlC);
      if ($stmtC === false) throw new RuntimeException('Prepare failed');
      if ($types) $stmtC->bind_param($types, ...$params);
      $stmtC->execute();
      $resC = $stmtC->get_result();
      $rowC = $resC ? $resC->fetch_assoc() : null;
      $total = $rowC ? (int)$rowC['c'] : 0;
      $stmtC->close();
    }

    $sql = "SELECT `$idCol` AS id, `$labelCol` AS label FROM `$table` $where ORDER BY `$idCol` ASC LIMIT ? OFFSET ?";
    $stmt = $mysqli->prepare($sql);
    if ($stmt === false) throw new RuntimeException('Prepare failed');
    if ($types) {
      $stmt->bind_param($types . 'ii', ...array_merge($params, [$limit, $offset]));
    } else {
      $stmt->bind_param('ii', $limit, $offset);
    }
    $stmt->execute();
    $res = $stmt->get_result();
    $items = [];
    while ($r = $res->fetch_assoc()) {
      $items[] = ['value' => (string)$r['id'], 'label' => (string)($r['label'] ?? '')];
    }
    $stmt->close();
    $mysqli->close();

    $hasMore = count($items) >= $limit;
    respond([
      'ok' => true,
      'items' => $items,
      'has_more' => $hasMore,
      'total' => $withTotal ? $total : null,
    ]);
  }

  if ($action === 'get_one') {
    $source = (string)($_GET['source'] ?? '');
    $id = (string)($_GET['id'] ?? '');
    if (!isset($SOURCES[$source])) {
      respond(['ok' => false, 'error' => 'Unknown source'], 400);
    }
    if ($id === '' || !ctype_digit($id)) {
      respond(['ok' => false, 'error' => 'Invalid id'], 400);
    }
    $meta = $SOURCES[$source];
    $mysqli = db_connect();
    $table = $meta['table'];
    $idCol = $meta['id'];
    $labelCol = $meta['label'];

    $sql = "SELECT `$idCol` AS id, `$labelCol` AS label FROM `$table` WHERE `$idCol` = ? LIMIT 1";
    $stmt = $mysqli->prepare($sql);
    if ($stmt === false) throw new RuntimeException('Prepare failed');
    $iid = (int)$id;
    $stmt->bind_param('i', $iid);
    $stmt->execute();
    $res = $stmt->get_result();
    $row = $res ? $res->fetch_assoc() : null;
    $stmt->close();
    $mysqli->close();
    if (!$row) respond(['ok' => true, 'item' => null]);
    respond(['ok' => true, 'item' => ['value' => (string)$row['id'], 'label' => (string)($row['label'] ?? '')]]);
  }

  respond(['ok' => false, 'error' => 'Unknown action'], 400);

} catch (Throwable $e) {
  respond(['ok' => false, 'error' => $e->getMessage()], 500);
}
