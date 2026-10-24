<?php
// Session-based auth endpoint for Editor Studio.
// Endpoint: api/auth.php
// Actions:
//  - status      (GET)
//  - login       (POST) administrator login
//  - guest_login (POST) {login, password, guest_key} — personal guest account created by an admin
//  - logout      (POST)
//
// Guests are personal accounts (SQLite table guest_users). Each account has its own
// password and its own set of allowed editor tabs. A guest session can never write
// to the game database: its edits become change requests.

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';

$currentRole = bootstrap_editor_api_roles(null);

$action = (string)($_GET['action'] ?? 'status');

function auth_credentials(?array $cfg = null): array {
  $cfg ??= load_cfg(false);
  return [
    'user' => trim((string)($cfg['editor_user'] ?? 'admin')),
    'password' => (string)($cfg['editor_password'] ?? ''),
  ];
}

function auth_password_matches(string $expected, string $provided): bool {
  $info = password_get_info($expected);
  if (!empty($info['algo'])) {
    return password_verify($provided, $expected);
  }
  return hash_equals($expected, $provided);
}

/**
 * Shared brute-force protection for admin and guest logins.
 * The counters live in the SQLite store (table auth_throttle) and therefore
 * survive a dropped session cookie; the account scope is bound to the address,
 * so guessing a login from another host cannot lock the real guest out.
 */
const AUTH_IP_MAX_ATTEMPTS = 25;
const AUTH_IP_WINDOW = 600;
const AUTH_IP_LOCK = 300;
const AUTH_ACCOUNT_MAX_ATTEMPTS = 5;
const AUTH_ACCOUNT_WINDOW = 600;
const AUTH_ACCOUNT_LOCK = 60;

function auth_client_ip(): string {
  $ip = get_request_ip();
  return $ip !== '' ? $ip : 'unknown';
}

/** Seconds the client must wait before the next attempt (0 = allowed). */
function auth_throttle_wait(string $login): int {
  $ip = auth_client_ip();
  return max(
    auth_throttle_left('login-ip', $ip),
    auth_throttle_left('login-account', $ip . '|' . $login)
  );
}

function auth_check_throttle(string $login = ''): void {
  $left = auth_throttle_wait($login);
  if ($left > 0) {
    respond(['ok' => false, 'error' => "Слишком много попыток входа. Повторите через {$left} сек."], 429);
  }
}

/** Registers a failure; returns the lock time when this attempt hit the limit. */
function auth_register_failure(string $login = ''): int {
  $ip = auth_client_ip();
  $left = auth_throttle_fail('login-ip', $ip, AUTH_IP_MAX_ATTEMPTS, AUTH_IP_WINDOW, AUTH_IP_LOCK);
  if ($login !== '') {
    $left = max($left, auth_throttle_fail('login-account', $ip . '|' . $login, AUTH_ACCOUNT_MAX_ATTEMPTS, AUTH_ACCOUNT_WINDOW, AUTH_ACCOUNT_LOCK));
  }
  return $left;
}

/** Uniform answer for a rejected login; the attempt that trips the limit says so. */
function auth_reject(string $message, int $lockLeft): never {
  if ($lockLeft > 0) {
    respond(['ok' => false, 'error' => "Слишком много попыток входа. Повторите через {$lockLeft} сек."], 429);
  }
  respond(['ok' => false, 'error' => $message], 401);
}

function auth_clear_throttle(string $login): void {
  $ip = auth_client_ip();
  auth_throttle_clear('login-account', $ip . '|' . $login);
  auth_throttle_clear('login-ip', $ip);
}

function auth_status_payload(?string $role): array {
  $cfg = load_cfg(false);
  $guest = guest_cfg($cfg);
  $guestEnabled = $guest['enabled'] && $guest['users_count'] > 0;

  $payload = [
    'ok' => true,
    'authorized' => $role !== null,
    'role' => $role,
    'user' => null,
    'guest' => [
      // Guest login is possible only when the administrator enabled it AND at least one account is active.
      'enabled' => $guestEnabled,
      'accounts' => $guest['users_count'],
      'allow_remote' => $guest['allow_remote'],
      'pages' => [],
      'login' => null,
      'name' => null,
      'id' => null,
    ],
    // When true, this client may only use a guest account (admin login is blocked by IP policy).
    'guest_only_network' => editor_network_guest_only(),
    // Пароль администратора не задан: вход невозможен, пока он не настроен.
    // Публиковать пароль по умолчанию нельзя — это открытый доступ к базе.
    'admin_setup_needed' => auth_credentials($cfg)['password'] === '',
  ];

  if ($role === EDITOR_ROLE_ADMIN) {
    $payload['user'] = (string)($_SESSION['editor_studio_user'] ?? 'admin');
  } elseif ($role === EDITOR_ROLE_GUEST) {
    $account = editor_guest_user($cfg);
    if ($account === null) {
      // Session was revoked between the role check and here.
      clear_guest_session();
      $payload['authorized'] = false;
      $payload['role'] = null;
      return $payload;
    }
    $payload['user'] = $account['name'];
    $payload['guest']['pages'] = $account['pages'];
    $payload['guest']['login'] = $account['login'];
    $payload['guest']['name'] = $account['name'];
    $payload['guest']['id'] = $account['id'];
    $payload['guest']['limits'] = [
      'max_pending' => $guest['max_pending'],
      'cooldown' => $guest['cooldown'],
      'max_operations' => $guest['max_operations'],
    ];
  }
  return $payload;
}

