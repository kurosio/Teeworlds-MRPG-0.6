<?php
// Change requests (guest → admin) — storage, validation, analysis, apply.
//
// Security model:
//  * Requests and their message threads are stored in the SQLite store
//    (data/editor-studio.sqlite), NOT in the game database. A guest has no code
//    path that writes to the game DB.
//  * The game DB is only READ while a request is analysed (prepared statements,
//    whitelisted tables/columns from db-resources.php).
//  * Writing happens exclusively in cr_apply(), which is reachable only by an
//    administrator (api/change-requests.php → action=approve).

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';
require_once __DIR__ . '/db-resources.php';
require_once __DIR__ . '/reference-data.php';

const CR_NICK_MIN = 2;
const CR_NICK_MAX = 32;
const CR_MESSAGE_MIN = 10;
const CR_MESSAGE_MAX = 2000;
const CR_COMMENT_MAX = 2000;
const CR_MAX_BODY_BYTES = 2 * 1024 * 1024;
const CR_MAX_VALUE_BYTES = 256 * 1024;
const CR_GLOBAL_PENDING_MAX = 300;
const CR_STATUSES = ['pending', 'approved', 'rejected', 'withdrawn'];

final class CrInputError extends RuntimeException
{
    public function __construct(string $message, public readonly int $httpCode = 400, public readonly array $extra = [])
    {
        parent::__construct($message);
    }
}

// ── Text helpers ─────────────────────────────────────────────────────────────

function cr_strlen(string $s): int
{
    return function_exists('mb_strlen') ? mb_strlen($s, 'UTF-8') : (int)preg_match_all('/./us', $s);
}

function cr_substr(string $s, int $len): string
{
    if (cr_strlen($s) <= $len) return $s;
    if (function_exists('mb_substr')) return mb_substr($s, 0, $len, 'UTF-8') . '…';
    preg_match('/^.{0,' . $len . '}/us', $s, $m);
    return ($m[0] ?? '') . '…';
}

/** Valid UTF-8, no control characters (except \n and \t when multiline), trimmed. */
function cr_clean_text(mixed $raw, bool $multiline): string
{
    $s = is_scalar($raw) ? (string)$raw : '';
    if (preg_match('//u', $s) !== 1) {
        throw new CrInputError('Текст содержит некорректные символы (ожидается UTF-8).');
    }
    $s = str_replace(["\r\n", "\r"], "\n", $s);
    $s = $multiline
        ? preg_replace('/[\x00-\x08\x0B-\x1F\x7F\x{200B}-\x{200F}\x{202A}-\x{202E}\x{2066}-\x{2069}]/u', '', $s)
        : preg_replace('/[\x00-\x1F\x7F\x{200B}-\x{200F}\x{202A}-\x{202E}\x{2066}-\x{2069}]/u', '', $s);
    $s = (string)$s;
    if ($multiline) {
        $s = (string)preg_replace("/\n{4,}/", "\n\n\n", $s);
    } else {
        $s = (string)preg_replace('/\s+/u', ' ', $s);
    }
    return trim($s);
}

function cr_validate_nickname(mixed $raw): string
{
    $nick = cr_clean_text($raw, false);
    $len = cr_strlen($nick);
    if ($len < CR_NICK_MIN || $len > CR_NICK_MAX) {
        throw new CrInputError(sprintf('Укажите Nickname (от %d до %d символов).', CR_NICK_MIN, CR_NICK_MAX));
    }
    return $nick;
}

function cr_validate_message(mixed $raw): string
{
    $msg = cr_clean_text($raw, true);
    $len = cr_strlen($msg);
    if ($len < CR_MESSAGE_MIN || $len > CR_MESSAGE_MAX) {
        throw new CrInputError(sprintf('Опишите, зачем нужны изменения (от %d до %d символов).', CR_MESSAGE_MIN, CR_MESSAGE_MAX));
    }
    return $msg;
}

function cr_now(): string
{
    return gmdate('Y-m-d\TH:i:s\Z');
}

function cr_guest_hash(string $guestKey): string
{
    return hash('sha256', 'guest|' . $guestKey);
}

/**
 * Does the current guest own the request?
 * Requests store the account id, so the identity survives logout/login, a
 * password change and a new session. `guestHash` (a random per-session key) is
 * only the fallback for requests created before personal guest accounts existed.
 */
function cr_guest_owns(array $req, ?array $account = null): bool
{
    $author = is_array($req['author'] ?? null) ? $req['author'] : [];
    $account ??= editor_guest_user();
    $guestId = (string)($account['id'] ?? '');
    if ($guestId !== '') {
        if (!isset($author['guestId']) || (string)$author['guestId'] === '') {
            // Legacy request: fall through to the session key check below.
        } elseif ((string)$author['guestId'] === $guestId) {
            return true;
        } else {
            return false; // Another account — the hash must not matter.
        }
    }
    $key = editor_guest_key();
    $stored = (string)($author['guestHash'] ?? '');
    return $key !== '' && $stored !== '' && hash_equals($stored, cr_guest_hash($key));
}

// ── Storage (SQLite, отдельно от игровой БД) ────────────────────────────────
//
// Запросы, переписка по ним и счётчики частоты живут в data/editor-studio.sqlite.
// Документ запроса (operations/analysis/review) хранится как JSON в payload,
// а статус, автор и время вынесены в колонки — по ним идут выборки.

function cr_valid_id(string $id): bool
{
    return preg_match('/^CR-\d{8}-\d{6}-[a-f0-9]{6}$/', $id) === 1;
}

function cr_new_id(): string
{
    return 'CR-' . gmdate('Ymd-His') . '-' . bin2hex(random_bytes(3));
}

function cr_assert_id(string $id): string
{
    if (!cr_valid_id($id)) {
        throw new CrInputError('Некорректный ID запроса.');
    }
    return $id;
}

/** Все изменения запросов сериализуются транзакцией SQLite. */
function cr_with_lock(callable $fn): mixed
{
    return store_tx($fn);
}

function cr_load(string $id): ?array
{
    cr_assert_id($id);
    $stmt = store_db()->prepare('SELECT payload FROM change_requests WHERE id = ?');
    $stmt->execute([$id]);
    $raw = $stmt->fetchColumn();
    if ($raw === false) {
        return null;
    }
    $data = json_decode((string)$raw, true);
    return is_array($data) ? $data : null;
}

function cr_save(array $req): void
{
    $id = cr_assert_id((string)($req['id'] ?? ''));
    $author = is_array($req['author'] ?? null) ? $req['author'] : [];
    $stmt = store_db()->prepare(
        'INSERT INTO change_requests(id, status, guest_id, guest_login, nickname, message, created_at, updated_at, payload)
         VALUES(:id, :status, :guest_id, :guest_login, :nickname, :message, :created_at, :updated_at, :payload)
         ON CONFLICT(id) DO UPDATE SET
            status = excluded.status,
            guest_id = excluded.guest_id,
            guest_login = excluded.guest_login,
            nickname = excluded.nickname,
            message = excluded.message,
            updated_at = excluded.updated_at,
            payload = excluded.payload'
    );
    $stmt->execute([
        ':id' => $id,
        ':status' => (string)($req['status'] ?? 'pending'),
        ':guest_id' => (string)($author['guestId'] ?? ''),
        ':guest_login' => (string)($author['guestLogin'] ?? ''),
        ':nickname' => (string)($author['nickname'] ?? ''),
        ':message' => (string)($req['message'] ?? ''),
        ':created_at' => (string)($req['createdAt'] ?? cr_now()),
        ':updated_at' => (string)($req['updatedAt'] ?? cr_now()),
        ':payload' => json_encode($req, JSON_UNESCAPED_UNICODE | JSON_THROW_ON_ERROR),
    ]);
}

