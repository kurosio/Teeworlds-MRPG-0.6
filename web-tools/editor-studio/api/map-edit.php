<?php
// Administrator-only, atomic save endpoint for browser-edited DDNet v4 maps.
declare(strict_types=1);
require_once __DIR__ . '/db-core.php';
require_once __DIR__ . '/map-datafile.php';
bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN]);

// Keep under the project's default PHP post_max_size (50M in Docker).
const MAP_EDIT_MAX_BYTES = 50331648; // 48 MiB
// Потолок распаковки: карта main.map — 1.1 MiB на диске, но ~101 MiB в блоках,
// поэтому ограничение на распакованные данные должно быть заметно выше файла.
// Защита от «бомб» остаётся: длина каждого блока ограничена в gzuncompress.
const MAP_EDIT_MAX_UNCOMPRESSED = 512 * 1024 * 1024; // 512 MiB

function map_edit_fail(string $code, string $message, int $status): never
{
    respond(['ok' => false, 'code' => $code, 'error' => $message], $status);
}

function map_edit_guard(): void
{
    if (strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? '')) !== 'POST') {
        header('Allow: POST');
        map_edit_fail('method_not_allowed', 'Для сохранения карты требуется POST.', 405);
    }
    $contentType = strtolower(trim(explode(';', (string)($_SERVER['CONTENT_TYPE'] ?? ''))[0]));
    if ($contentType !== 'application/octet-stream') {
        map_edit_fail('content_type', 'Ожидается двоичный файл карты.', 415);
    }
    $origin = trim((string)($_SERVER['HTTP_ORIGIN'] ?? ''));
    if ($origin === '' || strtolower($origin) === 'null') {
        map_edit_fail('origin', 'Заголовок Origin обязателен.', 403);
    }
    $parts = parse_url($origin);
    $scheme = strtolower((string)($parts['scheme'] ?? ''));
    $originHost = strtolower((string)($parts['host'] ?? ''));
    $originPort = isset($parts['port']) ? (int)$parts['port'] : ($scheme === 'https' ? 443 : 80);
    $requestHost = strtolower(trim((string)($_SERVER['HTTP_HOST'] ?? '')));
    $requestParts = parse_url('//' . $requestHost);
    $requestName = strtolower((string)($requestParts['host'] ?? ''));
    $requestPort = isset($requestParts['port']) ? (int)$requestParts['port'] : $originPort;
    if (!in_array($scheme, ['http', 'https'], true) || $originHost === ''
        || $originHost !== $requestName || $originPort !== $requestPort) {
        map_edit_fail('origin', 'Запрос с постороннего сайта отклонён.', 403);
    }
    editor_csrf_verify();
}

function map_edit_maps_dir(): ?string
{
    return ddnet_map_first_dir(load_cfg(), 'EDITOR_MAPS_DIR', ['maps', 'data/maps'], 'maps_dir');
}

function map_edit_resolve(string $root, string $relative): ?string
{
    return ddnet_map_resolve($root, $relative);
}

function map_edit_u32(string $bytes, int $offset): int
{
    return ddnet_u32($bytes, $offset);
}

function map_edit_i32(string $bytes, int $offset): int
{
    return ddnet_i32($bytes, $offset);
}

/** Expand CTile m_Skip data into semantic [index, flags, reserved] cells. */
function map_edit_expand_tiles(string $plain, int $count, int $version): string
{
    return ddnet_expand_tiles($plain, $count, $version);
}

/** Switch layer is CSwitchTileExtra per cell: number, type, flags, delay. */
function map_edit_expand_switch(string $plain, int $count): string
{
    return ddnet_expand_switch($plain, $count);
}

/** Validate a complete datafile and return only its compact structure summary. */
function map_edit_validate_datafile(string $bytes): array
{
    return ddnet_parse_datafile($bytes, MAP_EDIT_MAX_BYTES, MAP_EDIT_MAX_UNCOMPRESSED, true);
}

/**
 * Game-layer cells may be added, moved and deleted; switch-layer cells may be
 * added, moved, edited and deleted. Everything else about the map must stay
 * byte-identical, and every entity/switch type has to come from the allowlist
 * supplied by the editor (X-Map-Allowed-Tiles).
 *
 * Returns `['game' => n, 'switch' => m]` counts for the audit log.
 */
