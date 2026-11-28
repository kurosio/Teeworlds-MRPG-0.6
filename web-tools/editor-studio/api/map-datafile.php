<?php
// Shared DDNet v4 binary datafile parser and safe map-directory helpers.
// Used by api/map.php, api/map-edit.php, and api/ai-lib.php.
declare(strict_types=1);

/**
 * Candidate directories for map/mapres lookup in priority order:
 * explicit editor setting -> environment variable -> repo/studio directories.
 *
 * @param array<string, mixed> $cfg
 * @param array<int, string>   $relative
 * @return array<int, string>
 */
function ddnet_map_candidates(array $cfg, string $env, array $relative, string $settingKey = ''): array
{
    $dirs = [];
    if ($settingKey !== '') {
        $configured = trim((string)($cfg[$settingKey] ?? ''));
        if ($configured !== '') {
            $dirs[] = $configured;
        }
    }
    $custom = getenv($env);
    if (is_string($custom) && $custom !== '') {
        $dirs[] = rtrim($custom, '/\\');
    }
    $studio = dirname(__DIR__);
    $repo = dirname($studio, 2);
    foreach ($relative as $rel) {
        $dirs[] = $repo . '/' . $rel;
        $dirs[] = $studio . '/' . $rel;
    }
    return array_values(array_unique($dirs));
}

/**
 * All existing readable directories matching the candidate list.
 *
 * @param array<string, mixed> $cfg
 * @param array<int, string>   $relative
 * @return array<int, string>
 */
function ddnet_map_existing_dirs(array $cfg, string $env, array $relative, string $settingKey = ''): array
{
    $resolved = [];
    foreach (ddnet_map_candidates($cfg, $env, $relative, $settingKey) as $dir) {
        $real = realpath($dir);
        if ($real !== false && is_dir($real)) {
            $resolved[] = $real;
        }
    }
    return array_values(array_unique($resolved));
}

/**
 * First existing readable directory matching the candidate list.
 *
 * @param array<string, mixed> $cfg
 * @param array<int, string>   $relative
 */
function ddnet_map_first_dir(array $cfg, string $env, array $relative, string $settingKey = ''): ?string
{
    $dirs = ddnet_map_existing_dirs($cfg, $env, $relative, $settingKey);
    return $dirs[0] ?? null;
}

/**
 * Resolve a relative asset/map path strictly inside $dir (protects against
 * directory traversal and symlinks escaping the base directory).
 */
function ddnet_map_resolve(string $dir, string $relative): ?string
{
    $relative = str_replace('\\', '/', trim($relative));
    if (
        $relative === ''
        || str_starts_with($relative, '/')
        || str_contains($relative, '..')
        || preg_match('/^[A-Za-z]:/', $relative)
    ) {
        return null;
    }
    $base = realpath($dir);
    $real = $base === false ? false : realpath($base . '/' . $relative);
    if ($real === false || !is_file($real) || !is_readable($real)) {
        return null;
    }
    return ($real === $base || str_starts_with($real, $base . DIRECTORY_SEPARATOR)) ? $real : null;
}

function ddnet_u32(string $bytes, int $offset): int
{
    if ($offset < 0 || $offset + 4 > strlen($bytes)) {
        throw new EditorUserError('Повреждён заголовок карты DDNet.');
    }
    return (int)unpack('Vvalue', substr($bytes, $offset, 4))['value'];
}

function ddnet_i32(string $bytes, int $offset): int
{
    $value = ddnet_u32($bytes, $offset);
    return $value >= 0x80000000 ? $value - 0x100000000 : $value;
}

/** Expand CTile (skip-compressed in v4+) into [index, flags, reserved] (3 bytes/cell). */
function ddnet_expand_tiles(string $plain, int $count, int $version): string
{
    if ($count < 1 || $count > 4000000 || strlen($plain) % 4 !== 0) {
        throw new EditorUserError('Данные игрового слоя карты повреждены.');
    }
    $tiles = '';
    if ($version >= 4) {
        $written = 0;
        for ($offset = 0, $length = strlen($plain); $offset < $length && $written < $count; $offset += 4) {
            $tile = substr($plain, $offset, 2) . $plain[$offset + 3];
            $repeat = min(ord($plain[$offset + 2]) + 1, $count - $written);
            $tiles .= str_repeat($tile, $repeat);
            $written += $repeat;
        }
        if ($written !== $count) {
            throw new EditorUserError('Игровой слой карты обрезан.');
        }
    } else {
        if (strlen($plain) < $count * 4) {
            throw new EditorUserError('Игровой слой карты обрезан.');
        }
        for ($i = 0; $i < $count; $i++) {
            $offset = $i * 4;
            $tiles .= substr($plain, $offset, 2) . $plain[$offset + 3];
        }
    }
    return $tiles;
}