function cr_delete(string $id): void
{
    cr_assert_id($id);
    $stmt = store_db()->prepare('DELETE FROM change_requests WHERE id = ?');
    $stmt->execute([$id]);
}

/**
 * @return list<array> newest first
 * @deprecated Полная загрузка всех запросов в память. Оставлена для
 * совместимости; новый код использует cr_query_list()/cr_counts_by_status().
 */
function cr_all(): array
{
    $out = [];
    foreach (store_db()->query('SELECT payload FROM change_requests ORDER BY created_at DESC, id DESC') as $row) {
        $data = json_decode((string)$row['payload'], true);
        if (is_array($data)) {
            $out[] = $data;
        }
    }
    return $out;
}

/** Экранирование пользовательского текста для LIKE … ESCAPE '\'. */
function cr_like_escape(string $value): string
{
    return str_replace(['\\', '%', '_'], ['\\\\', '\\%', '\\_'], $value);
}

/**
 * Постраничная выборка запросов из SQLite (индексы по статусу и гостю),
 * вместо загрузки всех документов в память.
 *
 * @param array{status?:string, guest_id?:string, q?:string, resource?:string, pending_first?:bool} $filters
 * @return array{rows:list<array>, total:int} rows — полные документы запросов
 */
function cr_query_list(array $filters, int $limit, int $offset): array
{
    $where = [];
    $params = [];

    $status = (string)($filters['status'] ?? 'all');
    if ($status !== 'all' && $status !== '') {
        if (!in_array($status, CR_STATUSES, true)) {
            throw new CrInputError('Неизвестный статус запроса.');
        }
        $where[] = 'status = ?';
        $params[] = $status;
    }
    $guestId = trim((string)($filters['guest_id'] ?? ''));
    if ($guestId !== '') {
        $where[] = 'guest_id = ?';
        $params[] = $guestId;
    }
    $q = trim((string)($filters['q'] ?? ''));
    if ($q !== '') {
        $like = '%' . cr_like_escape($q) . '%';
        $where[] = "(id LIKE ? ESCAPE '\\' OR nickname LIKE ? ESCAPE '\\' OR guest_login LIKE ? ESCAPE '\\'"
            . " OR message LIKE ? ESCAPE '\\' OR payload LIKE ? ESCAPE '\\')";
        array_push($params, $like, $like, $like, $like, $like);
    }
    $resource = trim((string)($filters['resource'] ?? ''));
    if ($resource !== '') {
        // Раздел хранится только внутри документа операции («"resource":"items"»);
        // формат записи контролирует cr_save (json_encode без пробелов).
        if (!isset(editor_resources()[$resource])) {
            throw new CrInputError('Неизвестный раздел: ' . $resource);
        }
        $where[] = "payload LIKE ? ESCAPE '\\'";
        $params[] = '%"resource":"' . cr_like_escape($resource) . '"%';
    }

    $sqlWhere = $where ? (' WHERE ' . implode(' AND ', $where)) : '';
    $order = !empty($filters['pending_first'])
        ? " ORDER BY (status = 'pending') DESC, created_at DESC, id DESC"
        : ' ORDER BY created_at DESC, id DESC';
    $pdo = store_db();

    $stmt = $pdo->prepare('SELECT COUNT(*) FROM change_requests' . $sqlWhere);
    $stmt->execute($params);
    $total = (int)$stmt->fetchColumn();

    $stmt = $pdo->prepare('SELECT payload FROM change_requests' . $sqlWhere . $order . ' LIMIT ? OFFSET ?');
    $i = 1;
    foreach ($params as $param) {
        $stmt->bindValue($i++, $param);
    }
    $stmt->bindValue($i++, max(1, $limit), PDO::PARAM_INT);
    $stmt->bindValue($i, max(0, $offset), PDO::PARAM_INT);
    $stmt->execute();

    $rows = [];
    foreach ($stmt->fetchAll() as $row) {
        $data = json_decode((string)$row['payload'], true);
        if (is_array($data)) {
            $rows[] = $data;
        }
    }
    return ['rows' => $rows, 'total' => $total];
}

/** @return array<string,int> количество запросов по статусам */
function cr_counts_by_status(): array
{
    $counts = array_fill_keys(CR_STATUSES, 0);
    foreach (store_db()->query('SELECT status, COUNT(*) AS c FROM change_requests GROUP BY status') as $row) {
        $status = (string)$row['status'];
        if (isset($counts[$status])) {
            $counts[$status] = (int)$row['c'];
        }
    }
    return $counts;
}

/** Число запросов на рассмотрении: всех или одного гостя. */
function cr_pending_count(string $guestId = ''): int
{
    if ($guestId === '') {
        $stmt = store_db()->prepare("SELECT COUNT(*) FROM change_requests WHERE status = 'pending'");
        $stmt->execute();
    } else {
        $stmt = store_db()->prepare("SELECT COUNT(*) FROM change_requests WHERE status = 'pending' AND guest_id = ?");
        $stmt->execute([$guestId]);
    }
    return (int)$stmt->fetchColumn();
}

/** @return array<string,int> guest_id => число запросов на рассмотрении */
function cr_pending_by_guest(): array
{
    $out = [];
    $stmt = store_db()->query(
        "SELECT guest_id, COUNT(*) AS c FROM change_requests WHERE status = 'pending' AND guest_id <> '' GROUP BY guest_id"
    );
    foreach ($stmt as $row) {
        $out[(string)$row['guest_id']] = (int)$row['c'];
    }
    return $out;
}

/** Simple per-IP cooldown stored in SQLite. Returns seconds left (0 = allowed). */
function cr_ip_cooldown_left(string $ip, int $cooldown, bool $touch): int
{
    if ($cooldown <= 0 || $ip === '') return 0;
    $now = time();
    $key = 'cr-ip|' . hash('sha256', $ip);
    store_rate_prune($now - 86400);
    $last = store_rate_get($key);
    $left = $last > 0 ? max(0, $last + $cooldown - $now) : 0;
    if ($touch && $left === 0) {
        store_rate_touch($key, $now);
    }
    return $left;
}

// ── Переписка по запросу (гость ↔ администратор) ─────────────────────────────
//
// Каждое сообщение знает, кто его прочитал: администратор и гость ставят
// отметки независимо, поэтому в интерфейсе видно «есть ответ, который вы
// ещё не открывали».

const CR_MSG_MIN = 1;
const CR_MSG_MAX = 2000;

function cr_msg_validate(mixed $raw): string
{
    $text = cr_clean_text($raw, true);
    $len = cr_strlen($text);
    if ($len < CR_MSG_MIN) {
        throw new CrInputError('Сообщение пустое.');
    }
    if ($len > CR_MSG_MAX) {
        throw new CrInputError(sprintf('Сообщение слишком длинное (максимум %d символов).', CR_MSG_MAX));
    }
    return $text;
}

