<?php
// DB maintenance API: dumps and restores.
// Endpoint: api/db-maintenance.php
// Actions:
//  - list_dumps (GET)
//  - create_dump (POST)
//  - restore_dump (POST)
//  - delete_dump (POST)
//  - download (GET)

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');

require_once __DIR__ . '/db-core.php';

ensure_editor_auth();

function dumps_dir(): string {
  return dirname(__DIR__) . '/data/db-dumps';
}

function ensure_dumps_dir(): string {
  $dir = dumps_dir();
  if (!is_dir($dir)) {
    @mkdir($dir, 0775, true);
  }
  return $dir;
}

function sanitize_label(string $raw): string {
  $clean = preg_replace('/[^a-zA-Z0-9_\-]+/', '_', $raw);
  return trim($clean ?? '', '_');
}

function normalize_upper(string $value): string {
  $value = trim($value);
  if (function_exists('mb_strtoupper')) {
    return mb_strtoupper($value, 'UTF-8');
  }
  return strtoupper($value);
}

function ends_with(string $value, string $suffix): bool {
  if ($suffix === '') return true;
  $len = strlen($suffix);
  if ($len === 0) return true;
  if (strlen($value) < $len) return false;
  return substr($value, -$len) === $suffix;
}

function validate_dump_name(string $file): string {
  $file = basename($file);
  if ($file === '' || !ends_with($file, '.sql')) {
    respond(['ok' => false, 'error' => 'Invalid dump name'], 400);
  }
  return $file;
}

function dump_path(string $file, bool $mustExist = false): string {
  $path = dumps_dir() . '/' . $file;
  if ($mustExist && !is_file($path)) {
    respond(['ok' => false, 'error' => 'Dump not found'], 404);
  }
  return $path;
}

function db_conn_params(array $cfg): array {
  return [
    'host' => (string)($cfg['host'] ?? '127.0.0.1'),
    'port' => (int)($cfg['port'] ?? 3306),
    'user' => (string)($cfg['user'] ?? 'root'),
    'pass' => (string)($cfg['password'] ?? ''),
    'db' => (string)($cfg['database'] ?? ''),
  ];
}

function require_confirm_db(array $cfg, string $provided): void {
  $dbName = (string)($cfg['database'] ?? '');
  if ($dbName === '') {
    respond(['ok' => false, 'error' => 'Database name is not configured'], 400);
  }
  if (trim($provided) !== $dbName) {
    respond(['ok' => false, 'error' => 'Confirmation database name mismatch'], 400);
  }
}

function find_binary(string $bin): string {
  $out = [];
  $code = 0;
  @exec('command -v ' . escapeshellarg($bin), $out, $code);
  if ($code === 0 && !empty($out[0])) return trim($out[0]);

  $paths = [
    '/usr/bin/' . $bin,
    '/usr/local/bin/' . $bin,
    '/bin/' . $bin,
    '/usr/sbin/' . $bin,
    '/usr/local/mysql/bin/' . $bin,
  ];
  foreach ($paths as $path) {
    if (is_file($path) && is_executable($path)) return $path;
  }
  return '';
}

function run_process(string $cmd, array $env = []): array {
  $descriptor = [
    0 => ['pipe', 'r'],
    1 => ['pipe', 'w'],
    2 => ['pipe', 'w'],
  ];
  $process = proc_open($cmd, $descriptor, $pipes, null, $env ?: null);
  if (!is_resource($process)) {
    return ['ok' => false, 'error' => 'Failed to start process'];
  }
  fclose($pipes[0]);
  $stdout = stream_get_contents($pipes[1]);
  $stderr = stream_get_contents($pipes[2]);
  fclose($pipes[1]);
  fclose($pipes[2]);
  $code = proc_close($process);
  if ($code !== 0) {
    return ['ok' => false, 'error' => trim($stderr) ?: 'Command failed', 'stdout' => trim($stdout)];
  }
  return ['ok' => true, 'stdout' => trim($stdout)];
}

