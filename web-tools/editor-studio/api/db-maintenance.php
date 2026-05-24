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

require_once __DIR__ . '/db-core.php';

bootstrap_editor_api(true);

// ── Helpers ──────────────────────────────────────────────────────────────────

function dumps_dir(): string
{
    return dirname(__DIR__) . '/data/db-dumps';
}

function ensure_dumps_dir(): string
{
    $dir = dumps_dir();
    if (!is_dir($dir) && !@mkdir($dir, 0775, true) && !is_dir($dir)) {
        respond(['ok' => false, 'error' => 'Cannot create dumps directory'], 500);
    }
    return $dir;
}

function sanitize_label(string $raw): string
{
    return trim((string)preg_replace('/[^a-zA-Z0-9_\-]+/', '_', $raw), '_');
}

function normalize_upper(string $value): string
{
    return mb_strtoupper(trim($value), 'UTF-8');
}

function validate_dump_name(string $file): string
{
    $file = basename($file);
    if ($file === '' || !str_ends_with($file, '.sql') || str_contains($file, '..')) {
        respond(['ok' => false, 'error' => 'Invalid dump name'], 400);
    }
    return $file;
}

function dump_path(string $file, bool $mustExist = false): string
{
    $path = dumps_dir() . '/' . $file;

    // Защита от path traversal
    $realDir  = realpath(dumps_dir());
    $realPath = $mustExist ? realpath($path) : false;

    if ($mustExist) {
        if ($realPath === false || !is_file($realPath)) {
            respond(['ok' => false, 'error' => 'Dump not found'], 404);
        }
        if ($realDir === false || !str_starts_with($realPath, $realDir . DIRECTORY_SEPARATOR)) {
            respond(['ok' => false, 'error' => 'Invalid dump path'], 400);
        }
        return $realPath;
    }

    return $path;
}

function db_conn_params(array $cfg): array
{
    return [
        'host' => (string)($cfg['host']     ?? '127.0.0.1'),
        'port' => (int)($cfg['port']        ?? 3306),
        'user' => (string)($cfg['user']     ?? 'root'),
        'pass' => (string)($cfg['password'] ?? ''),
        'db'   => (string)($cfg['database'] ?? ''),
    ];
}

function require_confirm_db(array $cfg, string $provided): void
{
    $dbName = (string)($cfg['database'] ?? '');
    if ($dbName === '') {
        respond(['ok' => false, 'error' => 'Database name is not configured'], 400);
    }
    if (trim($provided) !== $dbName) {
        respond(['ok' => false, 'error' => 'Confirmation database name mismatch'], 400);
    }
}

// ── External binary helpers ──────────────────────────────────────────────────

function find_binary(string $bin): string
{
    // Разрешаем только известные бинарники
    if (!in_array($bin, ['mysql', 'mysqldump'], true)) {
        return '';
    }

    $out  = [];
    $code = 0;
    @exec('command -v ' . escapeshellarg($bin) . ' 2>/dev/null', $out, $code);
    if ($code === 0 && !empty($out[0])) {
        return trim($out[0]);
    }

    $searchPaths = [
        '/usr/bin/',
        '/usr/local/bin/',
        '/bin/',
        '/usr/sbin/',
        '/usr/local/mysql/bin/',
    ];
    foreach ($searchPaths as $prefix) {
        $path = $prefix . $bin;
        if (is_file($path) && is_executable($path)) {
            return $path;
        }
    }
    return '';
}

function run_process(string $cmd, array $env = []): array
{
    $descriptors = [
        0 => ['pipe', 'r'],
        1 => ['pipe', 'w'],
        2 => ['pipe', 'w'],
    ];
    $process = proc_open($cmd, $descriptors, $pipes, null, $env ?: null);
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
        return [
            'ok'     => false,
            'error'  => trim((string)$stderr) ?: 'Command failed (exit code ' . $code . ')',
            'stdout' => trim((string)$stdout),
        ];
    }
    return ['ok' => true, 'stdout' => trim((string)$stdout)];
}