function cr_msg_row(array $row, string $viewerRole): array
{
    $isAdminMessage = (string)$row['author_role'] === 'admin';
    $readAt = $isAdminMessage ? (string)$row['read_by_guest_at'] : (string)$row['read_by_admin_at'];
    return [
        'id' => (int)$row['id'],
        'requestId' => (string)$row['request_id'],
        'role' => (string)$row['author_role'],
        'author' => (string)$row['author_name'],
        'body' => (string)$row['body'],
        'createdAt' => (string)$row['created_at'],
        // Сообщения «своей» стороны прочитаны по определению.
        'mine' => $viewerRole === (string)$row['author_role'],
        'read' => $viewerRole === (string)$row['author_role'] ? true : $readAt !== '',
    ];
}

/** @return list<array> */
function cr_msg_list(string $requestId, string $viewerRole): array
{
    cr_assert_id($requestId);
    $stmt = store_db()->prepare('SELECT * FROM request_messages WHERE request_id = ? ORDER BY id ASC');
    $stmt->execute([$requestId]);
    $out = [];
    foreach ($stmt->fetchAll() as $row) {
        $out[] = cr_msg_row($row, $viewerRole);
    }
    return $out;
}

function cr_msg_add(string $requestId, string $role, string $authorId, string $authorName, string $body): array
{
    cr_assert_id($requestId);
    $role = $role === 'admin' ? 'admin' : 'guest';
    $now = cr_now();
    $stmt = store_db()->prepare(
        'INSERT INTO request_messages(request_id, author_role, author_id, author_name, body, created_at, read_by_admin_at, read_by_guest_at)
         VALUES(?, ?, ?, ?, ?, ?, ?, ?)'
    );
    // Своё сообщение сразу считается прочитанным своей стороной.
    $stmt->execute([
        $requestId,
        $role,
        $authorId,
        $authorName,
        $body,
        $now,
        $role === 'admin' ? $now : '',
        $role === 'guest' ? $now : '',
    ]);
    return [
        'id' => (int)store_db()->lastInsertId(),
        'requestId' => $requestId,
        'role' => $role,
        'author' => $authorName,
        'body' => $body,
        'createdAt' => $now,
        'mine' => true,
        'read' => true,
    ];
}

/** Отметить чужие сообщения запроса прочитанными (свои не трогаем). */
function cr_msg_mark_read(string $requestId, string $role): void
{
    cr_assert_id($requestId);
    $column = $role === 'admin' ? 'read_by_admin_at' : 'read_by_guest_at';
    $authorRole = $role === 'admin' ? 'guest' : 'admin';
    $stmt = store_db()->prepare(
        "UPDATE request_messages SET $column = ? WHERE request_id = ? AND author_role = ? AND $column = ''"
    );
    $stmt->execute([cr_now(), $requestId, $authorRole]);
}

/** Сколько сообщений запроса ещё не прочитано указанной стороной. */
function cr_msg_unread_for(string $requestId, string $role): int
{
    $column = $role === 'admin' ? 'read_by_admin_at' : 'read_by_guest_at';
    $authorRole = $role === 'admin' ? 'guest' : 'admin';
    $stmt = store_db()->prepare(
        "SELECT COUNT(*) FROM request_messages WHERE request_id = ? AND author_role = ? AND $column = ''"
    );
    $stmt->execute([$requestId, $authorRole]);
    return (int)$stmt->fetchColumn();
}

/**
 * Непрочитанное по всем запросам.
 * @param list<string>|null $guestIds null — все запросы (администратор)
 * @return array{total:int, by_request:array<string,int>, by_guest:array<string,int>}
 */
function cr_msg_unread_map(string $role, ?array $guestIds = null): array
{
    $column = $role === 'admin' ? 'read_by_admin_at' : 'read_by_guest_at';
    $authorRole = $role === 'admin' ? 'guest' : 'admin';
    $sql = "SELECT m.request_id AS rid, r.guest_id AS gid, COUNT(*) AS c
              FROM request_messages m
              JOIN change_requests r ON r.id = m.request_id
             WHERE m.author_role = ? AND m.$column = ''";
    $params = [$authorRole];
    if ($guestIds !== null) {
        if (!$guestIds) {
            return ['total' => 0, 'by_request' => [], 'by_guest' => []];
        }
        $sql .= ' AND r.guest_id IN (' . implode(',', array_fill(0, count($guestIds), '?')) . ')';
        array_push($params, ...$guestIds);
    }
    $sql .= ' GROUP BY m.request_id, r.guest_id';
    $stmt = store_db()->prepare($sql);
    $stmt->execute($params);
    $out = ['total' => 0, 'by_request' => [], 'by_guest' => []];
    foreach ($stmt->fetchAll() as $row) {
        $count = (int)$row['c'];
        $out['total'] += $count;
        $out['by_request'][(string)$row['rid']] = $count;
        $gid = (string)$row['gid'];
        if ($gid !== '') {
            $out['by_guest'][$gid] = ($out['by_guest'][$gid] ?? 0) + $count;
        }
    }
    return $out;
}

// ── Operation validation ─────────────────────────────────────────────────────

function cr_resource(string $key, bool $guestOnly = true): array
{
    $resources = editor_resources();
    if (!isset($resources[$key])) {
        throw new CrInputError('Неизвестный раздел: ' . $key);
    }
    if ($guestOnly && empty($resources[$key]['guest'])) {
        throw new CrInputError('Раздел «' . ($resources[$key]['title'] ?? $key) . '» недоступен для запросов на изменение.');
    }
    return $resources[$key] + ['key' => $key];
}

/**
 * A guest may only propose changes for the editor tabs granted to its account.
 * Enforced for both preview and submit (the operation list is untrusted input).
 */
function cr_assert_guest_resources(array $ops): void
{
    $allowed = editor_guest_write_resources_current();
    $resources = editor_resources();
    foreach ($ops as $op) {
        $key = (string)($op['resource'] ?? '');
        if (!in_array($key, $allowed, true)) {
            $title = $resources[$key]['title'] ?? $key;
            throw new CrInputError('Раздел «' . $title . '» не включён для вашего гостевого аккаунта.', 403);
        }
    }
}

function cr_parse_id(mixed $raw, string $what): int
{
    if (is_int($raw)) {
        $id = $raw;
    } elseif (is_string($raw) && ctype_digit($raw)) {
        $id = (int)$raw;
    } elseif (is_float($raw) && floor($raw) === $raw) {
        $id = (int)$raw;
    } else {
        throw new CrInputError("Некорректный $what");
    }
    if ($id <= 0) throw new CrInputError("Некорректный $what");
    return $id;
}

function cr_normalize_value(mixed $v, bool $isJson, string $col): mixed
{
    if ($v === null) return null;
    if (is_bool($v)) return $v ? 1 : 0;
    if (is_int($v)) return $v;
    if (is_float($v)) {
        if (!is_finite($v)) throw new CrInputError("Поле $col: некорректное число");
        return $v;
    }
    if (is_string($v)) {
        if (preg_match('//u', $v) !== 1) throw new CrInputError("Поле $col: некорректная кодировка (ожидается UTF-8)");
        if (strlen($v) > CR_MAX_VALUE_BYTES) throw new CrInputError("Поле $col: слишком большое значение");
        return $v;
    }
    if (is_array($v)) {
        if (!$isJson) throw new CrInputError("Поле $col не поддерживает структурированные значения");
        $json = json_encode($v, JSON_UNESCAPED_UNICODE);
        if ($json === false || strlen($json) > CR_MAX_VALUE_BYTES) throw new CrInputError("Поле $col: слишком большое или некорректное JSON-значение");
        return $v;
    }
    throw new CrInputError("Поле $col: неподдерживаемый тип значения");
}

