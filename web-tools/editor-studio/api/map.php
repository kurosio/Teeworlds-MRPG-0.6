<?php
// Карты миров и их текстуры для окна выбора точки (editor-core/point-map.js).
//
// Endpoint: api/map.php
//   GET ?action=file&world=<ID>   — файл карты мира (tw_worlds.Path) для рендера
//   GET ?action=file&path=<путь>  — тот же файл напрямую (для моста map-api)
//   GET ?action=asset&name=<имя>  — внешняя текстура карты (data/mapres/<имя>.png)
//   GET ?action=status            — откуда берутся карты (для настроек)
//
// Откуда берутся файлы (по порядку):
//   1. путь из настроек редактора — «Путь к картам»/«Путь к текстурам»
//      (удобно, когда редактор и игровой сервер на одной машине);
//   2. мост map-api — адрес и токен из настроек: файлы забираются у сервиса
//      рядом с игровым сервером (см. map_api_request() в db-core.php);
//   3. каталоги рядом с редактором (maps/, data/mapres/) и переменные
//      окружения EDITOR_MAPS_DIR / EDITOR_MAPRES_DIR.
//
// Оба ответа — бинарные (не JSON), поэтому заголовки безопасности из
// bootstrap_editor_api_roles() переопределяются ниже.
//
// Доступ: администратор всегда; гость — если ему открыта хотя бы одна вкладка,
// где есть координаты. Запрос выполняется только на чтение: подставить свой
// путь или имя файла нельзя (см. map_resolve()).

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';

$ROLE = bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN, EDITOR_ROLE_GUEST]);

/** Вкладки, в которых редактор показывает координаты (значит, нужна карта). */
const MAP_TABS = ['quests', 'bots_npc', 'bots_mobs', 'warehouses', 'worlds', 'crafts', 'dungeons', 'aethers', 'nodes_collecting'];

if ($ROLE !== EDITOR_ROLE_ADMIN && !array_intersect(MAP_TABS, editor_guest_pages())) {
    respond(['ok' => false, 'error' => 'Доступ запрещён: карта не входит в ваши вкладки.', 'code' => 'guest_forbidden'], 403);
}

// Map and texture requests are read-only. Release PHP's per-session lock after
// authorization so slow downloads cannot block the editor or other textures.
if (session_status() === PHP_SESSION_ACTIVE) {
    session_write_close();
}

// ── Где искать файлы ─────────────────────────────────────────────────────────
//
// В Docker-образ попадает только каталог web-tools/editor-studio, поэтому
// maps/ и data/mapres/ обычно лежат рядом с ним (в корне репозитория) и
// подключаются томом. Пути можно задать переменными окружения
// EDITOR_MAPS_DIR и EDITOR_MAPRES_DIR — это же удобно на обычном хостинге.

function map_config(): array
{
    static $cfg = null;
    if ($cfg === null) {
        $cfg = load_cfg();
    }
    return $cfg;
}

/** Путь из настроек редактора (если задан). */
function map_setting(string $key): string
{
    return trim((string)(map_config()[$key] ?? ''));
}

/** Адрес моста; пусто — мост не используется. */
function map_api_url(): string
{
    return map_setting('map_api');
}

function map_api_token(): string
{
    return (string)(map_config()['map_api_token'] ?? '');
}

function map_candidates(string $env, array $relative, string $settingKey = ''): array
{
    $dirs = [];

    // 1. Путь из настроек: главный способ, когда каталог игрового сервера
    //    смонтирован рядом с редактором.
    $configured = $settingKey !== '' ? map_setting($settingKey) : '';
    if ($configured !== '') {
        $dirs[] = $configured;
    }

    $custom = getenv($env);
    if (is_string($custom) && $custom !== '') {
        $dirs[] = rtrim($custom, '/\\');
    }
    $base = dirname(__DIR__);        // web-tools/editor-studio
    $repo = dirname($base, 2);       // корень репозитория
    foreach ($relative as $path) {
        $dirs[] = $repo . '/' . $path;
        $dirs[] = $base . '/' . $path;
    }
    return $dirs;
}

function map_dir(string $env, array $relative, string $settingKey = ''): ?string
{
    static $found = [];
    $key = $env . '|' . implode(',', $relative) . '|' . $settingKey;
    if (array_key_exists($key, $found)) {
        return $found[$key];
    }
    $found[$key] = null;
    foreach (map_candidates($env, $relative, $settingKey) as $dir) {
        $real = realpath($dir);
        if ($real !== false && is_dir($real)) {
            $found[$key] = $real;
            break;
        }
    }
    return $found[$key];
}

