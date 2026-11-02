<?php
// История изменений игровой БД.
// Endpoint: api/db-history.php?action=...
//
//   list   (GET)  — resource, op, source, actor_role, request_id, state, q, limit, offset
//   get    (GET)  — id: полная запись (before/after) для окна отмены
//   revert (POST) — { id, force? } — отменить изменение (JSON-POST только со своего узла)
//
// Доступ — только администратор. История содержит значения полей игровой базы
// (в том числе секретов: коды ваучеров, награды), поэтому гостю она не видна
// ни при каких выданных вкладках.
//
// Отмена выполняется по одной записи: «создание» удаляет добавленную строку,
// «изменение» возвращает прежние значения колонок, «удаление» восстанавливает
// строку с тем же ID. Сама отмена становится отдельной записью журнала, поэтому
// её тоже можно отменить.

declare(strict_types=1);

require_once __DIR__ . '/db-history-lib.php';

$ROLE = bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN]);
$action = (string)($_GET['action'] ?? '');
guard_write_actions($action, ['revert']);

/** Значение записи истории в виде, удобном интерфейсу. */
function history_view(array $row, bool $full): array
{
    $op = (string)$row['op'];
    $before = is_array($row['before']) ? $row['before'] : [];
    $after = is_array($row['after']) ? $row['after'] : [];
    $changed = $op === 'create' ? array_keys($after) : array_keys($before);
    if ($op === 'update') {
        $changed = array_values(array_unique(array_merge($changed, array_keys($after))));
    }

    $resources = editor_resources();
    $known = isset($resources[$row['resource']]);
    $view = [
        'id' => (int)$row['id'],
        'at' => (string)$row['at'],
        'actor_role' => (string)$row['actor_role'],
        'actor_id' => (string)$row['actor_id'],
        'actor_name' => (string)$row['actor_name'],
        'approved_by' => (string)$row['approved_by'],
        'source' => (string)$row['source'],
        'request_id' => (string)$row['request_id'],
        'resource' => (string)$row['resource'],
        'resource_title' => $known ? (string)$resources[$row['resource']]['title'] : (string)$row['resource'],
        'table_name' => (string)$row['table_name'],
        'pk_col' => (string)$row['pk_col'] !== '' ? (string)$row['pk_col'] : 'ID',
        'record_id' => (int)$row['record_id'],
        'op' => $op,
        'label' => (string)$row['label'],
        'changed' => $changed,
        'changes' => count($changed),
        'revert_of' => (int)$row['revert_of'],
        'reverted_at' => (string)$row['reverted_at'],
        'reverted_by' => (int)$row['reverted_by'],
        'revertable' => (bool)$row['revertable'],
        'can_revert' => (bool)$row['revertable'] && (string)$row['reverted_at'] === '' && in_array($op, DB_HISTORY_OPS, true),
        'known_resource' => $known,
    ];
    if ($full) {
        $view['before'] = $before;
        $view['after'] = $after;
    }
    return $view;
}

