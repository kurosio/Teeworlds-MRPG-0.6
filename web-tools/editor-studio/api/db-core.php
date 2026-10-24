<?php
// Shared helpers for DB API endpoints.

declare(strict_types=1);

require_once __DIR__ . '/store.php';

// ── HTTP / Response ──────────────────────────────────────────────────────────

function apply_security_headers(): void
{
    header('Content-Type: application/json; charset=utf-8');
    header('X-Content-Type-Options: nosniff');
    header('X-Frame-Options: SAMEORIGIN');
    header('Referrer-Policy: same-origin');
    header('Cross-Origin-Resource-Policy: same-origin');
    header('Cross-Origin-Opener-Policy: same-origin');
    header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
    header('Pragma: no-cache');
}

function respond(array $payload, int $code = 200): never
{
    http_response_code($code);
    echo json_encode($payload, JSON_UNESCAPED_UNICODE | JSON_THROW_ON_ERROR);
    exit;
}

/**
 * Ошибка, текст которой безопасно показать пользователю.
 *
 * SQL-ошибки драйвера (mysqli) содержат имена таблиц и колонок — клиенту их
 * отдавать нельзя, поэтому такие исключения считаются внутренними и заменяются
 * общим сообщением, а подробности уходят в журнал сервера.
 */
class EditorUserError extends RuntimeException
{
}

function editor_log_error(Throwable $e): void
{
    error_log('[editor-studio] ' . get_class($e) . ': ' . $e->getMessage()
        . ' in ' . $e->getFile() . ':' . $e->getLine());
}

/** Ответ на непредвиденную ошибку: наружу — понятный текст, детали — в лог. */
function editor_internal_error(
    Throwable $e,
    string $friendly = 'Не удалось выполнить операцию с базой данных. Попробуйте ещё раз; если ошибка повторится — сообщите администратору.',
    int $code = 500
): never {
    editor_log_error($e);
    respond(['ok' => false, 'error' => $e instanceof EditorUserError ? $e->getMessage() : $friendly], $code);
}

/**
 * Безопасный текст ошибки для клиента.
 *
 * Свои сообщения (EditorUserError, DomainException, CrInputError) показываем
 * как есть — они написаны для пользователя. Всё остальное (исключения драйвера
 * БД, файловой системы) может содержать имена таблиц, пути и учётные данные:
 * такие подробности уходят в журнал, а клиент получает общий текст.
 */
function editor_safe_message(Throwable $e, string $fallback = 'внутренняя ошибка сервера, подробности в журнале'): string
{
    if ($e instanceof EditorUserError || $e instanceof DomainException || $e instanceof InvalidArgumentException) {
        return $e->getMessage();
    }
    editor_log_error($e);
    return $fallback;
}

/**
 * Raw request body.
 *
 * Offline checks (php-wasm) can inject a body through two environment
 * variables, EDITOR_STUDIO_TEST=1 + EDITOR_STUDIO_BODY=… — a normal web
 * deployment never sets them, so over HTTP this is always php://input.
 */
function editor_request_raw_body(int $maxBytes = 0): string
{
    if (getenv('EDITOR_STUDIO_TEST') === '1') {
        $injected = getenv('EDITOR_STUDIO_BODY');
        if ($injected !== false) {
            $raw = (string)$injected;
            return $maxBytes > 0 ? substr($raw, 0, $maxBytes) : $raw;
        }
    }
    $raw = file_get_contents('php://input', false, null, 0, $maxBytes > 0 ? $maxBytes + 1 : null);
    return $raw === false ? '' : (string)$raw;
}

function read_json_body(): array
{
    $raw = editor_request_raw_body(4 * 1024 * 1024);
    if ($raw === '') {
        return [];
    }
    $data = json_decode($raw, true);
    return is_array($data) ? $data : [];
}

// ── Configuration ────────────────────────────────────────────────────────────
//
// Настройки подключения к игровой БД и правила гостевого доступа живут в
// SQLite (api/store.php), по строке на параметр. Функции ниже сохраняют
// прежний контракт: массив конфигурации, чтение/запись целиком и
// read-modify-write под блокировкой.

/** @return array<string, mixed> */
function load_cfg(): array
{
    try {
        $cfg = store_settings_load();
        // Гостевые аккаунты — отдельная таблица, но код выше ожидает их в конфиге.
        $cfg['guest_users'] = store_guest_users_all();
        return $cfg;
    } catch (Throwable $e) {
        editor_log_error($e);
        return [];
    }
}

function save_cfg(array $cfg): bool
{
    try {
        return store_settings_save($cfg);
    } catch (Throwable $e) {
        editor_log_error($e);
        return false;
    }
}

/**
 * Read-modify-write конфигурации в одной транзакции SQLite.
 * $fn получает текущий конфиг и возвращает новый (или null, чтобы не писать).
 *
 * Чтение здесь намеренно НЕ прощает ошибки (в отличие от load_cfg): если базу
 * не удалось прочитать, сохранение отменяется целиком. Иначе $fn получил бы
 * пустой конфиг и store_settings_save стёр бы из таблицы settings все ключи,
 * которых нет в частичной записи — настройки «сбрасывались» бы сами собой.
 */
