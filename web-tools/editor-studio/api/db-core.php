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
    header('Permissions-Policy: camera=(), microphone=(), geolocation=(), payment=()');
    header('X-Permitted-Cross-Domain-Policies: none');
    header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
    header('Pragma: no-cache');
}

function respond(array $payload, int $code = 200): never
{
    http_response_code($code);
    echo json_encode($payload, JSON_UNESCAPED_UNICODE | JSON_THROW_ON_ERROR);
    exit;
}

/** Единый ответ «слишком часто»: заголовок Retry-After + машинные поля для UI. */
function respond_rate_limited(int $retryAfter, string $message): never
{
    $retryAfter = max(1, $retryAfter);
    header('Retry-After: ' . $retryAfter);
    respond([
        'ok' => false,
        'error' => $message,
        'code' => 'rate_limited',
        'retry_after' => $retryAfter,
    ], 429);
}

/**
 * Общая политика паролей (гостевые аккаунты; для администратора — рекомендация
 * при инициализации). Возвращает текст ошибки или null.
 *
 * Максимум 72 БАЙТА — граница bcrypt (PASSWORD_DEFAULT): более длинные пароли
 * password_hash молча обрезает, и два разных пароля с общим префиксом в 72
 * байта стали бы одинаковыми.
 */
function editor_password_policy_error(string $password, int $minLength = 10): ?string
{
    $bytes = strlen($password);
    if ($bytes > 72) {
        return 'Пароль слишком длинный (максимум 72 байта в UTF-8).';
    }
    $chars = function_exists('mb_strlen') ? mb_strlen($password, 'UTF-8') : $bytes;
    if ($chars < $minLength) {
        return sprintf('Пароль слишком короткий: минимум %d символов.', $minLength);
    }
    if (preg_match('//u', $password) !== 1) {
        return 'Пароль содержит некорректные символы (ожидается UTF-8).';
    }
    return null;
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
    // Read one byte past the limit so oversized bodies cannot be silently
    // truncated into valid JSON (or accepted as an empty/partial request).
    $maxBytes = 4 * 1024 * 1024;
    $raw = editor_request_raw_body($maxBytes);
    if (strlen($raw) > $maxBytes) {
        respond(['ok' => false, 'error' => 'Тело запроса слишком большое'], 413);
    }
    if ($raw === '') {
        return [];
    }
    $data = json_decode($raw, true);
    if (json_last_error() !== JSON_ERROR_NONE || !is_array($data)) {
        respond(['ok' => false, 'error' => 'Некорректный JSON'], 400);
    }
    return $data;
}

// ── Configuration ────────────────────────────────────────────────────────────
//
// Настройки подключения к игровой БД и правила гостевого доступа живут в
// SQLite (api/store.php), по строке на параметр. Функции ниже сохраняют
// прежний контракт: массив конфигурации, чтение/запись целиком и
// read-modify-write под блокировкой.