function maps_dir(): ?string
{
    return map_dir('EDITOR_MAPS_DIR', ['maps', 'data/maps'], 'maps_dir');
}

function mapres_dir(): ?string
{
    return map_dir('EDITOR_MAPRES_DIR', ['data/mapres', 'mapres'], 'mapres_dir');
}

/**
 * Безопасный путь: только относительный, без «..», и итоговый файл обязан
 * остаться внутри каталога-источника (защита от символических ссылок).
 */
function map_resolve(string $dir, string $relative): ?string
{
    $relative = str_replace('\\', '/', $relative);
    if ($relative === '' || $relative[0] === '/' || str_contains($relative, '..')) {
        return null;
    }
    $base = realpath($dir);
    if ($base === false) {
        return null;
    }
    $real = realpath($base . '/' . $relative);
    if ($real === false || !is_file($real) || !is_readable($real)) {
        return null;
    }
    if ($real !== $base && !str_starts_with($real, $base . DIRECTORY_SEPARATOR)) {
        return null;
    }
    return $real;
}

/**
 * Ошибка для окна карты: код уходит и в JSON, и в заголовок X-Map-Error.
 * Рендер карты читает заголовок и показывает понятный текст вместо «404».
 */
function map_error(string $code, string $message, int $status = 404): never
{
    header('X-Map-Error: ' . $code);
    respond(['ok' => false, 'error' => $message, 'code' => $code], $status);
}

// ── Отдача файла ─────────────────────────────────────────────────────────────

function map_send_file(string $path, string $mime): never
{
    $size = filesize($path);
    $mtime = filemtime($path);
    if ($size === false || $mtime === false) {
        map_error('map_missing', 'Файл карты недоступен');
    }

    $etag = '"' . dechex((int)$size) . '-' . dechex((int)$mtime) . '"';
    $ifNoneMatch = (string)($_SERVER['HTTP_IF_NONE_MATCH'] ?? '');

    header_remove('Pragma');
    header('Content-Type: ' . $mime);
    header('Content-Length: ' . (string)$size);
    header('Cache-Control: private, max-age=600');
    header('ETag: ' . $etag);
    header('Last-Modified: ' . gmdate('D, d M Y H:i:s', (int)$mtime) . ' GMT');
    header('Content-Disposition: inline');

    if ($ifNoneMatch !== '' && str_contains($ifNoneMatch, $etag)) {
        header_remove('Content-Length');
        http_response_code(304);
        exit;
    }

    http_response_code(200);
    // Файл отдаётся как есть: это двоичный формат .map или PNG.
    readfile($path);
    exit;
}

/** Отдать ответ моста map-api как есть (с сохранением его ETag/кода ошибки). */
function map_send_remote(array $response, string $mime, int $maxBytes): never
{
    if (!$response['ok']) {
        $code = (int)($response['status'] ?? 0);
        $remoteCode = (string)($response['headers']['x-map-error'] ?? '');
        $errorCode = $remoteCode !== '' ? $remoteCode : ($code > 0 ? 'http_' . $code : 'map_api_unreachable');
        map_error($errorCode, 'Карта недоступна через map-api: ' . ($response['error'] ?: 'нет ответа'), 502);
    }

    $body = (string)$response['body'];
    if (strlen($body) > $maxBytes) {
        map_error('map_too_large', 'Файл карты слишком большой', 502);
    }

    $etag = (string)($response['headers']['etag'] ?? '');
    $ifNoneMatch = (string)($_SERVER['HTTP_IF_NONE_MATCH'] ?? '');

    header_remove('Pragma');
    header('Content-Type: ' . $mime);
    header('Content-Length: ' . (string)strlen($body));
    header('Cache-Control: private, max-age=600');
    if ($etag !== '') {
        header('ETag: ' . $etag);
    }

    if ($etag !== '' && $ifNoneMatch !== '' && str_contains($ifNoneMatch, $etag)) {
        header_remove('Content-Length');
        http_response_code(304);
        exit;
    }

    http_response_code(200);
    echo $body;
    exit;
}

/** URL моста с параметрами действия. */
function map_api_target(array $query): string
{
    $base = map_api_url();
    $query = array_filter($query, static fn($v) => $v !== '' && $v !== null);
    return $base . (str_contains($base, '?') ? '&' : '?') . http_build_query($query);
}

// ── Действия ────────────────────────────────────────────────────────────────

$action = (string)($_GET['action'] ?? '');