function mutate_cfg(callable $fn): bool
{
    try {
        return (bool)store_tx(function () use ($fn): bool {
            $cfg = store_settings_load();
            // Гостевые аккаунты — отдельная таблица, но код выше ожидает их в конфиге.
            $cfg['guest_users'] = store_guest_users_all();
            $next = $fn($cfg);
            if (!is_array($next)) {
                return false;
            }
            unset($next['guest_users']);
            return store_settings_save($next);
        });
    } catch (Throwable $e) {
        editor_log_error($e);
        return false;
    }
}

// ── Login throttling (shared state) ─────────────────────────────────────────
//
// Brute force protection lives in SQLite (auth_throttle), not in $_SESSION:
// a session counter is trivially reset by dropping the session cookie.
// Two scopes are tracked:
//   * login-ip      — every failed attempt from one address (protects shared logins);
//   * login-account — address + login (a stranger cannot lock out a guest account
//                     by guessing its login from another IP).

/**
 * Read-modify-write of the throttle table in one transaction.
 * $fn(array $map, int $now) may return ['map' => array, 'result' => mixed];
 * returning null keeps the store untouched.
 */
function auth_throttle_mutate(callable $fn): mixed
{
    try {
        return store_throttle_mutate(function (array $map, int $now) use ($fn) {
            foreach ($map as $key => $row) {
                // Drop only stale records: an unlocked counter (until = 0) stays alive
                // until its own window has long passed, otherwise the counter resets.
                $until = is_array($row) ? (int)($row['until'] ?? 0) : 0;
                $first = is_array($row) ? (int)($row['first'] ?? 0) : 0;
                if (!is_array($row) || ($until < $now - 3600 && $first < $now - 86400)) {
                    unset($map[$key]);
                }
            }
            // Never let the table grow forever on a hostile host.
            if (count($map) > 800) {
                uasort($map, fn($a, $b) => (int)($b['first'] ?? 0) <=> (int)($a['first'] ?? 0));
                $map = array_slice($map, 0, 400, true);
            }
            $out = $fn($map, $now);
            // Контракт store_throttle_mutate: ['map' => …, 'result' => …] или null.
            return is_array($out) ? $out : null;
        });
    } catch (Throwable $e) {
        editor_log_error($e);
        return null;
    }
}

function auth_throttle_key(string $scope, string $identity): string
{
    return hash('sha256', $scope . '|' . strtolower(trim($identity)));
}

/** Seconds left in the lock (0 = not locked). */
function auth_throttle_left(string $scope, string $identity): int
{
    $key = auth_throttle_key($scope, $identity);
    $result = auth_throttle_mutate(function (array $map) use ($key) {
        $row = $map[$key] ?? null;
        if (!is_array($row)) {
            return null;
        }
        $left = max(0, (int)($row['until'] ?? 0) - time());
        return ['map' => $map, 'result' => $left];
    });
    return (int)($result ?? 0);
}

/**
 * Register a failed attempt; returns seconds the caller must wait (0 when the
 * limit is not reached yet and the attempt may simply be rejected as usual).
 */
function auth_throttle_fail(string $scope, string $identity, int $maxAttempts, int $window, int $lockSeconds): int
{
    $key = auth_throttle_key($scope, $identity);
    $result = auth_throttle_mutate(function (array $map) use ($key, $maxAttempts, $window, $lockSeconds) {
        $now = time();
        $row = $map[$key] ?? ['n' => 0, 'first' => $now, 'until' => 0];
        if (!is_array($row)) {
            $row = ['n' => 0, 'first' => $now, 'until' => 0];
        }
        if ((int)($row['until'] ?? 0) > $now) {
            return ['map' => $map, 'result' => (int)$row['until'] - $now];
        }
        if ($now - (int)($row['first'] ?? 0) > $window) {
            $row = ['n' => 0, 'first' => $now, 'until' => 0];
        }
        $row['n'] = (int)($row['n'] ?? 0) + 1;
        $locked = 0;
        if ($row['n'] >= $maxAttempts) {
            $locked = $lockSeconds;
            $row['until'] = $now + $lockSeconds;
            $row['n'] = 0;
            $row['first'] = $now;
        }
        $map[$key] = $row;
        return ['map' => $map, 'result' => $locked];
    });
    return (int)($result ?? 0);
}

/** A successful login clears both counters of the identity. */
function auth_throttle_clear(string $scope, string $identity): void
{
    $key = auth_throttle_key($scope, $identity);
    auth_throttle_mutate(function (array $map) use ($key) {
        unset($map[$key]);
        return ['map' => $map];
    });
}

/** New unique id of a guest account. */
function guest_new_id(): string
{
    return 'g' . bin2hex(random_bytes(8));
}

