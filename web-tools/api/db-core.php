<?php
// Shared helpers for DB API endpoints.

declare(strict_types=1);

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
