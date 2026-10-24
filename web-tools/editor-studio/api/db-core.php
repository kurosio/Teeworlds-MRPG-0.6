<?php
// Shared helpers for DB API endpoints.

declare(strict_types=1);

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

function read_json_body(): array
{
    $raw = file_get_contents('php://input');
    if ($raw === false || $raw === '') {
        return [];
    }
    $data = json_decode($raw, true);
    return is_array($data) ? $data : [];
}

// ── Configuration ────────────────────────────────────────────────────────────

function cfg_path(): string
{
    return dirname(__DIR__) . '/data/db-config.json';
}

function load_cfg(): array
{
    $path = cfg_path();
    if (!is_file($path)) {
        return [];
    }
    $raw = file_get_contents($path);
    if ($raw === false) {
        return [];
    }
    $cfg = json_decode($raw, true);
    return is_array($cfg) ? $cfg : [];
}

function save_cfg(array $cfg): bool
{
    $path = cfg_path();
    $dir  = dirname($path);
    if (!is_dir($dir) && !@mkdir($dir, 0775, true) && !is_dir($dir)) {
        return false;
    }
    $json = json_encode($cfg, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT | JSON_THROW_ON_ERROR);
    $bytes = file_put_contents($path, $json, LOCK_EX);
    return $bytes !== false;
}

// ── Session ──────────────────────────────────────────────────────────────────

function start_editor_session(): void
{
    if (session_status() === PHP_SESSION_ACTIVE) {
        return;
    }

    $isHttps = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off');

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
        'password'       => (string)($cfg['guest_password'] ?? ''),
        'allow_remote'   => (bool)($cfg['guest_allow_remote'] ?? false),
        'max_pending'    => clamp_int($cfg['guest_max_pending'] ?? null, 1, 50, 5),
        'cooldown'       => clamp_int($cfg['guest_submit_cooldown'] ?? null, 0, 3600, 60),
        'max_operations' => clamp_int($cfg['guest_max_operations'] ?? null, 1, 500, 100),
    ];
}

/** Fingerprint of the guest password: a password change invalidates guest sessions. */
function guest_password_epoch(array $guest): string
{
    return substr(hash('sha256', 'guest-epoch|' . $guest['password']), 0, 16);
}

function clear_guest_session(): void
{
    unset(
        $_SESSION['editor_studio_guest'],
        $_SESSION['editor_studio_guest_key'],
        $_SESSION['editor_studio_guest_epoch'],
        $_SESSION['editor_studio_guest_last_submit']
    );
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
        $guest = guest_cfg($cfg);
        $epoch = (string)($_SESSION['editor_studio_guest_epoch'] ?? '');
        // Guest access disabled or password rotated → session is no longer valid.
        if (!$guest['enabled'] || !hash_equals(guest_password_epoch($guest), $epoch)) {
            clear_guest_session();
            return null;
        }
        return EDITOR_ROLE_GUEST;
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
        respond(['ok' => false, 'error' => 'Unauthorized'], 401);
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
        respond(['ok' => false, 'error' => 'Method not allowed'], 405);
    }
    $ctype = strtolower((string)($_SERVER['CONTENT_TYPE'] ?? $_SERVER['HTTP_CONTENT_TYPE'] ?? ''));
    if (!str_starts_with($ctype, 'application/json')) {
        respond(['ok' => false, 'error' => 'Content-Type must be application/json'], 415);
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
            respond(['ok' => false, 'error' => 'Cross-origin request rejected'], 403);
        }
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
        return ip_in_cidrs($ip, $allowed) ? null : 'Access denied for this IP';
    }

    // Local-only mode
    return ip_is_loopback(get_request_ip()) ? null : 'Editor API is available only from localhost';
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
        $guestNetworkAllowed = $guest['enabled'] && $guest['allow_remote']
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

function test_http_endpoint(string $url): array
{
    if (!filter_var($url, FILTER_VALIDATE_URL)) {
        return ['ok' => false, 'error' => 'Invalid URL'];
    }

    // Restrict to http(s) schemes only
    $scheme = parse_url($url, PHP_URL_SCHEME);
    if (!in_array(strtolower((string)$scheme), ['http', 'https'], true)) {
        return ['ok' => false, 'error' => 'Only http and https schemes are allowed'];
    }

    if (!function_exists('curl_init')) {
        return ['ok' => false, 'error' => 'cURL extension is not available'];
    }

    $ch = curl_init($url);
    if ($ch === false) {
        return ['ok' => false, 'error' => 'Failed to initialize cURL'];
    }

    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_FOLLOWLOCATION => true,
        CURLOPT_MAXREDIRS      => 3,
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
        throw new RuntimeException('DB select failed: ' . $error);
    }
    return $mysqli;
}