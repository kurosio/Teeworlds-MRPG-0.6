<?php
// Безопасная работа с JSON-файлами сценариев в каталоге scenarios/.
// Используется общим AI API (api/ai.php) и системой запросов на изменение
// (api/change-requests-lib.php / api/change-requests.php).
//
// Гостевая прямая запись запрещена: все операции со сценариями из гостевой
// сессии проходят валидацию, предпросмотр diff и утверждение администратором.
//
// Модуль не выполняет сетевых/HTTP-операций и побочных эффектов при подключении.

declare(strict_types=1);

/** Верхний размер одного файла сценария (1 MiB — согласовано с историей БД). */
const SCENARIO_MAX_BYTES = 1048576;

/** Максимальная длина имени файла сценария (байт). */
const SCENARIO_FILENAME_MAX_BYTES = 128;

/** Каталог сценариев на диске. */
function scenario_dir(?string $baseDir = null): string
{
    if ($baseDir !== null && $baseDir !== '') {
        return rtrim($baseDir, '/\\');
    }
    return dirname(__DIR__) . DIRECTORY_SEPARATOR . 'scenarios';
}

/** Виртуальное описание ресурса для записи в журнал db_history (revertable=false). */
function scenario_history_resource(): array
{
    return [
        'table' => 'scenarios',
        'pk' => 'filename',
        'label' => 'filename',
        'title' => 'Сценарии (файлы)',
    ];
}

/**
 * Проверка имени файла сценария без скрытой «очистки»: попытки обхода пути
 * (../, /, \, NUL, ведущая точка, не-.json) отклоняются явным кодом ошибки.
 *
 * @return array{ok:bool, filename?:string, code?:string, error?:string}
 */
function scenario_validate_filename(mixed $raw): array
{
    if (!is_string($raw)) {
        return ['ok' => false, 'code' => 'invalid_filename', 'error' => 'Имя файла сценария должно быть строкой.'];
    }
    $trimmed = trim($raw);
    if ($trimmed === '' || $trimmed !== $raw) {
        return ['ok' => false, 'code' => 'invalid_filename', 'error' => 'Имя файла сценария не должно быть пустым или содержать крайние пробелы.'];
    }
    if (strlen($raw) > SCENARIO_FILENAME_MAX_BYTES) {
        return ['ok' => false, 'code' => 'filename_too_long', 'error' => 'Имя файла сценария слишком длинное (максимум ' . SCENARIO_FILENAME_MAX_BYTES . ' байт).'];
    }
    if (preg_match('//u', $raw) !== 1 || preg_match('/[\x00-\x1f\x7f]/', $raw)) {
        return ['ok' => false, 'code' => 'invalid_filename', 'error' => 'Имя файла сценария содержит недопустимые управляющие символы.'];
    }
    if (str_contains($raw, '/') || str_contains($raw, '\\') || str_contains($raw, ':') || str_contains($raw, '..')) {
        return ['ok' => false, 'code' => 'path_traversal_blocked', 'error' => 'Имя файла сценария не должно содержать путь или «..».'];
    }
    if (str_starts_with($raw, '.')) {
        return ['ok' => false, 'code' => 'invalid_filename', 'error' => 'Скрытые файлы и файлы начинающиеся с точки запрещены.'];
    }
    if (!str_ends_with(strtolower($raw), '.json')) {
        return ['ok' => false, 'code' => 'invalid_extension', 'error' => 'Файл сценария должен иметь расширение .json.'];
    }
    if (preg_match('/^[A-Za-z0-9_\p{L}][A-Za-z0-9\p{L}._ -]*\.json$/u', $raw) !== 1) {
        return ['ok' => false, 'code' => 'invalid_filename', 'error' => 'Допустимы только буквы, цифры, «_», «-», «.» и расширение .json.'];
    }
    return ['ok' => true, 'filename' => $raw];
}

/**
 * Разрешение пути файла строго внутри scenarios/ (защита от symlink и выхода за каталог).
 *
 * @return array{ok:bool, filename?:string, dir?:string, path?:string, exists?:bool, code?:string, error?:string}
 */
