<?php
declare(strict_types=1);

/**
 * Server-side DDNet/MRPG map screenshot & region renderer for Editor Studio AI.
 *
 * Produces real visual PNG screenshots (combining embedded RGBA tilesets, external
 * data/mapres/*.png tilesets, sky/background quads, and optional semantic overlays
 * for buildings, houses, castles, zones, collision, spawns, NPCs, and coordinate rulers)
 * so multimodal AI models can directly inspect maps and regions.
 *
 * Uses ext-gd as the primary high-detail renderer with a pure-PHP zlib PNG fallback
 * when ext-gd is unavailable.
 */

require_once __DIR__ . '/map-analysis.php';

/** Canonical screenshot rendering modes exposed in schema & OpenAPI. */
function ai_map_screenshot_modes(): array
{
    return [
        'hybrid' => 'Реальный визуальный рендер тайлсетов и квадов карты + полупрозрачные рамки построек/домов/замков, подписи зон, маркеры объектов и линейка координат (рекомендуется по умолчанию)',
        'visual' => 'Чистый визуальный вид карты (небо, квады, визуальные слои тайлов как в игре) без схематичных заливок',
        'schematic' => 'Структурно-коллизионная схема карты (земля, стены построек, крытые интерьеры, пещеры, вода, смерть, дома, сервисы, спавны и объекты БД)',
    ];
}

/**
 * Safely load an external mapres PNG asset by name (e.g. "grass_main" -> "grass_main.png")
 * without ever exposing internal filesystem paths or bridge tokens.
 */
function ai_map_load_mapres_png(string $imageName): ?string
{
    $clean = trim(str_replace('\\', '/', $imageName));
    if ($clean === '' || str_contains($clean, '..') || str_starts_with($clean, '/')) {
        return null;
    }
    $clean = ltrim($clean, '/');
    $candidates = str_ends_with(strtolower($clean), '.png') ? [$clean] : [$clean . '.png'];

    $cfg = function_exists('load_cfg') ? load_cfg() : [];
    $dirs = ddnet_map_existing_dirs($cfg, 'EDITOR_MAPRES_DIR', ['data/mapres', 'mapres'], 'mapres_dir');
    foreach ($dirs as $dir) {
        foreach ($candidates as $cand) {
            $file = ddnet_map_resolve($dir, $cand);
            if ($file !== null && is_file($file) && @filesize($file) <= 16 * 1024 * 1024) {
                $raw = @file_get_contents($file);
                if (is_string($raw) && strlen($raw) > 24 && substr($raw, 0, 8) === "\x89PNG\r\n\x1a\n") {
                    return $raw;
                }
            }
        }
    }

    // Optional map_api bridge fallback (matches api/map.php asset proxy).
    $mapApi = trim((string)($cfg['map_api'] ?? ''));
    if ($mapApi !== '' && function_exists('map_api_request')) {
        $sep = str_contains($mapApi, '?') ? '&' : '?';
        $q = http_build_query(['action' => 'asset', 'name' => $candidates[0]]);
        $resp = map_api_request($mapApi . $sep . $q, (string)($cfg['map_api_token'] ?? ''), 16 * 1024 * 1024);
        if (!empty($resp['ok']) && is_string($resp['body']) && substr($resp['body'], 0, 8) === "\x89PNG\r\n\x1a\n") {
            return $resp['body'];
        }
    }

    return null;
}

/** Semantic RGB color palette for structure kinds (used across GD & pure-PHP fallback). */
function ai_map_structure_color(string $kind): array
{
    return match ($kind) {
        'castle_or_guild_hall' => [245, 158, 11],  // amber/gold
        'house' => [16, 185, 129],                 // emerald
        'shop_or_service' => [6, 182, 212],        // cyan
        'dungeon_or_ruins' => [168, 85, 247],      // purple
        'cave_or_underground' => [217, 119, 6],    // bronze
        default => [56, 189, 248],                 // sky blue
    };
}

/** Category fallback RGB tint when an external tileset PNG is unavailable. */
function ai_map_category_fallback_rgb(string $category): array
{
    return match ($category) {
        'architecture' => [168, 134, 102],
        'interior' => [110, 92, 78],
        'terrain' => [102, 146, 82],
        'nature' => [64, 138, 68],
        'water' => [48, 132, 198],
        'props' => [186, 142, 78],
        default => [128, 138, 150],
    };
}

/**
 * Render a map screenshot PNG for any viewport (full overview, spawn, structure, house,
 * guild hall, zone, warehouse, x/y+radius, or explicit pixel/tile box).
 *
 * @param array<string, mixed> $analysis Output of ai_map_analyze_full()
 * @param array<string, mixed> $options  Render parameters
 * @return array<string, mixed>
 */