/** Expand switch layer cells: CSwitchTileExtra [number, type, flags, delay] (4 bytes/cell). */
function ddnet_expand_switch(string $plain, int $count): string
{
    if ($count < 1 || $count > 4000000) {
        throw new EditorUserError('Данные switch-слоя повреждены.');
    }
    if (strlen($plain) < $count * 4) {
        throw new EditorUserError('Switch-слой карты обрезан.');
    }
    return substr($plain, 0, $count * 4);
}

/**
 * Validate and extract game/switch layers from a DDNet v4 binary datafile.
 *
 * When $verifyAllBlocks is true (used when saving edited maps), every raw data
 * block is decompressed and hashed to detect unauthorized or corrupt mutations.
 *
 * @return array<string, mixed>
 */
function ddnet_parse_datafile(
    string $bytes,
    int $maxBytes,
    int $maxUncompressed,
    bool $verifyAllBlocks = false
): array {
    $length = strlen($bytes);
    if ($length < 36 || $length > $maxBytes || substr($bytes, 0, 4) !== 'DATA') {
        throw new EditorUserError('Файл не является поддерживаемой картой DDNet.');
    }
    $version = ddnet_u32($bytes, 4);
    $size = ddnet_u32($bytes, 8);
    $swapLen = ddnet_u32($bytes, 12);
    $numTypes = ddnet_u32($bytes, 16);
    $numItems = ddnet_u32($bytes, 20);
    $numData = ddnet_u32($bytes, 24);
    $itemSize = ddnet_u32($bytes, 28);
    $dataSize = ddnet_u32($bytes, 32);
    if (
        $version !== 4
        || $size !== $length - 16
        || $numTypes > 65536
        || $numItems > 2000000
        || $numData > 100000
        || $itemSize > $length
        || $dataSize > $length
    ) {
        throw new EditorUserError('Структура карты DDNet повреждена или не поддерживается.');
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

    $game = null;
    $switch = null;
    $layerCount = 0;

    for ($i = 0; $i < $numTypes; $i++) {
        $type = ddnet_u32($bytes, $typesStart + $i * 12);
        if ($type !== 5) { // MAPITEMTYPE_LAYER
            continue;
        }
        $first = ddnet_u32($bytes, $typesStart + $i * 12 + 4);
        $count = ddnet_u32($bytes, $typesStart + $i * 12 + 8);
        $layerCount += $count;
        if ($first + $count > $numItems) {
            throw new EditorUserError('Таблица слоёв карты повреждена.');
        }
        for ($itemIndex = $first; $itemIndex < $first + $count; $itemIndex++) {
            $itemOffset = ddnet_u32($bytes, $typesEnd + $itemIndex * 4);
            if ($itemOffset + 8 > $itemSize) {
                if ($verifyAllBlocks) {
                    throw new EditorUserError('Слой карты выходит за границы данных.');
                }
                continue;
            }
            $typeAndId = ddnet_u32($bytes, $itemStart + $itemOffset);
            if ((($typeAndId >> 16) & 0xffff) !== 5) {
                continue;
            }
            $payloadSize = ddnet_u32($bytes, $itemStart + $itemOffset + 4);
            $payload = $itemStart + $itemOffset + 8;
            if ($payloadSize < 60 || $payload + $payloadSize > $itemStart + $itemSize) {
                continue;
            }
            if (ddnet_i32($bytes, $payload + 4) !== 2) { // LAYERTYPE_TILES
                continue;
            }
            $tileVersion = ddnet_i32($bytes, $payload + 12);
            $width = ddnet_i32($bytes, $payload + 16);
            $height = ddnet_i32($bytes, $payload + 20);
            $flags = ddnet_u32($bytes, $payload + 24);
            $image = ddnet_i32($bytes, $payload + 52);
            $rawIndex = ddnet_i32($bytes, $payload + 56);
            if ($width < 1 || $height < 1 || $width * $height > 4000000) {
                if ($verifyAllBlocks) {
                    throw new EditorUserError('Слой карты имеет неверные размеры.');
                }
                continue;
            }
            $switchAt = $tileVersion <= 2 ? 72 : 84;
            if ($switch === null && ($flags & 16) !== 0 && $payload + $switchAt + 4 <= $payload + $payloadSize) {
                $switchIndex = ddnet_i32($bytes, $payload + $switchAt);
                if ($switchIndex >= 0 && $switchIndex < $numData) {
                    $switch = ['data' => $switchIndex, 'width' => $width, 'height' => $height, 'rows_bytes' => 4];
                }
            }
            if ($game === null && ($flags & 1) !== 0 && $image === -1) {
                if ($rawIndex < 0 || $rawIndex >= $numData) {
                    if ($verifyAllBlocks) {
                        throw new EditorUserError('Игровой слой карты имеет неверные размеры.');
                    }
                    continue;
                }
                $game = ['data' => $rawIndex, 'width' => $width, 'height' => $height, 'version' => $tileVersion];
            }
            if ($verifyAllBlocks && $game !== null && $switch !== null) {
                break 2;
            }
        }
    }

    if ($game === null) {
        throw new EditorUserError('В карте не найден поддерживаемый игровой слой.');
    }

    $rawHashes = [];
    $rawSizes = [];
    $gamePlain = null;
    $switchPlain = null;
    $last = -1;
    $uncompressedTotal = 0;

    for ($i = 0; $i < $numData; $i++) {
        $off = ddnet_u32($bytes, $itemOffsetsEnd + $i * 4);
        $plainSize = ddnet_u32($bytes, $dataOffsetsEnd + $i * 4);
        $next = $i + 1 < $numData ? ddnet_u32($bytes, $itemOffsetsEnd + ($i + 1) * 4) : $dataSize;
        if ($off < 0 || $off < $last || $next < $off || $next > $dataSize || $plainSize > $maxUncompressed) {
            throw new EditorUserError('Таблица ресурсов карты повреждена.');
        }
        $uncompressedTotal += $plainSize;
        if ($uncompressedTotal > $maxUncompressed) {
            throw new EditorUserError('Распакованные данные карты превышают допустимый предел.');
        }
        $last = $off;

        $needed = $verifyAllBlocks || $i === $game['data'] || ($switch !== null && $i === $switch['data']);
        if (!$needed) {
            continue;
        }
        $compressed = substr($bytes, $dataStart + $off, $next - $off);
        $plain = @gzuncompress($compressed, min($plainSize + 1, $maxUncompressed));
        if (!is_string($plain) || strlen($plain) !== $plainSize) {
            throw new EditorUserError('Сжатый блок карты повреждён.');
        }
        if ($verifyAllBlocks) {
            $rawHashes[] = hash('sha256', $compressed);
            $rawSizes[] = $plainSize;
        }
        if ($i === $game['data']) {
            $gamePlain = $plain;
        }
        if ($switch !== null && $i === $switch['data']) {
            if (strlen($plain) >= $switch['width'] * $switch['height'] * 4) {
                $switchPlain = $plain;
            } else {
                $switch = null;
            }
        }
    }

    if (!is_string($gamePlain)) {
        throw new EditorUserError('Данные игрового слоя не найдены.');
    }

    $gameTiles = ddnet_expand_tiles($gamePlain, $game['width'] * $game['height'], (int)$game['version']);
    $switchTiles = null;
    if ($switch !== null) {
        if (!is_string($switchPlain)) {
            throw new EditorUserError('Данные switch-слоя не найдены.');
        }
        $switchTiles = ddnet_expand_switch($switchPlain, $switch['width'] * $switch['height']);
    }

    return [
        'bytes' => $length,
        'counts' => [$numTypes, $numItems, $numData, $itemSize],
        'types_hash' => hash('sha256', substr($bytes, $typesStart, $numTypes * 12)),
        'item_offsets_hash' => hash('sha256', substr($bytes, $typesEnd, $numItems * 4)),
        'items_hash' => hash('sha256', substr($bytes, $itemStart, $itemSize)),
        'raw_hashes' => $rawHashes,
        'raw_sizes' => $rawSizes,
        'layers_count' => $layerCount,
        'game' => $game,
        'game_tiles' => $gameTiles,
        'switch' => $switch,
        'switch_tiles' => $switchTiles,
    ];
}
