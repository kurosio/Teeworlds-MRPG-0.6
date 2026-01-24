<?php
// Generic CRUD API for DB editors.
// Endpoint: api/db-crud.php
// Actions (query param action=...):
//  - list   (GET)  : resource, search, limit, offset
//  - get    (GET)  : resource, id
//  - create (POST) : resource, {data:{...}}
//  - update (POST) : resource, id, {data:{...}}
//  - delete (POST) : resource, id

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

function is_assoc(array $arr): bool {
  $i = 0;
  foreach ($arr as $k => $_) {
    if ($k !== $i++) return true;
  }
  return false;
}

// Resource map.
// IMPORTANT: only whitelisted tables/columns are accessible.
$RESOURCES = [
  // Existing editor: vouchers-editor.html
  'vouchers' => [
    'table' => 'tw_voucher',
    'pk' => 'ID',
    'columns' => ['Code', 'Data', 'Multiple', 'ValidUntil'],
    'search' => ['Code', 'ID'],
    'json' => ['Data'],
    'order' => 'ID DESC',
  ],
  // Existing editor: mobs-editor.html
  'bots_mobs' => [
    'table' => 'tw_bots_mobs',
    'pk' => 'ID',
    'columns' => [
      'BotID','WorldID','PositionX','PositionY','Debuffs','Behavior','Level','Power','Number','Respawn','Radius','Boss',
      'it_drop_0','it_drop_1','it_drop_2','it_drop_3','it_drop_4','it_drop_count','it_drop_chance'
    ],
    'search' => ['ID','BotID'],
    'order' => 'ID DESC',
  ],
  // New editor: crafts-editor.html
  'crafts' => [
    'table' => 'tw_crafts_list',
    'pk' => 'ID',
    'columns' => ['GroupName','ItemID','ItemValue','RequiredItems','Price','WorldID'],
    'search' => ['GroupName','ID','ItemID'],
    'order' => 'ID ASC',
  ],
  // New editor: worlds-editor.html
  'worlds' => [
    'table' => 'tw_worlds',
    'pk' => 'ID',
    'columns' => ['Name', 'Path', 'Type', 'Flags', 'RespawnWorldID', 'JailWorldID', 'RequiredLevel'],
    'search' => ['ID', 'Name', 'Path'],
    'order' => 'ID ASC',
  ],
  // New editor: items-editor.html
  'items' => [
    'table' => 'tw_items_list',
    'pk' => 'ID',
    'columns' => [
      'Comment',
      'Name',
      'Description',
      'Group',
      'Type',
      'Flags',
      'ScenarioData',
      'InitialPrice',
      'RequiresProducts',
      'AT1',
      'AT2',
      'ATValue1',
      'ATValue2',
      'Data',
    ],
    'search' => ['ID', 'Name', 'Description', 'Group', 'Type'],
    'json' => ['Data'],
    'order' => 'ID ASC',
  ],
];

function decode_json_cols(array $row, array $jsonCols): array {
  foreach ($jsonCols as $col) {
    if (!array_key_exists($col, $row)) continue;
    $val = $row[$col];
    if ($val == null) continue;
    if (is_array($val) || is_object($val)) continue;
    $s = (string)$val;
    $trim = trim($s);
    if ($trim === '') continue;
    $decoded = json_decode($s, true);
    if (json_last_error() === JSON_ERROR_NONE) {
      $row[$col] = $decoded;
    }
  }
  return $row;
}