function scenario_resolve_path(mixed $rawFilename, ?string $baseDir = null, bool $mustExist = false): array
{
    $nameCheck = scenario_validate_filename($rawFilename);
    if (!$nameCheck['ok']) {
        return $nameCheck;
    }
    $filename = (string)$nameCheck['filename'];
    $dir = scenario_dir($baseDir);
    $dirReal = realpath($dir);
    if ($dirReal === false || !is_dir($dirReal)) {
        return ['ok' => false, 'code' => 'scenarios_dir_missing', 'error' => 'Каталог сценариев не найден на сервере.'];
    }
    $target = $dirReal . DIRECTORY_SEPARATOR . $filename;
    if (is_link($target)) {
        return ['ok' => false, 'code' => 'path_traversal_blocked', 'error' => 'Символические ссылки в каталоге сценариев запрещены.'];
    }
    $exists = file_exists($target);
    if ($exists) {
        if (!is_file($target)) {
            return ['ok' => false, 'code' => 'invalid_filename', 'error' => 'Указанный объект не является обычным файлом.'];
        }
        $targetReal = realpath($target);
        if ($targetReal === false || !str_starts_with($targetReal, $dirReal . DIRECTORY_SEPARATOR)) {
            return ['ok' => false, 'code' => 'path_traversal_blocked', 'error' => 'Путь сценария выходит за пределы разрешённого каталога.'];
        }
        $target = $targetReal;
    } elseif ($mustExist) {
        return ['ok' => false, 'code' => 'scenario_not_found', 'error' => "Файл сценария «{$filename}» не найден."];
    }

    return [
        'ok' => true,
        'filename' => $filename,
        'dir' => $dirReal,
        'path' => $target,
        'exists' => $exists,
    ];
}

/** Короткая сводка по структуре декодированного JSON сценария. */
function scenario_extract_summary(array $content): array
{
    $mode = isset($content['scenario_mode']) && is_string($content['scenario_mode'])
        ? $content['scenario_mode']
        : null;

    $stepsRaw = $content['steps'] ?? [];
    $stepsCount = 0;
    $stepIds = [];
    $componentTypes = [];
    $events = [];

    if (is_array($stepsRaw)) {
        foreach ($stepsRaw as $k => $step) {
            if (!is_array($step)) {
                continue;
            }
            $stepsCount++;
            $sid = isset($step['id']) && is_scalar($step['id']) ? (string)$step['id'] : (is_string($k) ? $k : '');
            if ($sid !== '') {
                $stepIds[] = $sid;
            }
            if (isset($step['event']) && is_string($step['event']) && $step['event'] !== '') {
                $events[$step['event']] = true;
            }
            if (is_array($step['components'] ?? null)) {
                foreach ($step['components'] as $comp) {
                    if (is_array($comp) && isset($comp['type']) && is_string($comp['type']) && $comp['type'] !== '') {
                        $componentTypes[$comp['type']] = true;
                    }
                }
            }
        }
    }

    foreach ($content as $topKey => $_val) {
        if (is_string($topKey) && str_starts_with($topKey, 'on_') && is_array($_val)) {
            $events[$topKey] = true;
        }
    }

    $groupsRaw = is_array($content['editor_data'] ?? null) ? ($content['editor_data']['groups'] ?? []) : [];
    $groupsCount = is_array($groupsRaw) ? count($groupsRaw) : 0;
    if (is_array($groupsRaw)) {
        foreach ($groupsRaw as $g) {
            if (is_array($g) && isset($g['event']) && is_string($g['event']) && $g['event'] !== '') {
                $events[$g['event']] = true;
            }
        }
    }

    return [
        'scenario_mode' => $mode,
        'steps_count' => $stepsCount,
        'groups_count' => $groupsCount,
        'step_ids' => array_values($stepIds),
        'component_types' => array_keys($componentTypes),
        'events' => array_keys($events),
    ];
}

/**
 * Извлечение типизированных ссылок на игровые таблицы внутри JSON сценария.
 * Не затрагивает строковые step `id` / group `id`.
 *
 * @return array<int, array{path:string, target:string, id:int}>
 */
function scenario_extract_references(mixed $content): array
{
    if (!is_array($content)) {
        return [];
    }
    $out = [];
    $walk = static function (mixed $node, string $path, string $parentKey = '') use (&$walk, &$out): void {
        if (!is_array($node)) {
            return;
        }
        $fieldTargets = [
            'item_id' => 'tw_items_list',
            'quest_id' => 'tw_quests_list',
            'need_quest_id' => 'tw_quests_list',
            'completed_quest_id' => 'tw_quests_list',
            'active_quest_id' => 'tw_quests_list',
            'bot_id' => 'tw_bots_info',
            'mob_id' => 'tw_bots_info',
            'world_id' => 'tw_worlds',
            'target_world_id' => 'tw_worlds',
        ];
        $nestedItemParents = ['item', 'items', 'rewards', 'give_items', 'required_items', 'need_items', 'drops'];
        $nestedQuestParents = ['quests'];
        $nestedBotParents = ['bots', 'npcs'];

        foreach ($node as $key => $value) {
            $childPath = $path === '' ? (string)$key : (is_int($key) ? "{$path}[{$key}]" : "{$path}.{$key}");

            if (is_string($key) && isset($fieldTargets[$key])) {
                if (is_int($value) || (is_string($value) && preg_match('/^-?\d+$/', $value) === 1)) {
                    $id = (int)$value;
                    $target = $fieldTargets[$key];
                    if ($target === 'tw_worlds' ? $id >= 0 : $id > 0) {
                        $out[] = ['path' => $childPath, 'target' => $target, 'id' => $id];
                    }
                }
                continue;
            }

            if ($key === 'id') {
                $target = null;
                if (in_array($parentKey, $nestedItemParents, true)) {
                    $target = 'tw_items_list';
                } elseif (in_array($parentKey, $nestedQuestParents, true)) {
                    $target = 'tw_quests_list';
                } elseif (in_array($parentKey, $nestedBotParents, true)) {
                    $target = 'tw_bots_info';
                }
                if ($target !== null && (is_int($value) || (is_string($value) && preg_match('/^-?\d+$/', $value) === 1))) {
                    $id = (int)$value;
                    if ($id > 0) {
                        $out[] = ['path' => $childPath, 'target' => $target, 'id' => $id];
                    }
                }
                continue;
            }

            if (is_array($value)) {
                $nextParent = is_string($key) ? $key : $parentKey;
                $walk($value, $childPath, $nextParent);
            }
        }
    };
    $walk($content, '', '');
    return $out;
}

