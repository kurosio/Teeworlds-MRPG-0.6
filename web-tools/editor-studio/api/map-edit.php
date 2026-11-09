<?php
// Administrator-only, atomic save endpoint for browser-edited DDNet v4 maps.
declare(strict_types=1);
require_once __DIR__ . '/db-core.php';
bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN]);

// Keep under the project's default PHP post_max_size (50M in Docker).
const MAP_EDIT_MAX_BYTES = 50331648; // 48 MiB

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
    $cfg = load_cfg();
    $candidates = [];
    $setting = trim((string)($cfg['maps_dir'] ?? ''));
    if ($setting !== '') $candidates[] = $setting;
    $env = getenv('EDITOR_MAPS_DIR');
    if (is_string($env) && $env !== '') $candidates[] = rtrim($env, '/\\');
    $repo = dirname(__DIR__, 2);
    $studio = dirname(__DIR__);
    foreach ([$repo . '/maps', $repo . '/data/maps', $studio . '/maps'] as $candidate) $candidates[] = $candidate;
    foreach (array_unique($candidates) as $candidate) {
        $real = realpath($candidate);
        if ($real !== false && is_dir($real)) return $real;
    }
    return null;
}

function map_edit_resolve(string $root, string $relative): ?string
{
    $relative = str_replace('\\', '/', trim($relative));
    if ($relative === '' || str_starts_with($relative, '/') || str_contains($relative, '..')
        || preg_match('/^[A-Za-z]:/', $relative)) return null;
    $base = realpath($root);
    $real = $base === false ? false : realpath($base . '/' . $relative);
    if ($real === false || !is_file($real) || !is_readable($real)) return null;
    return $real === $base || str_starts_with($real, $base . DIRECTORY_SEPARATOR) ? $real : null;
}

function map_edit_u32(string $bytes, int $offset): int
{
    if ($offset < 0 || $offset + 4 > strlen($bytes)) throw new EditorUserError('Повреждён заголовок карты.');
    return (int)unpack('Vvalue', substr($bytes, $offset, 4))['value'];
}

function map_edit_i32(string $bytes, int $offset): int
{
    $value = map_edit_u32($bytes, $offset);
    return $value >= 0x80000000 ? $value - 0x100000000 : $value;
}

/** Expand CTile m_Skip data into semantic [index, flags, reserved] cells. */
function map_edit_expand_tiles(string $plain, int $count, int $version): string
{
    if ($count < 1 || $count > 4000000 || strlen($plain) % 4 !== 0) throw new EditorUserError('Данные игрового слоя повреждены.');
    $tiles = '';
    if ($version >= 4) {
        $written = 0;
        for ($offset = 0, $length = strlen($plain); $offset < $length && $written < $count; $offset += 4) {
            $tile = substr($plain, $offset, 2) . $plain[$offset + 3];
            $repeat = min(ord($plain[$offset + 2]) + 1, $count - $written);
            $tiles .= str_repeat($tile, $repeat);
            $written += $repeat;
        }
        if ($written !== $count) throw new EditorUserError('Игровой слой обрезан.');
    } else {
        if (strlen($plain) < $count * 4) throw new EditorUserError('Игровой слой обрезан.');
        for ($i = 0; $i < $count; $i++) {
            $offset = $i * 4;
            $tiles .= substr($plain, $offset, 2) . $plain[$offset + 3];
        }
    }
    return $tiles;
}

/** Switch layer is CSwitchTileExtra per cell: number, type, flags, delay. */
function map_edit_expand_switch(string $plain, int $count): string
{
    if ($count < 1 || $count > 4000000) throw new EditorUserError('Данные switch-слоя повреждены.');
    if (strlen($plain) < $count * 4) throw new EditorUserError('Switch-слой обрезан.');
    return substr($plain, 0, $count * 4);
}

