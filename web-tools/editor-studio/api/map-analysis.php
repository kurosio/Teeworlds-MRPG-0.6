<?php
declare(strict_types=1);

/**
 * Semantic & structural DDNet/MRPG map analysis for Editor Studio AI.
 *
 * Goes beyond raw game/switch tile counts: inspects ALL layers (game, front,
 * switch, tele, speedup, visual tile layers, quad layers, images/tilesets,
 * INFO settings), correlates world DB entities (houses, guild halls, NPCs,
 * warehouses, aethers, quest boards, dungeons, world swaps), builds a compact
 * visual bitmask + vertical floor/roof geometry profile, clusters connected
 * architectural structures (castles/guild halls, houses, shops/services,
 * dungeons/ruins, caves, general buildings), provides rich spatial point
 * context + nearest standable point, and produces semantic region analysis
 * with an ASCII minimap.
 */

require_once __DIR__ . '/map-datafile.php';

/** Visual bitmask flags (1 byte per map tile cell). */
const AI_VIS_ARCHITECTURE = 0x01;
const AI_VIS_INTERIOR     = 0x02;
const AI_VIS_TERRAIN      = 0x04;
const AI_VIS_NATURE       = 0x08;
const AI_VIS_WATER        = 0x10;
const AI_VIS_PROPS        = 0x20;
const AI_VIS_BG_VISUAL    = 0x40;
const AI_VIS_FG_VISUAL    = 0x80;

/** Canonical MRPG special gameplay tiles (game & front layers, src/game/mapitems.h). */
function ai_map_special_tile_catalog(): array
{
    return [
        4 => 'fixed_camera',
        5 => 'fixed_camera_smooth',
        6 => 'water',
        8 => 'player_house',
        11 => 'destroyer_projectile',
        14 => 'world_swapper',
        15 => 'jail_zone',
        16 => 'guild_house',
        17 => 'auction',
        21 => 'quest_board',
        28 => 'aether_teleport',
        29 => 'shop_zone',
        30 => 'craft_zone',
        32 => 'guild_chair',
        33 => 'chair_lv1',
        34 => 'chair_lv2',
        35 => 'chair_lv3',
        38 => 'bank_manager',
        39 => 'fishing_mode',
        40 => 'casino_action',
        60 => 'stop_all',
        61 => 'stop_single',
        62 => 'stop_both',
    ];
}

/** Canonical tele layer tile types (src/game/mapitems.h). */
function ai_map_tele_tile_catalog(): array
{
    return [
        10 => 'tele_from_confirm',
        26 => 'tele_from',
        27 => 'tele_out',
    ];
}

/** Canonical visual layer semantic categories. */
function ai_map_visual_category_catalog(): array
{
    return [
        'architecture' => 'Архитектура: стены, крыши, дома, замки, мосты, заборы, колонны, городские тайлсеты',
        'interior' => 'Интерьер: фоновые стены помещений, мебель, окна, ковры, внутренний декор',
        'terrain' => 'Рельеф и земля: трава, камень, пустыня, снег, почва, скалы, основные тайлсеты ландшафта',
        'nature' => 'Природа: деревья, кусты, листва, цветы, дудадсы растительности',
        'water' => 'Вода и жидкости: реки, озёра, море, водопады, лава',
        'props' => 'Предметы и декор: вывески, факелы, ящики, статуи, флаги, рыночные лавки',
        'lighting_fx' => 'Освещение и эффекты: лучи солнца, свечение, тени, туман, оверлеи',
        'sky_bg' => 'Небо и дальний фон (parallax != 100%, облака, солнце, горы на горизонте)',
        'other' => 'Прочие визуальные слои',
    ];
}

/** Canonical recognized structure kinds returned by semantic map analysis. */
function ai_map_structure_kinds_catalog(): array
{
    return [
        'castle_or_guild_hall' => 'Замок, гильдейский дом, крепость, башня или крупная многоэтажная цитадель',
        'house' => 'Жилой дом игрока, квартира, хижина, таверна или коттедж',
        'shop_or_service' => 'Магазин, банк, кузница/крафт, склад, аукцион, тюрьма или сервисная постройка',
        'dungeon_or_ruins' => 'Подземелье, руины, храм, склеп',
        'cave_or_underground' => 'Пещера, шахта или подземный грот без архитектуры',
        'building' => 'Прочая крытая архитектурная постройка / помещение',
    ];
}

/**
 * Classify a visual tile/quad layer by its group name, layer name, tileset/image name,
 * and parallax settings. Returns category string + visual bitmask contribution.
 *
 * @param array<string, mixed> $layer
 * @return array{category:string,mask:int,world_aligned:bool,is_lighting:bool}
 */
function ai_map_classify_visual_layer(array $layer): array
{
    $paraX = (int)($layer['parallax_x'] ?? 100);
    $paraY = (int)($layer['parallax_y'] ?? 100);
    $worldAligned = ($paraX === 100 && $paraY === 100);

    $haystack = strtolower(trim(
        (string)($layer['group_name'] ?? '') . ' '
        . (string)($layer['name'] ?? '') . ' '
        . (string)($layer['image_name'] ?? '')
    ));

    if (!$worldAligned) {
        $isLight = (bool)preg_match('/light|sun|glow|ray|beam|shadow|fog|mist|fx|shine|aura/', $haystack);
        return [
            'category' => $isLight ? 'lighting_fx' : 'sky_bg',
            'mask' => 0,
            'world_aligned' => false,
            'is_lighting' => $isLight,
        ];
    }

    if (preg_match('/sun[-_ ]?light|light|glow|shadow|fog|mist|ray|beam|shine|sparkle|aura|vignette|grad/', $haystack)) {
        return [
            'category' => 'lighting_fx',
            'mask' => 0,
            'world_aligned' => true,
            'is_lighting' => true,
        ];
    }

    if (preg_match('/sky|cloud|moon|star|horizon|backdrop/', $haystack)) {
        return [
            'category' => 'sky_bg',
            'mask' => 0,
            'world_aligned' => true,
            'is_lighting' => false,
        ];
    }

    $mask = 0;
    $category = 'other';

    $isWater = (bool)preg_match('/water|sea|ocean|river|lake|pond|pool|wave|waterfall|lava|swamp/', $haystack);
    $isInterior = (bool)preg_match('/interior|indoor|room|bg[_ -]?col|backwall|wall[_ -]?bg|furniture|carpet|bookshelf|window|tavern|bed|lamp|curtain|wallpaper|shelf|inn|indoor/', $haystack);
    $isArch = (bool)preg_match('/house|home|castle|guild|fort|tower|city|town|village|nari|roof|wall|brick|masonry|wood|plank|bridge|fence|door|gate|pillar|column|stair|floor|shop|bank|market|ruin|temple|dungeon|medieval|arch|build|struct|apt|cabin|hut|citadel|palace|stone[_ -]?wall|tileset[_ -]?city|city_m/', $haystack);
    $isProps = (bool)preg_match('/prop|decor|sign|crate|barrel|chest|torch|lantern|banner|flag|statue|fountain|well|anvil|forge|stall|market|chair|table|box|object/', $haystack);
    $isNature = (bool)preg_match('/tree|bush|flower|plant|leaf|leaves|vine|foliage|forest|palm|mushroom|fern|branch|trunk|doodad/', $haystack);
    $isTerrain = (bool)preg_match('/grass|jungle|desert|winter|snow|ice|rock|mountain|dirt|earth|soil|ground|sand|hill|cliff|cave|stone|generic_unhookable|ddmax|freeze|terrain|land|main/', $haystack);

    if ($isInterior) {
        $mask |= AI_VIS_INTERIOR | AI_VIS_ARCHITECTURE;
        $category = 'interior';
    }
    if ($isArch) {
        $mask |= AI_VIS_ARCHITECTURE;
        if ($category === 'other') {
            $category = 'architecture';
        }
    }
    if ($isWater) {
        $mask |= AI_VIS_WATER;
        if ($category === 'other') {
            $category = 'water';
        }
    }
    if ($isProps) {
        $mask |= AI_VIS_PROPS;
        if ($category === 'other') {
            $category = 'props';
        }
    }
    if ($isNature) {
        $mask |= AI_VIS_NATURE;
        if ($category === 'other') {
            $category = 'nature';
        }
    }
    if ($isTerrain) {
        $mask |= AI_VIS_TERRAIN;
        if ($category === 'other') {
            $category = 'terrain';
        }
    }

    return [
        'category' => $category,
        'mask' => $mask,
        'world_aligned' => true,
        'is_lighting' => false,
    ];
}

/**
 * Resolve game/front tile ID to a canonical human-readable name.
 */
function ai_map_tile_name(int $tileId): string
{
    static $collision = [0 => 'air', 1 => 'solid', 2 => 'death', 3 => 'nohook'];
    if (isset($collision[$tileId])) {
        return $collision[$tileId];
    }
    $special = ai_map_special_tile_catalog();
    if (isset($special[$tileId])) {
        return $special[$tileId];
    }
    if ($tileId >= 191) {
        $entities = ai_schema_map_definitions()['game_entity_tiles'] ?? [];
        if (isset($entities[$tileId])) {
            return (string)$entities[$tileId];
        }
        return 'entity_' . $tileId;
    }
    return 'tile_' . $tileId;
}

/**
 * Scan all visual tile layers once to build:
 *  - 1-byte/cell world-aligned visual bitmask ($visMask)
 *  - per-layer metadata (category, depth, non_empty_tiles, bbox, coverage_pct)
 *
 * Uses C-speed strspn zero skipping on v3 flat layers and skip-RLE on v4.
 *
 * @param array<string, mixed> $parsed Output of ddnet_parse_full_map()
 * @return array{vis_mask:string,visual_layers:array<int, array<string, mixed>>,category_counts:array<string, int>,quad_layers:array<int, array<string, mixed>>}
 */
function ai_map_scan_visual_layers(string $bytes, array $parsed, int $maxUncompressed): array
{
    $gw = (int)$parsed['game']['width'];
    $gh = (int)$parsed['game']['height'];
    $gameLayerIdx = (int)($parsed['game']['layer_index'] ?? 0);
    $visMask = str_repeat("\x00", $gw * $gh);

    $visualLayers = [];
    $quadLayers = [];
    $categoryCounts = [];
    $tables = $parsed['tables'];

    foreach ($parsed['layers'] as $layer) {
        $role = (string)($layer['role'] ?? '');
        if ($role !== 'visual_tiles' && $role !== 'visual_quads') {
            continue;
        }
        $cls = ai_map_classify_visual_layer($layer);
        $cat = $cls['category'];
        $categoryCounts[$cat] = ($categoryCounts[$cat] ?? 0) + 1;
        $depth = ((int)$layer['index'] < $gameLayerIdx) ? 'background' : 'foreground';

        if ($role === 'visual_quads') {
            $quadLayers[] = [
                'index' => (int)$layer['index'],
                'name' => (string)$layer['name'],
                'group' => (string)$layer['group_name'],
                'image' => $layer['image_name'],
                'num_quads' => (int)($layer['num_quads'] ?? 0),
                'category' => $cat,
                'depth' => $depth,
                'parallax' => [(int)$layer['parallax_x'], (int)$layer['parallax_y']],
            ];
            continue;
        }

        $lw = (int)($layer['width'] ?? 0);
        $lh = (int)($layer['height'] ?? 0);
        $dataIdx = (int)($layer['data_index'] ?? -1);
        $tileVer = (int)($layer['tile_version'] ?? 3);

        $nonEmpty = 0;
        $minX = $lw;
        $minY = $lh;
        $maxX = -1;
        $maxY = -1;

        if ($lw >= 1 && $lh >= 1 && $lw * $lh <= 4000000 && $dataIdx >= 0) {
            $plain = ddnet_read_data_block($bytes, $tables, $dataIdx, $maxUncompressed);
            if (is_string($plain) && $plain !== '') {
                $writeMask = $cls['world_aligned'] && !$cls['is_lighting'];
                $bit = $cls['mask'];
                if ($writeMask) {
                    $bit |= ($depth === 'background') ? AI_VIS_BG_VISUAL : AI_VIS_FG_VISUAL;
                }
                $offTx = (int)round(((int)($layer['offset_x'] ?? 0)) / 32.0);
                $offTy = (int)round(((int)($layer['offset_y'] ?? 0)) / 32.0);
                $count = $lw * $lh;
                $len = strlen($plain);
                $bitChr = $bit !== 0 ? chr($bit) : "\x00";
                $orLut = null;
                if ($writeMask && $bit !== 0) {
                    $orLut = '';
                    for ($b = 0; $b < 256; $b++) {
                        $orLut .= chr($b | $bit);
                    }
                }

                if ($tileVer >= 4) {
                    $written = 0;
                    for ($off = 0; $off + 4 <= $len && $written < $count; $off += 4) {
                        $repeat = min(ord($plain[$off + 2]) + 1, $count - $written);
                        if ($plain[$off] === "\x00") {
                            $written += $repeat;
                            continue;
                        }
                        $nonEmpty += $repeat;
                        $endCell = $written + $repeat - 1;
                        $y0 = intdiv($written, $lw);
                        $y1 = intdiv($endCell, $lw);
                        if ($y0 < $minY) {
                            $minY = $y0;
                        }
                        if ($y1 > $maxY) {
                            $maxY = $y1;
                        }
                        if ($y0 === $y1) {
                            $x0 = $written % $lw;
                            $x1 = $endCell % $lw;
                            if ($x0 < $minX) {
                                $minX = $x0;
                            }
                            if ($x1 > $maxX) {
                                $maxX = $x1;
                            }
                        } else {
                            $minX = 0;
                            $maxX = $lw - 1;
                        }
                        if ($orLut !== null) {
                            for ($cell = $written; $cell <= $endCell; $cell++) {
                                $tx = ($cell % $lw) + $offTx;
                                $ty = intdiv($cell, $lw) + $offTy;
                                if ($tx >= 0 && $tx < $gw && $ty >= 0 && $ty < $gh) {
                                    $gCell = $ty * $gw + $tx;
                                    $cur = $visMask[$gCell];
                                    $visMask[$gCell] = $cur === "\x00" ? $bitChr : $orLut[ord($cur)];
                                }
                            }
                        }
                        $written += $repeat;
                    }
                } else {
                    // v3 flat 4 bytes/cell: skip long zero runs with C strspn().
                    $rows = min($lh, intdiv($len, $lw * 4));
                    $rowBytes = $lw * 4;
                    for ($ty = 0; $ty < $rows; $ty++) {
                        $rowStart = $ty * $rowBytes;
                        $rowEnd = $rowStart + $rowBytes;
                        $pos = $rowStart;
                        $gTy = $ty + $offTy;
                        $gRowValid = $orLut !== null && $gTy >= 0 && $gTy < $gh;
                        $gRowOff = $gTy * $gw;
                        while ($pos < $rowEnd) {
                            $z = strspn($plain, "\x00", $pos, $rowEnd - $pos);
                            $pos += ($z & ~3);
                            if ($pos >= $rowEnd) {
                                break;
                            }
                            if ($plain[$pos] !== "\x00") {
                                $tx = ($pos - $rowStart) >> 2;
                                $nonEmpty++;
                                if ($tx < $minX) {
                                    $minX = $tx;
                                }
                                if ($tx > $maxX) {
                                    $maxX = $tx;
                                }
                                if ($ty < $minY) {
                                    $minY = $ty;
                                }
                                if ($ty > $maxY) {
                                    $maxY = $ty;
                                }
                                if ($gRowValid) {
                                    $gTx = $tx + $offTx;
                                    if ($gTx >= 0 && $gTx < $gw) {
                                        $gCell = $gRowOff + $gTx;
                                        $cur = $visMask[$gCell];
                                        $visMask[$gCell] = $cur === "\x00" ? $bitChr : $orLut[ord($cur)];
                                    }
                                }
                            }
                            $pos += 4;
                        }
                    }
                }
            }
        }

        $visualLayers[] = [
            'index' => (int)$layer['index'],
            'name' => (string)$layer['name'],
            'group' => (string)$layer['group_name'],
            'image' => $layer['image_name'],
            'category' => $cat,
            'depth' => $depth,
            'width' => $lw,
            'height' => $lh,
            'parallax' => [(int)$layer['parallax_x'], (int)$layer['parallax_y']],
            'world_aligned' => $cls['world_aligned'],
            'non_empty_tiles' => $nonEmpty,
            'coverage_pct' => ($lw * $lh > 0) ? round(($nonEmpty * 100.0) / ($lw * $lh), 2) : 0.0,
            'bbox_tiles' => $nonEmpty > 0 ? ['tx0' => $minX, 'ty0' => $minY, 'tx1' => $maxX, 'ty1' => $maxY] : null,
        ];
    }

    return [
        'vis_mask' => $visMask,
        'visual_layers' => $visualLayers,
        'category_counts' => $categoryCounts,
        'quad_layers' => $quadLayers,
    ];
}

