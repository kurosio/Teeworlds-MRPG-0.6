<?php
// Универсальный AI API для Editor Studio (чтение, анализ связей, проверка,
// предпросмотр diff и создание запросов на изменение от имени гостевой сессии).
//
// Endpoint: api/ai.php?action=...
//
// GET:
//   discovery  — публичный дескриптор обнаружения AI (без секретов, /.well-known/ai-plugin.json)
//   openapi    — машиночитаемая спецификация OpenAPI 3.1.0 (публично)
//   manifest   — описание возможностей, ограничений, прав и схем AI API
//   resources  — детальная схема разрешённых ресурсов (&resource=..., &include_examples=1)
//   list       — постраничный поиск/фильтрация записей БД, сценариев, справочников, карт
//   get        — чтение одной записи/сценария/карты (с опциями связей и проверки точки)
//   references — граф входящих и исходящих ссылок или чтение словарей (&source=...)
//   history    — журнал действий и запросов на изменение текущего актора
//
// POST (требует Same-Origin JSON POST + X-Editor-CSRF ИЛИ Authorization: Bearer esai_...):
//   validate   — полная проверка операций (БД + сценарии + patch_json + copy) без записи
//   preview    — dry-run анализ: diff, затронутые связи, dangerous_actions (без записи)
//   propose    — отправка или обновление запроса на изменение в SQLite (только гость/токен)
//
// Прямая запись в игровую БД или файлы scenarios/ через этот API невозможна.

declare(strict_types=1);

require_once __DIR__ . '/ai-lib.php';
require_once __DIR__ . '/db-history-lib.php';

$action = (string)($_GET['action'] ?? '');
$method = strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET'));

if (!in_array($action, ['validate', 'preview', 'propose'], true) && $method !== 'GET') {
    respond(['ok' => false, 'code' => 'method_not_allowed', 'error' => 'Метод запроса не поддерживается'], 405);
}

// Машиночитаемая OpenAPI 3.1 спецификация доступна напрямую через PHP (apache-config блокирует прямой .json).
if ($action === 'openapi') {
    $specPath = __DIR__ . '/ai-openapi.json';
    $rawSpec = is_file($specPath) ? @file_get_contents($specPath) : false;
    $decodedSpec = is_string($rawSpec) ? json_decode($rawSpec, true) : null;
    if (!is_array($decodedSpec) || !isset($decodedSpec['openapi'])) {
        respond(['ok' => false, 'code' => 'openapi_unavailable', 'error' => 'OpenAPI-спецификация недоступна или повреждена.'], 500);
    }
    respond($decodedSpec);
}

// Публичное машиночитаемое обнаружение возможностей AI (аналог /.well-known/ai-plugin.json, без секретов и локальных путей).
if ($action === 'discovery') {
    apply_security_headers();
    $host = trim((string)($_SERVER['HTTP_HOST'] ?? 'localhost'));
    $proto = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off')
        || strtolower((string)($_SERVER['HTTP_X_FORWARDED_PROTO'] ?? '')) === 'https'
        ? 'https' : 'http';
    $origin = $proto . '://' . $host;
    $scriptDir = rtrim(str_replace('\\', '/', dirname(dirname((string)($_SERVER['SCRIPT_NAME'] ?? '/api/ai.php')))), '/');
    $base = $origin . ($scriptDir === '' ? '' : $scriptDir);
    respond([
        'schema_version' => 'v1',
        'name_for_human' => 'Teeworlds-MRPG-0.6 Editor Studio',
        'name_for_model' => 'mrpg_editor_studio',
        'description_for_human' => 'AI-assisted content creation and review platform for Teeworlds MRPG 0.6 worlds, quests, items, NPCs, mobs, shops, and JSON scenarios.',
        'description_for_model' => 'Model-agnostic REST & MCP JSON-RPC 2.0 platform for inspecting schemas/maps/references and submitting safe Change Requests for Teeworlds-MRPG-0.6. Read manifest first (api/ai.php?action=manifest), validate/preview operations before propose, prefer patch_json for existing JSON edits, and confirm dangerous_actions explicitly.',
        'api_version' => '2.0',
        'onboarding_url' => $base . '/ai/',
        'api' => [
            'type' => 'openapi',
            'url' => $base . '/openapi.json',
            'fallback_url' => $base . '/api/ai.php?action=openapi',
        ],
        'mcp' => [
            'protocol' => '2024-11-05',
            'transport' => 'http_json_rpc_2_0',
            'endpoint' => $base . '/mcp',
            'fallback_endpoint' => $base . '/api/mcp.php',
        ],
        'auth' => [
            'type' => 'bearer_or_guest_session',
            'header' => 'Authorization: Bearer esai_<token>',
            'scopes' => ['read', 'create', 'update', 'delete'],
            'guest_login_endpoint' => $base . '/api/auth.php?action=guest_login',
            'status_endpoint' => $base . '/api/auth.php?action=status',
        ],
        'capabilities' => [
            'model_agnostic' => true,
            'auto_resource_discovery' => true,
            'patch_json' => true,
            'copy' => true,
            'dangerous_confirm_gate' => true,
            'safe_change_requests_only' => true,
        ],
    ]);
}