/** Validate column whitelist + values; returns DB-ready data (JSON columns encoded like db-crud.php). */
function cr_normalize_data(array $R, mixed $raw): array
{
    if (!is_array($raw) || array_is_list($raw) && $raw !== []) {
        throw new CrInputError('Некорректные данные операции');
    }
    $jsonCols = $R['json'] ?? [];
    $unknown = array_diff(array_keys($raw), $R['columns']);
    // The primary key may be echoed back by editors — it is never writable, ignore it.
    $unknown = array_values(array_filter($unknown, fn($c) => $c !== $R['pk']));
    if ($unknown) {
        throw new CrInputError('Недопустимые поля для «' . $R['title'] . '»: ' . implode(', ', array_map('strval', $unknown)));
    }
    $data = [];
    foreach ($R['columns'] as $col) {
        if (!array_key_exists($col, $raw)) continue;
        $data[$col] = cr_normalize_value($raw[$col], in_array($col, $jsonCols, true), $col);
    }
    foreach ($R['refs'] ?? [] as $col => $_table) {
        if (!array_key_exists($col, $data) || $data[$col] === null || $data[$col] === '') continue;
        $value = $data[$col];
        if ((!is_int($value) && !is_float($value) && !(is_string($value) && preg_match('/^-?[0-9]+$/', $value)))
            || (float)$value !== floor((float)$value) || (float)$value < -2147483648 || (float)$value > 2147483647) {
            throw new CrInputError("Поле $col: требуется целый ID записи");
        }
        $data[$col] = (int)$value;
    }
    // Списки ID (модули бота) — только числа и разделители. Пустые элементы
    // допускаются: они встречаются в старых строках и не меняют смысл.
    // JSON-колонки не отклоняются здесь: прямое сохранение администратором
    // (db-crud.php) их тоже не проверяет, а неразобранные значения показывает
    // анализ запроса — см. предупреждение о временных ID ниже.
    foreach (editor_reference_schema()[$R['key']] ?? [] as $rule) {
        $value = $data[$rule['column']] ?? null;
        if ($rule['codec'] !== 'csv' || !is_string($value) || trim($value) === '') continue;
        if (!preg_match('/^\s*-?[0-9]*(?:\s*,\s*-?[0-9]*)*\s*$/', $value)) {
            throw new CrInputError("Поле {$rule['column']}: требуются ID, разделённые запятыми");
        }
    }
    return encode_json_cols($data, $jsonCols);
}

/**
 * Validate + merge raw operations.
 * Result ops: ['type','resource','id'(int|null),'tempId'(int|null),'data'(array)]
 */
function cr_normalize_ops(mixed $rawOps, int $maxOps): array
{
    if (!is_array($rawOps) || !array_is_list($rawOps) || $rawOps === []) {
        throw new CrInputError('Черновик пуст — нет изменений для запроса.');
    }
    if (count($rawOps) > $maxOps) {
        throw new CrInputError("Слишком много операций в одном запросе (максимум $maxOps). Разделите изменения на несколько запросов.");
    }

    $ops = [];
    $rowIndex = [];   // "resource:id" => op index
    $tempIndex = [];  // tempId => op index

    foreach ($rawOps as $n => $raw) {
        if (!is_array($raw)) throw new CrInputError('Некорректная операция #' . ($n + 1));
        $type = (string)($raw['type'] ?? '');
        if (!in_array($type, ['create', 'update', 'delete'], true)) {
            throw new CrInputError('Неизвестный тип операции #' . ($n + 1));
        }
        $R = cr_resource((string)($raw['resource'] ?? ''));
        $key = $R['key'];

        if ($type === 'create') {
            $tempId = cr_parse_id($raw['tempId'] ?? null, 'временный ID');
            if (!is_guest_temp_id($tempId)) throw new CrInputError('Некорректный временный ID новой записи');
            if (isset($tempIndex[$tempId])) throw new CrInputError("Повторяющийся временный ID #$tempId");
            $data = cr_normalize_data($R, $raw['data'] ?? null);
            if (!$data) throw new CrInputError('Новая запись «' . $R['title'] . '» не содержит данных');
            $tempIndex[$tempId] = count($ops);
            $ops[] = ['type' => 'create', 'resource' => $key, 'id' => null, 'tempId' => $tempId, 'data' => $data];
            continue;
        }

        $rawId = $raw['id'] ?? null;
        $id = $key === 'worlds' && in_array($rawId, [0, '0'], true) ? 0 : cr_parse_id($rawId, 'ID записи');
        if (is_guest_temp_id($id)) {
            throw new CrInputError('Изменение/удаление новой записи должно быть объединено с её созданием. Обновите черновик.');
        }
        $rk = "$key:$id";

        if ($type === 'update') {
            $data = cr_normalize_data($R, $raw['data'] ?? null);
            if (!$data) continue;
            if (isset($rowIndex[$rk])) {
                $prev = &$ops[$rowIndex[$rk]];
                if ($prev['type'] === 'delete') {
                    throw new CrInputError("Запись {$R['title']} #$id одновременно удаляется и изменяется");
                }
                $prev['data'] = array_merge($prev['data'], $data);
                unset($prev);
                continue;
            }
            $rowIndex[$rk] = count($ops);
            $ops[] = ['type' => 'update', 'resource' => $key, 'id' => $id, 'tempId' => null, 'data' => $data];
            continue;
        }

        // delete
        if (isset($rowIndex[$rk])) {
            $ops[$rowIndex[$rk]] = ['type' => 'delete', 'resource' => $key, 'id' => $id, 'tempId' => null, 'data' => []];
            continue;
        }
        $rowIndex[$rk] = count($ops);
        $ops[] = ['type' => 'delete', 'resource' => $key, 'id' => $id, 'tempId' => null, 'data' => []];
    }

    if (!$ops) throw new CrInputError('В черновике нет фактических изменений.');
    return cr_order_ops($ops);
}

/** Temporary item IDs inside tw_crafts_list.RequiredItems pairs. */
function cr_craft_required_item_temp_refs(mixed $raw): array
{
    return array_values(array_unique(array_column(array_filter(
        editor_data_references('crafts', ['RequiredItems'=>$raw]),
        static fn($ref) => is_guest_temp_id($ref['id'])
    ), 'id')));
}

/** Compatibility helper; approval uses the generic typed remapper. */
function cr_remap_craft_required_items(mixed $raw, array $idMap): mixed
{
    return editor_remap_references('crafts', ['RequiredItems'=>$raw], $idMap)['RequiredItems'];
}

/** Reserved draft IDs occurring anywhere in a value (text, numbers, JSON). */
function cr_scan_temp_ids(mixed $value): array
{
    if ($value === null) return [];
    $s = is_string($value) ? $value : (string)json_encode($value);
    if (!preg_match_all('/(?<!\d)(200\d{7})(?!\d)/', $s, $m)) return [];
    return array_values(array_unique(array_filter(array_map('intval', $m[1]), 'is_guest_temp_id')));
}

/** All typed references, including embedded JSON and serialized lists. */
function cr_op_temp_refs(array $op): array
{
    return array_values(array_unique(array_column(array_filter(
        editor_data_references($op['resource'], $op['data']),
        static fn($ref) => is_guest_temp_id($ref['id'])
    ), 'id')));
}