function ai_map_render_screenshot(string $bytes, array $analysis, array $options = []): array
{
    $maxBytes = defined('AI_MAP_MAX_BYTES') ? AI_MAP_MAX_BYTES : (16 * 1024 * 1024);
    $maxUncompressed = defined('AI_MAP_MAX_UNCOMPRESSED') ? AI_MAP_MAX_UNCOMPRESSED : (64 * 1024 * 1024);

    if (empty($options['no_cache']) && function_exists('ai_map_static_analysis_get')) {
        $parsed = ai_map_static_analysis_get($bytes, $maxBytes, $maxUncompressed, true)['parsed'];
    } else {
        $parsed = ddnet_parse_full_map($bytes, $maxBytes, $maxUncompressed);
    }
    $gw = (int)$parsed['game']['width'];
    $gh = (int)$parsed['game']['height'];

    $mode = strtolower(trim((string)($options['mode'] ?? 'hybrid')));
    if (!in_array($mode, ['hybrid', 'visual', 'schematic'], true)) {
        $mode = 'hybrid';
    }

    $padTiles = isset($options['pad_tiles']) ? max(0, min(40, (int)$options['pad_tiles'])) : 6;
    $vp = ai_map_resolve_viewport($analysis, $options, $padTiles);
    $tx0 = $vp['tx0'];
    $ty0 = $vp['ty0'];
    $tx1 = $vp['tx1'];
    $ty1 = $vp['ty1'];
    $bw = max(1, $tx1 - $tx0 + 1);
    $bh = max(1, $ty1 - $ty0 + 1);

    $showRuler = array_key_exists('show_grid', $options) ? (bool)$options['show_grid'] : true;
    $showStructures = array_key_exists('show_structures', $options) ? (bool)$options['show_structures'] : ($mode !== 'visual');
    $showLabels = array_key_exists('show_labels', $options) ? (bool)$options['show_labels'] : ($mode !== 'visual');
    $showObjects = array_key_exists('show_objects', $options) ? (bool)$options['show_objects'] : ($mode !== 'visual');
    $showCollision = array_key_exists('show_collision', $options) ? (bool)$options['show_collision'] : ($mode === 'schematic');

    $targetW = max(256, min(1600, (int)($options['width'] ?? 1024)));
    $maxH = max(256, min(1200, (int)($options['height'] ?? 768)));

    $marginLeft = $showRuler ? 42 : 0;
    $marginTop = $showRuler ? 24 : 0;
    $marginBottom = $showRuler ? 26 : 0;

    $availW = max(128, $targetW - $marginLeft);
    $availH = max(128, $maxH - $marginTop - $marginBottom);
    $tilePx = min($availW / $bw, $availH / $bh);
    if ($tilePx > 32.0) {
        $tilePx = 32.0;
    }

    $drawW = max(1, (int)round($bw * $tilePx));
    $drawH = max(1, (int)round($bh * $tilePx));
    $imgW = $marginLeft + $drawW;
    $imgH = $marginTop + $drawH + $marginBottom;

    // Collect structures, zones, and DB objects overlapping the viewport.
    $visibleStructures = [];
    foreach ($analysis['structures']['items'] ?? [] as $st) {
        $b = $st['bbox_tiles'];
        if ($b['tx1'] >= $tx0 && $b['tx0'] <= $tx1 && $b['ty1'] >= $ty0 && $b['ty0'] <= $ty1) {
            $visibleStructures[] = $st;
        }
    }
    $visibleZones = [];
    foreach ($analysis['zones']['named_zones'] ?? [] as $z) {
        $b = $z['bbox_tiles'];
        if ($b['tx1'] >= $tx0 && $b['tx0'] <= $tx1 && $b['ty1'] >= $ty0 && $b['ty0'] <= $ty1) {
            $visibleZones[] = $z;
        }
    }
    $visibleObjects = [];
    $wo = $analysis['world_objects'] ?? [];
    foreach ([
        'warehouses' => ['W', [6, 182, 212]],
        'aethers' => ['A', [192, 132, 252]],
        'world_swaps' => ['P', [244, 114, 182]],
        'quest_boards' => ['Q', [250, 204, 21]],
        'dungeons' => ['D', [239, 68, 68]],
        'npcs' => ['N', [52, 211, 153]],
    ] as $key => [$tag, $rgb]) {
        foreach ($wo[$key] ?? [] as $o) {
            if (isset($o['tile_x'], $o['tile_y']) && $o['tile_x'] >= $tx0 && $o['tile_x'] <= $tx1 && $o['tile_y'] >= $ty0 && $o['tile_y'] <= $ty1) {
                $visibleObjects[] = [
                    'type' => $key,
                    'tag' => $tag,
                    'rgb' => $rgb,
                    'id' => $o['id'] ?? null,
                    'name' => (string)($o['name'] ?? ($key . ' #' . ($o['id'] ?? ''))),
                    'x' => (int)$o['x'],
                    'y' => (int)$o['y'],
                    'tile_x' => (int)$o['tile_x'],
                    'tile_y' => (int)$o['tile_y'],
                ];
            }
        }
    }

    $highlightPt = null;
    if (isset($options['highlight_x'], $options['highlight_y']) && $options['highlight_x'] !== null && $options['highlight_y'] !== null) {
        $highlightPt = ['x' => (int)$options['highlight_x'], 'y' => (int)$options['highlight_y']];
    } elseif (isset($options['x'], $options['y']) && $options['x'] !== null && $options['y'] !== null && !isset($options['tx0'], $options['x0'])) {
        $highlightPt = ['x' => (int)$options['x'], 'y' => (int)$options['y']];
    }

    $missingExternal = [];
    $forceFallback = !empty($options['_force_php_fallback']);
    $useGd = !$forceFallback && extension_loaded('gd') && function_exists('imagecreatetruecolor') && function_exists('imagepng');

    $ctx = [
        'bytes' => $bytes,
        'parsed' => $parsed,
        'analysis' => $analysis,
        'mode' => $mode,
        'tx0' => $tx0,
        'ty0' => $ty0,
        'tx1' => $tx1,
        'ty1' => $ty1,
        'bw' => $bw,
        'bh' => $bh,
        'tilePx' => $tilePx,
        'marginLeft' => $marginLeft,
        'marginTop' => $marginTop,
        'marginBottom' => $marginBottom,
        'drawW' => $drawW,
        'drawH' => $drawH,
        'imgW' => $imgW,
        'imgH' => $imgH,
        'showRuler' => $showRuler,
        'showStructures' => $showStructures,
        'showLabels' => $showLabels,
        'showObjects' => $showObjects,
        'showCollision' => $showCollision,
        'visibleStructures' => $visibleStructures,
        'visibleZones' => $visibleZones,
        'visibleObjects' => $visibleObjects,
        'highlightPt' => $highlightPt,
        'maxUncompressed' => $maxUncompressed,
    ];

    if ($useGd) {
        $png = ai_map_render_gd($ctx, $missingExternal);
        $renderer = 'gd';
    } else {
        $png = ai_map_render_pure_php($ctx);
        $renderer = 'php-fallback';
    }

    return [
        'png_bytes' => $png,
        'mime_type' => 'image/png',
        'renderer' => $renderer,
        'mode' => $mode,
        'width_px' => $imgW,
        'height_px' => $imgH,
        'bytes' => strlen($png),
        'sha256' => hash('sha256', $png),
        'viewport' => [
            'label' => $vp['label'],
            'target_resolved' => $vp['target_resolved'],
            'tx0' => $tx0,
            'ty0' => $ty0,
            'tx1' => $tx1,
            'ty1' => $ty1,
            'x0' => $tx0 * 32,
            'y0' => $ty0 * 32,
            'x1' => ($tx1 + 1) * 32,
            'y1' => ($ty1 + 1) * 32,
            'width_tiles' => $bw,
            'height_tiles' => $bh,
        ],
        'coordinate_mapping' => [
            'map_origin_img_x' => $marginLeft,
            'map_origin_img_y' => $marginTop,
            'map_draw_width_px' => $drawW,
            'map_draw_height_px' => $drawH,
            'tile_px' => round($tilePx, 4),
            'world_px_per_img_px' => round(32.0 / max(0.0001, $tilePx), 4),
            'formula' => 'world_x = x0 + (img_x - map_origin_img_x) * (32 / tile_px); world_y = y0 + (img_y - map_origin_img_y) * (32 / tile_px)',
        ],
        'visible_structures' => array_map(static fn(array $s): array => [
            'id' => $s['id'],
            'kind' => $s['kind'],
            'name' => $s['name'],
            'bbox_tiles' => $s['bbox_tiles'],
            'center' => $s['center'],
        ], array_slice($visibleStructures, 0, 40)),
        'visible_zones' => array_map(static fn(array $z): array => [
            'number' => $z['number'],
            'name' => $z['name'],
            'environment' => $z['environment'],
        ], array_slice($visibleZones, 0, 30)),
        'visible_objects_count' => count($visibleObjects),
        'missing_external_tilesets' => array_values(array_unique($missingExternal)),
    ];
}

/**
 * Sample sky gradient top/bottom RGB from map's first sky quad layer (parallax 0,0),
 * falling back to a pleasant daytime Teeworlds sky gradient.
 *
 * @return array{0:array{0:int,1:int,2:int},1:array{0:int,1:int,2:int}}
 */
function ai_map_extract_sky_gradient(string $bytes, array $parsed, int $maxUncompressed): array
{
    $top = [92, 146, 206];
    $bot = [184, 214, 240];
    foreach ($parsed['layers'] as $layer) {
        if (($layer['role'] ?? '') !== 'visual_quads') {
            continue;
        }
        if ((int)$layer['parallax_x'] !== 0 || (int)$layer['parallax_y'] !== 0) {
            continue;
        }
        if ((int)($layer['image_index'] ?? -1) !== -1 || (int)($layer['num_quads'] ?? 0) < 1) {
            continue;
        }
        $plain = ddnet_read_data_block($bytes, $parsed['tables'], (int)$layer['data_index'], $maxUncompressed);
        if (!is_string($plain) || strlen($plain) < 152) {
            continue;
        }
        $quads = ddnet_parse_quads_block($plain, 1, 1);
        if ($quads !== []) {
            $c = $quads[0]['colors'];
            // Vertex 0 & 1 are top corners; 2 & 3 are bottom corners in DDNet quads.
            $top = [
                intdiv($c[0][0] + $c[1][0], 2),
                intdiv($c[0][1] + $c[1][1], 2),
                intdiv($c[0][2] + $c[1][2], 2),
            ];
            $bot = [
                intdiv($c[2][0] + $c[3][0], 2),
                intdiv($c[2][1] + $c[3][1], 2),
                intdiv($c[2][2] + $c[3][2], 2),
            ];
            return [$top, $bot];
        }
    }
    return [$top, $bot];
}

/**
 * Decode a map image (embedded raw RGBA or external data/mapres/*.png) into a GD image
 * plus precomputed 16x16 per-tile average RGBA swatch array for fast overview rendering.
 *
 * @return array{gd:\GdImage|null,w:int,h:int,swatches:array<int, array{0:int,1:int,2:int,3:int}>}|null
 */