function run_restore(string $cmd, string $file, array $env = []): array {
  $descriptor = [
    0 => ['file', $file, 'r'],
    1 => ['pipe', 'w'],
    2 => ['pipe', 'w'],
  ];
  $process = proc_open($cmd, $descriptor, $pipes, null, $env ?: null);
  if (!is_resource($process)) {
    return ['ok' => false, 'error' => 'Failed to start process'];
  }
  $stdout = stream_get_contents($pipes[1]);
  $stderr = stream_get_contents($pipes[2]);
  fclose($pipes[1]);
  fclose($pipes[2]);
  $code = proc_close($process);
  if ($code !== 0) {
    return ['ok' => false, 'error' => trim($stderr) ?: 'Command failed', 'stdout' => trim($stdout)];
  }
  return ['ok' => true, 'stdout' => trim($stdout)];
}

function dump_database_php(mysqli $mysqli, string $dbName, string $path): array {
  $fh = @fopen($path, 'w');
  if (!$fh) return ['ok' => false, 'error' => 'Failed to open dump file'];

  $header = "-- Editor Studio dump\n";
  $header .= "-- Database: `{$dbName}`\n";
  $header .= "-- Generated at: " . date('c') . "\n\n";
  $header .= "SET FOREIGN_KEY_CHECKS=0;\n";
  $header .= "SET SQL_MODE='NO_AUTO_VALUE_ON_ZERO';\n";
  $header .= "SET NAMES utf8mb4;\n\n";
  fwrite($fh, $header);

  $tables = [];
  $resTables = $mysqli->query('SHOW FULL TABLES WHERE Table_type = \'BASE TABLE\'');
  if ($resTables) {
    while ($row = $resTables->fetch_array(MYSQLI_NUM)) {
      $tables[] = $row[0];
    }
    $resTables->free();
  }

  foreach ($tables as $table) {
    $resCreate = $mysqli->query('SHOW CREATE TABLE `' . $mysqli->real_escape_string($table) . '`');
    if (!$resCreate) continue;
    $row = $resCreate->fetch_assoc();
    $resCreate->free();
    if (!$row || empty($row['Create Table'])) continue;

    fwrite($fh, "--\n-- Table structure for `{$table}`\n--\n");
    fwrite($fh, "DROP TABLE IF EXISTS `{$table}`;\n");
    fwrite($fh, $row['Create Table'] . ";\n\n");

    $resData = $mysqli->query('SELECT * FROM `' . $mysqli->real_escape_string($table) . '`');
    if ($resData) {
      $fields = $resData->fetch_fields();
      $columns = array_map(static fn($f) => '`' . $f->name . '`', $fields);
      $colList = implode(',', $columns);

      $batch = [];
      $batchSize = 200;
      while ($data = $resData->fetch_assoc()) {
        $values = [];
        foreach ($fields as $f) {
          $val = $data[$f->name];
          if ($val === null) {
            $values[] = 'NULL';
          } else {
            $values[] = "'" . $mysqli->real_escape_string((string)$val) . "'";
          }
        }
        $batch[] = '(' . implode(',', $values) . ')';
        if (count($batch) >= $batchSize) {
          fwrite($fh, "INSERT INTO `{$table}` ({$colList}) VALUES\n" . implode(",\n", $batch) . ";\n");
          $batch = [];
        }
      }
      if ($batch) {
        fwrite($fh, "INSERT INTO `{$table}` ({$colList}) VALUES\n" . implode(",\n", $batch) . ";\n");
      }
      fwrite($fh, "\n");
      $resData->free();
    }
  }

  fwrite($fh, "SET FOREIGN_KEY_CHECKS=1;\n");
  fclose($fh);
  return ['ok' => true];
}