/** Creates first (dependency ordered), then updates, then deletes. */
function cr_order_ops(array $ops): array
{
    $creates = array_values(array_filter($ops, fn($o) => $o['type'] === 'create'));
    $others = array_values(array_filter($ops, fn($o) => $o['type'] !== 'create'));
    $known = [];
    foreach ($creates as $op) $known[$op['tempId']] = $op['resource'];

    $deleted = [];
    foreach ($ops as $op) if ($op['type'] === 'delete') $deleted[$op['resource'] . ':' . $op['id']] = true;
    foreach ($ops as $op) {
        foreach (editor_data_references($op['resource'], $op['data']) as $ref) {
            $t = $ref['id'];
            if (isset($deleted[$ref['target'] . ':' . $t])) throw new CrInputError("Поле {$ref['column']} ссылается на запись #$t, удаляемую в этом же запросе");
            if (!is_guest_temp_id($t)) continue;
            if (!isset($known[$t])) {
                throw new CrInputError("Поле {$ref['column']} ссылается на новую запись #$t, которой нет в этом запросе. Дождитесь одобрения связанного запроса или добавьте запись в этот черновик.");
            }
            if ($known[$t] !== $ref['target']) {
                throw new CrInputError("Поле {$ref['column']}: новая запись #$t относится к {$known[$t]}, а требуется {$ref['target']}");
            }
        }
    }

    $ordered = [];
    $placed = [];
    while ($creates) {
        $progress = false;
        foreach ($creates as $i => $op) {
            $deps = array_filter(cr_op_temp_refs($op), fn($t) => $t !== $op['tempId']);
            if (array_diff($deps, array_keys($placed))) continue;
            $ordered[] = $op;
            $placed[$op['tempId']] = true;
            unset($creates[$i]);
            $progress = true;
        }
        if (!$progress) {
            // Embedded JSON/list references can be filled after all INSERT IDs
            // are known. Ordinary FK cycles are rejected before any DB writes.
            foreach ($creates as $i => $op) {
                $hardDeps = [];
                foreach (editor_reference_schema()[$op['resource']] ?? [] as $rule) {
                    if ($rule['codec'] !== 'id') continue;
                    $value = $op['data'][$rule['column']] ?? null;
                    if (is_guest_temp_id($value) && (int)$value !== (int)$op['tempId']) $hardDeps[] = (int)$value;
                }
                if (array_diff($hardDeps, array_keys($placed))) continue;
                $ordered[] = $op;
                $placed[$op['tempId']] = true;
                unset($creates[$i]);
                $progress = true;
                break;
            }
            if (!$progress) throw new CrInputError('Циклическая зависимость обычных полей-ссылок между новыми записями. Сохраните одну из связей отдельным изменением после одобрения.');
        }
    }
    $updates = array_filter($others, fn($o) => $o['type'] === 'update');
    $deletes = array_filter($others, fn($o) => $o['type'] === 'delete');
    return array_values(array_merge($ordered, $updates, $deletes));
}

// ── Analysis ─────────────────────────────────────────────────────────────────

/**
 * Сравнение значений вынесено в db-resources.php (нужно и прямому сохранению
 * через db-crud.php): NULL, '' и 0 означают одно и то же «нет значения».
 * Здесь только тонкие обёртки, чтобы не расходились правила анализа запросов.
 */
function cr_decode_json_value(mixed $v): mixed
{
    return editor_decode_json_value($v);
}

function cr_canonical(mixed $v, bool $isJson): ?string
{
    return editor_value_canonical($v, $isJson);
}

function cr_values_equal(mixed $a, mixed $b, bool $isJson): bool
{
    return editor_values_equal($a, $b, $isJson);
}