/**
 * Подмена временных гостевых ID (2000000000..2000999999) на реальные ID после
 * применения операций БД в том же запросе на изменение.
 */
function scenario_remap_references(mixed $content, array $idMap): mixed
{
    if (!is_array($content) || !$idMap) {
        return $content;
    }
    $entityKeys = [
        'item_id', 'quest_id', 'need_quest_id', 'completed_quest_id', 'active_quest_id',
        'bot_id', 'mob_id', 'world_id', 'target_world_id',
    ];
    $nestedTypedParents = ['item', 'items', 'rewards', 'give_items', 'required_items', 'need_items', 'drops', 'quests', 'bots', 'npcs'];

    $walk = static function (mixed $node, string $parentKey = '') use (&$walk, $idMap, $entityKeys, $nestedTypedParents): mixed {
        if (!is_array($node)) {
            return $node;
        }
        $copy = [];
        foreach ($node as $key => $value) {
            $isEntityKey = is_string($key) && in_array($key, $entityKeys, true);
            $isNestedId = ($key === 'id') && in_array($parentKey, $nestedTypedParents, true);
            if (($isEntityKey || $isNestedId) && (is_int($value) || (is_string($value) && preg_match('/^-?\d+$/', $value) === 1))) {
                $intVal = (int)$value;
                $copy[$key] = array_key_exists($intVal, $idMap)
                    ? (is_string($value) ? (string)$idMap[$intVal] : (int)$idMap[$intVal])
                    : $value;
                continue;
            }
            if (is_array($value)) {
                $nextParent = is_string($key) ? $key : $parentKey;
                $copy[$key] = $walk($value, $nextParent);
                continue;
            }
            $copy[$key] = $value;
        }
        return $copy;
    };
    return $walk($content, '');
}

/** Канонический перечень типов компонентов сценария (из editor-core/registry.js). */
function scenario_known_component_types(): array
{
    return [
        'branch_random',
        'use_chat_code',
        'message',
        'wait',
        'follow_camera',
        'dynamic_condition',
        'condition_movement',
        'teleport',
        'moving_disable',
        'emote',
        'quest_action',
        'quest_condition',
        'defeat_mobs',
        'restore_resources',
        'set_group_flags',
        'set_group_lives',
        'set_group_spawn',
        'dungeon_door_control',
        'dungeon_activate_point',
        'dungeon_complete',
        'universal_door_control',
        'universal_condition_item',
        'universal_pick_item_task',
        'universal_shootmarkers',
        'world_group_lives',
        'world_complete',
        'check_has_item',
        'reset_quest',
        'accept_quest',
        'check_quest_accepted',
        'check_quest_finished',
        'check_quest_step_finished',
        'pick_item_task',
        'shootmarkers',
    ];
}

/** Legacy типы (предупреждение о возможности устаревания, без ошибки). */
function scenario_legacy_component_types(): array
{
    return [
        'universal_teleport',
        'universal_use_chat',
        'dungeon_use_chat_code',
    ];
}

/**
 * Список всех сценариев (.json) в каталоге scenarios/.
 *
 * @return array<int, array<string, mixed>>
 */