function ai_map_load_image_gd(
    string $bytes,
    array $parsed,
    int $imgIdx,
    int $maxUncompressed,
    bool $needFullGd,
    array &$missingExternal
): ?array {
    static $swatchCache = [];
    static $swatchOrder = [];

    if (!isset($parsed['images'][$imgIdx])) {
        return null;
    }
    $meta = $parsed['images'][$imgIdx];
    $w = (int)$meta['width'];
    $h = (int)$meta['height'];
    $name = (string)$meta['name'];
    $sCacheKey = ($parsed['sha256'] ?? '') . ':' . $imgIdx . ':' . $name . ':' . $w . 'x' . $h;

    if (!empty($meta['external'])) {
        if (!$needFullGd && isset($swatchCache[$sCacheKey])) {
            return ['gd' => null, 'raw' => null, 'w' => $swatchCache[$sCacheKey]['w'], 'h' => $swatchCache[$sCacheKey]['h'], 'swatches' => $swatchCache[$sCacheKey]['swatches']];
        }
        $png = ai_map_load_mapres_png($name);
        if ($png === null) {
            $missingExternal[] = $name;
            return null;
        }
        $gd = @imagecreatefromstring($png);
        if (!$gd) {
            $missingExternal[] = $name;
            return null;
        }
        imagealphablending($gd, false);
        imagesavealpha($gd, true);
        $iw = imagesx($gd);
        $ih = imagesy($gd);
        if (isset($swatchCache[$sCacheKey])) {
            $swatches = $swatchCache[$sCacheKey]['swatches'];
        } else {
            $swatches = ai_map_compute_swatches_from_gd($gd, $iw, $ih);
            $swatchCache[$sCacheKey] = ['w' => $iw, 'h' => $ih, 'swatches' => $swatches];
            $swatchOrder[] = $sCacheKey;
            if (count($swatchOrder) > 64) {
                $evict = array_shift($swatchOrder);
                if ($evict !== null && $evict !== $sCacheKey) {
                    unset($swatchCache[$evict]);
                }
            }
        }
        if (!$needFullGd) {
            imagedestroy($gd);
            $gd = null;
        }
        return ['gd' => $gd, 'raw' => null, 'w' => $iw, 'h' => $ih, 'swatches' => $swatches];
    }

    // Embedded raw 32-bit RGBA data block (w * h * 4 bytes).
    $dataIdx = (int)$meta['data_index'];
    if ($w < 16 || $h < 16 || $w > 2048 || $h > 2048 || $dataIdx < 0) {
        return null;
    }
    // Fast swatch-only cache hit (overview mode): avoids decompressing multi-MB RGBA blocks on repeat renders!
    if (!$needFullGd && isset($swatchCache[$sCacheKey])) {
        return ['gd' => null, 'raw' => null, 'w' => $w, 'h' => $h, 'swatches' => $swatchCache[$sCacheKey]['swatches']];
    }

    $raw = ddnet_read_data_block($bytes, $parsed['tables'], $dataIdx, $maxUncompressed);
    if (!is_string($raw) || strlen($raw) < $w * $h * 4) {
        return null;
    }

    if (isset($swatchCache[$sCacheKey])) {
        $swatches = $swatchCache[$sCacheKey]['swatches'];
    } else {
        $swatches = ai_map_compute_swatches_from_rgba($raw, $w, $h);
        $swatchCache[$sCacheKey] = ['w' => $w, 'h' => $h, 'swatches' => $swatches];
        $swatchOrder[] = $sCacheKey;
        if (count($swatchOrder) > 64) {
            $evict = array_shift($swatchOrder);
            if ($evict !== null && $evict !== $sCacheKey) {
                unset($swatchCache[$evict]);
            }
        }
    }

    // Keep raw RGBA for lazy per-tile extraction in zoomed views (avoids encoding/decoding whole 4MB sheets
    // when a viewport uses only a handful of tiles); full GD sheet is promoted lazily only if needed.
    return ['gd' => null, 'raw' => $needFullGd ? $raw : null, 'w' => $w, 'h' => $h, 'swatches' => $swatches, 'raw_extractions' => 0];
}

/** Build 256 per-tile average RGBA swatches from a raw 16x16 tileset RGBA buffer (fast 4x4 grid sample per cell). */
function ai_map_compute_swatches_from_rgba(string $raw, int $w, int $h): array
{
    $cw = max(1, intdiv($w, 16));
    $ch = max(1, intdiv($h, 16));
    $swatches = [];
    $stepX = max(1, intdiv($cw, 4));
    $stepY = max(1, intdiv($ch, 4));

    for ($tid = 1; $tid < 256; $tid++) {
        $col = $tid & 15;
        $row = $tid >> 4;
        $x0 = $col * $cw;
        $y0 = $row * $ch;
        $rSum = 0;
        $gSum = 0;
        $bSum = 0;
        $aSum = 0;
        $wSum = 0;
        $samples = 0;
        for ($sy = $y0 + intdiv($stepY, 2); $sy < $y0 + $ch; $sy += $stepY) {
            $rowOff = $sy * $w * 4;
            for ($sx = $x0 + intdiv($stepX, 2); $sx < $x0 + $cw; $sx += $stepX) {
                $p = $rowOff + $sx * 4;
                $a = ord($raw[$p + 3]);
                $samples++;
                $aSum += $a;
                if ($a > 8) {
                    $rSum += ord($raw[$p]) * $a;
                    $gSum += ord($raw[$p + 1]) * $a;
                    $bSum += ord($raw[$p + 2]) * $a;
                    $wSum += $a;
                }
            }
        }
        if ($wSum > 0 && $samples > 0) {
            $swatches[$tid] = [
                intdiv($rSum, $wSum),
                intdiv($gSum, $wSum),
                intdiv($bSum, $wSum),
                intdiv($aSum, $samples),
            ];
        } else {
            $swatches[$tid] = [0, 0, 0, 0];
        }
    }
    return $swatches;
}

/** Build 256 per-tile average RGBA swatches from a GD tileset image. */
function ai_map_compute_swatches_from_gd(\GdImage $gd, int $w, int $h): array
{
    $cw = max(1, intdiv($w, 16));
    $ch = max(1, intdiv($h, 16));
    $swatches = [];
    $stepX = max(1, intdiv($cw, 4));
    $stepY = max(1, intdiv($ch, 4));

    for ($tid = 1; $tid < 256; $tid++) {
        $col = $tid & 15;
        $row = $tid >> 4;
        $x0 = $col * $cw;
        $y0 = $row * $ch;
        $rSum = 0;
        $gSum = 0;
        $bSum = 0;
        $aSum = 0;
        $wSum = 0;
        $samples = 0;
        for ($sy = $y0 + intdiv($stepY, 2); $sy < $y0 + $ch; $sy += $stepY) {
            for ($sx = $x0 + intdiv($stepX, 2); $sx < $x0 + $cw; $sx += $stepX) {
                $c = imagecolorat($gd, $sx, $sy);
                $gdA = ($c >> 24) & 0x7f; // GD alpha: 0=opaque, 127=transparent
                $a = (int)round((127 - $gdA) * 255.0 / 127.0);
                $samples++;
                $aSum += $a;
                if ($a > 8) {
                    $rSum += (($c >> 16) & 0xff) * $a;
                    $gSum += (($c >> 8) & 0xff) * $a;
                    $bSum += ($c & 0xff) * $a;
                    $wSum += $a;
                }
            }
        }
        if ($wSum > 0 && $samples > 0) {
            $swatches[$tid] = [
                intdiv($rSum, $wSum),
                intdiv($gSum, $wSum),
                intdiv($bSum, $wSum),
                intdiv($aSum, $samples),
            ];
        } else {
            $swatches[$tid] = [0, 0, 0, 0];
        }
    }
    return $swatches;
}

/** Convert raw 32-bit RGBA bytes into a GD truecolor alpha image (fast via uncompressed stored PNG decode in C!). */
function ai_map_rgba_to_gd(string $raw, int $w, int $h): ?\GdImage
{
    // Level 0 (stored deflate blocks) spends ~0ms on compression while libpng/GD decodes in native C speed!
    $png = ai_map_encode_rgba_png($raw, $w, $h, 0);
    $gd = @imagecreatefromstring($png);
    if (!$gd) {
        return null;
    }
    imagealphablending($gd, false);
    imagesavealpha($gd, true);
    return $gd;
}

/** Extract a single tile directly from a raw embedded 16x16 RGBA tileset without converting the entire sheet to GD. */
function ai_map_extract_tile_sprite_from_rgba(string $raw, int $sw, int $sh, int $tileId, int $flags, int $dstSize): ?\GdImage
{
    $cw = max(1, intdiv($sw, 16));
    $ch = max(1, intdiv($sh, 16));
    $col = $tileId & 15;
    $row = $tileId >> 4;
    $x0 = $col * $cw;
    $y0 = $row * $ch;
    $srcRowBytes = $sw * 4;
    $cellRowBytes = $cw * 4;
    $cellRgba = '';
    for ($y = 0; $y < $ch; $y++) {
        $cellRgba .= substr($raw, ($y0 + $y) * $srcRowBytes + $x0 * 4, $cellRowBytes);
    }
    $srcCell = ai_map_rgba_to_gd($cellRgba, $cw, $ch);
    if (!$srcCell) {
        return null;
    }

    $cell = imagecreatetruecolor($dstSize, $dstSize);
    imagealphablending($cell, false);
    imagesavealpha($cell, true);
    $trans = imagecolorallocatealpha($cell, 0, 0, 0, 127);
    imagefilledrectangle($cell, 0, 0, $dstSize - 1, $dstSize - 1, $trans);
    imagecopyresampled($cell, $srcCell, 0, 0, 0, 0, $dstSize, $dstSize, $cw, $ch);
    imagedestroy($srcCell);

    if (($flags & 2) !== 0) {
        imageflip($cell, IMG_FLIP_VERTICAL);
    }
    if (($flags & 1) !== 0) {
        imageflip($cell, IMG_FLIP_HORIZONTAL);
    }
    if (($flags & 8) !== 0) {
        $rot = imagerotate($cell, -90, $trans);
        if ($rot instanceof \GdImage) {
            imagedestroy($cell);
            $cell = $rot;
            imagealphablending($cell, false);
            imagesavealpha($cell, true);
        }
    }
    return $cell;
}

