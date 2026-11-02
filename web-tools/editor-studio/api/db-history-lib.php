<?php
// История изменений игровой БД: запись изменений и отмена (откат) по записи.
//
// Одна запись истории — одно изменение одного элемента игровой базы:
//   create — запись добавлена (after = значения новой строки);
//   update — изменены перечисленные колонки (before / after — только они);
//   delete — запись удалена (before = полная строка, чтобы её можно было вернуть).
//
// Отмена не «переписывает журнал»: она сама становится изменением (source =
// revert) и помечает исходную запись как отменённую. Поэтому отмену отмены
// можно сделать той же кнопкой — история остаётся линейной и проверяемой.
//
// Запись истории никогда не должна ломать саму операцию с базой: все ошибки
// хранилища только логируются (db_history_safe_record).

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';
require_once __DIR__ . '/db-resources.php';

/** Верхняя граница полезной нагрузки одной записи (before + after, JSON). */
const DB_HISTORY_MAX_PAYLOAD = 1048576;

/** Типы изменений, которые попадают в историю. */
const DB_HISTORY_OPS = ['create', 'update', 'delete'];

/** Сколько символов подписи записи храним. */
const DB_HISTORY_LABEL_MAX = 120;

function db_history_now(): string
{
    return gmdate('Y-m-d\TH:i:s\Z');
}

/**
 * Кто делает изменение — из сессии редактора.
 * @return array{role:string, id:string, name:string}
 */
function db_history_actor(): array
{
    if (session_status() === PHP_SESSION_ACTIVE && !empty($_SESSION['editor_studio_auth'])) {
        $id = (string)($_SESSION['editor_studio_user'] ?? 'admin');
        return ['role' => 'admin', 'id' => $id, 'name' => $id];
    }
    if (session_status() === PHP_SESSION_ACTIVE && !empty($_SESSION['editor_studio_guest'])) {
        $account = editor_guest_user();
        return [
            'role' => 'guest',
            'id' => (string)($account['id'] ?? ($_SESSION['editor_studio_guest_user'] ?? '')),
            'name' => (string)($account['name'] ?? $account['login'] ?? 'guest'),
        ];
    }
    return ['role' => 'anonymous', 'id' => '', 'name' => ''];
}

/** Короткая подпись записи: значение колонки-названия или пустая строка. */
function db_history_label(array $R, ?array $row): string
{
    $column = $R['label'] ?? null;
    if (!is_string($column) || $column === '' || !is_array($row)) {
        return '';
    }
    $value = $row[$column] ?? null;
    if (!is_scalar($value)) {
        return '';
    }
    $text = trim((string)$value);
    if ($text === '') {
        return '';
    }
    // Подпись — одна строка: переводы строк в названиях ломают список.
    $text = (string)preg_replace('/\s+/u', ' ', $text);
    if (function_exists('mb_substr')) {
        return mb_substr($text, 0, DB_HISTORY_LABEL_MAX, 'UTF-8');
    }
    return substr($text, 0, DB_HISTORY_LABEL_MAX);
}

/** Значения строки в том виде, в котором они попадают в журнал (JSON-колонки — декодированы). */
function db_history_row_values(array $R, ?array $row): array
{
    if (!is_array($row)) {
        return [];
    }
    return decode_json_cols($row, $R['json'] ?? []);
}

/**
 * Собирает запись истории: проверяет размер полезной нагрузки и решает,
 * можно ли изменение отменить.
 *
 * @param array<string, mixed> $before
 * @param array<string, mixed> $after
 * @param array<string, mixed> $meta source, request_id, approved_by, label, revert_of, ip, actor_*
 * @return array<string, mixed>
 */