$ROLE = bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN, EDITOR_ROLE_GUEST], true);
$IS_ADMIN = $ROLE === EDITOR_ROLE_ADMIN;

guard_write_actions($action, ['validate', 'preview', 'propose']);

/** Единый формат структурированной ошибки для AI-клиентов. */
function ai_fail(string $code, string $message, int $status = 400, ?string $field = null, array $extra = []): never
{
    $payload = array_merge([
        'ok' => false,
        'code' => $code,
        'error' => $message,
    ], $extra);
    if ($field !== null && !isset($payload['field'])) {
        $payload['field'] = $field;
    }
    respond($payload, $status);
}

function ai_read_body(): array
{
    $len = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
    if ($len > CR_MAX_BODY_BYTES) {
        ai_fail('payload_too_large', 'Слишком большой запрос (максимум 2 МБ).', 413);
    }
    $raw = editor_request_raw_body(CR_MAX_BODY_BYTES + 1);
    if ($raw === '') {
        return [];
    }
    if (strlen($raw) > CR_MAX_BODY_BYTES) {
        ai_fail('payload_too_large', 'Слишком большой запрос (максимум 2 МБ).', 413);
    }
    $data = json_decode($raw, true, 128);
    if (!is_array($data)) {
        ai_fail('invalid_json', 'Некорректный JSON в теле запроса.', 400);
    }
    return $data;
}

/** Простой фильтр в URL: JSON в `?filter={...}` или пара `?filter_key=...&filter_value=...`. */
function ai_parse_query_filter(): array
{
    $raw = $_GET['filter'] ?? null;
    if (is_string($raw) && trim($raw) !== '') {
        $decoded = json_decode($raw, true);
        if (!is_array($decoded) || (array_is_list($decoded) && $decoded !== [])) {
            throw new CrInputError('Параметр filter должен быть корректным JSON-объектом.', 400, [
                'code' => 'invalid_filter',
                'field' => 'filter',
            ]);
        }
        return $decoded;
    }
    $fk = trim((string)($_GET['filter_key'] ?? ''));
    if ($fk !== '') {
        if (isset($_GET['filter_values']) && is_string($_GET['filter_values'])) {
            $vals = array_map('trim', explode(',', $_GET['filter_values']));
            return [$fk => $vals];
        }
        return [$fk => $_GET['filter_value'] ?? null];
    }
    return [];
}

function ai_validation_http_status(array $errors): int
{
    foreach ($errors as $e) {
        if (($e['code'] ?? '') === 'resource_forbidden') {
            return 403;
        }
    }
    foreach ($errors as $e) {
        if (in_array((string)($e['code'] ?? ''), ['record_not_found', 'scenario_exists', 'scenario_not_found', 'base_hash_mismatch', 'version_conflict'], true)) {
            return 409;
        }
    }
    return 400;
}

