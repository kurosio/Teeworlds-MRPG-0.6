<?php
// Universal Model Context Protocol (MCP) Server over HTTP (JSON-RPC 2.0).
// Endpoint: api/mcp.php (also routed at /mcp)
//
// Supports MCP protocol 2024-11-05:
//   - initialize, ping
//   - tools/list, tools/call (11 typed MRPG tools)
//   - resources/list, resources/read (dynamic schemas + manifest + OpenAPI)
//   - prompts/list, prompts/get (model-agnostic workflow prompts)
//
// Auth: Authorization: Bearer esai_<token> (or X-AI-Token) or Editor Studio session cookie.
// Unauthenticated `initialize`, `ping`, `tools/list`, and `prompts/list` return capability metadata only
// (no game data, no secrets); all data/write calls require authentication + token scopes.

declare(strict_types=1);

require_once __DIR__ . '/ai-lib.php';
require_once __DIR__ . '/db-history-lib.php';

const MCP_PROTOCOL_VERSION = '2024-11-05';
const MCP_SERVER_NAME = 'mrpg-editor-studio-mcp';
const MCP_SERVER_VERSION = '2.0.0';

function mcp_respond_rpc(mixed $id, mixed $result = null, ?array $error = null, int $status = 200): never
{
    $payload = ['jsonrpc' => '2.0', 'id' => $id];
    if ($error !== null) {
        $payload['error'] = $error;
    } else {
        $payload['result'] = $result ?? new stdClass();
    }
    respond($payload, $status);
}