function encode_json_cols(array $data, array $jsonCols): array {
  foreach ($jsonCols as $col) {
    if (!array_key_exists($col, $data)) continue;
    $v = $data[$col];
    if (is_array($v) || is_object($v)) {
      $data[$col] = json_encode($v, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
    }
  }
  return $data;
}

function sanitize_columns(array $data, array $allowed): array {
  $out = [];
  foreach ($allowed as $col) {
    if (array_key_exists($col, $data)) $out[$col] = $data[$col];
  }
  return $out;
}

$action = (string)($_GET['action'] ?? '');
$resource = (string)($_GET['resource'] ?? '');

try {
  if (!$action) respond(['ok' => false, 'error' => 'Missing action'], 400);
  if (!isset($RESOURCES[$resource])) respond(['ok' => false, 'error' => 'Unknown resource'], 400);
  $R = $RESOURCES[$resource];
  $table = $R['table'];
  $pk = $R['pk'];
  $cols = $R['columns'];
  $jsonCols = $R['json'] ?? [];
  $order = $R['order'] ?? "$pk DESC";
  $searchCols = $R['search'] ?? [$pk];

  $mysqli = db_connect();

  if ($action === 'list') {
    $search = trim((string)($_GET['search'] ?? ''));
    $limit = max(1, min(5000, (int)($_GET['limit'] ?? 100)));
    $offset = max(0, (int)($_GET['offset'] ?? 0));

    $where = '';
    $types = '';
    $params = [];
    if ($search !== '') {
      $parts = [];
      $isNum = ctype_digit($search);
      foreach ($searchCols as $c) {
        if ($c === $pk && $isNum) {
          $parts[] = "`$c` = ?";
          $types .= 'i';
          $params[] = (int)$search;
        } else {
          $parts[] = "`$c` LIKE ?";
          $types .= 's';
          $params[] = '%' . $search . '%';
        }
      }
      if ($parts) $where = 'WHERE ' . implode(' OR ', $parts);
    }

    $colSql = '`' . $pk . '`, ' . implode(', ', array_map(fn($c) => '`' . $c . '`', $cols));
    $sql = "SELECT $colSql FROM `$table` $where ORDER BY $order LIMIT ? OFFSET ?";
    $stmt = $mysqli->prepare($sql);
    if ($stmt === false) throw new RuntimeException('Prepare failed');
    if ($types) {
      $types2 = $types . 'ii';
      $params2 = array_merge($params, [$limit, $offset]);
      $stmt->bind_param($types2, ...$params2);
    } else {
      $stmt->bind_param('ii', $limit, $offset);
    }
    $stmt->execute();
    $res = $stmt->get_result();
    $rows = [];
    while ($r = $res->fetch_assoc()) {
      $rows[] = decode_json_cols($r, $jsonCols);
    }
    $stmt->close();
    $hasMore = count($rows) >= $limit;
    $mysqli->close();
    respond(['ok' => true, 'rows' => $rows, 'has_more' => $hasMore]);
  }

  if ($action === 'get') {
    $id = (string)($_GET['id'] ?? '');
    if ($id === '' || !ctype_digit($id)) respond(['ok' => false, 'error' => 'Invalid id'], 400);
    $iid = (int)$id;
    $colSql = '`' . $pk . '`, ' . implode(', ', array_map(fn($c) => '`' . $c . '`', $cols));
    $sql = "SELECT $colSql FROM `$table` WHERE `$pk` = ? LIMIT 1";
    $stmt = $mysqli->prepare($sql);
    if ($stmt === false) throw new RuntimeException('Prepare failed');
    $stmt->bind_param('i', $iid);
    $stmt->execute();
    $res = $stmt->get_result();
    $row = $res ? $res->fetch_assoc() : null;
    $stmt->close();
    $mysqli->close();
    if (!$row) respond(['ok' => true, 'row' => null]);
    respond(['ok' => true, 'row' => decode_json_cols($row, $jsonCols)]);
  }

  if ($action === 'create') {
    $body = read_json_body();
    $data = is_array($body['data'] ?? null) ? $body['data'] : [];
    $data = sanitize_columns($data, $cols);
    $data = encode_json_cols($data, $jsonCols);
    if (!$data) respond(['ok' => false, 'error' => 'Empty data'], 400);

    $keys = array_keys($data);
    $placeholders = implode(',', array_fill(0, count($keys), '?'));
    $colSql = implode(',', array_map(fn($c) => '`' . $c . '`', $keys));
    $sql = "INSERT INTO `$table` ($colSql) VALUES ($placeholders)";
    $stmt = $mysqli->prepare($sql);
    if ($stmt === false) throw new RuntimeException('Prepare failed');

    $types = '';
    $vals = [];
    foreach ($keys as $k) {
      $v = $data[$k];
      if ($v === null) {
        // bind null as string, MySQL will coerce (works for ints as well)
        $types .= 's';
        $vals[] = null;
      } elseif (is_int($v) || is_float($v) || (is_string($v) && ctype_digit($v))) {
        $types .= 'i';
        $vals[] = (int)$v;
      } else {
        $types .= 's';
        $vals[] = (string)$v;
      }
    }
    $stmt->bind_param($types, ...$vals);
    $stmt->execute();
    $newId = (int)$mysqli->insert_id;
    $stmt->close();
    $mysqli->close();
    respond(['ok' => true, 'id' => $newId]);
  }

  if ($action === 'update') {
    $id = (string)($_GET['id'] ?? '');
    if ($id === '' || !ctype_digit($id)) respond(['ok' => false, 'error' => 'Invalid id'], 400);
    $iid = (int)$id;
    $body = read_json_body();
    $data = is_array($body['data'] ?? null) ? $body['data'] : [];
    $data = sanitize_columns($data, $cols);
    $data = encode_json_cols($data, $jsonCols);
    if (!$data) respond(['ok' => false, 'error' => 'Empty data'], 400);

    $keys = array_keys($data);
    $setSql = implode(', ', array_map(fn($c) => '`' . $c . '` = ?', $keys));
    $sql = "UPDATE `$table` SET $setSql WHERE `$pk` = ? LIMIT 1";
    $stmt = $mysqli->prepare($sql);
    if ($stmt === false) throw new RuntimeException('Prepare failed');

    $types = '';
    $vals = [];
    foreach ($keys as $k) {
      $v = $data[$k];
      if ($v === null) {
        $types .= 's';
        $vals[] = null;
      } elseif (is_int($v) || is_float($v) || (is_string($v) && ctype_digit($v))) {
        $types .= 'i';
        $vals[] = (int)$v;
      } else {
        $types .= 's';
        $vals[] = (string)$v;
      }
    }
    $types .= 'i';
    $vals[] = $iid;

    $stmt->bind_param($types, ...$vals);
    $stmt->execute();
    $stmt->close();
    $mysqli->close();
    respond(['ok' => true]);
  }

  if ($action === 'delete') {
    $id = (string)($_GET['id'] ?? '');
    if ($id === '' || !ctype_digit($id)) respond(['ok' => false, 'error' => 'Invalid id'], 400);
    $iid = (int)$id;
    $sql = "DELETE FROM `$table` WHERE `$pk` = ? LIMIT 1";
    $stmt = $mysqli->prepare($sql);
    if ($stmt === false) throw new RuntimeException('Prepare failed');
    $stmt->bind_param('i', $iid);
    $stmt->execute();
    $stmt->close();
    $mysqli->close();
    respond(['ok' => true]);
  }

  $mysqli->close();
  respond(['ok' => false, 'error' => 'Unknown action'], 400);

} catch (Throwable $e) {
  respond(['ok' => false, 'error' => $e->getMessage()], 500);
}