try {
    if ($action === '') {
        ai_fail('missing_action', 'Укажите параметр ?action=...', 400, 'action');
    }

    if ($method === 'GET') {
        ai_require_scope('read');
    }

    $account = $IS_ADMIN ? null : editor_guest_user();
    $guestCfg = guest_cfg();
    $effectiveLimits = $IS_ADMIN
        ? ['max_pending' => $guestCfg['max_pending'], 'cooldown' => $guestCfg['cooldown'], 'max_operations' => 500, 'custom' => false]
        : guest_effective_limits($account);

    // ── GET manifest ────────────────────────────────────────────────────────
    if ($action === 'manifest') {
        $includeSchemas = ((string)($_GET['include_schemas'] ?? '1')) !== '0';
        respond([
            'ok' => true,
            'manifest' => ai_build_manifest($ROLE, $account, [
                'nickname' => ['min' => CR_NICK_MIN, 'max' => CR_NICK_MAX],
                'message' => ['min' => CR_MESSAGE_MIN, 'max' => CR_MESSAGE_MAX],
                'max_operations' => $effectiveLimits['max_operations'],
                'max_pending' => $effectiveLimits['max_pending'],
                'cooldown' => $effectiveLimits['cooldown'],
                'max_scenario_bytes' => SCENARIO_MAX_BYTES,
            ], null, $includeSchemas),
        ]);
    }

    // ── GET resources ───────────────────────────────────────────────────────
    if ($action === 'resources') {
        $includeExamples = ((string)($_GET['include_examples'] ?? '1')) !== '0';
        $allowed = ai_allowed_resources($ROLE, null, $includeExamples);
        $target = trim((string)($_GET['resource'] ?? ''));
        if ($target !== '') {
            $known = isset(editor_resources()[$target]) || isset(ai_schema_lookups()[$target]) || in_array($target, ['scenarios', 'maps'], true);
            if (!$known) {
                ai_fail('unknown_resource', "Неизвестный ресурс «{$target}».", 404, 'resource');
            }
            if (!isset($allowed[$target])) {
                ai_fail('resource_forbidden', "Ресурс «{$target}» не включён в права вашей сессии.", 403, 'resource');
            }
            respond(['ok' => true, 'resource' => $target, 'schema' => $allowed[$target]]);
        }
        respond(['ok' => true, 'resources' => $allowed]);
    }

    // ── GET list ────────────────────────────────────────────────────────────
    if ($action === 'list') {
        $resource = trim((string)($_GET['resource'] ?? ''));
        if ($resource === '') {
            ai_fail('missing_required_field', 'Укажите параметр resource.', 400, 'resource');
        }
        $known = isset(editor_resources()[$resource]) || isset(ai_schema_lookups()[$resource]) || in_array($resource, ['scenarios', 'maps'], true);
        if (!$known) {
            ai_fail('unknown_resource', "Неизвестный ресурс «{$resource}».", 404, 'resource');
        }
        if (!ai_can_read_resource($resource, $ROLE)) {
            ai_fail('resource_forbidden', "Доступ к ресурсу «{$resource}» запрещён вашей учётной записи.", 403, 'resource');
        }

        $search = trim((string)($_GET['search'] ?? ($_GET['q'] ?? '')));
        $limit = clamp_int($_GET['limit'] ?? null, 1, 500, 50);
        $offset = clamp_int($_GET['offset'] ?? null, 0, 10000000, 0);
        $maxBytes = clamp_int($_GET['max_bytes'] ?? null, 4096, AI_MAX_RESPONSE_MAX_BYTES, AI_DEFAULT_RESPONSE_MAX_BYTES);

        if ($resource === 'scenarios') {
            $all = scenario_list_files($search);
            $filter = ai_parse_query_filter();
            if ($filter !== []) {
                $allowedScenarioFilters = ['scenario_mode', 'valid_json'];
                foreach (array_keys($filter) as $fk) {
                    if (!in_array((string)$fk, $allowedScenarioFilters, true)) {
                        ai_fail('unknown_filter_field', "Неизвестное поле фильтрации «{$fk}» для ресурса «scenarios».", 400, (string)$fk, ['allowed' => $allowedScenarioFilters]);
                    }
                }
                $all = array_values(array_filter($all, static function (array $item) use ($filter): bool {
                    if (array_key_exists('scenario_mode', $filter) && (($item['summary']['scenario_mode'] ?? null) !== $filter['scenario_mode'])) {
                        return false;
                    }
                    if (array_key_exists('valid_json', $filter) && ((bool)$item['valid_json'] !== (bool)$filter['valid_json'])) {
                        return false;
                    }
                    return true;
                }));
            }
            $total = count($all);
            $slice = array_slice($all, $offset, $limit);
            $page = ai_paginate_slice($slice, $total, $limit, $offset, $maxBytes);
            respond(array_merge(['ok' => true, 'resource' => 'scenarios'], $page));
        }

        $db = db_connect();
        try {
            if (isset(ai_schema_lookups()[$resource])) {
                $res = ai_query_lookup($db, $resource, [
                    'search' => $search,
                    'limit' => $limit,
                    'offset' => $offset,
                    'max_bytes' => $maxBytes,
                ]);
                respond(array_merge(['ok' => true, 'resource' => $resource], $res));
            }

            if ($resource === 'maps') {
                $worldsR = editor_resources()['worlds'];
                $worldsData = ai_query_db_list($db, $worldsR, [
                    'search' => $search,
                    'filter' => ai_parse_query_filter(),
                    'limit' => $limit,
                    'offset' => $offset,
                    'max_bytes' => $maxBytes,
                    'decode' => true,
                ]);
                $rows = [];
                foreach ($worldsData['rows'] as $w) {
                    $wid = (int)$w['ID'];
                    $loaded = ai_map_load_bytes($db, $wid, null);
                    $rows[] = [
                        'world_id' => $wid,
                        'name' => (string)($w['Name'] ?? ''),
                        'path' => (string)($w['Path'] ?? ''),
                        'type' => (string)($w['Type'] ?? ''),
                        'available' => (bool)$loaded['available'],
                        'source' => $loaded['source'],
                        'bytes' => is_string($loaded['bytes']) ? strlen($loaded['bytes']) : null,
                    ];
                }
                $page = ai_paginate_slice($rows, $worldsData['total'], $worldsData['limit'], $worldsData['offset'], $maxBytes);
                respond(array_merge(['ok' => true, 'resource' => 'maps'], $page));
            }

            $R = editor_resources()[$resource] + ['key' => $resource];
            $decode = ((string)($_GET['decode'] ?? '1')) !== '0';
            $res = ai_query_db_list($db, $R, [
                'search' => $search,
                'filter' => ai_parse_query_filter(),
                'fields' => $_GET['fields'] ?? null,
                'limit' => $limit,
                'offset' => $offset,
                'max_bytes' => $maxBytes,
                'decode' => $decode,
            ]);
            respond(array_merge(['ok' => true, 'resource' => $resource], $res));
        } finally {
            $db->close();
        }
    }

    // ── GET get ─────────────────────────────────────────────────────────────
    if ($action === 'get') {
        $resource = trim((string)($_GET['resource'] ?? ''));
        if ($resource === '') {
            ai_fail('missing_required_field', 'Укажите параметр resource.', 400, 'resource');
        }
        $known = isset(editor_resources()[$resource]) || isset(ai_schema_lookups()[$resource]) || in_array($resource, ['scenarios', 'maps'], true);
        if (!$known) {
            ai_fail('unknown_resource', "Неизвестный ресурс «{$resource}».", 404, 'resource');
        }
        if (!ai_can_read_resource($resource, $ROLE)) {
            ai_fail('resource_forbidden', "Доступ к ресурсу «{$resource}» запрещён вашей учётной записи.", 403, 'resource');
        }

        $includeRefs = ((string)($_GET['include_references'] ?? '0')) === '1';

        if ($resource === 'scenarios') {
            $fn = (string)($_GET['file'] ?? ($_GET['filename'] ?? ($_GET['id'] ?? '')));
            if ($fn === '') {
                ai_fail('missing_required_field', 'Укажите имя файла сценария (параметр file или id).', 400, 'id');
            }
            $read = scenario_read_file($fn);
            if (!$read['ok']) {
                $status = ($read['code'] ?? '') === 'scenario_not_found' ? 404 : 400;
                ai_fail((string)($read['code'] ?? 'invalid_filename'), (string)$read['error'], $status, 'id');
            }
            $summaryOnly = ((string)($_GET['summary_only'] ?? '0')) === '1';
            $out = [
                'ok' => true,
                'resource' => 'scenarios',
                'id' => $read['filename'],
                'filename' => $read['filename'],
                'bytes' => $read['bytes'],
                'sha256' => $read['sha256'],
                'updated_at' => $read['updated_at'],
                'summary' => $read['summary'],
            ];
            if (!$summaryOnly) {
                $out['content'] = $read['content'];
            }
            if ($includeRefs) {
                $db = db_connect();
                try {
                    $out['references'] = ai_references_inspect($db, 'scenarios', $read['filename'], $ROLE);
                } finally {
                    $db->close();
                }
            }
            respond($out);
        }

        $db = db_connect();
        try {
            if ($resource === 'maps') {
                $worldParam = $_GET['world'] ?? ($_GET['id'] ?? null);
                $pathParam = isset($_GET['path']) ? trim((string)$_GET['path']) : null;
                $worldId = null;
                if ($worldParam !== null && (string)$worldParam !== '') {
                    $worldId = crud_record_id((string)$worldParam, 'worlds');
                    if ($worldId === null) {
                        ai_fail('invalid_id', 'Некорректный ID мира (ожидается целое >= 0).', 400, 'id');
                    }
                } elseif ($pathParam === null || $pathParam === '') {
                    ai_fail('missing_required_field', 'Укажите world / id или относительный path карты.', 400, 'id');
                }
                $loaded = ai_map_load_bytes($db, $worldId, $pathParam);
                if (empty($loaded['available']) || !is_string($loaded['bytes'])) {
                    respond([
                        'ok' => true,
                        'resource' => 'maps',
                        'world' => $loaded['world'],
                        'available' => false,
                        'reason' => $loaded['reason'] ?? 'Карта недоступна',
                    ]);
                }
                $incEnt = ((string)($_GET['include_entities'] ?? '0')) === '1';
                $px = isset($_GET['x']) && is_numeric($_GET['x']) ? (int)$_GET['x'] : null;
                $py = isset($_GET['y']) && is_numeric($_GET['y']) ? (int)$_GET['y'] : null;
                $parsed = ai_map_parse_datafile($loaded['bytes'], $incEnt, $px, $py);
                respond([
                    'ok' => true,
                    'resource' => 'maps',
                    'world' => $loaded['world'],
                    'available' => true,
                    'source' => $loaded['source'],
                    'filename' => $loaded['basename'] ?? null,
                    'map' => $parsed,
                ]);
            }

            if (isset(ai_schema_lookups()[$resource])) {
                $rawId = (string)($_GET['id'] ?? '');
                $recId = crud_record_id($rawId, $resource);
                if ($recId === null) {
                    ai_fail('invalid_id', 'Некорректный ID записи справочника.', 400, 'id');
                }
                $L = ai_schema_lookups()[$resource];
                $tbl = editor_identifier($L['table'], 'table');
                $cols = implode(', ', array_map(static fn(string $c): string => '`' . editor_identifier($c, 'c') . '`', $L['columns']));
                $stmt = $db->prepare("SELECT $cols FROM `$tbl` WHERE `ID` = ? LIMIT 1");
                $stmt->bind_param('i', $recId);
                $stmt->execute();
                $row = $stmt->get_result()->fetch_assoc();
                $stmt->close();
                if (!$row) {
                    ai_fail('record_not_found', "Запись {$resource} #{$recId} не найдена.", 404, 'id');
                }
                respond(['ok' => true, 'resource' => $resource, 'id' => $recId, 'record' => $row]);
            }

            $rawId = (string)($_GET['id'] ?? '');
            $recId = crud_record_id($rawId, $resource);
            if ($recId === null) {
                ai_fail('invalid_id', 'Некорректный ID записи.', 400, 'id');
            }
            $R = editor_resources()[$resource] + ['key' => $resource];
            $row = crud_fetch_row($db, $R, $recId);
            if ($row === null) {
                ai_fail('record_not_found', "Запись {$R['title']} #{$recId} не найдена.", 404, 'id');
            }
            $decoded = ((string)($_GET['decode'] ?? '1')) !== '0'
                ? decode_json_cols($row, $R['json'] ?? [])
                : $row;
            $decoded = ai_project_record_fields($decoded, $R, $_GET['fields'] ?? null);
            $out = ['ok' => true, 'resource' => $resource, 'id' => $recId, 'record' => $decoded];
            if ($includeRefs) {
                $out['references'] = ai_references_inspect($db, $resource, $recId, $ROLE);
            }
            respond($out);
        } finally {
            $db->close();
        }
    }

    // ── GET references ──────────────────────────────────────────────────────
    if ($action === 'references') {
        $source = trim((string)($_GET['source'] ?? ''));
        if ($source !== '') {
            if (!isset(ai_lookup_tabs()[$source])) {
                ai_fail('unknown_source', "Неизвестный справочник «{$source}».", 400, 'source');
            }
            if (!ai_can_read_lookup_source($source, $ROLE)) {
                ai_fail('resource_forbidden', "Справочник «{$source}» не входит в ваши права.", 403, 'source');
            }
            $maxBytes = clamp_int($_GET['max_bytes'] ?? null, 4096, AI_MAX_RESPONSE_MAX_BYTES, AI_DEFAULT_RESPONSE_MAX_BYTES);
            $db = db_connect();
            try {
                $res = ai_query_lookup($db, $source, [
                    'search' => trim((string)($_GET['search'] ?? ($_GET['q'] ?? ''))),
                    'limit' => $_GET['limit'] ?? 200,
                    'offset' => $_GET['offset'] ?? 0,
                    'max_bytes' => $maxBytes,
                ]);
                respond(array_merge(['ok' => true, 'mode' => 'lookup', 'source' => $source], $res));
            } finally {
                $db->close();
            }
        }

        $resource = trim((string)($_GET['resource'] ?? ''));
        $rawId = trim((string)($_GET['id'] ?? ($_GET['file'] ?? '')));
        if ($resource !== '' && $rawId !== '') {
            $known = isset(editor_resources()[$resource]) || isset(ai_schema_lookups()[$resource]) || $resource === 'scenarios';
            if (!$known) {
                ai_fail('unknown_resource', "Неизвестный ресурс «{$resource}».", 404, 'resource');
            }
            if (!ai_can_read_resource($resource, $ROLE)) {
                ai_fail('resource_forbidden', "Доступ к ресурсу «{$resource}» запрещён.", 403, 'resource');
            }
            $idArg = $resource === 'scenarios' ? $rawId : crud_record_id($rawId, $resource);
            if ($idArg === null) {
                ai_fail('invalid_id', 'Некорректный ID записи.', 400, 'id');
            }
            $db = db_connect();
            try {
                $refs = ai_references_inspect($db, $resource, $idArg, $ROLE);
                respond(['ok' => true, 'mode' => 'record', 'references' => $refs]);
            } finally {
                $db->close();
            }
        }

        // Граф зависимостей схемы (без чтения строк БД).
        $allowed = ai_allowed_resources($ROLE);
        $refSchema = editor_reference_schema();
        $graph = [];
        foreach ($allowed as $key => $info) {
            if (($info['kind'] ?? '') !== 'database') {
                continue;
            }
            $R = editor_resources()[$key];
            $rev = array_values(array_filter(
                editor_reverse_refs($R['table']),
                static function (array $r) use ($allowed): bool {
                    $resKey = ai_table_to_resource_map()[$r['table']] ?? '';
                    return $resKey !== '' && isset($allowed[$resKey]);
                }
            ));
            $graph[$key] = [
                'table' => $R['table'],
                'direct_refs' => $R['refs'] ?? [],
                'embedded_rules' => $refSchema[$key] ?? [],
                'inbound_foreign_keys' => $rev,
            ];
        }
        respond(['ok' => true, 'mode' => 'schema', 'graph' => $graph]);
    }

    // ── GET history ─────────────────────────────────────────────────────────
    if ($action === 'history') {
        $limit = clamp_int($_GET['limit'] ?? null, 1, 200, 30);
        $offset = clamp_int($_GET['offset'] ?? null, 0, 100000, 0);
        $resFilter = trim((string)($_GET['resource'] ?? ''));

        if ($IS_ADMIN) {
            $hist = store_history_list(['resource' => $resFilter], $limit, $offset);
            $audit = store_audit_list(['q' => $resFilter], $limit, $offset);
            respond([
                'ok' => true,
                'role' => 'admin',
                'db_history' => $hist['rows'],
                'db_history_total' => $hist['total'],
                'audit' => $audit['rows'],
                'audit_total' => $audit['total'],
            ]);
        }

        $guestId = (string)($account['id'] ?? '');
        $reqList = cr_query_list([
            'guest_id' => $guestId,
            'resource' => $resFilter,
        ], $limit, $offset);
        $auditList = store_audit_list(['actor_id' => $guestId], $limit, $offset);
        $auditSafe = array_map(static function (array $row): array {
            unset($row['ip']);
            return $row;
        }, $auditList['rows']);
        respond([
            'ok' => true,
            'role' => 'guest',
            'requests' => array_map(static fn(array $r): array => cr_list_item($r, false), $reqList['rows']),
            'requests_total' => $reqList['total'],
            'audit' => $auditSafe,
            'audit_total' => $auditList['total'],
        ]);
    }

    // ── POST validate ───────────────────────────────────────────────────────
    if ($action === 'validate') {
        $body = ai_read_body();
        $rawOps = ai_extract_operations_payload($body);
        $db = db_connect();
        try {
            $res = ai_validate_operations($db, $rawOps, $ROLE, null, null, 500);
        } finally {
            $db->close();
        }
        respond([
            'ok' => true,
            'valid' => $res['valid'],
            'errors' => $res['errors'],
            'warnings' => $res['warnings'],
            'required_scopes' => $res['required_scopes'],
            'requires_confirmation' => $res['requires_confirmation'],
            'dangerous_actions' => $res['dangerous_actions'],
            'normalized_operations' => $res['normalized_operations'],
        ]);
    }

    // ── POST preview ────────────────────────────────────────────────────────
    if ($action === 'preview') {
        cr_preview_throttle($IS_ADMIN);
        $body = ai_read_body();
        $rawOps = ai_extract_operations_payload($body);
        $maxOps = $IS_ADMIN ? 500 : (int)$effectiveLimits['max_operations'];
        if (count($rawOps) > $maxOps) {
            ai_fail('too_many_operations', "Слишком много операций в одном запросе (максимум {$maxOps}).", 400, 'operations');
        }

        $db = db_connect();
        try {
            $val = ai_validate_operations($db, $rawOps, $ROLE, null, null, $maxOps);
            if (!$val['valid']) {
                $first = $val['errors'][0];
                $status = ai_validation_http_status($val['errors']);
                ai_fail(
                    (string)($first['code'] ?? 'validation_failed'),
                    (string)($first['message'] ?? 'Проверка операций завершилась с ошибками.'),
                    $status,
                    $first['field'] ?? null,
                    ['errors' => $val['errors'], 'warnings' => $val['warnings']]
                );
            }
            $normOps = cr_normalize_ops($val['normalized_operations'], $maxOps);
            if (!$IS_ADMIN) {
                cr_assert_guest_resources($normOps);
            }
            $analysis = ai_enrich_analysis_diffs(cr_public_analysis(cr_analyze($db, $normOps)));
        } finally {
            $db->close();
        }

        respond([
            'ok' => true,
            'valid' => true,
            'analysis' => $analysis,
            'warnings' => $val['warnings'],
            'required_scopes' => $val['required_scopes'],
            'requires_confirmation' => $val['requires_confirmation'],
            'dangerous_actions' => $val['dangerous_actions'],
            'normalized_operations' => $val['normalized_operations'],
        ]);
    }

    // ── POST propose ────────────────────────────────────────────────────────
    if ($action === 'propose') {
        if ($IS_ADMIN) {
            ai_fail(
                'guest_session_required',
                'Для создания запросов на изменение AI должен работать через гостевую сессию Editor Studio, чтобы изменения проходили проверку администратора.',
                403
            );
        }
        $body = ai_read_body();
        $rawOps = ai_extract_operations_payload($body);
        $maxOps = (int)$effectiveLimits['max_operations'];
        if (count($rawOps) > $maxOps) {
            ai_fail('too_many_operations', "Слишком много операций в одном запросе (максимум {$maxOps}).", 400, 'operations');
        }

        $requestId = trim((string)($body['request_id'] ?? ($body['id'] ?? '')));
        $isRevise = $requestId !== '';

        $idemKey = trim((string)($body['idempotency_key'] ?? ($body['idempotencyKey'] ?? '')));
        if ($idemKey !== '' && !preg_match('/^[A-Za-z0-9_-]{8,80}$/', $idemKey)) {
            ai_fail('invalid_idempotency_key', 'Некорректный формат idempotency_key (ожидается 8–80 символов A-Za-z0-9_-).', 400, 'idempotency_key');
        }
        $keyHash = (!$isRevise && $idemKey !== '')
            ? hash('sha256', (string)($account['id'] ?? '') . '|' . $idemKey)
            : '';

        if ($keyHash !== '') {
            $existingId = store_submit_key_get($keyHash);
            if ($existingId !== null) {
                $existing = cr_load($existingId);
                if ($existing && cr_guest_owns($existing, $account)) {
                    respond([
                        'ok' => true,
                        'duplicate' => true,
                        'warnings' => [],
                        'request' => cr_full_view($existing, false),
                    ]);
                }
            }
        }

        $nextMessage = null;
        if (!$isRevise) {
            $nickRaw = $body['nickname'] ?? ($account['name'] ?? ($account['login'] ?? 'ai_guest'));
            $nickCleaned = cr_clean_text($nickRaw, false);
            if (cr_strlen($nickCleaned) < CR_NICK_MIN) {
                $nickCleaned = 'AI Guest';
            }
            $nickname = cr_validate_nickname($nickCleaned);
            $msgRaw = $body['message'] ?? ($body['summary'] ?? '');
            try {
                $message = cr_validate_message($msgRaw);
            } catch (CrInputError $e) {
                ai_fail('invalid_message', $e->getMessage(), 400, 'message');
            }
            // Ранний cooldown-чек (сессия + SQLite-счётчик для stateless Bearer-токенов).
            $lastSession = (int)($_SESSION['editor_studio_guest_last_submit'] ?? 0);
            $acctCooldownKey = 'ai-submit|' . (string)($account['id'] ?? '');
            $lastAccount = store_rate_get($acctCooldownKey);
            $lastAny = max($lastSession, $lastAccount);
            if ((int)$effectiveLimits['cooldown'] > 0 && $lastAny + (int)$effectiveLimits['cooldown'] > time()) {
                respond_rate_limited($lastAny + (int)$effectiveLimits['cooldown'] - time(), 'Подождите перед отправкой следующего запроса.');
            }
        } elseif (isset($body['message']) || isset($body['summary'])) {
            $msgRaw = $body['message'] ?? $body['summary'];
            try {
                $nextMessage = cr_validate_message($msgRaw);
            } catch (CrInputError $e) {
                ai_fail('invalid_message', $e->getMessage(), 400, 'message');
            }
        }

        $db = db_connect();
        try {
            $val = ai_validate_operations($db, $rawOps, $ROLE, null, null, $maxOps);
            if (!$val['valid']) {
                $first = $val['errors'][0];
                $status = ai_validation_http_status($val['errors']);
                ai_fail(
                    (string)($first['code'] ?? 'validation_failed'),
                    (string)($first['message'] ?? 'Проверка операций завершилась с ошибками.'),
                    $status,
                    $first['field'] ?? null,
                    ['errors' => $val['errors'], 'warnings' => $val['warnings']]
                );
            }
            ai_assert_operation_scopes($val['required_scopes']);
            if (!empty($val['requires_confirmation']) && empty($body['confirm_dangerous'])) {
                ai_fail(
                    'confirmation_required',
                    'Операция содержит потенциально опасные изменения (удаление, массовый пакет или критический ресурс). Проверьте dangerous_actions и повторите запрос с confirm_dangerous=true.',
                    400,
                    'confirm_dangerous',
                    [
                        'requires_confirmation' => true,
                        'dangerous_actions' => $val['dangerous_actions'],
                        'warnings' => $val['warnings'],
                    ]
                );
            }
            $normOps = cr_normalize_ops($val['normalized_operations'], $maxOps);
            cr_assert_guest_resources($normOps);
            [$kept, $analysis] = cr_prepare_for_submit($db, $normOps);
        } finally {
            $db->close();
        }

        $ip = get_request_ip();

        if ($isRevise) {
            $expectedVersion = (int)($body['version'] ?? 0);
            if ($expectedVersion < 1) {
                ai_fail('missing_required_field', 'Для обновления запроса укажите актуальное поле version.', 400, 'version');
            }
            $actorId = (string)($account['id'] ?? '');
            $actorName = (string)($account['name'] ?? ($account['login'] ?? 'guest'));
            $updated = cr_with_lock(function () use ($requestId, $expectedVersion, $kept, $analysis, $nextMessage, $actorId, $actorName) {
                $req = cr_load_visible($requestId, false);
                if ($req['status'] !== 'pending') {
                    throw new CrInputError('Можно изменить только ожидающий запрос (текущий статус: ' . $req['status'] . ').', 409, ['code' => 'request_not_pending']);
                }
                $curVer = max(1, (int)($req['version'] ?? 1));
                if ($expectedVersion !== $curVer) {
                    throw new CrInputError('Запрос был изменён другим действием. Загрузите актуальную версию.', 409, [
                        'code' => 'version_conflict',
                        'version' => $curVer,
                    ]);
                }
                $nextVer = $curVer + 1;
                $at = cr_now();
                $enrichedAnalysis = ai_enrich_analysis_diffs(cr_public_analysis($analysis));
                $fieldsChanged = (int)($enrichedAnalysis['summary']['fields'] ?? 0);
                if ($nextMessage !== null) {
                    $req['message'] = $nextMessage;
                }
                $req['operations'] = $kept;
                $req['analysis'] = $enrichedAnalysis;
                $req['version'] = $nextVer;
                $req['updatedAt'] = $at;
                $req['lastRevision'] = [
                    'version' => $nextVer,
                    'role' => 'guest',
                    'by' => $actorName,
                    'at' => $at,
                ];
                $req['revisions'] = array_slice(array_merge(is_array($req['revisions'] ?? null) ? $req['revisions'] : [], [[
                    'version' => $nextVer,
                    'role' => 'guest',
                    'by' => $actorName,
                    'at' => $at,
                    'operations' => count($kept),
                    'fields' => $fieldsChanged,
                ]]), -20);
                cr_save($req);
                cr_msg_add(
                    $requestId,
                    'guest',
                    $actorId,
                    $actorName,
                    "Обновлён состав запроса через AI API: версия {$nextVer}, операций: " . count($kept) . ", изменённых полей: {$fieldsChanged}."
                );
                return $req;
            });
            editor_audit('ai.propose.revise', $requestId, ['version' => (int)$updated['version'], 'ops' => count($kept)]);
            respond([
                'ok' => true,
                'revised' => true,
                'duplicate' => false,
                'warnings' => $val['warnings'],
                'request' => cr_full_view($updated, false),
            ]);
        }

        // Новое предложение
        $res = cr_with_lock(function () use (
            $keyHash,
            $account,
            $effectiveLimits,
            $nickname,
            $message,
            $kept,
            $analysis,
            $ip
        ) {
            if ($keyHash !== '') {
                $existingId = store_submit_key_get($keyHash);
                if ($existingId !== null) {
                    $existing = cr_load($existingId);
                    if ($existing && cr_guest_owns($existing, $account)) {
                        return ['req' => $existing, 'duplicate' => true];
                    }
                }
            }

            $pendingMine = cr_pending_count((string)($account['id'] ?? ''));
            if ($pendingMine >= (int)$effectiveLimits['max_pending']) {
                throw new CrInputError(
                    "У вас уже {$pendingMine} запрос(ов) на рассмотрении (лимит {$effectiveLimits['max_pending']}). Дождитесь решения администратора или отзовите запрос.",
                    429,
                    ['code' => 'too_many_pending']
                );
            }
            if (cr_pending_count() >= CR_GLOBAL_PENDING_MAX) {
                throw new CrInputError('Очередь запросов заполнена. Попробуйте позже или попросите администратора разобрать накопившиеся запросы.', 429, ['code' => 'queue_full']);
            }
            $left = cr_ip_cooldown_left($ip, (int)$effectiveLimits['cooldown'], true);
            if ($left > 0) {
                throw new CrInputError('Подождите перед отправкой следующего запроса.', 429, ['code' => 'rate_limited', 'retry_after' => $left]);
            }

            $now = cr_now();
            $req = [
                'id' => cr_new_id(),
                'status' => 'pending',
                'version' => 1,
                'createdAt' => $now,
                'updatedAt' => $now,
                'author' => [
                    'nickname' => $nickname,
                    'guestId' => (string)($account['id'] ?? ''),
                    'guestLogin' => (string)($account['login'] ?? ''),
                    'guestName' => (string)($account['name'] ?? ''),
                    'ip' => $ip,
                    'userAgent' => cr_substr(cr_clean_text($_SERVER['HTTP_USER_AGENT'] ?? '', false), 200),
                    'via' => editor_ai_token_actor() !== null ? 'ai_token' : 'ai_api',
                ],
                'message' => $message,
                'operations' => $kept,
                'analysis' => ai_enrich_analysis_diffs(cr_public_analysis($analysis)),
                'review' => null,
            ];
            cr_save($req);
            if ($keyHash !== '') {
                store_submit_key_put($keyHash, (string)$req['id'], (string)($account['id'] ?? ''));
                store_submit_keys_prune();
            }
            return ['req' => $req, 'duplicate' => false];
        });

        $_SESSION['editor_studio_guest_last_submit'] = time();
        if (!$res['duplicate']) {
            store_rate_touch('ai-submit|' . (string)($account['id'] ?? ''), time());
            editor_audit('ai.propose', (string)$res['req']['id'], ['ops' => count($kept), 'nickname' => $nickname]);
        }
        respond([
            'ok' => true,
            'duplicate' => $res['duplicate'],
            'warnings' => $val['warnings'],
            'request' => cr_full_view($res['req'], false),
        ]);
    }

    ai_fail('unknown_action', "Неизвестное действие API: «{$action}».", 400, 'action');
} catch (CrInputError $e) {
    if ($e->httpCode === 429 && !empty($e->extra['retry_after'])) {
        header('Retry-After: ' . (int)$e->extra['retry_after']);
    }
    $code = (string)($e->extra['code'] ?? ($e->httpCode === 403 ? 'forbidden' : ($e->httpCode === 409 ? 'conflict' : 'bad_request')));
    ai_fail($code, $e->getMessage(), $e->httpCode, $e->extra['field'] ?? null, $e->extra);
} catch (EditorUserError $e) {
    ai_fail('user_error', $e->getMessage(), 400);
} catch (Throwable $e) {
    editor_log_error($e);
    ai_fail('internal_error', editor_safe_message($e), 500);
}