try {
    if ($action === '') {
        respond(['ok' => false, 'error' => 'Не указано действие'], 400);
    }

    if ($action === 'list') {
        $filters = [
            'q' => trim((string)($_GET['q'] ?? '')),
            'resource' => trim((string)($_GET['resource'] ?? '')),
            'op' => trim((string)($_GET['op'] ?? '')),
            'source' => trim((string)($_GET['source'] ?? '')),
            'actor_role' => trim((string)($_GET['actor_role'] ?? '')),
            'request_id' => trim((string)($_GET['request_id'] ?? '')),
            'state' => (string)($_GET['state'] ?? 'all'),
        ];
        $limit = clamp_int($_GET['limit'] ?? null, 1, 300, 60);
        $offset = clamp_int($_GET['offset'] ?? null, 0, 1000000, 0);
        $list = store_history_list($filters, $limit, $offset);
        respond([
            'ok' => true,
            'rows' => array_map(static fn(array $row): array => history_view($row, false), $list['rows']),
            'total' => $list['total'],
            'limit' => $limit,
            'offset' => $offset,
            'resources' => db_history_resources(),
            'counts' => store_history_counts(),
        ]);
    }

    if ($action === 'get') {
        $id = (int)($_GET['id'] ?? 0);
        $entry = $id > 0 ? store_history_get($id) : null;
        if ($entry === null) {
            respond(['ok' => false, 'error' => 'Запись истории не найдена', 'code' => 'history_not_found'], 404);
        }
        respond(['ok' => true, 'entry' => history_view($entry, true)]);
    }

    if ($action === 'revert') {
        $body = read_json_body();
        $id = (int)($body['id'] ?? 0);
        $force = ($body['force'] ?? false) === true;
        $entry = $id > 0 ? store_history_get($id) : null;
        if ($entry === null) {
            respond(['ok' => false, 'error' => 'Запись истории не найдена', 'code' => 'history_not_found'], 404);
        }
        $R = db_history_resource((string)$entry['resource']);
        if ($R === null) {
            respond([
                'ok' => false,
                'error' => 'Раздел больше не редактируется в студии — отменить это изменение автоматически нельзя.',
                'code' => 'history_resource_unknown',
            ], 409);
        }
        $recordId = (int)$entry['record_id'];
        $db = db_connect();
        $plan = null;
        $newBefore = [];
        $newAfter = [];
        try {
            $db->begin_transaction();
            $currentRow = crud_fetch_row($db, $R, $recordId, true);
            $plan = db_history_plan_revert($R, $entry, $currentRow, $force);
            if (is_string($plan['error'])) {
                $db->rollback();
                $db->close();
                respond([
                    'ok' => false,
                    'error' => $plan['error'],
                    'code' => $plan['code'],
                    'conflicts' => (object)($plan['conflicts'] ?? []),
                ], 409);
            }

            if ($plan['action'] === 'delete') {
                $newBefore = db_history_row_values($R, $currentRow);
                crud_delete($db, $R, $recordId);
            } elseif ($plan['action'] === 'update') {
                foreach (array_keys($plan['data']) as $column) {
                    $newBefore[$column] = $plan['current'][$column] ?? null;
                }
                $newAfter = $plan['data'];
                crud_update($db, $R, $recordId, encode_json_cols($plan['data'], $R['json'] ?? []));
            } elseif ($plan['action'] === 'insert') {
                db_history_insert_with_id($db, $R, $recordId, $plan['data']);
                $restored = crud_fetch_row($db, $R, $recordId);
                $newAfter = db_history_row_values($R, $restored);
                if (!$newAfter) {
                    $newAfter = $plan['data'];
                }
            }
            $db->commit();
        } catch (Throwable $e) {
            try { $db->rollback(); } catch (Throwable) {}
            $db->close();
            throw $e;
        }
        $db->close();

        $inverse = ['create' => 'delete', 'delete' => 'create', 'update' => 'update'][$entry['op']] ?? 'update';
        $newEntryId = 0;
        $unchanged = $plan['action'] === 'noop';
        try {
            store_tx(function () use (&$newEntryId, $R, $entry, $inverse, $recordId, $newBefore, $newAfter): void {
                if ($inverse !== '' && ($newBefore || $newAfter)) {
                    $newEntryId = db_history_safe_record($R, (string)$entry['resource'], $inverse, $recordId, $newBefore, $newAfter, [
                        'source' => 'revert',
                        'revert_of' => (int)$entry['id'],
                        'request_id' => (string)$entry['request_id'],
                        'label' => (string)$entry['label'],
                    ]);
                }
                store_history_mark_reverted((int)$entry['id'], $newEntryId);
                // Отмена отмены: изменение, которое откатывала эта запись,
                // снова в силе — снимаем с него отметку.
                if ((int)$entry['revert_of'] > 0) {
                    store_history_unmark_reverted((int)$entry['revert_of']);
                }
            });
        } catch (Throwable $e) {
            // Изменение в базе уже применено: журнал не должен «врать» молча,
            // поэтому ошибку пишем в лог сервера, а ответ остаётся успешным.
            editor_log_error($e);
        }

        editor_audit('history.revert', (string)$entry['request_id'], [
            'id' => (int)$entry['id'],
            'resource' => (string)$entry['resource'],
            'record_id' => $recordId,
            'op' => (string)$entry['op'],
            'action' => $plan['action'],
            'force' => $force,
        ]);

        $original = store_history_get((int)$entry['id']);
        $created = $newEntryId > 0 ? store_history_get($newEntryId) : null;
        respond([
            'ok' => true,
            'unchanged' => $unchanged,
            'action' => $plan['action'],
            'entry' => $original ? history_view($original, true) : null,
            'revert_entry' => $created ? history_view($created, true) : null,
            'conflicts' => (object)($plan['conflicts'] ?? []),
        ]);
    }

    respond(['ok' => false, 'error' => 'Неизвестное действие'], 400);
} catch (Throwable $e) {
    editor_internal_error($e);
}