/**
 * 2-pass O(w*h) vertical scan producing per-cell distToRoof (255=open sky),
 * distToFloor (255=void), and compact terrain elevation profile across columns.
 *
 * @return array{dist_roof:string,dist_floor:string,terrain_profile:array<int, array<string, mixed>>,composition:array<string, mixed>}
 */
function ai_map_scan_geometry(int $w, int $h, string $gameTiles, ?string $frontTiles, string $visMask): array
{
    static $chrTable = null;
    if ($chrTable === null) {
        $chrTable = '';
        for ($i = 0; $i < 256; $i++) {
            $chrTable .= chr($i);
        }
    }

    $n = $w * $h;
    $distRoof = str_repeat("\xff", $n);
    $distFloor = str_repeat("\xff", $n);
    $hasFront = ($frontTiles !== null && strlen($frontTiles) >= $n);

    $bands = min(20, max(1, intdiv($w, 16)));
    $bandW = max(1, (int)ceil($w / $bands));
    $bandInfo = [];
    for ($b = 0; $b < $bands; $b++) {
        $bandInfo[$b] = [
            'tx0' => $b * $bandW,
            'tx1' => min($w - 1, ($b + 1) * $bandW - 1),
            'surface_min_ty' => $h,
            'surface_max_ty' => -1,
            'surface_sum' => 0,
            'surface_cols' => 0,
            'arch_cells' => 0,
            'interior_cells' => 0,
            'terrain_cells' => 0,
            'water_cells' => 0,
            'hazard_cells' => 0,
        ];
    }

    $txBand = [];
    for ($tx = 0; $tx < $w; $tx++) {
        $txBand[$tx] = min($bands - 1, intdiv($tx, $bandW));
    }

    $counts = [
        'solid_ground' => 0,
        'solid_building' => 0,
        'interior_enclosed' => 0,
        'underground_cave' => 0,
        'sheltered_overhang' => 0,
        'outdoor_air' => 0,
        'water' => 0,
        'hazard' => 0,
        'void_air' => 0,
    ];

    // Pass 1 (row-major top-down): dist_roof + outdoor surface height per column.
    // Ignore the top map border (ty === 0) so open sky below the top boundary wall
    // is never mistaken for an indoor ceiling/roof.
    $lastSolid = array_fill(0, $w, -1);
    $firstSurface = array_fill(0, $w, -1);
    $inTopBorder = array_fill(0, $w, true);

    for ($ty = 0; $ty < $h; $ty++) {
        $rowOff = $ty * $w;
        for ($tx = 0; $tx < $w; $tx++) {
            $cell = $rowOff + $tx;
            $gb = $gameTiles[$cell];
            $solid = ($gb === "\x01" || $gb === "\x03");
            if (!$solid && $hasFront) {
                $fb = $frontTiles[$cell];
                $solid = ($fb === "\x01" || $fb === "\x03");
            }
            if ($solid) {
                if (!$inTopBorder[$tx]) {
                    if ($firstSurface[$tx] === -1) {
                        $firstSurface[$tx] = $ty;
                    }
                    $lastSolid[$tx] = $ty;
                }
                $distRoof[$cell] = "\x00";
            } else {
                $inTopBorder[$tx] = false;
                $ls = $lastSolid[$tx];
                if ($ls >= 0) {
                    $d = $ty - $ls;
                    $distRoof[$cell] = $chrTable[$d > 254 ? 254 : $d];
                }
            }
        }
    }
    for ($tx = 0; $tx < $w; $tx++) {
        $fs = $firstSurface[$tx];
        if ($fs >= 0) {
            $bIdx = $txBand[$tx];
            $bandInfo[$bIdx]['surface_cols']++;
            $bandInfo[$bIdx]['surface_sum'] += $fs;
            if ($fs < $bandInfo[$bIdx]['surface_min_ty']) {
                $bandInfo[$bIdx]['surface_min_ty'] = $fs;
            }
            if ($fs > $bandInfo[$bIdx]['surface_max_ty']) {
                $bandInfo[$bIdx]['surface_max_ty'] = $fs;
            }
        }
    }

    // Pass 2 (row-major bottom-up): dist_floor + global semantic cell breakdown.
    $nextSolid = array_fill(0, $w, -1);
    $archOrIntMask = AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR;
    $archIntBgMask = AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR | AI_VIS_BG_VISUAL;

    for ($ty = $h - 1; $ty >= 0; $ty--) {
        $rowOff = $ty * $w;
        for ($tx = 0; $tx < $w; $tx++) {
            $cell = $rowOff + $tx;
            $gb = $gameTiles[$cell];
            $fb = $hasFront ? $frontTiles[$cell] : "\x00";
            $vmb = $visMask[$cell];
            $vm = ($vmb === "\x00") ? 0 : ord($vmb);
            $solid = ($gb === "\x01" || $gb === "\x03" || $fb === "\x01" || $fb === "\x03");
            if ($solid) {
                $nextSolid[$tx] = $ty;
                $distFloor[$cell] = "\x00";
                $bIdx = $txBand[$tx];
                if (($vm & $archOrIntMask) !== 0 && ($vm & AI_VIS_TERRAIN) === 0) {
                    $counts['solid_building']++;
                    $bandInfo[$bIdx]['arch_cells']++;
                } else {
                    $counts['solid_ground']++;
                    $bandInfo[$bIdx]['terrain_cells']++;
                }
                continue;
            }
            $ns = $nextSolid[$tx];
            $df = 255;
            if ($ns >= 0) {
                $d = $ns - $ty;
                $df = $d > 254 ? 254 : $d;
                $distFloor[$cell] = $chrTable[$df];
            }
            $drb = $distRoof[$cell];
            $dr = ($drb === "\xff") ? 255 : ord($drb);

            if ($gb === "\x02" || $fb === "\x02") {
                $counts['hazard']++;
                $bandInfo[$txBand[$tx]]['hazard_cells']++;
            } elseif ($gb === "\x06" || $fb === "\x06" || (($vm & AI_VIS_WATER) !== 0 && ($vm & $archOrIntMask) === 0)) {
                $counts['water']++;
                $bandInfo[$txBand[$tx]]['water_cells']++;
            } elseif ($dr < 36 && $df < 36) {
                $hasArch = ($vm & $archIntBgMask) !== 0;
                $onlyTerrain = ($vm & AI_VIS_TERRAIN) !== 0 && ($vm & $archOrIntMask) === 0;
                if ($hasArch && !$onlyTerrain) {
                    $counts['interior_enclosed']++;
                    $bIdx = $txBand[$tx];
                    $bandInfo[$bIdx]['interior_cells']++;
                    $bandInfo[$bIdx]['arch_cells']++;
                } elseif ($onlyTerrain) {
                    $counts['underground_cave']++;
                    $bandInfo[$txBand[$tx]]['terrain_cells']++;
                } else {
                    $counts['sheltered_overhang']++;
                }
            } elseif ($df === 255) {
                $counts['void_air']++;
            } else {
                $counts['outdoor_air']++;
            }
        }
    }

    $profile = [];
    foreach ($bandInfo as $b) {
        $dom = 'outdoor_terrain';
        if ($b['interior_cells'] >= 40 || $b['arch_cells'] > max(30, $b['terrain_cells'])) {
            $dom = 'built_up';
        } elseif ($b['water_cells'] > max(20, $b['terrain_cells'])) {
            $dom = 'water';
        } elseif ($b['surface_cols'] === 0) {
            $dom = 'open_void';
        }
        $profile[] = [
            'tx0' => $b['tx0'],
            'tx1' => $b['tx1'],
            'x0' => $b['tx0'] * 32,
            'x1' => ($b['tx1'] + 1) * 32,
            'avg_surface_ty' => $b['surface_cols'] > 0 ? round($b['surface_sum'] / $b['surface_cols'], 1) : null,
            'min_surface_ty' => $b['surface_cols'] > 0 ? $b['surface_min_ty'] : null,
            'max_surface_ty' => $b['surface_cols'] > 0 ? $b['surface_max_ty'] : null,
            'dominant' => $dom,
            'interior_tiles' => $b['interior_cells'],
            'architecture_tiles' => $b['arch_cells'],
            'water_tiles' => $b['water_cells'],
        ];
    }

    $composition = [];
    foreach ($counts as $k => $v) {
        $composition[$k . '_tiles'] = $v;
        $composition[$k . '_pct'] = $n > 0 ? round(($v * 100.0) / $n, 2) : 0.0;
    }

    return [
        'dist_roof' => $distRoof,
        'dist_floor' => $distFloor,
        'terrain_profile' => $profile,
        'composition' => $composition,
    ];
}

/**
 * Safely parse a JSON column containing coordinates like {"x":123,"y":456}.
 *
 * @return array<string, mixed>|array<int, mixed>|null
 */
function ai_map_decode_json_field(?string $raw): ?array
{
    if ($raw === null || trim($raw) === '') {
        return null;
    }
    $decoded = json_decode($raw, true);
    return is_array($decoded) ? $decoded : null;
}

/**
 * Load correlated DB world objects (houses, guild houses, warehouses, aethers,
 * quest boards, dungeons, world swaps, NPCs, mobs, quest bots, collecting nodes)
 * for a given WorldID, strictly honoring guest/admin ACL and missing-table safety.
 *
 * @return array<string, mixed>
 */
function ai_map_load_world_objects(?mysqli $db, ?int $worldId, string $role = EDITOR_ROLE_ADMIN, ?array $guestPages = null): array
{
    $empty = [
        'houses' => [],
        'guild_houses' => [],
        'warehouses' => [],
        'aethers' => [],
        'quest_boards' => [],
        'dungeons' => [],
        'world_swaps' => [],
        'npcs' => [],
        'mobs' => [],
        'quest_bots' => [],
        'collecting_nodes' => [],
        'counts' => [],
    ];
    if ($db === null || $worldId === null || $worldId < 0) {
        return $empty;
    }

    $runQuery = static function (string $sql, int $wid) use ($db): array {
        try {
            $stmt = $db->prepare($sql);
            if (!$stmt) {
                return [];
            }
            $stmt->bind_param('i', $wid);
            $stmt->execute();
            $res = $stmt->get_result();
            $rows = [];
            while ($res && ($row = $res->fetch_assoc())) {
                $rows[] = $row;
            }
            $stmt->close();
            return $rows;
        } catch (Throwable) {
            return [];
        }
    };

    $can = static function (string $resource) use ($role, $guestPages): bool {
        if (!function_exists('ai_can_read_resource')) {
            return $role === EDITOR_ROLE_ADMIN;
        }
        return ai_can_read_resource($resource, $role, $guestPages);
    };

    $hasTable = static function (string $table) use ($db): bool {
        if (function_exists('ai_table_exists')) {
            return ai_table_exists($db, $table);
        }
        return false;
    };

    // Public world geometry tables (always accessible when maps are readable).
    if ($hasTable('tw_houses')) {
        foreach ($runQuery('SELECT ID, Class, InitialFee, RentDays, Doors, Farmzones, Properties FROM tw_houses WHERE WorldID = ? ORDER BY ID ASC LIMIT 200', $worldId) as $r) {
            $props = ai_map_decode_json_field(isset($r['Properties']) ? (string)$r['Properties'] : null) ?? [];
            $pos = is_array($props['position'] ?? null) ? $props['position'] : null;
            $doorsRaw = ai_map_decode_json_field(isset($r['Doors']) ? (string)$r['Doors'] : null) ?? [];
            $doors = [];
            foreach ($doorsRaw as $d) {
                if (is_array($d) && is_array($d['position'] ?? null)) {
                    $dx = (int)round((float)($d['position']['x'] ?? 0));
                    $dy = (int)round((float)($d['position']['y'] ?? 0));
                    $doors[] = [
                        'name' => (string)($d['name'] ?? 'Door'),
                        'x' => $dx,
                        'y' => $dy,
                        'tile_x' => intdiv($dx, 32),
                        'tile_y' => intdiv($dy, 32),
                    ];
                }
            }
            $px = $pos !== null ? (int)round((float)($pos['x'] ?? 0)) : ($doors[0]['x'] ?? null);
            $py = $pos !== null ? (int)round((float)($pos['y'] ?? 0)) : ($doors[0]['y'] ?? null);
            $empty['houses'][] = [
                'id' => (int)$r['ID'],
                'class' => (string)($r['Class'] ?? ''),
                'initial_fee' => (int)($r['InitialFee'] ?? 0),
                'rent_days' => (int)($r['RentDays'] ?? 0),
                'x' => $px,
                'y' => $py,
                'tile_x' => $px !== null ? intdiv($px, 32) : null,
                'tile_y' => $py !== null ? intdiv($py, 32) : null,
                'doors' => $doors,
            ];
        }
    }

    if ($hasTable('tw_guilds_houses')) {
        foreach ($runQuery('SELECT ID, InitialFee, RentDays, Doors, Farmzones, Properties, KickPos FROM tw_guilds_houses WHERE WorldID = ? ORDER BY ID ASC LIMIT 100', $worldId) as $r) {
            $props = ai_map_decode_json_field(isset($r['Properties']) ? (string)$r['Properties'] : null) ?? [];
            $pos = is_array($props['position'] ?? null) ? $props['position'] : null;
            $radius = isset($props['radius']) ? (int)round((float)$props['radius']) : null;
            $doorsRaw = ai_map_decode_json_field(isset($r['Doors']) ? (string)$r['Doors'] : null) ?? [];
            $doors = [];
            foreach ($doorsRaw as $d) {
                if (is_array($d) && is_array($d['position'] ?? null)) {
                    $dx = (int)round((float)($d['position']['x'] ?? 0));
                    $dy = (int)round((float)($d['position']['y'] ?? 0));
                    $doors[] = [
                        'name' => (string)($d['name'] ?? 'Door'),
                        'x' => $dx,
                        'y' => $dy,
                        'tile_x' => intdiv($dx, 32),
                        'tile_y' => intdiv($dy, 32),
                    ];
                }
            }
            $farmsRaw = ai_map_decode_json_field(isset($r['Farmzones']) ? (string)$r['Farmzones'] : null) ?? [];
            $farms = [];
            foreach ($farmsRaw as $fz) {
                if (is_array($fz) && is_array($fz['position'] ?? null)) {
                    $fx = (int)round((float)($fz['position']['x'] ?? 0));
                    $fy = (int)round((float)($fz['position']['y'] ?? 0));
                    $farms[] = [
                        'name' => (string)($fz['name'] ?? 'FarmZone'),
                        'x' => $fx,
                        'y' => $fy,
                        'radius' => isset($fz['radius']) ? (int)round((float)$fz['radius']) : null,
                    ];
                }
            }
            $px = $pos !== null ? (int)round((float)($pos['x'] ?? 0)) : ($doors[0]['x'] ?? null);
            $py = $pos !== null ? (int)round((float)($pos['y'] ?? 0)) : ($doors[0]['y'] ?? null);
            $empty['guild_houses'][] = [
                'id' => (int)$r['ID'],
                'initial_fee' => (int)($r['InitialFee'] ?? 0),
                'rent_days' => (int)($r['RentDays'] ?? 0),
                'x' => $px,
                'y' => $py,
                'tile_x' => $px !== null ? intdiv($px, 32) : null,
                'tile_y' => $py !== null ? intdiv($py, 32) : null,
                'radius_px' => $radius,
                'doors' => $doors,
                'farmzones' => $farms,
            ];
        }
    }

    if ($hasTable('tw_world_swap')) {
        foreach ($runQuery('SELECT ID, PositionX, PositionY, TwoWorldID, TwoPositionX, TwoPositionY FROM tw_world_swap WHERE WorldID = ? ORDER BY ID ASC LIMIT 100', $worldId) as $r) {
            $x = (int)($r['PositionX'] ?? 0);
            $y = (int)($r['PositionY'] ?? 0);
            $empty['world_swaps'][] = [
                'id' => (int)$r['ID'],
                'x' => $x,
                'y' => $y,
                'tile_x' => intdiv($x, 32),
                'tile_y' => intdiv($y, 32),
                'target_world_id' => isset($r['TwoWorldID']) ? (int)$r['TwoWorldID'] : null,
            ];
        }
    }

    // Resource-scoped tables: only query if session ACL permits reading the resource.
    if ($can('warehouses') && $hasTable('tw_warehouses')) {
        foreach ($runQuery('SELECT ID, Name, Type, PosX, PosY FROM tw_warehouses WHERE WorldID = ? ORDER BY ID ASC LIMIT 100', $worldId) as $r) {
            $x = (int)($r['PosX'] ?? 0);
            $y = (int)($r['PosY'] ?? 0);
            $empty['warehouses'][] = [
                'id' => (int)$r['ID'],
                'name' => (string)($r['Name'] ?? ''),
                'type' => (string)($r['Type'] ?? ''),
                'x' => $x,
                'y' => $y,
                'tile_x' => intdiv($x, 32),
                'tile_y' => intdiv($y, 32),
            ];
        }
    }

    if ($can('aethers') && $hasTable('tw_aethers')) {
        foreach ($runQuery('SELECT ID, Name, TeleX, TeleY FROM tw_aethers WHERE WorldID = ? ORDER BY ID ASC LIMIT 100', $worldId) as $r) {
            $x = (int)($r['TeleX'] ?? 0);
            $y = (int)($r['TeleY'] ?? 0);
            $empty['aethers'][] = [
                'id' => (int)$r['ID'],
                'name' => (string)($r['Name'] ?? ''),
                'x' => $x,
                'y' => $y,
                'tile_x' => intdiv($x, 32),
                'tile_y' => intdiv($y, 32),
            ];
        }
    }

    if ($can('quest_boards') && $hasTable('tw_quest_boards')) {
        foreach ($runQuery('SELECT ID, Name, PosX, PosY FROM tw_quest_boards WHERE WorldID = ? ORDER BY ID ASC LIMIT 100', $worldId) as $r) {
            $x = (int)($r['PosX'] ?? 0);
            $y = (int)($r['PosY'] ?? 0);
            $empty['quest_boards'][] = [
                'id' => (int)$r['ID'],
                'name' => (string)($r['Name'] ?? ''),
                'x' => $x,
                'y' => $y,
                'tile_x' => intdiv($x, 32),
                'tile_y' => intdiv($y, 32),
            ];
        }
    }

    if ($can('dungeons') && $hasTable('tw_dungeons')) {
        foreach ($runQuery('SELECT ID, Level, DoorX, DoorY, TimeLimit FROM tw_dungeons WHERE WorldID = ? ORDER BY ID ASC LIMIT 50', $worldId) as $r) {
            $x = (int)($r['DoorX'] ?? 0);
            $y = (int)($r['DoorY'] ?? 0);
            $empty['dungeons'][] = [
                'id' => (int)$r['ID'],
                'level' => (int)($r['Level'] ?? 0),
                'x' => $x,
                'y' => $y,
                'tile_x' => intdiv($x, 32),
                'tile_y' => intdiv($y, 32),
                'time_limit' => (int)($r['TimeLimit'] ?? 0),
            ];
        }
    }

    $botNames = [];
    if ($can('bots_info') && $hasTable('tw_bots_info')) {
        try {
            $res = $db->query('SELECT ID, Name FROM tw_bots_info LIMIT 2000');
            while ($res && ($row = $res->fetch_assoc())) {
                $botNames[(int)$row['ID']] = (string)$row['Name'];
            }
        } catch (Throwable) {
        }
    }

    if ($can('bots_npc') && $hasTable('tw_bots_npc')) {
        foreach ($runQuery('SELECT ID, BotID, PosX, PosY, Function, GiveQuestID FROM tw_bots_npc WHERE WorldID = ? ORDER BY ID ASC LIMIT 250', $worldId) as $r) {
            $x = (int)($r['PosX'] ?? 0);
            $y = (int)($r['PosY'] ?? 0);
            $bid = (int)($r['BotID'] ?? 0);
            $empty['npcs'][] = [
                'id' => (int)$r['ID'],
                'bot_id' => $bid,
                'name' => $botNames[$bid] ?? ('NPC #' . $r['ID']),
                'function' => (int)($r['Function'] ?? 0),
                'give_quest_id' => (int)($r['GiveQuestID'] ?? 0),
                'x' => $x,
                'y' => $y,
                'tile_x' => intdiv($x, 32),
                'tile_y' => intdiv($y, 32),
            ];
        }
    }

    if ($can('bots_mobs') && $hasTable('tw_bots_mobs')) {
        foreach ($runQuery('SELECT ID, BotID, PositionX, PositionY, Level, Radius, Boss FROM tw_bots_mobs WHERE WorldID = ? ORDER BY ID ASC LIMIT 250', $worldId) as $r) {
            $x = (int)($r['PositionX'] ?? 0);
            $y = (int)($r['PositionY'] ?? 0);
            $bid = (int)($r['BotID'] ?? 0);
            $empty['mobs'][] = [
                'id' => (int)$r['ID'],
                'bot_id' => $bid,
                'name' => $botNames[$bid] ?? ('Mob #' . $r['ID']),
                'level' => (int)($r['Level'] ?? 1),
                'radius' => (int)($r['Radius'] ?? 0),
                'boss' => !empty($r['Boss']),
                'x' => $x,
                'y' => $y,
                'tile_x' => intdiv($x, 32),
                'tile_y' => intdiv($y, 32),
            ];
        }
    }

    if ($can('quest_bots') && $hasTable('tw_bots_quest')) {
        foreach ($runQuery('SELECT ID, BotID, QuestID, Step, PosX, PosY FROM tw_bots_quest WHERE WorldID = ? ORDER BY ID ASC LIMIT 250', $worldId) as $r) {
            $x = (int)($r['PosX'] ?? 0);
            $y = (int)($r['PosY'] ?? 0);
            $bid = (int)($r['BotID'] ?? 0);
            $empty['quest_bots'][] = [
                'id' => (int)$r['ID'],
                'bot_id' => $bid,
                'name' => $botNames[$bid] ?? ('QuestBot #' . $r['ID']),
                'quest_id' => (int)($r['QuestID'] ?? 0),
                'step' => (int)($r['Step'] ?? 0),
                'x' => $x,
                'y' => $y,
                'tile_x' => intdiv($x, 32),
                'tile_y' => intdiv($y, 32),
            ];
        }
    }

    if ($can('nodes_collecting') && $hasTable('tw_nodes_collecting')) {
        foreach ($runQuery('SELECT ID, SwitchID, Type, Name, Level, Health FROM tw_nodes_collecting WHERE WorldID = ? ORDER BY ID ASC LIMIT 200', $worldId) as $r) {
            $empty['collecting_nodes'][] = [
                'id' => (int)$r['ID'],
                'switch_id' => (int)($r['SwitchID'] ?? 0),
                'type' => (string)($r['Type'] ?? ''),
                'name' => (string)($r['Name'] ?? ''),
                'level' => (int)($r['Level'] ?? 1),
                'health' => (int)($r['Health'] ?? 1),
            ];
        }
    }

    foreach (['houses', 'guild_houses', 'warehouses', 'aethers', 'quest_boards', 'dungeons', 'world_swaps', 'npcs', 'mobs', 'quest_bots', 'collecting_nodes'] as $k) {
        $empty['counts'][$k] = count($empty[$k]);
    }

    return $empty;
}