function scenario_list_files(string $search = '', ?string $baseDir = null): array
{
    $dir = scenario_dir($baseDir);
    $dirReal = realpath($dir);
    if ($dirReal === false || !is_dir($dirReal)) {
        return [];
    }
    $entries = @scandir($dirReal);
    if (!is_array($entries)) {
        return [];
    }

    $needle = trim($search);
    $needleLower = $needle !== '' ? mb_strtolower($needle, 'UTF-8') : '';
    $items = [];

    foreach ($entries as $name) {
        if ($name === '.' || $name === '..' || str_starts_with($name, '.')) {
            continue;
        }
        $check = scenario_validate_filename($name);
        if (!$check['ok']) {
            continue;
        }
        $full = $dirReal . DIRECTORY_SEPARATOR . $name;
        if (is_link($full) || !is_file($full) || !is_readable($full)) {
            continue;
        }
        $size = (int)(@filesize($full) ?: 0);
        $mtime = (int)(@filemtime($full) ?: 0);
        $raw = $size <= SCENARIO_MAX_BYTES ? @file_get_contents($full) : false;
        $sha = is_string($raw) ? hash('sha256', $raw) : (string)(@hash_file('sha256', $full) ?: '');
        $decoded = is_string($raw) ? json_decode($raw, true) : null;
        $parseError = is_string($raw) && json_last_error() !== JSON_ERROR_NONE ? json_last_error_msg() : null;
        $summary = is_array($decoded) ? scenario_extract_summary($decoded) : null;

        if ($needleLower !== '') {
            $hay = mb_strtolower(implode(' ', array_filter([
                $name,
                $summary['scenario_mode'] ?? '',
                implode(' ', $summary['step_ids'] ?? []),
                implode(' ', $summary['component_types'] ?? []),
                implode(' ', $summary['events'] ?? []),
            ])), 'UTF-8');
            if (!str_contains($hay, $needleLower)) {
                continue;
            }
        }

        $items[] = [
            'filename' => $name,
            'bytes' => $size,
            'sha256' => $sha,
            'updated_at' => $mtime > 0 ? gmdate('Y-m-d\TH:i:s\Z', $mtime) : null,
            'valid_json' => is_array($decoded),
            'parse_error' => $parseError,
            'summary' => $summary,
        ];
    }

    usort($items, static fn(array $a, array $b): int => strcmp((string)$a['filename'], (string)$b['filename']));
    return $items;
}

/**
 * Безопасное чтение одного файла сценария с хешем, сводкой и ссылками.
 *
 * @return array<string, mixed>
 */
function scenario_read_file(mixed $rawFilename, ?string $baseDir = null): array
{
    $resolved = scenario_resolve_path($rawFilename, $baseDir, true);
    if (!$resolved['ok']) {
        return $resolved;
    }
    $path = (string)$resolved['path'];
    $size = (int)(@filesize($path) ?: 0);
    if ($size > SCENARIO_MAX_BYTES) {
        return [
            'ok' => false,
            'code' => 'scenario_too_large',
            'error' => "Файл сценария «{$resolved['filename']}» превышает лимит " . SCENARIO_MAX_BYTES . ' байт.',
        ];
    }
    $raw = @file_get_contents($path);
    if (!is_string($raw)) {
        return [
            'ok' => false,
            'code' => 'scenario_read_failed',
            'error' => "Не удалось прочитать файл сценария «{$resolved['filename']}».",
        ];
    }
    $decoded = json_decode($raw, true);
    if (!is_array($decoded) || json_last_error() !== JSON_ERROR_NONE) {
        return [
            'ok' => false,
            'code' => 'invalid_scenario_json',
            'error' => "Файл сценария «{$resolved['filename']}» содержит некорректный JSON: " . json_last_error_msg(),
        ];
    }
    $mtime = (int)(@filemtime($path) ?: 0);
    return [
        'ok' => true,
        'filename' => (string)$resolved['filename'],
        'bytes' => strlen($raw),
        'sha256' => hash('sha256', $raw),
        'updated_at' => $mtime > 0 ? gmdate('Y-m-d\TH:i:s\Z', $mtime) : null,
        'content' => $decoded,
        'summary' => scenario_extract_summary($decoded),
        'references' => scenario_extract_references($decoded),
    ];
}

/**
 * Приведение контента сценария (массив или JSON-строка) к декодированному объекту.
 *
 * @return array{ok:bool, content?:array<string, mixed>, encoded?:string, code?:string, error?:string}
 */