// ── Session ──────────────────────────────────────────────────────────────────

function start_editor_session(): void
{
    if (session_status() === PHP_SESSION_ACTIVE) {
        return;
    }

    $isHttps = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off');

    // Do not accept session ids that were never issued by this server and never
    // fall back to ids in the URL.
    @ini_set('session.use_strict_mode', '1');
    @ini_set('session.use_only_cookies', '1');
    @ini_set('session.use_trans_sid', '0');

    session_set_cookie_params([
        'lifetime' => 0,
        'path'     => '/',
        'domain'   => '',
        'secure'   => $isHttps,
        'httponly'  => true,
        'samesite'  => 'Lax',
    ]);

    session_start();
}

// ── Roles ────────────────────────────────────────────────────────────────────
//
// Editor Studio knows two roles:
//   admin — full access (the original behaviour);
//   guest — read-only access to whitelisted editors. A guest can NEVER write
//           to the game database; it can only create change requests that an
//           administrator reviews and applies (see api/change-requests.php).
//
// Every endpoint is admin-only by default (bootstrap_editor_api(true)), so a
// guest session is rejected everywhere unless an endpoint explicitly opts in
// via bootstrap_editor_api_roles([...'guest']) AND checks the action.

const EDITOR_ROLE_ADMIN = 'admin';
const EDITOR_ROLE_GUEST = 'guest';

/** True when the current request came from a network that is allowed only for guests. */
$GLOBALS['EDITOR_NETWORK_GUEST_ONLY'] = false;

function clamp_int(mixed $value, int $min, int $max, int $default): int
{
    if (!is_numeric($value)) {
        return $default;
    }
    return max($min, min($max, (int)$value));
}

/** Normalised guest access settings (stored in data/db-config.json). */
function guest_cfg(?array $cfg = null): array
{
    $cfg ??= load_cfg();
    return [
        'enabled'        => (bool)($cfg['guest_enabled'] ?? false),
        'allow_remote'   => (bool)($cfg['guest_allow_remote'] ?? false),
        'max_pending'    => clamp_int($cfg['guest_max_pending'] ?? null, 1, 50, 5),
        'cooldown'       => clamp_int($cfg['guest_submit_cooldown'] ?? null, 0, 3600, 60),
        'max_operations' => clamp_int($cfg['guest_max_operations'] ?? null, 1, 500, 100),
        // Guest logins exist only as personal accounts created by an administrator.
        'users_count'    => count(array_filter(guest_users($cfg), fn($u) => $u['enabled'])),
    ];
}

// ── Guest accounts ───────────────────────────────────────────────────────────
//
// A guest is a personal account created by the administrator:
//   { id, login, name, enabled, pages[], passwordHash, sessionVersion, createdAt, updatedAt, lastLoginAt }
//
// "pages" is the list of editor tabs the administrator allowed for this guest
// (see editor_guest_page_catalog()). It is enforced on the server:
//   * api/db-crud.php        — reads only for resources of the allowed pages;
//   * api/change-requests.php— operations only for resources of the allowed pages.
//
// A guest still can NEVER write to the game database: its edits become change
// requests that an administrator reviews and applies (strict guest → admin → DB).

/** Editor tabs a guest account can be granted. 'write' = allowed in change requests, 'read' = extra reads. */
function editor_guest_page_catalog(): array
{
    static $catalog = null;
    if ($catalog !== null) {
        return $catalog;
    }
    $catalog = [
        'items'      => ['title' => 'Предметы',           'write' => ['items'],                 'read' => ['items']],
        'crafts'     => ['title' => 'Крафты',             'write' => ['crafts'],                'read' => ['crafts']],
        'warehouses' => ['title' => 'Склады',             'write' => ['warehouses'],            'read' => ['warehouses', 'items']],
        'worlds'     => ['title' => 'Миры',               'write' => ['worlds'],                'read' => ['worlds']],
        'aethers'    => ['title' => 'Телепорты (Aether)', 'write' => ['aethers'],               'read' => ['aethers']],
        'dungeons'   => ['title' => 'Подземелья',         'write' => ['dungeons'],              'read' => ['dungeons']],
        'quests'     => ['title' => 'Квесты',             'write' => ['quests', 'quest_bots'],  'read' => ['quests', 'quest_bots']],
        'bots_info'  => ['title' => 'Боты',               'write' => ['bots_info'],             'read' => ['bots_info']],
        'bots_mobs'  => ['title' => 'Мобы',               'write' => ['bots_mobs'],             'read' => ['bots_mobs']],
        'bots_npc'   => ['title' => 'NPC',                'write' => ['bots_npc'],              'read' => ['bots_npc']],
    ];
    return $catalog;
}

function editor_guest_page_keys(): array
{
    return array_keys(editor_guest_page_catalog());
}