function map_edit_assert_tile_only_change(array $before, array $after, array $allowedTileIds): array
{
    foreach (['counts', 'types_hash', 'item_offsets_hash', 'items_hash', 'game', 'switch'] as $key) {
        if ($before[$key] !== $after[$key]) throw new EditorUserError('Можно менять только тайлы игрового и switch-слоя; структура карты не должна изменяться.');
    }
    $editable = [$before['game']['data']];
    if (is_array($before['switch'])) $editable[] = $before['switch']['data'];
    $changedRaw = [];
    for ($i = 0; $i < count($before['raw_hashes']); $i++) {
        if (in_array($i, $editable, true)) continue;
        if ($before['raw_hashes'][$i] !== $after['raw_hashes'][$i] || $before['raw_sizes'][$i] !== $after['raw_sizes'][$i]) {
            throw new EditorUserError('Изменён посторонний ресурс карты; сохранение отклонено.');
        }
    }
    foreach ($editable as $rawIndex) {
        if ($before['raw_hashes'][$rawIndex] !== $after['raw_hashes'][$rawIndex]) $changedRaw[] = $rawIndex;
    }
    if (!$changedRaw) throw new EditorUserError('В карте не обнаружены изменённые тайлы.');
    $allowed = array_fill_keys($allowedTileIds, true);

    // Game layer: 3 bytes per cell (index, flags, reserved). Flags and the
    // reserved byte must stay untouched; only the entity index changes.
    $old = $before['game_tiles'];
    $new = $after['game_tiles'];
    if (strlen($old) !== strlen($new)) throw new EditorUserError('Размер игрового слоя изменился.');
    $gameChanges = 0;
    for ($offset = 0, $length = strlen($old); $offset < $length; $offset += 3) {
        if ($old[$offset] === $new[$offset] && $old[$offset + 1] === $new[$offset + 1] && $old[$offset + 2] === $new[$offset + 2]) continue;
        $oldIndex = ord($old[$offset]);
        $newIndex = ord($new[$offset]);
        if ($old[$offset + 1] !== $new[$offset + 1] || $old[$offset + 2] !== $new[$offset + 2]) {
            throw new EditorUserError('У тайла игрового слоя нельзя менять флаги.');
        }
        if ($newIndex !== 0 && !isset($allowed[$newIndex])) {
            throw new EditorUserError('Тайл не входит в список объявленных в highlightTiles сущностей; сохранение отклонено.');
        }
        if ($oldIndex !== 0 && $oldIndex <= 3) {
            throw new EditorUserError('Тайлы столкновений (1–3) нельзя удалять или перемещать.');
        }
        if ($oldIndex !== 0 && !isset($allowed[$oldIndex])) {
            throw new EditorUserError('На карте нашлись тайлы вне списка объявленных в highlightTiles сущностей; сохранение отклонено.');
        }
        $gameChanges++;
    }
    if ($gameChanges > 1000) throw new EditorUserError('За одно применение можно изменить не более 1000 тайлов игрового слоя.');

    // Switch layer: 4 bytes per cell (number, type, flags, delay). The switch
    // type is the entity: type 0 clears the cell, otherwise the type has to be
    // in the allowlist and the switch number must be a real 1–255 value.
    $switchChanges = 0;
    if (is_array($before['switch'])) {
        $oldSwitch = (string)$before['switch_tiles'];
        $newSwitch = (string)$after['switch_tiles'];
        if ($oldSwitch === '' || strlen($oldSwitch) !== strlen($newSwitch)) throw new EditorUserError('Размер switch-слоя изменился.');
        for ($offset = 0, $length = strlen($oldSwitch); $offset < $length; $offset += 4) {
            if (substr($oldSwitch, $offset, 4) === substr($newSwitch, $offset, 4)) continue;
            $oldNumber = ord($oldSwitch[$offset]);
            $oldType = ord($oldSwitch[$offset + 1]);
            $oldFlags = $oldSwitch[$offset + 2];
            $oldDelay = $oldSwitch[$offset + 3];
            $newNumber = ord($newSwitch[$offset]);
            $newType = ord($newSwitch[$offset + 1]);
            if ($newSwitch[$offset + 2] !== $oldFlags || $newSwitch[$offset + 3] !== $oldDelay) {
                throw new EditorUserError('У switch-тайла нельзя менять флаги и задержку.');
            }
            if ($newType !== 0 && !isset($allowed[$newType])) {
                throw new EditorUserError('Тип switch-тайла не объявлен в highlightTiles; сохранение отклонено.');
            }
            if ($oldType !== 0 && !isset($allowed[$oldType])) {
                throw new EditorUserError('На карте нашлись switch-тайлы вне списка объявленных в highlightTiles типов; сохранение отклонено.');
            }
            // Номер switch меняется свободно (1–255): он связывает тайл с
            // узлом сбора в базе. Ограничение — только на объявленные типы.
            if ($newType !== 0 && ($newNumber < 1)) {
                throw new EditorUserError('Номер switch должен быть от 1 до 255.');
            }
            $switchChanges++;
        }
        if ($switchChanges > 1000) throw new EditorUserError('За одно применение можно изменить не более 1000 switch-тайлов.');
    } elseif (is_array($after['switch'])) {
        throw new EditorUserError('Карта потеряла switch-слой; сохранение отклонено.');
    }

    if ($gameChanges + $switchChanges < 1) throw new EditorUserError('В карте не обнаружены изменённые тайлы.');
    return ['game' => $gameChanges, 'switch' => $switchChanges];
}