function db_history_entry(array $R, string $resource, string $op, int $recordId, array $before, array $after, array $meta = []): array
{
    $pk = (string)($R['pk'] ?? 'ID');
    // Первичный ключ в полезной нагрузке не дублируем: он есть в record_id.
    unset($before[$pk], $after[$pk]);

    $label = (string)($meta['label'] ?? '');
    if ($label === '') {
        $label = db_history_label($R, array_replace($before, $after));
    }

    $revertable = in_array($op, DB_HISTORY_OPS, true) && (bool)($meta['revertable'] ?? true);
    $payload = json_encode(
        ['before' => $before, 'after' => $after],
        JSON_UNESCAPED_UNICODE | JSON_INVALID_UTF8_SUBSTITUTE | JSON_PARTIAL_OUTPUT_ON_ERROR
    );
    if (!is_string($payload) || strlen($payload) > DB_HISTORY_MAX_PAYLOAD) {
        // Слишком большая запись (огромный JSON в строке): изменение останется
        // в журнале, но отменить его автоматически нельзя — данных не хватит.
        $before = [];
        $after = [];
        $revertable = false;
    }
    if ($op === 'delete' && !$before) {
        $revertable = false;
    }

    $actor = db_history_actor();
    return [
        'at' => (string)($meta['at'] ?? db_history_now()),
        'actor_role' => (string)($meta['actor_role'] ?? $actor['role']),
        'actor_id' => (string)($meta['actor_id'] ?? $actor['id']),
        'actor_name' => (string)($meta['actor_name'] ?? $actor['name']),
        'approved_by' => (string)($meta['approved_by'] ?? ''),
        'source' => (string)($meta['source'] ?? 'editor'),
        'request_id' => (string)($meta['request_id'] ?? ''),
        'resource' => $resource,
        'table_name' => (string)($R['table'] ?? ''),
        'pk_col' => $pk,
        'record_id' => $recordId,
        'op' => $op,
        'label' => $label,
        'before' => $before,
        'after' => $after,
        'revert_of' => (int)($meta['revert_of'] ?? 0),
        'revertable' => $revertable,
        'ip' => (string)($meta['ip'] ?? ''),
    ];
}

/**
 * Записывает изменение в историю. Возвращает id записи (0, если хранилище
 * недоступно — сама операция при этом не отменяется).
 *
 * @param array<string, mixed> $before
 * @param array<string, mixed> $after
 * @param array<string, mixed> $meta
 */
function db_history_safe_record(array $R, string $resource, string $op, int $recordId, array $before, array $after, array $meta = []): int
{
    try {
        if (($meta['ip'] ?? '') === '') {
            $meta['ip'] = get_request_ip();
        }
        return store_history_add(db_history_entry($R, $resource, $op, $recordId, $before, $after, $meta));
    } catch (Throwable $e) {
        editor_log_error($e);
        return 0;
    }
}

/**
 * Новый ресурс по ключу записи истории. null — раздел больше не редактируется
 * (переименован/удалён из карты): такую запись отменить нельзя.
 */
function db_history_resource(string $key): ?array
{
    $resources = editor_resources();
    if (!isset($resources[$key])) {
        return null;
    }
    return $resources[$key];
}

/**
 * План отмены — чистая функция: по записи истории и текущей строке БД решает,
 * что именно сделать. Выполнение запросов остаётся вызывающему коду, поэтому
 * правила отмены проверяются тестами без базы данных.
 *
 * @param array<string, mixed> $entry запись истории (before/after декодированы)
 * @param array<string, mixed>|null $currentRow текущая строка из БД (как из crud_fetch_row)
 * @return array{
 *   action: string, error: ?string, code: ?string, data: array<string, mixed>,
 *   conflicts: array<string, array{current:mixed, recorded:mixed, before:mixed}>,
 *   current: array<string, mixed>, before: array<string, mixed>, after: array<string, mixed>
 * }
 */
