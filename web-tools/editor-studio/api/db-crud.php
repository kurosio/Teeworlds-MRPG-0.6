<?php
// Generic CRUD API for DB editors.
// Endpoint: api/db-crud.php
// Actions (query param action=...):
//  - list   (GET)  : resource, search, limit, offset
//  - get    (GET)  : resource, id
//  - create (POST) : resource, {data:{...}}
//  - update (POST) : resource, id, {data:{...}}
//  - delete (POST) : resource, id
//  - rename_group (POST, crafts only): atomic hierarchy rename

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';

bootstrap_editor_api(true);

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
      'BotID','WorldID','PositionX','PositionY','Debuffs','Behavior','Level','Power','Number','Respawn','Radius','ActiveRadius','Boss',
      'it_drop_0','it_drop_1','it_drop_2','it_drop_3','it_drop_4','it_drop_count','it_drop_chance'
    ],
    'search' => ['ID','BotID'],
    'order' => 'ID DESC',
  ],
  // Support mobs editor list with skin previews
  'bots_info' => [
    'table' => 'tw_bots_info',
    'pk' => 'ID',
    'columns' => [
      'Name',
      'JsonTeeInfo',
      'EquippedModules',
      'SlotHammer',
      'SlotGun',
      'SlotShotgun',
      'SlotGrenade',
      'SlotRifle',
      'SlotArmor',
    ],
    'search' => ['ID', 'Name'],
    'order' => 'ID ASC',
  ],
  // New editor: bots-npc-editor.html
  'bots_npc' => [
    'table' => 'tw_bots_npc',
    'pk' => 'ID',
    'columns' => [
      'BotID',
      'PosX',
      'PosY',
      'GiveQuestID',
      'DialogData',
      'Function',
      'Static',
      'Emote',
      'WorldID',
    ],
    'search' => ['ID', 'BotID', 'WorldID'],
    'json' => ['DialogData'],
    'order' => 'ID ASC',
  ],
  // New editor: crafts-editor.html
  'crafts' => [
    'table' => 'tw_crafts_list',
    'pk' => 'ID',
    'columns' => ['GroupName','ItemID','ItemValue','RequiredItems','Price','WorldID'],
    'search' => ['GroupName','ID','ItemID'],
    'order' => 'ID ASC',
  ],
  // New editor: warehouse-editor.html
  'warehouses' => [
    'table' => 'tw_warehouses',
    'pk' => 'ID',
    'columns' => ['Name','Type','Trades','PosX','PosY','StorageData','Currency','WorldID'],
    'search' => ['ID','Name','Type','WorldID','Currency'],
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
  // New editor: aethers-editor.html
  'aethers' => [
    'table' => 'tw_aethers',
    'pk' => 'ID',
    'columns' => ['Name', 'WorldID', 'TeleX', 'TeleY'],
    'search' => ['ID', 'Name', 'WorldID'],
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
      'ScenarioMode',
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

  // New editor: dungeons-editor.html
  'dungeons' => [
    'table' => 'tw_dungeons',
    'pk' => 'ID',
    'columns' => ['Level', 'DoorX', 'DoorY', 'Scenario', 'WorldID', 'TimeLimit'],
    'search' => ['ID', 'Level', 'WorldID'],
    'order' => 'ID ASC',
  ],
  // New editor: quests-editor.html
  'quests' => [
    'table' => 'tw_quests_list',
    'pk' => 'ID',
    'columns' => ['NextQuestID', 'Name', 'Money', 'Exp', 'Flags'],
    'search' => ['ID', 'Name', 'NextQuestID'],
    'order' => 'ID ASC',
  ],
  // New editor: quests-editor.html (quest steps)
  'quest_bots' => [
    'table' => 'tw_bots_quest',
    'pk' => 'ID',
    'columns' => ['BotID', 'QuestID', 'Step', 'WorldID', 'PosX', 'PosY', 'AutoFinish', 'DialogData', 'ScenarioData', 'TasksData'],
    'search' => ['ID', 'QuestID', 'BotID', 'Step'],
    'json' => ['DialogData', 'TasksData'],
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

function bind_type_for_value(mixed $value): string {
  if ($value === null) return 's';
  if (is_int($value)) return 'i';
  if (is_float($value)) return 'd';
  if (is_bool($value)) return 'i';
  if (is_string($value) && preg_match('/^-?\d+$/', $value) === 1) return 'i';
  if (is_string($value) && is_numeric($value)) return 'd';
  return 's';
}

function normalize_bind_value(mixed $value, string $type): mixed {
  if ($value === null) return null;
  if ($type === 'i') return (int)$value;
  if ($type === 'd') return (float)$value;
  return (string)$value;
}

function parse_craft_group_name(string $value): array {
  $raw = trim($value);
  $main = '';
  $nested = $raw;

  if (str_starts_with($raw, '|')) {
    $end = strpos($raw, '|', 1);
    if ($end !== false) {
      $main = trim(substr($raw, 1, $end - 1));
      $nested = substr($raw, $end + 1);
    } else {
      $nested = substr($raw, 1);
    }
  }

  $separator = strpos($nested, ':');
  if ($separator === false) {
    return ['main' => $main, 'group' => trim($nested), 'sub' => ''];
  }

  return [
    'main' => $main,
    'group' => trim(substr($nested, 0, $separator)),
    'sub' => trim(substr($nested, $separator + 1)),
  ];
}

function join_craft_group_name(string $main, string $group, string $sub): string {
  $main = trim($main);
  $group = trim($group);
  $sub = trim($sub);
  $nested = $group !== '' && $sub !== '' ? "$group:$sub" : ($group !== '' ? $group : $sub);

  if ($main === '') return $nested;
  return $nested === '' ? "|$main|" : "|$main|$nested";
}

function craft_text_length(string $value): int {
  if (function_exists('mb_strlen')) return mb_strlen($value, 'UTF-8');
  $matches = [];
  $count = preg_match_all('/./us', $value, $matches);
  return $count === false ? strlen($value) : $count;
}

function fetch_craft_group_rows(mysqli $mysqli, bool $forUpdate = false): array {
  $lockSql = $forUpdate ? ' FOR UPDATE' : '';
  $stmt = $mysqli->prepare("SELECT `ID`, `GroupName` FROM `tw_crafts_list` ORDER BY `ID` ASC$lockSql");
  if ($stmt === false) throw new RuntimeException('Prepare failed');
  $stmt->execute();
  $res = $stmt->get_result();
  $rows = [];
  while ($row = $res->fetch_assoc()) $rows[] = $row;
  $stmt->close();
  return $rows;
}

function craft_rename_parent_matches(array $parts, string $level, string $main, string $group): bool {
  if ($level === 'main') return true;
  return $parts['main'] === $main && ($level === 'group' || $parts['group'] === $group);
}

function craft_rename_matches(array $parts, string $level, string $main, string $group, string $sub): bool {
  if ($level === 'main') return $parts['main'] === $main;
  if ($parts['main'] !== $main || $parts['group'] !== $group) return false;
  return $level === 'group' || $parts['sub'] === $sub;
}

function craft_rename_name(array $parts, string $level): string {
  return $level === 'main' ? $parts['main'] : ($level === 'group' ? $parts['group'] : $parts['sub']);
}

function craft_renamed_group_name(array $parts, string $level, string $newName): string {
  if ($level === 'main') return join_craft_group_name($newName, $parts['group'], $parts['sub']);
  if ($level === 'group') return join_craft_group_name($parts['main'], $newName, $parts['sub']);
  return join_craft_group_name($parts['main'], $parts['group'], $newName);
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

  if ($action === 'rename_group') {
    if ($resource !== 'crafts') {
      $mysqli->close();
      respond(['ok' => false, 'error' => 'This action is only available for crafts'], 400);
    }

    $body = read_json_body();
    $level = (string)($body['level'] ?? '');
    $mainValue = $body['mainGroup'] ?? '';
    $groupValue = $body['group'] ?? '';
    $subValue = $body['subGroup'] ?? '';
    $mainGroup = trim(is_scalar($mainValue) ? (string)$mainValue : '');
    $group = trim(is_scalar($groupValue) ? (string)$groupValue : '');
    $subGroup = trim(is_scalar($subValue) ? (string)$subValue : '');
    $currentName = $level === 'main' ? $mainGroup : ($level === 'group' ? $group : $subGroup);

    if (!in_array($level, ['main', 'group', 'sub'], true) || $currentName === ''
      || ($level === 'sub' && $group === '')) {
      $mysqli->close();
      respond(['ok' => false, 'error' => 'Invalid craft group target'], 400);
    }


    $newValue = $body['newGroup'] ?? '';
    $newName = trim(is_scalar($newValue) ? (string)$newValue : '');
    if ($newName === '' || str_contains($newName, ':') || str_contains($newName, '|')
      || preg_match('//u', $newName) !== 1 || craft_text_length($newName) > 256) {
      $mysqli->close();
      respond(['ok' => false, 'error' => 'Некорректное новое имя группы. Не используйте : или |; максимум 256 символов.'], 400);
    }
    if ($newName === $currentName) {
      $mysqli->close();
      respond(['ok' => true, 'changed' => []]);
    }

    try {
      $mysqli->begin_transaction();
      $updates = [];
      $merged = false;
      foreach (fetch_craft_group_rows($mysqli, true) as $row) {
        $parts = parse_craft_group_name((string)$row['GroupName']);
        if (!craft_rename_parent_matches($parts, $level, $mainGroup, $group)) continue;
        if (craft_rename_name($parts, $level) === $newName) $merged = true;
        if (!craft_rename_matches($parts, $level, $mainGroup, $group, $subGroup)) continue;
        $renamed = craft_renamed_group_name($parts, $level, $newName);
        if (craft_text_length($renamed) > 256) {
          throw new DomainException('Итоговое имя группы для крафта #' . (int)$row['ID'] . ' превышает 256 символов.');
        }
        $updates[] = ['ID' => (int)$row['ID'], 'old' => (string)$row['GroupName'], 'new' => $renamed];
      }

      if ($updates && $merged && ($body['mergeExisting'] ?? false) !== true) {
        $mysqli->rollback();
        $mysqli->close();
        respond(['ok' => false, 'error' => 'Такое имя уже занято на этом уровне. Проверьте предупреждение и повторите.'], 409);
      }

      $changed = [];
      if ($updates) {
        $stmt = $mysqli->prepare('UPDATE `tw_crafts_list` SET `GroupName` = ? WHERE `ID` = ? AND `GroupName` = ? LIMIT 1');
        if ($stmt === false) throw new RuntimeException('Prepare failed');
        foreach ($updates as $update) {
          $newGroupName = $update['new'];
          $id = $update['ID'];
          $oldGroupName = $update['old'];
          $stmt->bind_param('sis', $newGroupName, $id, $oldGroupName);
          $stmt->execute();
          if ($stmt->affected_rows !== 1) throw new RuntimeException('Craft #' . $id . ' changed during rename');
          $changed[] = ['ID' => $id, 'newGroupName' => $newGroupName];
        }
        $stmt->close();
      }
      $mysqli->commit();
    } catch (DomainException $e) {
      $mysqli->rollback();
      $mysqli->close();
      respond(['ok' => false, 'error' => $e->getMessage()], 400);
    } catch (Throwable $e) {
      $mysqli->rollback();
      throw $e;
    }

    $mysqli->close();
    respond(['ok' => true, 'changed' => $changed]);
  }

  if ($action === 'list') {
    $search = trim((string)($_GET['search'] ?? ''));
    $limit = max(1, min(5000, (int)($_GET['limit'] ?? 100)));
    $fetchLimit = min(5001, $limit + 1);
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
      $params2 = array_merge($params, [$fetchLimit, $offset]);
      $stmt->bind_param($types2, ...$params2);
    } else {
      $stmt->bind_param('ii', $fetchLimit, $offset);
    }
    $stmt->execute();
    $res = $stmt->get_result();
    $rows = [];
    while ($r = $res->fetch_assoc()) {
      $rows[] = decode_json_cols($r, $jsonCols);
    }
    $stmt->close();
    $hasMore = count($rows) > $limit;
    if ($hasMore) {
      $rows = array_slice($rows, 0, $limit);
    }
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
      $t = bind_type_for_value($v);
      $types .= $t;
      $vals[] = normalize_bind_value($v, $t);
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
      $t = bind_type_for_value($v);
      $types .= $t;
      $vals[] = normalize_bind_value($v, $t);
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
