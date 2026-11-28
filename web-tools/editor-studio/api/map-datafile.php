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
    if ($version >= 4) {
        $tiles = '';
        $written = 0;
        for ($offset = 0, $length = strlen($plain); $offset < $length && $written < $count; $offset += 4) {
            $tile = $plain[$offset] . $plain[$offset + 1] . $plain[$offset + 3];
            $repeat = min(ord($plain[$offset + 2]) + 1, $count - $written);
            $tiles .= str_repeat($tile, $repeat);
            $written += $repeat;
        }
        if ($written !== $count) {
            throw new EditorUserError('Игровой слой карты обрезан.');
        }
        return $tiles;
    }

    $end = $count * 4;
    if (strlen($plain) < $end) {
        throw new EditorUserError('Игровой слой карты обрезан.');
    }
    // Most tile cells are \x00\x00\x00\x00 — skip zero runs in C speed and write non-zero cells in-place.
    $tiles = str_repeat("\x00", $count * 3);
    $pos = 0;
    while ($pos < $end) {
        $z = strspn($plain, "\x00", $pos, $end - $pos);
        $pos += ($z & ~3);
        if ($pos >= $end) {
            break;
        }
        $dst = ($pos >> 2) * 3;
        $tiles[$dst] = $plain[$pos];
        $tiles[$dst + 1] = $plain[$pos + 1];
        $tiles[$dst + 2] = $plain[$pos + 3];
        $pos += 4;
    }
    return $tiles;
}

