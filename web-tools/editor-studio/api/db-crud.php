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
//
// Every write is recorded in the change history (api/db-history.php): one entry
// per changed record, together with the old values, so it can be reverted.
//
// Guest role: only list/get on resources with guest=true (see db-resources.php).

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';
require_once __DIR__ . '/db-resources.php';
require_once __DIR__ . '/db-history-lib.php';

// Admin: full CRUD. Guest: read-only (list/get) and only guest-visible resources.
// A guest can never write here — its edits become change requests (api/change-requests.php).
$ROLE = bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN, EDITOR_ROLE_GUEST]);

$RESOURCES = editor_resources();
$transactionOpen = false;

$action = (string)($_GET['action'] ?? '');
$resource = (string)($_GET['resource'] ?? '');

try {
  if (!$action) respond(['ok' => false, 'error' => 'Не указано действие'], 400);
  if (!isset($RESOURCES[$resource])) respond(['ok' => false, 'error' => 'Неизвестный раздел'], 400);
  // Сначала роль (гостю запись запрещена в принципе), затем защита от подделки
  // запроса: любая запись в игровую базу — только JSON-POST своего узла.
  guest_action_guard($ROLE, $action, ['list', 'get']);
  guard_write_actions($action, ['create', 'update', 'delete', 'rename_group']);
  if ($ROLE === EDITOR_ROLE_GUEST) {
    // Two independent gates: the resource must be guest-visible at all AND the
    // administrator must have granted the matching editor tab to this account.
    if (empty($RESOURCES[$resource]['guest']) || !guest_can_read_resource($resource)) {
      respond(['ok' => false, 'error' => 'Доступ запрещён: этот раздел не включён для вашего гостевого аккаунта.', 'code' => 'guest_forbidden'], 403);
    }
  }
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
      respond(['ok' => false, 'error' => 'Действие доступно только в редакторе крафтов'], 400);
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
      respond(['ok' => false, 'error' => 'Некорректная целевая группа крафтов'], 400);
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
      if (!$mysqli->begin_transaction()) throw new RuntimeException('Не удалось начать транзакцию');
      $transactionOpen = true;
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
        $transactionOpen = false;
        $mysqli->close();
        respond(['ok' => false, 'error' => 'Такое имя уже занято на этом уровне. Проверьте предупреждение и повторите.'], 409);
      }

      $changed = [];
      if ($updates) {
        $stmt = $mysqli->prepare('UPDATE `tw_crafts_list` SET `GroupName` = ? WHERE `ID` = ? AND `GroupName` = ? LIMIT 1');
        if ($stmt === false) throw new RuntimeException('Ошибка подготовки запроса к базе');
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
      if (!$mysqli->commit()) throw new RuntimeException('Не удалось завершить транзакцию');
      $transactionOpen = false;
    } catch (DomainException $e) {
      $mysqli->rollback();
      $transactionOpen = false;
      $mysqli->close();
      respond(['ok' => false, 'error' => $e->getMessage()], 400);
    } catch (Throwable $e) {
      $mysqli->rollback();
      $transactionOpen = false;
      throw $e;
    }

    // Переименование группы — одно действие пользователя, но каждая задетая
    // запись крафта меняется отдельно: в истории столько записей, сколько строк.
    foreach ($updates as $update) {
      db_history_safe_record($R, $resource, 'update', (int)$update['ID'],
        ['GroupName' => $update['old']], ['GroupName' => $update['new']]);
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
    if ($stmt === false) throw new RuntimeException('Ошибка подготовки запроса к базе');
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
    $iid = crud_record_id((string)($_GET['id'] ?? ''), $resource);
    if ($iid === null) respond(['ok' => false, 'error' => 'Некорректный идентификатор записи'], 400);
    $row = crud_fetch_row($mysqli, $R, $iid);
    $mysqli->close();
    if (!$row) respond(['ok' => true, 'row' => null]);
    respond(['ok' => true, 'row' => decode_json_cols($row, $jsonCols)]);
  }

  if ($action === 'create') {
    $body = read_json_body();
    $data = is_array($body['data'] ?? null) ? $body['data'] : [];
    $data = sanitize_columns($data, $cols);
    $data = encode_json_cols($data, $jsonCols);
    if (!$data) respond(['ok' => false, 'error' => 'Пустые данные: нечего сохранять'], 400);

    $newId = crud_insert($mysqli, $R, $data);
    // Запись могла получить значения по умолчанию (NULL, счётчики, триггеры):
    // в историю кладём фактическую строку, а не только то, что прислала форма.
    $created = crud_fetch_row($mysqli, $R, $newId);
    $after = db_history_row_values($R, $created);
    if (!$after) {
      $after = decode_json_cols($data, $jsonCols);
    }
    db_history_safe_record($R, $resource, 'create', $newId, [], $after);
    $mysqli->close();
    respond(['ok' => true, 'id' => $newId]);
  }

  if ($action === 'update') {
    $iid = crud_record_id((string)($_GET['id'] ?? ''), $resource);
    if ($iid === null) respond(['ok' => false, 'error' => 'Некорректный идентификатор записи'], 400);
    $body = read_json_body();
    $data = is_array($body['data'] ?? null) ? $body['data'] : [];
    $data = sanitize_columns($data, $cols);
    $data = encode_json_cols($data, $jsonCols);
    if (!$data) respond(['ok' => false, 'error' => 'Пустые данные: нечего сохранять'], 400);

    // Compare and write under the same row lock. A concurrent delete cannot
    // leave a successful no-op save or history entry for a nonexistent record.
    if (!$mysqli->begin_transaction()) throw new RuntimeException('Не удалось начать транзакцию');
    $transactionOpen = true;
    $current = crud_fetch_row($mysqli, $R, $iid, true);
    $plan = crud_update_plan($current, $data, $jsonCols);
    if ($plan === null) {
      $mysqli->rollback();
      $transactionOpen = false;
      respond(['ok' => false, 'error' => 'Запись уже удалена. Обновите список.', 'code' => 'record_not_found'], 404);
    }
    if (!$plan['data']) {
      if (!$mysqli->commit()) throw new RuntimeException('Не удалось завершить транзакцию');
      $transactionOpen = false;
      respond(['ok' => true, 'changed' => 0, 'unchanged' => true]);
    }
    $data = $plan['data'];
    crud_update($mysqli, $R, $iid, $data);
    if (!$mysqli->commit()) throw new RuntimeException('Не удалось завершить транзакцию');
    $transactionOpen = false;
    db_history_safe_record($R, $resource, 'update', $iid, $plan['before'], decode_json_cols($data, $jsonCols),
      ['label' => db_history_label($R, $plan['values'])]);
    $mysqli->close();
    respond(['ok' => true, 'changed' => count($data)]);
  }

  if ($action === 'delete') {
    $iid = crud_record_id((string)($_GET['id'] ?? ''), $resource);
    if ($iid === null) respond(['ok' => false, 'error' => 'Некорректный идентификатор записи'], 400);
    // Полная строка до удаления — единственный способ вернуть запись обратно.
    if (!$mysqli->begin_transaction()) throw new RuntimeException('Не удалось начать транзакцию');
    $transactionOpen = true;
    $removed = crud_fetch_row($mysqli, $R, $iid, true);
    $before = db_history_row_values($R, $removed);
    if (!$before) {
      $mysqli->rollback();
      $transactionOpen = false;
      respond(['ok' => false, 'error' => 'Запись уже удалена. Обновите список.', 'code' => 'record_not_found'], 404);
    }
    crud_delete($mysqli, $R, $iid);
    if (!$mysqli->commit()) throw new RuntimeException('Не удалось завершить транзакцию');
    $transactionOpen = false;
    db_history_safe_record($R, $resource, 'delete', $iid, $before, []);
    $mysqli->close();
    respond(['ok' => true]);
  }

  $mysqli->close();
  respond(['ok' => false, 'error' => 'Неизвестное действие'], 400);

} catch (Throwable $e) {
  if ($transactionOpen && isset($mysqli) && $mysqli instanceof mysqli) {
    try { $mysqli->rollback(); }
    catch (Throwable $rollbackError) { error_log('[editor-studio] rollback: ' . $rollbackError->getMessage()); }
  }
  editor_internal_error($e);
}
