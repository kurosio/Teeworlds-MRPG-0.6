<?php
// Admin-only SSH server collection and remote controls for Editor Studio.
declare(strict_types=1);

require_once __DIR__ . '/db-core.php';
bootstrap_editor_api();

const SSH_SERVERS_SETTING = 'ssh_servers';

function ssh_servers_load(): array
{
    $rows = store_settings_load()[SSH_SERVERS_SETTING] ?? [];
    if (!is_array($rows)) return [];
    return array_values(array_filter($rows, 'is_array'));
}

function ssh_servers_public(array $servers): array
{
    $fields = ['id', 'name', 'host', 'port', 'user', 'key_path', 'directory', 'enabled', 'update_command', 'start_command', 'stop_command'];
    return array_map(static function (array $server) use ($fields): array {
        return array_intersect_key($server, array_flip($fields));
    }, $servers);
}

function ssh_server_validate(array $input): array
{
    $id = trim((string)($input['id'] ?? ''));
    if ($id === '') $id = bin2hex(random_bytes(8));
    if (preg_match('/^[a-zA-Z0-9_-]{1,40}$/', $id) !== 1) throw new InvalidArgumentException('Некорректный ID сервера.');

    $name = trim((string)($input['name'] ?? ''));
    $host = trim((string)($input['host'] ?? ''));
    $user = trim((string)($input['user'] ?? ''));
    $port = (int)($input['port'] ?? 22);
    preg_match_all('/./us', $name, $nameChars);
    if ($name === '' || count($nameChars[0]) > 80) throw new InvalidArgumentException('Укажите название сервера (до 80 символов).');
    if (preg_match('/^(?=.{1,253}$)[a-zA-Z0-9](?:[a-zA-Z0-9.-]*[a-zA-Z0-9])?$/', $host) !== 1) throw new InvalidArgumentException('Укажите корректный SSH-хост (домен или IPv4).');
    if (preg_match('/^[a-z_][a-zA-Z0-9_.-]{0,63}$/', $user) !== 1) throw new InvalidArgumentException('Некорректное SSH-имя пользователя.');
    if ($port < 1 || $port > 65535) throw new InvalidArgumentException('SSH-порт должен быть от 1 до 65535.');

    $directory = trim((string)($input['directory'] ?? ''));
    if ($directory === '' || $directory[0] !== '/' || str_contains($directory, "\0") || strlen($directory) > 500) {
        throw new InvalidArgumentException('Укажите абсолютную рабочую директорию на сервере.');
    }
    $keyPath = trim((string)($input['key_path'] ?? ''));
    if (str_contains($keyPath, "\0") || strlen($keyPath) > 500) throw new InvalidArgumentException('Некорректный путь к SSH-ключу.');

    $commands = [];
    foreach (['update_command', 'start_command', 'stop_command'] as $key) {
        $command = trim((string)($input[$key] ?? ''));
        if (strlen($command) > 2000 || str_contains($command, "\0") || preg_match('/[\r\n]/', $command)) {
            throw new InvalidArgumentException('SSH-команды должны быть однострочными и не длиннее 2000 символов.');
        }
        $commands[$key] = $command;
    }

    return [
        'id' => $id,
        'name' => $name,
        'host' => $host,
        'port' => $port,
        'user' => $user,
        'key_path' => $keyPath,
        'directory' => $directory,
        'enabled' => !empty($input['enabled']),
        'update_command' => $commands['update_command'],
        'start_command' => $commands['start_command'],
        'stop_command' => $commands['stop_command'],
    ];
}

function ssh_shell_quote(string $value): string
{
    return "'" . str_replace("'", "'\\''", $value) . "'";
}