try {
    if ($action === 'status') {
        $remote = map_api_url();
        $payload = [
            'ok'     => true,
            'source' => $remote !== '' ? 'map-api' : 'local',
        ];
        if ($ROLE === EDITOR_ROLE_ADMIN) {
            // Пути — внутренняя информация: показываем только администратору.
            $payload['maps_dir'] = maps_dir() ?? '';
            $payload['mapres_dir'] = mapres_dir() ?? '';
            $payload['map_api'] = $remote;
        }
        respond($payload);
    }

    if ($action === 'asset') {
        // Имена текстур в картах DDNet — короткие латинские идентификаторы.
        // Рендер карты сам дописывает «.png», поэтому принимаем и с ним, и без.
        $name = preg_replace('/\.png$/i', '', (string)($_GET['name'] ?? ''));
        if (preg_match('/^[A-Za-z0-9_-]{1,64}$/', (string)$name) !== 1) {
            map_error('bad_name', 'Некорректное имя текстуры', 400);
        }

        $dir = mapres_dir();
        $file = $dir !== null ? map_resolve($dir, $name . '.png') : null;
        if ($file !== null) {
            map_send_file($file, 'image/png');
        }

        if (map_api_url() !== '') {
            // asset: 8 МБ более чем достаточно для текстуры 1024×1024.
            $response = map_api_request(map_api_target(['action' => 'asset', 'name' => $name]), map_api_token(), 8388608);
            map_send_remote($response, 'image/png', 8388608);
        }

        if ($dir === null) {
            map_error('mapres_dir_missing', 'На сервере нет каталога с текстурами карт');
        }
        map_error('asset_missing', 'Текстура не найдена');
    }

    if ($action === 'file') {
        $rawWorldId = $_GET['world'] ?? '';
        if (!is_scalar($rawWorldId)) {
            map_error('bad_world', 'Некорректный номер мира', 400);
        }
        $rawWorldId = trim((string)$rawWorldId);
        $hasWorldId = $rawWorldId !== '';
        $worldId = $hasWorldId ? filter_var($rawWorldId, FILTER_VALIDATE_INT, ['options' => ['min_range' => 0]]) : null;
        if ($hasWorldId && $worldId === false) {
            map_error('bad_world', 'Некорректный номер мира', 400);
        }
        $rawPath = trim((string)($_GET['path'] ?? ''));
        if (!$hasWorldId && $rawPath === '') {
            map_error('no_world', 'Мир не выбран', 400);
        }

        $path = '';
        if ($hasWorldId) {
            $mysqli = db_connect();
            $stmt = $mysqli->prepare('SELECT Path FROM tw_worlds WHERE ID = ? LIMIT 1');
            if ($stmt === false) {
                throw new RuntimeException('Не удалось подготовить запрос карты мира');
            }
            $stmt->bind_param('i', $worldId);
            $stmt->execute();
            $result = $stmt->get_result();
            $row = $result !== false ? $result->fetch_assoc() : null;
            $stmt->close();
            $mysqli->close();
            $path = is_array($row) ? (string)($row['Path'] ?? '') : '';
        }
        if ($rawPath !== '') {
            $path = $rawPath;
        }

        $dir = maps_dir();
        $file = ($path !== '' && $dir !== null) ? map_resolve($dir, $path) : null;
        // Фолбэк: на части установок карты лежат «плоско» (maps/main.map вместо
        // maps/worlds/main.map). Ищем по имени файла — путь всё равно проверяется.
        if ($file === null && $path !== '' && $dir !== null) {
            $file = map_resolve($dir, basename(str_replace('\\', '/', $path)));
        }
        if ($file !== null) {
            map_send_file($file, 'application/octet-stream');
        }

        if (map_api_url() !== '') {
            // Мост знает свой каталог карт: передаём путь к файлу, а если мира
            // нет в нашей БД — только его номер.
            $response = map_api_request(
                map_api_target(['action' => 'file', 'world' => $hasWorldId ? (string)$worldId : '', 'path' => $path]),
                map_api_token()
            );
            map_send_remote($response, 'application/octet-stream', 67108864);
        }

        if ($dir === null) {
            map_error('maps_dir_missing', 'На сервере нет каталога с картами мира');
        }
        // Текст ошибки намеренно общий: путь к файлам — внутренняя информация.
        map_error('map_missing', 'Карта мира не найдена на сервере');
    }

    map_error('bad_action', 'Неизвестное действие', 400);
} catch (EditorUserError $e) {
    respond(['ok' => false, 'error' => $e->getMessage()], 400);
} catch (Throwable $e) {
    editor_internal_error($e);
}