function exec_sql_stream(mysqli $mysqli, string $path): array {
  $file = new SplFileObject($path, 'r');
  $buffer = '';
  $inSingle = false;
  $inDouble = false;
  $inBacktick = false;
  $escape = false;

  while (!$file->eof()) {
    $line = $file->fgets();
    $len = strlen($line);
    for ($i = 0; $i < $len; $i++) {
      $ch = $line[$i];
      if ($escape) {
        $escape = false;
      } elseif ($ch === '\\') {
        $escape = true;
      } elseif (!$inDouble && !$inBacktick && $ch === "'") {
        $inSingle = !$inSingle;
      } elseif (!$inSingle && !$inBacktick && $ch === '"') {
        $inDouble = !$inDouble;
      } elseif (!$inSingle && !$inDouble && $ch === '`') {
        $inBacktick = !$inBacktick;
      }

      $buffer .= $ch;
      if ($ch === ';' && !$inSingle && !$inDouble && !$inBacktick) {
        $statement = trim($buffer);
        $buffer = '';
        if ($statement === '' || $statement === ';') {
          continue;
        }
        if (!$mysqli->query($statement)) {
          return ['ok' => false, 'error' => $mysqli->error ?: 'Restore failed'];
        }
      }
    }
  }

  $tail = trim($buffer);
  if ($tail !== '') {
    if (!$mysqli->query($tail)) {
      return ['ok' => false, 'error' => $mysqli->error ?: 'Restore failed'];
    }
  }
  return ['ok' => true];
}

function restore_database_php(mysqli $mysqli, string $path): array {
  if (!$mysqli->query('SET FOREIGN_KEY_CHECKS=0')) {
    return ['ok' => false, 'error' => $mysqli->error ?: 'Restore failed'];
  }
  $res = exec_sql_stream($mysqli, $path);
  $mysqli->query('SET FOREIGN_KEY_CHECKS=1');
  return $res;
}

function reset_database(array $cfg): void {
  $dbName = (string)($cfg['database'] ?? '');
  if ($dbName === '') {
    respond(['ok' => false, 'error' => 'Database name is not configured'], 400);
  }
  $mysqli = db_connect_server();
  $safe = $mysqli->real_escape_string($dbName);
  $mysqli->query("DROP DATABASE IF EXISTS `{$safe}`");
  $mysqli->query("CREATE DATABASE `{$safe}` CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci");
  if ($mysqli->errno) {
    $err = $mysqli->error ?: 'Failed to recreate database';
    $mysqli->close();
    respond(['ok' => false, 'error' => $err], 500);
  }
  $mysqli->close();
}

$action = (string)($_GET['action'] ?? '');

