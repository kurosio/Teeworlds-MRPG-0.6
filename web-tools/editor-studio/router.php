<?php
// router.php — роутер для встроенного сервера PHP:
//
//   cd web-tools/editor-studio
//   php -S 127.0.0.1:8080 router.php
//
// Зачем он нужен: встроенный сервер PHP не читает .htaccess, поэтому без
// роутера любой желающий скачал бы data/editor-studio.sqlite (пароль
// администратора и доступ к игровой базе) и data/db-config.json. Роутер
// закрывает эти пути так же, как это делают apache-config.conf и data/.htaccess
// на «взрослом» сервере.
//
// Для обычного хостинга роутер не нужен — подключать его в Apache/Nginx не надо.

declare(strict_types=1);

$path = (string)parse_url((string)($_SERVER['REQUEST_URI'] ?? '/'), PHP_URL_PATH);
$path = rawurldecode($path);

$hidden = preg_match('#(^|/)data(/|$)#i', $path) === 1
    || preg_match('#(^|/)\.(git|env)#i', $path) === 1
    // Сценарии читаются только через api.php (доступ администратора),
    // статикой они не отдаются — как и в apache-config.conf.
    || preg_match('#(^|/)scenarios/.+\.json$#i', $path) === 1
    || preg_match('#\.(sqlite|sqlite3|db|sql|sqlite-wal|sqlite-shm)$#i', $path) === 1;

if ($hidden) {
    http_response_code(404);
    header('Content-Type: text/plain; charset=utf-8');
    echo "Not found\n";
    return true;
}

// Единая точка входа AI/MCP/OpenAPI/Discovery для встроенного PHP-сервера.
if (preg_match('#^/\.well-known/ai-plugin\.json$#i', $path) === 1) {
    $_GET['action'] = 'discovery';
    $_SERVER['SCRIPT_NAME'] = '/api/ai.php';
    require __DIR__ . '/api/ai.php';
    return true;
}
if (preg_match('#^/openapi\.json$#i', $path) === 1) {
    $_GET['action'] = 'openapi';
    $_SERVER['SCRIPT_NAME'] = '/api/ai.php';
    require __DIR__ . '/api/ai.php';
    return true;
}
if (preg_match('#^/mcp/?$#i', $path) === 1) {
    $_SERVER['SCRIPT_NAME'] = '/api/mcp.php';
    require __DIR__ . '/api/mcp.php';
    return true;
}
if (preg_match('#^/ai/?$#i', $path) === 1) {
    $_SERVER['SCRIPT_NAME'] = '/ai/index.php';
    require __DIR__ . '/ai/index.php';
    return true;
}

// Всё остальное отдаёт встроенный сервер (PHP-файлы выполняются как обычно).
return false;