/** @return array<string, mixed> */
function load_cfg(bool $withGuests = true): array
{
    try {
        $cfg = store_settings_load();
        // DB connections do not need to scan guest accounts on every request.
        if ($withGuests) $cfg['guest_users'] = store_guest_users_all();
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

function auth_throttle_key(string $scope, string $identity): string
{
    return hash('sha256', $scope . '|' . strtolower(trim($identity)));
}

/** Seconds left in the lock (0 = not locked). */
function auth_throttle_left(string $scope, string $identity): int
{
    try {
        $row = store_throttle_get(auth_throttle_key($scope, $identity));
        return max(0, (int)($row['until'] ?? 0) - time());
    } catch (Throwable $e) {
        editor_log_error($e);
        return 0;
    }
}

/**
 * Register a failed attempt under BEGIN IMMEDIATE. Parallel requests cannot
 * lose increments, while only one indexed row is read and written.
 */
function auth_throttle_fail(string $scope, string $identity, int $maxAttempts, int $window, int $lockSeconds): int
{
    $key = auth_throttle_key($scope, $identity);
    try {
        return (int)store_tx(function () use ($key, $maxAttempts, $window, $lockSeconds): int {
            $now = time();
            store_throttle_prune($now);
            $row = store_throttle_get($key) ?? ['n' => 0, 'first' => $now, 'until' => 0];
            if ((int)$row['until'] > $now) {
                return (int)$row['until'] - $now;
            }
            if ($now - (int)$row['first'] > $window) {
                $row = ['n' => 0, 'first' => $now, 'until' => 0];
            }

            $row['n'] = (int)$row['n'] + 1;
            $locked = 0;
            if ($row['n'] >= $maxAttempts) {
                $locked = $lockSeconds;
                $row['until'] = $now + $lockSeconds;
                $row['n'] = 0;
                $row['first'] = $now;
            }
            store_throttle_set($key, $row);
            return $locked;
        });
    } catch (Throwable $e) {
        editor_log_error($e);
        return 0;
    }
}

/** A successful login clears both counters of the identity. */
function auth_throttle_clear(string $scope, string $identity): void
{
    try {
        store_throttle_clear(auth_throttle_key($scope, $identity));
    } catch (Throwable $e) {
        editor_log_error($e);
    }
}

// ── Generic rate limiting ────────────────────────────────────────────────────
//
// Та же механика счётчиков, что и для входа, но для любых дорогих/шумных
// действий (анализ запроса, переписка, отправка). Состояние — в SQLite,
// а не в сессии: сброс cookie лимит не обходит.

/** Секунд до снятия блокировки (0 — действие разрешено). */
function editor_rate_limit_left(string $scope, string $identity): int
{
    return auth_throttle_left('rate:' . $scope, $identity);
}

/**
 * Учитывает одно действие; возвращает секунды блокировки, если лимит
 * ($maxAttempts действий за $window секунд) исчерпан, иначе 0.
 */
function editor_rate_limit_hit(string $scope, string $identity, int $maxAttempts, int $window, int $lockSeconds): int
{
    return auth_throttle_fail('rate:' . $scope, $identity, $maxAttempts, $window, $lockSeconds);
}

/**
 * Удобная проверка «можно ли прямо сейчас»: при блокировке отвечает 429
 * с Retry-After, иначе учитывает действие и продолжает выполнение.
 */
function editor_rate_limit_guard(string $scope, string $identity, int $maxAttempts, int $window, int $lockSeconds, string $message): void
{
    $left = editor_rate_limit_left($scope, $identity);
    if ($left > 0) {
        respond_rate_limited($left, $message);
    }
    $locked = editor_rate_limit_hit($scope, $identity, $maxAttempts, $window, $lockSeconds);
    if ($locked > 0) {
        respond_rate_limited($locked, $message);
    }
}

// ── Audit log ────────────────────────────────────────────────────────────────

/**
 * Запись в журнал аудита от имени текущего актора сессии.
 * Ошибки журнала никогда не ломают саму операцию — только уходят в лог.
 */
function editor_audit(string $action, string $requestId = '', array $detail = []): void
{
    try {
        if (session_status() === PHP_SESSION_ACTIVE && !empty($_SESSION['editor_studio_auth'])) {
            $role = EDITOR_ROLE_ADMIN;
            $actorId = (string)($_SESSION['editor_studio_user'] ?? 'admin');
            $actorName = $actorId;
        } elseif (session_status() === PHP_SESSION_ACTIVE && !empty($_SESSION['editor_studio_guest'])) {
            $account = editor_guest_user();
            $role = EDITOR_ROLE_GUEST;
            $actorId = (string)($account['id'] ?? ($_SESSION['editor_studio_guest_user'] ?? ''));
            $actorName = (string)($account['name'] ?? $account['login'] ?? 'guest');
        } else {
            $role = 'anonymous';
            $actorId = '';
            $actorName = '';
        }
        store_audit_add($role, $actorId, $actorName, $action, $requestId, $detail, get_request_ip());
    } catch (Throwable $e) {
        editor_log_error($e);
    }
}

/** New unique id of a guest account. */
function guest_new_id(): string
{
    return 'g' . bin2hex(random_bytes(8));
}

// ── Session ──────────────────────────────────────────────────────────────────
//
// Жизненный цикл сессии ограничен двумя сроками (настройки в хранилище,
// ключи session_idle_seconds / session_absolute_seconds):
//   * idle     — бездействие: сессия умирает, если ею не пользовались дольше
//                срока (по умолчанию 12 часов — рабочий день редактора);
//   * absolute — полный срок жизни от входа (по умолчанию 7 дней): украденная
//                cookie не живёт вечно, даже при постоянной активности.
// Периодическая ротация ID намеренно НЕ делается: студия держит несколько
// параллельных запросов из iframe, и ротация под нагрузкой обрывала бы часть
// из них (строгий режим сессий). ID меняется при входе/выходе и при истечении.

const EDITOR_SESSION_IDLE_DEFAULT = 43200;      // 12 часов
const EDITOR_SESSION_ABSOLUTE_DEFAULT = 604800; // 7 дней

$GLOBALS['EDITOR_SESSION_EXPIRED'] = false;

/** Путь cookie: каталог студии, а не весь домен. */
function editor_cookie_path(): string
{
    $script = str_replace('\\', '/', (string)($_SERVER['SCRIPT_NAME'] ?? '/'));
    $apiPos = strpos($script, '/api/');
    if ($apiPos !== false) {
        $root = substr($script, 0, $apiPos + 1);
        return $root !== '' ? $root : '/';
    }
    $dir = rtrim(dirname($script), '/');
    return ($dir === '' || $dir === '.') ? '/' : $dir . '/';
}

/** @return array{idle:int, absolute:int} */
function editor_session_limits(?array $cfg = null): array
{
    $cfg ??= load_cfg(false);
    return [
        'idle' => clamp_int($cfg['session_idle_seconds'] ?? null, 900, 172800, EDITOR_SESSION_IDLE_DEFAULT),
        'absolute' => clamp_int($cfg['session_absolute_seconds'] ?? null, 3600, 2592000, EDITOR_SESSION_ABSOLUTE_DEFAULT),
    ];
}

/** Отмечает начало авторизованной сессии (вызывается при успешном входе). */
function editor_session_mark_login(): void
{
    $now = time();
    $_SESSION['_es_created'] = $now;
    $_SESSION['_es_last'] = $now;
    $GLOBALS['EDITOR_SESSION_EXPIRED'] = false;
}

/** Момент (ISO), когда текущая авторизованная сессия истечёт. */
function editor_session_expires_at(?array $cfg = null): ?string
{
    if (session_status() !== PHP_SESSION_ACTIVE || empty($_SESSION['_es_created'])) {
        return null;
    }
    $limits = editor_session_limits($cfg);
    $created = (int)$_SESSION['_es_created'];
    $last = (int)($_SESSION['_es_last'] ?? $created);
    $expires = min($created + $limits['absolute'], $last + $limits['idle']);
    return gmdate('Y-m-d\TH:i:s\Z', $expires);
}

/**
 * Проверка сроков сессии. Просроченная сессия теряет авторизацию (роли и
 * CSRF-токен), получает новый ID и помечается флагом EDITOR_SESSION_EXPIRED —
 * оболочка по нему показывает «сессия истекла», а не «войдите».
 */
function editor_session_enforce_lifetime(?array $cfg = null): void
{
    if (session_status() !== PHP_SESSION_ACTIVE) {
        return;
    }
    $now = time();
    $created = (int)($_SESSION['_es_created'] ?? 0);
    if ($created <= 0) {
        $_SESSION['_es_created'] = $now;
        $_SESSION['_es_last'] = $now;
        return;
    }
    $last = (int)($_SESSION['_es_last'] ?? $created);
    $authorized = !empty($_SESSION['editor_studio_auth']) || !empty($_SESSION['editor_studio_guest']);
    if ($authorized) {
        $limits = editor_session_limits($cfg);
        if ($now - $last > $limits['idle'] || $now - $created > $limits['absolute']) {
            unset(
                $_SESSION['editor_studio_auth'],
                $_SESSION['editor_studio_user'],
                $_SESSION['editor_studio_csrf']
            );
            clear_guest_session();
            $_SESSION['_es_created'] = $now;
            $GLOBALS['EDITOR_SESSION_EXPIRED'] = true;
            @session_regenerate_id(true);
        }
    }
    $_SESSION['_es_last'] = $now;
}

function start_editor_session(?array $cfg = null): void
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
        'path'     => editor_cookie_path(),
        'domain'   => '',
        'secure'   => $isHttps,
        'httponly'  => true,
        // Strict: студия — самостоятельное приложение, переходов с внешних
        // сайтов с сохранением сессии ей не нужно; запросы самой страницы
        // (fetch к своему API) cookie получают как обычно (same-site).
        'samesite'  => 'Strict',
    ]);

    session_start();
    editor_session_enforce_lifetime($cfg);
}