function db_history_plan_revert(array $R, array $entry, ?array $currentRow, bool $force = false): array
{
    $jsonCols = $R['json'] ?? [];
    $columns = $R['columns'];
    $op = (string)($entry['op'] ?? '');
    $current = $currentRow ? db_history_row_values($R, $currentRow) : [];
    $before = is_array($entry['before'] ?? null) ? $entry['before'] : [];
    $after = is_array($entry['after'] ?? null) ? $entry['after'] : [];

    $plan = [
        'action' => 'none', 'error' => null, 'code' => null, 'data' => [],
        'conflicts' => [], 'current' => $current, 'before' => $before, 'after' => $after,
    ];

    $fail = static function (string $code, string $message) use ($plan): array {
        $plan['error'] = $message;
        $plan['code'] = $code;
        return $plan;
    };

    if (($entry['revertable'] ?? true) === false) {
        return $fail('history_not_revertable', 'Эту запись нельзя отменить: в журнале нет полных данных об изменении.');
    }
    if ((string)($entry['reverted_at'] ?? '') !== '') {
        return $fail('history_already_reverted', 'Это изменение уже отменено.');
    }
    if (!in_array($op, DB_HISTORY_OPS, true)) {
        return $fail('history_bad_op', 'Неизвестный тип изменения — отмена невозможна.');
    }

    if ($op === 'create') {
        if (!$current) {
            return $fail('history_record_missing', 'Запись уже удалена: отменять нечего.');
        }
        $plan['action'] = 'delete';
        return $plan;
    }

    if ($op === 'update') {
        if (!$current) {
            return $fail('history_record_missing', 'Запись больше не существует — восстановить значения нельзя.');
        }
        $restore = [];
        $conflicts = [];
        foreach ($before as $column => $value) {
            if (!in_array($column, $columns, true)) {
                continue; // колонку убрали из редактора — молча пропускаем
            }
            $isJson = in_array($column, $jsonCols, true);
            $currentValue = $current[$column] ?? null;
            if (editor_values_equal($currentValue, $value, $isJson)) {
                continue; // значение уже такое, как до изменения
            }
            $recorded = $after[$column] ?? null;
            if (!editor_values_equal($currentValue, $recorded, $isJson)) {
                $conflicts[$column] = ['current' => $currentValue, 'recorded' => $recorded, 'before' => $value];
                if (!$force) {
                    continue;
                }
            }
            $restore[$column] = $value;
        }
        if ($conflicts && !$force) {
            $plan['error'] = 'Запись изменили после этого изменения. Проверьте конфликты и повторите с принудительной отменой, если уверены.';
            $plan['code'] = 'history_conflict';
            $plan['conflicts'] = $conflicts;
            return $plan;
        }
        $plan['conflicts'] = $conflicts;
        if (!$restore) {
            $plan['action'] = 'noop';
            return $plan;
        }
        $plan['action'] = 'update';
        $plan['data'] = $restore;
        return $plan;
    }

    // delete → вернуть удалённую строку с прежним ID
    if ($current) {
        return $fail('history_id_taken', 'Запись с этим ID уже существует — восстановление перезапишет чужие данные и потому запрещено.');
    }
    $row = [];
    foreach ($columns as $column) {
        if (array_key_exists($column, $before)) {
            $row[$column] = $before[$column];
        }
    }
    if (!$row) {
        return $fail('history_payload_incomplete', 'В журнале нет значений удалённой записи — восстановить её нельзя.');
    }
    $plan['action'] = 'insert';
    $plan['data'] = $row;
    return $plan;
}

/**
 * Возвращает удалённую запись с прежним ID (crud_insert не пишет первичный ключ).
 *
 * @param array<string, mixed> $row декодированные значения
 */
function db_history_insert_with_id(mysqli $db, array $R, int $id, array $row): void
{
    $pk = (string)$R['pk'];
    $data = [];
    foreach ($R['columns'] as $column) {
        if (array_key_exists($column, $row)) {
            $data[$column] = $row[$column];
        }
    }
    $data = encode_json_cols($data, $R['json'] ?? []);
    if (!$data) {
        throw new EditorUserError('В журнале нет значений удалённой записи — восстановить её нельзя.');
    }
    $columns = array_merge([$pk], array_keys($data));
    $placeholders = implode(',', array_fill(0, count($columns), '?'));
    $colSql = implode(',', array_map(static fn(string $c): string => '`' . $c . '`', $columns));
    $stmt = $db->prepare('INSERT INTO `' . $R['table'] . "` ($colSql) VALUES ($placeholders)");
    if ($stmt === false) {
        throw new RuntimeException('Ошибка подготовки запроса к базе: ' . $db->error);
    }
    [$types, $values] = crud_bind_values(array_merge([$id], $data));
    $stmt->bind_param($types, ...$values);
    if (!$stmt->execute()) {
        $error = $stmt->error;
        $stmt->close();
        throw new EditorUserError('Не удалось вернуть удалённую запись: ' . $error);
    }
    $stmt->close();
}

/**
 * Список разделов для фильтра: стабильный порядок по подписи.
 * @return list<array{key:string, title:string}>
 */
function db_history_resources(): array
{
    $out = [];
    foreach (editor_resources() as $key => $R) {
        $out[] = ['key' => (string)$key, 'title' => (string)($R['title'] ?? $key)];
    }
    usort($out, static fn(array $a, array $b): int => strcmp($a['title'], $b['title']));
    return $out;
}