/**
 * Spatial connected-component clustering on a list of tile coordinates [tx, ty]
 * with Chebyshev gap tolerance `$gap` (e.g. gap=2 bridges doors/thin walls inside
 * one building while keeping separate buildings with the same switch ID apart).
 *
 * @param array<int, array{0:int,1:int}> $points
 * @return array<int, array{cells:array<int, array{0:int,1:int}>,tx0:int,ty0:int,tx1:int,ty1:int,count:int}>
 */
function ai_map_cluster_points(array $points, int $gap = 2): array
{
    if ($points === []) {
        return [];
    }
    $grid = [];
    foreach ($points as $idx => [$tx, $ty]) {
        $grid[($ty << 16) | $tx] = $idx;
    }
    $clusters = [];
    foreach ($points as $startIdx => $pt) {
        $startKey = ($pt[1] << 16) | $pt[0];
        if (!isset($grid[$startKey])) {
            continue;
        }
        unset($grid[$startKey]);
        $queue = [$startIdx];
        $qLen = 1;
        $cells = [];
        $tx0 = $pt[0];
        $ty0 = $pt[1];
        $tx1 = $pt[0];
        $ty1 = $pt[1];

        for ($qi = 0; $qi < $qLen; $qi++) {
            $cur = $points[$queue[$qi]];
            $cx = $cur[0];
            $cy = $cur[1];
            $cells[] = $cur;
            if ($cx < $tx0) {
                $tx0 = $cx;
            }
            if ($cx > $tx1) {
                $tx1 = $cx;
            }
            if ($cy < $ty0) {
                $ty0 = $cy;
            }
            if ($cy > $ty1) {
                $ty1 = $cy;
            }
            $yMin = max(0, $cy - $gap);
            $yMax = $cy + $gap;
            $xMin = max(0, $cx - $gap);
            $xMax = $cx + $gap;
            for ($ny = $yMin; $ny <= $yMax; $ny++) {
                $rowKey = $ny << 16;
                for ($nx = $xMin; $nx <= $xMax; $nx++) {
                    $key = $rowKey | $nx;
                    if (isset($grid[$key])) {
                        $queue[$qLen++] = $grid[$key];
                        unset($grid[$key]);
                    }
                }
            }
        }
        $clusters[] = [
            'cells' => $cells,
            'tx0' => $tx0,
            'ty0' => $ty0,
            'tx1' => $tx1,
            'ty1' => $ty1,
            'count' => $qLen,
        ];
    }
    usort($clusters, static fn(array $a, array $b): int => $b['count'] <=> $a['count']);
    return $clusters;
}

/**
 * Inspect a tile bounding box to measure enclosure, visual composition, floor levels,
 * and special tiles inside it.
 *
 * @return array<string, mixed>
 */
function ai_map_measure_bbox_features(
    int $w,
    int $h,
    int $tx0,
    int $ty0,
    int $tx1,
    int $ty1,
    string $gameTiles,
    ?string $frontTiles,
    string $visMask,
    string $distRoof,
    string $distFloor
): array {
    $tx0 = max(0, $tx0);
    $ty0 = max(0, $ty0);
    $tx1 = min($w - 1, $tx1);
    $ty1 = min($h - 1, $ty1);
    $hasFront = ($frontTiles !== null && strlen($frontTiles) >= $w * $h);
    $specialCatalog = ai_map_special_tile_catalog();

    $total = 0;
    $solid = 0;
    $roofedAir = 0;
    $openAir = 0;
    $arch = 0;
    $interior = 0;
    $terrain = 0;
    $water = 0;
    $bgVisual = 0;
    $specialCounts = [];
    $floorRows = [];

    for ($ty = $ty0; $ty <= $ty1; $ty++) {
        $rowSolidWithAirAbove = 0;
        $rowOff = $ty * $w;
        $aboveOff = $rowOff - $w;
        for ($tx = $tx0; $tx <= $tx1; $tx++) {
            $cell = $rowOff + $tx;
            $total++;
            $gb = $gameTiles[$cell];
            $fb = $hasFront ? $frontTiles[$cell] : "\x00";
            $vmb = $visMask[$cell];

            // Fast path: empty air with no visual overlay (60-75% of cells on typical maps).
            if ($gb === "\x00" && $fb === "\x00" && $vmb === "\x00") {
                $drb = $distRoof[$cell];
                if ($drb !== "\xff" && ord($drb) < 36) {
                    $dfb = $distFloor[$cell];
                    if ($dfb !== "\xff" && ord($dfb) < 36) {
                        $roofedAir++;
                    } else {
                        $openAir++;
                    }
                } else {
                    $openAir++;
                }
                continue;
            }

            if ($vmb !== "\x00") {
                $vm = ord($vmb);
                if (($vm & AI_VIS_ARCHITECTURE) !== 0) {
                    $arch++;
                }
                if (($vm & AI_VIS_INTERIOR) !== 0) {
                    $interior++;
                }
                if (($vm & AI_VIS_TERRAIN) !== 0) {
                    $terrain++;
                }
                if (($vm & AI_VIS_WATER) !== 0) {
                    $water++;
                } elseif ($gb === "\x06" || $fb === "\x06") {
                    $water++;
                }
                if (($vm & AI_VIS_BG_VISUAL) !== 0) {
                    $bgVisual++;
                }
            } elseif ($gb === "\x06" || $fb === "\x06") {
                $water++;
            }

            if ($gb !== "\x00" && $gb !== "\x01" && $gb !== "\x03") {
                $g = ord($gb);
                if (isset($specialCatalog[$g])) {
                    $sn = $specialCatalog[$g];
                    $specialCounts[$sn] = ($specialCounts[$sn] ?? 0) + 1;
                }
            }
            if ($fb !== "\x00" && $fb !== "\x01" && $fb !== "\x03") {
                $f = ord($fb);
                if (isset($specialCatalog[$f])) {
                    $sn = $specialCatalog[$f];
                    $specialCounts[$sn] = ($specialCounts[$sn] ?? 0) + 1;
                }
            }

            $isSolid = ($gb === "\x01" || $gb === "\x03" || $fb === "\x01" || $fb === "\x03");
            if ($isSolid) {
                $solid++;
                if ($ty > 0) {
                    $above = $aboveOff + $tx;
                    $gab = $gameTiles[$above];
                    if ($gab !== "\x01" && $gab !== "\x03") {
                        if (!$hasFront || ($frontTiles[$above] !== "\x01" && $frontTiles[$above] !== "\x03")) {
                            $rowSolidWithAirAbove++;
                        }
                    }
                }
            } else {
                $drb = $distRoof[$cell];
                if ($drb !== "\xff" && ord($drb) < 36) {
                    $dfb = $distFloor[$cell];
                    if ($dfb !== "\xff" && ord($dfb) < 36) {
                        $roofedAir++;
                    } else {
                        $openAir++;
                    }
                } else {
                    $openAir++;
                }
            }
        }
        $bw = $tx1 - $tx0 + 1;
        // Skip the top roof edge (ty <= ty0 + 2) so a building's outer roof isn't counted as an interior floor.
        if ($bw >= 4 && $ty > $ty0 + 2 && $rowSolidWithAirAbove >= max(3, (int)round($bw * 0.25))) {
            $floorRows[] = $ty;
        }
    }

    // Count distinct interior floor levels separated by >= 4 tiles vertically (min Teeworlds story height).
    $floors = 0;
    $prevRow = -10;
    foreach ($floorRows as $ry) {
        if ($ry - $prevRow >= 4) {
            $floors++;
            $prevRow = $ry;
        }
    }

    $airTotal = $roofedAir + $openAir;
    $enclosedRatio = $airTotal > 0 ? round($roofedAir / $airTotal, 3) : 0.0;
    $archRatio = $total > 0 ? round(($arch + $interior) / $total, 3) : 0.0;
    $terrainRatio = $total > 0 ? round($terrain / $total, 3) : 0.0;

    return [
        'total_tiles' => $total,
        'solid_tiles' => $solid,
        'roofed_air_tiles' => $roofedAir,
        'open_air_tiles' => $openAir,
        'enclosed_ratio' => $enclosedRatio,
        'arch_ratio' => $archRatio,
        'terrain_ratio' => $terrainRatio,
        'bg_visual_ratio' => $total > 0 ? round($bgVisual / $total, 3) : 0.0,
        'water_tiles' => $water,
        'estimated_floors' => max($roofedAir > 0 ? 1 : 0, $floors),
        'special_tiles' => $specialCounts,
        'is_enclosed' => $roofedAir >= 6 && $enclosedRatio >= 0.35,
        'has_architecture' => ($arch + $interior) >= 4 || ($bgVisual >= 8 && $enclosedRatio >= 0.45),
    ];
}

/**
 * Infer semantic structure kind from name keywords, special tiles, geometry, and visual signals.
 *
 * @param array<string, mixed> $features
 */
function ai_map_classify_structure_kind(string $name, array $features, int $widthTiles, int $heightTiles, ?string $hint = null): string
{
    if ($hint === 'guild_house') {
        return 'castle_or_guild_hall';
    }
    if ($hint === 'house') {
        return 'house';
    }

    $low = strtolower($name);
    $spec = $features['special_tiles'] ?? [];

    if (isset($spec['guild_house']) || isset($spec['guild_chair'])) {
        return 'castle_or_guild_hall';
    }
    if (preg_match('/castle|fort|fortress|citadel|palace|keep|tower|guild|замок|крепость|башн|цитадел|гильд/u', $low)) {
        return 'castle_or_guild_hall';
    }
    if (preg_match('/dungeon|ruin|crypt|temple|shrine|tomb|подземел|руин|храм|склеп/u', $low)) {
        return 'dungeon_or_ruins';
    }
    if (preg_match('/cave|mine|tunnel|cavern|underground|пещер|шахт|грот/u', $low)) {
        return 'cave_or_underground';
    }
    if (
        isset($spec['shop_zone']) || isset($spec['craft_zone']) || isset($spec['bank_manager'])
        || isset($spec['auction']) || isset($spec['jail_zone']) || isset($spec['quest_board'])
        || preg_match('/shop|store|market|bank|craft|smith|forge|auction|warehouse|jail|prison|weapon|armor|potion|guild[_ ]?shop|магазин|банк|кузн|крафт|склад|рынок|тюрьм|оружи/u', $low)
    ) {
        return 'shop_or_service';
    }
    if (isset($spec['player_house']) || preg_match('/house|home|apt|apartment|cabin|hut|cottage|villa|tavern|inn|дом|хижин|квартир|таверн|гостиниц/u', $low)) {
        return 'house';
    }

    $floors = (int)($features['estimated_floors'] ?? 1);
    $roofed = (int)($features['roofed_air_tiles'] ?? 0);
    $archRatio = (float)($features['arch_ratio'] ?? 0.0);
    $terrainRatio = (float)($features['terrain_ratio'] ?? 0.0);

    // Without explicit architectural/interior layers, enclosed underground spaces are caves/tunnels, not castles.
    if ($archRatio < 0.10 && empty($spec)) {
        return ($terrainRatio > 0.10 || $roofed >= 20) ? 'cave_or_underground' : 'building';
    }
    if ((($widthTiles >= 28 && $heightTiles >= 16) || $floors >= 3 || $roofed >= 350) && $archRatio >= 0.15) {
        return 'castle_or_guild_hall';
    }
    if ($widthTiles >= 8 && $widthTiles <= 36 && $heightTiles >= 5 && $heightTiles <= 24 && $archRatio >= 0.12) {
        return 'house';
    }
    return 'building';
}