map_edit_guard();
$worldRaw = $_GET['world'] ?? '';
$worldId = is_scalar($worldRaw) ? filter_var((string)$worldRaw, FILTER_VALIDATE_INT, ['options' => ['min_range' => 0]]) : false;
if ($worldId === false) map_edit_fail('bad_world', 'Некорректный номер мира.', 400);
$expectedHash = strtolower(trim((string)($_SERVER['HTTP_X_MAP_ORIGINAL_SHA256'] ?? '')));
if (preg_match('/^[a-f0-9]{64}$/', $expectedHash) !== 1) map_edit_fail('map_hash', 'Не передана контрольная сумма исходной карты.', 400);
$allowedHeader = trim((string)($_SERVER['HTTP_X_MAP_ALLOWED_TILES'] ?? ''));
$allowedTileIds = [];
if ($allowedHeader === '' || strlen($allowedHeader) > 1024) map_edit_fail('tile_allowlist', 'Не передан список разрешённых ID тайлов.', 400);
foreach (explode(',', $allowedHeader) as $rawTileId) {
    if (preg_match('/^(?:[4-9]|[1-9][0-9]{1,2})$/', $rawTileId) !== 1) map_edit_fail('tile_allowlist', 'Список разрешённых ID тайлов повреждён.', 400);
    $tileId = (int)$rawTileId;
    if ($tileId > 255) map_edit_fail('tile_allowlist', 'ID тайла вне допустимого диапазона.', 400);
    $allowedTileIds[$tileId] = $tileId;
}
$allowedTileIds = array_values($allowedTileIds);
if (!$allowedTileIds || count($allowedTileIds) > 256) map_edit_fail('tile_allowlist', 'Список разрешённых ID тайлов пуст или слишком велик.', 400);
$contentLength = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
if ($contentLength > MAP_EDIT_MAX_BYTES) map_edit_fail('map_too_large', 'Файл карты превышает лимит записи 48 МиБ.', 413);
$newBytes = file_get_contents('php://input', false, null, 0, MAP_EDIT_MAX_BYTES + 1);
if (!is_string($newBytes) || $newBytes === '' || strlen($newBytes) > MAP_EDIT_MAX_BYTES) {
    map_edit_fail('map_empty', 'Файл карты пуст или превышает лимит записи 48 МиБ.', 413);
}
try {
    $newMapInfo = map_edit_validate_datafile($newBytes);
} catch (Throwable $e) {
    map_edit_fail('map_invalid', $e->getMessage(), 400);
}