/** Extract & cache a single tile cell from a tileset GD image with DDNet VFLIP/HFLIP/ROTATE & target scale. */
function ai_map_extract_tile_sprite(\GdImage $sheet, int $sw, int $sh, int $tileId, int $flags, int $dstSize): \GdImage
{
    $cw = max(1, intdiv($sw, 16));
    $ch = max(1, intdiv($sh, 16));
    $col = $tileId & 15;
    $row = $tileId >> 4;

    $cell = imagecreatetruecolor($dstSize, $dstSize);
    imagealphablending($cell, false);
    imagesavealpha($cell, true);
    $trans = imagecolorallocatealpha($cell, 0, 0, 0, 127);
    imagefilledrectangle($cell, 0, 0, $dstSize - 1, $dstSize - 1, $trans);
    imagecopyresampled($cell, $sheet, 0, 0, $col * $cw, $row * $ch, $dstSize, $dstSize, $cw, $ch);

    // Exact DDNet flag transform order (matches vendor/ddnet-map/twwebgl.js):
    // bit 2 (HFLIP) flips Y, bit 1 (VFLIP) flips X, bit 8 (ROTATE) rotates 90 deg CW.
    if (($flags & 2) !== 0) {
        imageflip($cell, IMG_FLIP_VERTICAL);
    }
    if (($flags & 1) !== 0) {
        imageflip($cell, IMG_FLIP_HORIZONTAL);
    }
    if (($flags & 8) !== 0) {
        $rot = imagerotate($cell, -90, $trans);
        if ($rot instanceof \GdImage) {
            imagedestroy($cell);
            $cell = $rot;
            imagealphablending($cell, false);
            imagesavealpha($cell, true);
        }
    }
    return $cell;
}

