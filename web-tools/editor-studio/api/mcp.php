<?php
// Universal Model Context Protocol (MCP) Server over HTTP (JSON-RPC 2.0).
// Endpoint: api/mcp.php (also routed at /mcp)
//
// Supports MCP protocol 2024-11-05 / 2025-03-26 / 2025-06-18:
//   - initialize, ping
//   - tools/list, tools/call (19 typed MRPG tools with MCP annotations, global search, composite context, semantic map analysis & PNG screenshots, symbolic refs, and Change Request lifecycle)
//   - resources/list, resources/templates/list, resources/read (dynamic schemas + cookbook + manifest + OpenAPI + llms.txt + entity context)
//   - prompts/list, prompts/get (tailored model-agnostic MRPG workflow prompts)
//
// Auth: Authorization: Bearer esai_<token> (or X-AI-Token) or Editor Studio session cookie.
// Unauthenticated `initialize`, `ping`, `tools/list`, and `prompts/list` return capability metadata only
// (no game data, no secrets); all data/write calls require authentication + token scopes.

declare(strict_types=1);

require_once __DIR__ . '/ai-lib.php';
require_once __DIR__ . '/db-history-lib.php';

const MCP_PROTOCOL_VERSION = '2024-11-05';
const MCP_SUPPORTED_VERSIONS = ['2024-11-05', '2025-03-26', '2025-06-18'];
const MCP_SERVER_NAME = 'mrpg-editor-studio-mcp';
const MCP_SERVER_VERSION = '2.1.0';

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
    $ro = [
        'readOnlyHint' => true,
        'destructiveHint' => false,
        'idempotentHint' => true,
        'openWorldHint' => false,
    ];
    $writeSafe = [
        'readOnlyHint' => false,
        'destructiveHint' => false,
        'idempotentHint' => false,
        'openWorldHint' => false,
    ];

    return [
        [
            'name' => 'get_manifest',
            'description' => 'Inspect Editor Studio AI API capabilities, role, scopes, granted pages, limits, and resource directory.',
            'annotations' => $ro + ['title' => 'Get AI Capabilities Manifest'],
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
            'annotations' => $ro + ['title' => 'Get Resource Schema'],
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
            'name' => 'search_editor',
            'description' => 'Cross-resource search across all allowed DB tables, scenarios, and lookups in a single call. Great first step to locate IDs, items, quests, NPCs, worlds, or scenarios without issuing multiple list_records calls.',
            'annotations' => $ro + ['title' => 'Search Across Editor Resources'],
            'inputSchema' => [
                'type' => 'object',
                'required' => ['query'],
                'properties' => [
                    'query' => ['type' => 'string', 'minLength' => 1, 'description' => 'Keyword, name fragment, or numeric ID'],
                    'resources' => ['anyOf' => [['type' => 'string'], ['type' => 'array', 'items' => ['type' => 'string']]], 'description' => 'Optional subset of resources to search (e.g. ["items","quests","scenarios"])'],
                    'limit_per_resource' => ['type' => 'integer', 'minimum' => 1, 'maximum' => 20, 'default' => 5],
                    'max_matches' => ['type' => 'integer', 'minimum' => 5, 'maximum' => 100, 'default' => 40],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'get_context',
            'description' => 'One-shot composite context snapshot for a world (all NPCs, mobs, quest steps, shops, teleports, nodes, crafts, dungeons & map status), quest (ordered steps, givers, boards, chain), item (crafts, mob drops, harvest nodes, shops), bot (all spawns), or scenario.',
            'annotations' => $ro + ['title' => 'Get Composite Entity Context'],
            'inputSchema' => [
                'type' => 'object',
                'required' => ['resource', 'id'],
                'properties' => [
                    'resource' => ['type' => 'string', 'description' => 'Primary resource (worlds, quests, items, bots_info, scenarios, or any readable DB resource)'],
                    'id' => ['anyOf' => [['type' => 'integer'], ['type' => 'string']], 'description' => 'Record ID or scenario filename'],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'list_records',
            'description' => 'Search, filter, sort, and paginate existing DB records, scenario files, lookups, or maps. Filter supports exact values, arrays (IN), null, and comparison objects like {"Level":{"gte":5,"lte":15},"Name":{"contains":"Iron"},"NextQuestID":{"not_null":true}}.',
            'annotations' => $ro + ['title' => 'List & Filter Records'],
            'inputSchema' => [
                'type' => 'object',
                'required' => ['resource'],
                'properties' => [
                    'resource' => ['type' => 'string'],
                    'search' => ['type' => 'string'],
                    'filter' => ['type' => 'object', 'description' => 'Column filters: exact, array (IN), null, or operator map {eq,ne,gt,gte,lt,lte,contains,prefix,in,is_null,not_null}.'],
                    'sort' => ['type' => 'string', 'description' => 'Optional sort specification, e.g. "ID:desc", "-Level,Name:asc".'],
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
            'description' => 'Read a single DB record by ID, a batch of up to 50 DB records by ids=[...], a scenario JSON by filename (with sha256 baseHash), or inspect a DDNet .map binary (with visual layers, recognized structures/buildings/houses/castles, zones, and point context).',
            'annotations' => $ro + ['title' => 'Get Record(s) / Scenario / Map'],
            'inputSchema' => [
                'type' => 'object',
                'required' => ['resource'],
                'properties' => [
                    'resource' => ['type' => 'string'],
                    'id' => ['anyOf' => [['type' => 'integer'], ['type' => 'string']]],
                    'ids' => ['type' => 'array', 'items' => ['anyOf' => [['type' => 'integer'], ['type' => 'string']]], 'minItems' => 1, 'maxItems' => 50, 'description' => 'Batch-read up to 50 DB record IDs in one call.'],
                    'filename' => ['type' => 'string'],
                    'path' => ['type' => 'string', 'description' => 'For resource=maps: relative .map path when world id is not specified.'],
                    'fields' => ['anyOf' => [['type' => 'string'], ['type' => 'array', 'items' => ['type' => 'string']]]],
                    'summary_only' => ['type' => 'boolean', 'default' => false],
                    'include_references' => ['type' => 'boolean', 'default' => true],
                    'include_entities' => ['type' => 'boolean', 'default' => false],
                    'include_minimap' => ['type' => 'boolean', 'default' => false],
                    'x' => ['type' => 'integer'],
                    'y' => ['type' => 'integer'],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'get_references',
            'description' => 'Inspect inbound/outbound foreign-key & JSON/scenario references for a record, lookup table entries (?source=...), or full schema graph.',
            'annotations' => $ro + ['title' => 'Inspect References & Lookups'],
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
            'description' => 'Validate create/update/copy/delete/patch_json operations against DB schema, MRPG server compatibility, symbolic refs ("ref":"alias" / "$alias"), references, and dangerous action rules without writing anything. Returns self-healing did_you_mean hints on errors.',
            'annotations' => $ro + ['title' => 'Validate Operations (Dry-Run)'],
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
            'description' => 'Dry-run analysis returning exact field diffs, path-level json_diff, resolved_refs, dependency graph, warnings, and dangerous_actions without mutating storage.',
            'annotations' => $ro + ['title' => 'Preview Operations Diff'],
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
            'description' => 'Submit or revise a Change Request in Editor Studio SQLite queue for administrator approval. Supports symbolic "ref"/"$alias" cross-links across ops in one atomic batch. Requires confirm_dangerous=true when deleting, clearing JSON, bulk >10 ops, or editing high-impact resources.',
            'annotations' => $writeSafe + ['title' => 'Submit / Revise Change Request'],
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
            'name' => 'manage_change_request',
            'description' => 'Inspect status, admin review feedback, and discussion messages of Change Requests, reply to an administrator in a request thread, or withdraw an outdated pending request (freeing max_pending quota).',
            'annotations' => $writeSafe + ['title' => 'Inspect / Reply / Withdraw Change Request'],
            'inputSchema' => [
                'type' => 'object',
                'required' => ['action'],
                'properties' => [
                    'action' => ['type' => 'string', 'enum' => ['list', 'get', 'reply', 'withdraw'], 'description' => 'list = own requests; get = full CR details + admin messages; reply = post message to admin; withdraw = cancel own pending CR'],
                    'request_id' => ['type' => 'string', 'description' => 'Required for get, reply, withdraw'],
                    'body' => ['type' => 'string', 'description' => 'Message text (required for action=reply)'],
                    'resource' => ['type' => 'string', 'description' => 'Optional resource filter for action=list'],
                    'status' => ['type' => 'string', 'enum' => ['pending', 'approved', 'rejected', 'withdrawn'], 'description' => 'Optional status filter for action=list'],
                    'limit' => ['type' => 'integer', 'minimum' => 1, 'maximum' => 100, 'default' => 20],
                    'offset' => ['type' => 'integer', 'minimum' => 0, 'default' => 0],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'copy_record',
            'description' => 'Safely duplicate an existing DB record or scenario JSON file with optional field overrides or JSON merge patch; previews or submits a Change Request.',
            'annotations' => $writeSafe + ['title' => 'Duplicate Record or Scenario'],
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
            'annotations' => $writeSafe + ['title' => 'Safe JSON Merge Patch'],
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
            'name' => 'analyze_scenario',
            'description' => 'Deep analysis of scenario JSON files: per-step component inventory (known/legacy/unknown), outbound references resolved against the game DB (missing items/quests/bots/worlds are flagged), inbound links, and which other scenarios reuse the same components. Omit id for a corpus-wide overview (component histogram + missing dependencies across all scenarios).',
            'annotations' => $ro + ['title' => 'Analyze Scenario(s)'],
            'inputSchema' => [
                'type' => 'object',
                'properties' => [
                    'id' => ['type' => 'string', 'description' => 'Scenario filename to analyze. Omit for corpus-wide analysis.'],
                    'search' => ['type' => 'string', 'description' => 'Corpus mode only: filter scenario files by filename/component/event substring.'],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'find_similar',
            'description' => 'Find existing records similar to a proposed name (Dice bigram similarity on normalized text) to avoid duplicates before creating an item/quest/NPC/world. Returns scored candidates (id, label, score/score_percent).',
            'annotations' => $ro + ['title' => 'Find Similar Records'],
            'inputSchema' => [
                'type' => 'object',
                'required' => ['resource', 'name'],
                'properties' => [
                    'resource' => ['type' => 'string', 'description' => 'DB resource key (items, quests, bots_npc, ...) or lookup (skills, attributes, worlds, ...)'],
                    'name' => ['type' => 'string', 'description' => 'Proposed label to compare against existing records'],
                    'limit' => ['type' => 'integer', 'minimum' => 1, 'maximum' => 25, 'default' => 5],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'apply_patch',
            'description' => 'Read-only server-side verify of patch_json against the live record/file: merges RFC 7396 patch, returns before/after/diff and the exact errors action=propose would emit. Writes nothing; does not consume cooldown or confirm_dangerous. Call this before propose_changes.',
            'annotations' => $ro + ['title' => 'Verify JSON Patch (Read-Only)'],
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
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'get_history',
            'description' => 'Read the recent action audit log and Change Requests visible to the current AI token or session.',
            'annotations' => $ro + ['title' => 'Get Audit & Request History'],
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
        [
            'name' => 'analyze_map',
            'description' => 'Deep semantic & structural analysis of a DDNet/MRPG .map file: reads visual tile/quad layers, tilesets, terrain elevation profile, recognizes buildings and structures (castles/guild halls, houses, shops/services, dungeons/ruins, caves, general buildings) with bounding boxes & floor counts, named zones (#zone/#action_zone/#text), correlated DB world objects, rich point_check (roof/floor enclosure, visual layers, inside_structure, nearest_standable_point), and a semantic ASCII minimap.',
            'annotations' => $ro + ['title' => 'Analyze Map Structure & Terrain'],
            'inputSchema' => [
                'type' => 'object',
                'properties' => [
                    'world_id' => ['type' => 'integer', 'minimum' => 0, 'description' => 'World ID from tw_worlds (alternative to path).'],
                    'path' => ['type' => 'string', 'description' => 'Relative .map path (e.g. "main.map", "worlds/Forest.map") when world_id is omitted.'],
                    'target' => ['type' => 'string', 'description' => 'Optional region focus: overview, spawn, guild_house:<id>, house:<id>, zone:<num|name>, structure:<id>, warehouse:<id>, aether:<id>.'],
                    'structure_kind' => ['type' => 'string', 'enum' => ['castle_or_guild_hall', 'house', 'shop_or_service', 'dungeon_or_ruins', 'cave_or_underground', 'building'], 'description' => 'Optional filter for structures.items by semantic kind.'],
                    'x' => ['type' => 'integer', 'description' => 'World pixel X coordinate for rich point_check or region center.'],
                    'y' => ['type' => 'integer', 'description' => 'World pixel Y coordinate for rich point_check or region center.'],
                    'radius' => ['type' => 'integer', 'minimum' => 128, 'maximum' => 6400, 'description' => 'Pixel radius around (x, y) for focused region analysis.'],
                    'radius_tiles' => ['type' => 'integer', 'minimum' => 4, 'maximum' => 200, 'description' => 'Tile radius around (x, y) for focused region analysis.'],
                    'x0' => ['type' => 'integer'],
                    'y0' => ['type' => 'integer'],
                    'x1' => ['type' => 'integer'],
                    'y1' => ['type' => 'integer'],
                    'tx0' => ['type' => 'integer'],
                    'ty0' => ['type' => 'integer'],
                    'tx1' => ['type' => 'integer'],
                    'ty1' => ['type' => 'integer'],
                    'include_entities' => ['type' => 'boolean', 'default' => false],
                    'include_minimap' => ['type' => 'boolean', 'default' => true],
                    'compact' => ['type' => 'boolean', 'default' => false],
                ],
                'additionalProperties' => false,
            ],
        ],
        [
            'name' => 'get_map_screenshot',
            'description' => 'Render a real PNG screenshot of a DDNet/MRPG map or zoomed region/building (returning native MCP image content + exact pixel-to-world coordinate mapping and visible structures). Supports hybrid (real visual tilesets + structure/house/castle outlines & labels), visual (clean in-game look), and schematic (collision/interior/terrain blueprint) modes.',
            'annotations' => $ro + ['title' => 'Render PNG Map Screenshot'],
            'inputSchema' => [
                'type' => 'object',
                'properties' => [
                    'world_id' => ['type' => 'integer', 'minimum' => 0, 'description' => 'World ID from tw_worlds (alternative to path).'],
                    'path' => ['type' => 'string', 'description' => 'Relative .map path when world_id is omitted.'],
                    'mode' => ['type' => 'string', 'enum' => ['hybrid', 'visual', 'schematic'], 'default' => 'hybrid'],
                    'target' => ['type' => 'string', 'default' => 'overview', 'description' => 'Viewport target: overview, spawn, guild_house:<id>, house:<id>, zone:<num|name>, structure:<id>, warehouse:<id>, aether:<id>, or partial building name.'],
                    'x' => ['type' => 'integer', 'description' => 'Center world pixel X (draws crosshair & crops around point).'],
                    'y' => ['type' => 'integer', 'description' => 'Center world pixel Y (draws crosshair & crops around point).'],
                    'radius' => ['type' => 'integer', 'minimum' => 128, 'maximum' => 6400],
                    'radius_tiles' => ['type' => 'integer', 'minimum' => 4, 'maximum' => 200],
                    'x0' => ['type' => 'integer'],
                    'y0' => ['type' => 'integer'],
                    'x1' => ['type' => 'integer'],
                    'y1' => ['type' => 'integer'],
                    'tx0' => ['type' => 'integer'],
                    'ty0' => ['type' => 'integer'],
                    'tx1' => ['type' => 'integer'],
                    'ty1' => ['type' => 'integer'],
                    'highlight_x' => ['type' => 'integer', 'description' => 'Optional world pixel X to mark with a red crosshair.'],
                    'highlight_y' => ['type' => 'integer', 'description' => 'Optional world pixel Y to mark with a red crosshair.'],
                    'width' => ['type' => 'integer', 'minimum' => 256, 'maximum' => 1600, 'default' => 1024],
                    'height' => ['type' => 'integer', 'minimum' => 256, 'maximum' => 1200, 'default' => 768],
                    'pad_tiles' => ['type' => 'integer', 'minimum' => 0, 'maximum' => 40, 'default' => 6],
                    'show_grid' => ['type' => 'boolean', 'default' => true],
                    'show_structures' => ['type' => 'boolean'],
                    'show_labels' => ['type' => 'boolean'],
                    'show_objects' => ['type' => 'boolean'],
                    'show_collision' => ['type' => 'boolean'],
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
            'description' => 'Create a server-compatible MRPG item and optional craft/shop recipe safely in one atomic batch.',
            'arguments' => [
                ['name' => 'concept', 'description' => 'Item idea, stats, and rarity/purpose', 'required' => true],
            ],
        ],
        [
            'name' => 'create_mrpg_quest_scene',
            'description' => 'Design a quest + NPC/bot steps + scenario JSON linked via symbolic ref/$alias with verified map coordinates.',
            'arguments' => [
                ['name' => 'world_id', 'description' => 'Target world ID', 'required' => true],
                ['name' => 'goal', 'description' => 'Quest objective and rewards', 'required' => true],
            ],
        ],
        [
            'name' => 'safe_json_patch_edit',
            'description' => 'Inspect an existing object or scenario and apply a minimal verified patch_json without losing unrelated fields.',
            'arguments' => [
                ['name' => 'resource', 'description' => 'Target resource name', 'required' => true],
                ['name' => 'id', 'description' => 'Target record ID or scenario filename', 'required' => true],
            ],
        ],
        [
            'name' => 'inspect_map_and_place_object',
            'description' => 'Visually and structurally analyze a world map (buildings, castles, houses, terrain) with screenshots and point checks before placing NPCs, portals, or quest steps.',
            'arguments' => [
                ['name' => 'world_id', 'description' => 'Target world ID', 'required' => true],
                ['name' => 'goal', 'description' => 'What to inspect or where to place an object', 'required' => true],
            ],
        ],
        [
            'name' => 'review_and_revise_request',
            'description' => 'Check status and administrator feedback on pending Change Requests, revise with updated operations or withdraw stale drafts, and reply in the review thread.',
            'arguments' => [
                ['name' => 'request_id', 'description' => 'Optional Change Request ID (omit to list all pending requests first)', 'required' => false],
            ],
        ],
    ];
}

function mcp_prompt_message(string $name, array $args): string
{
    $argTxt = json_encode($args, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    return match ($name) {
        'create_mrpg_item' => "MRPG Item Authoring Workflow (concept: {$argTxt}):\n"
            . "1. Read `mrpg://cookbook` and call `get_schema(resource='items')` + `get_references(source='attributes')`.\n"
            . "2. Call `find_similar(resource='items', name=...)` and `search_editor(query=...)` to avoid duplicates and match existing stat/price balance.\n"
            . "3. Ensure `Group` and `Type` match valid MRPG pairs (e.g. Equipment -> Equip sword/gun/armor, Resource -> Resource mineable/harvestable/fishes, Potion -> Equip potion HP/MP).\n"
            . "4. To also create a craft recipe or shop entry in the same Change Request, set `\"ref\": \"new_item\"` on the item `create` op and reference `\"\$new_item\"` in `crafts.ItemID` or `\"[\$new_item/1(100)]\"` in `warehouses.Trades`.\n"
            . "5. Run `validate_operations` -> `preview_operations` -> `propose_changes` with a concise Russian/English summary.",
        'create_mrpg_quest_scene' => "MRPG Quest & Scenario Authoring Workflow ({$argTxt}):\n"
            . "1. Call `get_context(resource='worlds', id=world_id)` to see existing NPCs, mobs, quests, shops, and map path in one shot.\n"
            . "2. Call `analyze_scenario()` (corpus overview) to reuse existing scenario component types and avoid missing dependencies.\n"
            . "3. Call `analyze_map(world_id=...)` and `get_map_screenshot(world_id=..., target=...)` to pick valid standable coordinates (`point_check.nearest_standable_point`; remember `bots_npc`/`quest_bots` use `PosX`/`PosY`, `bots_mobs` uses `PositionX`/`PositionY`).\n"
            . "4. Call `find_similar(resource='quests', name=...)` to avoid duplicate quest names.\n"
            . "5. Build ONE atomic operations batch using symbolic `ref`/`\$alias` (e.g. `ref:'q_main'` -> `GiveQuestID:'\$q_main'`, `QuestID:'\$q_main'`; `ref:'bot_guide'` -> `BotID:'\$bot_guide'`).\n"
            . "6. Run `validate_operations` -> `preview_operations` -> `propose_changes`.",
        'safe_json_patch_edit' => "MRPG Safe JSON Patch Workflow ({$argTxt}):\n"
            . "1. Call `get_record(resource=..., id=..., include_references=true)` (keep `sha256` as `baseHash` for scenarios; note current field values for `expected_before`).\n"
            . "2. Draft a minimal RFC 7396 `patch_json` touching only the keys that must change (never send full replacement blobs unless required).\n"
            . "3. Verify server-side without writing using `apply_patch(resource=..., id|filename=..., patch_json=..., baseHash=...)` and inspect `diff` and `noop`.\n"
            . "4. Call `preview_operations` to inspect `dangerous_actions`, then `propose_changes` (or `patch_json_record` with `propose=true`).",
        'inspect_map_and_place_object' => "MRPG Visual Map Inspection & Spatial Placement Workflow ({$argTxt}):\n"
            . "1. Call `get_context(resource='worlds', id=world_id)` and `analyze_map(world_id=..., compact=true)` to list recognized buildings (castles/guild halls, houses, shops, caves), zones, and existing DB objects.\n"
            . "2. Call `get_map_screenshot(world_id=..., target='overview', mode='hybrid')` to visually see the whole world, then zoom in on a target building/region (`target='guild_house:1'` / `'house:1'` / `'spawn'` / `x,y,radius`).\n"
            . "3. Probe candidate coordinates with `analyze_map(world_id=..., x=..., y=...)` and verify `point_check.environment`, `inside_structure`, `safe_for_placement`, and `nearest_standable_point`.\n"
            . "4. Confirm final placement with `get_map_screenshot(..., highlight_x=x, highlight_y=y)` (red crosshair).\n"
            . "5. Submit object placement via `validate_operations` -> `preview_operations` -> `propose_changes`.",
        'review_and_revise_request' => "MRPG Change Request Review Loop ({$argTxt}):\n"
            . "1. Call `manage_change_request(action='list')` (or `action='get', request_id=...`) to inspect status, `version`, admin `review`, and unread `messages`.\n"
            . "2. If the admin requested adjustments on a `pending` CR, verify the updated operations with `validate_operations` + `preview_operations`, then revise in-place via `propose_changes(request_id=..., version=..., operations=...)`.\n"
            . "3. Post a concise update in the thread with `manage_change_request(action='reply', request_id=..., body=...)`.\n"
            . "4. If a pending CR is obsolete or blocking `max_pending`, cancel it with `manage_change_request(action='withdraw', request_id=...)`.",
        default => "Follow MRPG Editor Studio safety workflow ({$name}): 1) Orient via search_editor/get_context and read mrpg://cookbook; 2) Draft operations with ref/\$alias or patch_json; 3) Verify with apply_patch/validate_operations/preview_operations; 4) Submit via propose_changes. Arguments: {$argTxt}",
    };
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

/**
 * MCP multimodal tool result: returns native image content block + compact JSON metadata
 * without duplicating base64 image data inside text/structuredContent.
 */
function mcp_tool_image_result(array $meta, string $pngBytes): array
{
    return [
        'content' => [
            [
                'type' => 'image',
                'data' => base64_encode($pngBytes),
                'mimeType' => 'image/png',
            ],
            [
                'type' => 'text',
                'text' => (string)json_encode($meta, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES | JSON_PRETTY_PRINT),
            ],
        ],
        'structuredContent' => $meta,
        'isError' => false,
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
        $reqProto = isset($rpcParams['protocolVersion']) && is_string($rpcParams['protocolVersion'])
            ? trim($rpcParams['protocolVersion'])
            : '';
        if ($reqProto === '') {
            $negotiatedProto = MCP_PROTOCOL_VERSION;
        } elseif (in_array($reqProto, MCP_SUPPORTED_VERSIONS, true)) {
            $negotiatedProto = $reqProto;
        } else {
            $negotiatedProto = MCP_SUPPORTED_VERSIONS[count(MCP_SUPPORTED_VERSIONS) - 1];
        }
        mcp_respond_rpc($rpcId, [
            'protocolVersion' => $negotiatedProto,
            'capabilities' => [
                'tools' => ['listChanged' => false],
                'resources' => ['listChanged' => false, 'subscribe' => false],
                'prompts' => ['listChanged' => false],
            ],
            'serverInfo' => [
                'name' => MCP_SERVER_NAME,
                'version' => MCP_SERVER_VERSION,
            ],
            'instructions' => 'Start by calling get_manifest or reading resource mrpg://cookbook. Orient quickly in 1 call via search_editor (cross-resource search) and get_context (full world/quest/item/bot snapshot). Reuse existing content with analyze_scenario and find_similar. For maps and spatial placement, call analyze_map and get_map_screenshot (native PNG). Read multiple IDs at once with get_record(ids=[...]), and filter/sort with list_records(filter, sort). Link newly created rows inside one atomic batch via symbolic "ref":"alias" and "$alias" (in FKs, DSL strings, or JSON). Verify patch_json via apply_patch, run validate_operations/preview_operations, submit via propose_changes, and track/reply/withdraw pending requests via manage_change_request.',
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
                mcp_respond_rpc($rpcId, [
                    'description' => $p['description'],
                    'messages' => [[
                        'role' => 'user',
                        'content' => [
                            'type' => 'text',
                            'text' => mcp_prompt_message($pName, $pArgs),
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
    // ── MCP resources/list, resources/templates/list & resources/read ───────
    if ($rpcMethod === 'resources/list') {
        ai_require_scope('read');
        $allowed = ai_allowed_resources($ROLE, null, false);
        $list = [
            ['uri' => 'mrpg://manifest', 'name' => 'AI Capabilities Manifest', 'mimeType' => 'application/json'],
            ['uri' => 'mrpg://cookbook', 'name' => 'MRPG Authoring Cookbook & DSL Cheat Sheet', 'mimeType' => 'application/json'],
            ['uri' => 'mrpg://openapi', 'name' => 'OpenAPI 3.1.0 Specification', 'mimeType' => 'application/json'],
            ['uri' => 'mrpg://llms.txt', 'name' => 'Editor Studio llms.txt Guide', 'mimeType' => 'text/markdown'],
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

    if ($rpcMethod === 'resources/templates/list') {
        ai_require_scope('read');
        mcp_respond_rpc($rpcId, [
            'resourceTemplates' => [
                [
                    'uriTemplate' => 'mrpg://schema/{resource}',
                    'name' => 'Resource Schema',
                    'description' => 'Full field schema, enums, DSL syntax, and examples for a specific MRPG resource.',
                    'mimeType' => 'application/json',
                ],
                [
                    'uriTemplate' => 'mrpg://context/{resource}/{id}',
                    'name' => 'Composite Entity Context',
                    'description' => 'One-shot domain snapshot for a world, quest, item, bot, or scenario (respects guest ACL).',
                    'mimeType' => 'application/json',
                ],
            ],
        ]);
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
        if ($uri === 'mrpg://cookbook') {
            $book = ai_schema_cookbook();
            mcp_respond_rpc($rpcId, [
                'contents' => [['uri' => $uri, 'mimeType' => 'application/json', 'text' => (string)json_encode($book, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES | JSON_PRETTY_PRINT)]],
            ]);
        }
        if ($uri === 'mrpg://llms.txt') {
            $host = trim((string)($_SERVER['HTTP_HOST'] ?? 'localhost'));
            $proto = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off')
                || strtolower((string)($_SERVER['HTTP_X_FORWARDED_PROTO'] ?? '')) === 'https'
                ? 'https' : 'http';
            $scriptDir = rtrim(str_replace('\\', '/', dirname(dirname((string)($_SERVER['SCRIPT_NAME'] ?? '/api/mcp.php')))), '/');
            $base = $proto . '://' . $host . ($scriptDir === '' ? '' : $scriptDir);
            mcp_respond_rpc($rpcId, [
                'contents' => [['uri' => $uri, 'mimeType' => 'text/markdown', 'text' => ai_build_llms_txt($base)]],
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
        if (str_starts_with($uri, 'mrpg://context/')) {
            $rest = substr($uri, strlen('mrpg://context/'));
            $parts = explode('/', $rest, 2);
            if (count($parts) !== 2 || trim($parts[0]) === '' || trim($parts[1]) === '') {
                mcp_respond_rpc($rpcId, null, ['code' => -32602, 'message' => "Invalid context URI (expected mrpg://context/<resource>/<id>): {$uri}"], 400);
            }
            $db = db_connect();
            try {
                $ctx = ai_build_entity_context($db, trim($parts[0]), rawurldecode(trim($parts[1])), $ROLE, null);
                mcp_respond_rpc($rpcId, [
                    'contents' => [['uri' => $uri, 'mimeType' => 'application/json', 'text' => (string)json_encode($ctx, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES)]],
                ]);
            } finally {
                $db->close();
            }
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

    if ($toolName === 'search_editor') {
        ai_require_scope('read');
        $query = trim((string)($args['query'] ?? ($args['q'] ?? '')));
        $db = db_connect();
        try {
            $res = ai_global_search($db, $query, $ROLE, null, [
                'resources' => $args['resources'] ?? null,
                'limit_per_resource' => $args['limit_per_resource'] ?? null,
                'max_matches' => $args['max_matches'] ?? null,
            ]);
            mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true], $res)));
        } finally {
            $db->close();
        }
    }

    if ($toolName === 'get_context') {
        ai_require_scope('read');
        $resource = trim((string)($args['resource'] ?? ''));
        $rawId = trim((string)($args['id'] ?? ''));
        if ($resource === '' || $rawId === '') {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'missing_required_field', 'error' => 'Укажите resource и id.'], true));
        }
        $db = db_connect();
        try {
            $ctx = ai_build_entity_context($db, $resource, $rawId, $ROLE, null);
            mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true], $ctx)));
        } finally {
            $db->close();
        }
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
                'sort' => isset($args['sort']) ? trim((string)$args['sort']) : null,
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
                $worldId = isset($args['id']) && $args['id'] !== '' ? crud_record_id((string)$args['id'], 'worlds') : null;
                $pathArg = isset($args['path']) ? trim((string)$args['path']) : null;
                if ($worldId === null && ($pathArg === null || $pathArg === '')) {
                    mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'missing_required_field', 'error' => 'Укажите id мира или path карты.'], true));
                }
                $loaded = ai_map_load_bytes($db, $worldId, $pathArg);
                if (empty($loaded['available']) || !is_string($loaded['bytes'])) {
                    mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'resource' => 'maps', 'available' => false, 'reason' => $loaded['reason'] ?? 'Карта недоступна']));
                }
                $effectiveWid = $worldId ?? (isset($loaded['world']['ID']) ? (int)$loaded['world']['ID'] : null);
                $parsed = ai_map_parse_datafile(
                    $loaded['bytes'],
                    !empty($args['include_entities']),
                    isset($args['x']) ? (int)$args['x'] : null,
                    isset($args['y']) ? (int)$args['y'] : null,
                    $db,
                    $effectiveWid,
                    $ROLE,
                    null,
                    ['include_minimap' => !empty($args['include_minimap'])]
                );
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'resource' => 'maps', 'available' => true, 'world' => $loaded['world'], 'map' => $parsed]));
            }
            if (!empty($args['ids']) && is_array($args['ids'])) {
                $R = editor_resources()[$resource] + ['key' => $resource];
                $batch = ai_query_db_batch_get($db, $R, $args['ids'], [
                    'fields' => $args['fields'] ?? null,
                    'decode' => true,
                ]);
                mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true, 'resource' => $resource, 'batch' => true], $batch)));
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
                $out['references'] = ai_references_inspect($db, $resource, $recId, $ROLE, null, null, $row);
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

    if ($toolName === 'analyze_scenario') {
        ai_require_scope('read');
        if (!ai_can_read_resource('scenarios', $ROLE)) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'resource_forbidden', 'error' => 'Доступ к сценариям не выдан вашей учётной записи.'], true));
        }
        $search = trim((string)($args['search'] ?? ''));
        $id = trim((string)($args['id'] ?? ''));
        $db = db_connect();
        try {
            if ($id !== '') {
                $result = ai_analyze_scenario($db, $id, $ROLE, null);
                mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true, 'mode' => 'scenario', 'resource' => 'scenarios'], $result)));
            }
            $result = ai_analyze_scenario_corpus($db, $ROLE, null, null, $search);
            mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true, 'mode' => 'corpus', 'resource' => 'scenarios'], $result)));
        } finally {
            $db->close();
        }
    }

    if ($toolName === 'find_similar') {
        ai_require_scope('read');
        $resource = trim((string)($args['resource'] ?? ''));
        $known = isset(editor_resources()[$resource]) || isset(ai_schema_lookups()[$resource]);
        if ($resource === '' || !$known) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'unknown_resource', 'error' => "Неизвестный ресурс «{$resource}»."], true));
        }
        if (!ai_can_read_resource($resource, $ROLE)) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'resource_forbidden', 'error' => "Ресурс «{$resource}» недоступен для чтения."], true));
        }
        $query = trim((string)($args['name'] ?? ($args['q'] ?? '')));
        if ($query === '') {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'missing_required_field', 'error' => 'Укажите name для сравнения.'], true));
        }
        $limit = clamp_int($args['limit'] ?? null, 1, 25, 5);
        $db = db_connect();
        try {
            $similar = ai_find_similar_records($db, $resource, $query, $limit, $ROLE);
        } finally {
            $db->close();
        }
        mcp_respond_rpc($rpcId, mcp_tool_result([
            'ok' => true,
            'resource' => $resource,
            'query' => $query,
            'candidates' => $similar['candidates'] ?? [],
            'scanned' => $similar['scanned'] ?? 0,
            'threshold' => $similar['threshold'] ?? 0.35,
        ]));
    }

    if ($toolName === 'apply_patch') {
        ai_require_scope('read');
        $db = db_connect();
        try {
            $res = ai_apply_patch_verify($db, $args, $ROLE);
        } finally {
            $db->close();
        }
        $payload = $res['payload'];
        mcp_respond_rpc($rpcId, mcp_tool_result($payload, empty($payload['ok'])));
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
                    'resolved_refs' => $val['resolved_refs'] ?? new stdClass(),
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
                    'resolved_refs' => $val['resolved_refs'] ?? new stdClass(),
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
                    'resolved_refs' => $val['resolved_refs'] ?? new stdClass(),
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
                'resolved_refs' => $val['resolved_refs'] ?? new stdClass(),
                'request' => cr_full_view($res['req'], false),
            ]));
        } finally {
            $db->close();
        }
    }

    if ($toolName === 'manage_change_request') {
        $act = strtolower(trim((string)($args['action'] ?? '')));
        if (!in_array($act, ['list', 'get', 'reply', 'withdraw'], true)) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'invalid_action', 'error' => 'Укажите action: list, get, reply или withdraw.'], true));
        }

        if ($act === 'list') {
            ai_require_scope('read');
            $limit = clamp_int($args['limit'] ?? null, 1, 100, 20);
            $offset = clamp_int($args['offset'] ?? null, 0, 100000, 0);
            $filter = [];
            if (!$IS_ADMIN) {
                $filter['guest_id'] = (string)($account['id'] ?? '');
            }
            if (!empty($args['resource'])) {
                $filter['resource'] = trim((string)$args['resource']);
            }
            if (!empty($args['status'])) {
                $filter['status'] = trim((string)$args['status']);
            }
            $reqList = cr_query_list($filter, $limit, $offset);
            $items = array_map(static fn(array $r): array => cr_list_item($r, $IS_ADMIN), $reqList['rows']);
            mcp_respond_rpc($rpcId, mcp_tool_result([
                'ok' => true,
                'action' => 'list',
                'total' => $reqList['total'],
                'requests' => $items,
            ]));
        }

        $reqId = trim((string)($args['request_id'] ?? ($args['id'] ?? '')));
        if ($reqId === '') {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'missing_required_field', 'error' => 'Укажите request_id.'], true));
        }

        if ($act === 'get') {
            ai_require_scope('read');
            $req = cr_load_visible($reqId, $IS_ADMIN);
            $view = cr_full_view($req, $IS_ADMIN);
            $msgRole = $IS_ADMIN ? 'admin' : 'guest';
            $view['messages'] = cr_msg_list((string)$req['id'], $msgRole);
            $view['unread'] = cr_msg_unread_for((string)$req['id'], $msgRole);
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'action' => 'get', 'request' => $view]));
        }

        if ($act === 'reply') {
            if (!$IS_ADMIN && !ai_actor_has_scope('create') && !ai_actor_has_scope('update')) {
                ai_require_scope('update');
            }
            $req = cr_load_visible($reqId, $IS_ADMIN);
            $text = cr_msg_validate($args['body'] ?? ($args['text'] ?? ($args['message'] ?? '')));
            $msgRole = $IS_ADMIN ? 'admin' : 'guest';
            $authorId = $IS_ADMIN
                ? (string)($_SESSION['editor_studio_user'] ?? 'admin')
                : (string)($account['id'] ?? '');
            $authorName = $IS_ADMIN
                ? ($authorId !== '' ? $authorId : 'admin')
                : (string)($account['name'] ?? $req['author']['guestName'] ?? 'MCP Agent');
            $rateKey = 'cr-msg|' . $msgRole . '|' . ($authorId !== '' ? $authorId : get_request_ip());
            $now = time();
            $last = store_rate_get($rateKey);
            if ($last > 0 && $last + 3 > $now) {
                mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'rate_limited', 'error' => 'Слишком быстро — подождите несколько секунд.', 'retry_after' => $last + 3 - $now], true));
            }
            $msg = cr_with_lock(function () use ($req, $msgRole, $authorId, $authorName, $text) {
                $created = cr_msg_add((string)$req['id'], $msgRole, $authorId, $authorName, $text);
                cr_msg_mark_read((string)$req['id'], $msgRole);
                $fresh = cr_load((string)$req['id']) ?? $req;
                $fresh['updatedAt'] = cr_now();
                cr_save($fresh);
                return $created;
            });
            store_rate_touch($rateKey, $now);
            editor_audit('mcp.request.reply', (string)$req['id'], ['role' => $msgRole]);
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'action' => 'reply', 'request_id' => (string)$req['id'], 'message' => $msg]));
        }

        // withdraw
        if (!$IS_ADMIN && !ai_actor_has_scope('update') && !ai_actor_has_scope('delete')) {
            ai_require_scope('delete');
        }
        $updated = cr_with_lock(function () use ($reqId, $IS_ADMIN) {
            $req = cr_load_visible($reqId, $IS_ADMIN);
            if ($req['status'] !== 'pending') {
                throw new CrInputError('Можно отозвать только запрос на рассмотрении.', 409, ['code' => 'request_not_pending']);
            }
            $req['status'] = 'withdrawn';
            $req['updatedAt'] = cr_now();
            cr_save($req);
            return $req;
        });
        editor_audit('mcp.request.withdraw', (string)$updated['id']);
        mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => true, 'action' => 'withdraw', 'request' => cr_full_view($updated, $IS_ADMIN)]));
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

    if ($toolName === 'analyze_map') {
        ai_require_scope('read');
        if (!ai_can_read_resource('maps', $ROLE)) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'resource_forbidden', 'error' => 'Доступ к картам запрещён вашей учётной записи.'], true));
        }
        $worldId = isset($args['world_id']) && $args['world_id'] !== '' ? crud_record_id((string)$args['world_id'], 'worlds') : null;
        $pathArg = isset($args['path']) ? trim((string)$args['path']) : null;
        if ($worldId === null && ($pathArg === null || $pathArg === '')) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'missing_required_field', 'error' => 'Укажите world_id или относительный path карты.'], true));
        }
        $db = $worldId !== null ? db_connect() : null;
        try {
            $res = ai_map_analyze_request($db, $worldId, $pathArg, $ROLE, null, $args);
            mcp_respond_rpc($rpcId, mcp_tool_result(array_merge(['ok' => true], $res)));
        } finally {
            if ($db !== null) {
                $db->close();
            }
        }
    }

    if ($toolName === 'get_map_screenshot') {
        ai_require_scope('read');
        if (!ai_can_read_resource('maps', $ROLE)) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'resource_forbidden', 'error' => 'Доступ к картам запрещён вашей учётной записи.'], true));
        }
        $worldId = isset($args['world_id']) && $args['world_id'] !== '' ? crud_record_id((string)$args['world_id'], 'worlds') : null;
        $pathArg = isset($args['path']) ? trim((string)$args['path']) : null;
        if ($worldId === null && ($pathArg === null || $pathArg === '')) {
            mcp_respond_rpc($rpcId, mcp_tool_result(['ok' => false, 'code' => 'missing_required_field', 'error' => 'Укажите world_id или относительный path карты.'], true));
        }
        $db = $worldId !== null ? db_connect() : null;
        try {
            $res = ai_map_screenshot_request($db, $worldId, $pathArg, $ROLE, null, $args);
        } finally {
            if ($db !== null) {
                $db->close();
            }
        }
        $shot = $res['screenshot'];
        $pngBytes = (string)$shot['png_bytes'];
        unset($shot['png_bytes']);
        $meta = array_merge(['ok' => true], $res, ['screenshot' => $shot]);
        mcp_respond_rpc($rpcId, mcp_tool_image_result($meta, $pngBytes));
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