/**
 * Expand a seed bounding box outward to capture immediate surrounding walls/roof/floor
 * (bounded strictly by maxPad tiles so we don't bleed across whole towns).
 *
 * @return array{0:int,1:int,2:int,3:int}
 */
function ai_map_expand_to_shell(int $w, int $h, int $tx0, int $ty0, int $tx1, int $ty1, string $gameTiles, int $maxPad = 2): array
{
    $tx0 = max(0, $tx0 - $maxPad);
    $ty0 = max(0, $ty0 - $maxPad);
    $tx1 = min($w - 1, $tx1 + $maxPad);
    $ty1 = min($h - 1, $ty1 + $maxPad);
    return [$tx0, $ty0, $tx1, $ty1];
}

/**
 * Compute & cache the deterministic static analysis bundle of a DDNet .map binary
 * (depends strictly on raw map bytes, NEVER on DB world_objects or actor ACL).
 *
 * Uses a small in-process LRU cache plus an atomic, bounded temp-dir disk cache
 * keyed by SHA-256 so repeated analyze_map / get_map_screenshot / point_check calls
 * on the same map skip re-parsing and full-map scanning.
 *
 * @return array<string, mixed>
 */
function ai_map_static_analysis_get(
    string $bytes,
    int $maxBytes,
    int $maxUncompressed,
    bool $useCache = true
): array {
    static $memCache = [];
    static $memOrder = [];

    $sha = hash('sha256', $bytes);
    $cacheKey = $sha . ':' . $maxBytes . ':' . $maxUncompressed;

    if ($useCache && isset($memCache[$cacheKey])) {
        return $memCache[$cacheKey];
    }

    $diskFile = null;
    $diskDir = null;
    if ($useCache && strlen($bytes) >= 16384) {
        $tmpRoot = sys_get_temp_dir();
        if (is_string($tmpRoot) && $tmpRoot !== '' && @is_writable($tmpRoot)) {
            $diskDir = rtrim($tmpRoot, '/\\') . DIRECTORY_SEPARATOR . 'mrpg_ai_map_cache';
            $diskFile = $diskDir . DIRECTORY_SEPARATOR . 'static_v3_' . $sha . '.z';
            if (@is_file($diskFile)) {
                $compressed = @file_get_contents($diskFile);
                if (is_string($compressed) && $compressed !== '') {
                    $raw = @gzuncompress($compressed);
                    if (is_string($raw) && $raw !== '') {
                        $decoded = @unserialize($raw, ['allowed_classes' => false]);
                        if (is_array($decoded) && ($decoded['_cache_ver'] ?? 0) === 3 && ($decoded['parsed']['sha256'] ?? '') === $sha) {
                            $memCache[$cacheKey] = $decoded;
                            $memOrder[] = $cacheKey;
                            if (count($memOrder) > 4) {
                                $evict = array_shift($memOrder);
                                if ($evict !== null && $evict !== $cacheKey) {
                                    unset($memCache[$evict]);
                                }
                            }
                            return $decoded;
                        }
                    }
                }
            }
        }
    }

    $parsed = ddnet_parse_full_map($bytes, $maxBytes, $maxUncompressed);
    $w = (int)$parsed['game']['width'];
    $h = (int)$parsed['game']['height'];
    $n = $w * $h;
    $gameTiles = $parsed['game_tiles'];
    $frontTiles = $parsed['front_tiles'];
    $switchTiles = $parsed['switch_tiles'];
    $teleTiles = $parsed['tele_tiles'];
    $settings = $parsed['info']['settings'] ?? ['zones' => [], 'texts' => [], 'action_zones' => [], 'nodes' => []];

    // 1. Scan visual layers once -> 1-byte/cell visual bitmask + layer catalog.
    $vis = ai_map_scan_visual_layers($bytes, $parsed, $maxUncompressed);
    $visMask = $vis['vis_mask'];

    // 2. Vertical floor/roof geometry scan + terrain elevation profile.
    $geom = ai_map_scan_geometry($w, $h, $gameTiles, $frontTiles, $visMask);
    $distRoof = $geom['dist_roof'];
    $distFloor = $geom['dist_floor'];

    // 3. Collect gameplay stats, entities, special tiles, teleporters, switch zones (C-speed zero skipping).
    $defs = ai_schema_map_definitions();
    $entityNames = $defs['game_entity_tiles'];
    $switchNames = $defs['switch_tiles'];
    $specialCatalog = ai_map_special_tile_catalog();
    $teleCatalog = ai_map_tele_tile_catalog();
    $hasFront = ($frontTiles !== null && strlen($frontTiles) >= $n);

    $nonEmptyGame = 0;
    $solidCount = 0;
    $deathCount = 0;
    $nohookCount = 0;
    $entityCounts = [];
    $entitiesList = [];
    $spawnPoints = [];
    $specialPoints = []; // special_name => [[tx, ty], ...]
    $specialCounts = [];

    // Game layer scan: skip runs of 0x00 (air) and 0x01 (solid) in C via strspn().
    $cell = 0;
    while ($cell < $n) {
        $b = $gameTiles[$cell];
        if ($b === "\x00") {
            $cell += strspn($gameTiles, "\x00", $cell, $n - $cell);
            continue;
        }
        if ($b === "\x01") {
            $run = strspn($gameTiles, "\x01", $cell, $n - $cell);
            $nonEmptyGame += $run;
            $solidCount += $run;
            $cell += $run;
            continue;
        }
        $nonEmptyGame++;
        $g = ord($b);
        if ($g === 2) {
            $deathCount++;
        } elseif ($g === 3) {
            $nohookCount++;
        } elseif (isset($specialCatalog[$g])) {
            $sn = $specialCatalog[$g];
            $specialCounts[$sn] = ($specialCounts[$sn] ?? 0) + 1;
            if (count($specialPoints[$sn] ?? []) < 8000) {
                $specialPoints[$sn][] = [$cell % $w, intdiv($cell, $w)];
            }
        } elseif ($g >= 191) {
            $en = $entityNames[$g] ?? ('entity_' . $g);
            $entityCounts[$en] = ($entityCounts[$en] ?? 0) + 1;
            $tx = $cell % $w;
            $ty = intdiv($cell, $w);
            if ($g === 208 && count($spawnPoints) < 100) {
                $spawnPoints[] = ['x' => $tx * 32 + 16, 'y' => $ty * 32 + 16, 'tile_x' => $tx, 'tile_y' => $ty];
            }
            if (count($entitiesList) < 2000) {
                $entitiesList[] = [
                    'tile' => $g,
                    'name' => $en,
                    'x' => $tx * 32 + 16,
                    'y' => $ty * 32 + 16,
                    'tile_x' => $tx,
                    'tile_y' => $ty,
                ];
            }
        }
        $cell++;
    }

    if ($hasFront) {
        $cell = 0;
        while ($cell < $n) {
            $b = $frontTiles[$cell];
            if ($b === "\x00") {
                $cell += strspn($frontTiles, "\x00", $cell, $n - $cell);
                continue;
            }
            $f = ord($b);
            if (isset($specialCatalog[$f])) {
                $sn = $specialCatalog[$f];
                $specialCounts[$sn] = ($specialCounts[$sn] ?? 0) + 1;
                if (count($specialPoints[$sn] ?? []) < 8000) {
                    $specialPoints[$sn][] = [$cell % $w, intdiv($cell, $w)];
                }
            }
            $cell++;
        }
    }

    // Switch layer scan (4 bytes/cell: [number, type, flags, delay]; >95% zero -> strspn!).
    $switchCounts = [];
    $switchNonEmpty = 0;
    $swZonePts = [];
    $swHousePts = [];
    $swActionPts = [];
    $swTextPts = [];
    $swNodePts = ['plant' => [], 'ore' => [], 'fish' => []];

    if ($switchTiles !== null && $parsed['switch'] !== null) {
        $swW = (int)$parsed['switch']['width'];
        $swH = (int)$parsed['switch']['height'];
        $swEnd = min($swW * $swH, intdiv(strlen($switchTiles), 4)) * 4;
        $pos = 0;
        while ($pos < $swEnd) {
            $z = strspn($switchTiles, "\x00", $pos, $swEnd - $pos);
            $pos += ($z & ~3);
            if ($pos >= $swEnd) {
                break;
            }
            $type = ord($switchTiles[$pos + 1]);
            if ($type !== 0) {
                $switchNonEmpty++;
                $num = ord($switchTiles[$pos]);
                $name = $switchNames[$type] ?? ('switch_' . $type);
                $switchCounts[$name] = ($switchCounts[$name] ?? 0) + 1;
                $i = $pos >> 2;
                $tx = $i % $swW;
                $ty = intdiv($i, $swW);
                if ($type === 22) {
                    if (count($swZonePts[$num] ?? []) < 12000) {
                        $swZonePts[$num][] = [$tx, $ty];
                    }
                } elseif ($type === 24) {
                    if (count($swHousePts[$num] ?? []) < 12000) {
                        $swHousePts[$num][] = [$tx, $ty];
                    }
                } elseif ($type === 25) {
                    if (count($swActionPts[$num] ?? []) < 8000) {
                        $swActionPts[$num][] = [$tx, $ty];
                    }
                } elseif ($type === 23) {
                    if (count($swTextPts[$num] ?? []) < 4000) {
                        $swTextPts[$num][] = [$tx, $ty];
                    }
                } elseif ($type === 199) {
                    $swNodePts['plant'][$num][] = [$tx, $ty];
                } elseif ($type === 200) {
                    $swNodePts['ore'][$num][] = [$tx, $ty];
                } elseif ($type === 201) {
                    $swNodePts['fish'][$num][] = [$tx, $ty];
                }
            }
            $pos += 4;
        }
    }

    // Tele layer scan (2 bytes/cell: [number, type]; >98% zero -> strspn!).
    $teleCounts = [];
    $telePairs = [];
    if ($teleTiles !== null && $parsed['tele'] !== null) {
        $tW = (int)$parsed['tele']['width'];
        $tH = (int)$parsed['tele']['height'];
        $tEnd = min($tW * $tH, intdiv(strlen($teleTiles), 2)) * 2;
        $pos = 0;
        while ($pos < $tEnd) {
            $z = strspn($teleTiles, "\x00", $pos, $tEnd - $pos);
            $pos += ($z & ~1);
            if ($pos >= $tEnd) {
                break;
            }
            $type = ord($teleTiles[$pos + 1]);
            if ($type !== 0) {
                $num = ord($teleTiles[$pos]);
                $tName = $teleCatalog[$type] ?? ('tele_' . $type);
                $teleCounts[$tName] = ($teleCounts[$tName] ?? 0) + 1;
                if (!isset($telePairs[$num])) {
                    $telePairs[$num] = ['number' => $num, 'from' => [], 'out' => []];
                }
                $i = $pos >> 1;
                $tx = $i % $tW;
                $ty = intdiv($i, $tW);
                if ($type === 27) {
                    if (count($telePairs[$num]['out']) < 8) {
                        $telePairs[$num]['out'][] = ['x' => $tx * 32 + 16, 'y' => $ty * 32 + 16, 'tile_x' => $tx, 'tile_y' => $ty];
                    }
                } elseif (count($telePairs[$num]['from']) < 8) {
                    $telePairs[$num]['from'][] = ['x' => $tx * 32 + 16, 'y' => $ty * 32 + 16, 'tile_x' => $tx, 'tile_y' => $ty, 'confirm' => $type === 10];
                }
            }
            $pos += 2;
        }
    }

    // Precompute deterministic spatial clusters & features (independent of DB world_objects).
    $baseClaimed = str_repeat("\x00", $n);
    $markBase = static function (int $tx0, int $ty0, int $tx1, int $ty1) use ($w, $h, &$baseClaimed): void {
        $tx0 = max(0, $tx0);
        $ty0 = max(0, $ty0);
        $tx1 = min($w - 1, $tx1);
        $ty1 = min($h - 1, $ty1);
        for ($ty = $ty0; $ty <= $ty1; $ty++) {
            $rowOff = $ty * $w;
            for ($tx = $tx0; $tx <= $tx1; $tx++) {
                $baseClaimed[$rowOff + $tx] = "\x01";
            }
        }
    };

    $houseClusters = [];
    ksort($swHousePts);
    foreach ($swHousePts as $num => $pts) {
        foreach (ai_map_cluster_points($pts, 2) as $ci => $cl) {
            if ($cl['count'] < 2) {
                continue;
            }
            [$tx0, $ty0, $tx1, $ty1] = ai_map_expand_to_shell($w, $h, $cl['tx0'], $cl['ty0'], $cl['tx1'], $cl['ty1'], $gameTiles, 2);
            $feat = ai_map_measure_bbox_features($w, $h, $tx0, $ty0, $tx1, $ty1, $gameTiles, $frontTiles, $visMask, $distRoof, $distFloor);
            $markBase($tx0, $ty0, $tx1, $ty1);
            $houseClusters[] = [
                'num' => $num,
                'ci' => $ci,
                'count' => $cl['count'],
                'tx0' => $tx0,
                'ty0' => $ty0,
                'tx1' => $tx1,
                'ty1' => $ty1,
                'feat' => $feat,
            ];
        }
    }

    $zoneClusters = [];
    ksort($swZonePts);
    foreach ($swZonePts as $num => $pts) {
        $meta = $settings['zones'][$num] ?? null;
        $zName = $meta !== null && $meta['name'] !== '' ? (string)$meta['name'] : ('Zone #' . $num);
        $pvp = $meta !== null ? (bool)$meta['pvp'] : false;
        foreach (ai_map_cluster_points($pts, 3) as $ci => $cl) {
            [$tx0, $ty0, $tx1, $ty1] = [$cl['tx0'], $cl['ty0'], $cl['tx1'], $cl['ty1']];
            $feat = ai_map_measure_bbox_features($w, $h, $tx0, $ty0, $tx1, $ty1, $gameTiles, $frontTiles, $visMask, $distRoof, $distFloor);
            $bw = $tx1 - $tx0 + 1;
            $bh = $ty1 - $ty0 + 1;
            $shellBbox = null;
            if (($feat['is_enclosed'] || ($feat['has_architecture'] && $bw <= 90 && $bh <= 50)) && $bw >= 4 && $bh >= 3 && $bw * $bh <= 12000) {
                $shellBbox = ai_map_expand_to_shell($w, $h, $tx0, $ty0, $tx1, $ty1, $gameTiles, 1);
                // Mark tentative base claim only if not already >70% claimed by houses.
                $claimed = 0;
                $sample = 0;
                for ($sy = $ty0; $sy <= $ty1; $sy += 2) {
                    $rowOff = $sy * $w;
                    for ($sx = $tx0; $sx <= $tx1; $sx += 2) {
                        $sample++;
                        if ($baseClaimed[$rowOff + $sx] !== "\x00") {
                            $claimed++;
                        }
                    }
                }
                if ($sample === 0 || ($claimed / $sample) < 0.70) {
                    $markBase($shellBbox[0], $shellBbox[1], $shellBbox[2], $shellBbox[3]);
                }
            }
            $zoneClusters[] = [
                'num' => $num,
                'ci' => $ci,
                'name' => $zName,
                'pvp' => $pvp,
                'count' => $cl['count'],
                'tx0' => $tx0,
                'ty0' => $ty0,
                'tx1' => $tx1,
                'ty1' => $ty1,
                'feat' => $feat,
                'shell_bbox' => $shellBbox,
            ];
        }
    }

    $actionZones = [];
    ksort($swActionPts);
    foreach ($swActionPts as $num => $pts) {
        $meta = $settings['action_zones'][$num] ?? null;
        $aName = $meta !== null && $meta['name'] !== '' ? (string)$meta['name'] : ('Action Zone #' . $num);
        foreach (ai_map_cluster_points($pts, 2) as $cl) {
            $cx = intdiv($cl['tx0'] + $cl['tx1'], 2);
            $cy = intdiv($cl['ty0'] + $cl['ty1'], 2);
            $actionZones[] = [
                'number' => $num,
                'name' => $aName,
                'tiles_count' => $cl['count'],
                'bbox_tiles' => ['tx0' => $cl['tx0'], 'ty0' => $cl['ty0'], 'tx1' => $cl['tx1'], 'ty1' => $cl['ty1']],
                'center' => ['x' => $cx * 32 + 16, 'y' => $cy * 32 + 16, 'tile_x' => $cx, 'tile_y' => $cy],
            ];
        }
    }

    $textSigns = [];
    ksort($swTextPts);
    foreach ($swTextPts as $num => $pts) {
        $meta = $settings['texts'][$num] ?? null;
        $txt = $meta !== null ? (string)$meta['text'] : ('Text #' . $num);
        foreach (ai_map_cluster_points($pts, 2) as $cl) {
            $cx = intdiv($cl['tx0'] + $cl['tx1'], 2);
            $cy = intdiv($cl['ty0'] + $cl['ty1'], 2);
            $textSigns[] = [
                'number' => $num,
                'text' => $txt,
                'center' => ['x' => $cx * 32 + 16, 'y' => $cy * 32 + 16, 'tile_x' => $cx, 'tile_y' => $cy],
            ];
        }
    }

    $functionalSpecials = [
        'shop_zone' => 'Shop Building',
        'craft_zone' => 'Crafting Station',
        'bank_manager' => 'Bank',
        'auction' => 'Auction Hall',
        'jail_zone' => 'Jail',
        'quest_board' => 'Quest Board Area',
        'aether_teleport' => 'Aether Shrine',
    ];
    $specialClusters = [];
    foreach ($functionalSpecials as $specKey => $defaultTitle) {
        if (empty($specialPoints[$specKey])) {
            continue;
        }
        foreach (ai_map_cluster_points($specialPoints[$specKey], 3) as $ci => $cl) {
            $alreadyClaimed = false;
            foreach ($cl['cells'] as [$ptx, $pty]) {
                if ($baseClaimed[$pty * $w + $ptx] !== "\x00") {
                    $alreadyClaimed = true;
                    break;
                }
            }
            if ($alreadyClaimed) {
                continue;
            }
            [$tx0, $ty0, $tx1, $ty1] = ai_map_expand_to_shell($w, $h, $cl['tx0'], $cl['ty0'], $cl['tx1'], $cl['ty1'], $gameTiles, 2);
            $feat = ai_map_measure_bbox_features($w, $h, $tx0, $ty0, $tx1, $ty1, $gameTiles, $frontTiles, $visMask, $distRoof, $distFloor);
            $markBase($tx0, $ty0, $tx1, $ty1);
            $specialClusters[] = [
                'spec_key' => $specKey,
                'default_title' => $defaultTitle,
                'ci' => $ci,
                'tx0' => $tx0,
                'ty0' => $ty0,
                'tx1' => $tx1,
                'ty1' => $ty1,
                'feat' => $feat,
                'sample_cell' => $cl['cells'][0],
            ];
        }
    }

    // 5e. Unclaimed geometric/architectural enclosed buildings & caves (works with ZERO zones & ZERO DB!).
    // Fast BFS with explicit X/Y bounds (no horizontal wrap-around) and zero per-cell array allocations.
    $visitedGeo = str_repeat("\x00", $n);
    $geoCandidates = [];
    $archIntMask = AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR;
    $archIntBgMask = AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR | AI_VIS_BG_VISUAL;
    $geoAnyVisMask = AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR | AI_VIS_BG_VISUAL | AI_VIS_TERRAIN;
    $maxGeoBw = min(140, max(48, intdiv($w * 45, 100)));

    for ($ty = 1; $ty < $h - 1; $ty++) {
        $rowOff = $ty * $w;
        for ($tx = 1; $tx < $w - 1; $tx++) {
            $cell = $rowOff + $tx;
            if ($visitedGeo[$cell] !== "\x00" || $baseClaimed[$cell] !== "\x00") {
                continue;
            }
            $gb = $gameTiles[$cell];
            if ($gb === "\x01" || $gb === "\x03" || $gb === "\x02") {
                continue;
            }
            if ($hasFront) {
                $fb = $frontTiles[$cell];
                if ($fb === "\x01" || $fb === "\x03") {
                    continue;
                }
            }
            $drb = $distRoof[$cell];
            if ($drb === "\xff") {
                continue;
            }
            $dr = ord($drb);
            if ($dr >= 30) {
                continue;
            }
            $dfb = $distFloor[$cell];
            if ($dfb === "\xff") {
                continue;
            }
            $df = ord($dfb);
            if ($df >= 30) {
                continue;
            }
            $vmb = $visMask[$cell];
            if ($vmb === "\x00") {
                continue;
            }
            $vm = ord($vmb);
            if (($vm & $archIntBgMask) === 0 && !(($vm & AI_VIS_TERRAIN) !== 0 && $dr <= 18 && $df <= 18)) {
                continue;
            }

            $queue = [$cell];
            $qLen = 1;
            $visitedGeo[$cell] = "\x01";
            $minX = $tx;
            $maxX = $tx;
            $minY = $ty;
            $maxY = $ty;
            $archHits = 0;

            for ($qi = 0; $qi < $qLen; $qi++) {
                $cur = $queue[$qi];
                $cx = $cur % $w;
                $cy = intdiv($cur, $w);
                if ($cx < $minX) {
                    $minX = $cx;
                }
                if ($cx > $maxX) {
                    $maxX = $cx;
                }
                if ($cy < $minY) {
                    $minY = $cy;
                }
                if ($cy > $maxY) {
                    $maxY = $cy;
                }
                if ((ord($visMask[$cur]) & $archIntMask) !== 0) {
                    $archHits++;
                }

                // 4-neighbors with strict X/Y bounds so left/right map edges never wrap across rows.
                for ($dir = 0; $dir < 4; $dir++) {
                    if ($dir === 0) {
                        if ($cx === 0) {
                            continue;
                        }
                        $nb = $cur - 1;
                    } elseif ($dir === 1) {
                        if ($cx + 1 >= $w) {
                            continue;
                        }
                        $nb = $cur + 1;
                    } elseif ($dir === 2) {
                        if ($cy === 0) {
                            continue;
                        }
                        $nb = $cur - $w;
                    } else {
                        if ($cy + 1 >= $h) {
                            continue;
                        }
                        $nb = $cur + $w;
                    }
                    if ($visitedGeo[$nb] !== "\x00" || $baseClaimed[$nb] !== "\x00") {
                        continue;
                    }
                    $ngb = $gameTiles[$nb];
                    if ($ngb === "\x01" || $ngb === "\x03" || $ngb === "\x02") {
                        continue;
                    }
                    if ($hasFront) {
                        $nfb = $frontTiles[$nb];
                        if ($nfb === "\x01" || $nfb === "\x03") {
                            continue;
                        }
                    }
                    $ndrb = $distRoof[$nb];
                    if ($ndrb === "\xff" || ord($ndrb) >= 30) {
                        continue;
                    }
                    $ndfb = $distFloor[$nb];
                    if ($ndfb === "\xff" || ord($ndfb) >= 30) {
                        continue;
                    }
                    $nvmb = $visMask[$nb];
                    if ($nvmb === "\x00" || (ord($nvmb) & $geoAnyVisMask) === 0) {
                        continue;
                    }
                    $visitedGeo[$nb] = "\x01";
                    $queue[$qLen++] = $nb;
                }
            }

            $bw = $maxX - $minX + 1;
            $bh = $maxY - $minY + 1;
            if ($qLen >= 18 && $bw >= 4 && $bh >= 3 && $bw <= $maxGeoBw) {
                $geoCandidates[] = [
                    'tx0' => $minX,
                    'ty0' => $minY,
                    'tx1' => $maxX,
                    'ty1' => $maxY,
                    'count' => $qLen,
                    'arch_hits' => $archHits,
                ];
            }
        }
    }

    usort($geoCandidates, static fn(array $a, array $b): int => $b['count'] <=> $a['count']);
    $titleMap = [
        'castle_or_guild_hall' => 'Castle / Large Hall',
        'house' => 'Building / House',
        'cave_or_underground' => 'Cave / Underground',
        'dungeon_or_ruins' => 'Ruins / Dungeon',
        'shop_or_service' => 'Service Building',
        'building' => 'Building',
    ];
    $geoPrecomputed = [];
    foreach (array_slice($geoCandidates, 0, 40) as $gc) {
        [$tx0, $ty0, $tx1, $ty1] = ai_map_expand_to_shell($w, $h, $gc['tx0'], $gc['ty0'], $gc['tx1'], $gc['ty1'], $gameTiles, 1);
        $feat = ai_map_measure_bbox_features($w, $h, $tx0, $ty0, $tx1, $ty1, $gameTiles, $frontTiles, $visMask, $distRoof, $distFloor);
        $archDensity = $gc['arch_hits'] / max(1, $gc['count']);
        if ($archDensity < 0.35 && empty($feat['special_tiles'])) {
            $kind = 'cave_or_underground';
        } else {
            $kind = ai_map_classify_structure_kind('', $feat, $tx1 - $tx0 + 1, $ty1 - $ty0 + 1);
        }
        $geoPrecomputed[] = [
            'tx0' => $tx0,
            'ty0' => $ty0,
            'tx1' => $tx1,
            'ty1' => $ty1,
            'kind' => $kind,
            'title_prefix' => $titleMap[$kind] ?? 'Structure',
            'feat' => $feat,
            'interior_tiles' => $gc['count'],
        ];
    }

    $fullBboxFeatures = ai_map_measure_bbox_features($w, $h, 0, 0, $w - 1, $h - 1, $gameTiles, $frontTiles, $visMask, $distRoof, $distFloor);

    $bundle = [
        '_cache_ver' => 3,
        'parsed' => $parsed,
        'vis' => $vis,
        'geom' => $geom,
        'stats' => [
            'non_empty_game' => $nonEmptyGame,
            'solid_count' => $solidCount,
            'death_count' => $deathCount,
            'nohook_count' => $nohookCount,
            'entity_counts' => $entityCounts,
            'entities_list' => $entitiesList,
            'spawn_points' => $spawnPoints,
            'special_counts' => $specialCounts,
            'switch_counts' => $switchCounts,
            'switch_non_empty' => $switchNonEmpty,
            'tele_counts' => $teleCounts,
            'tele_pairs' => $telePairs,
        ],
        'house_clusters' => $houseClusters,
        'zone_clusters' => $zoneClusters,
        'action_zones' => $actionZones,
        'text_signs' => $textSigns,
        'special_clusters' => $specialClusters,
        'geo_precomputed' => $geoPrecomputed,
        'full_bbox_features' => $fullBboxFeatures,
    ];

    if ($useCache) {
        $memCache[$cacheKey] = $bundle;
        $memOrder[] = $cacheKey;
        if (count($memOrder) > 4) {
            $evict = array_shift($memOrder);
            if ($evict !== null && $evict !== $cacheKey) {
                unset($memCache[$evict]);
            }
        }
        if ($diskFile !== null && $diskDir !== null) {
            if (!@is_dir($diskDir)) {
                @mkdir($diskDir, 0700, true);
            }
            if (@is_dir($diskDir) && @is_writable($diskDir)) {
                $payload = @gzcompress(serialize($bundle), 1);
                if (is_string($payload) && strlen($payload) <= 8 * 1024 * 1024) {
                    $tmpPath = $diskFile . '.tmp.' . bin2hex(random_bytes(4));
                    if (@file_put_contents($tmpPath, $payload) !== false) {
                        @rename($tmpPath, $diskFile);
                    } else {
                        @unlink($tmpPath);
                    }
                    // Keep cache directory strictly bounded (max 12 files).
                    $files = @glob($diskDir . DIRECTORY_SEPARATOR . 'static_v3_*.z');
                    if (is_array($files) && count($files) > 12) {
                        usort($files, static fn(string $a, string $b): int => (@filemtime($a) ?: 0) <=> (@filemtime($b) ?: 0));
                        foreach (array_slice($files, 0, count($files) - 12) as $oldFile) {
                            @unlink($oldFile);
                        }
                    }
                }
            }
        }
    }

    return $bundle;
}