/** Expand DDNet tile layer to 1 byte/cell (tile index only) for fast analysis & rendering. */
function ddnet_expand_tile_indices(string $plain, int $count, int $version): string
{
    if ($count < 1 || $count > 4000000) {
        throw new EditorUserError('Размеры слоя карты недопустимы.');
    }
    if ($version >= 4) {
        $out = '';
        $written = 0;
        for ($offset = 0, $length = strlen($plain); $offset + 4 <= $length && $written < $count; $offset += 4) {
            $repeat = min(ord($plain[$offset + 2]) + 1, $count - $written);
            $out .= str_repeat($plain[$offset], $repeat);
            $written += $repeat;
        }
        if ($written < $count) {
            $out .= str_repeat("\x00", $count - $written);
        }
        return $out;
    }
    $len = strlen($plain);
    $avail = min($count, intdiv($len, 4));
    $end = $avail * 4;
    $out = str_repeat("\x00", $count);
    $pos = 0;
    while ($pos < $end) {
        $z = strspn($plain, "\x00", $pos, $end - $pos);
        $pos += ($z & ~3);
        if ($pos >= $end) {
            break;
        }
        $b = $plain[$pos];
        if ($b !== "\x00") {
            $out[$pos >> 2] = $b;
        }
        $pos += 4;
    }
    return $out;
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

/** Expand tele layer cells: CTeleTile [number, type] (2 bytes/cell). */
function ddnet_expand_tele(string $plain, int $count): string
{
    if ($count < 1 || $count > 4000000 || strlen($plain) < $count * 2) {
        return '';
    }
    return substr($plain, 0, $count * 2);
}

/** Expand speedup layer cells: CSpeedupTileExtra [force, max_speed, type, pad, angle_i16] (6 bytes/cell). */
function ddnet_expand_speedup(string $plain, int $count): string
{
    if ($count < 1 || $count > 4000000 || strlen($plain) < $count * 6) {
        return '';
    }
    return substr($plain, 0, $count * 6);
}

/**
 * Decode Teeworlds StrToInts/IntsToStr packed int32 array (group & layer names).
 * Each byte is stored shifted by +128 in big-endian byte order per int32.
 */
function ddnet_ints_to_str(string $bytes, int $offset, int $intCount): string
{
    if ($offset < 0 || $intCount <= 0 || $offset + $intCount * 4 > strlen($bytes)) {
        return '';
    }
    $out = '';
    for ($i = 0; $i < $intCount; $i++) {
        $v = ddnet_u32($bytes, $offset + $i * 4);
        $b0 = ((($v >> 24) & 0xff) - 128) & 0xff;
        $b1 = ((($v >> 16) & 0xff) - 128) & 0xff;
        $b2 = ((($v >> 8) & 0xff) - 128) & 0xff;
        $b3 = (($v & 0xff) - 128) & 0xff;
        foreach ([$b0, $b1, $b2, $b3] as $b) {
            if ($b === 0) {
                break 2;
            }
            $out .= chr($b);
        }
    }
    // Strip control chars and broken trailing bytes left by unpadded 12-byte map editor buffers.
    $clean = preg_replace('/[\x00-\x1F\x7F]/', '', $out);
    if (!is_string($clean)) {
        return '';
    }
    if (!mb_check_encoding($clean, 'UTF-8')) {
        $clean = @iconv('UTF-8', 'UTF-8//IGNORE', $clean);
        $clean = is_string($clean) ? $clean : '';
    }
    return trim($clean);
}

/**
 * Read a single zlib-compressed raw data block by index using parsed header tables.
 *
 * @param array<string, mixed> $tables
 */
function ddnet_read_data_block(string $bytes, array $tables, int $dataIndex, int $maxUncompressed): ?string
{
    $numData = (int)($tables['num_data'] ?? 0);
    if ($dataIndex < 0 || $dataIndex >= $numData) {
        return null;
    }
    $dataStart = (int)$tables['data_start'];
    $dataSize = (int)$tables['data_size'];
    $itemOffsetsEnd = (int)$tables['data_offsets_start'];
    $dataSizesStart = (int)$tables['data_sizes_start'];

    $off = ddnet_u32($bytes, $itemOffsetsEnd + $dataIndex * 4);
    $plainSize = ddnet_u32($bytes, $dataSizesStart + $dataIndex * 4);
    $next = $dataIndex + 1 < $numData ? ddnet_u32($bytes, $itemOffsetsEnd + ($dataIndex + 1) * 4) : $dataSize;
    if ($off < 0 || $next < $off || $next > $dataSize || $plainSize < 0 || $plainSize > $maxUncompressed) {
        return null;
    }
    if ($plainSize === 0) {
        return '';
    }
    $compressed = substr($bytes, $dataStart + $off, $next - $off);
    $plain = @gzuncompress($compressed, min($plainSize + 1, $maxUncompressed));
    if (!is_string($plain) || strlen($plain) !== $plainSize) {
        return null;
    }
    return $plain;
}

/**
 * Parse MRPG/DDNet map settings strings (from CMapItemInfoSettings.m_Settings).
 * Recognizes #zone, #text, #action_zone, #node_plant, #node_ore, #node_fish.
 *
 * @return array<string, mixed>
 */
function ddnet_parse_settings_lines(string $raw): array
{
    $parts = explode("\x00", $raw);
    $lines = [];
    $zones = [];
    $texts = [];
    $actionZones = [];
    $nodes = ['plant' => [], 'ore' => [], 'fish' => []];

    foreach ($parts as $part) {
        $line = trim($part);
        if ($line === '') {
            continue;
        }
        if (count($lines) < 200) {
            $lines[] = $line;
        }
        if (preg_match('/^#zone\s+(\d+)\s+"([^"]*)"(?:\s+(\d+))?/iu', $line, $m) === 1) {
            $num = (int)$m[1];
            $zones[$num] = [
                'number' => $num,
                'name' => trim($m[2]),
                'pvp' => isset($m[3]) ? ((int)$m[3] !== 0) : false,
            ];
            continue;
        }
        if (preg_match('/^#text\s+(\d+)\s+"([^"]*)"/iu', $line, $m) === 1) {
            $num = (int)$m[1];
            $texts[$num] = [
                'number' => $num,
                'text' => trim($m[2]),
            ];
            continue;
        }
        if (preg_match('/^#action_zone\s+(\d+)\s+"([^"]*)"/iu', $line, $m) === 1) {
            $num = (int)$m[1];
            $actionZones[$num] = [
                'number' => $num,
                'name' => trim($m[2]),
            ];
            continue;
        }
        if (preg_match('/^#node_(plant|ore|fish)\s+(\d+)\s+"([^"]*)"\s+(\d+)\s+(\d+)\s*"([^"]*)"/iu', $line, $m) === 1) {
            $kind = strtolower($m[1]);
            $num = (int)$m[2];
            $nodes[$kind][$num] = [
                'number' => $num,
                'name' => trim($m[3]),
                'level' => (int)$m[4],
                'health' => (int)$m[5],
                'properties' => trim($m[6]),
            ];
        }
    }

    return [
        'lines' => $lines,
        'zones' => $zones,
        'texts' => $texts,
        'action_zones' => $actionZones,
        'nodes' => $nodes,
    ];
}

/**
 * Fast iterator over non-zero tiles in a DDNet tile layer (supports both v3 flat
 * 4-byte cells via C-speed strspn zero skipping and v4 skip-RLE compressed streams).
 *
 * Callback signature: fn(int $tx, int $ty, int $index, int $flags): void
 */
function ddnet_iter_tile_layer_nonzero(
    string $plain,
    int $w,
    int $h,
    int $version,
    callable $cb,
    int $tx0 = 0,
    int $ty0 = 0,
    ?int $tx1 = null,
    ?int $ty1 = null
): void {
    if ($w < 1 || $h < 1) {
        return;
    }
    $tx0 = max(0, $tx0);
    $ty0 = max(0, $ty0);
    $tx1 = min($w - 1, $tx1 ?? ($w - 1));
    $ty1 = min($h - 1, $ty1 ?? ($h - 1));
    if ($tx0 > $tx1 || $ty0 > $ty1) {
        return;
    }

    $len = strlen($plain);
    $count = $w * $h;

    if ($version >= 4) {
        $written = 0;
        $minIdx = $ty0 * $w + $tx0;
        $maxIdx = $ty1 * $w + $tx1;
        for ($off = 0; $off + 4 <= $len && $written < $count; $off += 4) {
            $idxByte = ord($plain[$off]);
            $repeat = min(ord($plain[$off + 2]) + 1, $count - $written);
            if ($idxByte === 0 || $written + $repeat - 1 < $minIdx) {
                $written += $repeat;
                continue;
            }
            if ($written > $maxIdx) {
                break;
            }
            $flags = ord($plain[$off + 1]);
            $end = min($written + $repeat - 1, $maxIdx);
            $start = max($written, $minIdx);
            for ($cell = $start; $cell <= $end; $cell++) {
                $tx = $cell % $w;
                if ($tx < $tx0 || $tx > $tx1) {
                    continue;
                }
                $ty = intdiv($cell, $w);
                $cb($tx, $ty, $idxByte, $flags);
            }
            $written += $repeat;
        }
        return;
    }

    // Version <= 3: flat 4 bytes per cell [index, flags, skip, reserved].
    // Most visual layers are >95% empty (\x00\x00\x00\x00), so strspn skips whole runs in C.
    if ($len < $count * 4) {
        $h = intdiv($len, $w * 4);
        $ty1 = min($h - 1, $ty1);
        if ($ty0 > $ty1) {
            return;
        }
    }
    for ($ty = $ty0; $ty <= $ty1; $ty++) {
        $rowStart = ($ty * $w + $tx0) * 4;
        $rowEnd = ($ty * $w + $tx1 + 1) * 4;
        $pos = $rowStart;
        while ($pos < $rowEnd) {
            $z = strspn($plain, "\x00", $pos, $rowEnd - $pos);
            $pos += ($z & ~3);
            if ($pos >= $rowEnd) {
                break;
            }
            $idxByte = ord($plain[$pos]);
            if ($idxByte !== 0) {
                $tx = intdiv($pos - $ty * $w * 4, 4);
                $cb($tx, $ty, $idxByte, ord($plain[$pos + 1]));
            }
            $pos += 4;
        }
    }
}

/**
 * Fast zero-allocation check: returns true as soon as any non-zero tile is found inside [tx0..tx1, ty0..ty1].
 */
function ddnet_tile_layer_has_nonzero_in_rect(
    string $plain,
    int $w,
    int $h,
    int $version,
    int $tx0 = 0,
    int $ty0 = 0,
    ?int $tx1 = null,
    ?int $ty1 = null
): bool {
    if ($w < 1 || $h < 1) {
        return false;
    }
    $tx0 = max(0, $tx0);
    $ty0 = max(0, $ty0);
    $tx1 = min($w - 1, $tx1 ?? ($w - 1));
    $ty1 = min($h - 1, $ty1 ?? ($h - 1));
    if ($tx0 > $tx1 || $ty0 > $ty1) {
        return false;
    }
    $len = strlen($plain);
    $count = $w * $h;

    if ($version >= 4) {
        $written = 0;
        $minIdx = $ty0 * $w + $tx0;
        $maxIdx = $ty1 * $w + $tx1;
        for ($off = 0; $off + 4 <= $len && $written < $count; $off += 4) {
            $repeat = min(ord($plain[$off + 2]) + 1, $count - $written);
            if ($plain[$off] === "\x00" || $written + $repeat - 1 < $minIdx) {
                $written += $repeat;
                continue;
            }
            if ($written > $maxIdx) {
                return false;
            }
            $start = max($written, $minIdx);
            $end = min($written + $repeat - 1, $maxIdx);
            if ($tx0 === 0 && $tx1 === $w - 1) {
                return true;
            }
            $y0 = intdiv($start, $w);
            $y1 = intdiv($end, $w);
            if ($y1 > $y0) {
                // Spans across row wrap: check tail of y0, head of y1, or full middle row.
                if ($y1 - $y0 > 1) {
                    return true;
                }
                if (($start % $w) <= $tx1 || ($end % $w) >= $tx0) {
                    return true;
                }
            } else {
                $x0 = $start % $w;
                $x1 = $end % $w;
                if ($x0 <= $tx1 && $x1 >= $tx0) {
                    return true;
                }
            }
            $written += $repeat;
        }
        return false;
    }

    if ($len < $count * 4) {
        $h = intdiv($len, $w * 4);
        $ty1 = min($h - 1, $ty1);
        if ($ty0 > $ty1) {
            return false;
        }
    }
    $rowBytes = $w * 4;
    for ($ty = $ty0; $ty <= $ty1; $ty++) {
        $rowBase = $ty * $rowBytes;
        $pos = $rowBase + $tx0 * 4;
        $rowEnd = $rowBase + ($tx1 + 1) * 4;
        while ($pos < $rowEnd) {
            $z = strspn($plain, "\x00", $pos, $rowEnd - $pos);
            $pos += ($z & ~3);
            if ($pos >= $rowEnd) {
                break;
            }
            if ($plain[$pos] !== "\x00") {
                return true;
            }
            $pos += 4;
        }
    }
    return false;
}

/**
 * Parse a CQuad binary data block (152 bytes per quad).
 *
 * @return array<int, array<string, mixed>>
 */
function ddnet_parse_quads_block(string $plain, int $numQuads, int $maxQuads = 2000): array
{
    $len = strlen($plain);
    $actual = min($numQuads, $maxQuads, intdiv($len, 152));
    $quads = [];
    for ($i = 0; $i < $actual; $i++) {
        $base = $i * 152;
        $ints = unpack('V38', substr($plain, $base, 152));
        if (!is_array($ints)) {
            continue;
        }
        $toSigned = static fn(int $u): int => $u >= 0x80000000 ? $u - 0x100000000 : $u;
        $pts = [];
        for ($p = 0; $p < 5; $p++) {
            $pts[] = [
                $toSigned((int)$ints[1 + $p * 2]) / 1024.0,
                $toSigned((int)$ints[2 + $p * 2]) / 1024.0,
            ];
        }
        $cols = [];
        for ($c = 0; $c < 4; $c++) {
            $cols[] = [
                max(0, min(255, $toSigned((int)$ints[11 + $c * 4]))),
                max(0, min(255, $toSigned((int)$ints[12 + $c * 4]))),
                max(0, min(255, $toSigned((int)$ints[13 + $c * 4]))),
                max(0, min(255, $toSigned((int)$ints[14 + $c * 4]))),
            ];
        }
        $uvs = [];
        for ($u = 0; $u < 4; $u++) {
            $uvs[] = [
                $toSigned((int)$ints[27 + $u * 2]) / 1024.0,
                $toSigned((int)$ints[28 + $u * 2]) / 1024.0,
            ];
        }
        // Bounding box of the 4 quad corners (indices 0..3; index 4 is pivot).
        $xs = [$pts[0][0], $pts[1][0], $pts[2][0], $pts[3][0]];
        $ys = [$pts[0][1], $pts[1][1], $pts[2][1], $pts[3][1]];
        $quads[] = [
            'points' => $pts,
            'colors' => $cols,
            'texcoords' => $uvs,
            'min_x' => min($xs),
            'max_x' => max($xs),
            'min_y' => min($ys),
            'max_y' => max($ys),
        ];
    }
    return $quads;
}

/**
 * Full DDNet v4 binary map parser for rich AI map inspection and rendering.
 * Extracts INFO/settings, images, groups, ALL layers (game, front, switch, tele,
 * speedup, visual tile layers, visual quad layers), keeping heavy visual/image
 * data blocks lazy via $tables + ddnet_read_data_block().
 *
 * @return array<string, mixed>
 */
function ddnet_parse_full_map(string $bytes, int $maxBytes, int $maxUncompressed): array
{
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

    $tables = [
        'num_data' => $numData,
        'data_offsets_start' => $itemOffsetsEnd,
        'data_sizes_start' => $dataOffsetsEnd,
        'data_start' => $dataStart,
        'data_size' => $dataSize,
    ];

    $info = [
        'author' => null,
        'map_version' => null,
        'credits' => null,
        'license' => null,
        'settings_lines' => [],
        'settings' => ['zones' => [], 'texts' => [], 'action_zones' => [], 'nodes' => ['plant' => [], 'ore' => [], 'fish' => []]],
    ];
    $images = [];
    $groups = [];
    $layers = [];
    $game = null;
    $front = null;
    $switch = null;
    $tele = null;
    $speedup = null;

    for ($i = 0; $i < $numTypes; $i++) {
        $type = ddnet_u32($bytes, $typesStart + $i * 12);
        $first = ddnet_u32($bytes, $typesStart + $i * 12 + 4);
        $count = ddnet_u32($bytes, $typesStart + $i * 12 + 8);
        if ($first + $count > $numItems) {
            throw new EditorUserError('Таблица элементов карты повреждена.');
        }

        for ($itemIndex = $first; $itemIndex < $first + $count; $itemIndex++) {
            $itemOffset = ddnet_u32($bytes, $typesEnd + $itemIndex * 4);
            if ($itemOffset + 8 > $itemSize) {
                continue;
            }
            $typeAndId = ddnet_u32($bytes, $itemStart + $itemOffset);
            $itemType = ($typeAndId >> 16) & 0xffff;
            $itemId = $typeAndId & 0xffff;
            if ($itemType !== $type) {
                continue;
            }
            $payloadSize = ddnet_u32($bytes, $itemStart + $itemOffset + 4);
            $payload = $itemStart + $itemOffset + 8;
            if ($payload + $payloadSize > $itemStart + $itemSize) {
                continue;
            }

            // MAPITEMTYPE_INFO = 1
            if ($type === 1 && $payloadSize >= 20) {
                $readStrBlock = static function (int $idx) use ($bytes, $tables, $maxUncompressed): ?string {
                    if ($idx < 0) {
                        return null;
                    }
                    $raw = ddnet_read_data_block($bytes, $tables, $idx, min($maxUncompressed, 1048576));
                    if (!is_string($raw)) {
                        return null;
                    }
                    $trimmed = trim(rtrim($raw, "\x00"));
                    return $trimmed === '' ? null : $trimmed;
                };
                $info['author'] = $readStrBlock(ddnet_i32($bytes, $payload + 4));
                $info['map_version'] = $readStrBlock(ddnet_i32($bytes, $payload + 8));
                $info['credits'] = $readStrBlock(ddnet_i32($bytes, $payload + 12));
                $info['license'] = $readStrBlock(ddnet_i32($bytes, $payload + 16));
                if ($payloadSize >= 24) {
                    $settingsIdx = ddnet_i32($bytes, $payload + 20);
                    if ($settingsIdx >= 0) {
                        $rawSettings = ddnet_read_data_block($bytes, $tables, $settingsIdx, min($maxUncompressed, 1048576));
                        if (is_string($rawSettings) && $rawSettings !== '') {
                            $parsedSet = ddnet_parse_settings_lines($rawSettings);
                            $info['settings_lines'] = $parsedSet['lines'];
                            $info['settings'] = [
                                'zones' => $parsedSet['zones'],
                                'texts' => $parsedSet['texts'],
                                'action_zones' => $parsedSet['action_zones'],
                                'nodes' => $parsedSet['nodes'],
                            ];
                        }
                    }
                }
                continue;
            }

            // MAPITEMTYPE_IMAGE = 2
            if ($type === 2 && $payloadSize >= 24) {
                $imgW = ddnet_i32($bytes, $payload + 4);
                $imgH = ddnet_i32($bytes, $payload + 8);
                $external = ddnet_i32($bytes, $payload + 12) !== 0;
                $nameIdx = ddnet_i32($bytes, $payload + 16);
                $dataIdx = ddnet_i32($bytes, $payload + 20);
                $imgName = '';
                if ($nameIdx >= 0) {
                    $rawName = ddnet_read_data_block($bytes, $tables, $nameIdx, 65536);
                    if (is_string($rawName)) {
                        $imgName = trim(rtrim($rawName, "\x00"));
                    }
                }
                $images[] = [
                    'index' => count($images),
                    'id' => $itemId,
                    'name' => $imgName,
                    'width' => max(0, $imgW),
                    'height' => max(0, $imgH),
                    'external' => $external,
                    'data_index' => $dataIdx,
                ];
                continue;
            }

            // MAPITEMTYPE_GROUP = 4
            if ($type === 4 && $payloadSize >= 28) {
                $gVer = ddnet_i32($bytes, $payload);
                $offX = ddnet_i32($bytes, $payload + 4);
                $offY = ddnet_i32($bytes, $payload + 8);
                $paraX = ddnet_i32($bytes, $payload + 12);
                $paraY = ddnet_i32($bytes, $payload + 16);
                $startL = ddnet_i32($bytes, $payload + 20);
                $numL = ddnet_i32($bytes, $payload + 24);
                $useClip = ($gVer >= 2 && $payloadSize >= 48) ? (ddnet_i32($bytes, $payload + 28) !== 0) : false;
                $clip = $useClip ? [
                    'x' => ddnet_i32($bytes, $payload + 32),
                    'y' => ddnet_i32($bytes, $payload + 36),
                    'w' => ddnet_i32($bytes, $payload + 40),
                    'h' => ddnet_i32($bytes, $payload + 44),
                ] : null;
                $gName = ($gVer >= 3 && $payloadSize >= 60) ? ddnet_ints_to_str($bytes, $payload + 48, 3) : '';
                $groups[] = [
                    'index' => count($groups),
                    'id' => $itemId,
                    'name' => $gName,
                    'offset_x' => $offX,
                    'offset_y' => $offY,
                    'parallax_x' => $paraX,
                    'parallax_y' => $paraY,
                    'start_layer' => $startL,
                    'num_layers' => $numL,
                    'use_clipping' => $useClip,
                    'clip' => $clip,
                ];
                continue;
            }

            // MAPITEMTYPE_LAYER = 5
            if ($type === 5 && $payloadSize >= 12) {
                $layerType = ddnet_i32($bytes, $payload + 4);
                $layerFlags = ddnet_u32($bytes, $payload + 8);
                $layerIdx = count($layers);

                if ($layerType === 2 && $payloadSize >= 60) { // LAYERTYPE_TILES
                    $tileVersion = ddnet_i32($bytes, $payload + 12);
                    $width = ddnet_i32($bytes, $payload + 16);
                    $height = ddnet_i32($bytes, $payload + 20);
                    $flags = ddnet_u32($bytes, $payload + 24);
                    $rgba = [
                        max(0, min(255, ddnet_i32($bytes, $payload + 28))),
                        max(0, min(255, ddnet_i32($bytes, $payload + 32))),
                        max(0, min(255, ddnet_i32($bytes, $payload + 36))),
                        max(0, min(255, ddnet_i32($bytes, $payload + 40))),
                    ];
                    $imageIdx = ddnet_i32($bytes, $payload + 52);
                    $rawIndex = ddnet_i32($bytes, $payload + 56);
                    $layerName = ($tileVersion >= 3 && $payloadSize >= 72)
                        ? ddnet_ints_to_str($bytes, $payload + 60, 3)
                        : '';

                    $role = 'visual_tiles';
                    if (($flags & 1) !== 0 && $imageIdx === -1) {
                        $role = 'game';
                    } elseif (($flags & 16) !== 0) {
                        $role = 'switch';
                    } elseif (($flags & 8) !== 0) {
                        $role = 'front';
                    } elseif (($flags & 2) !== 0) {
                        $role = 'tele';
                    } elseif (($flags & 4) !== 0) {
                        $role = 'speedup';
                    }

                    $layers[] = [
                        'index' => $layerIdx,
                        'id' => $itemId,
                        'type' => 'tiles',
                        'role' => $role,
                        'name' => $layerName,
                        'layer_flags' => $layerFlags,
                        'tile_version' => $tileVersion,
                        'tile_flags' => $flags,
                        'width' => $width,
                        'height' => $height,
                        'rgba' => $rgba,
                        'image_index' => $imageIdx,
                        'data_index' => $rawIndex,
                    ];

                    if ($width >= 1 && $height >= 1 && $width * $height <= 4000000) {
                        if ($game === null && ($flags & 1) !== 0 && $imageIdx === -1 && $rawIndex >= 0 && $rawIndex < $numData) {
                            $game = ['data' => $rawIndex, 'width' => $width, 'height' => $height, 'version' => $tileVersion, 'layer_index' => $layerIdx];
                        }
                        $teleAt = $tileVersion <= 2 ? 60 : 72;
                        if ($tele === null && ($flags & 2) !== 0 && $payloadSize >= $teleAt + 4) {
                            $tIdx = ddnet_i32($bytes, $payload + $teleAt);
                            if ($tIdx >= 0 && $tIdx < $numData) {
                                $tele = ['data' => $tIdx, 'width' => $width, 'height' => $height, 'layer_index' => $layerIdx];
                            }
                        }
                        $speedAt = $tileVersion <= 2 ? 64 : 76;
                        if ($speedup === null && ($flags & 4) !== 0 && $payloadSize >= $speedAt + 4) {
                            $spIdx = ddnet_i32($bytes, $payload + $speedAt);
                            if ($spIdx >= 0 && $spIdx < $numData) {
                                $speedup = ['data' => $spIdx, 'width' => $width, 'height' => $height, 'layer_index' => $layerIdx];
                            }
                        }
                        $frontAt = $tileVersion <= 2 ? 68 : 80;
                        if ($front === null && ($flags & 8) !== 0 && $payloadSize >= $frontAt + 4) {
                            $fIdx = ddnet_i32($bytes, $payload + $frontAt);
                            if ($fIdx >= 0 && $fIdx < $numData) {
                                $front = ['data' => $fIdx, 'width' => $width, 'height' => $height, 'version' => $tileVersion, 'layer_index' => $layerIdx];
                            }
                        }
                        $switchAt = $tileVersion <= 2 ? 72 : 84;
                        if ($switch === null && ($flags & 16) !== 0 && $payloadSize >= $switchAt + 4) {
                            $swIdx = ddnet_i32($bytes, $payload + $switchAt);
                            if ($swIdx >= 0 && $swIdx < $numData) {
                                $switch = ['data' => $swIdx, 'width' => $width, 'height' => $height, 'layer_index' => $layerIdx];
                            }
                        }
                    }
                } elseif ($layerType === 3 && $payloadSize >= 28) { // LAYERTYPE_QUADS
                    $qVer = ddnet_i32($bytes, $payload + 12);
                    $numQuads = max(0, ddnet_i32($bytes, $payload + 16));
                    $qDataIdx = ddnet_i32($bytes, $payload + 20);
                    $qImageIdx = ddnet_i32($bytes, $payload + 24);
                    $layerName = ($qVer >= 2 && $payloadSize >= 40)
                        ? ddnet_ints_to_str($bytes, $payload + 28, 3)
                        : '';
                    $layers[] = [
                        'index' => $layerIdx,
                        'id' => $itemId,
                        'type' => 'quads',
                        'role' => 'visual_quads',
                        'name' => $layerName,
                        'layer_flags' => $layerFlags,
                        'num_quads' => $numQuads,
                        'image_index' => $qImageIdx,
                        'data_index' => $qDataIdx,
                    ];
                } else {
                    $layers[] = [
                        'index' => $layerIdx,
                        'id' => $itemId,
                        'type' => 'other_' . $layerType,
                        'role' => 'other',
                        'name' => '',
                        'layer_flags' => $layerFlags,
                        'image_index' => -1,
                        'data_index' => -1,
                    ];
                }
            }
        }
    }

    if ($game === null) {
        throw new EditorUserError('В карте не найден поддерживаемый игровой слой.');
    }

    // Attach image names and parent group metadata (parallax, offset) to every layer.
    foreach ($layers as &$layer) {
        $imgIdx = (int)($layer['image_index'] ?? -1);
        $layer['image_name'] = ($imgIdx >= 0 && isset($images[$imgIdx])) ? (string)$images[$imgIdx]['name'] : null;
        $layer['group_index'] = null;
        $layer['group_name'] = '';
        $layer['parallax_x'] = 100;
        $layer['parallax_y'] = 100;
        $layer['offset_x'] = 0;
        $layer['offset_y'] = 0;
    }
    unset($layer);

    foreach ($groups as $gIdx => $grp) {
        $start = (int)$grp['start_layer'];
        $end = $start + max(0, (int)$grp['num_layers']);
        for ($li = $start; $li < $end; $li++) {
            if (!isset($layers[$li])) {
                continue;
            }
            $layers[$li]['group_index'] = $gIdx;
            $layers[$li]['group_name'] = (string)$grp['name'];
            $layers[$li]['parallax_x'] = (int)$grp['parallax_x'];
            $layers[$li]['parallax_y'] = (int)$grp['parallax_y'];
            $layers[$li]['offset_x'] = (int)$grp['offset_x'];
            $layers[$li]['offset_y'] = (int)$grp['offset_y'];
        }
    }

    // Expand gameplay layers eagerly (compact 1-byte/cell index grids for fast analysis & rendering).
    $gamePlain = ddnet_read_data_block($bytes, $tables, (int)$game['data'], $maxUncompressed);
    if (!is_string($gamePlain)) {
        throw new EditorUserError('Данные игрового слоя не найдены.');
    }
    $gameTiles = ddnet_expand_tile_indices($gamePlain, (int)$game['width'] * (int)$game['height'], (int)$game['version']);

    $frontTiles = null;
    if ($front !== null) {
        $fp = ddnet_read_data_block($bytes, $tables, (int)$front['data'], $maxUncompressed);
        if (is_string($fp)) {
            try {
                $frontTiles = ddnet_expand_tile_indices($fp, (int)$front['width'] * (int)$front['height'], (int)$front['version']);
            } catch (Throwable) {
                $front = null;
            }
        } else {
            $front = null;
        }
    }

    $switchTiles = null;
    if ($switch !== null) {
        $sp = ddnet_read_data_block($bytes, $tables, (int)$switch['data'], $maxUncompressed);
        if (is_string($sp) && strlen($sp) >= (int)$switch['width'] * (int)$switch['height'] * 4) {
            $switchTiles = ddnet_expand_switch($sp, (int)$switch['width'] * (int)$switch['height']);
        } else {
            $switch = null;
        }
    }

    $teleTiles = null;
    if ($tele !== null) {
        $tp = ddnet_read_data_block($bytes, $tables, (int)$tele['data'], $maxUncompressed);
        if (is_string($tp) && strlen($tp) >= (int)$tele['width'] * (int)$tele['height'] * 2) {
            $teleTiles = ddnet_expand_tele($tp, (int)$tele['width'] * (int)$tele['height']);
        } else {
            $tele = null;
        }
    }

    $speedupTiles = null;
    if ($speedup !== null) {
        $upp = ddnet_read_data_block($bytes, $tables, (int)$speedup['data'], $maxUncompressed);
        if (is_string($upp) && strlen($upp) >= (int)$speedup['width'] * (int)$speedup['height'] * 6) {
            $speedupTiles = ddnet_expand_speedup($upp, (int)$speedup['width'] * (int)$speedup['height']);
        } else {
            $speedup = null;
        }
    }

    return [
        'bytes' => $length,
        'sha256' => hash('sha256', $bytes),
        'tables' => $tables,
        'info' => $info,
        'images' => $images,
        'groups' => $groups,
        'layers' => $layers,
        'layers_count' => count($layers),
        'game' => $game,
        'game_tiles' => $gameTiles,
        'front' => $front,
        'front_tiles' => $frontTiles,
        'switch' => $switch,
        'switch_tiles' => $switchTiles,
        'tele' => $tele,
        'tele_tiles' => $teleTiles,
        'speedup' => $speedup,
        'speedup_tiles' => $speedupTiles,
    ];
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