function run_restore(string $cmd, string $file, array $env = []): array
{
    $descriptors = [
        0 => ['file', $file, 'r'],
        1 => ['pipe', 'w'],
        2 => ['pipe', 'w'],
    ];
    $process = proc_open($cmd, $descriptors, $pipes, null, $env ?: null);
    if (!is_resource($process)) {
        return ['ok' => false, 'error' => 'Failed to start process'];
    }

    $stdout = stream_get_contents($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);

    $code = proc_close($process);
    if ($code !== 0) {
        return [
            'ok'     => false,
            'error'  => trim((string)$stderr) ?: 'Command failed (exit code ' . $code . ')',
            'stdout' => trim((string)$stdout),
        ];
    }
    return ['ok' => true, 'stdout' => trim((string)$stdout)];
}

// ── PHP-level dump / restore ─────────────────────────────────────────────────

function dump_database_php(mysqli $mysqli, string $dbName, string $path): array
{
    $fh = @fopen($path, 'wb');
    if (!$fh) {
        return ['ok' => false, 'error' => 'Failed to open dump file for writing'];
    }

    try {
        $header = "-- Editor Studio dump\n"
            . "-- Database: `" . addcslashes($dbName, '`') . "`\n"
            . "-- Generated at: " . date('c') . "\n\n"
            . "SET FOREIGN_KEY_CHECKS=0;\n"
            . "SET SQL_MODE='NO_AUTO_VALUE_ON_ZERO';\n"
            . "SET NAMES utf8mb4;\n\n";
        fwrite($fh, $header);

        // Собираем список таблиц
        $tables   = [];
        $resTables = $mysqli->query("SHOW FULL TABLES WHERE Table_type = 'BASE TABLE'");
        if ($resTables) {
            while ($row = $resTables->fetch_array(MYSQLI_NUM)) {
                $tables[] = $row[0];
            }
            $resTables->free();
        }

        $batchSize = 200;

        foreach ($tables as $table) {
            $escapedTable = $mysqli->real_escape_string($table);

            $resCreate = $mysqli->query("SHOW CREATE TABLE `{$escapedTable}`");
            if (!$resCreate) {
                continue;
            }
            $createRow = $resCreate->fetch_assoc();
            $resCreate->free();
            if (!$createRow || empty($createRow['Create Table'])) {
                continue;
            }

            fwrite($fh, "--\n-- Table structure for `{$table}`\n--\n");
            fwrite($fh, "DROP TABLE IF EXISTS `{$escapedTable}`;\n");
            fwrite($fh, $createRow['Create Table'] . ";\n\n");

            // Данные таблицы (unbuffered для экономии памяти на больших таблицах)
            $resData = $mysqli->query("SELECT * FROM `{$escapedTable}`", MYSQLI_USE_RESULT);
            if (!$resData) {
                continue;
            }

            $fields  = $resData->fetch_fields();
            $colList = implode(',', array_map(
                static fn(object $f): string => '`' . $f->name . '`',
                $fields
            ));

            $batch = [];
            while ($data = $resData->fetch_assoc()) {
                $values = [];
                foreach ($fields as $f) {
                    $val = $data[$f->name];
                    $values[] = $val === null
                        ? 'NULL'
                        : "'" . $mysqli->real_escape_string((string)$val) . "'";
                }
                $batch[] = '(' . implode(',', $values) . ')';

                if (count($batch) >= $batchSize) {
                    fwrite($fh, "INSERT INTO `{$escapedTable}` ({$colList}) VALUES\n"
                        . implode(",\n", $batch) . ";\n");
                    $batch = [];
                }
            }
            if ($batch !== []) {
                fwrite($fh, "INSERT INTO `{$escapedTable}` ({$colList}) VALUES\n"
                    . implode(",\n", $batch) . ";\n");
            }
            fwrite($fh, "\n");
            $resData->free();
        }

        fwrite($fh, "SET FOREIGN_KEY_CHECKS=1;\n");
    } finally {
        fclose($fh);
    }

    return ['ok' => true];
}