/**
 * Full semantic map analysis: combines gameplay layers, visual layers, INFO settings,
 * vertical geometry, connected-component spatial clustering, and correlated DB objects
 * to identify buildings/structures (castles, houses, shops, dungeons, caves), zones,
 * terrain profile, entities, and optional rich point context.
 *
 * @param array<string, mixed> $options
 * @return array<string, mixed>
 */
function ai_map_analyze_full(
    string $bytes,
    ?mysqli $db = null,
    ?int $worldId = null,
    string $role = EDITOR_ROLE_ADMIN,
    ?array $guestPages = null,
    array $options = []
): array {
    $maxBytes = defined('AI_MAP_MAX_BYTES') ? AI_MAP_MAX_BYTES : (16 * 1024 * 1024);
    $maxUncompressed = defined('AI_MAP_MAX_UNCOMPRESSED') ? AI_MAP_MAX_UNCOMPRESSED : (64 * 1024 * 1024);
    $useCache = empty($options['no_cache']);

    $static = ai_map_static_analysis_get($bytes, $maxBytes, $maxUncompressed, $useCache);
    $parsed = $static['parsed'];
    $vis = $static['vis'];
    $geom = $static['geom'];
    $stats = $static['stats'];

    $w = (int)$parsed['game']['width'];
    $h = (int)$parsed['game']['height'];
    $n = $w * $h;
    $gameTiles = $parsed['game_tiles'];
    $frontTiles = $parsed['front_tiles'];
    $visMask = $vis['vis_mask'];
    $distRoof = $geom['dist_roof'];
    $distFloor = $geom['dist_floor'];

    $nonEmptyGame = $stats['non_empty_game'];
    $solidCount = $stats['solid_count'];
    $deathCount = $stats['death_count'];
    $nohookCount = $stats['nohook_count'];
    $entityCounts = $stats['entity_counts'];
    $entitiesList = $stats['entities_list'];
    $spawnPoints = $stats['spawn_points'];
    $specialCounts = $stats['special_counts'];
    $switchCounts = $stats['switch_counts'];
    $switchNonEmpty = $stats['switch_non_empty'];
    $teleCounts = $stats['tele_counts'];
    $telePairs = $stats['tele_pairs'];

    // 4. Load correlated DB world objects (always fresh & ACL-checked for the current actor).
    $worldObjects = ai_map_load_world_objects($db, $worldId, $role, $guestPages);

    // 5. Build spatial zones & merge semantic structures with live DB records.
    $zones = [];
    $actionZones = $static['action_zones'];
    $textSigns = $static['text_signs'];
    $structures = [];
    $claimedMask = str_repeat("\x00", $n);

    $markClaimed = static function (int $tx0, int $ty0, int $tx1, int $ty1) use ($w, $h, &$claimedMask): void {
        $tx0 = max(0, $tx0);
        $ty0 = max(0, $ty0);
        $tx1 = min($w - 1, $tx1);
        $ty1 = min($h - 1, $ty1);
        for ($ty = $ty0; $ty <= $ty1; $ty++) {
            $rowOff = $ty * $w;
            for ($tx = $tx0; $tx <= $tx1; $tx++) {
                $claimedMask[$rowOff + $tx] = "\x01";
            }
        }
    };

    $addStructure = static function (
        string $id,
        string $kind,
        string $name,
        string $source,
        int $tx0,
        int $ty0,
        int $tx1,
        int $ty1,
        array $features,
        array $extra = []
    ) use (&$structures, $markClaimed): void {
        $markClaimed($tx0, $ty0, $tx1, $ty1);
        $cxTile = intdiv($tx0 + $tx1, 2);
        $cyTile = intdiv($ty0 + $ty1, 2);
        $structures[] = array_merge([
            'id' => $id,
            'kind' => $kind,
            'name' => $name,
            'source' => $source,
            'bbox_tiles' => ['tx0' => $tx0, 'ty0' => $ty0, 'tx1' => $tx1, 'ty1' => $ty1],
            'bbox_px' => ['x0' => $tx0 * 32, 'y0' => $ty0 * 32, 'x1' => ($tx1 + 1) * 32, 'y1' => ($ty1 + 1) * 32],
            'width_tiles' => $tx1 - $tx0 + 1,
            'height_tiles' => $ty1 - $ty0 + 1,
            'center' => [
                'x' => $cxTile * 32 + 16,
                'y' => $cyTile * 32 + 16,
                'tile_x' => $cxTile,
                'tile_y' => $cyTile,
            ],
            'enclosed' => (bool)$features['is_enclosed'],
            'enclosed_ratio' => $features['enclosed_ratio'],
            'estimated_floors' => (int)$features['estimated_floors'],
            'special_tiles' => $features['special_tiles'],
        ], $extra);
    };

    // Index DB houses & guild houses by ID for matching against precomputed SW_HOUSE_ZONE components.
    $dbHousesById = [];
    foreach ($worldObjects['houses'] as $dh) {
        $dbHousesById[$dh['id']] = $dh;
    }
    $dbGuildById = [];
    foreach ($worldObjects['guild_houses'] as $dg) {
        $dbGuildById[$dg['id']] = $dg;
    }
    $matchedDbHouses = [];
    $matchedDbGuilds = [];

    // 5a. House & Guild Hall components from precomputed SW_HOUSE_ZONE (24) clusters.
    foreach ($static['house_clusters'] as $hc) {
        $num = $hc['num'];
        $ci = $hc['ci'];
        $tx0 = $hc['tx0'];
        $ty0 = $hc['ty0'];
        $tx1 = $hc['tx1'];
        $ty1 = $hc['ty1'];
        $feat = $hc['feat'];
        $cx = ($tx0 + $tx1) / 2.0;
        $cy = ($ty0 + $ty1) / 2.0;

        $hint = null;
        $dbRef = null;
        $label = 'House Zone #' . $num;
        $gObj = $dbGuildById[$num] ?? null;
        $hObj = $dbHousesById[$num] ?? null;
        $gDist = ($gObj !== null && $gObj['tile_x'] !== null) ? hypot($cx - $gObj['tile_x'], $cy - $gObj['tile_y']) : 1e9;
        $hDist = ($hObj !== null && $hObj['tile_x'] !== null) ? hypot($cx - $hObj['tile_x'], $cy - $hObj['tile_y']) : 1e9;

        if (isset($feat['special_tiles']['guild_house']) || isset($feat['special_tiles']['guild_chair']) || ($gDist < 45 && $gDist <= $hDist)) {
            $hint = 'guild_house';
            $label = 'Guild Hall #' . $num;
            if ($gObj !== null && $gDist < 60) {
                $matchedDbGuilds[$num] = true;
                $dbRef = ['table' => 'tw_guilds_houses', 'id' => $num, 'doors' => $gObj['doors'], 'farmzones' => $gObj['farmzones']];
            }
        } elseif ($hObj !== null && $hDist < 45) {
            $hint = 'house';
            $label = $hObj['class'] !== '' ? ($hObj['class'] . ' (House #' . $num . ')') : ('House #' . $num);
            $matchedDbHouses[$num] = true;
            $dbRef = ['table' => 'tw_houses', 'id' => $num, 'class' => $hObj['class'], 'doors' => $hObj['doors']];
        } else {
            $bw = $tx1 - $tx0 + 1;
            $bh = $ty1 - $ty0 + 1;
            if (($bw >= 26 && $bh >= 16) || ($bw * $bh >= 480) || ($bh >= 18 && $feat['estimated_floors'] >= 3)) {
                $hint = 'guild_house';
                $label = 'Guild Hall #' . $num;
            } else {
                $hint = 'house';
                $label = 'Player House #' . $num;
            }
        }

        $kind = ai_map_classify_structure_kind($label, $feat, $tx1 - $tx0 + 1, $ty1 - $ty0 + 1, $hint);
        $id = ($kind === 'castle_or_guild_hall' ? 'guild_house:' : 'house:') . $num . ($ci > 0 ? '_' . ($ci + 1) : '');
        $extra = ['house_zone_number' => $num, 'zone_tiles' => $hc['count']];
        if ($dbRef !== null) {
            $extra['db_record'] = $dbRef;
        }
        $addStructure($id, $kind, $label, 'switch_house_zone', $tx0, $ty0, $tx1, $ty1, $feat, $extra);
    }

    // Add any DB guild halls / houses not matched by SW_HOUSE_ZONE (e.g. radius-defined in DB).
    $hasUnmatchedDbHouse = false;
    foreach ($worldObjects['guild_houses'] as $dg) {
        if (isset($matchedDbGuilds[$dg['id']]) || $dg['tile_x'] === null || $dg['tile_y'] === null) {
            continue;
        }
        $hasUnmatchedDbHouse = true;
        $radTiles = max(8, min(40, $dg['radius_px'] ? intdiv((int)$dg['radius_px'], 32) : 14));
        $tx0 = max(0, $dg['tile_x'] - $radTiles);
        $ty0 = max(0, $dg['tile_y'] - intdiv($radTiles, 2));
        $tx1 = min($w - 1, $dg['tile_x'] + $radTiles);
        $ty1 = min($h - 1, $dg['tile_y'] + intdiv($radTiles, 2));
        $feat = ai_map_measure_bbox_features($w, $h, $tx0, $ty0, $tx1, $ty1, $gameTiles, $frontTiles, $visMask, $distRoof, $distFloor);
        $addStructure('guild_house:' . $dg['id'], 'castle_or_guild_hall', 'Guild Hall #' . $dg['id'], 'db_guilds_houses', $tx0, $ty0, $tx1, $ty1, $feat, [
            'db_record' => ['table' => 'tw_guilds_houses', 'id' => $dg['id'], 'doors' => $dg['doors'], 'farmzones' => $dg['farmzones']],
        ]);
    }
    foreach ($worldObjects['houses'] as $dh) {
        if (isset($matchedDbHouses[$dh['id']]) || $dh['tile_x'] === null || $dh['tile_y'] === null) {
            continue;
        }
        $hasUnmatchedDbHouse = true;
        $tx0 = max(0, $dh['tile_x'] - 8);
        $ty0 = max(0, $dh['tile_y'] - 6);
        $tx1 = min($w - 1, $dh['tile_x'] + 8);
        $ty1 = min($h - 1, $dh['tile_y'] + 6);
        $feat = ai_map_measure_bbox_features($w, $h, $tx0, $ty0, $tx1, $ty1, $gameTiles, $frontTiles, $visMask, $distRoof, $distFloor);
        $label = $dh['class'] !== '' ? ($dh['class'] . ' (House #' . $dh['id'] . ')') : ('House #' . $dh['id']);
        $addStructure('house:' . $dh['id'], 'house', $label, 'db_houses', $tx0, $ty0, $tx1, $ty1, $feat, [
            'db_record' => ['table' => 'tw_houses', 'id' => $dh['id'], 'class' => $dh['class'], 'doors' => $dh['doors']],
        ]);
    }

    // 5b. Named #zone components (SW_ZONE = 22).
    foreach ($static['zone_clusters'] as $zc) {
        $num = $zc['num'];
        $ci = $zc['ci'];
        $zName = $zc['name'];
        $pvp = $zc['pvp'];
        $tx0 = $zc['tx0'];
        $ty0 = $zc['ty0'];
        $tx1 = $zc['tx1'];
        $ty1 = $zc['ty1'];
        $feat = $zc['feat'];
        $cx = intdiv($tx0 + $tx1, 2);
        $cy = intdiv($ty0 + $ty1, 2);
        $zones[] = [
            'number' => $num,
            'component' => $ci + 1,
            'name' => $zName,
            'pvp' => $pvp,
            'tiles_count' => $zc['count'],
            'bbox_tiles' => ['tx0' => $tx0, 'ty0' => $ty0, 'tx1' => $tx1, 'ty1' => $ty1],
            'bbox_px' => ['x0' => $tx0 * 32, 'y0' => $ty0 * 32, 'x1' => ($tx1 + 1) * 32, 'y1' => ($ty1 + 1) * 32],
            'center' => ['x' => $cx * 32 + 16, 'y' => $cy * 32 + 16, 'tile_x' => $cx, 'tile_y' => $cy],
            'enclosed' => $feat['is_enclosed'],
            'has_architecture' => $feat['has_architecture'],
            'environment' => $feat['is_enclosed'] ? ($feat['has_architecture'] ? 'interior_building' : 'cave_or_sheltered') : 'outdoor_area',
        ];

        if ($zc['shell_bbox'] !== null) {
            $claimed = 0;
            $sample = 0;
            for ($sy = $ty0; $sy <= $ty1; $sy += 2) {
                $rowOff = $sy * $w;
                for ($sx = $tx0; $sx <= $tx1; $sx += 2) {
                    $sample++;
                    if ($claimedMask[$rowOff + $sx] !== "\x00") {
                        $claimed++;
                    }
                }
            }
            if ($sample === 0 || ($claimed / $sample) < 0.70) {
                [$etx0, $ety0, $etx1, $ety1] = $zc['shell_bbox'];
                $kind = ai_map_classify_structure_kind($zName, $feat, $etx1 - $etx0 + 1, $ety1 - $ety0 + 1);
                $sid = 'zone:' . $num . ($ci > 0 ? '_' . ($ci + 1) : '');
                $addStructure($sid, $kind, $zName, 'switch_zone', $etx0, $ety0, $etx1, $ety1, $feat, [
                    'zone_number' => $num,
                    'pvp' => $pvp,
                ]);
            }
        }
    }

    // 5d. Functional special-tile buildings not yet claimed.
    foreach ($static['special_clusters'] as $sc) {
        if ($hasUnmatchedDbHouse) {
            [$stx, $sty] = $sc['sample_cell'];
            if ($claimedMask[$sty * $w + $stx] !== "\x00") {
                continue;
            }
        }
        $addStructure(
            'special:' . $sc['spec_key'] . '_' . ($sc['ci'] + 1),
            'shop_or_service',
            $sc['default_title'] . ($sc['ci'] > 0 ? ' #' . ($sc['ci'] + 1) : ''),
            'special_tile_' . $sc['spec_key'],
            $sc['tx0'],
            $sc['ty0'],
            $sc['tx1'],
            $sc['ty1'],
            $sc['feat']
        );
    }

    // 5e. Unclaimed geometric/architectural buildings & caves from precomputed BFS.
    $geoIdx = 0;
    foreach ($static['geo_precomputed'] as $gc) {
        if ($hasUnmatchedDbHouse) {
            $cx = intdiv($gc['tx0'] + $gc['tx1'], 2);
            $cy = intdiv($gc['ty0'] + $gc['ty1'], 2);
            if ($claimedMask[$cy * $w + $cx] !== "\x00") {
                continue;
            }
        }
        $geoIdx++;
        $addStructure('struct:' . $geoIdx, $gc['kind'], $gc['title_prefix'] . ' #' . $geoIdx, 'geometry_and_visuals', $gc['tx0'], $gc['ty0'], $gc['tx1'], $gc['ty1'], $gc['feat'], [
            'interior_tiles' => $gc['interior_tiles'],
        ]);
    }

    // 5f. Attach nearby DB objects (warehouses, NPCs, aethers, quest boards, dungeons, world swaps) to structures.
    $allPointObjects = [];
    foreach ($worldObjects['warehouses'] as $o) {
        $allPointObjects[] = ['type' => 'warehouse', 'id' => $o['id'], 'name' => $o['name'], 'x' => $o['x'], 'y' => $o['y'], 'tile_x' => $o['tile_x'], 'tile_y' => $o['tile_y']];
    }
    foreach ($worldObjects['aethers'] as $o) {
        $allPointObjects[] = ['type' => 'aether', 'id' => $o['id'], 'name' => $o['name'], 'x' => $o['x'], 'y' => $o['y'], 'tile_x' => $o['tile_x'], 'tile_y' => $o['tile_y']];
    }
    foreach ($worldObjects['quest_boards'] as $o) {
        $allPointObjects[] = ['type' => 'quest_board', 'id' => $o['id'], 'name' => $o['name'], 'x' => $o['x'], 'y' => $o['y'], 'tile_x' => $o['tile_x'], 'tile_y' => $o['tile_y']];
    }
    foreach ($worldObjects['dungeons'] as $o) {
        $allPointObjects[] = ['type' => 'dungeon_door', 'id' => $o['id'], 'name' => 'Dungeon #' . $o['id'] . ' (Lv.' . $o['level'] . ')', 'x' => $o['x'], 'y' => $o['y'], 'tile_x' => $o['tile_x'], 'tile_y' => $o['tile_y']];
    }
    foreach ($worldObjects['world_swaps'] as $o) {
        $allPointObjects[] = ['type' => 'world_swap', 'id' => $o['id'], 'name' => 'Portal -> World #' . ($o['target_world_id'] ?? '?'), 'x' => $o['x'], 'y' => $o['y'], 'tile_x' => $o['tile_x'], 'tile_y' => $o['tile_y']];
    }
    foreach ($worldObjects['npcs'] as $o) {
        $allPointObjects[] = ['type' => 'npc', 'id' => $o['id'], 'name' => $o['name'], 'x' => $o['x'], 'y' => $o['y'], 'tile_x' => $o['tile_x'], 'tile_y' => $o['tile_y']];
    }

    foreach ($structures as &$st) {
        $b = $st['bbox_tiles'];
        $linked = [];
        foreach ($allPointObjects as $obj) {
            if ($obj['tile_x'] >= $b['tx0'] - 3 && $obj['tile_x'] <= $b['tx1'] + 3 && $obj['tile_y'] >= $b['ty0'] - 3 && $obj['tile_y'] <= $b['ty1'] + 3) {
                $linked[] = ['type' => $obj['type'], 'id' => $obj['id'], 'name' => $obj['name'], 'x' => $obj['x'], 'y' => $obj['y']];
                if (count($linked) >= 12) {
                    break;
                }
            }
        }
        if ($linked !== []) {
            $st['linked_objects'] = $linked;
        }
    }
    unset($st);

    // Count structures by kind.
    $structuresByKind = [];
    foreach ($structures as $st) {
        $k = $st['kind'];
        $structuresByKind[$k] = ($structuresByKind[$k] ?? 0) + 1;
    }

    // 6. Optional rich point check (pixel x,y or tile tx,ty).
    $probeX = isset($options['x']) && $options['x'] !== null ? (int)$options['x'] : null;
    $probeY = isset($options['y']) && $options['y'] !== null ? (int)$options['y'] : null;
    $pointCheck = null;
    if ($probeX !== null && $probeY !== null) {
        $pointCheck = ai_map_inspect_point(
            $bytes,
            $parsed,
            $visMask,
            $distRoof,
            $distFloor,
            $structures,
            $zones,
            $actionZones,
            $textSigns,
            $allPointObjects,
            $probeX,
            $probeY,
            $maxUncompressed
        );
    }

    // 7. Optional region analysis + semantic ASCII minimap.
    $fullBboxFeatures = $static['full_bbox_features'] ?? null;
    $regionResult = null;
    if (!empty($options['region']) && is_array($options['region'])) {
        $regionResult = ai_map_analyze_region_box(
            $parsed,
            $visMask,
            $distRoof,
            $distFloor,
            $structures,
            $zones,
            $entitiesList,
            $allPointObjects,
            $options['region'],
            $fullBboxFeatures
        );
    } elseif (!empty($options['viewport_params']) && is_array($options['viewport_params'])) {
        $stubForResolve = [
            'width_tiles' => $w,
            'height_tiles' => $h,
            'game_layer' => ['spawn_points' => $spawnPoints],
            'structures' => ['items' => $structures],
            'zones' => ['named_zones' => $zones],
            'world_objects' => $worldObjects,
        ];
        $pad = isset($options['pad_tiles']) ? max(0, min(40, (int)$options['pad_tiles'])) : 4;
        $vp = ai_map_resolve_viewport($stubForResolve, $options['viewport_params'], $pad);
        $regionResult = ai_map_analyze_region_box(
            $parsed,
            $visMask,
            $distRoof,
            $distFloor,
            $structures,
            $zones,
            $entitiesList,
            $allPointObjects,
            $vp,
            $fullBboxFeatures
        );
        $regionResult['label'] = $vp['label'];
        $regionResult['target_resolved'] = $vp['target_resolved'];
    } elseif (!empty($options['include_minimap'])) {
        $regionResult = ai_map_analyze_region_box(
            $parsed,
            $visMask,
            $distRoof,
            $distFloor,
            $structures,
            $zones,
            $entitiesList,
            $allPointObjects,
            ['tx0' => 0, 'ty0' => 0, 'tx1' => $w - 1, 'ty1' => $h - 1],
            $fullBboxFeatures
        );
    }

    // Clean summary of images & groups without internal file paths.
    $imagesSummary = array_map(static fn(array $img): array => [
        'index' => $img['index'],
        'name' => $img['name'],
        'width' => $img['width'],
        'height' => $img['height'],
        'external' => $img['external'],
    ], $parsed['images']);

    $groupsSummary = array_map(static fn(array $g): array => [
        'index' => $g['index'],
        'name' => $g['name'],
        'parallax' => [$g['parallax_x'], $g['parallax_y']],
        'offset' => [$g['offset_x'], $g['offset_y']],
        'num_layers' => $g['num_layers'],
    ], $parsed['groups']);

    // Build concise human-readable narrative overview for AI.
    $overviewParts = [
        sprintf('Карта %dx%d тайлов (%dx%d px)', $w, $h, $w * 32, $h * 32),
        sprintf('слоёв: %d (визуальных тайловых: %d, quad: %d)', $parsed['layers_count'], count($vis['visual_layers']), count($vis['quad_layers'])),
    ];
    if ($structures !== []) {
        $kindLabels = [];
        foreach ($structuresByKind as $k => $cnt) {
            $kindLabels[] = "{$k}: {$cnt}";
        }
        $overviewParts[] = 'распознано построек/структур: ' . count($structures) . ' (' . implode(', ', $kindLabels) . ')';
    } else {
        $overviewParts[] = 'крытых построек не обнаружено (преимущественно открытый ландшафт)';
    }
    if ($zones !== []) {
        $zUnique = array_values(array_unique(array_map(static fn(array $z): string => $z['name'], $zones)));
        $overviewParts[] = 'именованные зоны: ' . implode(', ', array_slice($zUnique, 0, 12));
    }

    $includeEntities = !empty($options['include_entities']);
    $maxEntities = max(1, min(2000, (int)($options['max_entities'] ?? 500)));

    $summary = [
        // Backwards-compatible fields matching ai_map_parse_datafile()
        'format' => 'DDNet v4',
        'bytes' => $parsed['bytes'],
        'sha256' => $parsed['sha256'],
        'width_tiles' => $w,
        'height_tiles' => $h,
        'width_px' => $w * 32,
        'height_px' => $h * 32,
        'tile_size_px' => 32,
        'layers_count' => $parsed['layers_count'],
        'game_layer' => [
            'non_empty_tiles' => $nonEmptyGame,
            'solid_tiles' => $solidCount,
            'death_tiles' => $deathCount,
            'nohook_tiles' => $nohookCount,
            'special_tile_counts' => $specialCounts,
            'entity_counts' => $entityCounts,
            'entities_total' => array_sum($entityCounts),
            'spawn_points' => array_slice($spawnPoints, 0, 20),
        ],
        'front_layer' => [
            'present' => $parsed['front'] !== null,
        ],
        'switch_layer' => [
            'present' => $parsed['switch'] !== null,
            'non_empty_tiles' => $switchNonEmpty,
            'type_counts' => $switchCounts,
        ],
        'tele_layer' => [
            'present' => $parsed['tele'] !== null,
            'type_counts' => $teleCounts,
            'teleporters' => array_values(array_slice($telePairs, 0, 60)),
        ],
        'speedup_layer' => [
            'present' => $parsed['speedup'] !== null,
        ],
        // Rich semantic & visual map data
        'overview_text' => implode('; ', $overviewParts) . '.',
        'map_info' => [
            'author' => $parsed['info']['author'],
            'map_version' => $parsed['info']['map_version'],
            'credits' => $parsed['info']['credits'],
            'license' => $parsed['info']['license'],
            'settings_count' => count($parsed['info']['settings_lines'] ?? []),
        ],
        'groups' => $groupsSummary,
        'images' => $imagesSummary,
        'visual_layers' => [
            'tile_layers_count' => count($vis['visual_layers']),
            'quad_layers_count' => count($vis['quad_layers']),
            'category_counts' => $vis['category_counts'],
            'tile_layers' => $vis['visual_layers'],
            'quad_layers' => $vis['quad_layers'],
        ],
        'terrain' => [
            'composition' => $geom['composition'],
            'elevation_profile' => $geom['terrain_profile'],
        ],
        'structures' => [
            'total' => count($structures),
            'by_kind' => $structuresByKind,
            'items' => $structures,
        ],
        'zones' => [
            'named_zones' => $zones,
            'action_zones' => $actionZones,
            'text_signs' => array_slice($textSigns, 0, 60),
        ],
        'world_objects' => $worldObjects,
    ];

    if ($includeEntities) {
        $summary['game_layer']['entities'] = array_slice($entitiesList, 0, $maxEntities);
        $summary['game_layer']['entities_truncated'] = count($entitiesList) > $maxEntities;
    }
    if ($pointCheck !== null) {
        $summary['point_check'] = $pointCheck;
    }
    if ($regionResult !== null) {
        $summary['region_analysis'] = $regionResult;
    }

    return $summary;
}