// ── CSRF ─────────────────────────────────────────────────────────────────────
//
// Синхронизаторный токен в дополнение к проверке Origin: токен живёт в сессии,
// клиент присылает его заголовком X-Editor-CSRF в каждом POST. Чужой сайт
// токен не знает (и не прочитает ответ из-за same-origin policy), поэтому
// подделать запрос нельзя даже при ослабленной проверке Origin (прокси,
// misconfigured Host и т.п.). Токен ротируется при входе и выходе.

/** Текущий CSRF-токен сессии (создаётся при первом обращении). */
function editor_csrf_token(): string
{
    if (session_status() !== PHP_SESSION_ACTIVE) {
        return '';
    }
    $token = (string)($_SESSION['editor_studio_csrf'] ?? '');
    if (preg_match('/^[a-f0-9]{64}$/', $token) !== 1) {
        $token = bin2hex(random_bytes(32));
        $_SESSION['editor_studio_csrf'] = $token;
    }
    return $token;
}

function editor_csrf_rotate(): string
{
    if (session_status() !== PHP_SESSION_ACTIVE) {
        return '';
    }
    $token = bin2hex(random_bytes(32));
    $_SESSION['editor_studio_csrf'] = $token;
    return $token;
}

/** Проверка заголовка X-Editor-CSRF; при несовпадении — 403 с кодом для клиента. */
function editor_csrf_verify(): void
{
    $expected = editor_csrf_token();
    $provided = trim((string)($_SERVER['HTTP_X_EDITOR_CSRF'] ?? ''));
    if ($expected === '' || $provided === '' || !hash_equals($expected, $provided)) {
        respond([
            'ok' => false,
            'error' => 'Защитный токен сессии устарел. Обновите страницу и попробуйте ещё раз.',
            'code' => 'csrf_token',
        ], 403);
    }
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
    $cfg ??= load_cfg(false);
    $usersCount = array_key_exists('guest_users', $cfg)
        ? count(array_filter(guest_users($cfg), fn($u) => $u['enabled']))
        : store_guest_users_count(true);
    return [
        'enabled'        => (bool)($cfg['guest_enabled'] ?? false),
        'allow_remote'   => (bool)($cfg['guest_allow_remote'] ?? false),
        'max_pending'    => clamp_int($cfg['guest_max_pending'] ?? null, 1, 50, 5),
        'cooldown'       => clamp_int($cfg['guest_submit_cooldown'] ?? null, 0, 3600, 60),
        'max_operations' => clamp_int($cfg['guest_max_operations'] ?? null, 1, 500, 100),
        // Guest logins exist only as personal accounts created by an administrator.
        'users_count'    => $usersCount,
    ];
}