/** Primary ext-gd PNG renderer supporting visual, hybrid, and schematic modes. */
function ai_map_render_gd(array $ctx, array &$missingExternal): string
{
    $bytes = $ctx['bytes'];
    $parsed = $ctx['parsed'];
    $mode = $ctx['mode'];
    $tx0 = $ctx['tx0'];
    $ty0 = $ctx['ty0'];
    $tx1 = $ctx['tx1'];
    $ty1 = $ctx['ty1'];
    $bw = $ctx['bw'];
    $bh = $ctx['bh'];
    $tilePx = $ctx['tilePx'];
    $ox = $ctx['marginLeft'];
    $oy = $ctx['marginTop'];
    $drawW = $ctx['drawW'];
    $drawH = $ctx['drawH'];
    $imgW = $ctx['imgW'];
    $imgH = $ctx['imgH'];
    $maxUncompressed = $ctx['maxUncompressed'];

    $im = imagecreatetruecolor($imgW, $imgH);
    imagealphablending($im, true);
    imagesavealpha($im, true);

    // Outer frame background (dark slate for ruler & legend).
    $frameBg = imagecolorallocate($im, 16, 22, 34);
    imagefilledrectangle($im, 0, 0, $imgW - 1, $imgH - 1, $frameBg);

    // 1. Canvas sky / schematic backdrop.
    if ($mode === 'schematic') {
        $mapBg = imagecolorallocate($im, 20, 27, 42);
        imagefilledrectangle($im, $ox, $oy, $ox + $drawW - 1, $oy + $drawH - 1, $mapBg);
    } else {
        [$skyTop, $skyBot] = ai_map_extract_sky_gradient($bytes, $parsed, $maxUncompressed);
        for ($y = 0; $y < $drawH; $y++) {
            $t = $drawH > 1 ? ($y / ($drawH - 1)) : 0.0;
            $r = (int)round($skyTop[0] + ($skyBot[0] - $skyTop[0]) * $t);
            $g = (int)round($skyTop[1] + ($skyBot[1] - $skyTop[1]) * $t);
            $b = (int)round($skyTop[2] + ($skyBot[2] - $skyTop[2]) * $t);
            $col = imagecolorallocate($im, $r, $g, $b);
            imageline($im, $ox, $oy + $y, $ox + $drawW - 1, $oy + $y, $col);
        }
    }

    // Clip all map drawing strictly to the viewport rect so tiles/quads never bleed into rulers.
    imagesetclip($im, $ox, $oy, $ox + $drawW - 1, $oy + $drawH - 1);

    $gw = (int)$parsed['game']['width'];
    $gh = (int)$parsed['game']['height'];
    $gameTiles = $parsed['game_tiles'];
    $frontTiles = $parsed['front_tiles'];
    $hasFront = ($frontTiles !== null && strlen($frontTiles) >= $gw * $gh);

    // 2. Real visual layer rendering (for visual & hybrid modes).
    if ($mode !== 'schematic') {
        $useSprites = $tilePx >= 3.0;
        $spriteSize = max(3, (int)ceil($tilePx));
        $imageCache = []; // imgIdx => loaded entry or false
        $spriteCache = []; // "img:tid:flags" => GdImage
        $tables = $parsed['tables'];

        foreach ($parsed['layers'] as $layer) {
            $role = (string)($layer['role'] ?? '');
            if ($role !== 'visual_tiles' && $role !== 'visual_quads') {
                continue;
            }
            $paraX = (int)$layer['parallax_x'];
            $paraY = (int)$layer['parallax_y'];
            if ($paraX !== 100 || $paraY !== 100) {
                continue; // Sky already sampled; non-world parallax layers skip to keep exact tile alignment.
            }

            // World-aligned decorative quad layers.
            if ($role === 'visual_quads') {
                $numQ = (int)($layer['num_quads'] ?? 0);
                $dataIdx = (int)($layer['data_index'] ?? -1);
                if ($numQ < 1 || $dataIdx < 0) {
                    continue;
                }
                $plain = ddnet_read_data_block($bytes, $tables, $dataIdx, $maxUncompressed);
                if (!is_string($plain) || $plain === '') {
                    continue;
                }
                $quads = ddnet_parse_quads_block($plain, $numQ, 600);
                $vx0 = $tx0 * 32.0;
                $vy0 = $ty0 * 32.0;
                $vx1 = ($tx1 + 1) * 32.0;
                $vy1 = ($ty1 + 1) * 32.0;
                $offX = (float)($layer['offset_x'] ?? 0);
                $offY = (float)($layer['offset_y'] ?? 0);
                foreach ($quads as $q) {
                    if ($q['max_x'] + $offX < $vx0 || $q['min_x'] + $offX > $vx1 || $q['max_y'] + $offY < $vy0 || $q['min_y'] + $offY > $vy1) {
                        continue;
                    }
                    $c = $q['colors'];
                    $avgA = intdiv($c[0][3] + $c[1][3] + $c[2][3] + $c[3][3], 4);
                    if ($avgA < 12) {
                        continue;
                    }
                    $avgR = intdiv($c[0][0] + $c[1][0] + $c[2][0] + $c[3][0], 4);
                    $avgG = intdiv($c[0][1] + $c[1][1] + $c[2][1] + $c[3][1], 4);
                    $avgB = intdiv($c[0][2] + $c[1][2] + $c[2][2] + $c[3][2], 4);
                    // Draw textured quads subtly tinted unless solid colored.
                    $effA = ((int)$layer['image_index'] >= 0) ? intdiv($avgA, 3) : $avgA;
                    $gdAlpha = max(0, min(127, 127 - (int)round($effA * 127.0 / 255.0)));
                    $qCol = imagecolorallocatealpha($im, $avgR, $avgG, $avgB, $gdAlpha);
                    $pts = $q['points'];
                    // DDNet quad vertex order around perimeter: 0 (TL) -> 1 (TR) -> 3 (BR) -> 2 (BL).
                    $poly = [
                        (int)round($ox + (($pts[0][0] + $offX) / 32.0 - $tx0) * $tilePx),
                        (int)round($oy + (($pts[0][1] + $offY) / 32.0 - $ty0) * $tilePx),
                        (int)round($ox + (($pts[1][0] + $offX) / 32.0 - $tx0) * $tilePx),
                        (int)round($oy + (($pts[1][1] + $offY) / 32.0 - $ty0) * $tilePx),
                        (int)round($ox + (($pts[3][0] + $offX) / 32.0 - $tx0) * $tilePx),
                        (int)round($oy + (($pts[3][1] + $offY) / 32.0 - $ty0) * $tilePx),
                        (int)round($ox + (($pts[2][0] + $offX) / 32.0 - $tx0) * $tilePx),
                        (int)round($oy + (($pts[2][1] + $offY) / 32.0 - $ty0) * $tilePx),
                    ];
                    imagefilledpolygon($im, $poly, $qCol);
                }
                continue;
            }

            // World-aligned visual tile layers.
            $lw = (int)$layer['width'];
            $lh = (int)$layer['height'];
            $dataIdx = (int)$layer['data_index'];
            if ($lw < 1 || $lh < 1 || $dataIdx < 0) {
                continue;
            }
            $rgba = $layer['rgba'] ?? [255, 255, 255, 255];
            if ((int)$rgba[3] < 8) {
                continue;
            }
            $offTx = (int)round(((int)($layer['offset_x'] ?? 0)) / 32.0);
            $offTy = (int)round(((int)($layer['offset_y'] ?? 0)) / 32.0);
            $ltx0 = max(0, $tx0 - $offTx);
            $lty0 = max(0, $ty0 - $offTy);
            $ltx1 = min($lw - 1, $tx1 - $offTx);
            $lty1 = min($lh - 1, $ty1 - $offTy);
            if ($ltx0 > $ltx1 || $lty0 > $lty1) {
                continue;
            }

            $plain = ddnet_read_data_block($bytes, $tables, $dataIdx, $maxUncompressed);
            if (!is_string($plain) || $plain === '') {
                continue;
            }

            $tileVer = (int)$layer['tile_version'];
            // Zero-allocation pre-check: skip decoding tileset if layer is empty inside viewport.
            if (!ddnet_tile_layer_has_nonzero_in_rect($plain, $lw, $lh, $tileVer, $ltx0, $lty0, $ltx1, $lty1)) {
                continue;
            }

            $imgIdx = (int)($layer['image_index'] ?? -1);
            $imgData = null;
            if ($imgIdx >= 0) {
                if (!array_key_exists($imgIdx, $imageCache)) {
                    $imageCache[$imgIdx] = ai_map_load_image_gd($bytes, $parsed, $imgIdx, $maxUncompressed, $useSprites, $missingExternal) ?? false;
                }
                if ($imageCache[$imgIdx] !== false) {
                    $imgData = &$imageCache[$imgIdx];
                }
            }

            $cls = ai_map_classify_visual_layer($layer);
            $fallbackRgb = ai_map_category_fallback_rgb($cls['category']);
            $layerA = (int)$rgba[3];
            $layerR = (int)$rgba[0];
            $layerG = (int)$rgba[1];
            $layerB = (int)$rgba[2];
            $stepCeil = max(1, (int)ceil($tilePx));
            $canDrawSprites = $useSprites && $imgData !== null && ($imgData['gd'] instanceof \GdImage || is_string($imgData['raw'] ?? null));

            if ($canDrawSprites) {
                ddnet_iter_tile_layer_nonzero($plain, $lw, $lh, $tileVer, static function (int $lx, int $ly, int $tid, int $fl) use (
                    $im, $ox, $oy, $offTx, $offTy, $tx0, $ty0, $tilePx, $imgIdx, $spriteSize, &$imgData, &$spriteCache
                ): void {
                    $px = (int)floor($ox + ($lx + $offTx - $tx0) * $tilePx);
                    $py = (int)floor($oy + ($ly + $offTy - $ty0) * $tilePx);
                    $sKey = $imgIdx . ':' . $tid . ':' . ($fl & 11);
                    if (!isset($spriteCache[$sKey])) {
                        if (count($spriteCache) >= 1200) {
                            $firstKey = array_key_first($spriteCache);
                            if ($firstKey !== null) {
                                imagedestroy($spriteCache[$firstKey]);
                                unset($spriteCache[$firstKey]);
                            }
                        }
                        if ($imgData['gd'] instanceof \GdImage) {
                            $spriteCache[$sKey] = ai_map_extract_tile_sprite($imgData['gd'], $imgData['w'], $imgData['h'], $tid, $fl, $spriteSize);
                        } else {
                            // Promote whole embedded RGBA sheet to GD only when >80 unique tiles are needed from it.
                            $imgData['raw_extractions'] = (int)($imgData['raw_extractions'] ?? 0) + 1;
                            if ($imgData['raw_extractions'] > 80) {
                                $sheetGd = ai_map_rgba_to_gd((string)$imgData['raw'], $imgData['w'], $imgData['h']);
                                if ($sheetGd instanceof \GdImage) {
                                    $imgData['gd'] = $sheetGd;
                                    $imgData['raw'] = null;
                                    $spriteCache[$sKey] = ai_map_extract_tile_sprite($sheetGd, $imgData['w'], $imgData['h'], $tid, $fl, $spriteSize);
                                }
                            }
                            if (!isset($spriteCache[$sKey]) && is_string($imgData['raw'] ?? null)) {
                                $sp = ai_map_extract_tile_sprite_from_rgba($imgData['raw'], $imgData['w'], $imgData['h'], $tid, $fl, $spriteSize);
                                if ($sp instanceof \GdImage) {
                                    $spriteCache[$sKey] = $sp;
                                }
                            }
                        }
                    }
                    if (isset($spriteCache[$sKey])) {
                        imagecopy($im, $spriteCache[$sKey], $px, $py, 0, 0, $spriteSize, $spriteSize);
                    }
                }, $ltx0, $lty0, $ltx1, $lty1);
            } else {
                // Precompute up to 256 GD colors once per layer instead of calling imagecolorallocatealpha per tile!
                $defaultCol = null;
                if ($imgIdx < 0) {
                    $gdA = max(0, min(127, 127 - (int)round($layerA * 127.0 / 255.0)));
                    $defaultCol = imagecolorallocatealpha($im, $layerR, $layerG, $layerB, $gdA);
                } else {
                    $fr = intdiv($fallbackRgb[0] * $layerR, 255);
                    $fg = intdiv($fallbackRgb[1] * $layerG, 255);
                    $fb = intdiv($fallbackRgb[2] * $layerB, 255);
                    $fa = min(180, $layerA);
                    $gdA = max(0, min(127, 127 - (int)round($fa * 127.0 / 255.0)));
                    $defaultCol = imagecolorallocatealpha($im, $fr, $fg, $fb, $gdA);
                }
                $swatches = $imgData['swatches'] ?? null;
                $colorLut = [];
                if (is_array($swatches)) {
                    foreach ($swatches as $tidKey => $sw) {
                        if ($sw[3] < 10) {
                            $colorLut[$tidKey] = -1; // transparent skip
                            continue;
                        }
                        $r = intdiv($sw[0] * $layerR, 255);
                        $g = intdiv($sw[1] * $layerG, 255);
                        $b = intdiv($sw[2] * $layerB, 255);
                        $a = intdiv($sw[3] * $layerA, 255);
                        $gdA = max(0, min(127, 127 - (int)round($a * 127.0 / 255.0)));
                        $colorLut[$tidKey] = imagecolorallocatealpha($im, $r, $g, $b, $gdA);
                    }
                }

                // Stream directly without allocating $tilesInView[] arrays.
                $baseX = $ox + ($offTx - $tx0) * $tilePx;
                $baseY = $oy + ($offTy - $ty0) * $tilePx;
                $lastPx = -999999;
                $lastPy = -999999;
                $lastCol = -2;
                $len = strlen($plain);
                $count = $lw * $lh;

                if ($tileVer >= 4) {
                    $written = 0;
                    $minIdx = $lty0 * $lw + $ltx0;
                    $maxIdx = $lty1 * $lw + $ltx1;
                    for ($off = 0; $off + 4 <= $len && $written < $count; $off += 4) {
                        $repeat = min(ord($plain[$off + 2]) + 1, $count - $written);
                        if ($plain[$off] === "\x00" || $written + $repeat - 1 < $minIdx) {
                            $written += $repeat;
                            continue;
                        }
                        if ($written > $maxIdx) {
                            break;
                        }
                        $tid = ord($plain[$off]);
                        $col = $colorLut[$tid] ?? $defaultCol;
                        if ($col !== -1) {
                            $start = max($written, $minIdx);
                            $end = min($written + $repeat - 1, $maxIdx);
                            for ($cell = $start; $cell <= $end; $cell++) {
                                $lx = $cell % $lw;
                                if ($lx < $ltx0 || $lx > $ltx1) {
                                    continue;
                                }
                                $ly = intdiv($cell, $lw);
                                $px = (int)floor($baseX + $lx * $tilePx);
                                $py = (int)floor($baseY + $ly * $tilePx);
                                if ($stepCeil <= 1) {
                                    if ($px === $lastPx && $py === $lastPy && $col === $lastCol) {
                                        continue;
                                    }
                                    imagesetpixel($im, $px, $py, $col);
                                    $lastPx = $px;
                                    $lastPy = $py;
                                    $lastCol = $col;
                                } else {
                                    imagefilledrectangle($im, $px, $py, $px + $stepCeil - 1, $py + $stepCeil - 1, $col);
                                }
                            }
                        }
                        $written += $repeat;
                    }
                } else {
                    $rowsAvail = min($lh, intdiv($len, $lw * 4));
                    $ryEnd = min($lty1, $rowsAvail - 1);
                    $rowBytes = $lw * 4;
                    for ($ly = $lty0; $ly <= $ryEnd; $ly++) {
                        $py = (int)floor($baseY + $ly * $tilePx);
                        $rowBase = $ly * $rowBytes;
                        $pos = $rowBase + $ltx0 * 4;
                        $rowEnd = $rowBase + ($ltx1 + 1) * 4;
                        while ($pos < $rowEnd) {
                            $z = strspn($plain, "\x00", $pos, $rowEnd - $pos);
                            $pos += ($z & ~3);
                            if ($pos >= $rowEnd) {
                                break;
                            }
                            $tb = $plain[$pos];
                            if ($tb !== "\x00") {
                                $tid = ord($tb);
                                $col = $colorLut[$tid] ?? $defaultCol;
                                if ($col !== -1) {
                                    $lx = ($pos - $rowBase) >> 2;
                                    $px = (int)floor($baseX + $lx * $tilePx);
                                    if ($stepCeil <= 1) {
                                        if ($px !== $lastPx || $py !== $lastPy || $col !== $lastCol) {
                                            imagesetpixel($im, $px, $py, $col);
                                            $lastPx = $px;
                                            $lastPy = $py;
                                            $lastCol = $col;
                                        }
                                    } else {
                                        imagefilledrectangle($im, $px, $py, $px + $stepCeil - 1, $py + $stepCeil - 1, $col);
                                    }
                                }
                            }
                            $pos += 4;
                        }
                    }
                }
            }
            unset($imgData);
        }

        // Release temporary GD tile sheet and sprite memory.
        foreach ($spriteCache as $sp) {
            imagedestroy($sp);
        }
        foreach ($imageCache as $entry) {
            if (is_array($entry) && ($entry['gd'] ?? null) instanceof \GdImage) {
                imagedestroy($entry['gd']);
            }
        }
    }

    // 3. Schematic / Collision layer overlay (direct byte LUT + fast zero-run skipping).
    if ($ctx['showCollision'] || $mode === 'schematic') {
        $colSolid = imagecolorallocatealpha($im, 148, 163, 184, $mode === 'schematic' ? 10 : 72);
        $colNohook = imagecolorallocatealpha($im, 194, 154, 108, $mode === 'schematic' ? 10 : 72);
        $colDeath = imagecolorallocatealpha($im, 239, 68, 68, $mode === 'schematic' ? 15 : 55);
        $colWater = imagecolorallocatealpha($im, 56, 189, 248, $mode === 'schematic' ? 35 : 70);
        $colHouse = imagecolorallocatealpha($im, 16, 185, 129, 55);
        $colGuild = imagecolorallocatealpha($im, 245, 158, 11, 55);
        $colService = imagecolorallocatealpha($im, 6, 182, 212, 55);
        $colSpawn = imagecolorallocatealpha($im, 34, 197, 94, 15);
        $stepCeil = max(1, (int)ceil($tilePx));

        // Priority order matches original: death(2) > solid(1) > nohook(3) > water(6) > guild(16) > house(8) > service(17,21,29,30,38) > spawn(208).
        $prioMap = [
            "\x02" => 8, "\x01" => 7, "\x03" => 6, "\x06" => 5,
            "\x10" => 4, "\x08" => 3,
            "\x11" => 2, "\x15" => 2, "\x1d" => 2, "\x1e" => 2, "\x26" => 2,
            "\xd0" => 1,
        ];
        $prioCol = [
            8 => $colDeath, 7 => $colSolid, 6 => $colNohook, 5 => $colWater,
            4 => $colGuild, 3 => $colHouse, 2 => $colService, 1 => $colSpawn,
        ];

        $lastPx = -999999;
        $lastPy = -999999;
        $lastCol = -2;
        for ($ty = $ty0; $ty <= $ty1; $ty++) {
            $py = (int)floor($oy + ($ty - $ty0) * $tilePx);
            $rowOff = $ty * $gw;
            $tx = $tx0;
            while ($tx <= $tx1) {
                $cell = $rowOff + $tx;
                $gb = $gameTiles[$cell];
                if (!$hasFront && $gb === "\x00") {
                    $tx += max(1, strspn($gameTiles, "\x00", $cell, $tx1 - $tx + 1));
                    continue;
                }
                $fb = $hasFront ? $frontTiles[$cell] : "\x00";
                if ($gb === "\x00" && $fb === "\x00") {
                    $tx++;
                    continue;
                }
                $pg = $prioMap[$gb] ?? 0;
                $pf = ($fb !== "\x00" && $fb !== "\xd0") ? ($prioMap[$fb] ?? 0) : 0;
                $best = $pg >= $pf ? $pg : $pf;
                if ($best > 0) {
                    $c = $prioCol[$best];
                    $px = (int)floor($ox + ($tx - $tx0) * $tilePx);
                    if ($stepCeil <= 1) {
                        if ($px !== $lastPx || $py !== $lastPy || $c !== $lastCol) {
                            imagesetpixel($im, $px, $py, $c);
                            $lastPx = $px;
                            $lastPy = $py;
                            $lastCol = $c;
                        }
                    } else {
                        imagefilledrectangle($im, $px, $py, $px + $stepCeil - 1, $py + $stepCeil - 1, $c);
                    }
                }
                $tx++;
            }
        }
    }

    // 4. Semantic structure bounding boxes & labels (hybrid & schematic).
    $placedLabels = [];
    if ($ctx['showStructures']) {
        foreach (array_slice($ctx['visibleStructures'], 0, 50) as $st) {
            $b = $st['bbox_tiles'];
            $x0 = (int)floor($ox + ($b['tx0'] - $tx0) * $tilePx);
            $y0 = (int)floor($oy + ($b['ty0'] - $ty0) * $tilePx);
            $x1 = (int)ceil($ox + ($b['tx1'] - $tx0 + 1) * $tilePx);
            $y1 = (int)ceil($oy + ($b['ty1'] - $ty0 + 1) * $tilePx);
            [$r, $g, $bl] = ai_map_structure_color((string)$st['kind']);
            $fill = imagecolorallocatealpha($im, $r, $g, $bl, $mode === 'schematic' ? 98 : 112);
            $border = imagecolorallocatealpha($im, $r, $g, $bl, 12);
            imagefilledrectangle($im, $x0, $y0, $x1, $y1, $fill);
            imagerectangle($im, $x0, $y0, $x1, $y1, $border);
            if ($x1 - $x0 > 4 && $y1 - $y0 > 4) {
                imagerectangle($im, $x0 + 1, $y0 + 1, $x1 - 1, $y1 - 1, $border);
            }
            if ($ctx['showLabels'] && ($x1 - $x0) >= 14 && ($y1 - $y0) >= 10) {
                $lbl = (string)$st['name'];
                if (strlen($lbl) > 26) {
                    $lbl = substr($lbl, 0, 24) . '..';
                }
                $lx = max($ox + 2, min($ox + $drawW - 80, $x0 + 3));
                $ly = max($oy + 2, min($oy + $drawH - 14, $y0 + 3));
                ai_map_gd_draw_pill_label($im, $lx, $ly, $lbl, [$r, $g, $bl], $placedLabels);
            }
        }
    }

    // 5. Spawn & DB object markers (hybrid & schematic).
    if ($ctx['showObjects']) {
        $spCol = imagecolorallocate($im, 34, 197, 94);
        $darkCol = imagecolorallocate($im, 10, 14, 22);
        foreach ($ctx['analysis']['game_layer']['spawn_points'] ?? [] as $sp) {
            if ($sp['tile_x'] >= $tx0 && $sp['tile_x'] <= $tx1 && $sp['tile_y'] >= $ty0 && $sp['tile_y'] <= $ty1) {
                $sx = (int)round($ox + ($sp['tile_x'] - $tx0 + 0.5) * $tilePx);
                $sy = (int)round($oy + ($sp['tile_y'] - $ty0 + 0.5) * $tilePx);
                imagefilledellipse($im, $sx, $sy, 11, 11, $darkCol);
                imagefilledellipse($im, $sx, $sy, 7, 7, $spCol);
                if ($ctx['showLabels']) {
                    ai_map_gd_draw_pill_label($im, $sx + 6, $sy - 6, 'SPAWN', [34, 197, 94], $placedLabels);
                }
            }
        }
        foreach (array_slice($ctx['visibleObjects'], 0, 45) as $obj) {
            $mx = (int)round($ox + ($obj['tile_x'] - $tx0 + 0.5) * $tilePx);
            $my = (int)round($oy + ($obj['tile_y'] - $ty0 + 0.5) * $tilePx);
            [$r, $g, $b] = $obj['rgb'];
            $mc = imagecolorallocate($im, $r, $g, $b);
            imagefilledellipse($im, $mx, $my, 10, 10, $darkCol);
            imagefilledellipse($im, $mx, $my, 6, 6, $mc);
            if ($ctx['showLabels'] && $tilePx >= 4.0) {
                $short = $obj['tag'] . ':' . (strlen($obj['name']) > 16 ? substr($obj['name'], 0, 14) . '..' : $obj['name']);
                ai_map_gd_draw_pill_label($im, $mx + 5, $my - 6, $short, [$r, $g, $b], $placedLabels);
            }
        }
    }

    // 6. Optional highlighted coordinate crosshair.
    if ($ctx['highlightPt'] !== null) {
        $hx = (int)round($ox + ($ctx['highlightPt']['x'] / 32.0 - $tx0) * $tilePx);
        $hy = (int)round($oy + ($ctx['highlightPt']['y'] / 32.0 - $ty0) * $tilePx);
        $red = imagecolorallocate($im, 255, 59, 48);
        $white = imagecolorallocate($im, 255, 255, 255);
        imageline($im, $hx - 12, $hy, $hx + 12, $hy, $white);
        imageline($im, $hx, $hy - 12, $hx, $hy + 12, $white);
        imageellipse($im, $hx, $hy, 14, 14, $red);
        imageellipse($im, $hx, $hy, 12, 12, $red);
    }

    // Unclip for coordinate ruler & bottom legend bar.
    imagesetclip($im, 0, 0, $imgW - 1, $imgH - 1);

    if ($ctx['showRuler']) {
        $borderCol = imagecolorallocate($im, 71, 85, 105);
        $tickCol = imagecolorallocate($im, 148, 163, 184);
        $textCol = imagecolorallocate($im, 226, 232, 240);
        imagerectangle($im, $ox, $oy, $ox + $drawW - 1, $oy + $drawH - 1, $borderCol);

        // Choose clean tile step for ruler ticks (1, 5, 10, 20, 50, 100).
        $stepChoices = [1, 2, 5, 10, 20, 25, 50, 100, 200];
        $tickStep = 100;
        foreach ($stepChoices as $st) {
            if ($st * $tilePx >= 52) {
                $tickStep = $st;
                break;
            }
        }
        $firstTx = (int)(ceil($tx0 / $tickStep) * $tickStep);
        for ($tx = $firstTx; $tx <= $tx1; $tx += $tickStep) {
            $px = (int)round($ox + ($tx - $tx0) * $tilePx);
            imageline($im, $px, $oy - 5, $px, $oy, $tickCol);
            imagestring($im, 1, max($ox, min($imgW - 28, $px - 8)), 6, (string)$tx, $textCol);
        }
        $firstTy = (int)(ceil($ty0 / $tickStep) * $tickStep);
        for ($ty = $firstTy; $ty <= $ty1; $ty += $tickStep) {
            $py = (int)round($oy + ($ty - $ty0) * $tilePx);
            imageline($im, $ox - 5, $py, $ox, $py, $tickCol);
            imagestring($im, 1, 4, max($oy, min($oy + $drawH - 10, $py - 4)), (string)$ty, $textCol);
        }

        // Bottom status & legend bar.
        $barY = $oy + $drawH + 7;
        $infoStr = sprintf(
            '%s | tiles (%d,%d)..(%d,%d) | px (%d,%d)..(%d,%d) | mode=%s',
            ai_map_ascii_safe((string)$ctx['analysis']['overview_text'] !== '' ? ($ctx['visibleStructures'] !== [] ? count($ctx['visibleStructures']) . ' structs' : 'terrain') : 'map'),
            $tx0,
            $ty0,
            $tx1,
            $ty1,
            $tx0 * 32,
            $ty0 * 32,
            ($tx1 + 1) * 32,
            ($ty1 + 1) * 32,
            $mode
        );
        imagestring($im, 2, 6, $barY, substr($infoStr, 0, max(20, intdiv($imgW - 12, 6))), $textCol);
    }

    ob_start();
    imagepng($im, null, 6);
    $pngBytes = (string)ob_get_clean();
    imagedestroy($im);
    return $pngBytes;
}