function scenario_normalize_content(mixed $rawContent): array
{
    if (is_string($rawContent)) {
        if (preg_match('//u', $rawContent) !== 1) {
            return ['ok' => false, 'code' => 'invalid_encoding', 'error' => 'Содержимое сценария должно быть в UTF-8.'];
        }
        $decoded = json_decode($rawContent, true);
        if (json_last_error() !== JSON_ERROR_NONE || !is_array($decoded)) {
            return ['ok' => false, 'code' => 'invalid_json', 'error' => 'Некорректный JSON сценария: ' . json_last_error_msg()];
        }
        $content = $decoded;
    } elseif (is_array($rawContent)) {
        $content = $rawContent;
    } else {
        return ['ok' => false, 'code' => 'invalid_scenario_structure', 'error' => 'Содержимое сценария должно быть JSON-объектом.'];
    }

    if (array_is_list($content) && $content !== []) {
        return ['ok' => false, 'code' => 'invalid_scenario_structure', 'error' => 'Корень сценария должен быть JSON-объектом, а не массивом.'];
    }

    $encoded = json_encode($content, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
    if (!is_string($encoded)) {
        return ['ok' => false, 'code' => 'invalid_json', 'error' => 'Не удалось сериализовать JSON сценария.'];
    }
    if (strlen($encoded) > SCENARIO_MAX_BYTES) {
        return ['ok' => false, 'code' => 'scenario_too_large', 'error' => 'Сценарий превышает максимальный размер (' . SCENARIO_MAX_BYTES . ' байт).'];
    }
    return ['ok' => true, 'content' => $content, 'encoded' => $encoded];
}

/**
 * Структурная и ссылочная проверка содержимого сценария.
 *
 * @param array<int, string>   $tempKnown  tempId => resourceKey
 * @param array<string, true>  $deletedMap "resource:id" => true
 * @return array{errors:array<int, array<string, mixed>>, warnings:array<int, array<string, mixed>>, summary:array<string, mixed>, references:array<int, array<string, mixed>>}
 */
function scenario_validate_content(
    mixed $rawContent,
    ?mysqli $db = null,
    array $tempKnown = [],
    array $deletedMap = []
): array {
    $errors = [];
    $warnings = [];
    $norm = scenario_normalize_content($rawContent);
    if (!$norm['ok']) {
        $errors[] = [
            'code' => $norm['code'] ?? 'invalid_scenario_structure',
            'field' => 'content',
            'message' => (string)$norm['error'],
        ];
        return ['errors' => $errors, 'warnings' => $warnings, 'summary' => [], 'references' => []];
    }

    $content = $norm['content'];
    $allowedModes = ['default', 'dungeon', 'universal', 'world'];
    if (array_key_exists('scenario_mode', $content) && $content['scenario_mode'] !== null && $content['scenario_mode'] !== '') {
        if (!is_string($content['scenario_mode']) || !in_array($content['scenario_mode'], $allowedModes, true)) {
            $errors[] = [
                'code' => 'invalid_enum_value',
                'field' => 'content.scenario_mode',
                'message' => 'Недопустимый scenario_mode; допустимо: ' . implode(', ', $allowedModes),
                'allowed' => $allowedModes,
            ];
        }
    }

    $knownTypes = array_flip(scenario_known_component_types());
    $legacyTypes = array_flip(scenario_legacy_component_types());

    $validateStepGroup = static function (array $steps, string $scopePrefix) use (
        $knownTypes,
        $legacyTypes,
        &$errors,
        &$warnings
    ): void {
        $stepIds = [];
        $linkChecks = [];

        foreach ($steps as $idx => $step) {
            $stepPath = "{$scopePrefix}[{$idx}]";
            if (!is_array($step)) {
                $errors[] = [
                    'code' => 'invalid_field_type',
                    'field' => $stepPath,
                    'message' => 'Каждый шаг в steps должен быть JSON-объектом.',
                ];
                continue;
            }
            $sid = $step['id'] ?? (is_string($idx) ? $idx : null);
            if (!is_string($sid) || trim($sid) === '') {
                $errors[] = [
                    'code' => 'missing_required_field',
                    'field' => "{$stepPath}.id",
                    'message' => 'Каждый шаг сценария должен иметь непустой строковый id.',
                ];
            } elseif (isset($stepIds[$sid])) {
                $errors[] = [
                    'code' => 'duplicate_step_id',
                    'field' => "{$stepPath}.id",
                    'message' => "Повторяющийся id шага сценария: «{$sid}».",
                ];
            } else {
                $stepIds[$sid] = true;
            }

            if (isset($step['completion_logic']) && $step['completion_logic'] !== '') {
                $allowedLogic = ['all_of', 'any_of', 'sequential'];
                if (!is_string($step['completion_logic']) || !in_array($step['completion_logic'], $allowedLogic, true)) {
                    $warnings[] = [
                        'code' => 'unknown_completion_logic',
                        'field' => "{$stepPath}.completion_logic",
                        'message' => 'Нестандартное значение completion_logic: ' . (string)$step['completion_logic'],
                    ];
                }
            }

            $components = $step['components'] ?? null;
            if ($components !== null) {
                if (!is_array($components)) {
                    $errors[] = [
                        'code' => 'invalid_field_type',
                        'field' => "{$stepPath}.components",
                        'message' => 'Поле components в шаге должно быть массивом.',
                    ];
                } else {
                    foreach ($components as $cIdx => $comp) {
                        $cPath = "{$stepPath}.components[{$cIdx}]";
                        if (!is_array($comp)) {
                            $errors[] = [
                                'code' => 'invalid_field_type',
                                'field' => $cPath,
                                'message' => 'Каждый компонент шага должен быть JSON-объектом.',
                            ];
                            continue;
                        }
                        $cType = $comp['type'] ?? '';
                        if (!is_string($cType) || trim($cType) === '') {
                            $errors[] = [
                                'code' => 'missing_required_field',
                                'field' => "{$cPath}.type",
                                'message' => 'Каждый компонент должен указывать строковый type.',
                            ];
                        } elseif (!isset($knownTypes[$cType])) {
                            $warnings[] = [
                                'code' => isset($legacyTypes[$cType]) ? 'legacy_component_type' : 'unknown_component_type',
                                'field' => "{$cPath}.type",
                                'message' => isset($legacyTypes[$cType])
                                    ? "Компонент «{$cType}» относится к устаревшему формату сценариев."
                                    : "Неизвестный тип компонента «{$cType}» — проверьте схему компонентов.",
                            ];
                        }

                        foreach (['delay', 'execution_time'] as $tickCol) {
                            if (array_key_exists($tickCol, $comp) && $comp[$tickCol] !== null) {
                                if (!is_int($comp[$tickCol]) && !(is_numeric($comp[$tickCol]) && (float)$comp[$tickCol] === floor((float)$comp[$tickCol]))) {
                                    $errors[] = [
                                        'code' => 'invalid_integer',
                                        'field' => "{$cPath}.{$tickCol}",
                                        'message' => "Поле {$tickCol} должно быть целым числом тиков.",
                                    ];
                                } elseif ((int)$comp[$tickCol] < 0) {
                                    $errors[] = [
                                        'code' => 'out_of_range',
                                        'field' => "{$cPath}.{$tickCol}",
                                        'message' => "Поле {$tickCol} не может быть отрицательным.",
                                    ];
                                }
                            }
                        }

                        foreach (['next_step_id', 'true_step_id', 'false_step_id', 'pass_step_id', 'fail_step_id'] as $linkKey) {
                            if (isset($comp[$linkKey]) && is_string($comp[$linkKey]) && trim($comp[$linkKey]) !== '') {
                                $linkChecks[] = ['field' => "{$cPath}.{$linkKey}", 'target' => trim($comp[$linkKey])];
                            }
                        }
                    }
                }
            }
        }

        foreach ($linkChecks as $link) {
            if (!isset($stepIds[$link['target']])) {
                $errors[] = [
                    'code' => 'broken_step_link',
                    'field' => $link['field'],
                    'message' => "Ссылка на несуществующий шаг сценария «{$link['target']}».",
                ];
            }
        }
    };

    $steps = $content['steps'] ?? null;
    if ($steps !== null) {
        if (!is_array($steps)) {
            $errors[] = [
                'code' => 'invalid_field_type',
                'field' => 'content.steps',
                'message' => 'Поле steps должно быть массивом (или объектом) шагов сценария.',
            ];
        } else {
            $validateStepGroup($steps, 'content.steps');
        }
    }

    foreach ($content as $eventKey => $eventVal) {
        if (!is_string($eventKey) || !str_starts_with($eventKey, 'on_') || !is_array($eventVal)) {
            continue;
        }
        if (isset($eventVal['steps'])) {
            if (!is_array($eventVal['steps'])) {
                $errors[] = [
                    'code' => 'invalid_field_type',
                    'field' => "content.{$eventKey}.steps",
                    'message' => "Поле {$eventKey}.steps должно быть массивом шагов.",
                ];
            } else {
                $validateStepGroup($eventVal['steps'], "content.{$eventKey}.steps");
            }
        }
    }

    $tableToResource = [
        'tw_items_list' => 'items',
        'tw_quests_list' => 'quests',
        'tw_bots_info' => 'bots_info',
        'tw_worlds' => 'worlds',
    ];
    $refs = scenario_extract_references($content);
    $existCache = [];
    foreach ($refs as $ref) {
        $targetTable = $ref['target'];
        $targetRes = $tableToResource[$targetTable] ?? '';
        $id = (int)$ref['id'];

        if ($targetRes !== '' && isset($deletedMap["{$targetRes}:{$id}"])) {
            $errors[] = [
                'code' => 'deleted_reference_target',
                'field' => 'content.' . $ref['path'],
                'message' => "Сценарий ссылается на запись {$targetTable} #{$id}, удаляемую в этом же пакете.",
            ];
            continue;
        }

        if ($id >= 2000000000 && $id <= 2000999999) {
            if (!isset($tempKnown[$id])) {
                $errors[] = [
                    'code' => 'missing_temp_reference',
                    'field' => 'content.' . $ref['path'],
                    'message' => "Временный ID #{$id} отсутствует среди создаваемых записей в этом запросе.",
                ];
            } elseif ($targetRes !== '' && $tempKnown[$id] !== $targetRes) {
                $errors[] = [
                    'code' => 'invalid_reference_target',
                    'field' => 'content.' . $ref['path'],
                    'message' => "Временный ID #{$id} относится к «{$tempKnown[$id]}», а требуется «{$targetRes}».",
                ];
            }
            continue;
        }

        if ($db !== null) {
            $cacheKey = "{$targetTable}:{$id}";
            if (!array_key_exists($cacheKey, $existCache)) {
                $stmt = $db->prepare("SELECT 1 FROM `{$targetTable}` WHERE `ID` = ? LIMIT 1");
                if ($stmt) {
                    $stmt->bind_param('i', $id);
                    $stmt->execute();
                    $res = $stmt->get_result();
                    $existCache[$cacheKey] = ($res && $res->fetch_row() !== null);
                    $stmt->close();
                } else {
                    $existCache[$cacheKey] = true;
                }
            }
            if (!$existCache[$cacheKey]) {
                $errors[] = [
                    'code' => 'missing_reference',
                    'field' => 'content.' . $ref['path'],
                    'message' => "Ссылка на несуществующую запись {$targetTable} #{$id}.",
                ];
            }
        }
    }

    return [
        'errors' => $errors,
        'warnings' => $warnings,
        'summary' => scenario_extract_summary($content),
        'references' => $refs,
    ];
}

/**
 * Структурированный JSON-diff для двух значений (ограничен по количеству записей,
 * чтобы предпросмотр не раздувался на огромных сценариях).
 *
 * @return array<int, array<string, mixed>>
 */
function scenario_json_diff(mixed $before, mixed $after, string $prefix = '', int $limit = 200): array
{
    $diffs = [];
    $walk = static function (mixed $a, mixed $b, string $path) use (&$walk, &$diffs, $limit): void {
        if (count($diffs) >= $limit) {
            return;
        }
        if ($a === $b) {
            return;
        }
        if (is_array($a) && is_array($b)) {
            $aIsList = array_is_list($a);
            $bIsList = array_is_list($b);
            if ($aIsList && $bIsList) {
                $max = max(count($a), count($b));
                for ($i = 0; $i < $max && count($diffs) < $limit; $i++) {
                    $p = $path === '' ? "[{$i}]" : "{$path}[{$i}]";
                    if (!array_key_exists($i, $a)) {
                        $diffs[] = ['op' => 'add', 'path' => $p, 'after' => $b[$i]];
                    } elseif (!array_key_exists($i, $b)) {
                        $diffs[] = ['op' => 'remove', 'path' => $p, 'before' => $a[$i]];
                    } else {
                        $walk($a[$i], $b[$i], $p);
                    }
                }
                return;
            }
            if (!$aIsList && !$bIsList) {
                $keys = array_unique(array_merge(array_keys($a), array_keys($b)));
                foreach ($keys as $k) {
                    if (count($diffs) >= $limit) {
                        break;
                    }
                    $p = $path === '' ? (string)$k : "{$path}.{$k}";
                    $inA = array_key_exists($k, $a);
                    $inB = array_key_exists($k, $b);
                    if (!$inA) {
                        $diffs[] = ['op' => 'add', 'path' => $p, 'after' => $b[$k]];
                    } elseif (!$inB) {
                        $diffs[] = ['op' => 'remove', 'path' => $p, 'before' => $a[$k]];
                    } else {
                        $walk($a[$k], $b[$k], $p);
                    }
                }
                return;
            }
        }
        $diffs[] = ['op' => 'replace', 'path' => $path !== '' ? $path : '$', 'before' => $a, 'after' => $b];
    };
    $walk($before, $after, $prefix);
    return $diffs;
}

/**
 * Атомарное применение пакета файловых операций (create/update/delete) с блокировкой
 * и резервными копиями для отката при сбое транзакции БД.
 *
 * @param array<int, array<string, mixed>> $ops   Операции resource='scenarios'
 * @param array<int, int>                  $idMap Карта tempId => реальный ID БД
 * @return array{applied:array<int, array<string, mixed>>, history:array<int, array<string, mixed>>, rollback:callable}
 */
function scenario_apply_file_ops(array $ops, array $idMap = [], ?string $baseDir = null): array
{
    if ($ops === []) {
        return ['applied' => [], 'history' => [], 'rollback' => static function (): void {}];
    }

    $dirReal = realpath(scenario_dir($baseDir));
    if ($dirReal === false || !is_dir($dirReal)) {
        throw new RuntimeException('Каталог сценариев не найден.');
    }

    $lockFile = rtrim(sys_get_temp_dir(), '/\\') . DIRECTORY_SEPARATOR . 'mrpg-scenarios-' . sha1($dirReal) . '.lock';
    $lockFp = @fopen($lockFile, 'c+');
    if ($lockFp !== false) {
        flock($lockFp, LOCK_EX);
    }

    $backups = []; // path => ['existed' => bool, 'raw' => ?string]
    $applied = [];
    $history = [];

    $rollback = static function () use (&$backups, $lockFp): void {
        foreach (array_reverse($backups, true) as $path => $state) {
            if ($state['existed']) {
                $tmp = $path . '.rb.' . bin2hex(random_bytes(4));
                if (@file_put_contents($tmp, (string)$state['raw']) !== false) {
                    @rename($tmp, $path);
                } else {
                    @file_put_contents($path, (string)$state['raw']);
                }
            } else {
                if (is_file($path)) {
                    @unlink($path);
                }
            }
        }
        if ($lockFp !== false) {
            flock($lockFp, LOCK_UN);
            fclose($lockFp);
        }
    };

    try {
        foreach ($ops as $op) {
            $type = (string)($op['type'] ?? '');
            $resolved = scenario_resolve_path($op['filename'] ?? '', $dirReal, false);
            if (!$resolved['ok']) {
                throw new RuntimeException((string)$resolved['error']);
            }
            $filename = (string)$resolved['filename'];
            $path = (string)$resolved['path'];

            if (!array_key_exists($path, $backups)) {
                $existed = is_file($path);
                $prevRaw = $existed ? @file_get_contents($path) : null;
                if ($existed && !is_string($prevRaw)) {
                    throw new RuntimeException("Не удалось прочитать текущий файл сценария «{$filename}».");
                }
                $backups[$path] = ['existed' => $existed, 'raw' => $prevRaw];
            }

            $prevRaw = $backups[$path]['raw'];
            $prevDecoded = is_string($prevRaw) ? json_decode($prevRaw, true) : null;
            $prevData = is_array($prevDecoded) ? $prevDecoded : [];

            if ($type === 'delete') {
                if (!is_file($path)) {
                    continue;
                }
                if (!@unlink($path)) {
                    throw new RuntimeException("Не удалось удалить файл сценария «{$filename}».");
                }
                $applied[] = [
                    'type' => 'delete',
                    'resource' => 'scenarios',
                    'filename' => $filename,
                    'id' => null,
                    'data' => [],
                ];
                $history[] = [
                    'type' => 'delete',
                    'resource' => 'scenarios',
                    'filename' => $filename,
                    'id' => 0,
                    'label' => $filename,
                    'before' => $prevData,
                    'after' => [],
                ];
                continue;
            }

            if ($type === 'create' && is_file($path)) {
                throw new RuntimeException("Файл сценария «{$filename}» уже существует.");
            }
            if ($type === 'update' && !is_file($path)) {
                throw new RuntimeException("Файл сценария «{$filename}» не найден.");
            }

            $rawContent = $op['data']['content'] ?? ($op['data'] ?? null);
            $norm = scenario_normalize_content($rawContent);
            if (!$norm['ok']) {
                throw new RuntimeException((string)$norm['error']);
            }
            $finalContent = $idMap ? scenario_remap_references($norm['content'], $idMap) : $norm['content'];
            $json = json_encode($finalContent, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT) . "\n";
            if (!is_string($json) || strlen($json) > SCENARIO_MAX_BYTES) {
                throw new RuntimeException("Сценарий «{$filename}» превышает лимит размера или не сериализуется.");
            }

            $tmpPath = $dirReal . DIRECTORY_SEPARATOR . '.tmp_' . bin2hex(random_bytes(8)) . '.json';
            if (@file_put_contents($tmpPath, $json, LOCK_EX) === false) {
                @unlink($tmpPath);
                throw new RuntimeException("Не удалось записать временный файл сценария «{$filename}».");
            }
            if (!@rename($tmpPath, $path)) {
                @unlink($tmpPath);
                throw new RuntimeException("Не удалось сохранить файл сценария «{$filename}».");
            }

            $applied[] = [
                'type' => $type,
                'resource' => 'scenarios',
                'filename' => $filename,
                'id' => null,
                'sha256' => hash('sha256', $json),
                'data' => ['content' => $finalContent],
            ];
            $history[] = [
                'type' => $type,
                'resource' => 'scenarios',
                'filename' => $filename,
                'id' => 0,
                'label' => $filename,
                'before' => $type === 'update' ? $prevData : [],
                'after' => $finalContent,
            ];
        }

        // Отпускаем lock только после успешного завершения вызывающего кода (или через commit/rollback closure).
        $release = static function () use ($lockFp): void {
            if ($lockFp !== false) {
                flock($lockFp, LOCK_UN);
                fclose($lockFp);
            }
        };

        return [
            'applied' => $applied,
            'history' => $history,
            'commit' => $release,
            'rollback' => $rollback,
        ];
    } catch (Throwable $e) {
        $rollback();
        throw $e;
    }
}