/** Value prepared for display (JSON columns pretty printed). */
function cr_display_value(mixed $v, bool $isJson): mixed
{
    if ($v === null) return null;
    if ($isJson) {
        $d = cr_decode_json_value($v);
        if (is_array($d)) return json_encode($d, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
    }
    return is_scalar($v) ? (string)$v : json_encode($v, JSON_UNESCAPED_UNICODE);
}

final class CrLookup
{
    private array $labels = [];
    private array $counts = [];

    public function __construct(private mysqli $db) {}

    /** ['exists'=>bool, 'label'=>?string] — null when table is unavailable. */
    public function row(string $table, int $id): ?array
    {
        $key = "$table:$id";
        if (array_key_exists($key, $this->labels)) return $this->labels[$key];
        $labelCol = EDITOR_TABLE_META[$table]['label'] ?? null;
        try {
            editor_identifier($table);
            $sel = $labelCol ? '`' . editor_identifier($labelCol) . '` AS l' : '1 AS l';
            $stmt = $this->db->prepare("SELECT $sel FROM `$table` WHERE `ID` = ? LIMIT 1");
            if ($stmt === false) return $this->labels[$key] = null;
            $stmt->bind_param('i', $id);
            $stmt->execute();
            $res = $stmt->get_result();
            $r = $res ? $res->fetch_assoc() : null;
            $stmt->close();
            return $this->labels[$key] = [
                'exists' => (bool)$r,
                'label' => ($r && $labelCol) ? (string)$r['l'] : null,
            ];
        } catch (Throwable) {
            return $this->labels[$key] = null;
        }
    }

    public function count(string $table, string $column, int $id): ?int
    {
        $key = "$table:$column:$id";
        if (array_key_exists($key, $this->counts)) return $this->counts[$key];
        try {
            editor_identifier($table);
            editor_identifier($column);
            $stmt = $this->db->prepare("SELECT COUNT(*) AS c FROM `$table` WHERE `$column` = ?");
            if ($stmt === false) return $this->counts[$key] = null;
            $stmt->bind_param('i', $id);
            $stmt->execute();
            $res = $stmt->get_result();
            $r = $res ? $res->fetch_assoc() : null;
            $stmt->close();
            return $this->counts[$key] = $r ? (int)$r['c'] : 0;
        } catch (Throwable) {
            return $this->counts[$key] = null;
        }
    }
}

function cr_row_label(array $R, ?array $row): ?string
{
    $col = $R['label'] ?? null;
    if (!$col || !$row || !isset($row[$col])) return null;
    $label = trim((string)$row[$col]);
    return $label === '' ? null : cr_substr($label, 80);
}

function cr_describe_ref(CrLookup $lookup, string $table, mixed $value, array $tempLabels): ?array
{
    if ($value === null || $value === '' || !is_numeric($value)) return null;
    $id = (int)$value;
    if ($id <= 0) return null;
    if (is_guest_temp_id($id)) {
        return [
            'id' => $id, 'temp' => true, 'exists' => isset($tempLabels[$id]),
            'label' => $tempLabels[$id] ?? null,
        ];
    }
    $info = $lookup->row($table, $id);
    if ($info === null) return ['id' => $id, 'temp' => false, 'exists' => null, 'label' => null];
    return ['id' => $id, 'temp' => false, 'exists' => $info['exists'], 'label' => $info['label']];
}

function cr_ref_text(string $table, ?array $ref): string
{
    if (!$ref) return '—';
    $title = editor_table_title($table);
    if (!empty($ref['temp'])) return "$title (новая запись из этого запроса)";
    return $title . ' #' . $ref['id'] . (!empty($ref['label']) ? ' «' . $ref['label'] . '»' : '');
}

/** Reverse-reference impact for update/delete of a row. */
function cr_impact(CrLookup $lookup, string $table, int $id, string $type): array
{
    $grouped = [];
    foreach (editor_reverse_refs($table) as $ref) {
        $count = $lookup->count($ref['table'], $ref['column'], $id);
        if (!$count) continue;
        $k = $ref['table'] . '|' . $ref['action'];
        $grouped[$k] ??= ['table' => $ref['table'], 'tableTitle' => editor_table_title($ref['table']), 'action' => $ref['action'], 'columns' => [], 'count' => 0];
        $grouped[$k]['columns'][] = $ref['column'];
        $grouped[$k]['count'] += $count;
    }
    $out = [];
    foreach ($grouped as $g) {
        $cols = implode(', ', $g['columns']);
        if ($type === 'delete') {
            if ($g['action'] === 'cascade') {
                $g['severity'] = 'danger';
                $g['text'] = "Будет удалено каскадно: {$g['count']} — {$g['tableTitle']} (поле $cols)";
            } elseif ($g['action'] === 'set_null') {
                $g['severity'] = 'warning';
                $g['text'] = "Потеряют ссылку (станет NULL): {$g['count']} — {$g['tableTitle']} (поле $cols)";
            } else {
                $g['severity'] = 'warning';
                $g['text'] = "Останутся ссылаться на удалённую запись: {$g['count']} — {$g['tableTitle']} (поле $cols)";
            }
        } else {
            $g['severity'] = 'info';
            $g['text'] = "{$g['tableTitle']}: {$g['count']} (поле $cols)";
        }
        $out[] = $g;
    }
    // Updates do not break references: collapse them into one compact informational line.
    if ($type !== 'delete' && $out) {
        $total = array_sum(array_column($out, 'count'));
        return [[
            'table' => '', 'tableTitle' => '', 'action' => 'reference', 'columns' => [], 'count' => $total,
            'severity' => 'info',
            'text' => 'Запись используется в других таблицах (увидят новые значения): ' . implode(', ', array_column($out, 'text')),
        ]];
    }
    usort($out, fn($a, $b) => ['danger' => 0, 'warning' => 1, 'info' => 2][$a['severity']] <=> ['danger' => 0, 'warning' => 1, 'info' => 2][$b['severity']]);
    return $out;
}

/**
 * Что произойдёт с временными ID значения при применении:
 *  * info    — ссылка описана схемой, ID будет заменён автоматически;
 *  * warning — зарезервированный ID стоит там, где ссылки быть не может
 *              (свободный текст, количество, неразобранный JSON): замена не выполняется.
 */
function cr_temp_id_notes(string $resource, array $data, array $refs = []): array
{
    $notes = [];
    $typedByColumn = [];
    foreach (editor_data_references($resource, $data) as $ref) {
        if (!is_guest_temp_id($ref['id'])) continue;
        $typedByColumn[$ref['column']][] = $ref['id'];
        if (isset($refs[$ref['column']])) continue;
        $notes[] = ['severity' => 'info', 'text' => "Поле {$ref['column']}: временный ID #{$ref['id']} будет автоматически заменён ID из БД ({$ref['target']})."];
    }
    foreach ($data as $col => $val) {
        $uncovered = array_values(array_diff(cr_scan_temp_ids($val), $typedByColumn[$col] ?? []));
        if (!$uncovered) continue;
        $notes[] = ['severity' => 'warning', 'text' => "Поле $col содержит временный ID новой записи (#" . implode(', #', $uncovered) . ") в месте, которое не является ссылкой. Он не будет заменён автоматически — проверьте значение перед применением."];
    }
    return $notes;
}

/**
 * Full analysis of normalized operations against the CURRENT database state.
 * Returns ['operations'=>[...], 'summary'=>[...], 'errors'=>[...], 'snapshots'=>[index=>row]]
 */
function cr_analyze(mysqli $db, array $ops): array
{
    $lookup = new CrLookup($db);
    $tempLabels = [];
    foreach ($ops as $op) {
        if ($op['type'] !== 'create') continue;
        $R = cr_resource($op['resource'], false);
        $tempLabels[$op['tempId']] = cr_row_label($R, $op['data']) ?? ($R['title'] . ' (новая)');
    }

    $result = [];
    $errors = [];
    $snapshots = [];
    $summary = ['create' => 0, 'update' => 0, 'delete' => 0, 'fields' => 0, 'danger' => 0, 'warning' => 0, 'resources' => []];

    foreach ($ops as $i => $op) {
        $R = cr_resource($op['resource'], false);
        $jsonCols = $R['json'] ?? [];
        $refs = $R['refs'] ?? [];
        $entry = [
            'index' => $i,
            'type' => $op['type'],
            'resource' => $op['resource'],
            'resourceTitle' => $R['title'],
            'table' => $R['table'],
            'id' => $op['id'],
            'tempId' => $op['tempId'],
            'label' => null,
            'title' => '',
            'changes' => [],
            'impact' => [],
            'warnings' => [],
            'noop' => false,
            'error' => null,
        ];

        $current = null;
        if ($op['type'] !== 'create') {
            $current = crud_fetch_row($db, $R, (int)$op['id']);
            if (!$current) {
                $entry['error'] = "Запись {$R['title']} #{$op['id']} не найдена в базе данных (возможно, уже удалена).";
                $errors[] = $entry['error'];
            }
        }

        $entry['label'] = $op['type'] === 'create' ? cr_row_label($R, $op['data']) : cr_row_label($R, $current);
        $name = $entry['label'] ? ' «' . $entry['label'] . '»' : '';

        if ($op['type'] === 'create') {
            $entry['title'] = "Создание: {$R['title']}$name (новая запись)";
            foreach ($op['data'] as $col => $val) {
                $isJson = in_array($col, $jsonCols, true);
                $change = ['column' => $col, 'before' => null, 'after' => cr_display_value($val, $isJson), 'json' => $isJson];
                if (isset($refs[$col])) {
                    $ref = cr_describe_ref($lookup, $refs[$col], $val, $tempLabels);
                    $change['refAfter'] = $ref ? cr_ref_text($refs[$col], $ref) : null;
                    if ($ref && $ref['exists'] === false) {
                        $entry['warnings'][] = ['severity' => 'danger', 'text' => "Поле $col ссылается на несуществующую запись: " . editor_table_title($refs[$col]) . " #{$ref['id']}"];
                    }
                }
                $entry['changes'][] = $change;
            }
        } elseif ($op['type'] === 'update' && $current) {
            $snap = [];
            foreach ($op['data'] as $col => $val) {
                $isJson = in_array($col, $jsonCols, true);
                $before = $current[$col] ?? null;
                if (cr_values_equal($before, $val, $isJson)) continue;
                $snap[$col] = $before;
                $change = ['column' => $col, 'before' => cr_display_value($before, $isJson), 'after' => cr_display_value($val, $isJson), 'json' => $isJson];
                if (isset($refs[$col])) {
                    $rb = cr_describe_ref($lookup, $refs[$col], $before, $tempLabels);
                    $ra = cr_describe_ref($lookup, $refs[$col], $val, $tempLabels);
                    $change['refBefore'] = $rb ? cr_ref_text($refs[$col], $rb) : null;
                    $change['refAfter'] = $ra ? cr_ref_text($refs[$col], $ra) : null;
                    if ($ra && $ra['exists'] === false) {
                        $entry['warnings'][] = ['severity' => 'danger', 'text' => "Поле $col ссылается на несуществующую запись: " . editor_table_title($refs[$col]) . " #{$ra['id']}"];
                    }
                }
                $entry['changes'][] = $change;
            }
            $snapshots[$i] = $snap;
            $entry['title'] = "Изменение: {$R['title']} #{$op['id']}$name";
            if (!$entry['changes']) {
                $entry['noop'] = true;
            } else {
                $entry['impact'] = cr_impact($lookup, $R['table'], (int)$op['id'], 'update');
            }
        } elseif ($op['type'] === 'delete' && $current) {
            $snapshots[$i] = $current;
            $entry['title'] = "Удаление: {$R['title']} #{$op['id']}$name";
            foreach ($R['columns'] as $col) {
                $isJson = in_array($col, $jsonCols, true);
                $before = $current[$col] ?? null;
                $change = ['column' => $col, 'before' => cr_display_value($before, $isJson), 'after' => null, 'json' => $isJson];
                if (isset($refs[$col])) {
                    $rb = cr_describe_ref($lookup, $refs[$col], $before, $tempLabels);
                    $change['refBefore'] = $rb ? cr_ref_text($refs[$col], $rb) : null;
                }
                $entry['changes'][] = $change;
            }
            $entry['impact'] = cr_impact($lookup, $R['table'], (int)$op['id'], 'delete');
        } else {
            $entry['title'] = ($op['type'] === 'update' ? 'Изменение' : 'Удаление') . ": {$R['title']} #{$op['id']}";
        }

        foreach (cr_temp_id_notes($op['resource'], $op['data'], $refs) as $note) {
            $entry['warnings'][] = $note;
        }

        if (!$entry['noop'] && !$entry['error']) {
            $summary[$op['type']]++;
            $summary['fields'] += $op['type'] === 'delete' ? 0 : count($entry['changes']);
            $summary['resources'][$R['title']] = ($summary['resources'][$R['title']] ?? 0) + 1;
            foreach (array_merge($entry['impact'], $entry['warnings']) as $item) {
                if (($item['severity'] ?? '') === 'danger') $summary['danger']++;
                if (($item['severity'] ?? '') === 'warning') $summary['warning']++;
            }
        }
        $result[] = $entry;
    }

    return ['operations' => $result, 'summary' => $summary, 'errors' => $errors, 'snapshots' => $snapshots, 'generatedAt' => cr_now()];
}

/** Drop no-op updates; returns [ops, analysis] re-indexed. */
function cr_prepare_for_submit(mysqli $db, array $ops): array
{
    $analysis = cr_analyze($db, $ops);
    if ($analysis['errors']) {
        throw new CrInputError(implode("\n", $analysis['errors']), 409, ['analysis' => cr_public_analysis($analysis)]);
    }
    $kept = [];
    foreach ($analysis['operations'] as $entry) {
        if ($entry['noop']) continue;
        $op = $ops[$entry['index']];
        $op['before'] = $analysis['snapshots'][$entry['index']] ?? null;
        // Черновик мог быть создан до правила «NULL = '' = 0» и содержать
        // пустые поля там, где в БД NULL. В запрос попадают только реальные
        // изменения — иначе карточка запроса показывала бы «NULL → пусто».
        if (($op['type'] ?? '') === 'update' && is_array($entry['changes'] ?? null)) {
            $columns = array_column($entry['changes'], 'column');
            $op['data'] = array_intersect_key($op['data'], array_flip($columns));
        }
        $kept[] = $op;
    }
    if (!$kept) {
        throw new CrInputError('Изменения совпадают с текущими данными — запрашивать нечего.');
    }
    $final = cr_analyze($db, array_map(fn($o) => array_diff_key($o, ['before' => 1]), $kept));
    return [$kept, $final];
}

function cr_public_analysis(array $analysis): array
{
    unset($analysis['snapshots']);
    return $analysis;
}

// ── Conflicts & apply (admin only) ───────────────────────────────────────────

/** Compare stored snapshots with the current DB state. */
function cr_detect_conflicts(mysqli $db, array $ops, bool $forUpdate = false): array
{
    $conflicts = [];
    foreach ($ops as $i => $op) {
        if ($op['type'] === 'create') continue;
        $R = cr_resource($op['resource'], false);
        $jsonCols = $R['json'] ?? [];
        $current = crud_fetch_row($db, $R, (int)$op['id'], $forUpdate);
        if (!$current) {
            $conflicts[] = ['index' => $i, 'kind' => 'missing', 'text' => "{$R['title']} #{$op['id']}: запись больше не существует"];
            continue;
        }
        $before = is_array($op['before'] ?? null) ? $op['before'] : [];
        $changed = [];
        foreach ($before as $col => $val) {
            if (!in_array($col, $R['columns'], true)) continue;
            if (!cr_values_equal($current[$col] ?? null, $val, in_array($col, $jsonCols, true))) $changed[] = $col;
        }
        if ($changed) {
            $conflicts[] = [
                'index' => $i, 'kind' => 'changed', 'columns' => $changed,
                'text' => "{$R['title']} #{$op['id']}: с момента создания запроса изменены поля " . implode(', ', $changed),
            ];
        }
    }
    return $conflicts;
}

/**
 * Последовательность применения операций без доступа к БД: замена временных ID,
 * порядок вставок и отложенная дозапись ссылок на саму запись. Колбэки выполняют
 * реальные запросы, поэтому логика проверяется отдельно от mysqli.
 *
 * @param callable(array $R, array $data): int          $insert
 * @param callable(array $R, int $id, array $data): void $updateRow
 * @param callable(array $R, int $id): void              $deleteRow
 * @param callable(array $R, int $id): ?array            $fetchRow
 */
function cr_apply_sequence(array $ops, callable $insert, callable $updateRow, callable $deleteRow, callable $fetchRow, array $missing = []): array
{
    $idMap = [];
    $applied = [];
    $deferred = [];
    $skipped = [];
    foreach ($ops as $i => $op) {
        // Defence in depth: a request may only touch resources that are
        // guest-editable at all, even if the stored file was tampered with.
        $R = cr_resource($op['resource'], true);
        $selfTemp = $op['type'] === 'create' ? (int)$op['tempId'] : null;
        $data = editor_remap_references($op['resource'], $op['data'], $idMap, $selfTemp !== null, $selfTemp);
        if (isset($missing[$i])) {
            $skipped[] = ['index' => $i, 'text' => "{$R['title']} #{$op['id']}: пропущено — запись не существует"];
            continue;
        }
        if ($op['type'] === 'create') {
            $newId = $insert($R, $data);
            $idMap[(int)$op['tempId']] = $newId;
            $pendingColumns = [];
            foreach (editor_data_references($op['resource'], $op['data']) as $ref) {
                // Unknown at INSERT time: another record created later, or this very record.
                if (is_guest_temp_id($ref['id']) && !isset($idMap[$ref['id']])) $pendingColumns[$ref['column']] = $op['data'][$ref['column']];
                if ($ref['id'] === (int)$op['tempId']) $pendingColumns[$ref['column']] = $op['data'][$ref['column']];
            }
            if ($pendingColumns) $deferred[] = ['resource' => $op['resource'], 'id' => $newId, 'data' => $pendingColumns, 'appliedIndex' => count($applied)];
            $applied[] = ['type' => 'create', 'resource' => $op['resource'], 'id' => $newId, 'tempId' => $op['tempId'], 'data' => decode_json_cols($data, $R['json'] ?? [])];
        } elseif ($op['type'] === 'update') {
            // Only columns that actually differ from the snapshot are written.
            $write = is_array($op['before'] ?? null) && $op['before'] ? array_intersect_key($data, $op['before']) : $data;
            // Запрос мог быть создан до правила «NULL = '' = 0» и содержать
            // пустые значения там, где в БД NULL. Такие поля не пишем —
            // иначе одобрение заменило бы NULL на пустую строку/ноль.
            $currentRow = $fetchRow($R, (int)$op['id']);
            if ($currentRow) {
                $currentValues = decode_json_cols($currentRow, $R['json'] ?? []);
                $rowJsonCols = $R['json'] ?? [];
                $write = array_filter(
                    $write,
                    fn($val, $col) => !editor_values_equal($currentValues[$col] ?? null, $val, in_array($col, $rowJsonCols, true)),
                    ARRAY_FILTER_USE_BOTH
                );
            }
            if (!$write) continue;
            $updateRow($R, (int)$op['id'], $write);
            $applied[] = ['type' => 'update', 'resource' => $op['resource'], 'id' => (int)$op['id'], 'data' => decode_json_cols($write, $R['json'] ?? [])];
        } else {
            $deleteRow($R, (int)$op['id']);
            $applied[] = ['type' => 'delete', 'resource' => $op['resource'], 'id' => (int)$op['id'], 'data' => []];
        }
    }
    foreach ($deferred as $pending) {
        $R = cr_resource($pending['resource'], true);
        $data = editor_remap_references($pending['resource'], $pending['data'], $idMap);
        $updateRow($R, $pending['id'], $data);
        $applied[$pending['appliedIndex']]['data'] = array_replace(
            $applied[$pending['appliedIndex']]['data'], decode_json_cols($data, $R['json'] ?? [])
        );
    }
    return ['idMap' => $idMap, 'applied' => $applied, 'skipped' => $skipped];
}

/**
 * Apply a request in ONE transaction (all-or-nothing).
 * Returns ['idMap'=>[temp=>real], 'applied'=>[...], 'skipped'=>[...]]
 */
function cr_apply(mysqli $db, array $req, bool $force): array
{
    // Re-validate stored operations (defence in depth: file could be tampered).
    $rawOps = array_map(fn($o) => [
        'type' => $o['type'] ?? '', 'resource' => $o['resource'] ?? '', 'id' => $o['id'] ?? null,
        'tempId' => $o['tempId'] ?? null, 'data' => cr_decode_ops_data($o),
    ], is_array($req['operations'] ?? null) ? $req['operations'] : []);
    $ops = cr_normalize_ops($rawOps, 1000);
    // Re-attach snapshots by identity.
    $snapByKey = [];
    foreach ($req['operations'] as $o) {
        $k = ($o['type'] ?? '') . ':' . ($o['resource'] ?? '') . ':' . ($o['id'] ?? $o['tempId'] ?? '');
        $snapByKey[$k] = $o['before'] ?? null;
    }
    foreach ($ops as &$op) {
        $op['before'] = $snapByKey[$op['type'] . ':' . $op['resource'] . ':' . ($op['id'] ?? $op['tempId'])] ?? null;
    }
    unset($op);

    $db->begin_transaction();
    try {
        $conflicts = cr_detect_conflicts($db, $ops, true);
        if ($conflicts && !$force) {
            $db->rollback();
            throw new CrInputError('Данные в БД изменились с момента создания запроса. Проверьте конфликты и примените принудительно, если уверены.', 409, ['conflicts' => $conflicts]);
        }
        $missing = [];
        foreach ($conflicts as $c) {
            if ($c['kind'] === 'missing') $missing[$c['index']] = true;
        }

        $result = cr_apply_sequence(
            $ops,
            fn(array $R, array $data): int => crud_insert($db, $R, $data),
            function (array $R, int $id, array $data) use ($db): void { crud_update($db, $R, $id, $data); },
            function (array $R, int $id) use ($db): void { crud_delete($db, $R, $id); },
            fn(array $R, int $id): ?array => crud_fetch_row($db, $R, $id, true),
            $missing,
        );
        $idMap = $result['idMap'];
        $applied = $result['applied'];
        $skipped = $result['skipped'];
        $db->commit();
        return ['idMap' => $idMap, 'applied' => $applied, 'skipped' => $skipped, 'conflicts' => $conflicts];
    } catch (CrInputError $e) {
        throw $e;
    } catch (Throwable $e) {
        try { $db->rollback(); } catch (Throwable) {}
        throw new CrInputError('Применение отменено (транзакция откатена): ' . editor_safe_message($e), 500);
    }
}

/** Stored ops keep DB-ready data; return it as-is for re-validation. */
function cr_decode_ops_data(array $o): array
{
    return is_array($o['data'] ?? null) ? $o['data'] : [];
}

// ── Views ────────────────────────────────────────────────────────────────────

function cr_list_item(array $req, bool $admin, int $unread = 0): array
{
    $item = [
        'id' => $req['id'],
        'status' => $req['status'],
        'createdAt' => $req['createdAt'] ?? null,
        'updatedAt' => $req['updatedAt'] ?? null,
        'version' => max(1, (int)($req['version'] ?? 1)),
        'lastRevision' => $req['lastRevision'] ?? null,
        'nickname' => $req['author']['nickname'] ?? '',
        'guestName' => $req['author']['guestName'] ?? '',
        'guestLogin' => $req['author']['guestLogin'] ?? '',
        'message' => cr_substr((string)($req['message'] ?? ''), 220),
        'summary' => $req['analysis']['summary'] ?? null,
        'operationsCount' => count($req['operations'] ?? []),
        'review' => isset($req['review']) ? [
            'at' => $req['review']['at'] ?? null,
            'comment' => $req['review']['comment'] ?? '',
        ] : null,
        // Непрочитанные сообщения переписки — по ним рисуется бейдж.
        'unread' => $unread,
    ];
    if ($admin) {
        $item['ip'] = $req['author']['ip'] ?? '';
        if (isset($req['review'])) $item['review']['by'] = $req['review']['by'] ?? '';
    }
    return $item;
}

function cr_full_view(array $req, bool $admin): array
{
    $view = cr_list_item($req, $admin);
    $view['message'] = (string)($req['message'] ?? '');
    $view['analysis'] = $req['analysis'] ?? null;
    $view['revisions'] = $req['revisions'] ?? [];
    // The owner and the administrator may revise the pending request. Return
    // decoded values for the workbench; approval still revalidates everything.
    $view['operations'] = array_map(function (array $o): array {
        $R = cr_resource((string)$o['resource']);
        $jsonCols = $R['json'] ?? [];
        $before = is_array($o['before'] ?? null) ? $o['before'] : null;
        return [
            'type' => $o['type'], 'resource' => $o['resource'],
            'id' => $o['id'], 'tempId' => $o['tempId'],
            'data' => decode_json_cols(is_array($o['data'] ?? null) ? $o['data'] : [], $jsonCols),
            'before' => $before === null ? null : decode_json_cols($before, $jsonCols),
        ];
    }, $req['operations'] ?? []);
    if ($admin) {
        $view['author'] = $req['author'] ?? [];
        unset($view['author']['guestHash']);
        $view['review'] = $req['review'] ?? null;
    } elseif (isset($req['review']['idMap']) && is_array($req['review']['idMap'])) {
        // The owner needs the same temporary → real ID mapping for links to
        // approved create operations. It contains only this request's IDs.
        $view['review']['idMap'] = $req['review']['idMap'];
    }
    return $view;
}