/** Draw a non-overlapping crisp dark pill label using GD built-in bitmap font (ASCII transliterated safely). */
function ai_map_gd_draw_pill_label(\GdImage $im, int $x, int $y, string $text, array $accentRgb, array &$placed): void
{
    $ascii = ai_map_ascii_safe($text);
    if ($ascii === '') {
        return;
    }
    $w = strlen($ascii) * 6 + 6;
    $h = 11;
    foreach ($placed as [$px0, $py0, $px1, $py1]) {
        if ($x < $px1 + 2 && $x + $w + 2 > $px0 && $y < $py1 + 2 && $y + $h + 2 > $py0) {
            return; // Avoid overlapping labels cluttering the screenshot.
        }
    }
    $placed[] = [$x, $y, $x + $w, $y + $h];
    $bg = imagecolorallocatealpha($im, 12, 16, 26, 22);
    $bd = imagecolorallocate($im, $accentRgb[0], $accentRgb[1], $accentRgb[2]);
    $fg = imagecolorallocate($im, 248, 250, 252);
    imagefilledrectangle($im, $x, $y, $x + $w, $y + $h, $bg);
    imagerectangle($im, $x, $y, $x + $w, $y + $h, $bd);
    imagestring($im, 2, $x + 3, $y - 1, $ascii, $fg);
}