/**
 * Inspect a single pixel coordinate (x, y) on the map and return full collision,
 * roof/floor enclosure, environment classification, overlapping visual layers,
 * active zones, enclosing structure, nearby structures/objects, and nearest standable point.
 *
 * @return array<string, mixed>
 */
function ai_map_inspect_point(
    string $bytes,
    array $parsed,
    string $visMask,
    string $distRoof,
    string $distFloor,
    array $structures,
    array $zones,
    array $actionZones,
    array $textSigns,
    array $allPointObjects,
    int $probeX,
    int $probeY,
    int $maxUncompressed
): array {
    $w = (int)$parsed['game']['width'];
    $h = (int)$parsed['game']['height'];
    $tx = intdiv($probeX, 32);
    $ty = intdiv($probeY, 32);
    $inBounds = $tx >= 0 && $ty >= 0 && $tx < $w && $ty < $h;

    if (!$inBounds) {
        return [
            'x' => $probeX,
            'y' => $probeY,
            'tile_x' => $tx,
            'tile_y' => $ty,
            'in_bounds' => false,
            'safe_for_placement' => false,
            'terrain_context' => ['environment' => 'out_of_bounds', 'standable' => false],
        ];
    }

    $defs = ai_schema_map_definitions();
    $entityNames = $defs['game_entity_tiles'];
    $switchNames = $defs['switch_tiles'];
    $teleCatalog = ai_map_tele_tile_catalog();
    $settings = $parsed['info']['settings'] ?? ['zones' => [], 'texts' => [], 'action_zones' => []];

    $cell = $ty * $w + $tx;
    $g = ord($parsed['game_tiles'][$cell]);
    $hasFront = ($parsed['front_tiles'] !== null && strlen($parsed['front_tiles']) >= $w * $h);
    $f = $hasFront ? ord($parsed['front_tiles'][$cell]) : 0;
    $solid = ($g === 1 || $g === 3 || $f === 1 || $f === 3);
    $death = ($g === 2 || $f === 2);
    $nohook = ($g === 3 || $f === 3);

    $swInfo = null;
    if ($parsed['switch_tiles'] !== null && $parsed['switch'] !== null) {
        $swW = (int)$parsed['switch']['width'];
        $swH = (int)$parsed['switch']['height'];
        if ($tx < $swW && $ty < $swH) {
            $si = ($ty * $swW + $tx) * 4;
            $st = ord($parsed['switch_tiles'][$si + 1]);
            if ($st !== 0) {
                $sNum = ord($parsed['switch_tiles'][$si]);
                $swInfo = [
                    'type' => $st,
                    'name' => $switchNames[$st] ?? ('switch_' . $st),
                    'number' => $sNum,
                    'delay' => ord($parsed['switch_tiles'][$si + 3]),
                ];
                if ($st === 22 && isset($settings['zones'][$sNum])) {
                    $swInfo['zone_name'] = $settings['zones'][$sNum]['name'];
                    $swInfo['pvp'] = $settings['zones'][$sNum]['pvp'];
                } elseif ($st === 25 && isset($settings['action_zones'][$sNum])) {
                    $swInfo['action_zone_name'] = $settings['action_zones'][$sNum]['name'];
                } elseif ($st === 23 && isset($settings['texts'][$sNum])) {
                    $swInfo['text'] = $settings['texts'][$sNum]['text'];
                }
            }
        }
    }

    $teleInfo = null;
    if ($parsed['tele_tiles'] !== null && $parsed['tele'] !== null) {
        $tW = (int)$parsed['tele']['width'];
        $tH = (int)$parsed['tele']['height'];
        if ($tx < $tW && $ty < $tH) {
            $ti = ($ty * $tW + $tx) * 2;
            $tt = ord($parsed['tele_tiles'][$ti + 1]);
            if ($tt !== 0) {
                $teleInfo = [
                    'number' => ord($parsed['tele_tiles'][$ti]),
                    'type' => $tt,
                    'name' => $teleCatalog[$tt] ?? ('tele_' . $tt),
                ];
            }
        }
    }

    // Visual layers actually painted at (tx, ty).
    $visualAtPoint = [];
    $gameLayerIdx = (int)($parsed['game']['layer_index'] ?? 0);
    $tables = $parsed['tables'];
    foreach ($parsed['layers'] as $layer) {
        if (($layer['role'] ?? '') !== 'visual_tiles') {
            continue;
        }
        $cls = ai_map_classify_visual_layer($layer);
        if (!$cls['world_aligned']) {
            continue;
        }
        $lw = (int)$layer['width'];
        $lh = (int)$layer['height'];
        $ltx = $tx - (int)round(((int)$layer['offset_x']) / 32.0);
        $lty = $ty - (int)round(((int)$layer['offset_y']) / 32.0);
        if ($ltx < 0 || $lty < 0 || $ltx >= $lw || $lty >= $lh) {
            continue;
        }
        $plain = ddnet_read_data_block($bytes, $tables, (int)$layer['data_index'], $maxUncompressed);
        if (!is_string($plain) || $plain === '') {
            continue;
        }
        $hitTile = 0;
        ddnet_iter_tile_layer_nonzero($plain, $lw, $lh, (int)$layer['tile_version'], static function (int $x, int $y, int $idx) use (&$hitTile): void {
            $hitTile = $idx;
        }, $ltx, $lty, $ltx, $lty);
        if ($hitTile !== 0) {
            $visualAtPoint[] = [
                'layer' => (string)$layer['name'],
                'group' => (string)$layer['group_name'],
                'image' => $layer['image_name'],
                'category' => $cls['category'],
                'depth' => ((int)$layer['index'] < $gameLayerIdx) ? 'background' : 'foreground',
                'tile_id' => $hitTile,
            ];
            if (count($visualAtPoint) >= 16) {
                break;
            }
        }
    }

    // Enclosing & nearby structures.
    $insideStructure = null;
    $nearbyStructures = [];
    foreach ($structures as $st) {
        $b = $st['bbox_tiles'];
        $dx = max(0, $b['tx0'] - $tx, $tx - $b['tx1']);
        $dy = max(0, $b['ty0'] - $ty, $ty - $b['ty1']);
        $dist = round(hypot($dx, $dy), 1);
        if ($dx === 0 && $dy === 0 && $insideStructure === null) {
            $insideStructure = [
                'id' => $st['id'],
                'kind' => $st['kind'],
                'name' => $st['name'],
                'source' => $st['source'],
                'bbox_tiles' => $b,
                'estimated_floors' => $st['estimated_floors'],
            ];
        } else {
            $nearbyStructures[] = [
                'id' => $st['id'],
                'kind' => $st['kind'],
                'name' => $st['name'],
                'distance_tiles' => $dist,
                'direction' => ai_map_cardinal_direction($tx, $ty, $st['center']['tile_x'], $st['center']['tile_y']),
                'center' => $st['center'],
            ];
        }
    }
    usort($nearbyStructures, static fn(array $a, array $b): int => $a['distance_tiles'] <=> $b['distance_tiles']);
    $nearbyStructures = array_slice($nearbyStructures, 0, 5);

    // Nearby DB world objects.
    $nearbyObjects = [];
    foreach ($allPointObjects as $obj) {
        $d = round(hypot($obj['tile_x'] - $tx, $obj['tile_y'] - $ty), 1);
        if ($d <= 48.0) {
            $nearbyObjects[] = [
                'type' => $obj['type'],
                'id' => $obj['id'],
                'name' => $obj['name'],
                'x' => $obj['x'],
                'y' => $obj['y'],
                'distance_tiles' => $d,
                'direction' => ai_map_cardinal_direction($tx, $ty, $obj['tile_x'], $obj['tile_y']),
            ];
        }
    }
    usort($nearbyObjects, static fn(array $a, array $b): int => $a['distance_tiles'] <=> $b['distance_tiles']);
    $nearbyObjects = array_slice($nearbyObjects, 0, 8);

    // Environment classification.
    $vm = ord($visMask[$cell]);
    $dr = ord($distRoof[$cell]);
    $df = ord($distFloor[$cell]);
    $hasRoof = !$solid && $dr < 36;
    $hasFloor = !$solid && $df < 255;

    if ($death) {
        $env = 'hazard';
    } elseif ($solid) {
        $inBuiltStructure = $insideStructure !== null && ($insideStructure['kind'] ?? '') !== 'cave_or_underground';
        $env = ($inBuiltStructure || (($vm & (AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR)) !== 0 && ($vm & AI_VIS_TERRAIN) === 0))
            ? 'solid_building_wall'
            : 'solid_terrain';
    } elseif ($g === 6 || $f === 6 || (($vm & AI_VIS_WATER) !== 0 && ($vm & (AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR)) === 0)) {
        $env = 'water';
    } elseif ($hasRoof && $df < 36) {
        $isArch = $insideStructure !== null
            || ($vm & (AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR | AI_VIS_BG_VISUAL)) !== 0;
        $onlyTerrain = ($vm & AI_VIS_TERRAIN) !== 0 && ($vm & (AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR)) === 0 && $insideStructure === null;
        if ($onlyTerrain) {
            $env = 'underground_cave';
        } elseif ($isArch) {
            $env = 'interior_building';
        } else {
            $env = 'sheltered_overhang';
        }
    } elseif (!$hasFloor) {
        $env = 'void';
    } elseif ($df <= 2) {
        $env = 'outdoor_ground_surface';
    } else {
        $env = 'outdoor_air';
    }

    $standable = !$solid && !$death && $g !== 6 && $df >= 1 && $df <= 2;
    $nearestStandable = $standable
        ? ['x' => $tx * 32 + 16, 'y' => $ty * 32 + 16, 'tile_x' => $tx, 'tile_y' => $ty, 'distance_tiles' => 0.0]
        : ai_map_find_nearest_standable($w, $h, $tx, $ty, $parsed['game_tiles'], $parsed['front_tiles'], $distFloor, 24);

    // Active zones covering this point.
    $zonesAtPoint = [];
    foreach ($zones as $z) {
        $b = $z['bbox_tiles'];
        if ($tx >= $b['tx0'] && $tx <= $b['tx1'] && $ty >= $b['ty0'] && $ty <= $b['ty1']) {
            $zonesAtPoint[] = ['type' => 'zone', 'number' => $z['number'], 'name' => $z['name'], 'pvp' => $z['pvp']];
        }
    }
    foreach ($actionZones as $az) {
        $b = $az['bbox_tiles'];
        if ($tx >= $b['tx0'] && $tx <= $b['tx1'] && $ty >= $b['ty0'] && $ty <= $b['ty1']) {
            $zonesAtPoint[] = ['type' => 'action_zone', 'number' => $az['number'], 'name' => $az['name']];
        }
    }

    return [
        // Backwards-compatible keys
        'x' => $probeX,
        'y' => $probeY,
        'tile_x' => $tx,
        'tile_y' => $ty,
        'in_bounds' => true,
        'game_tile' => $g,
        'game_tile_name' => ai_map_tile_name($g),
        'solid' => $solid,
        'death' => $death,
        'nohook' => $nohook,
        'entity_at_point' => $g >= 191 ? ($entityNames[$g] ?? ('entity_' . $g)) : null,
        'switch_at_point' => $swInfo,
        'safe_for_placement' => !$solid && !$death,
        // New rich semantic context
        'front_tile' => $f,
        'front_tile_name' => $f !== 0 ? ai_map_tile_name($f) : null,
        'tele_at_point' => $teleInfo,
        'terrain_context' => [
            'environment' => $env,
            'has_roof_above' => $hasRoof,
            'tiles_to_roof' => (!$solid && $dr < 255) ? $dr : null,
            'has_floor_below' => $hasFloor,
            'tiles_to_floor' => (!$solid && $df < 255) ? $df : null,
            'standable' => $standable,
            'nearest_standable_point' => $nearestStandable,
        ],
        'visual_layers_at_point' => $visualAtPoint,
        'zones_at_point' => $zonesAtPoint,
        'inside_structure' => $insideStructure,
        'nearby_structures' => $nearbyStructures,
        'nearby_objects' => $nearbyObjects,
    ];
}