function mcp_tool_catalog(): array
{
    return [
        [
            'name' => 'get_manifest',
            'description' => 'Inspect Editor Studio AI API capabilities, role, scopes, granted pages, limits, and resource directory.',
            'inputSchema' => [
                'type' => 'object',
                'properties' => [
                    'include_schemas' => ['type' => 'boolean', 'default' => false],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'get_schema',
            'description' => 'Get full field schema, enums, DSL syntax, foreign-key references, and valid examples for one resource or all allowed resources.',
            'inputSchema' => [
                'type' => 'object',
                'properties' => [
                    'resource' => ['type' => 'string', 'description' => 'Resource key (items, worlds, scenarios, quests, bots_npc, etc.)'],
                    'include_examples' => ['type' => 'boolean', 'default' => true],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'list_records',
            'description' => 'Search and paginate existing DB records, scenario files, lookups, or maps.',
            'inputSchema' => [
                'type' => 'object',
                'required' => ['resource'],
                'properties' => [
                    'resource' => ['type' => 'string'],
                    'search' => ['type' => 'string'],
                    'filter' => ['type' => 'object'],
                    'fields' => ['anyOf' => [['type' => 'string'], ['type' => 'array', 'items' => ['type' => 'string']]]],
                    'limit' => ['type' => 'integer', 'minimum' => 1, 'maximum' => 500, 'default' => 50],
                    'offset' => ['type' => 'integer', 'minimum' => 0, 'default' => 0],
                    'max_bytes' => ['type' => 'integer', 'minimum' => 4096, 'maximum' => 2097152],
                    'decode' => ['type' => 'boolean', 'default' => true],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'get_record',
            'description' => 'Read a single DB record by ID, scenario JSON by filename (with sha256 baseHash), or inspect a DDNet .map binary.',
            'inputSchema' => [
                'type' => 'object',
                'required' => ['resource'],
                'properties' => [
                    'resource' => ['type' => 'string'],
                    'id' => ['anyOf' => [['type' => 'integer'], ['type' => 'string']]],
                    'filename' => ['type' => 'string'],
                    'fields' => ['anyOf' => [['type' => 'string'], ['type' => 'array', 'items' => ['type' => 'string']]]],
                    'summary_only' => ['type' => 'boolean', 'default' => false],
                    'include_references' => ['type' => 'boolean', 'default' => true],
                    'include_entities' => ['type' => 'boolean', 'default' => false],
                    'x' => ['type' => 'integer'],
                    'y' => ['type' => 'integer'],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'get_references',
            'description' => 'Inspect inbound/outbound foreign-key & JSON/scenario references for a record, lookup table entries (?source=...), or full schema graph.',
            'inputSchema' => [
                'type' => 'object',
                'properties' => [
                    'resource' => ['type' => 'string'],
                    'id' => ['anyOf' => [['type' => 'integer'], ['type' => 'string']]],
                    'source' => ['type' => 'string'],
                    'search' => ['type' => 'string'],
                    'limit' => ['type' => 'integer', 'minimum' => 1, 'maximum' => 500],
                    'offset' => ['type' => 'integer', 'minimum' => 0],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'validate_operations',
            'description' => 'Validate create/update/copy/delete/patch_json operations against DB schema, MRPG server compatibility, references, and dangerous action rules without writing anything.',
            'inputSchema' => [
                'type' => 'object',
                'required' => ['operations'],
                'properties' => [
                    'operations' => ['type' => 'array', 'minItems' => 1],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'preview_operations',
            'description' => 'Dry-run analysis returning exact field diffs, path-level json_diff, dependency graph, warnings, and dangerous_actions without mutating storage.',
            'inputSchema' => [
                'type' => 'object',
                'required' => ['operations'],
                'properties' => [
                    'operations' => ['type' => 'array', 'minItems' => 1],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'propose_changes',
            'description' => 'Submit or revise a Change Request in Editor Studio SQLite queue for administrator approval. Requires confirm_dangerous=true when deleting, clearing JSON, bulk >10 ops, or editing high-impact resources.',
            'inputSchema' => [
                'type' => 'object',
                'required' => ['operations'],
                'properties' => [
                    'message' => ['type' => 'string', 'minLength' => 5],
                    'nickname' => ['type' => 'string'],
                    'operations' => ['type' => 'array', 'minItems' => 1],
                    'confirm_dangerous' => ['type' => 'boolean', 'default' => false],
                    'idempotency_key' => ['type' => 'string'],
                    'request_id' => ['type' => 'string'],
                    'version' => ['type' => 'integer', 'minimum' => 1],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'copy_record',
            'description' => 'Safely duplicate an existing DB record or scenario JSON file with optional field overrides or JSON merge patch; previews or submits a Change Request.',
            'inputSchema' => [
                'type' => 'object',
                'required' => ['resource'],
                'properties' => [
                    'resource' => ['type' => 'string'],
                    'source_id' => ['type' => 'integer', 'minimum' => 0],
                    'source_filename' => ['type' => 'string'],
                    'filename' => ['type' => 'string'],
                    'data' => ['type' => 'object'],
                    'patch_json' => ['type' => 'object'],
                    'propose' => ['type' => 'boolean', 'default' => false],
                    'message' => ['type' => 'string'],
                    'idempotency_key' => ['type' => 'string'],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'patch_json_record',
            'description' => 'Safely merge-patch (RFC 7396) a JSON column (items.Data, scenario JSON, quest_bots.ScenarioData, etc.) without wiping untouched sibling keys.',
            'inputSchema' => [
                'type' => 'object',
                'required' => ['resource', 'patch_json'],
                'properties' => [
                    'resource' => ['type' => 'string'],
                    'id' => ['anyOf' => [['type' => 'integer'], ['type' => 'string']]],
                    'filename' => ['type' => 'string'],
                    'patch_json' => ['type' => 'object'],
                    'data' => ['type' => 'object'],
                    'baseHash' => ['type' => 'string'],
                    'expected_before' => ['type' => 'object'],
                    'propose' => ['type' => 'boolean', 'default' => false],
                    'confirm_dangerous' => ['type' => 'boolean', 'default' => false],
                    'message' => ['type' => 'string'],
                    'idempotency_key' => ['type' => 'string'],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'get_history',
            'description' => 'Read the recent action audit log and Change Requests visible to the current AI token or session.',
            'inputSchema' => [
                'type' => 'object',
                'properties' => [
                    'resource' => ['type' => 'string'],
                    'limit' => ['type' => 'integer', 'minimum' => 1, 'maximum' => 200, 'default' => 20],
                    'offset' => ['type' => 'integer', 'minimum' => 0, 'default' => 0],
                ],
                'additionalProperties' => false,
            ],
        ],
    ];
}

function mcp_prompts_catalog(): array
{
    return [
        [
            'name' => 'create_mrpg_item',
            'description' => 'Create a server-compatible MRPG item and optional craft/scenario safely.',
            'arguments' => [
                ['name' => 'concept', 'description' => 'Item idea, stats, and rarity/purpose', 'required' => true],
            ],
        ],
        [
            'name' => 'create_mrpg_quest_scene',
            'description' => 'Design a quest scenario JSON + quest/NPC linkage without broken step links or invalid coordinates.',
            'arguments' => [
                ['name' => 'world_id', 'description' => 'Target world ID', 'required' => true],
                ['name' => 'goal', 'description' => 'Quest objective and rewards', 'required' => true],
            ],
        ],
        [
            'name' => 'safe_json_patch_edit',
            'description' => 'Inspect an existing object and apply minimal patch_json without losing unrelated fields.',
            'arguments' => [
                ['name' => 'resource', 'description' => 'Target resource name', 'required' => true],
                ['name' => 'id', 'description' => 'Target record ID or scenario filename', 'required' => true],
            ],
        ],
    ];
}

function mcp_tool_result(array $payload, bool $isError = false): array
{
    return [
        'content' => [
            [
                'type' => 'text',
                'text' => (string)json_encode($payload, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES | JSON_PRETTY_PRINT),
            ],
        ],
        'structuredContent' => $payload,
        'isError' => $isError,
    ];
}

$method = strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET'));

if ($method === 'GET') {
    apply_security_headers();
    respond([
        'ok' => true,
        'protocol' => 'mcp',
        'protocolVersion' => MCP_PROTOCOL_VERSION,
        'transport' => 'http_json_rpc_2_0',
        'serverInfo' => [
            'name' => MCP_SERVER_NAME,
            'version' => MCP_SERVER_VERSION,
        ],
        'auth' => [
            'methods' => ['bearer_token', 'guest_session'],
            'header' => 'Authorization: Bearer esai_<token>',
        ],
        'tools' => array_map(static fn(array $t): string => $t['name'], mcp_tool_catalog()),
    ]);
}

if ($method !== 'POST') {
    respond(['ok' => false, 'code' => 'method_not_allowed', 'error' => 'Используйте POST (JSON-RPC 2.0) или GET.'], 405);
}

$rawBody = editor_request_raw_body(CR_MAX_BODY_BYTES + 1);
if ($rawBody === '' || strlen($rawBody) > CR_MAX_BODY_BYTES) {
    mcp_respond_rpc(null, null, ['code' => -32600, 'message' => 'Empty or oversized JSON-RPC request body.'], 400);
}
$rpc = json_decode($rawBody, true, 128);
if (!is_array($rpc) || (($rpc['jsonrpc'] ?? '') !== '2.0') || empty($rpc['method']) || !is_string($rpc['method'])) {
    mcp_respond_rpc($rpc['id'] ?? null, null, ['code' => -32600, 'message' => 'Invalid JSON-RPC 2.0 request.'], 400);
}

$rpcId = $rpc['id'] ?? null;
$rpcMethod = (string)$rpc['method'];
$rpcParams = is_array($rpc['params'] ?? null) ? $rpc['params'] : [];

// JSON-RPC notifications (no id) acknowledge cleanly.
if (!array_key_exists('id', $rpc) || str_starts_with($rpcMethod, 'notifications/')) {
    apply_security_headers();
    http_response_code(204);
    exit;
}

// Discovery-only MCP methods before requiring auth (still validate Bearer header if client sent one!).
$publicMethods = ['initialize', 'ping', 'tools/list', 'prompts/list', 'prompts/get'];
if (in_array($rpcMethod, $publicMethods, true)) {
    if (editor_has_bearer_header()) {
        bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN, EDITOR_ROLE_GUEST], true);
    } else {
        apply_security_headers();
    }

    if ($rpcMethod === 'initialize') {
        mcp_respond_rpc($rpcId, [
            'protocolVersion' => MCP_PROTOCOL_VERSION,
            'capabilities' => [
                'tools' => ['listChanged' => false],
                'resources' => ['listChanged' => false],
                'prompts' => ['listChanged' => false],
            ],
            'serverInfo' => [
                'name' => MCP_SERVER_NAME,
                'version' => MCP_SERVER_VERSION,
            ],
            'instructions' => 'Call get_manifest and get_schema first, use list_records/get_record to inspect existing world/item/scenario IDs, validate/preview changes before propose_changes, and set confirm_dangerous=true after reviewing dangerous_actions.',
        ]);
    }
    if ($rpcMethod === 'ping') {
        mcp_respond_rpc($rpcId, new stdClass());
    }
    if ($rpcMethod === 'tools/list') {
        mcp_respond_rpc($rpcId, ['tools' => mcp_tool_catalog()]);
    }
    if ($rpcMethod === 'prompts/list') {
        mcp_respond_rpc($rpcId, ['prompts' => mcp_prompts_catalog()]);
    }
    if ($rpcMethod === 'prompts/get') {
        $pName = (string)($rpcParams['name'] ?? '');
        $pArgs = is_array($rpcParams['arguments'] ?? null) ? $rpcParams['arguments'] : [];
        foreach (mcp_prompts_catalog() as $p) {
            if ($p['name'] === $pName) {
                $argTxt = json_encode($pArgs, JSON_UNESCAPED_UNICODE);
                mcp_respond_rpc($rpcId, [
                    'description' => $p['description'],
                    'messages' => [[
                        'role' => 'user',
                        'content' => [
                            'type' => 'text',
                            'text' => "Follow MRPG Editor Studio safety workflow ({$pName}): 1) Call get_schema + list_records/get_references; 2) Draft valid operations using tempId / patch_json; 3) Call preview_operations; 4) Call propose_changes. Arguments: {$argTxt}",
                        ],
                    ]],
                ]);
            }
        }
        mcp_respond_rpc($rpcId, null, ['code' => -32602, 'message' => "Unknown prompt: {$pName}"], 400);
    }
}

// All data/execution MCP methods require authenticated guest/token/admin access.
$ROLE = bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN, EDITOR_ROLE_GUEST], true);
$IS_ADMIN = $ROLE === EDITOR_ROLE_ADMIN;
$account = $IS_ADMIN ? null : editor_guest_user();
$guestCfg = guest_cfg();
$effectiveLimits = $IS_ADMIN
    ? ['max_pending' => $guestCfg['max_pending'], 'cooldown' => $guestCfg['cooldown'], 'max_operations' => 500, 'custom' => false]
    : guest_effective_limits($account);

try {
    // ── MCP resources/list & resources/read ─────────────────────────────────
    if ($rpcMethod === 'resources/list') {
        ai_require_scope('read');
        $allowed = ai_allowed_resources($ROLE, null, false);
        $list = [
            ['uri' => 'mrpg://manifest', 'name' => 'AI Capabilities Manifest', 'mimeType' => 'application/json'],
            ['uri' => 'mrpg://openapi', 'name' => 'OpenAPI 3.1.0 Specification', 'mimeType' => 'application/json'],
        ];
        foreach ($allowed as $resKey => $meta) {
            $list[] = [
                'uri' => 'mrpg://schema/' . $resKey,
                'name' => (string)($meta['title'] ?? $resKey) . " ({$resKey})",
                'mimeType' => 'application/json',
            ];
        }
        mcp_respond_rpc($rpcId, ['resources' => $list]);
    }

    if ($rpcMethod === 'resources/read') {
        ai_require_scope('read');
        $uri = trim((string)($rpcParams['uri'] ?? ''));
        if ($uri === 'mrpg://manifest') {
            $man = ai_build_manifest($ROLE, $account, [
                'nickname' => ['min' => CR_NICK_MIN, 'max' => CR_NICK_MAX],
                'message' => ['min' => CR_MESSAGE_MIN, 'max' => CR_MESSAGE_MAX],
                'max_operations' => $effectiveLimits['max_operations'],
                'max_pending' => $effectiveLimits['max_pending'],
                'cooldown' => $effectiveLimits['cooldown'],
                'max_scenario_bytes' => SCENARIO_MAX_BYTES,
            ], null, true);
            mcp_respond_rpc($rpcId, [
                'contents' => [['uri' => $uri, 'mimeType' => 'application/json', 'text' => (string)json_encode($man, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES)]],
            ]);
        }
        if ($uri === 'mrpg://openapi') {
            $spec = @file_get_contents(__DIR__ . '/ai-openapi.json') ?: '{}';
            mcp_respond_rpc($rpcId, [
                'contents' => [['uri' => $uri, 'mimeType' => 'application/json', 'text' => $spec]],
            ]);
        }
        if (str_starts_with($uri, 'mrpg://schema/')) {
            $resKey = substr($uri, strlen('mrpg://schema/'));
            $allowed = ai_allowed_resources($ROLE, null, true);
            if (!isset($allowed[$resKey])) {
                mcp_respond_rpc($rpcId, null, ['code' => -32002, 'message' => "Resource not accessible or unknown: {$resKey}"], 403);
            }
            mcp_respond_rpc($rpcId, [
                'contents' => [['uri' => $uri, 'mimeType' => 'application/json', 'text' => (string)json_encode($allowed[$resKey], JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES)]],
            ]);
        }
        mcp_respond_rpc($rpcId, null, ['code' => -32602, 'message' => "Unknown resource URI: {$uri}"], 400);
    }

    if ($rpcMethod !== 'tools/call') {
        mcp_respond_rpc($rpcId, null, ['code' => -32601, 'message' => "Method not found: {$rpcMethod}"], 404);
    }

    $toolName = (string)($rpcParams['name'] ?? '');
    $args = is_array($rpcParams['arguments'] ?? null) ? $rpcParams['arguments'] : [];

    // ── Tool handlers ───────────────────────────────────────────────────────
    if ($toolName === 'get_manifest') {
        ai_require_scope('read');
        $inc = !empty($args['include_schemas']);
        $man = ai_build_manifest($ROLE, $account, [
            'nickname' => ['min' => CR_NICK_MIN, 'max' => CR_NICK_MAX],
            'message' => ['min' => CR_MESSAGE_MIN, 'max' => CR_MESSAGE_MAX],
            'max_operations' => $effectiveLimits['max_operations'],
            'max_pending' => $effectiveLimits['max_pending'],
            'cooldown' => $effectiveLimits['cooldown'],
            'max_scenario_bytes' => SCENARIO_MAX_BYTES,
        ], null, $inc);
        mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'manifest' => $man]));
    }

    if ($toolName === 'get_schema') {
        ai_require_scope('read');
        $incEx = array_key_exists('include_examples', $args) ? (bool)$args['include_examples'] : true;
        $allowed = ai_allowed_resources($ROLE, null, $incEx);
        $target = trim((string)($args['resource'] ?? ''));
        if ($target !== '') {
            if (!isset($allowed[$target])) {
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'resource_forbidden', 'error' => "Ресурс «{$target}» недоступен или неизвестен."], true));
            }
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'resource' => $target, 'schema' => $allowed[$target]]));
        }
        mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'resources' => $allowed]));
    }

    if ($toolName === 'list_records') {
        ai_require_scope('read');
        $resource = trim((string)($args['resource'] ?? ''));
        if ($resource === '' || !ai_can_read_resource($resource, $ROLE)) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'resource_forbidden', 'error' => "Ресурс «{$resource}» недоступен для чтения."], true));
        }
        $search = trim((string)($args['search'] ?? ''));
        $limit = clamp_int($args['limit'] ?? null, 1, 500, 50);
        $offset = clamp_int($args['offset'] ?? null, 0, 10000000, 0);
        $maxBytes = clamp_int($args['max_bytes'] ?? null, 4096, AI_MAX_RESPONSE_MAX_BYTES, AI_DEFAULT_RESPONSE_MAX_BYTES);

        if ($resource === 'scenarios') {
            $all = scenario_list_files($search);
            $page = ai_paginate_slice(array_slice($all, $offset, $limit), count($all), $limit, $offset, $maxBytes);
            mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true, 'resource' => 'scenarios'], $page)));
        }
        $db = db_connect();
        try {
            if (isset(ai_schema_lookups()[$resource])) {
                $res = ai_query_lookup($db, $resource, ['search' => $search, 'limit' => $limit, 'offset' => $offset, 'max_bytes' => $maxBytes]);
                mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true, 'resource' => $resource], $res)));
            }
            if ($resource === 'maps') {
                $worldsR = editor_resources()['worlds'];
                $wData = ai_query_db_list($db, $worldsR, ['search' => $search, 'filter' => $args['filter'] ?? [], 'limit' => $limit, 'offset' => $offset, 'max_bytes' => $maxBytes, 'decode' => true]);
                $rows = [];
                foreach ($wData['rows'] as $w) {
                    $wid = (int)$w['ID'];
                    $loaded = ai_map_load_bytes($db, $wid, null);
                    $rows[] = ['world_id' => $wid, 'name' => (string)($w['Name'] ?? ''), 'path' => (string)($w['Path'] ?? ''), 'type' => (string)($w['Type'] ?? ''), 'available' => (bool)$loaded['available'], 'source' => $loaded['source']];
                }
                $page = ai_paginate_slice($rows, $wData['total'], $wData['limit'], $wData['offset'], $maxBytes);
                mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true, 'resource' => 'maps'], $page)));
            }
            $R = editor_resources()[$resource] + ['key' => $resource];
            $res = ai_query_db_list($db, $R, [
                'search' => $search,
                'filter' => is_array($args['filter'] ?? null) ? $args['filter'] : [],
                'fields' => $args['fields'] ?? null,
                'limit' => $limit,
                'offset' => $offset,
                'max_bytes' => $maxBytes,
                'decode' => array_key_exists('decode', $args) ? (bool)$args['decode'] : true,
            ]);
            mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true, 'resource' => $resource], $res)));
        } finally {
            $db->close();
        }
    }

    if ($toolName === 'get_record') {
        ai_require_scope('read');
        $resource = trim((string)($args['resource'] ?? ''));
        if ($resource === '' || !ai_can_read_resource($resource, $ROLE)) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'resource_forbidden', 'error' => "Ресурс «{$resource}» недоступен для чтения."], true));
        }
        $incRefs = !empty($args['include_references']);

        if ($resource === 'scenarios') {
            $fn = (string)($args['filename'] ?? ($args['id'] ?? ''));
            $read = scenario_read_file($fn);
            if (!$read['ok']) {
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => $read['code'] ?? 'scenario_not_found', 'error' => $read['error']], true));
            }
            $out = [
                'ok' => true, 'resource' => 'scenarios', 'id' => $read['filename'],
                'filename' => $read['filename'], 'sha256' => $read['sha256'], 'summary' => $read['summary'],
            ];
            if (empty($args['summary_only'])) {
                $out['content'] = $read['content'];
            }
            if ($incRefs) {
                $db = db_connect();
                try { $out['references'] = ai_references_inspect($db, 'scenarios', $read['filename'], $ROLE); } finally { $db->close(); }
            }
            mcp_respond_rpc($rpcId, mcp_tool_result($out));
        }

        $db = db_connect();
        try {
            if ($resource === 'maps') {
                $worldId = isset($args['id']) ? crud_record_id((string)$args['id'], 'worlds') : null;
                $loaded = ai_map_load_bytes($db, $worldId, null);
                if (empty($loaded['available']) || !is_string($loaded['bytes'])) {
                    mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'resource' => 'maps', 'available' => false, 'reason' => $loaded['reason'] ?? 'Карта недоступна']));
                }
                $parsed = ai_map_parse_datafile(
                    $loaded['bytes'],
                    !empty($args['include_entities']),
                    isset($args['x']) ? (int)$args['x'] : null,
                    isset($args['y']) ? (int)$args['y'] : null
                );
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'resource' => 'maps', 'available' => true, 'world' => $loaded['world'], 'map' => $parsed]));
            }
            $recId = crud_record_id((string)($args['id'] ?? ''), $resource);
            if ($recId === null) {
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'invalid_id', 'error' => 'Некорректный ID записи.'], true));
            }
            $R = editor_resources()[$resource] + ['key' => $resource];
            $row = crud_fetch_row($db, $R, $recId);
            if ($row === null) {
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'record_not_found', 'error' => "Запись {$R['title']} #{$recId} не найдена."], true));
            }
            $decoded = ai_project_record_fields(decode_json_cols($row, $R['json'] ?? []), $R, $args['fields'] ?? null);
            $out = ['ok' => true, 'resource' => $resource, 'id' => $recId, 'record' => $decoded];
            if ($incRefs) {
                $out['references'] = ai_references_inspect($db, $resource, $recId, $ROLE);
            }
            mcp_respond_rpc($rpcId, mcp_tool_result($out));
        } finally {
            $db->close();
        }
    }

    if ($toolName === 'get_references') {
        ai_require_scope('read');
        $source = trim((string)($args['source'] ?? ''));
        if ($source !== '') {
            if (!ai_can_read_lookup_source($source, $ROLE)) {
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'resource_forbidden', 'error' => "Справочник «{$source}» недоступен."], true));
            }
            $db = db_connect();
            try {
                $res = ai_query_lookup($db, $source, ['search' => $args['search'] ?? '', 'limit' => $args['limit'] ?? 200, 'offset' => $args['offset'] ?? 0]);
                mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true, 'mode' => 'lookup', 'source' => $source], $res)));
            } finally {
                $db->close();
            }
        }
        $resource = trim((string)($args['resource'] ?? ''));
        $rawId = trim((string)($args['id'] ?? ''));
        if ($resource !== '' && $rawId !== '') {
            if (!ai_can_read_resource($resource, $ROLE)) {
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'resource_forbidden', 'error' => "Ресурс «{$resource}» недоступен."], true));
            }
            $idArg = $resource === 'scenarios' ? $rawId : crud_record_id($rawId, $resource);
            if ($idArg === null) {
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'invalid_id', 'error' => 'Некорректный ID.'], true));
            }
            $db = db_connect();
            try {
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'mode' => 'record', 'references' => ai_references_inspect($db, $resource, $idArg, $ROLE)]));
            } finally {
                $db->close();
            }
        }
        $allowed = ai_allowed_resources($ROLE, null, false);
        mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'mode' => 'schema', 'resources' => array_keys($allowed)]));
    }

    if (in_array($toolName, ['validate_operations', 'preview_operations', 'propose_changes', 'copy_record', 'patch_json_record'], true)) {
        if ($toolName === 'copy_record') {
            $op = [
                'type' => 'copy',
                'resource' => (string)($args['resource'] ?? ''),
            ];
            if (isset($args['source_id'])) $op['source_id'] = $args['source_id'];
            if (isset($args['source_filename'])) $op['source_filename'] = $args['source_filename'];
            if (isset($args['filename'])) $op['filename'] = $args['filename'];
            if (isset($args['data'])) $op['data'] = $args['data'];
            if (isset($args['patch_json'])) $op['patch_json'] = $args['patch_json'];
            $rawOps = ai_extract_operations_payload(['operations' => [$op]]);
            $mode = !empty($args['propose']) ? 'propose' : 'preview';
        } elseif ($toolName === 'patch_json_record') {
            $op = [
                'type' => 'update',
                'resource' => (string)($args['resource'] ?? ''),
                'patch_json' => $args['patch_json'] ?? [],
            ];
            if (isset($args['id'])) $op['id'] = $args['id'];
            if (isset($args['filename'])) $op['filename'] = $args['filename'];
            if (isset($args['data'])) $op['data'] = $args['data'];
            if (isset($args['baseHash'])) $op['baseHash'] = $args['baseHash'];
            if (isset($args['expected_before'])) $op['expected_before'] = $args['expected_before'];
            $rawOps = ai_extract_operations_payload(['operations' => [$op]]);
            $mode = !empty($args['propose']) ? 'propose' : 'preview';
        } else {
            $rawOps = ai_extract_operations_payload($args);
            $mode = $toolName === 'validate_operations' ? 'validate' : ($toolName === 'preview_operations' ? 'preview' : 'propose');
        }

        $maxOps = $IS_ADMIN && $mode !== 'propose' ? 500 : (int)$effectiveLimits['max_operations'];
        $db = db_connect();
        try {
            $val = ai_validate_operations($db, $rawOps, $ROLE, null, null, $maxOps);
            if ($mode === 'validate') {
                mcp_respond_rpc($rpcId, mcp_tool_result([
                    'ok' => true,
                    'valid' => $val['valid'],
                    'errors' => $val['errors'],
                    'warnings' => $val['warnings'],
                    'required_scopes' => $val['required_scopes'],
                    'requires_confirmation' => $val['requires_confirmation'],
                    'dangerous_actions' => $val['dangerous_actions'],
                    'normalized_operations' => $val['normalized_operations'],
                ], !$val['valid']));
            }

            if (!$val['valid']) {
                mcp_respond_rpc($rpcId, mcp_tool_result([
                    'ok' => false,
                    'code' => (string)($val['errors'][0]['code'] ?? 'validation_failed'),
                    'error' => (string)($val['errors'][0]['message'] ?? 'Проверка операций завершилась с ошибками.'),
                    'errors' => $val['errors'],
                    'warnings' => $val['warnings'],
                ], true));
            }

            $normOps = cr_normalize_ops($val['normalized_operations'], $maxOps);
            if (!$IS_ADMIN) {
                cr_assert_guest_resources($normOps);
            }

            if ($mode === 'preview') {
                cr_preview_throttle($IS_ADMIN);
                $analysis = ai_enrich_analysis_diffs(cr_public_analysis(cr_analyze($db, $normOps)));
                mcp_respond_rpc($rpcId, mcp_tool_result([
                    'ok' => true,
                    'valid' => true,
                    'analysis' => $analysis,
                    'warnings' => $val['warnings'],
                    'required_scopes' => $val['required_scopes'],
                    'requires_confirmation' => $val['requires_confirmation'],
                    'dangerous_actions' => $val['dangerous_actions'],
                    'normalized_operations' => $val['normalized_operations'],
                ]));
            }

            // mode === 'propose'
            if ($IS_ADMIN) {
                mcp_respond_rpc($rpcId, mcp_tool_result([
                    'ok' => false,
                    'code' => 'guest_session_required',
                    'error' => 'Для создания запросов на изменение AI должен работать через гостевой аккаунт или AI-токен.',
                ], true));
            }
            ai_assert_operation_scopes($val['required_scopes']);
            if (!empty($val['requires_confirmation']) && empty($args['confirm_dangerous'])) {
                mcp_respond_rpc($rpcId, mcp_tool_result([
                    'ok' => false,
                    'code' => 'confirmation_required',
                    'error' => 'Операция требует confirm_dangerous=true после проверки dangerous_actions.',
                    'requires_confirmation' => true,
                    'dangerous_actions' => $val['dangerous_actions'],
                ], true));
            }

            $msg = trim((string)($args['message'] ?? ''));
            $requestId = trim((string)($args['request_id'] ?? ''));
            $isRevise = $requestId !== '';
            [$kept, $analysis] = cr_prepare_for_submit($db, $normOps);
            $ip = get_request_ip();

            if ($isRevise) {
                $expectedVersion = (int)($args['version'] ?? 0);
                if ($expectedVersion < 1) {
                    mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'missing_required_field', 'error' => 'Укажите актуальную версию (version) для обновления запроса.'], true));
                }
                $actorId = (string)($account['id'] ?? '');
                $actorName = (string)($account['name'] ?? ($account['login'] ?? 'MCP Agent'));
                $nextMessage = $msg !== '' ? cr_validate_message($msg) : null;
                $updated = cr_with_lock(function () use ($requestId, $expectedVersion, $kept, $analysis, $nextMessage, $actorId, $actorName) {
                    $req = cr_load_visible($requestId, false);
                    if ($req['status'] !== 'pending') {
                        throw new CrInputError('Можно изменить только ожидающий запрос.', 409, ['code' => 'request_not_pending']);
                    }
                    $curVer = max(1, (int)($req['version'] ?? 1));
                    if ($expectedVersion !== $curVer) {
                        throw new CrInputError('Запрос уже изменён.', 409, ['code' => 'version_conflict', 'version' => $curVer]);
                    }
                    $nextVer = $curVer + 1;
                    $at = cr_now();
                    $enriched = ai_enrich_analysis_diffs(cr_public_analysis($analysis));
                    $fieldsChanged = (int)($enriched['summary']['fields'] ?? 0);
                    if ($nextMessage !== null) $req['message'] = $nextMessage;
                    $req['operations'] = $kept;
                    $req['analysis'] = $enriched;
                    $req['version'] = $nextVer;
                    $req['updatedAt'] = $at;
                    $req['lastRevision'] = ['version' => $nextVer, 'role' => 'guest', 'by' => $actorName, 'at' => $at];
                    $req['revisions'] = array_slice(array_merge(is_array($req['revisions'] ?? null) ? $req['revisions'] : [], [[
                        'version' => $nextVer, 'role' => 'guest', 'by' => $actorName, 'at' => $at,
                        'operations' => count($kept), 'fields' => $fieldsChanged,
                    ]]), -20);
                    cr_save($req);
                    cr_msg_add($requestId, 'guest', $actorId, $actorName, "Обновлён запрос через MCP: версия {$nextVer}, операций: " . count($kept) . '.');
                    return $req;
                });
                editor_audit('mcp.propose.revise', $requestId, ['version' => (int)$updated['version'], 'ops' => count($kept)]);
                mcp_respond_rpc($rpcId, mcp_tool_result([
                    'ok' => true,
                    'revised' => true,
                    'duplicate' => false,
                    'warnings' => $val['warnings'],
                    'request' => cr_full_view($updated, false),
                ]));
            }

            if ($msg === '') {
                $msg = $toolName === 'copy_record' ? 'Копирование объекта через MCP'
                    : ($toolName === 'patch_json_record' ? 'Безопасный JSON-патч через MCP' : 'Изменения через MCP');
            }
            $message = cr_validate_message($msg);
            $nickCleaned = cr_clean_text($args['nickname'] ?? ($account['name'] ?? ($account['login'] ?? 'MCP Agent')), false);
            if (cr_strlen($nickCleaned) < CR_NICK_MIN) $nickCleaned = 'MCP Agent';
            $nickname = cr_validate_nickname($nickCleaned);

            $idemKey = trim((string)($args['idempotency_key'] ?? ''));
            $keyHash = $idemKey !== '' ? hash('sha256', (string)($account['id'] ?? '') . '|' . $idemKey) : '';

            $res = cr_with_lock(function () use ($keyHash, $account, $effectiveLimits, $nickname, $message, $kept, $analysis, $ip) {
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
                    throw new CrInputError("Превышен лимит ожидающих запросов ({$effectiveLimits['max_pending']}).", 429, ['code' => 'too_many_pending']);
                }
                $left = cr_ip_cooldown_left($ip, (int)$effectiveLimits['cooldown'], true);
                if ($left > 0) {
                    throw new CrInputError('Слишком частая отправка запросов.', 429, ['code' => 'rate_limited', 'retry_after' => $left]);
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
                        'userAgent' => 'MCP/' . MCP_SERVER_VERSION,
                        'via' => 'mcp',
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
            if (!$res['duplicate']) {
                store_rate_touch('ai-submit|' . (string)($account['id'] ?? ''), time());
                editor_audit('mcp.propose', (string)$res['req']['id'], ['ops' => count($kept), 'tool' => $toolName]);
            }
            mcp_respond_rpc($rpcId, mcp_tool_result([
                'ok' => true,
                'duplicate' => $res['duplicate'],
                'warnings' => $val['warnings'],
                'request' => cr_full_view($res['req'], false),
            ]));
        } finally {
            $db->close();
        }
    }

    if ($toolName === 'get_history') {
        ai_require_scope('read');
        $limit = clamp_int($args['limit'] ?? null, 1, 200, 20);
        $offset = clamp_int($args['offset'] ?? null, 0, 100000, 0);
        $resFilter = trim((string)($args['resource'] ?? ''));
        if ($IS_ADMIN) {
            $hist = store_history_list(['resource' => $resFilter], $limit, $offset);
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'role' => 'admin', 'db_history' => $hist['rows'], 'total' => $hist['total']]));
        }
        $guestId = (string)($account['id'] ?? '');
        $reqList = cr_query_list(['guest_id' => $guestId, 'resource' => $resFilter], $limit, $offset);
        $auditList = store_audit_list(['actor_id' => $guestId], $limit, $offset);
        $auditSafe = array_map(static function (array $row): array {
            unset($row['ip']);
            return $row;
        }, $auditList['rows']);
        mcp_respond_rpc($rpcId, mcp_tool_result([
            'ok' => true,
            'role' => 'guest',
            'requests' => array_map(static fn(array $r): array => cr_list_item($r, false), $reqList['rows']),
            'audit' => $auditSafe,
        ]));
    }

    mcp_respond_rpc($rpcId, null, ['code' => -32601, 'message' => "Unknown tool: {$toolName}"], 404);
} catch (CrInputError $e) {
    mcp_respond_rpc($rpcId, mcp_tool_result([
        'ok' => false,
        'code' => (string)($e->extra['code'] ?? 'bad_request'),
        'error' => $e->getMessage(),
        'details' => $e->extra,
    ], true));
} catch (Throwable $e) {
    editor_log_error($e);
    mcp_respond_rpc($rpcId, mcp_tool_result([
        'ok' => false,
        'code' => 'internal_error',
        'error' => editor_safe_message($e),
    ], true));
}