/** Allowed page keys → write resources. */
function editor_guest_write_resources(array $pages): array
{
    $catalog = editor_guest_page_catalog();
    $out = [];
    foreach ($pages as $page) {
        foreach (($catalog[$page]['write'] ?? []) as $resource) {
            $out[$resource] = true;
        }
    }
    return array_keys($out);
}

/** Allowed page keys → readable resources (writes + extra lookups). */
function editor_guest_read_resources(array $pages): array
{
    $catalog = editor_guest_page_catalog();
    $out = [];
    foreach ($pages as $page) {
        foreach (array_merge($catalog[$page]['write'] ?? [], $catalog[$page]['read'] ?? []) as $resource) {
            $out[$resource] = true;
        }
    }
    return array_keys($out);
}

/** Normalise one stored guest account; null when the record is unusable. */
function guest_user_normalize(mixed $raw): ?array
{
    if (!is_array($raw)) {
        return null;
    }
    $id = strtolower(trim((string)($raw['id'] ?? '')));
    $login = strtolower(trim((string)($raw['login'] ?? '')));
    if (preg_match('/^g[a-z0-9]{7,31}$/', $id) !== 1) {
        return null;
    }
    if (preg_match('/^[a-z0-9_.-]{3,32}$/', $login) !== 1) {
        return null;
    }
    $pageKeys = editor_guest_page_keys();
    $rawPages = is_array($raw['pages'] ?? null) ? array_map('strval', $raw['pages']) : [];
    $pages = array_values(array_intersect($pageKeys, $rawPages));

    $name = trim((string)($raw['name'] ?? ''));
    if ($name === '') {
        $name = $login;
    }

    return [
        'id'             => $id,
        'login'          => $login,
        'name'           => function_exists('mb_substr') ? mb_substr($name, 0, 48, 'UTF-8') : substr($name, 0, 48),
        'enabled'        => (bool)($raw['enabled'] ?? false),
        'pages'          => $pages,
        'passwordHash'   => (string)($raw['passwordHash'] ?? ''),
        'sessionVersion' => max(1, (int)($raw['sessionVersion'] ?? 1)),
        'createdAt'      => (string)($raw['createdAt'] ?? ''),
        'updatedAt'      => (string)($raw['updatedAt'] ?? ''),
        'lastLoginAt'    => (string)($raw['lastLoginAt'] ?? ''),
    ];
}

/** @return array<string, array> guest id => normalised account */
function guest_users(?array $cfg = null): array
{
    $cfg ??= load_cfg();
    $raw = $cfg['guest_users'] ?? [];
    if (!is_array($raw)) {
        return [];
    }
    $out = [];
    foreach ($raw as $item) {
        $user = guest_user_normalize($item);
        if ($user !== null && !isset($out[$user['id']])) {
            $out[$user['id']] = $user;
        }
    }
    return $out;
}

function guest_user_by_id(?array $cfg, string $id): ?array
{
    return guest_users($cfg)[strtolower(trim($id))] ?? null;
}

function guest_user_by_login(?array $cfg, string $login): ?array
{
    $login = strtolower(trim($login));
    if ($login === '') {
        return null;
    }
    foreach (guest_users($cfg) as $user) {
        if ($user['login'] === $login) {
            return $user;
        }
    }
    return null;
}

/** Public (safe) representation — never exposes the password hash. */
function guest_user_public(array $user): array
{
    return [
        'id'          => $user['id'],
        'login'       => $user['login'],
        'name'        => $user['name'],
        'enabled'     => $user['enabled'],
        'pages'       => $user['pages'],
        'createdAt'   => $user['createdAt'],
        'updatedAt'   => $user['updatedAt'],
        'lastLoginAt' => $user['lastLoginAt'],
    ];
}

/**
 * Session fingerprint of an account. A password reset, "revoke sessions" or a
 * disabled account immediately invalidates existing guest sessions.
 */
function guest_user_epoch(array $user): string
{
    return substr(hash('sha256', 'guest-account|' . $user['id'] . '|' . $user['passwordHash'] . '|' . $user['sessionVersion']), 0, 24);
}

function clear_guest_session(): void
{
    unset(
        $_SESSION['editor_studio_guest'],
        $_SESSION['editor_studio_guest_key'],
        $_SESSION['editor_studio_guest_epoch'],
        $_SESSION['editor_studio_guest_user'],
        $_SESSION['editor_studio_guest_last_submit']
    );
}

/** Current guest account of the session (validated against the config). */
function editor_guest_user(?array $cfg = null): ?array
{
    if (session_status() !== PHP_SESSION_ACTIVE) {
        return null;
    }
    if (!isset($_SESSION['editor_studio_guest']) || $_SESSION['editor_studio_guest'] !== true) {
        return null;
    }
    $guest = guest_cfg($cfg);
    $user = guest_user_by_id($cfg, (string)($_SESSION['editor_studio_guest_user'] ?? ''));
    if (!$guest['enabled'] || $user === null || !$user['enabled']) {
        clear_guest_session();
        return null;
    }
    $epoch = (string)($_SESSION['editor_studio_guest_epoch'] ?? '');
    if ($epoch === '' || !hash_equals(guest_user_epoch($user), $epoch)) {
        clear_guest_session();
        return null;
    }
    return $user;
}

