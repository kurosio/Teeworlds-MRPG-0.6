<?php
// Shared helpers for DB API endpoints.

declare(strict_types=1);

function apply_security_headers(): void {
  header('Content-Type: application/json; charset=utf-8');
  header('X-Content-Type-Options: nosniff');
  header('X-Frame-Options: SAMEORIGIN');
  header('Referrer-Policy: same-origin');
  header('Cross-Origin-Resource-Policy: same-origin');
  header('Cross-Origin-Opener-Policy: same-origin');
  header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
  header('Pragma: no-cache');
}

function respond(array $payload, int $code = 200): void {
  http_response_code($code);
  echo json_encode($payload, JSON_UNESCAPED_UNICODE);
  exit;
}

function read_json_body(): array {
  $raw = file_get_contents('php://input');
  if (!$raw) return [];
  $data = json_decode($raw, true);
  return is_array($data) ? $data : [];
}

function cfg_path(): string {
  return dirname(__DIR__) . '/data/db-config.json';
}

function load_cfg(): array {
  $path = cfg_path();
  if (!file_exists($path)) return [];
  $raw = file_get_contents($path);
  $cfg = json_decode($raw ?: 'null', true);
  return is_array($cfg) ? $cfg : [];
}

function save_cfg(array $cfg): bool {
  $path = cfg_path();
  $dir = dirname($path);
  if (!is_dir($dir)) @mkdir($dir, 0775, true);
  $json = json_encode($cfg, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
  return file_put_contents($path, $json) !== false;
}

function start_editor_session(): void {
  if (session_status() === PHP_SESSION_ACTIVE) return;
  $isHttps = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off');
  session_set_cookie_params([
    'lifetime' => 0,
    'path' => '/',
    'domain' => '',
    'secure' => $isHttps,
    'httponly' => true,
    'samesite' => 'Lax',
  ]);
  session_start();
}

function get_request_ip(): string {
  $candidates = [
    (string)($_SERVER['REMOTE_ADDR'] ?? ''),
    trim((string)($_SERVER['HTTP_X_FORWARDED_FOR'] ?? '')),
    trim((string)($_SERVER['HTTP_X_REAL_IP'] ?? '')),
  ];
  foreach ($candidates as $candidate) {
    if ($candidate === '') continue;
    $parts = array_map('trim', explode(',', $candidate));
    foreach ($parts as $part) {
      if (filter_var($part, FILTER_VALIDATE_IP)) {
        return $part;
      }
    }
  }
  return '';
}

function ip_is_loopback(string $ip): bool {
  if ($ip === '') return false;
  return $ip === '127.0.0.1' || $ip === '::1';
}

function ip_in_cidrs(string $ip, array $cidrs): bool {
  if ($ip === '' || !$cidrs) return false;
  foreach ($cidrs as $raw) {
    $cidr = trim((string)$raw);
    if ($cidr === '') continue;
    if (strpos($cidr, '/') === false) {
      if (hash_equals($cidr, $ip)) return true;
      continue;
    }
    [$base, $prefix] = explode('/', $cidr, 2);
    $prefixNum = (int)$prefix;
    $ipBin = @inet_pton($ip);
    $baseBin = @inet_pton($base);
    if ($ipBin === false || $baseBin === false || strlen($ipBin) !== strlen($baseBin)) continue;
    $bits = strlen($ipBin) * 8;
    if ($prefixNum < 0 || $prefixNum > $bits) continue;
    $bytes = intdiv($prefixNum, 8);
    $rem = $prefixNum % 8;
    $ok = true;
    for ($i = 0; $i < $bytes; $i++) {
      if (ord($ipBin[$i]) !== ord($baseBin[$i])) {
        $ok = false;
        break;
      }
    }
    if (!$ok) continue;
    if ($rem > 0) {
      $mask = 0xFF << (8 - $rem);
      if ((ord($ipBin[$bytes]) & $mask) !== (ord($baseBin[$bytes]) & $mask)) {
        continue;
      }
    }
    return true;
  }
  return false;
}

function enforce_network_policy(array $cfg): void {
  $allowRemote = (bool)($cfg['allow_remote_editor'] ?? false);
  if ($allowRemote) {
    $allowed = $cfg['allowed_editor_ips'] ?? [];
    if (!is_array($allowed) || !$allowed) return;
    $ip = get_request_ip();
    if (!ip_in_cidrs($ip, $allowed)) {
      respond(['ok' => false, 'error' => 'Access denied for this IP'], 403);
    }
    return;
  }
  $ip = get_request_ip();
  if (!ip_is_loopback($ip)) {
    respond(['ok' => false, 'error' => 'Editor API is available only from localhost'], 403);
  }
}

function bootstrap_editor_api(bool $requireAuth = true): void {
  apply_security_headers();
  $cfg = load_cfg();
  enforce_network_policy($cfg);
  if ($requireAuth) {
    ensure_editor_auth();
  } else {
    start_editor_session();
  }
}

function test_http_endpoint(string $url): array {
  if (!filter_var($url, FILTER_VALIDATE_URL)) {
    return ['ok' => false, 'error' => 'Invalid URL'];
  }
  if (!function_exists('curl_init')) {
    return ['ok' => false, 'error' => 'cURL extension is not available'];
  }
  $ch = curl_init($url);
  curl_setopt_array($ch, [
    CURLOPT_RETURNTRANSFER => true,
    CURLOPT_FOLLOWLOCATION => true,
    CURLOPT_MAXREDIRS => 3,
    CURLOPT_TIMEOUT => 8,
    CURLOPT_CONNECTTIMEOUT => 3,
    CURLOPT_USERAGENT => 'EditorStudio/1.0',
  ]);
  curl_exec($ch);
  $err = curl_error($ch);
  $code = (int)curl_getinfo($ch, CURLINFO_RESPONSE_CODE);
  curl_close($ch);

  if ($err) {
    return ['ok' => false, 'error' => $err];
  }
  if ($code < 200 || $code >= 400) {
    return ['ok' => false, 'error' => 'HTTP status ' . $code, 'status' => $code];
  }
  return ['ok' => true, 'status' => $code];
}


function ensure_editor_auth(): void {
  start_editor_session();
  $authorized = isset($_SESSION['editor_studio_auth']) && $_SESSION['editor_studio_auth'] === true;
  if (!$authorized) {
    respond(['ok' => false, 'error' => 'Unauthorized'], 401);
  }
}

function db_connect_server(): mysqli {
  $cfg = load_cfg();
  $host = (string)($cfg['host'] ?? '127.0.0.1');
  $port = (int)($cfg['port'] ?? 3306);
  $user = (string)($cfg['user'] ?? 'root');
  $pass = (string)($cfg['password'] ?? '');

  mysqli_report(MYSQLI_REPORT_OFF);
  $mysqli = @new mysqli($host, $user, $pass, '', $port);
  if ($mysqli->connect_errno) {
    throw new RuntimeException('DB connection failed: ' . $mysqli->connect_error);
  }
  $mysqli->set_charset('utf8mb4');
  return $mysqli;
}

function db_connect(): mysqli {
  $cfg = load_cfg();
  $db = (string)($cfg['database'] ?? '');
  $mysqli = db_connect_server();
  if ($db !== '' && !$mysqli->select_db($db)) {
    throw new RuntimeException('DB select failed: ' . $mysqli->error);
  }
  return $mysqli;
}