/** Find nearest standable tile (open air 1 tile directly above solid ground, not death/water). */
function ai_map_find_nearest_standable(
    int $w,
    int $h,
    int $tx,
    int $ty,
    string $gameTiles,
    ?string $frontTiles,
    string $distFloor,
    int $maxRadius = 24
): ?array {
    $hasFront = ($frontTiles !== null && strlen($frontTiles) >= $w * $h);
    $best = null;
    $bestD2 = PHP_INT_MAX;

    $x0 = max(0, $tx - $maxRadius);
    $x1 = min($w - 1, $tx + $maxRadius);
    $y0 = max(0, $ty - $maxRadius);
    $y1 = min($h - 1, $ty + $maxRadius);

    for ($y = $y0; $y <= $y1; $y++) {
        $dy = $y - $ty;
        $dy2 = $dy * $dy;
        if ($dy2 >= $bestD2) {
            continue;
        }
        for ($x = $x0; $x <= $x1; $x++) {
            $dx = $x - $tx;
            $d2 = $dx * $dx + $dy2;
            if ($d2 >= $bestD2) {
                continue;
            }
            $cell = $y * $w + $x;
            if (ord($distFloor[$cell]) !== 1) {
                continue;
            }
            $g = ord($gameTiles[$cell]);
            $f = $hasFront ? ord($frontTiles[$cell]) : 0;
            if ($g === 1 || $g === 2 || $g === 3 || $g === 6 || $f === 1 || $f === 2 || $f === 3 || $f === 6) {
                continue;
            }
            $bestD2 = $d2;
            $best = [
                'x' => $x * 32 + 16,
                'y' => $y * 32 + 16,
                'tile_x' => $x,
                'tile_y' => $y,
                'distance_tiles' => round(sqrt($d2), 1),
            ];
        }
    }
    return $best;
}