/** Run an administrator-configured command over SSH without invoking a local shell. */
function ssh_server_run(array $server, string $remoteCommand): array
{
    $knownHosts = dirname(__DIR__) . '/data/ssh-known-hosts';
    $args = ['ssh', '-p', (string)$server['port'], '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', '-o', 'StrictHostKeyChecking=accept-new', '-o', 'UserKnownHostsFile=' . $knownHosts];
    if ($server['key_path'] !== '') {
        $args[] = '-i';
        $args[] = $server['key_path'];
        $args[] = '-o';
        $args[] = 'IdentitiesOnly=yes';
    }
    $args[] = $server['user'] . '@' . $server['host'];
    $args[] = $remoteCommand;
    $pipes = [];
    $process = @proc_open($args, [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    if (!is_resource($process)) throw new RuntimeException('Не удалось запустить SSH-клиент на веб-сервере.');
    fclose($pipes[0]);
    $stdout = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[2]);
    $status = proc_close($process);
    return ['code' => $status, 'output' => trim((string)$stdout . "\n" . (string)$stderr)];
}

try {
    $method = strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET'));
    if ($method === 'GET') {
        respond(['ok' => true, 'servers' => ssh_servers_public(ssh_servers_load())]);
    }
    if ($method !== 'POST') respond(['ok' => false, 'error' => 'Метод запроса не поддерживается'], 405);
    require_same_origin_json_post();
    $body = read_json_body();
    $action = (string)($body['action'] ?? 'save');
    $servers = ssh_servers_load();

    if ($action === 'save') {
        $incoming = $body['servers'] ?? null;
        if (!is_array($incoming) || count($incoming) > 100) throw new InvalidArgumentException('Ожидался список до 100 серверов.');
        $validated = [];
        $ids = [];
        foreach ($incoming as $item) {
            if (!is_array($item)) throw new InvalidArgumentException('Некорректная запись сервера.');
            $server = ssh_server_validate($item);
            if (isset($ids[$server['id']])) throw new InvalidArgumentException('ID серверов должны быть уникальными.');
            $ids[$server['id']] = true;
            $validated[] = $server;
        }
        store_settings_set(SSH_SERVERS_SETTING, $validated);
        respond(['ok' => true, 'servers' => ssh_servers_public($validated)]);
    }

    if (!in_array($action, ['update', 'start', 'stop'], true)) {
        respond(['ok' => false, 'error' => 'Неизвестное действие.'], 400);
    }
    $ids = $body['ids'] ?? [];
    if (!is_array($ids) || count($ids) < 1 || count($ids) > 100) throw new InvalidArgumentException('Выберите хотя бы один сервер.');
    $selected = [];
    foreach ($servers as $server) if (in_array($server['id'] ?? '', $ids, true) && !empty($server['enabled'])) $selected[] = $server;
    if (!$selected) throw new InvalidArgumentException('Среди выбранных серверов нет активных.');

    @set_time_limit(600);
    $results = [];
    foreach ($selected as $server) {
        try {
            $commandField = ['update' => 'update_command', 'start' => 'start_command', 'stop' => 'stop_command'][$action];
            $command = trim((string)($server[$commandField] ?? ''));
            if ($command === '') throw new InvalidArgumentException('Для этого сервера не задана команда.');
            $remoteCommand = 'cd -- ' . ssh_shell_quote($server['directory']) . ' && ' . $command;
            $result = ssh_server_run($server, $remoteCommand);
            $results[] = ['id' => $server['id'], 'name' => $server['name'], 'ok' => $result['code'] === 0, 'message' => $result['code'] === 0 ? ($result['output'] ?: 'Готово') : ('SSH завершился с кодом ' . $result['code'] . ($result['output'] !== '' ? ': ' . substr($result['output'], 0, 500) : ''))];
        } catch (Throwable $e) {
            $results[] = ['id' => $server['id'], 'name' => $server['name'], 'ok' => false, 'message' => $e instanceof InvalidArgumentException ? $e->getMessage() : 'Ошибка операции. Проверьте SSH-доступ и журнал PHP.'];
            editor_log_error($e);
        }
    }
    // A completed batch is an API success even if individual hosts failed.
    respond(['ok' => true, 'results' => $results]);
} catch (Throwable $e) {
    if ($e instanceof InvalidArgumentException) respond(['ok' => false, 'error' => $e->getMessage()], 400);
    editor_log_error($e);
    respond(['ok' => false, 'error' => 'Не удалось обработать SSH-операцию. Подробности в журнале сервера.'], 500);
}