try {
    $mysqli = db_connect();
    $stmt = $mysqli->prepare('SELECT Path FROM tw_worlds WHERE ID = ? LIMIT 1');
    if ($stmt === false) throw new EditorUserError('Не удалось найти путь карты мира.');
    $stmt->bind_param('i', $worldId);
    $stmt->execute();
    $result = $stmt->get_result();
    $row = $result !== false ? $result->fetch_assoc() : null;
    $stmt->close();
    $mysqli->close();
    $relative = is_array($row) ? trim((string)($row['Path'] ?? '')) : '';
    if ($relative === '') map_edit_fail('map_path_missing', 'У мира не задан файл карты.', 404);
    $root = map_edit_maps_dir();
    if ($root === null) map_edit_fail('map_remote_readonly', 'Сохранение доступно только для локальной папки карт с правом записи; удалённый map-api пока доступен только для чтения.', 409);
    $file = map_edit_resolve($root, $relative);
    if ($file === null) $file = map_edit_resolve($root, basename(str_replace('\\', '/', $relative)));
    if ($file === null) map_edit_fail('map_missing', 'Файл карты мира не найден в локальной папке карт.', 404);
    if (!is_writable($file) || !is_writable(dirname($file))) map_edit_fail('map_readonly', 'У веб-сервера нет права записи в папку карты.', 403);

    $dataDir = __DIR__ . '/../data';
    $lockDir = $dataDir . '/map-locks';
    $backupDir = $dataDir . '/map-backups';
    foreach ([$lockDir, $backupDir] as $dir) {
        if (!is_dir($dir) && !@mkdir($dir, 0700, true) && !is_dir($dir)) throw new EditorUserError('Не удалось создать служебную папку сохранения карты.');
    }
    if (!is_file($backupDir . '/.htaccess')) @file_put_contents($backupDir . '/.htaccess', "Require all denied\n");
    $lockPath = $lockDir . '/' . hash('sha256', $file) . '.lock';
    $lock = @fopen($lockPath, 'c');
    if (!is_resource($lock) || !flock($lock, LOCK_EX)) throw new EditorUserError('Не удалось заблокировать карту для сохранения.');
    clearstatcache(true, $file);
    $oldBytes = @file_get_contents($file);
    if (!is_string($oldBytes)) throw new EditorUserError('Не удалось прочитать исходный файл карты.');
    if (strlen($oldBytes) > MAP_EDIT_MAX_BYTES) map_edit_fail('map_too_large', 'Исходная карта превышает лимит записи 48 МиБ.', 413);
    if (!hash_equals($expectedHash, hash('sha256', $oldBytes))) {
        flock($lock, LOCK_UN); fclose($lock);
        map_edit_fail('map_conflict', 'Карта изменилась после открытия. Обновите окно карты и повторите добавление тайла.', 409);
    }
    if (hash_equals(hash('sha256', $oldBytes), hash('sha256', $newBytes))) {
        flock($lock, LOCK_UN); fclose($lock);
        respond(['ok' => true, 'changed' => false, 'sha256' => $expectedHash, 'tiles' => 0, 'switch_tiles' => 0]);
    }
    $oldMapInfo = map_edit_validate_datafile($oldBytes);
    $tileChanges = map_edit_assert_tile_only_change($oldMapInfo, $newMapInfo, $allowedTileIds);
    $safeName = preg_replace('/[^A-Za-z0-9_.-]+/', '_', basename($relative)) ?: 'world.map';
    $backup = $backupDir . '/' . $worldId . '-' . gmdate('Ymd-His') . '-' . bin2hex(random_bytes(4)) . '-' . $safeName . '.bak';
    if (!@file_put_contents($backup, $oldBytes, LOCK_EX)) throw new EditorUserError('Не удалось создать резервную копию — карта не сохранена.');
    @chmod($backup, 0600);
    $temp = @tempnam(dirname($file), '.map-picker-');
    if (!is_string($temp)) throw new EditorUserError('Не удалось подготовить временный файл карты.');
    $mode = fileperms($file);
    $group = filegroup($file);
    if (!@file_put_contents($temp, $newBytes, LOCK_EX) || filesize($temp) !== strlen($newBytes)) {
        @unlink($temp);
        throw new EditorUserError('Не удалось записать временный файл карты.');
    }
    if ($group !== false) @chgrp($temp, $group);
    if ($mode !== false) @chmod($temp, $mode & 0777);
    // Recheck after validation/temp write: another process may have edited the
    // map without honoring our advisory lock while this request was working.
    clearstatcache(true, $file);
    $latestHash = @hash_file('sha256', $file);
    if (!is_string($latestHash) || !hash_equals($expectedHash, $latestHash)) {
        @unlink($temp);
        flock($lock, LOCK_UN); fclose($lock);
        map_edit_fail('map_conflict', 'Карта изменилась во время сохранения. Обновите окно карты и повторите попытку.', 409);
    }
    if (!@rename($temp, $file)) {
        @unlink($temp);
        throw new EditorUserError('Не удалось атомарно заменить файл карты.');
    }
    clearstatcache(true, $file);
    $newHash = hash('sha256', $newBytes);
    $backups = glob($backupDir . '/' . $worldId . '-*.bak') ?: [];
    usort($backups, static fn($a, $b) => (filemtime($b) ?: 0) <=> (filemtime($a) ?: 0));
    foreach (array_slice($backups, 10) as $staleBackup) @unlink($staleBackup);
    flock($lock, LOCK_UN); fclose($lock);
    if (function_exists('editor_audit')) editor_audit('map.tiles.save', '', ['world_id' => (int)$worldId, 'tiles' => $tileChanges, 'bytes' => strlen($newBytes)]);
    respond(['ok' => true, 'changed' => true, 'sha256' => $newHash,
        'tiles' => $tileChanges['game'], 'switch_tiles' => $tileChanges['switch']]);
} catch (Throwable $e) {
    if (isset($lock) && is_resource($lock)) { @flock($lock, LOCK_UN); @fclose($lock); }
    // Ошибки правки карты написаны для пользователя (какой тайл, какая клетка,
    // какое правило нарушено) и не должны подменяться общим текстом про БД.
    // Всё остальное (файловая система, БД) уходит в журнал как раньше.
    editor_internal_error($e, $e instanceof EditorUserError ? $e->getMessage()
        : 'Не удалось сохранить карту. Попробуйте ещё раз; если ошибка повторится — сообщите администратору.', 400);
}