function ai_map_cardinal_direction(int $fromX, int $fromY, int $toX, int $toY): string
{
    $dx = $toX - $fromX;
    $dy = $toY - $fromY;
    if ($dx === 0 && $dy === 0) {
        return 'here';
    }
    $ns = $dy < -2 ? 'north' : ($dy > 2 ? 'south' : '');
    $ew = $dx < -2 ? 'west' : ($dx > 2 ? 'east' : '');
    if ($ns !== '' && $ew !== '') {
        return $ns . '-' . $ew;
    }
    return $ns !== '' ? $ns : ($ew !== '' ? $ew : 'nearby');
}

/**
 * Resolve a high-level target specifier (overview, spawn, structure:<id>, house:<id>,
 * guild_house:<id>, zone:<num>, warehouse:<id>, x/y/radius, x0..y1, tx0..ty1) to a clamped
 * tile bounding box [tx0, ty0, tx1, ty1].
 *
 * @param array<string, mixed> $analysis Full analysis result from ai_map_analyze_full()
 * @param array<string, mixed> $params   Request parameters
 * @return array{tx0:int,ty0:int,tx1:int,ty1:int,label:string,target_resolved:string}
 */
function ai_map_resolve_viewport(array $analysis, array $params, int $padTiles = 6): array
{
    $w = max(1, (int)$analysis['width_tiles']);
    $h = max(1, (int)$analysis['height_tiles']);
    $clamp = static function (int $tx0, int $ty0, int $tx1, int $ty1, string $label, string $resolved) use ($w, $h): array {
        $tx0 = max(0, min($w - 1, $tx0));
        $ty0 = max(0, min($h - 1, $ty0));
        $tx1 = max($tx0, min($w - 1, $tx1));
        $ty1 = max($ty0, min($h - 1, $ty1));
        return [
            'tx0' => $tx0,
            'ty0' => $ty0,
            'tx1' => $tx1,
            'ty1' => $ty1,
            'label' => $label,
            'target_resolved' => $resolved,
        ];
    };

    // 1. Explicit tile bounds tx0..ty1.
    if (isset($params['tx0'], $params['ty0'], $params['tx1'], $params['ty1'])) {
        return $clamp((int)$params['tx0'], (int)$params['ty0'], (int)$params['tx1'], (int)$params['ty1'], 'Custom tile region', 'tiles_bbox');
    }

    // 2. Explicit pixel bounds x0..y1.
    if (isset($params['x0'], $params['y0'], $params['x1'], $params['y1'])) {
        return $clamp(
            intdiv((int)$params['x0'], 32),
            intdiv((int)$params['y0'], 32),
            intdiv((int)$params['x1'], 32),
            intdiv((int)$params['y1'], 32),
            'Custom pixel region',
            'px_bbox'
        );
    }

    // 3. Center point (x, y) + radius (tiles or pixels).
    if (isset($params['x'], $params['y']) && $params['x'] !== null && $params['y'] !== null) {
        $cx = intdiv((int)$params['x'], 32);
        $cy = intdiv((int)$params['y'], 32);
        $radTiles = isset($params['radius_tiles'])
            ? max(4, min(200, (int)$params['radius_tiles']))
            : (isset($params['radius']) ? max(4, min(200, intdiv((int)$params['radius'], 32))) : 28);
        return $clamp($cx - $radTiles, $cy - $radTiles, $cx + $radTiles, $cy + $radTiles, "Area around ({$params['x']}, {$params['y']})", 'point_radius');
    }

    // 4. Semantic target string.
    $target = trim((string)($params['target'] ?? 'overview'));
    $low = strtolower($target);

    if ($low === '' || $low === 'overview' || $low === 'full') {
        return $clamp(0, 0, $w - 1, $h - 1, 'Full map overview', 'overview');
    }

    if ($low === 'spawn') {
        $spawns = $analysis['game_layer']['spawn_points'] ?? [];
        if ($spawns !== []) {
            $cx = (int)$spawns[0]['tile_x'];
            $cy = (int)$spawns[0]['tile_y'];
            return $clamp($cx - 32, $cy - 20, $cx + 32, $cy + 20, 'Spawn area', 'spawn');
        }
        return $clamp(0, 0, $w - 1, $h - 1, 'Full map overview (no spawn tile)', 'overview');
    }

    // Match structure ID directly (e.g. "house:1", "guild_house:1", "zone:3", "struct:1", "structure:house:1").
    $normTarget = str_starts_with($low, 'structure:') ? substr($low, 10) : $low;
    foreach ($analysis['structures']['items'] ?? [] as $st) {
        if (strtolower((string)$st['id']) === $normTarget || strtolower((string)$st['name']) === $normTarget) {
            $b = $st['bbox_tiles'];
            return $clamp($b['tx0'] - $padTiles, $b['ty0'] - $padTiles, $b['tx1'] + $padTiles, $b['ty1'] + $padTiles, (string)$st['name'], 'structure:' . $st['id']);
        }
    }

    // Match zone:<num> or zone:<name>
    if (str_starts_with($low, 'zone:')) {
        $zKey = trim(substr($target, 5));
        foreach ($analysis['zones']['named_zones'] ?? [] as $z) {
            if ((string)$z['number'] === $zKey || strcasecmp((string)$z['name'], $zKey) === 0) {
                $b = $z['bbox_tiles'];
                return $clamp($b['tx0'] - $padTiles, $b['ty0'] - $padTiles, $b['tx1'] + $padTiles, $b['ty1'] + $padTiles, 'Zone: ' . $z['name'], 'zone:' . $z['number']);
            }
        }
    }

    // Match warehouse:<id> or aether:<id>
    if (str_starts_with($low, 'warehouse:')) {
        $wid = (int)substr($target, 10);
        foreach ($analysis['world_objects']['warehouses'] ?? [] as $wh) {
            if ((int)$wh['id'] === $wid) {
                return $clamp($wh['tile_x'] - 24, $wh['tile_y'] - 16, $wh['tile_x'] + 24, $wh['tile_y'] + 16, 'Warehouse: ' . $wh['name'], 'warehouse:' . $wid);
            }
        }
    }
    if (str_starts_with($low, 'aether:')) {
        $aid = (int)substr($target, 7);
        foreach ($analysis['world_objects']['aethers'] ?? [] as $ae) {
            if ((int)$ae['id'] === $aid) {
                return $clamp($ae['tile_x'] - 24, $ae['tile_y'] - 16, $ae['tile_x'] + 24, $ae['tile_y'] + 16, 'Aether: ' . $ae['name'], 'aether:' . $aid);
            }
        }
    }

    // Partial name match across structures and zones.
    foreach ($analysis['structures']['items'] ?? [] as $st) {
        if (str_contains(strtolower((string)$st['name']), $normTarget)) {
            $b = $st['bbox_tiles'];
            return $clamp($b['tx0'] - $padTiles, $b['ty0'] - $padTiles, $b['tx1'] + $padTiles, $b['ty1'] + $padTiles, (string)$st['name'], 'structure:' . $st['id']);
        }
    }
    foreach ($analysis['zones']['named_zones'] ?? [] as $z) {
        if (str_contains(strtolower((string)$z['name']), $normTarget)) {
            $b = $z['bbox_tiles'];
            return $clamp($b['tx0'] - $padTiles, $b['ty0'] - $padTiles, $b['tx1'] + $padTiles, $b['ty1'] + $padTiles, 'Zone: ' . $z['name'], 'zone:' . $z['number']);
        }
    }

    return $clamp(0, 0, $w - 1, $h - 1, 'Full map overview (target not found: ' . $target . ')', 'overview');
}

/**
 * Produce semantic region breakdown + compact ASCII minimap (max 64x30 chars)
 * for any tile bounding box.
 *
 * @return array<string, mixed>
 */
function ai_map_analyze_region_box(
    array $parsed,
    string $visMask,
    string $distRoof,
    string $distFloor,
    array $structures,
    array $zones,
    array $entitiesList,
    array $allPointObjects,
    array $box,
    ?array $fullMapFeatures = null
): array {
    $w = (int)$parsed['game']['width'];
    $h = (int)$parsed['game']['height'];
    $tx0 = max(0, min($w - 1, (int)($box['tx0'] ?? 0)));
    $ty0 = max(0, min($h - 1, (int)($box['ty0'] ?? 0)));
    $tx1 = max($tx0, min($w - 1, (int)($box['tx1'] ?? ($w - 1))));
    $ty1 = max($ty0, min($h - 1, (int)($box['ty1'] ?? ($h - 1))));
    $bw = $tx1 - $tx0 + 1;
    $bh = $ty1 - $ty0 + 1;

    if ($fullMapFeatures !== null && $tx0 === 0 && $ty0 === 0 && $tx1 === $w - 1 && $ty1 === $h - 1) {
        $feat = $fullMapFeatures;
    } else {
        $feat = ai_map_measure_bbox_features($w, $h, $tx0, $ty0, $tx1, $ty1, $parsed['game_tiles'], $parsed['front_tiles'], $visMask, $distRoof, $distFloor);
    }

    // Filter overlapping structures, zones, entities, DB objects.
    $inStructures = [];
    foreach ($structures as $st) {
        $b = $st['bbox_tiles'];
        if ($b['tx1'] >= $tx0 && $b['tx0'] <= $tx1 && $b['ty1'] >= $ty0 && $b['ty0'] <= $ty1) {
            $inStructures[] = [
                'id' => $st['id'],
                'kind' => $st['kind'],
                'name' => $st['name'],
                'bbox_tiles' => $b,
                'center' => $st['center'],
            ];
        }
    }
    $inZones = [];
    foreach ($zones as $z) {
        $b = $z['bbox_tiles'];
        if ($b['tx1'] >= $tx0 && $b['tx0'] <= $tx1 && $b['ty1'] >= $ty0 && $b['ty0'] <= $ty1) {
            $inZones[] = ['number' => $z['number'], 'name' => $z['name'], 'pvp' => $z['pvp'], 'environment' => $z['environment']];
        }
    }
    $inObjects = [];
    foreach ($allPointObjects as $obj) {
        if ($obj['tile_x'] >= $tx0 && $obj['tile_x'] <= $tx1 && $obj['tile_y'] >= $ty0 && $obj['tile_y'] <= $ty1) {
            $inObjects[] = $obj;
            if (count($inObjects) >= 30) {
                break;
            }
        }
    }

    // Build semantic ASCII minimap (max 64 cols x 28 rows).
    $cols = min(64, max(8, $bw));
    $rows = min(28, max(6, (int)round($cols * ($bh / max(1, $bw)) * 0.6)));
    $rows = min($rows, $bh);
    $hasFront = ($parsed['front_tiles'] !== null && strlen($parsed['front_tiles']) >= $w * $h);

    // Pre-index spawns and DB objects onto minimap grid cells for crisp markers.
    $overlayChar = [];
    foreach ($entitiesList as $ent) {
        if ($ent['tile'] === 208 && $ent['tile_x'] >= $tx0 && $ent['tile_x'] <= $tx1 && $ent['tile_y'] >= $ty0 && $ent['tile_y'] <= $ty1) {
            $mc = min($cols - 1, (int)floor((($ent['tile_x'] - $tx0) / max(1, $bw)) * $cols));
            $mr = min($rows - 1, (int)floor((($ent['tile_y'] - $ty0) / max(1, $bh)) * $rows));
            $overlayChar[$mr . ':' . $mc] = 'S';
        }
    }
    foreach ($inObjects as $obj) {
        $mc = min($cols - 1, (int)floor((($obj['tile_x'] - $tx0) / max(1, $bw)) * $cols));
        $mr = min($rows - 1, (int)floor((($obj['tile_y'] - $ty0) / max(1, $bh)) * $rows));
        $ch = match ($obj['type']) {
            'warehouse' => '$',
            'aether', 'world_swap' => 'T',
            'dungeon_door' => 'D',
            default => 'N',
        };
        $overlayChar[$mr . ':' . $mc] ??= $ch;
    }

    $lines = [];
    $gameTilesStr = $parsed['game_tiles'];
    $frontTilesStr = $hasFront ? $parsed['front_tiles'] : '';
    $serviceSet = [17 => true, 21 => true, 29 => true, 30 => true, 38 => true];
    for ($r = 0; $r < $rows; $r++) {
        $ry0 = $ty0 + (int)floor(($r * $bh) / $rows);
        $ry1 = max($ry0, $ty0 + (int)floor((($r + 1) * $bh) / $rows) - 1);
        $line = '';
        for ($c = 0; $c < $cols; $c++) {
            $okey = $r . ':' . $c;
            if (isset($overlayChar[$okey])) {
                $line .= $overlayChar[$okey];
                continue;
            }
            $cx0 = $tx0 + (int)floor(($c * $bw) / $cols);
            $cx1 = max($cx0, $tx0 + (int)floor((($c + 1) * $bw) / $cols) - 1);

            $cSolidGround = 0;
            $cSolidArch = 0;
            $cInterior = 0;
            $cCave = 0;
            $cWater = 0;
            $cHazard = 0;
            $cHouse = 0;
            $cService = 0;
            $cTotal = 0;

            $stepX = max(1, intdiv($cx1 - $cx0 + 1, 4));
            $stepY = max(1, intdiv($ry1 - $ry0 + 1, 4));
            for ($sy = $ry0; $sy <= $ry1; $sy += $stepY) {
                $rowOff = $sy * $w;
                for ($sx = $cx0; $sx <= $cx1; $sx += $stepX) {
                    $cell = $rowOff + $sx;
                    $cTotal++;
                    $gb = $gameTilesStr[$cell];
                    $fb = $hasFront ? $frontTilesStr[$cell] : "\x00";
                    $vmb = $visMask[$cell];
                    if ($gb === "\x00" && $fb === "\x00" && $vmb === "\x00") {
                        if ($distRoof[$cell] !== "\xff" && ord($distRoof[$cell]) < 34 && ord($distFloor[$cell]) < 34) {
                            $cCave++;
                        }
                        continue;
                    }
                    $g = ord($gb);
                    $f = $hasFront ? ord($fb) : 0;
                    $vm = ord($vmb);
                    if ($g === 2 || $f === 2) {
                        $cHazard++;
                    } elseif ($g === 16 || $g === 8 || $f === 16 || $f === 8) {
                        $cHouse++;
                    } elseif (isset($serviceSet[$g]) || isset($serviceSet[$f])) {
                        $cService++;
                    } elseif ($g === 1 || $g === 3 || $f === 1 || $f === 3) {
                        if (($vm & (AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR)) !== 0 && ($vm & AI_VIS_TERRAIN) === 0) {
                            $cSolidArch++;
                        } else {
                            $cSolidGround++;
                        }
                    } elseif ($g === 6 || $f === 6 || (($vm & AI_VIS_WATER) !== 0 && ($vm & (AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR)) === 0)) {
                        $cWater++;
                    } elseif (ord($distRoof[$cell]) < 34 && ord($distFloor[$cell]) < 34) {
                        if (($vm & (AI_VIS_ARCHITECTURE | AI_VIS_INTERIOR | AI_VIS_BG_VISUAL)) !== 0) {
                            $cInterior++;
                        } else {
                            $cCave++;
                        }
                    }
                }
            }

            if ($cHouse > 0) {
                $line .= 'H';
            } elseif ($cService > 0) {
                $line .= '$';
            } elseif ($cHazard > max(0, intdiv($cTotal, 4))) {
                $line .= '!';
            } elseif ($cSolidArch > 0 && $cSolidArch >= $cSolidGround && ($cSolidArch + $cSolidGround) * 3 >= $cTotal) {
                $line .= '=';
            } elseif (($cSolidGround + $cSolidArch) * 3 >= $cTotal) {
                $line .= '#';
            } elseif ($cWater * 3 >= $cTotal) {
                $line .= '~';
            } elseif ($cInterior > 0 && $cInterior * 4 >= $cTotal) {
                $line .= '.';
            } elseif ($cCave > 0 && $cCave * 4 >= $cTotal) {
                $line .= ':';
            } else {
                $line .= ' ';
            }
        }
        $lines[] = $line;
    }

    return [
        'bbox_tiles' => ['tx0' => $tx0, 'ty0' => $ty0, 'tx1' => $tx1, 'ty1' => $ty1],
        'bbox_px' => ['x0' => $tx0 * 32, 'y0' => $ty0 * 32, 'x1' => ($tx1 + 1) * 32, 'y1' => ($ty1 + 1) * 32],
        'features' => $feat,
        'structures_in_region' => $inStructures,
        'zones_in_region' => $inZones,
        'objects_in_region' => $inObjects,
        'semantic_minimap' => [
            'cols' => $cols,
            'rows' => $rows,
            'legend' => [
                '#' => 'solid terrain/ground',
                '=' => 'building wall/roof/floor',
                '.' => 'enclosed building interior',
                ':' => 'underground cave',
                '~' => 'water',
                '!' => 'death/hazard',
                'H' => 'house/guild hall tile',
                '$' => 'shop/craft/bank/warehouse',
                'S' => 'spawn point',
                'T' => 'teleport/aether/world swap',
                'D' => 'dungeon door',
                'N' => 'NPC/object',
                ' ' => 'open outdoor air',
            ],
            'grid' => $lines,
        ],
    ];
}