/** Transliterate Cyrillic/Unicode labels to clean ASCII for GD built-in bitmap font legibility. */
function ai_map_ascii_safe(string $text): string
{
    static $map = [
        'А' => 'A', 'Б' => 'B', 'В' => 'V', 'Г' => 'G', 'Д' => 'D', 'Е' => 'E', 'Ё' => 'E', 'Ж' => 'Zh',
        'З' => 'Z', 'И' => 'I', 'Й' => 'Y', 'К' => 'K', 'Л' => 'L', 'М' => 'M', 'Н' => 'N', 'О' => 'O',
        'П' => 'P', 'Р' => 'R', 'С' => 'S', 'Т' => 'T', 'У' => 'U', 'Ф' => 'F', 'Х' => 'Kh', 'Ц' => 'Ts',
        'Ч' => 'Ch', 'Ш' => 'Sh', 'Щ' => 'Shch', 'Ъ' => '', 'Ы' => 'Y', 'Ь' => '', 'Э' => 'E', 'Ю' => 'Yu', 'Я' => 'Ya',
        'а' => 'a', 'б' => 'b', 'в' => 'v', 'г' => 'g', 'д' => 'd', 'е' => 'e', 'ё' => 'e', 'ж' => 'zh',
        'з' => 'z', 'и' => 'i', 'й' => 'y', 'к' => 'k', 'л' => 'l', 'м' => 'm', 'н' => 'n', 'о' => 'o',
        'п' => 'p', 'р' => 'r', 'с' => 's', 'т' => 't', 'у' => 'u', 'ф' => 'f', 'х' => 'kh', 'ц' => 'ts',
        'ч' => 'ch', 'ш' => 'sh', 'щ' => 'shch', 'ъ' => '', 'ы' => 'y', 'ь' => '', 'э' => 'e', 'ю' => 'yu', 'я' => 'ya',
    ];
    $s = strtr($text, $map);
    $s = preg_replace('/[^\x20-\x7E]/', '', $s);
    return is_string($s) ? trim($s) : '';
}

/**
 * Pure-PHP PNG encoder for a packed raw RGBA byte buffer (4 bytes/pixel, top-down).
 * Used both to fast-import embedded DDNet RGBA tilesets into GD and as the no-GD fallback encoder.
 */
function ai_map_encode_rgba_png(string $rgba, int $w, int $h, int $zlibLevel = 6): string
{
    $rowBytes = $w * 4;
    $scanlines = '';
    for ($y = 0; $y < $h; $y++) {
        $scanlines .= "\x00" . substr($rgba, $y * $rowBytes, $rowBytes);
    }
    $idat = gzcompress($scanlines, $zlibLevel);
    if (!is_string($idat)) {
        $idat = '';
    }

    $chunk = static function (string $type, string $data): string {
        $crc = crc32($type . $data);
        return pack('N', strlen($data)) . $type . $data . pack('N', $crc);
    };

    $ihdr = pack('NNCCCCC', $w, $h, 8, 6, 0, 0, 0); // 8-bit RGBA
    return "\x89PNG\r\n\x1a\n"
        . $chunk('IHDR', $ihdr)
        . $chunk('IDAT', $idat)
        . $chunk('IEND', '');
}

/**
 * Pure-PHP RGBA framebuffer PNG renderer fallback when ext-gd is not installed.
 * Renders sky/schematic backdrop, collision/water/building tiles, structure boxes,
 * spawn/object markers, and coordinate border without any external extension beyond zlib.
 */
