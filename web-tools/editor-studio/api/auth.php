<?php
// Session-based auth endpoint for Editor Studio.
// Endpoint: api/auth.php
// Actions:
//  - status      (GET)
//  - login       (POST) administrator login
//  - guest_login (POST) guest login (only when guest access is enabled)
//  - logout      (POST)

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';

$currentRole = bootstrap_editor_api_roles(null);

$action = (string)($_GET['action'] ?? 'status');

function auth_credentials(): array {
  $cfg = load_cfg();
  return [
    'user' => trim((string)($cfg['editor_user'] ?? 'admin')),
    'password' => (string)($cfg['editor_password'] ?? 'admin1'),
  ];
}

function auth_password_matches(string $expected, string $provided): bool {
  $info = password_get_info($expected);
  if (!empty($info['algo'])) {
    return password_verify($provided, $expected);
  }
  return hash_equals($expected, $provided);
}

/** Shared brute-force protection for admin and guest logins. */
function auth_check_throttle(): void {
  $failUntil = (int)($_SESSION['editor_studio_auth_fail_until'] ?? 0);
  if ($failUntil > time()) {
    $left = max(1, $failUntil - time());
    respond(['ok' => false, 'error' => "Слишком много попыток входа. Повторите через {$left} сек."], 429);
  }
}

function auth_register_failure(): void {
  $fails = (int)($_SESSION['editor_studio_auth_fail_count'] ?? 0) + 1;
  $_SESSION['editor_studio_auth_fail_count'] = $fails;
  if ($fails >= 5) {
    $_SESSION['editor_studio_auth_fail_until'] = time() + 30;
    $_SESSION['editor_studio_auth_fail_count'] = 0;
  }
}

function auth_status_payload(?string $role): array {
  $guest = guest_cfg();
  $payload = [
    'ok' => true,
    'authorized' => $role !== null,
    'role' => $role,
    'user' => null,
    'guest' => [
      'enabled' => $guest['enabled'],
      'password_required' => $guest['password'] !== '',
    ],
    // When true, this client may only use the guest account (admin login is blocked by IP policy).
    'guest_only_network' => editor_network_guest_only(),
  ];
  if ($role === EDITOR_ROLE_ADMIN) {
    $payload['user'] = (string)($_SESSION['editor_studio_user'] ?? 'admin');
  } elseif ($role === EDITOR_ROLE_GUEST) {
    $payload['user'] = 'guest';
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
    respond(['ok' => false, 'error' => 'Method not allowed'], 405);
  }
  if (in_array($action, ['login', 'guest_login', 'logout'], true) && $method !== 'POST') {
    respond(['ok' => false, 'error' => 'Method not allowed'], 405);
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

    auth_check_throttle();

    if (!hash_equals($expectedUser, $user) || !auth_password_matches($expectedPassword, $password)) {
      auth_register_failure();
      respond(['ok' => false, 'error' => 'Неверный логин или пароль'], 401);
    }

    clear_guest_session();
    $_SESSION['editor_studio_auth'] = true;
    $_SESSION['editor_studio_user'] = $expectedUser;
    unset($_SESSION['editor_studio_auth_fail_count'], $_SESSION['editor_studio_auth_fail_until']);
    session_regenerate_id(true);

    respond(auth_status_payload(EDITOR_ROLE_ADMIN));
  }

  if ($action === 'guest_login') {
    require_same_origin_json_post();
    $guest = guest_cfg();
    if (!$guest['enabled']) {
      respond(['ok' => false, 'error' => 'Гостевой доступ отключён администратором.'], 403);
    }

    $body = read_json_body();
    auth_check_throttle();

    if ($guest['password'] !== '') {
      $password = (string)($body['password'] ?? '');
      if ($password === '' || !auth_password_matches($guest['password'], $password)) {
        auth_register_failure();
        respond(['ok' => false, 'error' => 'Неверный гостевой пароль'], 401);
      }
    }

    // A guest can resume its identity (to see its previous requests) with the key stored in its browser.
    $resumeKey = strtolower(trim((string)($body['guest_key'] ?? '')));
    $guestKey = preg_match('/^[a-f0-9]{48}$/', $resumeKey) === 1 ? $resumeKey : bin2hex(random_bytes(24));

    // Never keep admin privileges in a guest session.
    unset($_SESSION['editor_studio_auth'], $_SESSION['editor_studio_user']);
    unset($_SESSION['editor_studio_auth_fail_count'], $_SESSION['editor_studio_auth_fail_until']);
    $_SESSION['editor_studio_guest'] = true;
    $_SESSION['editor_studio_guest_key'] = $guestKey;
    $_SESSION['editor_studio_guest_epoch'] = guest_password_epoch($guest);
    session_regenerate_id(true);

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

  respond(['ok' => false, 'error' => 'Unknown action'], 400);
} catch (Throwable $e) {
  respond(['ok' => false, 'error' => $e->getMessage()], 500);
}