/** Validate a complete datafile and return only its compact structure summary. */
function map_edit_validate_datafile(string $bytes): array
{
    $length = strlen($bytes);
    if ($length < 36 || $length > MAP_EDIT_MAX_BYTES || substr($bytes, 0, 4) !== 'DATA') {
        throw new EditorUserError('Файл не является поддерживаемой картой DDNet.');
    }
    $version = map_edit_u32($bytes, 4);
    $size = map_edit_u32($bytes, 8);
    $swapLen = map_edit_u32($bytes, 12);
    $numTypes = map_edit_u32($bytes, 16);
    $numItems = map_edit_u32($bytes, 20);
    $numData = map_edit_u32($bytes, 24);
    $itemSize = map_edit_u32($bytes, 28);
    $dataSize = map_edit_u32($bytes, 32);
    if ($version !== 4 || $size !== $length - 16 || $numTypes > 65536 || $numItems > 2000000
        || $numData > 100000 || $itemSize > $length || $dataSize > $length) {
        throw new EditorUserError('Структура карты повреждена или не поддерживается.');
    }
    $typesStart = 36;
    $typesEnd = $typesStart + $numTypes * 12;
    $itemOffsetsEnd = $typesEnd + $numItems * 4;
    $dataOffsetsEnd = $itemOffsetsEnd + $numData * 4;
    $dataSizesEnd = $dataOffsetsEnd + $numData * 4;
    $itemStart = $dataSizesEnd;
    $dataStart = $itemStart + $itemSize;
    if ($dataStart > $length || $swapLen !== $dataStart - 16 || $dataStart + $dataSize !== $length) {
        throw new EditorUserError('Таблицы или размеры карты не согласованы.');
    }

    // Locate the actual game-layer and switch-layer raw-data indexes from the
    // unmodified item table. Both tables are read from the same $flags field:
    // TILESLAYERFLAG_GAME (1) marks the gameplay layer, TILESLAYERFLAG_SWITCH
    // (16) marks the switch layer, whose extra data lives in m_Switch (offset
    // 84 for current maps, 72 for legacy ones).
    $game = null;
    $switch = null;
    for ($i = 0; $i < $numTypes; $i++) {
        $type = map_edit_u32($bytes, $typesStart + $i * 12);
        if ($type !== 5) continue; // MAPITEMTYPE_LAYER
        $first = map_edit_u32($bytes, $typesStart + $i * 12 + 4);
        $count = map_edit_u32($bytes, $typesStart + $i * 12 + 8);
        if ($first + $count > $numItems) throw new EditorUserError('Таблица слоёв карты повреждена.');
        for ($itemIndex = $first; $itemIndex < $first + $count; $itemIndex++) {
            $itemOffset = map_edit_u32($bytes, $typesEnd + $itemIndex * 4);
            if ($itemOffset + 8 > $itemSize) throw new EditorUserError('Слой карты выходит за границы данных.');
            $typeAndId = map_edit_u32($bytes, $itemStart + $itemOffset);
            if ((($typeAndId >> 16) & 0xffff) !== 5) continue;
            $payloadSize = map_edit_u32($bytes, $itemStart + $itemOffset + 4);
            $payload = $itemStart + $itemOffset + 8;
            if ($payloadSize < 60 || $payload + $payloadSize > $itemStart + $itemSize) continue;
            if (map_edit_i32($bytes, $payload + 4) !== 2) continue; // LAYERTYPE_TILES
            $tileVersion = map_edit_i32($bytes, $payload + 12);
            $width = map_edit_i32($bytes, $payload + 16);
            $height = map_edit_i32($bytes, $payload + 20);
            $flags = map_edit_u32($bytes, $payload + 24);
            $image = map_edit_i32($bytes, $payload + 52);
            $rawIndex = map_edit_i32($bytes, $payload + 56);
            if ($width < 1 || $height < 1 || $width * $height > 4000000) {
                throw new EditorUserError('Слой карты имеет неверные размеры.');
            }
            $switchAt = $tileVersion <= 2 ? 72 : 84;
            if ($switch === null && ($flags & 16) !== 0 && $payload + $switchAt + 4 <= $payload + $payloadSize) {
                $switchIndex = map_edit_i32($bytes, $payload + $switchAt);
                if ($switchIndex >= 0 && $switchIndex < $numData) {
                    $switch = ['data' => $switchIndex, 'width' => $width, 'height' => $height, 'rows_bytes' => 4];
                }
            }
            if ($game === null && ($flags & 1) !== 0 && $image === -1) {
                if ($rawIndex < 0 || $rawIndex >= $numData) {
                    throw new EditorUserError('Игровой слой карты имеет неверные размеры.');
                }
                $game = ['data' => $rawIndex, 'width' => $width, 'height' => $height, 'version' => $tileVersion];
            }
            if ($game !== null && $switch !== null) break 2;
        }
    }
    if ($game === null) throw new EditorUserError('В карте не найден поддерживаемый игровой слой.');

    $itemOffsetsHash = hash('sha256', substr($bytes, $typesEnd, $numItems * 4));
    $rawHashes = [];
    $rawSizes = [];
    $gamePlain = null;
    $switchPlain = null;
    $last = -1;
    $uncompressedTotal = 0;
    for ($i = 0; $i < $numData; $i++) {
        $off = map_edit_u32($bytes, $itemOffsetsEnd + $i * 4);
        $plainSize = map_edit_u32($bytes, $dataOffsetsEnd + $i * 4);
        $next = $i + 1 < $numData ? map_edit_u32($bytes, $itemOffsetsEnd + ($i + 1) * 4) : $dataSize;
        if ($off < 0 || $off < $last || $next < $off || $next > $dataSize || $plainSize > MAP_EDIT_MAX_BYTES) {
            throw new EditorUserError('Таблица ресурсов карты повреждена.');
        }
        $compressed = substr($bytes, $dataStart + $off, $next - $off);
        $plain = @gzuncompress($compressed, min($plainSize + 1, MAP_EDIT_MAX_BYTES));
        if (!is_string($plain) || strlen($plain) !== $plainSize) throw new EditorUserError('Сжатый блок карты повреждён.');
        $rawHashes[] = hash('sha256', $compressed);
        $rawSizes[] = $plainSize;
        if ($i === $game['data']) $gamePlain = $plain;
        if ($switch !== null && $i === $switch['data']) {
            // A switch layer shorter than width*height*4 is ignored by the game
            // engine itself, so it is not editable either.
            if (strlen($plain) >= $switch['width'] * $switch['height'] * $switch['rows_bytes']) {
                $switchPlain = $plain;
            } else {
                $switch = null;
            }
        }
        $uncompressedTotal += $plainSize;
        if ($uncompressedTotal > MAP_EDIT_MAX_BYTES * 2) throw new EditorUserError('Распакованные данные карты слишком велики.');
        $last = $off;
    }
    if (!is_string($gamePlain)) throw new EditorUserError('Данные игрового слоя не найдены.');
    $gameTiles = map_edit_expand_tiles($gamePlain, $game['width'] * $game['height'], $game['version']);
    $switchTiles = null;
    if ($switch !== null) {
        if (!is_string($switchPlain)) throw new EditorUserError('Данные switch-слоя не найдены.');
        $switchTiles = map_edit_expand_switch($switchPlain, $switch['width'] * $switch['height']);
    }
    return [
        'counts' => [$numTypes, $numItems, $numData, $itemSize],
        'types_hash' => hash('sha256', substr($bytes, $typesStart, $numTypes * 12)),
        'item_offsets_hash' => $itemOffsetsHash,
        'items_hash' => hash('sha256', substr($bytes, $itemStart, $itemSize)),
        'raw_hashes' => $rawHashes,
        'raw_sizes' => $rawSizes,
        'game' => $game,
        'game_tiles' => $gameTiles,
        'switch' => $switch,
        'switch_tiles' => $switchTiles,
    ];
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
            throw new EditorUserError('Тайл не входит в список разрешённых сущностей.');
        }
        if ($oldIndex !== 0 && $oldIndex <= 3) {
            throw new EditorUserError('Тайлы столкновений (1–3) нельзя удалять или перемещать.');
        }
        if ($oldIndex !== 0 && !isset($allowed[$oldIndex])) {
            throw new EditorUserError('На карте нашлись тайлы вне списка разрешённых сущностей; сохранение отклонено.');
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
                throw new EditorUserError('Switch-тайл не входит в список разрешённых сущностей.');
            }
            if ($oldType !== 0 && !isset($allowed[$oldType])) {
                throw new EditorUserError('На карте нашлись switch-тайлы вне списка разрешённых сущностей; сохранение отклонено.');
            }
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