function ai_map_render_pure_php(array $ctx): string
{
    $parsed = $ctx['parsed'];
    $mode = $ctx['mode'];
    $tx0 = $ctx['tx0'];
    $ty0 = $ctx['ty0'];
    $tx1 = $ctx['tx1'];
    $ty1 = $ctx['ty1'];
    $tilePx = $ctx['tilePx'];
    $ox = $ctx['marginLeft'];
    $oy = $ctx['marginTop'];
    $drawW = $ctx['drawW'];
    $drawH = $ctx['drawH'];
    $imgW = $ctx['imgW'];
    $imgH = $ctx['imgH'];

    // Cap fallback dimensions to keep pure-PHP framebuffer rendering fast & memory-light.
    if ($imgW > 800 || $imgH > 600) {
        $scale = min(800 / max(1, $imgW), 600 / max(1, $imgH));
        $ox = (int)round($ox * $scale);
        $oy = (int)round($oy * $scale);
        $drawW = max(64, (int)round($drawW * $scale));
        $drawH = max(64, (int)round($drawH * $scale));
        $tilePx *= $scale;
        $imgW = max($ox + $drawW, (int)round($imgW * $scale));
        $imgH = max($oy + $drawH, (int)round($imgH * $scale));
    }

    // Store framebuffer as per-row RGBA strings (2.5KB/row instead of 1MB monolithic string)
    // so pixel & rectangle writes mutate small rows in-place instead of copying 1MB per tile!
    $bgRow = str_repeat(pack('CCCC', 18, 24, 38, 255), $imgW);
    $rows = array_fill(0, $imgH, $bgRow);

    $fillRect = static function (int $x0, int $y0, int $x1, int $y1, int $r, int $g, int $b, int $a = 255) use (&$rows, $imgW, $imgH): void {
        $x0 = max(0, $x0);
        $y0 = max(0, $y0);
        $x1 = min($imgW - 1, $x1);
        $y1 = min($imgH - 1, $y1);
        if ($x0 > $x1 || $y0 > $y1) {
            return;
        }
        $rc = chr($r);
        $gc = chr($g);
        $bc = chr($b);
        $ac = chr($a);
        if ($x0 === $x1) {
            $off = $x0 * 4;
            for ($y = $y0; $y <= $y1; $y++) {
                $rows[$y][$off] = $rc;
                $rows[$y][$off + 1] = $gc;
                $rows[$y][$off + 2] = $bc;
                $rows[$y][$off + 3] = $ac;
            }
            return;
        }
        $w = $x1 - $x0 + 1;
        $run = str_repeat($rc . $gc . $bc . $ac, $w);
        $off = $x0 * 4;
        $bytes = $w * 4;
        for ($y = $y0; $y <= $y1; $y++) {
            $rows[$y] = substr_replace($rows[$y], $run, $off, $bytes);
        }
    };

    $drawRect = static function (int $x0, int $y0, int $x1, int $y1, int $r, int $g, int $b) use ($fillRect): void {
        $fillRect($x0, $y0, $x1, $y0, $r, $g, $b);
        $fillRect($x0, $y1, $x1, $y1, $r, $g, $b);
        $fillRect($x0, $y0, $x0, $y1, $r, $g, $b);
        $fillRect($x1, $y0, $x1, $y1, $r, $g, $b);
    };

    // Map viewport background.
    if ($mode === 'schematic') {
        $fillRect($ox, $oy, $ox + $drawW - 1, $oy + $drawH - 1, 22, 30, 46);
    } else {
        $fillRect($ox, $oy, $ox + $drawW - 1, $oy + $drawH - 1, 118, 168, 220);
    }

    // Draw gameplay/collision tiles using direct byte LUT + in-place pixel writes.
    $gw = (int)$parsed['game']['width'];
    $gh = (int)$parsed['game']['height'];
    $gameTiles = $parsed['game_tiles'];
    $frontTiles = $parsed['front_tiles'];
    $hasFront = ($frontTiles !== null && strlen($frontTiles) >= $gw * $gh);
    $step = max(1, (int)ceil($tilePx));

    $prioMap = [
        "\x02" => 7, "\x01" => 6, "\x03" => 5, "\x06" => 4,
        "\x10" => 3, "\x08" => 2, "\xd0" => 1,
    ];
    $prioRgb = [
        7 => [239, 68, 68],
        6 => [132, 152, 174],
        5 => [184, 146, 104],
        4 => [56, 189, 248],
        3 => [245, 158, 11],
        2 => [16, 185, 129],
        1 => [34, 197, 94],
    ];
    $prioChr = [];
    foreach ($prioRgb as $pk => $rgb) {
        $prioChr[$pk] = [chr($rgb[0]), chr($rgb[1]), chr($rgb[2]), "\xff"];
    }

    for ($ty = $ty0; $ty <= $ty1; $ty++) {
        $py = (int)floor($oy + ($ty - $ty0) * $tilePx);
        if ($py < 0 || $py >= $imgH) {
            continue;
        }
        $rowOff = $ty * $gw;
        $tx = $tx0;
        while ($tx <= $tx1) {
            $cell = $rowOff + $tx;
            $gb = $gameTiles[$cell];
            if (!$hasFront && $gb === "\x00") {
                $tx += max(1, strspn($gameTiles, "\x00", $cell, $tx1 - $tx + 1));
                continue;
            }
            $fb = $hasFront ? $frontTiles[$cell] : "\x00";
            if ($gb === "\x00" && $fb === "\x00") {
                $tx++;
                continue;
            }
            $pg = $prioMap[$gb] ?? 0;
            $pf = ($fb !== "\x00" && $fb !== "\xd0") ? ($prioMap[$fb] ?? 0) : 0;
            $best = $pg >= $pf ? $pg : $pf;
            if ($best > 0) {
                $px = (int)floor($ox + ($tx - $tx0) * $tilePx);
                if ($step <= 1) {
                    if ($px >= 0 && $px < $imgW) {
                        $off = $px * 4;
                        $pc = $prioChr[$best];
                        $rows[$py][$off] = $pc[0];
                        $rows[$py][$off + 1] = $pc[1];
                        $rows[$py][$off + 2] = $pc[2];
                        $rows[$py][$off + 3] = $pc[3];
                    }
                } else {
                    $rgb = $prioRgb[$best];
                    $fillRect($px, $py, $px + $step - 1, $py + $step - 1, $rgb[0], $rgb[1], $rgb[2]);
                }
            }
            $tx++;
        }
    }

    // Draw structure outlines.
    if ($ctx['showStructures']) {
        foreach (array_slice($ctx['visibleStructures'], 0, 50) as $st) {
            $b = $st['bbox_tiles'];
            $x0 = (int)floor($ox + ($b['tx0'] - $tx0) * $tilePx);
            $y0 = (int)floor($oy + ($b['ty0'] - $ty0) * $tilePx);
            $x1 = (int)ceil($ox + ($b['tx1'] - $tx0 + 1) * $tilePx);
            $y1 = (int)ceil($oy + ($b['ty1'] - $ty0 + 1) * $tilePx);
            [$r, $g, $bl] = ai_map_structure_color((string)$st['kind']);
            $drawRect($x0, $y0, $x1, $y1, $r, $g, $bl);
        }
    }

    // Draw object markers & highlight crosshair.
    if ($ctx['showObjects']) {
        foreach (array_slice($ctx['visibleObjects'], 0, 45) as $obj) {
            $mx = (int)round($ox + ($obj['tile_x'] - $tx0 + 0.5) * $tilePx);
            $my = (int)round($oy + ($obj['tile_y'] - $ty0 + 0.5) * $tilePx);
            [$r, $g, $b] = $obj['rgb'];
            $fillRect($mx - 2, $my - 2, $mx + 2, $my + 2, $r, $g, $b);
        }
    }
    if ($ctx['highlightPt'] !== null) {
        $hx = (int)round($ox + ($ctx['highlightPt']['x'] / 32.0 - $tx0) * $tilePx);
        $hy = (int)round($oy + ($ctx['highlightPt']['y'] / 32.0 - $ty0) * $tilePx);
        $fillRect($hx - 6, $hy, $hx + 6, $hy, 255, 59, 48);
        $fillRect($hx, $hy - 6, $hx, $hy + 6, 255, 59, 48);
    }

    // Viewport frame border.
    $drawRect($ox, $oy, $ox + $drawW - 1, $oy + $drawH - 1, 90, 108, 132);

    // Assemble PNG scanlines directly from per-row buffers without intermediate 1MB copy.
    $scanlines = "\x00" . implode("\x00", $rows);
    $idat = gzcompress($scanlines, 6);
    if (!is_string($idat)) {
        $idat = '';
    }
    $chunk = static function (string $type, string $data): string {
        $crc = crc32($type . $data);
        return pack('N', strlen($data)) . $type . $data . pack('N', $crc);
    };
    $ihdr = pack('NNCCCCC', $imgW, $imgH, 8, 6, 0, 0, 0);
    return "\x89PNG\r\n\x1a\n"
        . $chunk('IHDR', $ihdr)
        . $chunk('IDAT', $idat)
        . $chunk('IEND', '');
}
