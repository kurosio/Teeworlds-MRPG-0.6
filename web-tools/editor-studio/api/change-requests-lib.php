<?php
// Change requests (guest → admin) — storage, validation, analysis, apply.
//
// Security model:
//  * Requests are stored as JSON files in data/change-requests/ — NOT in the
//    game database. A guest has no code path that writes to the game DB.
//  * The game DB is only READ while a request is analysed (prepared statements,
//    whitelisted tables/columns from db-resources.php).
//  * Writing happens exclusively in cr_apply(), which is reachable only by an
//    administrator (api/change-requests.php → action=approve).

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';
require_once __DIR__ . '/db-resources.php';

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
        throw new CrInputError('Укажите Nickname (от ' . CR_NICK_MIN . ' до ' . CR_NICK_MAX . ' символов).');
    }
    return $nick;
}

function cr_validate_message(mixed $raw): string
{
    $msg = cr_clean_text($raw, true);
    $len = cr_strlen($msg);
    if ($len < CR_MESSAGE_MIN || $len > CR_MESSAGE_MAX) {
        throw new CrInputError('Опишите, зачем нужны изменения (от ' . CR_MESSAGE_MIN . ' до ' . CR_MESSAGE_MAX . ' символов).');
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

// ── Storage (outside of the game database) ───────────────────────────────────

function cr_dir(): string
{
    $dir = dirname(__DIR__) . '/data/change-requests';
    if (!is_dir($dir) && !@mkdir($dir, 0775, true) && !is_dir($dir)) {
        throw new RuntimeException('Не удалось создать каталог для запросов: data/change-requests');
    }
    // Defence in depth: deny direct web access even without the provided Apache config.
    $ht = $dir . '/.htaccess';
    if (!is_file($ht)) {
        @file_put_contents($ht, "Require all denied\nDeny from all\n");
    }
    $idx = $dir . '/index.html';
    if (!is_file($idx)) {
        @file_put_contents($idx, '');
    }
    return $dir;
}

function cr_valid_id(string $id): bool
{
    return preg_match('/^CR-\d{8}-\d{6}-[a-f0-9]{6}$/', $id) === 1;
}

function cr_new_id(): string
{
    return 'CR-' . gmdate('Ymd-His') . '-' . bin2hex(random_bytes(3));
}

function cr_path(string $id): string
{
    if (!cr_valid_id($id)) {
        throw new CrInputError('Некорректный ID запроса.');
    }
    return cr_dir() . '/' . $id . '.json';
}

/** Run $fn under an exclusive lock (all state transitions are serialized). */
function cr_with_lock(callable $fn): mixed
{
    $fh = fopen(cr_dir() . '/.lock', 'c');
    if ($fh === false) throw new RuntimeException('Не удалось открыть lock-файл запросов');
    try {
        if (!flock($fh, LOCK_EX)) throw new RuntimeException('Не удалось получить блокировку');
        return $fn();
    } finally {
        flock($fh, LOCK_UN);
        fclose($fh);
    }
}

function cr_load(string $id): ?array
{
    $path = cr_path($id);
    if (!is_file($path)) return null;
    $raw = file_get_contents($path);
    $data = $raw === false ? null : json_decode($raw, true);
    return is_array($data) ? $data : null;
}

function cr_save(array $req): void
{
    $path = cr_path((string)$req['id']);
    $json = json_encode($req, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT | JSON_THROW_ON_ERROR);
    $tmp = $path . '.tmp' . bin2hex(random_bytes(4));
    if (file_put_contents($tmp, $json, LOCK_EX) === false || !rename($tmp, $path)) {
        @unlink($tmp);
        throw new RuntimeException('Не удалось сохранить запрос');
    }
}

function cr_delete_file(string $id): void
{
    $path = cr_path($id);
    if (is_file($path) && !unlink($path)) throw new RuntimeException('Не удалось удалить запрос');
}

/** @return list<array> newest first */
function cr_all(): array
{
    $out = [];
    foreach (glob(cr_dir() . '/CR-*.json') ?: [] as $file) {
        $id = basename($file, '.json');
        if (!cr_valid_id($id)) continue;
        $raw = file_get_contents($file);
        $data = $raw === false ? null : json_decode($raw, true);
        if (is_array($data)) $out[] = $data;
    }
    usort($out, fn($a, $b) => strcmp((string)($b['createdAt'] ?? ''), (string)($a['createdAt'] ?? '')) ?: strcmp((string)$b['id'], (string)$a['id']));
    return $out;
}

/** Simple per-IP throttle stored next to requests. Returns seconds left (0 = allowed). */
function cr_ip_cooldown_left(string $ip, int $cooldown, bool $touch): int
{
    if ($cooldown <= 0 || $ip === '') return 0;
    $file = cr_dir() . '/.ratelimit.json';
    $map = is_file($file) ? json_decode((string)file_get_contents($file), true) : [];
    if (!is_array($map)) $map = [];
    $now = time();
    foreach ($map as $k => $ts) {
        if (!is_int($ts) || $ts < $now - 86400) unset($map[$k]);
    }
    $key = hash('sha256', 'ip|' . $ip);
    $left = isset($map[$key]) ? max(0, $map[$key] + $cooldown - $now) : 0;
    if ($touch && $left === 0) {
        $map[$key] = $now;
        @file_put_contents($file, json_encode($map), LOCK_EX);
    }
    return $left;
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

        $id = cr_parse_id($raw['id'] ?? null, 'ID записи');
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

/** Temp ids referenced from FK columns of an operation. */
function cr_op_temp_refs(array $op): array
{
    $R = cr_resource($op['resource'], false);
    $refs = [];
    foreach (($R['refs'] ?? []) as $col => $_table) {
        if (array_key_exists($col, $op['data']) && is_guest_temp_id($op['data'][$col])) {
            $refs[] = (int)$op['data'][$col];
        }
    }
    return $refs;
}

/** Creates first (dependency ordered), then updates, then deletes. */
function cr_order_ops(array $ops): array
{
    $creates = array_values(array_filter($ops, fn($o) => $o['type'] === 'create'));
    $others = array_values(array_filter($ops, fn($o) => $o['type'] !== 'create'));
    $known = array_flip(array_map(fn($o) => $o['tempId'], $creates));

    foreach ($ops as $op) {
        foreach (cr_op_temp_refs($op) as $t) {
            if (!isset($known[$t])) {
                throw new CrInputError("Операция ссылается на новую запись #$t, которой нет в черновике");
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
        if (!$progress) throw new CrInputError('Циклическая зависимость между новыми записями');
    }
    $updates = array_filter($others, fn($o) => $o['type'] === 'update');
    $deletes = array_filter($others, fn($o) => $o['type'] === 'delete');
    return array_values(array_merge($ordered, $updates, $deletes));
}

// ── Analysis ─────────────────────────────────────────────────────────────────

function cr_decode_json_value(mixed $v): mixed
{
    if (!is_string($v)) return $v;
    $t = trim($v);
    if ($t === '' || ($t[0] !== '{' && $t[0] !== '[')) return $v;
    $d = json_decode($t, true);
    return json_last_error() === JSON_ERROR_NONE ? $d : $v;
}

function cr_canonical(mixed $v, bool $isJson): ?string
{
    if ($v === null) return null;
    if ($isJson) {
        $d = cr_decode_json_value($v);
        if (is_array($d)) return json_encode($d, JSON_UNESCAPED_UNICODE);
    }
    if (is_bool($v)) return $v ? '1' : '0';
    if (is_float($v) || is_int($v)) return rtrim(rtrim(number_format((float)$v, 10, '.', ''), '0'), '.') ?: '0';
    $s = (string)$v;
    if (is_numeric($s) && preg_match('/^-?\d+(\.\d+)?$/', $s) === 1) {
        return rtrim(rtrim(number_format((float)$s, 10, '.', ''), '0'), '.') ?: '0';
    }
    return $s;
}

function cr_values_equal(mixed $a, mixed $b, bool $isJson): bool
{
    return cr_canonical($a, $isJson) === cr_canonical($b, $isJson);
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

function cr_scan_temp_ids(mixed $value): array
{
    if ($value === null) return [];
    $s = is_string($value) ? $value : (string)json_encode($value);
    if (!preg_match_all('/(?<!\d)(200\d{7})(?!\d)/', $s, $m)) return [];
    return array_values(array_unique(array_filter(array_map('intval', $m[1]), 'is_guest_temp_id')));
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

        // Temp ids hidden inside text/JSON columns are NOT remapped automatically.
        foreach ($op['data'] as $col => $val) {
            if (isset($refs[$col])) continue;
            $found = cr_scan_temp_ids($val);
            if ($found) {
                $entry['warnings'][] = ['severity' => 'warning', 'text' => "Поле $col содержит временный ID новой записи (#" . implode(', #', $found) . "). Он не будет заменён автоматически — администратору нужно поправить значение после применения."];
            }
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

        $idMap = [];
        $applied = [];
        $skipped = [];
        foreach ($ops as $i => $op) {
            $R = cr_resource($op['resource'], false);
            $data = $op['data'];
            foreach (($R['refs'] ?? []) as $col => $_t) {
                if (array_key_exists($col, $data) && is_guest_temp_id($data[$col])) {
                    $t = (int)$data[$col];
                    if (!isset($idMap[$t])) throw new RuntimeException("Не удалось сопоставить временный ID #$t");
                    $data[$col] = $idMap[$t];
                }
            }
            if (isset($missing[$i])) {
                $skipped[] = ['index' => $i, 'text' => "{$R['title']} #{$op['id']}: пропущено — запись не существует"];
                continue;
            }
            if ($op['type'] === 'create') {
                $newId = crud_insert($db, $R, $data);
                $idMap[(int)$op['tempId']] = $newId;
                $applied[] = ['type' => 'create', 'resource' => $op['resource'], 'id' => $newId, 'tempId' => $op['tempId'], 'data' => decode_json_cols($data, $R['json'] ?? [])];
            } elseif ($op['type'] === 'update') {
                // Only columns that actually differ from the snapshot are written.
                $write = is_array($op['before']) && $op['before'] ? array_intersect_key($data, $op['before']) : $data;
                if (!$write) continue;
                crud_update($db, $R, (int)$op['id'], $write);
                $applied[] = ['type' => 'update', 'resource' => $op['resource'], 'id' => (int)$op['id'], 'data' => decode_json_cols($write, $R['json'] ?? [])];
            } else {
                crud_delete($db, $R, (int)$op['id']);
                $applied[] = ['type' => 'delete', 'resource' => $op['resource'], 'id' => (int)$op['id'], 'data' => []];
            }
        }
        $db->commit();
        return ['idMap' => $idMap, 'applied' => $applied, 'skipped' => $skipped, 'conflicts' => $conflicts];
    } catch (CrInputError $e) {
        throw $e;
    } catch (Throwable $e) {
        try { $db->rollback(); } catch (Throwable) {}
        throw new CrInputError('Применение отменено (транзакция откатена): ' . $e->getMessage(), 500);
    }
}

/** Stored ops keep DB-ready data; return it as-is for re-validation. */
function cr_decode_ops_data(array $o): array
{
    return is_array($o['data'] ?? null) ? $o['data'] : [];
}

// ── Views ────────────────────────────────────────────────────────────────────

function cr_list_item(array $req, bool $admin): array
{
    $item = [
        'id' => $req['id'],
        'status' => $req['status'],
        'createdAt' => $req['createdAt'] ?? null,
        'updatedAt' => $req['updatedAt'] ?? null,
        'nickname' => $req['author']['nickname'] ?? '',
        'message' => cr_substr((string)($req['message'] ?? ''), 220),
        'summary' => $req['analysis']['summary'] ?? null,
        'operationsCount' => count($req['operations'] ?? []),
        'review' => isset($req['review']) ? [
            'at' => $req['review']['at'] ?? null,
            'comment' => $req['review']['comment'] ?? '',
        ] : null,
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
    if ($admin) {
        $view['author'] = $req['author'] ?? [];
        unset($view['author']['guestHash']);
        $view['review'] = $req['review'] ?? null;
        $view['operations'] = array_map(fn($o) => [
            'type' => $o['type'], 'resource' => $o['resource'], 'id' => $o['id'], 'tempId' => $o['tempId'],
        ], $req['operations'] ?? []);
    }
    return $view;
}