/** Pages of the current guest account (empty for non-guests). */
function editor_guest_pages(?array $cfg = null): array
{
    return editor_guest_user($cfg)['pages'] ?? [];
}

/** Resources the current guest may read (empty for non-guests). */
function editor_guest_read_resources_current(?array $cfg = null): array
{
    return editor_guest_read_resources(editor_guest_pages($cfg));
}

/** Resources the current guest may request changes for (empty for non-guests). */
function editor_guest_write_resources_current(?array $cfg = null): array
{
    return editor_guest_write_resources(editor_guest_pages($cfg));
}

function guest_can_write_resource(string $resource, ?array $cfg = null): bool
{
    return in_array($resource, editor_guest_write_resources_current($cfg), true);
}

function guest_can_read_resource(string $resource, ?array $cfg = null): bool
{
    return in_array($resource, editor_guest_read_resources_current($cfg), true);
}

/** Returns 'admin', 'guest' or null for the current session. */
function editor_session_role(?array $cfg = null): ?string
{
    if (session_status() !== PHP_SESSION_ACTIVE) {
        return null;
    }
    if (isset($_SESSION['editor_studio_auth']) && $_SESSION['editor_studio_auth'] === true) {
        return EDITOR_ROLE_ADMIN;
    }
    if (isset($_SESSION['editor_studio_guest']) && $_SESSION['editor_studio_guest'] === true) {
        // Validates: guest access enabled, account exists/enabled, epoch matches.
        return editor_guest_user($cfg) !== null ? EDITOR_ROLE_GUEST : null;
    }
    return null;
}

/** Stable random identifier of the guest (used to show "my requests"). */
function editor_guest_key(): string
{
    $key = (string)($_SESSION['editor_studio_guest_key'] ?? '');
    return preg_match('/^[a-f0-9]{48}$/', $key) === 1 ? $key : '';
}

function editor_network_guest_only(): bool
{
    return (bool)($GLOBALS['EDITOR_NETWORK_GUEST_ONLY'] ?? false);
}

function ensure_editor_auth(): void
{
    start_editor_session();
    ensure_editor_role([EDITOR_ROLE_ADMIN]);
}

/**
 * Make sure the current session has one of the allowed roles.
 * 401 → not logged in (the shell shows the login dialog),
 * 403 → logged in, but the role is not allowed here (guest in admin area).
 */
function ensure_editor_role(array $roles): string
{
    $role = editor_session_role();
    if ($role === EDITOR_ROLE_ADMIN && editor_network_guest_only()) {
        // Admin session cookie used from a network allowed only for guests.
        $role = null;
    }
    if ($role === null) {
        respond(['ok' => false, 'error' => 'Требуется вход в Editor Studio'], 401);
    }
    if (!in_array($role, $roles, true)) {
        respond([
            'ok' => false,
            'error' => 'Доступ запрещён: этот раздел недоступен для гостевого аккаунта.',
            'code' => 'guest_forbidden',
        ], 403);
    }
    return $role;
}

/** Guests may only use explicitly whitelisted actions of an endpoint. */
function guest_action_guard(string $role, string $action, array $allowedActions): void
{
    if ($role === EDITOR_ROLE_GUEST && !in_array($action, $allowedActions, true)) {
        respond([
            'ok' => false,
            'error' => 'Гостевой аккаунт не может выполнять это действие. Создайте запрос на изменение.',
            'code' => 'guest_forbidden',
        ], 403);
    }
}

/**
 * CSRF protection for state-changing requests: body must be JSON (cannot be
 * sent cross-site without a CORS preflight) and Origin, when present, must
 * match the host.
 */
function require_same_origin_json_post(): void
{
    $method = strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET'));
    if ($method !== 'POST') {
        respond(['ok' => false, 'error' => 'Метод запроса не поддерживается'], 405);
    }
    $ctype = strtolower((string)($_SERVER['CONTENT_TYPE'] ?? $_SERVER['HTTP_CONTENT_TYPE'] ?? ''));
    if (!str_starts_with($ctype, 'application/json')) {
        respond(['ok' => false, 'error' => 'Ожидается JSON-запрос'], 415);
    }
    $origin = (string)($_SERVER['HTTP_ORIGIN'] ?? '');
    if ($origin !== '' && $origin !== 'null') {
        $originHost = strtolower((string)parse_url($origin, PHP_URL_HOST));
        $originPort = parse_url($origin, PHP_URL_PORT);
        if ($originPort !== null && $originPort !== false) {
            $originHost .= ':' . $originPort;
        }
        $hosts = array_filter([
            strtolower((string)($_SERVER['HTTP_HOST'] ?? '')),
            strtolower(trim(explode(',', (string)($_SERVER['HTTP_X_FORWARDED_HOST'] ?? ''))[0])),
        ]);
        $matches = false;
        foreach ($hosts as $host) {
            if ($host === $originHost || preg_replace('/:\d+$/', '', $host) === preg_replace('/:\d+$/', '', $originHost)) {
                $matches = true;
                break;
            }
        }
        if (!$matches) {
            respond(['ok' => false, 'error' => 'Запрос с постороннего сайта отклонён'], 403);
        }
    }
}

