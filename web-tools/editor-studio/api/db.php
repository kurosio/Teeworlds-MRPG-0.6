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

header('Content-Type: application/json; charset=utf-8');

require_once __DIR__ . '/db-core.php';

ensure_editor_auth();

// Whitelisted sources for DBSelect.
// Each source returns items {value,label}
$SOURCES = [
  'quests' => ['table' => 'tw_quests_list', 'id' => 'ID', 'label' => 'Name'],
  'items'  => ['table' => 'tw_items_list',  'id' => 'ID', 'label' => 'Name', 'filters' => ['Type', 'Group']],
  'worlds' => ['table' => 'tw_worlds',      'id' => 'ID', 'label' => 'Name'],
  'skills' => ['table' => 'tw_skills_list', 'id' => 'ID', 'label' => 'Name'],
  'bots'   => ['table' => 'tw_bots_info',   'id' => 'ID', 'label' => 'Name'],
  'attributes' => ['table' => 'tw_attributes', 'id' => 'ID', 'label' => 'Name'],
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
      'skins_api' => (string)($c['skins_api'] ?? ''),
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
      'skins_api' => trim((string)($body['skins_api'] ?? ($prev['skins_api'] ?? ''))),
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

  if ($action === 'test_skins') {
    $body = read_json_body();
    $cfg = load_cfg();
    $url = trim((string)($body['url'] ?? ($cfg['skins_api'] ?? '')));
    if ($url === '') {
      respond(['ok' => false, 'error' => 'Skins API URL is empty'], 400);
    }
    $res = test_http_endpoint($url);
    if (!$res['ok']) {
      respond(['ok' => false, 'error' => $res['error'] ?? 'Skins API error', 'status' => $res['status'] ?? null], 502);
    }
    respond(['ok' => true, 'status' => $res['status'] ?? 200]);
  }

  if ($action === 'list') {
    $source = (string)($_GET['source'] ?? '');
    if (!isset($SOURCES[$source])) {
      respond(['ok' => false, 'error' => 'Unknown source'], 400);
    }
    $meta = $SOURCES[$source];
    $search = trim((string)($_GET['search'] ?? ''));
    $filterKey = trim((string)($_GET['filter_key'] ?? ''));
    $filterValue = trim((string)($_GET['filter_value'] ?? ''));
    $filterValuesRaw = trim((string)($_GET['filter_values'] ?? ''));
    $limit = max(1, min(5000, (int)($_GET['limit'] ?? 1000)));
    $offset = max(0, (int)($_GET['offset'] ?? 0));
    $withTotal = (string)($_GET['with_total'] ?? '') === '1';

    $mysqli = db_connect();
    $table = $meta['table'];
    $idCol = $meta['id'];
    $labelCol = $meta['label'];

    if ($filterKey !== '') {
      $allowed = $meta['filters'] ?? [];
      if (!in_array($filterKey, $allowed, true)) {
        respond(['ok' => false, 'error' => 'Invalid filter key'], 400);
      }
    }

    $whereParts = [];
    $types = '';
    $params = [];
    if ($search !== '') {
      $isNum = ctype_digit($search);
      if ($isNum) {
        $whereParts[] = "(`$idCol` = ? OR `$labelCol` LIKE ?)";
        $types .= 'is';
        $params[] = (int)$search;
        $params[] = '%' . $search . '%';
      } else {
        $whereParts[] = "`$labelCol` LIKE ?";
        $types .= 's';
        $params[] = '%' . $search . '%';
      }
    }
    $filterValues = [];
    if ($filterValuesRaw !== '') {
      $decoded = json_decode($filterValuesRaw, true);
      if (is_array($decoded)) {
        $filterValues = array_values(array_filter(array_map('trim', $decoded), static fn($v) => $v !== ''));
      } else {
        $filterValues = array_values(array_filter(array_map('trim', explode(',', $filterValuesRaw)), static fn($v) => $v !== ''));
      }
    } elseif ($filterValue !== '') {
      $filterValues = [$filterValue];
    }

    if ($filterKey !== '' && $filterValues) {
      $placeholders = implode(',', array_fill(0, count($filterValues), '?'));
      $whereParts[] = "`$filterKey` IN ($placeholders)";
      $types .= str_repeat('s', count($filterValues));
      $params = array_merge($params, $filterValues);
    }
    $where = $whereParts ? ('WHERE ' . implode(' AND ', $whereParts)) : '';

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