/**
 * Нормализованные индивидуальные лимиты гостевого аккаунта.
 * null — переопределений нет, действуют глобальные правила guest_cfg().
 * Ключи: max_pending (1–50), cooldown (0–3600 сек), max_operations (1–500).
 */
function guest_limits_normalize(mixed $raw): ?array
{
    if (is_string($raw) && trim($raw) !== '') {
        $raw = json_decode($raw, true);
    }
    if (!is_array($raw)) {
        return null;
    }
    $out = [];
    if (array_key_exists('max_pending', $raw) && $raw['max_pending'] !== null && $raw['max_pending'] !== '') {
        $out['max_pending'] = clamp_int($raw['max_pending'], 1, 50, 5);
    }
    if (array_key_exists('cooldown', $raw) && $raw['cooldown'] !== null && $raw['cooldown'] !== '') {
        $out['cooldown'] = clamp_int($raw['cooldown'], 0, 3600, 60);
    }
    if (array_key_exists('max_operations', $raw) && $raw['max_operations'] !== null && $raw['max_operations'] !== '') {
        $out['max_operations'] = clamp_int($raw['max_operations'], 1, 500, 100);
    }
    return $out !== [] ? $out : null;
}

/**
 * Действующие лимиты конкретного гостя: глобальные правила, перекрытые
 * индивидуальными настройками аккаунта (если администратор их задал).
 * @return array{max_pending:int, cooldown:int, max_operations:int, custom:bool}
 */
function guest_effective_limits(?array $account, ?array $cfg = null): array
{
    $global = guest_cfg($cfg);
    $limits = [
        'max_pending' => $global['max_pending'],
        'cooldown' => $global['cooldown'],
        'max_operations' => $global['max_operations'],
        'custom' => false,
    ];
    $own = guest_limits_normalize($account['limits'] ?? null);
    if ($own !== null) {
        $limits = array_merge($limits, $own, ['custom' => true]);
    }
    return $limits;
}