/**
 * Защита операций, которые меняют данные.
 *
 * Читающие действия (списки, конфигурация) остаются доступны GET-ом, а запись
 * принимается только как JSON-POST с этого же узла — так запрос нельзя подделать
 * с постороннего сайта.
 */
function guard_write_actions(string $action, array $writeActions): void
{
    if (in_array($action, $writeActions, true)) {
        require_same_origin_json_post();
    }
}

// ── Network / IP helpers ─────────────────────────────────────────────────────

function get_request_ip(): string
{
    // REMOTE_ADDR is the most trustworthy; proxy headers checked as fallback
    $candidates = [
        (string)($_SERVER['REMOTE_ADDR'] ?? ''),
        trim((string)($_SERVER['HTTP_X_FORWARDED_FOR'] ?? '')),
        trim((string)($_SERVER['HTTP_X_REAL_IP'] ?? '')),
    ];

    foreach ($candidates as $candidate) {
        if ($candidate === '') {
            continue;
        }
        // X-Forwarded-For may contain a chain: take the first valid IP
        $parts = array_map('trim', explode(',', $candidate));
        foreach ($parts as $part) {
            if (filter_var($part, FILTER_VALIDATE_IP)) {
                return $part;
            }
        }
    }

    return '';
}

function ip_is_loopback(string $ip): bool
{
    if ($ip === '') {
        return false;
    }
    return $ip === '127.0.0.1' || $ip === '::1';
}

function ip_in_cidrs(string $ip, array $cidrs): bool
{
    if ($ip === '' || $cidrs === []) {
        return false;
    }

    $ipBin = @inet_pton($ip);
    if ($ipBin === false) {
        return false;
    }
    $ipLen = strlen($ipBin);

    foreach ($cidrs as $raw) {
        $cidr = trim((string)$raw);
        if ($cidr === '') {
            continue;
        }

        // Plain IP (no prefix)
        if (!str_contains($cidr, '/')) {
            if (hash_equals($cidr, $ip)) {
                return true;
            }
            continue;
        }

        [$base, $prefix] = explode('/', $cidr, 2);
        $prefixNum = (int)$prefix;

        $baseBin = @inet_pton($base);
        if ($baseBin === false || strlen($baseBin) !== $ipLen) {
            continue;
        }

        $totalBits = $ipLen * 8;
        if ($prefixNum < 0 || $prefixNum > $totalBits) {
            continue;
        }

        $fullBytes    = intdiv($prefixNum, 8);
        $remainBits   = $prefixNum % 8;

        // Compare full bytes
        if (substr($ipBin, 0, $fullBytes) !== substr($baseBin, 0, $fullBytes)) {
            continue;
        }

        // Compare remaining bits in the boundary byte
        if ($remainBits > 0) {
            $mask = 0xFF << (8 - $remainBits);
            if ((ord($ipBin[$fullBytes]) & $mask) !== (ord($baseBin[$fullBytes]) & $mask)) {
                continue;
            }
        }

        return true;
    }

    return false;
}

/** Returns null when the request IP passes the admin network policy, otherwise an error message. */
function network_policy_error(array $cfg): ?string
{
    $allowRemote = (bool)($cfg['allow_remote_editor'] ?? false);

    if ($allowRemote) {
        $allowed = $cfg['allowed_editor_ips'] ?? [];
        if (!is_array($allowed) || $allowed === []) {
            // Remote allowed but no IP whitelist → open access
            return null;
        }
        $ip = get_request_ip();
        return ip_in_cidrs($ip, $allowed) ? null : 'Доступ с этого IP запрещён';
    }

    // Local-only mode
    return ip_is_loopback(get_request_ip()) ? null : 'API редактора доступен только с localhost';
}

function enforce_network_policy(array $cfg): void
{
    $error = network_policy_error($cfg);
    if ($error !== null) {
        respond(['ok' => false, 'error' => $error], 403);
    }
}

// ── Bootstrap ────────────────────────────────────────────────────────────────

/**
 * Admin-only bootstrap (unchanged semantics for every existing endpoint).
 * With $requireAuth=false only the session is started (used by auth.php).
 */
function bootstrap_editor_api(bool $requireAuth = true): void
{
    bootstrap_editor_api_roles($requireAuth ? [EDITOR_ROLE_ADMIN] : null);
}

/**
 * Role-aware bootstrap.
 *  - $roles === null : no auth required, returns current role (or null)
 *  - otherwise       : session must have one of $roles
 *
 * Network: requests that fail the admin IP policy are accepted only when guest
 * remote access is enabled, and then ONLY for guest sessions.
 */