try {
  $method = strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET'));

  if ($action === 'status' && $method !== 'GET') {
    respond(['ok' => false, 'error' => 'Метод запроса не поддерживается'], 405);
  }
  if (in_array($action, ['login', 'guest_login', 'logout'], true) && $method !== 'POST') {
    respond(['ok' => false, 'error' => 'Метод запроса не поддерживается'], 405);
  }
  if (in_array($action, ['login', 'guest_login', 'logout'], true)) {
    require_same_origin_json_post();
  }

  if ($action === 'status') {
    respond(auth_status_payload($currentRole));
  }

  if ($action === 'login') {
    if (editor_network_guest_only()) {
      respond(['ok' => false, 'error' => 'Вход администратора с этого адреса запрещён. Доступен только гостевой режим.'], 403);
    }

    $body = read_json_body();
    $user = trim((string)($body['user'] ?? ''));
    $password = (string)($body['password'] ?? '');

    $creds = auth_credentials();
    $expectedUser = $creds['user'] === '' ? 'admin' : $creds['user'];
    $expectedPassword = $creds['password'];

    if ($expectedPassword === '') {
      respond([
        'ok' => false,
        'error' => 'Пароль администратора не задан: создайте его командой php tools/init-db.php --password=… (старые установки: поле editor_password в data/db-config.json).',
      ], 409);
    }

    auth_check_throttle($user);

    if (!hash_equals($expectedUser, $user) || !auth_password_matches($expectedPassword, $password)) {
      auth_reject('Неверный логин или пароль', auth_register_failure($user));
    }

    clear_guest_session();
    $_SESSION['editor_studio_auth'] = true;
    $_SESSION['editor_studio_user'] = $expectedUser;
    auth_clear_throttle($user);
    session_regenerate_id(true);

    respond(auth_status_payload(EDITOR_ROLE_ADMIN));
  }

  if ($action === 'guest_login') {
    $cfg = load_cfg(false);
    $guest = guest_cfg($cfg);
    if (!$guest['enabled'] || $guest['users_count'] === 0) {
      respond(['ok' => false, 'error' => 'Гостевой доступ отключён администратором.'], 403);
    }

    $body = read_json_body();

    $login = strtolower(trim((string)($body['login'] ?? '')));
    $password = (string)($body['password'] ?? '');

    if ($login === '' || $password === '') {
      respond(['ok' => false, 'error' => 'Укажите логин и пароль гостевого аккаунта.'], 400);
    }

    auth_check_throttle($login);

    $account = guest_user_by_login($cfg, $login);
    // Uniform error: never reveal whether the account exists.
    if ($account === null || !$account['enabled'] || $account['passwordHash'] === ''
      || !password_verify($password, $account['passwordHash'])) {
      auth_reject('Неверный логин или пароль гостевого аккаунта', auth_register_failure($login));
    }

    auth_clear_throttle($login);

    // A guest can resume its identity (to see its previous requests) with the key stored in its browser.
    $resumeKey = strtolower(trim((string)($body['guest_key'] ?? '')));
    $guestKey = preg_match('/^[a-f0-9]{48}$/', $resumeKey) === 1 ? $resumeKey : bin2hex(random_bytes(24));

    // Never keep admin privileges in a guest session.
    unset($_SESSION['editor_studio_auth'], $_SESSION['editor_studio_user']);
    session_regenerate_id(true);

    $_SESSION['editor_studio_guest'] = true;
    $_SESSION['editor_studio_guest_user'] = $account['id'];
    $_SESSION['editor_studio_guest_key'] = $guestKey;
    $_SESSION['editor_studio_guest_epoch'] = guest_user_epoch($account);

    // Remember the last successful login (admin sees it in "Гостевые пользователи").
    store_guest_user_update($account['id'], ['lastLoginAt' => gmdate('Y-m-d\TH:i:s\Z')]);

    $payload = auth_status_payload(EDITOR_ROLE_GUEST);
    $payload['guest_key'] = $guestKey;
    respond($payload);
  }

  if ($action === 'logout') {
    unset($_SESSION['editor_studio_auth'], $_SESSION['editor_studio_user']);
    clear_guest_session();
    session_regenerate_id(true);
    respond(['ok' => true, 'authorized' => false, 'role' => null]);
  }

  respond(['ok' => false, 'error' => 'Неизвестное действие'], 400);
} catch (Throwable $e) {
  editor_internal_error($e);
}