/** Истёк ли срок действия гостевого аккаунта ('' — бессрочно). */
function guest_user_expired(array $user): bool
{
    $expires = trim((string)($user['expiresAt'] ?? ''));
    if ($expires === '') {
        return false;
    }
    $ts = strtotime($expires);
    return $ts !== false && $ts <= time();
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
        'nodes_collecting' => ['title' => 'Узлы ресурсов', 'write' => ['nodes_collecting'], 'read' => ['nodes_collecting', 'items']],
        'aethers'    => ['title' => 'Телепорты (Aether)', 'write' => ['aethers'],               'read' => ['aethers']],
        'dungeons'   => ['title' => 'Подземелья',         'write' => ['dungeons'],              'read' => ['dungeons']],
        'quests'     => ['title' => 'Квесты',             'write' => ['quests', 'quest_bots', 'quest_board_links'],  'read' => ['quests', 'quest_bots', 'quest_board_links', 'quest_boards']],
        'quest_boards' => ['title' => 'Доски квестов',    'write' => ['quest_boards', 'quest_board_links'], 'read' => ['quests', 'quest_board_links', 'quest_boards']],
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

    $expiresAt = trim((string)($raw['expiresAt'] ?? ''));
    if ($expiresAt !== '' && strtotime($expiresAt) === false) {
        $expiresAt = '';
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
        'limits'         => guest_limits_normalize($raw['limits'] ?? null),
        'expiresAt'      => $expiresAt,
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
    $id = strtolower(trim($id));
    if ($id === '') {
        return null;
    }
    if ($cfg === null || !array_key_exists('guest_users', $cfg)) {
        $row = store_guest_user_get($id);
        return $row === null ? null : guest_user_normalize($row);
    }
    return guest_users($cfg)[$id] ?? null;
}

function guest_user_by_login(?array $cfg, string $login): ?array
{
    $login = strtolower(trim($login));
    if ($login === '') {
        return null;
    }
    if ($cfg === null || !array_key_exists('guest_users', $cfg)) {
        $row = store_guest_user_by_login($login);
        return $row === null ? null : guest_user_normalize($row);
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
        'limits'      => $user['limits'] ?? null,
        'expiresAt'   => $user['expiresAt'] ?? '',
        'expired'     => guest_user_expired($user),
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
    if (!$guest['enabled'] || $user === null || !$user['enabled'] || guest_user_expired($user)) {
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
    start_editor_session(load_cfg(false));
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
        $expired = !empty($GLOBALS['EDITOR_SESSION_EXPIRED']);
        respond([
            'ok' => false,
            'error' => $expired ? 'Сессия истекла. Войдите в Editor Studio ещё раз.' : 'Требуется вход в Editor Studio',
            'code' => $expired ? 'session_expired' : 'auth_required',
        ], 401);
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
    $origin = trim((string)($_SERVER['HTTP_ORIGIN'] ?? ''));
    // Never trust X-Forwarded-Host here: an untrusted client can supply it.
    // Origin on a different port or scheme is a different security origin.
    if ($origin === '' || strtolower($origin) === 'null') {
        respond(['ok' => false, 'error' => 'Заголовок Origin обязателен'], 403);
    }
    $parts = parse_url($origin);
    $scheme = strtolower((string)($parts['scheme'] ?? ''));
    $originHost = strtolower((string)($parts['host'] ?? ''));
    $originPort = isset($parts['port']) ? (int)$parts['port'] : ($scheme === 'https' ? 443 : 80);
    $requestHost = strtolower(trim((string)($_SERVER['HTTP_HOST'] ?? '')));
    $requestParts = parse_url('//' . $requestHost);
    $requestName = strtolower((string)($requestParts['host'] ?? ''));
    // HTTPS may terminate at a reverse proxy, so PHP can see HTTP while the
    // browser correctly sends an HTTPS Origin. Host is the externally visible
    // authority; only compare an explicit Host port. When Host has no port,
    // use Origin's scheme default (80/443) rather than PHP's internal scheme.
    $requestPort = isset($requestParts['port']) ? (int)$requestParts['port'] : $originPort;
    if (!in_array($scheme, ['http', 'https'], true) || $originHost === ''
        || $originHost !== $requestName || $originPort !== $requestPort) {
        respond(['ok' => false, 'error' => 'Запрос с постороннего сайта отклонён'], 403);
    }
    // Второй рубеж: синхронизаторный CSRF-токен сессии (см. editor_csrf_token).
    editor_csrf_verify();
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

/**
 * Список доверенных обратных прокси (CIDR/IP), которым разрешено сообщать
 * реальный адрес клиента заголовками X-Forwarded-For / X-Real-IP.
 * Источник: переменная окружения EDITOR_STUDIO_TRUSTED_PROXIES (через запятую)
 * или настройка trusted_proxies в хранилище. По умолчанию список пуст:
 * заголовки игнорируются, клиент — это REMOTE_ADDR.
 *
 * Почему это важно: без списка любой клиент мог прислать
 * `X-Forwarded-For: 127.0.0.1` и обойти сетевую политику «только localhost»,
 * а за настоящим прокси, наоборот, все клиенты выглядели бы одним адресом
 * прокси (общий счётчик попыток входа и «localhost» для всех).
 * @return list<string>
 */
function editor_trusted_proxies(?array $cfg = null): array
{
    $env = getenv('EDITOR_STUDIO_TRUSTED_PROXIES');
    if (is_string($env) && trim($env) !== '') {
        return array_values(array_filter(array_map('trim', explode(',', $env)), fn($v) => $v !== ''));
    }
    $cfg ??= load_cfg(false);
    $raw = $cfg['trusted_proxies'] ?? [];
    if (is_string($raw)) {
        $raw = array_map('trim', explode(',', $raw));
    }
    if (!is_array($raw)) {
        return [];
    }
    return array_values(array_filter(array_map('strval', $raw), fn($v) => trim($v) !== ''));
}

/**
 * Чистая логика выбора адреса клиента (выделена для тестов).
 * Заголовки прокси учитываются, только если непосредственный сосед
 * (REMOTE_ADDR) входит в список доверенных прокси. В цепочке
 * X-Forwarded-For идём справа налево, пропуская доверенные узлы:
 * первый недоверенный — клиент; если доверены все — самый левый.
 */
function editor_resolve_client_ip(string $remoteAddr, string $forwardedFor, string $realIp, array $trustedProxies): string
{
    $remote = trim($remoteAddr);
    if (!filter_var($remote, FILTER_VALIDATE_IP)) {
        return '';
    }
    if ($trustedProxies === [] || !ip_in_cidrs($remote, $trustedProxies)) {
        return $remote;
    }

    $chain = [];
    foreach (array_map('trim', explode(',', $forwardedFor)) as $part) {
        if (filter_var($part, FILTER_VALIDATE_IP)) {
            $chain[] = $part;
        }
    }
    if ($chain !== []) {
        for ($i = count($chain) - 1; $i >= 0; $i--) {
            if (!ip_in_cidrs($chain[$i], $trustedProxies)) {
                return $chain[$i];
            }
        }
        return $chain[0];
    }

    $real = trim($realIp);
    return filter_var($real, FILTER_VALIDATE_IP) ? $real : $remote;
}

function get_request_ip(?array $cfg = null): string
{
    return editor_resolve_client_ip(
        (string)($_SERVER['REMOTE_ADDR'] ?? ''),
        (string)($_SERVER['HTTP_X_FORWARDED_FOR'] ?? ''),
        (string)($_SERVER['HTTP_X_REAL_IP'] ?? ''),
        editor_trusted_proxies($cfg)
    );
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
        $ip = get_request_ip($cfg);
        return ip_in_cidrs($ip, $allowed) ? null : 'Доступ с этого IP запрещён';
    }

    // Local-only mode
    return ip_is_loopback(get_request_ip($cfg)) ? null : 'API редактора доступен только с localhost';
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

    $cfg = load_cfg(false);
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

    start_editor_session($cfg);

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

function db_connect_server(?array $cfg = null): mysqli
{
    $cfg ??= load_cfg(false);
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
    $cfg = load_cfg(false);
    $db  = (string)($cfg['database'] ?? '');

    $mysqli = db_connect_server($cfg);
    if ($db !== '' && !$mysqli->select_db($db)) {
        $error = $mysqli->error;
        $mysqli->close();
        throw new RuntimeException('Не удалось выбрать базу данных: ' . $error);
    }
    return $mysqli;
}