<?php
// Session-based auth endpoint for Editor Studio.
// Endpoint: api/auth.php
// Actions:
//  - status (GET)
//  - login (POST)
//  - logout (POST)

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('X-Content-Type-Options: nosniff');

require_once __DIR__ . '/db-core.php';

start_editor_session();

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

try {
  $method = strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET'));

  if ($action === 'status' && $method !== 'GET') {
    respond(['ok' => false, 'error' => 'Method not allowed'], 405);
  }
  if (($action === 'login' || $action === 'logout') && $method !== 'POST') {
    respond(['ok' => false, 'error' => 'Method not allowed'], 405);
  }

  if ($action === 'status') {
    $authorized = isset($_SESSION['editor_studio_auth']) && $_SESSION['editor_studio_auth'] === true;
    respond([
      'ok' => true,
      'authorized' => $authorized,
      'user' => $authorized ? (string)($_SESSION['editor_studio_user'] ?? 'admin') : null,
    ]);
  }

  if ($action === 'login') {
    $body = read_json_body();
    $user = trim((string)($body['user'] ?? ''));
    $password = (string)($body['password'] ?? '');

    $creds = auth_credentials();
    $expectedUser = $creds['user'] === '' ? 'admin' : $creds['user'];
    $expectedPassword = $creds['password'];

    $failUntil = (int)($_SESSION['editor_studio_auth_fail_until'] ?? 0);
    if ($failUntil > time()) {
      $left = max(1, $failUntil - time());
      respond(['ok' => false, 'error' => "Слишком много попыток входа. Повторите через {$left} сек."], 429);
    }

    if (!hash_equals($expectedUser, $user) || !auth_password_matches($expectedPassword, $password)) {
      $fails = (int)($_SESSION['editor_studio_auth_fail_count'] ?? 0) + 1;
      $_SESSION['editor_studio_auth_fail_count'] = $fails;
      if ($fails >= 5) {
        $_SESSION['editor_studio_auth_fail_until'] = time() + 30;
        $_SESSION['editor_studio_auth_fail_count'] = 0;
      }
      respond(['ok' => false, 'error' => 'Неверный логин или пароль'], 401);
    }

    $_SESSION['editor_studio_auth'] = true;
    $_SESSION['editor_studio_user'] = $expectedUser;
    unset($_SESSION['editor_studio_auth_fail_count'], $_SESSION['editor_studio_auth_fail_until']);
    session_regenerate_id(true);

    respond(['ok' => true, 'authorized' => true, 'user' => $expectedUser]);
  }

  if ($action === 'logout') {
    unset($_SESSION['editor_studio_auth'], $_SESSION['editor_studio_user']);
    respond(['ok' => true, 'authorized' => false]);
  }

  respond(['ok' => false, 'error' => 'Unknown action'], 400);
} catch (Throwable $e) {
  respond(['ok' => false, 'error' => $e->getMessage()], 500);
}