function bootstrap_editor_api_roles(?array $roles): ?string
{
    apply_security_headers();

    $cfg = load_cfg();
    $networkError = network_policy_error($cfg);
    $guest = guest_cfg($cfg);

    if ($networkError !== null) {
        $guestNetworkAllowed = $guest['enabled'] && $guest['users_count'] > 0 && $guest['allow_remote']
            && ($roles === null || in_array(EDITOR_ROLE_GUEST, $roles, true));
        if (!$guestNetworkAllowed) {
            respond(['ok' => false, 'error' => $networkError], 403);
        }
        $GLOBALS['EDITOR_NETWORK_GUEST_ONLY'] = true;
    }

    start_editor_session();

    if ($roles === null) {
        $role = editor_session_role($cfg);
        if ($role === EDITOR_ROLE_ADMIN && editor_network_guest_only()) {
            return null;
        }
        return $role;
    }

    return ensure_editor_role($roles);
}

// ── HTTP endpoint testing ────────────────────────────────────────────────────

/**
 * Локальные и служебные адреса, к которым сервер не должен ходить по просьбе
 * браузера: иначе проверка соединения превращается в доступ к внутренней сети
 * (SSRF) — например, к 127.0.0.1 или к порталу облачных метаданных.
 */
function editor_host_is_internal(string $host): bool
{
    $host = strtolower(trim($host));
    if ($host === '' || $host === 'localhost' || str_ends_with($host, '.localhost')) {
        return true;
    }
    if (!filter_var($host, FILTER_VALIDATE_IP)) {
        // Имя может разрешиться в приватный адрес: проверяем и результат.
        $resolved = gethostbyname($host);
        if ($resolved === $host) {
            return false; // не разрешается — подключение всё равно не состоится
        }
        $host = $resolved;
    }
    return filter_var(
        $host,
        FILTER_VALIDATE_IP,
        FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE
    ) === false;
}

function test_http_endpoint(string $url): array
{
    if (!filter_var($url, FILTER_VALIDATE_URL)) {
        return ['ok' => false, 'error' => 'Некорректный адрес'];
    }
    if (editor_host_is_internal((string)parse_url($url, PHP_URL_HOST))) {
        return ['ok' => false, 'error' => 'Адреса во внутренней сети недоступны для проверки'];
    }

    // Restrict to http(s) schemes only
    $scheme = parse_url($url, PHP_URL_SCHEME);
    if (!in_array(strtolower((string)$scheme), ['http', 'https'], true)) {
        return ['ok' => false, 'error' => 'Разрешены только адреса http и https'];
    }

    if (!function_exists('curl_init')) {
        return ['ok' => false, 'error' => 'На сервере не установлено расширение cURL'];
    }

    $ch = curl_init($url);
    if ($ch === false) {
        return ['ok' => false, 'error' => 'Не удалось запустить cURL'];
    }

    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true,
        // Переадресация могла бы увести запрос во внутреннюю сеть — не идём по ней.
        CURLOPT_FOLLOWLOCATION => false,
        CURLOPT_TIMEOUT        => 8,
        CURLOPT_CONNECTTIMEOUT => 3,
        CURLOPT_USERAGENT      => 'EditorStudio/1.0',
        CURLOPT_PROTOCOLS      => CURLPROTO_HTTP | CURLPROTO_HTTPS,
    ]);

    curl_exec($ch);
    $err  = curl_error($ch);
    $code = (int)curl_getinfo($ch, CURLINFO_RESPONSE_CODE);
    curl_close($ch);

    if ($err !== '') {
        return ['ok' => false, 'error' => $err];
    }
    if ($code < 200 || $code >= 400) {
        return ['ok' => false, 'error' => 'HTTP status ' . $code, 'status' => $code];
    }
    return ['ok' => true, 'status' => $code];
}

/**
 * Запрос к мосту карт (map-api) — общий для настроек (проверка) и для
 * api/map.php (сама отдача файлов).
 *
 * Мост — это отдельный сервис рядом с игровым сервером, который отдаёт .map и
 * текстуры mapres по тому же контракту, что и api/map.php:
 *   ?action=status                 → {"ok":true,"source":"…"}
 *   ?action=file&world=<ID>        → .map
 *   ?action=file&path=<путь>       → .map (если мир неизвестен редактору)
 *   ?action=asset&name=<имя>       → .png
 *
 * Ограничения безопасности: только http/https, без переадресаций (иначе
 * запрос можно увести в чужую сеть), таймаут и лимит размера ответа.
 *
 * Возвращает ['ok'=>bool, 'status'=>int, 'body'=>string, 'error'=>string,
 *              'headers'=>array<string,string>].
 */