function exec_sql_stream(mysqli $mysqli, string $path): array
{
    $file      = new SplFileObject($path, 'rb');
    $buffer    = '';
    $inSingle  = false;
    $inDouble  = false;
    $inBacktick = false;
    $escape    = false;

    while (!$file->eof()) {
        $line = $file->fgets();
        if ($line === false) {
            break;
        }

        // Пропускаем чистые комментарии (оптимизация)
        $trimmed = ltrim($line);
        if (!$inSingle && !$inDouble && !$inBacktick && $buffer === '') {
            if ($trimmed === '' || str_starts_with($trimmed, '--') || str_starts_with($trimmed, '#')) {
                continue;
            }
        }

        $len = strlen($line);
        for ($i = 0; $i < $len; $i++) {
            $ch = $line[$i];

            if ($escape) {
                $buffer .= $ch;
                $escape = false;
                continue;
            }

            if ($ch === '\\' && ($inSingle || $inDouble)) {
                $escape = true;
                $buffer .= $ch;
                continue;
            }

            if (!$inDouble && !$inBacktick && $ch === "'") {
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

function restore_database_php(mysqli $mysqli, string $path): array
{
    if (!$mysqli->query('SET FOREIGN_KEY_CHECKS=0')) {
        return ['ok' => false, 'error' => $mysqli->error ?: 'Failed to disable FK checks'];
    }
    $res = exec_sql_stream($mysqli, $path);
    $mysqli->query('SET FOREIGN_KEY_CHECKS=1');
    return $res;
}

function reset_database(array $cfg): void
{
    $dbName = (string)($cfg['database'] ?? '');
    if ($dbName === '') {
        respond(['ok' => false, 'error' => 'Database name is not configured'], 400);
    }

    $mysqli = db_connect_server();
    try {
        $safe = $mysqli->real_escape_string($dbName);
        if (!$mysqli->query("DROP DATABASE IF EXISTS `{$safe}`")) {
            respond(['ok' => false, 'error' => $mysqli->error ?: 'Failed to drop database'], 500);
        }
        if (!$mysqli->query("CREATE DATABASE `{$safe}` CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci")) {
            respond(['ok' => false, 'error' => $mysqli->error ?: 'Failed to create database'], 500);
        }
    } finally {
        $mysqli->close();
    }
}

// ── Build CLI commands ───────────────────────────────────────────────────────

function build_mysqldump_cmd(string $bin, array $conn, string $outPath): string
{
    return sprintf(
        '%s --host=%s --port=%d --user=%s'
        . ' --single-transaction --routines --events --triggers'
        . ' --hex-blob --skip-extended-insert'
        . ' --max-allowed-packet=64M --default-character-set=utf8mb4'
        . ' --databases %s --result-file=%s',
        escapeshellarg($bin),
        escapeshellarg($conn['host']),
        $conn['port'],
        escapeshellarg($conn['user']),
        escapeshellarg($conn['db']),
        escapeshellarg($outPath)
    );
}

function build_mysql_cmd(string $bin, array $conn): string
{
    return sprintf(
        '%s --host=%s --port=%d --user=%s'
        . ' --default-character-set=utf8mb4 --max-allowed-packet=64M'
        . ' --database=%s --init-command=%s',
        escapeshellarg($bin),
        escapeshellarg($conn['host']),
        $conn['port'],
        escapeshellarg($conn['user']),
        escapeshellarg($conn['db']),
        escapeshellarg('SET FOREIGN_KEY_CHECKS=0')
    );
}

function env_with_password(string $pass): array
{
    return $pass !== '' ? ['MYSQL_PWD' => $pass] : [];
}

// ── Action handlers ──────────────────────────────────────────────────────────

function handle_download(): void
{
    $file = validate_dump_name((string)($_GET['file'] ?? ''));
    $path = dump_path($file, true);
    $size = filesize($path);

    header('Content-Type: application/sql');
    header('Content-Disposition: attachment; filename="' . $file . '"');
    if ($size !== false) {
        header('Content-Length: ' . $size);
    }
    readfile($path);
    exit;
}

function handle_list_dumps(): never
{
    $dir   = ensure_dumps_dir();
    $files = glob($dir . '/*.sql') ?: [];
    $items = [];

    foreach ($files as $filePath) {
        $stat    = @stat($filePath);
        $items[] = [
            'file'  => basename($filePath),
            'size'  => $stat ? (int)$stat['size'] : 0,
            'mtime' => $stat ? (int)$stat['mtime'] : 0,
        ];
    }

    usort($items, static fn(array $a, array $b): int => $b['mtime'] <=> $a['mtime']);
    respond(['ok' => true, 'items' => $items]);
}

function handle_create_dump(): never
{
    $body = read_json_body();
    $cfg  = load_cfg();
    require_confirm_db($cfg, (string)($body['confirm_db'] ?? ''));

    $label   = sanitize_label((string)($body['label'] ?? ''));
    $dbName  = (string)($cfg['database'] ?? '');
    $dbSafe  = sanitize_label($dbName) ?: 'db';
    $suffix  = $label !== '' ? ('_' . $label) : '';
    $filename = $dbSafe . '_' . date('Ymd_His') . $suffix . '.sql';

    $dir  = ensure_dumps_dir();
    $path = $dir . '/' . $filename;
    $conn = db_conn_params($cfg);

    $bin = find_binary('mysqldump');
    if ($bin !== '') {
        $cmd = build_mysqldump_cmd($bin, $conn, $path);
        $res = run_process($cmd, env_with_password($conn['pass']));
        if (!$res['ok']) {
            // Удаляем частично записанный файл
            @unlink($path);
            respond(['ok' => false, 'error' => $res['error'] ?? 'Dump failed'], 500);
        }
    } else {
        $mysqli = db_connect();
        try {
            $res = dump_database_php($mysqli, $dbName, $path);
        } finally {
            $mysqli->close();
        }
        if (!$res['ok']) {
            @unlink($path);
            respond(['ok' => false, 'error' => $res['error'] ?? 'Dump failed'], 500);
        }
    }

    respond(['ok' => true, 'file' => $filename]);
}

function handle_restore_dump(): never
{
    $body = read_json_body();
    $cfg  = load_cfg();
    require_confirm_db($cfg, (string)($body['confirm_db'] ?? ''));

    $phrase = normalize_upper((string)($body['confirm_phrase'] ?? ''));
    if ($phrase !== 'ВОССТАНОВИТЬ' && $phrase !== 'RESTORE') {
        respond(['ok' => false, 'error' => 'Confirmation phrase mismatch'], 400);
    }

    $file = validate_dump_name((string)($body['file'] ?? ''));
    $path = dump_path($file, true);

    reset_database($cfg);

    $conn = db_conn_params($cfg);

    $bin = find_binary('mysql');
    if ($bin !== '') {
        $cmd = build_mysql_cmd($bin, $conn);
        $res = run_restore($cmd, $path, env_with_password($conn['pass']));
        if (!$res['ok']) {
            respond(['ok' => false, 'error' => $res['error'] ?? 'Restore failed'], 500);
        }
    } else {
        $mysqli = db_connect();
        try {
            $res = restore_database_php($mysqli, $path);
        } finally {
            $mysqli->close();
        }
        if (!$res['ok']) {
            respond(['ok' => false, 'error' => $res['error'] ?? 'Restore failed'], 500);
        }
    }

    respond(['ok' => true]);
}

function handle_delete_dump(): never
{
    $body = read_json_body();
    $cfg  = load_cfg();
    require_confirm_db($cfg, (string)($body['confirm_db'] ?? ''));

    $file = validate_dump_name((string)($body['file'] ?? ''));
    $path = dump_path($file, true);

    if (!@unlink($path)) {
        respond(['ok' => false, 'error' => 'Failed to delete dump'], 500);
    }
    respond(['ok' => true]);
}

// ── Router ───────────────────────────────────────────────────────────────────

$action = (string)($_GET['action'] ?? '');

$handlers = [
    'download'     => 'handle_download',
    'list_dumps'   => 'handle_list_dumps',
    'create_dump'  => 'handle_create_dump',
    'restore_dump' => 'handle_restore_dump',
    'delete_dump'  => 'handle_delete_dump',
];

try {
    if (isset($handlers[$action])) {
        $handlers[$action]();
    } else {
        respond(['ok' => false, 'error' => 'Unknown action'], 400);
    }
} catch (Throwable $e) {
    respond(['ok' => false, 'error' => $e->getMessage()], 500);
}