try {
  if ($action === 'download') {
    $file = validate_dump_name((string)($_GET['file'] ?? ''));
    $path = dump_path($file, true);
    header('Content-Type: application/sql');
    header('Content-Disposition: attachment; filename="' . $file . '"');
    header('Content-Length: ' . filesize($path));
    readfile($path);
    exit;
  }

  if ($action === 'list_dumps') {
    $dir = ensure_dumps_dir();
    $files = glob($dir . '/*.sql') ?: [];
    $items = [];
    foreach ($files as $path) {
      $name = basename($path);
      $stat = @stat($path);
      $items[] = [
        'file' => $name,
        'size' => $stat ? (int)$stat['size'] : 0,
        'mtime' => $stat ? (int)$stat['mtime'] : 0,
      ];
    }
    usort($items, static fn($a, $b) => ($b['mtime'] ?? 0) <=> ($a['mtime'] ?? 0));
    respond(['ok' => true, 'items' => $items]);
  }

  if ($action === 'create_dump') {
    $body = read_json_body();
    $cfg = load_cfg();
    require_confirm_db($cfg, (string)($body['confirm_db'] ?? ''));

    $label = sanitize_label((string)($body['label'] ?? ''));
    $dbName = (string)($cfg['database'] ?? '');
    $dbSafe = sanitize_label($dbName) ?: 'db';
    $suffix = $label !== '' ? ('_' . $label) : '';
    $timestamp = date('Ymd_His');
    $filename = $dbSafe . '_' . $timestamp . $suffix . '.sql';
    $dir = ensure_dumps_dir();
    $path = $dir . '/' . $filename;

    $conn = db_conn_params($cfg);
    $host = $conn['host'];
    $port = $conn['port'];
    $user = $conn['user'];
    $pass = $conn['pass'];

    $bin = find_binary('mysqldump');
    if ($bin !== '') {
      $cmd = sprintf(
        '%s --host=%s --port=%d --user=%s --single-transaction --routines --events --triggers --hex-blob --skip-extended-insert --max-allowed-packet=64M --default-character-set=utf8mb4 --databases %s --result-file=%s',
        escapeshellarg($bin),
        escapeshellarg($host),
        $port,
        escapeshellarg($user),
        escapeshellarg($conn['db']),
        escapeshellarg($path)
      );

      $env = [];
      if ($pass !== '') $env['MYSQL_PWD'] = $pass;

      $res = run_process($cmd, $env);
      if (!$res['ok']) {
        respond(['ok' => false, 'error' => $res['error'] ?? 'Dump failed'], 500);
      }
    } else {
      $mysqli = db_connect();
      $res = dump_database_php($mysqli, $dbName, $path);
      $mysqli->close();
      if (!$res['ok']) {
        respond(['ok' => false, 'error' => $res['error'] ?? 'Dump failed'], 500);
      }
    }
    respond(['ok' => true, 'file' => $filename]);
  }

  if ($action === 'restore_dump') {
    $body = read_json_body();
    $cfg = load_cfg();
    require_confirm_db($cfg, (string)($body['confirm_db'] ?? ''));

    $phrase = normalize_upper((string)($body['confirm_phrase'] ?? ''));
    if ($phrase !== 'ВОССТАНОВИТЬ' && $phrase !== 'RESTORE') {
      respond(['ok' => false, 'error' => 'Confirmation phrase mismatch'], 400);
    }

    $file = validate_dump_name((string)($body['file'] ?? ''));
    $path = dump_path($file, true);

    reset_database($cfg);

    $conn = db_conn_params($cfg);
    $host = $conn['host'];
    $port = $conn['port'];
    $user = $conn['user'];
    $pass = $conn['pass'];

    $bin = find_binary('mysql');
    if ($bin !== '') {
      $cmd = sprintf(
        '%s --host=%s --port=%d --user=%s --default-character-set=utf8mb4 --max-allowed-packet=64M --database=%s --init-command=%s',
        escapeshellarg($bin),
        escapeshellarg($host),
        $port,
        escapeshellarg($user),
        escapeshellarg($conn['db']),
        escapeshellarg('SET FOREIGN_KEY_CHECKS=0')
      );

      $env = [];
      if ($pass !== '') $env['MYSQL_PWD'] = $pass;

      $res = run_restore($cmd, $path, $env);
      if (!$res['ok']) {
        respond(['ok' => false, 'error' => $res['error'] ?? 'Restore failed'], 500);
      }
    } else {
      $mysqli = db_connect();
      $res = restore_database_php($mysqli, $path);
      $mysqli->close();
      if (!$res['ok']) {
        respond(['ok' => false, 'error' => $res['error'] ?? 'Restore failed'], 500);
      }
    }
    respond(['ok' => true]);
  }

  if ($action === 'delete_dump') {
    $body = read_json_body();
    $cfg = load_cfg();
    require_confirm_db($cfg, (string)($body['confirm_db'] ?? ''));

    $file = validate_dump_name((string)($body['file'] ?? ''));
    $path = dump_path($file, true);
    if (!@unlink($path)) {
      respond(['ok' => false, 'error' => 'Failed to delete dump'], 500);
    }
    respond(['ok' => true]);
  }

  respond(['ok' => false, 'error' => 'Unknown action'], 400);

} catch (Throwable $e) {
  respond(['ok' => false, 'error' => $e->getMessage()], 500);
}