function map_api_request(string $url, string $token = '', int $maxBytes = 67108864)
{
    $result = ['ok' => false, 'status' => 0, 'body' => '', 'error' => '', 'headers' => []];

    $scheme = strtolower((string)parse_url($url, PHP_URL_SCHEME));
    if (!in_array($scheme, ['http', 'https'], true)) {
        $result['error'] = 'Разрешены только адреса http и https';
        return $result;
    }
    if (!filter_var($url, FILTER_VALIDATE_URL)) {
        $result['error'] = 'Некорректный адрес map-api';
        return $result;
    }

    $headers = ['Accept: */*'];
    if ($token !== '') {
        // Два распространённых способа: Bearer и собственный заголовок.
        $headers[] = 'Authorization: Bearer ' . $token;
        $headers[] = 'X-Editor-Token: ' . $token;
    }

    if (function_exists('curl_init')) {
        $ch = curl_init($url);
        if ($ch === false) {
            $result['error'] = 'Не удалось запустить cURL';
            return $result;
        }

        $responseHeaders = [];
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER   => true,
            CURLOPT_FOLLOWLOCATION   => false,
            CURLOPT_TIMEOUT          => 20,
            CURLOPT_CONNECTTIMEOUT   => 5,
            CURLOPT_USERAGENT        => 'EditorStudio/1.0',
            CURLOPT_HTTPHEADER       => $headers,
            CURLOPT_PROTOCOLS        => CURLPROTO_HTTP | CURLPROTO_HTTPS,
            CURLOPT_MAXFILESIZE      => $maxBytes,
            CURLOPT_HEADERFUNCTION   => static function ($ch2, string $line) use (&$responseHeaders): int {
                $parts = explode(':', $line, 2);
                if (count($parts) === 2) {
                    $responseHeaders[strtolower(trim($parts[0]))] = trim($parts[1]);
                }
                return strlen($line);
            },
        ]);

        $body = curl_exec($ch);
        $err  = curl_error($ch);
        $code = (int)curl_getinfo($ch, CURLINFO_RESPONSE_CODE);
        curl_close($ch);

        if ($body === false || $err !== '') {
            $result['error'] = $err !== '' ? $err : 'map-api не ответил';
            return $result;
        }
        if (strlen((string)$body) > $maxBytes) {
            $result['error'] = 'Ответ map-api слишком большой';
            return $result;
        }

        $result['status']  = $code;
        $result['body']    = (string)$body;
        $result['headers'] = $responseHeaders;
        $result['ok']      = $code >= 200 && $code < 300;
        if (!$result['ok']) {
            $result['error'] = 'map-api: HTTP ' . $code;
        }
        return $result;
    }

    // Без cURL: обычный поток. Отключаем переадресации и ограничиваем время.
    $context = stream_context_create([
        'http' => [
            'method'          => 'GET',
            'header'          => implode("\r\n", $headers),
            'timeout'         => 20,
            'follow_location' => 0,
            'user_agent'      => 'EditorStudio/1.0',
        ],
        'ssl' => ['verify_peer' => true, 'verify_peer_name' => true],
    ]);

    $body = @file_get_contents($url, false, $context, 0, $maxBytes);
    if ($body === false) {
        $result['error'] = 'map-api недоступен (нет cURL и потоковый запрос не удался)';
        return $result;
    }

    $code = 0;
    $responseHeaders = [];
    foreach ($http_response_header ?? [] as $line) {
        if (preg_match('#^HTTP/\S+\s+(\d{3})#', $line, $m)) {
            $code = (int)$m[1];
            $responseHeaders = [];
            continue;
        }
        $parts = explode(':', $line, 2);
        if (count($parts) === 2) {
            $responseHeaders[strtolower(trim($parts[0]))] = trim($parts[1]);
        }
    }

    $result['status']  = $code;
    $result['body']    = $body;
    $result['headers'] = $responseHeaders;
    $result['ok']      = $code >= 200 && $code < 300;
    if (!$result['ok']) {
        $result['error'] = 'map-api: HTTP ' . $code;
    }
    return $result;
}

// ── Database connection ──────────────────────────────────────────────────────

function db_connect_server(): mysqli
{
    $cfg  = load_cfg();
    $host = (string)($cfg['host']     ?? '127.0.0.1');
    $port = (int)($cfg['port']        ?? 3306);
    $user = (string)($cfg['user']     ?? 'root');
    $pass = (string)($cfg['password'] ?? '');

    mysqli_report(MYSQLI_REPORT_OFF);

    $mysqli = @new mysqli($host, $user, $pass, '', $port);
    if ($mysqli->connect_errno) {
        throw new RuntimeException('DB connection failed: ' . $mysqli->connect_error);
    }
    $mysqli->set_charset('utf8mb4');
    return $mysqli;
}

function db_connect(): mysqli
{
    $cfg = load_cfg();
    $db  = (string)($cfg['database'] ?? '');

    $mysqli = db_connect_server();
    if ($db !== '' && !$mysqli->select_db($db)) {
        $error = $mysqli->error;
        $mysqli->close();
        throw new RuntimeException('Не удалось выбрать базу данных: ' . $error);
    }
    return $mysqli;
}