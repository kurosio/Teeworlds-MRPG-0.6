<?php
// Схема данных, правил валидации, DSL-грамматик и связей MRPG для AI API.
//
// Модуль декларативен (без побочных HTTP-операций при подключении) и служит
// единым источником истины для endpoints `manifest`, `resources`, `validate`.

declare(strict_types=1);

require_once __DIR__ . '/db-resources.php';
require_once __DIR__ . '/reference-data.php';
require_once __DIR__ . '/scenario-files.php';

/**
 * Справочник метаданных всех ресурсов игровой БД + виртуальных ресурсов.
 *
 * @return array<string, array<string, mixed>>
 */
function ai_schema_tables(): array
{
    static $cache = null;
    if ($cache !== null) {
        return $cache;
    }
    $cache = [
        'worlds' => [
            'allow_zero_id' => true,
            'required_on_create' => ['Name', 'Path', 'Type', 'RespawnWorldID', 'JailWorldID'],
            'columns' => [
                'Name' => ['type' => 'string', 'nullable' => false, 'max_length' => 256, 'description' => 'Display name of the world/map'],
                'Path' => ['type' => 'string', 'nullable' => false, 'max_length' => 256, 'description' => 'Relative .map file path under maps directory'],
                'Type' => ['type' => 'enum', 'nullable' => false, 'enum' => ['default', 'dungeon', 'tutorial', 'deep_dungeon', 'treasure_dungeon', 'pvp', 'mini_games', 'rhythm', 'social']],
                'Flags' => ['type' => 'set', 'nullable' => true, 'set' => ['rating_system', 'crime_score', 'lost_gold_death', 'spawn_full_mana', 'allowed_pvp', 'no_prepare_map', 'no_multiplier', 'no_daytime', 'no_moneybag']],
                'RespawnWorldID' => ['type' => 'int', 'nullable' => false, 'min' => -1, 'ref' => 'tw_worlds'],
                'JailWorldID' => ['type' => 'int', 'nullable' => false, 'min' => -1, 'ref' => 'tw_worlds'],
                'RequiredLevel' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'default' => 1],
            ],
        ],
        'items' => [
            'required_on_create' => [],
            'columns' => [
                'Comment' => ['type' => 'string', 'nullable' => true, 'max_length' => 256],
                'Name' => ['type' => 'string', 'nullable' => false, 'max_length' => 32, 'default' => 'Item name'],
                'Description' => ['type' => 'string', 'nullable' => false, 'max_length' => 64, 'default' => 'Item desc'],
                'Group' => ['type' => 'enum', 'nullable' => false, 'enum' => ['Other', 'Usable', 'Resource', 'Quest', 'Settings', 'Equipment', 'Decoration', 'Potion', 'Currency'], 'default' => 'Quest'],
                'Type' => [
                    'type' => 'enum',
                    'nullable' => false,
                    'default' => 'Default',
                    'enum' => [
                        'Default', 'Equip hammer', 'Equip gun', 'Equip shotgun', 'Equip grenade', 'Equip rifle',
                        'Equip pickaxe', 'Equip rake', 'Equip fishrod', 'Equip gloves',
                        'Equip armor (tank)', 'Equip armor (dps)', 'Equip armor (healer)',
                        'Equip helmet (tank)', 'Equip helmet (dps)', 'Equip helmet (healer)',
                        'Equip eidolon', 'Equip title', 'Equip potion HP', 'Equip potion MP',
                        'Single use x1', 'Multiple use x99', 'Resource harvestable', 'Resource mineable',
                        'Resource fishes', 'Equip potion Miner',
                    ],
                ],
                'Flags' => ['type' => 'set', 'nullable' => true, 'set' => ["Can't droppable", "Can't tradeable"]],
                'ScenarioData' => ['type' => 'json', 'nullable' => true, 'json_kind' => 'scenario'],
                'ScenarioMode' => ['type' => 'enum', 'nullable' => false, 'enum' => ['universal', 'world'], 'default' => 'universal'],
                'InitialPrice' => ['type' => 'int', 'nullable' => true, 'min' => 0],
                'RequiresProducts' => ['type' => 'int', 'nullable' => true, 'min' => 0],
                'AT1' => ['type' => 'int', 'nullable' => true, 'ref' => 'tw_attributes'],
                'AT2' => ['type' => 'int', 'nullable' => true, 'ref' => 'tw_attributes'],
                'ATValue1' => ['type' => 'int', 'nullable' => true],
                'ATValue2' => ['type' => 'int', 'nullable' => true],
                'Data' => ['type' => 'json', 'nullable' => true, 'json_kind' => 'item_data', 'description' => 'JSON payload: bonus, random_box, box, potion, or use_scenario'],
            ],
            'group_type_rules' => [
                'Resource' => ['Default', 'Resource harvestable', 'Resource mineable', 'Resource fishes'],
                'Usable' => ['Default', 'Single use x1', 'Multiple use x99'],
                'Equipment' => [
                    'Default', 'Equip hammer', 'Equip gun', 'Equip shotgun', 'Equip grenade', 'Equip rifle',
                    'Equip pickaxe', 'Equip rake', 'Equip fishrod', 'Equip gloves',
                    'Equip armor (tank)', 'Equip armor (dps)', 'Equip armor (healer)',
                    'Equip helmet (tank)', 'Equip helmet (dps)', 'Equip helmet (healer)',
                    'Equip eidolon', 'Equip title',
                ],
                'Potion' => ['Default', 'Equip potion HP', 'Equip potion MP', 'Equip potion Miner'],
            ],
        ],
        'bots_info' => [
            'required_on_create' => [],
            'columns' => [
                'Name' => ['type' => 'string', 'nullable' => false, 'max_length' => 32, 'default' => 'Bot name'],
                'JsonTeeInfo' => ['type' => 'json', 'nullable' => false, 'max_length' => 256, 'json_kind' => 'tee_info', 'description' => '{"color_body":int,"color_feet":int,"custom_color":0|1,"skin":string}'],
                'EquippedModules' => ['type' => 'csv_int', 'nullable' => false, 'max_length' => 64, 'default' => '0', 'description' => 'Comma-separated item IDs equipped on the bot'],
                'SlotHammer' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'SlotGun' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'SlotShotgun' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'SlotGrenade' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'SlotRifle' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'SlotArmor' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
            ],
        ],
        'bots_mobs' => [
            'required_on_create' => ['PositionX', 'PositionY'],
            'columns' => [
                'BotID' => ['type' => 'int', 'nullable' => false, 'default' => -1, 'ref' => 'tw_bots_info'],
                'WorldID' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_worlds'],
                'PositionX' => ['type' => 'int', 'nullable' => false, 'description' => 'World X coordinate in pixels (tileX * 32 + 16)'],
                'PositionY' => ['type' => 'int', 'nullable' => false, 'description' => 'World Y coordinate in pixels (tileY * 32 + 16)'],
                'Debuffs' => ['type' => 'set', 'nullable' => true, 'set' => ['Slowdown', 'Poison', 'Fire']],
                'Behavior' => ['type' => 'set', 'nullable' => true, 'set' => ['sleepy', 'slower', 'poisonous', 'neutral', 'skills_base', 'skills_tank', 'skills_healer', 'skills_dps', 'ambient_chat']],
                'Level' => ['type' => 'int', 'nullable' => false, 'min' => 1, 'default' => 1],
                'Power' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'default' => 10],
                'Number' => ['type' => 'int', 'nullable' => false, 'min' => 1, 'default' => 1],
                'Respawn' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'default' => 1],
                'Radius' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'default' => 800],
                'ActiveRadius' => ['type' => 'int', 'nullable' => true, 'min' => 0],
                'WanderRadius' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'default' => 0],
                'Boss' => ['type' => 'bool_int', 'nullable' => false, 'min' => 0, 'max' => 1, 'default' => 0],
                'it_drop_0' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'it_drop_1' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'it_drop_2' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'it_drop_3' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'it_drop_4' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'it_drop_count' => ['type' => 'pipe_int5', 'nullable' => false, 'max_length' => 64, 'default' => '|0|0|0|0|0|'],
                'it_drop_chance' => ['type' => 'pipe_float5', 'nullable' => false, 'max_length' => 64, 'default' => '|0|0|0|0|0|'],
            ],
        ],
        'bots_npc' => [
            'required_on_create' => ['PosX', 'PosY', 'Static'],
            'columns' => [
                'BotID' => ['type' => 'int', 'nullable' => false, 'default' => -1, 'ref' => 'tw_bots_info'],
                'WorldID' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_worlds'],
                'PosX' => ['type' => 'int', 'nullable' => false],
                'PosY' => ['type' => 'int', 'nullable' => false],
                'GiveQuestID' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_quests_list'],
                'Function' => ['type' => 'int', 'nullable' => false, 'min' => -1, 'max' => 2, 'default' => -1, 'description' => 'NPC role: -1=None, 0=Nurse, 1=GiveQuest, 2=Guardian'],
                'Static' => ['type' => 'bool_int', 'nullable' => false, 'min' => 0, 'max' => 1],
                'Emote' => ['type' => 'enum', 'nullable' => true, 'enum' => ['Pain', 'Happy', 'Surprise', 'Angry', 'Blink']],
                'DialogData' => ['type' => 'json', 'nullable' => true, 'json_kind' => 'dialog_list'],
            ],
        ],
        'crafts' => [
            'required_on_create' => ['GroupName', 'ItemValue', 'RequiredItems', 'WorldID'],
            'columns' => [
                'GroupName' => ['type' => 'string', 'nullable' => false, 'max_length' => 256, 'description' => 'Format: |Main|Group:Sub (or pipe-prefixed group hierarchy)'],
                'ItemID' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_items_list'],
                'ItemValue' => ['type' => 'int', 'nullable' => false, 'min' => 1],
                'RequiredItems' => ['type' => 'dsl_required_items', 'nullable' => false, 'max_length' => 256, 'description' => 'Comma-separated [ItemID/Qty] pairs, e.g. [12/3],[45/1]'],
                'Price' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'default' => 100],
                'WorldID' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'ref' => 'tw_worlds'],
            ],
        ],
        'warehouses' => [
            'required_on_create' => ['PosX', 'PosY', 'WorldID'],
            'columns' => [
                'Name' => ['type' => 'string', 'nullable' => false, 'max_length' => 32, 'default' => 'Bussines name'],
                'Type' => ['type' => 'set', 'nullable' => false, 'set' => ['buying', 'selling', 'storage'], 'default' => 'buying'],
                'Trades' => ['type' => 'dsl_trades', 'nullable' => true, 'description' => 'Multiline *Group headers and [ItemID/Qty(Price[/Alt])] or [COLLECTION(g:s)] entries'],
                'PosX' => ['type' => 'int', 'nullable' => false],
                'PosY' => ['type' => 'int', 'nullable' => false],
                'StorageData' => ['type' => 'json', 'nullable' => true, 'json_kind' => 'storage_data', 'description' => '{"value":"<bigint_str>","position":{"x":int,"y":int}}'],
                'Currency' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'default' => 1, 'ref' => 'tw_items_list'],
                'WorldID' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'ref' => 'tw_worlds'],
            ],
        ],
        'nodes_collecting' => [
            'required_on_create' => ['SwitchID', 'Name', 'Properties', 'Type', 'WorldID'],
            'unique_keys' => [['WorldID', 'Type', 'SwitchID']],
            'columns' => [
                'SwitchID' => ['type' => 'int', 'nullable' => false, 'min' => 1, 'max' => 255, 'description' => 'Map switch tile number (1..255), unique per (WorldID, Type)'],
                'Level' => ['type' => 'int', 'nullable' => false, 'min' => 1, 'default' => 1],
                'Health' => ['type' => 'int', 'nullable' => false, 'min' => 1, 'default' => 1],
                'Name' => ['type' => 'string', 'nullable' => false, 'max_length' => 128],
                'Properties' => ['type' => 'dsl_collecting_properties', 'nullable' => false, 'description' => 'One item per line or space/comma separated: [ItemID/Chance] or [ItemID/Qty/Chance]'],
                'Type' => ['type' => 'enum', 'nullable' => false, 'enum' => ['ore', 'plant', 'fish']],
                'WorldID' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'ref' => 'tw_worlds'],
            ],
        ],
        'aethers' => [
            'required_on_create' => ['WorldID', 'TeleX', 'TeleY'],
            'columns' => [
                'Name' => ['type' => 'string', 'nullable' => false, 'max_length' => 64, 'default' => 'Teleport name'],
                'WorldID' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'ref' => 'tw_worlds'],
                'TeleX' => ['type' => 'int', 'nullable' => false],
                'TeleY' => ['type' => 'int', 'nullable' => false],
            ],
        ],
        'dungeons' => [
            'required_on_create' => ['WorldID'],
            'columns' => [
                'Level' => ['type' => 'int', 'nullable' => false, 'min' => 1, 'default' => 1],
                'DoorX' => ['type' => 'int', 'nullable' => false, 'default' => 0],
                'DoorY' => ['type' => 'int', 'nullable' => false, 'default' => 0],
                'Scenario' => ['type' => 'json', 'nullable' => true, 'json_kind' => 'scenario'],
                'WorldID' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'ref' => 'tw_worlds'],
                'TimeLimit' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'default' => 600],
            ],
        ],
        'quests' => [
            'required_on_create' => ['Money', 'Exp'],
            'columns' => [
                'NextQuestID' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_quests_list'],
                'Name' => ['type' => 'string', 'nullable' => false, 'max_length' => 24, 'default' => 'Quest name'],
                'Money' => ['type' => 'int', 'nullable' => false, 'min' => 0],
                'Exp' => ['type' => 'int', 'nullable' => false, 'min' => 0],
                'Flags' => ['type' => 'set', 'nullable' => false, 'default' => 'Type main', 'set' => ['Type main', 'Type side', 'Type daily', 'Type weekly', 'Type repeatable', "Can't refuse", 'No activity point', 'Tutorial']],
            ],
        ],
        'quest_boards' => [
            'required_on_create' => ['Name', 'PosX', 'PosY', 'WorldID'],
            'columns' => [
                'Name' => ['type' => 'string', 'nullable' => false, 'max_length' => 32],
                'PosX' => ['type' => 'int', 'nullable' => false],
                'PosY' => ['type' => 'int', 'nullable' => false],
                'WorldID' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'ref' => 'tw_worlds'],
            ],
        ],
        'quest_board_links' => [
            'required_on_create' => ['QuestID', 'DailyBoardID'],
            'columns' => [
                'QuestID' => ['type' => 'int', 'nullable' => false, 'min' => 1, 'ref' => 'tw_quests_list'],
                'DailyBoardID' => ['type' => 'int', 'nullable' => false, 'min' => 1, 'ref' => 'tw_quest_boards'],
            ],
        ],
        'quest_bots' => [
            'required_on_create' => ['PosX', 'PosY'],
            'columns' => [
                'BotID' => ['type' => 'int', 'nullable' => false, 'default' => -1, 'ref' => 'tw_bots_info'],
                'QuestID' => ['type' => 'int', 'nullable' => false, 'default' => -1, 'ref' => 'tw_quests_list'],
                'Step' => ['type' => 'int', 'nullable' => false, 'min' => 1, 'default' => 1],
                'WorldID' => ['type' => 'int', 'nullable' => true, 'min' => 0, 'ref' => 'tw_worlds'],
                'PosX' => ['type' => 'int', 'nullable' => false],
                'PosY' => ['type' => 'int', 'nullable' => false],
                'AutoFinish' => ['type' => 'set', 'nullable' => true, 'set' => ['Partial']],
                'DialogData' => ['type' => 'json', 'nullable' => true, 'json_kind' => 'dialog_list'],
                'ScenarioData' => ['type' => 'json', 'nullable' => true, 'json_kind' => 'scenario'],
                'TasksData' => ['type' => 'json', 'nullable' => true, 'json_kind' => 'tasks_data'],
            ],
        ],
        'vouchers' => [
            'required_on_create' => ['Code', 'Data'],
            'columns' => [
                'Code' => ['type' => 'string', 'nullable' => false, 'max_length' => 32],
                'ValidUntil' => ['type' => 'int', 'nullable' => false, 'min' => 0, 'default' => 0, 'description' => 'Unix timestamp (seconds); 0 = never expires'],
                'Multiple' => ['type' => 'bool_int', 'nullable' => false, 'min' => 0, 'max' => 1],
                'Data' => ['type' => 'json', 'nullable' => false, 'description' => 'JSON payload: {"exp":int,"money":int,"items":[{"id":int,"value":int}]}'],
            ],
        ],
    ];
    return $cache;
}

/** Справочники-словари (только чтение). */
function ai_schema_lookups(): array
{
    static $cache = null;
    if ($cache !== null) {
        return $cache;
    }
    $cache = [
        'attributes' => [
            'table' => 'tw_attributes',
            'pk' => 'ID',
            'label' => 'Name',
            'title' => 'Атрибуты (справочник)',
            'columns' => ['ID', 'Name', 'Price', 'Group'],
            'search' => ['Name'],
            'order' => 'ID ASC',
            'tabs' => ['items', 'bots_mobs'],
            'readonly' => true,
        ],
        'skills' => [
            'table' => 'tw_skills_list',
            'pk' => 'ID',
            'label' => 'Name',
            'title' => 'Умения (справочник)',
            'columns' => ['ID', 'Name', 'Description', 'ManaCostPct', 'PriceSP', 'Passive', 'ProfessionID'],
            'search' => ['Name', 'Description'],
            'order' => 'ID ASC',
            'tabs' => ['items', 'bots_mobs', 'quests'],
            'readonly' => true,
        ],
    ];
    return $cache;
}

/** Схемы сценариев и доступных компонентов (соответствует editor-core/registry.js). */
function ai_schema_scenario_definitions(): array
{
    static $cache = null;
    if ($cache !== null) {
        return $cache;
    }
    $defaultComponents = [
        'branch_random', 'use_chat_code', 'message', 'wait', 'follow_camera',
        'dynamic_condition', 'condition_movement', 'teleport', 'moving_disable',
        'emote', 'quest_action', 'quest_condition', 'defeat_mobs', 'restore_resources',
    ];
    $cache = [
        'modes' => [
            'default' => ['label' => 'Default', 'components' => $defaultComponents],
            'dungeon' => [
                'label' => 'Dungeon',
                'components' => array_merge($defaultComponents, [
                    'set_group_flags', 'set_group_lives', 'set_group_spawn',
                    'dungeon_door_control', 'dungeon_activate_point', 'dungeon_complete',
                ]),
            ],
            'universal' => [
                'label' => 'Universal',
                'components' => array_merge($defaultComponents, [
                    'universal_door_control', 'universal_condition_item',
                    'universal_pick_item_task', 'universal_shootmarkers',
                ]),
            ],
            'world' => [
                'label' => 'World',
                'components' => array_merge($defaultComponents, [
                    'set_group_flags', 'set_group_lives', 'set_group_spawn',
                    'world_group_lives', 'world_complete',
                ]),
            ],
        ],
        'events' => [
            'on_recieve_objectives', 'on_complete_objectives', 'on_end',
            'on_got', 'on_lost', 'on_equip', 'on_unequip', 'on_use',
        ],
        'step_completion_logic' => ['all_of', 'any_of', 'sequential'],
        'link_fields' => ['next_step_id', 'true_step_id', 'false_step_id', 'pass_step_id', 'fail_step_id'],
        'known_components' => scenario_known_component_types(),
        'legacy_components' => scenario_legacy_component_types(),
        'timing_unit' => 'ticks (50 ticks ≈ 1 second in Teeworlds/DDNet)',
    ];
    return $cache;
}

/** Схема тайлов и координат DDNet .map для AI-инспекции карт. */
function ai_schema_map_definitions(): array
{
    static $cache = null;
    if ($cache !== null) {
        return $cache;
    }
    $cache = [
        'tile_size_px' => 32,
        'tile_center_offset_px' => 16,
        'formula' => 'pixel = tile * 32 + 16; tile = floor(pixel / 32)',
        'collision_tiles' => [
            1 => 'solid',
            2 => 'death',
            3 => 'unhookable',
        ],
        'game_entity_tiles' => [
            192 => 'armor', 193 => 'health', 194 => 'shotgun', 195 => 'grenade', 196 => 'laser',
            197 => 'npcWall', 198 => 'mobWall',
            199 => 'plant', 200 => 'ore', 201 => 'fish',
            208 => 'spawn', 209 => 'spawnMobs', 211 => 'spawnPrison',
            241 => 'rhythm', 242 => 'casinoDice', 243 => 'casinoRoulette',
        ],
        'switch_tiles' => [
            22 => 'swZone', 23 => 'swText', 24 => 'swHouseZone', 25 => 'swActionZone',
            199 => 'plant', 200 => 'ore', 201 => 'fish',
        ],
        'special_tiles' => [
            4 => 'fixed_camera', 5 => 'fixed_camera_smooth', 6 => 'water', 8 => 'player_house',
            11 => 'destroyer_projectile', 14 => 'world_swapper', 15 => 'jail_zone', 16 => 'guild_house',
            17 => 'auction', 21 => 'quest_board', 28 => 'aether_teleport', 29 => 'shop_zone',
            30 => 'craft_zone', 32 => 'guild_chair', 33 => 'chair_lv1', 34 => 'chair_lv2',
            35 => 'chair_lv3', 38 => 'bank_manager', 39 => 'fishing_mode', 40 => 'casino_action',
            60 => 'stop_all', 61 => 'stop_single', 62 => 'stop_both',
        ],
        'tele_tiles' => [
            10 => 'tele_from_confirm',
            26 => 'tele_from',
            27 => 'tele_out',
        ],
        'visual_categories' => [
            'architecture', 'interior', 'terrain', 'nature', 'water', 'props', 'lighting_fx', 'sky_bg', 'other',
        ],
        'structure_kinds' => [
            'castle_or_guild_hall', 'house', 'shop_or_service', 'dungeon_or_ruins', 'cave_or_underground', 'building',
        ],
        'screenshot_modes' => [
            'hybrid', 'visual', 'schematic',
        ],
    ];
    return $cache;
}

/**
 * Проверка одного значения SET (строка через запятую или массив строк).
 *
 * @return array{ok:bool, invalid?:string[], normalized?:string}
 */
function ai_validate_set_value(mixed $value, array $allowed): array
{
    if ($value === null || $value === '') {
        return ['ok' => true, 'normalized' => ''];
    }
    if (is_array($value)) {
        $tokens = array_map(static fn($x) => trim((string)$x), $value);
    } elseif (is_string($value)) {
        $tokens = array_map('trim', explode(',', $value));
    } else {
        return ['ok' => false, 'invalid' => ['(non-string)']];
    }
    $tokens = array_values(array_filter($tokens, static fn(string $t): bool => $t !== ''));
    $allowedSet = array_flip($allowed);
    $invalid = [];
    foreach ($tokens as $tok) {
        if (!isset($allowedSet[$tok])) {
            $invalid[] = $tok;
        }
    }
    if ($invalid) {
        return ['ok' => false, 'invalid' => $invalid];
    }
    return ['ok' => true, 'normalized' => implode(',', array_values(array_unique($tokens)))];
}

/**
 * Проверка DSL `RequiredItems`: запятая-разделённые пары `[ItemID/Qty]`.
 */
function ai_validate_required_items_dsl(mixed $value): ?string
{
    if ($value === null || trim((string)$value) === '') {
        return null;
    }
    $raw = trim((string)$value);
    // Убираем валидные токены [id/qty] (разделители: запятая и/или пробелы).
    $remainder = preg_replace('/\[(\s*\d+\s*\/\s*\d+\s*)\]/', '', $raw);
    if (!is_string($remainder) || trim(str_replace(',', '', $remainder)) !== '') {
        return 'Ожидается формат [ItemID/Qty], перечисленный через запятую (например, [10/2],[15/1]).';
    }
    if (!preg_match_all('/\[\s*(\d+)\s*\/\s*(\d+)\s*\]/', $raw, $m, PREG_SET_ORDER)) {
        return 'Укажите хотя бы одну корректную пару [ItemID/Qty].';
    }
    foreach ($m as $pair) {
        if ((int)$pair[1] <= 0 || (int)$pair[2] <= 0) {
            return 'В паре [ItemID/Qty] оба числа должны быть больше 0.';
        }
    }
    return null;
}

/**
 * Проверка pipe-вектора из 5 чисел `|a|b|c|d|e|`.
 */
function ai_validate_pipe_vector(mixed $value, bool $allowFloat): ?string
{
    if (!is_string($value) || trim($value) === '') {
        return 'Ожидается строка формата |n0|n1|n2|n3|n4|.';
    }
    $numRe = $allowFloat ? '(?:\d+(?:\.\d+)?)' : '\d+';
    if (preg_match('/^\|' . $numRe . '\|' . $numRe . '\|' . $numRe . '\|' . $numRe . '\|' . $numRe . '\|$/', trim($value)) !== 1) {
        return 'Ожидается ровно 5 неотрицательных чисел в формате |n0|n1|n2|n3|n4|.';
    }
    return null;
}

/**
 * Проверка DSL `tw_warehouses.Trades`.
 */
function ai_validate_trades_dsl(mixed $value): ?string
{
    if ($value === null || trim((string)$value) === '') {
        return null;
    }
    if (!is_string($value)) {
        return 'Поле Trades должно быть строкой.';
    }
    $lines = preg_split('/\r?\n/', $value) ?: [];
    foreach ($lines as $lineNo => $rawLine) {
        $line = trim($rawLine);
        if ($line === '' || str_starts_with($line, '*') || str_starts_with($line, '#') || str_starts_with($line, '//')) {
            continue;
        }
        $isTrade = preg_match('/^\[\s*\d+\s*\/\s*\d+\s*\(\s*\d+(?:\s*\/\s*\d+)?\s*\)\s*\]$/', $line) === 1;
        $isCollection = preg_match('/^\[COLLECTION\([^)]+\)\]$/', $line) === 1;
        if (!$isTrade && !$isCollection) {
            return 'Строка ' . ($lineNo + 1) . ": некорректная запись сделки «{$line}»; ожидается *Группа, [ItemID/Qty(Price)] или [COLLECTION(g:s)].";
        }
    }
    return null;
}

/**
 * Полевая и доменная проверка одной операции с записью игровой БД.
 *
 * @param array<string, mixed> $rawData Исходные (декодированные) поля из запроса AI
 * @param array<string, mixed> $dbData  Нормализованные поля (JSON уже сериализован)
 * @param array<int, string>   $tempKnown
 * @param array<string, true>  $deletedMap
 * @return array{errors:array<int, array<string, mixed>>, warnings:array<int, array<string, mixed>>}
 */
function ai_schema_validate_db_record(
    string $resourceKey,
    string $opType,
    array $rawData,
    array $dbData,
    int $opIndex,
    ?mysqli $db = null,
    array $tempKnown = [],
    array $deletedMap = [],
    ?int $recordId = null
): array {
    $errors = [];
    $warnings = [];
    $R = editor_resources()[$resourceKey] ?? null;
    $schema = ai_schema_for_resource($resourceKey);
    if ($schema === null || $R === null) {
        return ['errors' => $errors, 'warnings' => $warnings];
    }

    $addError = static function (string $code, string $field, string $message, array $extra = []) use (&$errors, $opIndex, $resourceKey): void {
        $errors[] = array_merge([
            'code' => $code,
            'operationIndex' => $opIndex,
            'resource' => $resourceKey,
            'field' => $field,
            'message' => $message,
        ], $extra);
    };
    $addWarning = static function (string $code, string $field, string $message, array $extra = []) use (&$warnings, $opIndex, $resourceKey): void {
        $warnings[] = array_merge([
            'code' => $code,
            'operationIndex' => $opIndex,
            'resource' => $resourceKey,
            'field' => $field,
            'message' => $message,
        ], $extra);
    };

    $colSchemas = $schema['columns'] ?? [];

    if ($opType === 'create') {
        foreach ($schema['required_on_create'] ?? [] as $reqCol) {
            if (!array_key_exists($reqCol, $rawData) || $rawData[$reqCol] === null || ($rawData[$reqCol] === '' && (($colSchemas[$reqCol]['type'] ?? '') !== 'string'))) {
                $reqDef = $colSchemas[$reqCol] ?? [];
                $extra = ['expected_type' => (string)($reqDef['type'] ?? 'string')];
                if (array_key_exists('default', $reqDef)) {
                    $extra['default'] = $reqDef['default'];
                }
                if (!empty($reqDef['enum'])) {
                    $extra['allowed'] = $reqDef['enum'];
                } elseif (!empty($reqDef['set'])) {
                    $extra['allowed'] = $reqDef['set'];
                }
                if (!empty($reqDef['description'])) {
                    $extra['hint'] = (string)$reqDef['description'];
                }
                $addError('missing_required_field', $reqCol, "Обязательное поле «{$reqCol}» не задано для создания записи «{$R['title']}».", $extra);
            }
        }
    }

    foreach ($rawData as $col => $rawVal) {
        if ($col === ($R['pk'] ?? 'ID')) {
            continue;
        }
        $colDef = $colSchemas[$col] ?? null;
        if ($colDef === null) {
            continue;
        }

        if ($rawVal === null) {
            if (($colDef['nullable'] ?? true) === false) {
                $addError('null_not_allowed', $col, "Поле «{$col}» не допускает значение NULL.");
            }
            continue;
        }

        $type = (string)($colDef['type'] ?? 'string');

        if ($type === 'int' || $type === 'bool_int') {
            if (!is_int($rawVal) && !is_bool($rawVal) && !(is_string($rawVal) && preg_match('/^-?\d+$/', trim($rawVal)) === 1)) {
                $addError('invalid_integer', $col, "Поле «{$col}» должно быть целым числом.");
                continue;
            }
            $num = is_bool($rawVal) ? ($rawVal ? 1 : 0) : (int)$rawVal;
            if (isset($colDef['min']) && $num < (int)$colDef['min']) {
                $addError('out_of_range', $col, "Поле «{$col}» должно быть не меньше {$colDef['min']}.", ['min' => $colDef['min']]);
            }
            if (isset($colDef['max']) && $num > (int)$colDef['max']) {
                $addError('out_of_range', $col, "Поле «{$col}» должно быть не больше {$colDef['max']}.", ['max' => $colDef['max']]);
            }
        } elseif ($type === 'enum') {
            $valStr = is_scalar($rawVal) ? (string)$rawVal : '';
            if ($valStr === '' && ($colDef['nullable'] ?? false)) {
                continue;
            }
            $allowed = $colDef['enum'] ?? [];
            if (!in_array($valStr, $allowed, true)) {
                $suggest = ai_schema_suggest_closest($valStr, $allowed, 3);
                $extra = ['allowed' => $allowed];
                if ($suggest !== []) {
                    $extra['did_you_mean'] = $suggest;
                    $extra['hint'] = 'Возможно, имелось в виду: ' . implode(', ', $suggest) . '.';
                }
                $addError('invalid_enum_value', $col, "Недопустимое значение «{$valStr}» для поля «{$col}».", $extra);
            }
        } elseif ($type === 'set') {
            $allowed = $colDef['set'] ?? [];
            $check = ai_validate_set_value($rawVal, $allowed);
            if (!$check['ok']) {
                $extra = ['allowed' => $allowed];
                $sug = [];
                foreach ($check['invalid'] ?? [] as $inv) {
                    foreach (ai_schema_suggest_closest((string)$inv, $allowed, 2) as $cand) {
                        $sug[$cand] = true;
                    }
                }
                if ($sug !== []) {
                    $extra['did_you_mean'] = array_keys($sug);
                }
                $addError('invalid_set_value', $col, 'Недопустимые флаги в поле «' . $col . '»: ' . implode(', ', $check['invalid'] ?? []), $extra);
            }
        } elseif ($type === 'string') {
            if (!is_string($rawVal) && !is_numeric($rawVal)) {
                $addError('invalid_field_type', $col, "Поле «{$col}» должно быть строкой.");
                continue;
            }
            $str = (string)$rawVal;
            if (isset($colDef['max_length']) && mb_strlen($str, 'UTF-8') > (int)$colDef['max_length']) {
                $addError('max_length_exceeded', $col, "Поле «{$col}» превышает максимальную длину {$colDef['max_length']} симв.", ['max_length' => $colDef['max_length']]);
            }
        } elseif ($type === 'csv_int') {
            $str = is_scalar($rawVal) ? trim((string)$rawVal) : '';
            if ($str !== '' && preg_match('/^\s*-?\d*(?:\s*,\s*-?\d*)*\s*$/', $str) !== 1) {
                $addError('invalid_csv_ids', $col, "Поле «{$col}» должно содержать целые ID через запятую.");
            }
            if (isset($colDef['max_length']) && strlen($str) > (int)$colDef['max_length']) {
                $addError('max_length_exceeded', $col, "Поле «{$col}» превышает длину {$colDef['max_length']} байт.");
            }
        } elseif ($type === 'pipe_int5' || $type === 'pipe_float5') {
            $err = ai_validate_pipe_vector($rawVal, $type === 'pipe_float5');
            if ($err !== null) {
                $addError('invalid_pipe_vector', $col, $err);
            }
        } elseif ($type === 'dsl_required_items') {
            $err = ai_validate_required_items_dsl($rawVal);
            if ($err !== null) {
                $addError('invalid_dsl_format', $col, $err);
            }
            if (is_string($rawVal) && isset($colDef['max_length']) && strlen($rawVal) > (int)$colDef['max_length']) {
                $addError('max_length_exceeded', $col, "Поле «{$col}» превышает {$colDef['max_length']} байт.");
            }
        } elseif ($type === 'dsl_trades') {
            $err = ai_validate_trades_dsl($rawVal);
            if ($err !== null) {
                $addError('invalid_dsl_format', $col, $err);
            }
        } elseif ($type === 'json') {
            $decoded = null;
            if (is_array($rawVal)) {
                $decoded = $rawVal;
            } elseif (is_string($rawVal)) {
                $trimmed = trim($rawVal);
                if ($trimmed === '') {
                    if (($colDef['nullable'] ?? true) === false) {
                        $addError('invalid_json', $col, "Поле «{$col}» требует валидный JSON.");
                    }
                    continue;
                }
                $decoded = json_decode($trimmed, true);
                if (json_last_error() !== JSON_ERROR_NONE) {
                    $addError('invalid_json', $col, "Поле «{$col}» содержит некорректный JSON: " . json_last_error_msg(), [
                        'expected_type' => 'json',
                        'hint' => 'Передайте валидный JSON-объект/массив или JSON-строку (либо используйте patch_json для частичного обновления).',
                    ]);
                    continue;
                }
            } else {
                $addError('invalid_json', $col, "Поле «{$col}» должно быть JSON-объектом/массивом или JSON-строкой.");
                continue;
            }

            if (isset($colDef['max_length'])) {
                $encoded = json_encode($decoded, JSON_UNESCAPED_UNICODE);
                if (is_string($encoded) && strlen($encoded) > (int)$colDef['max_length']) {
                    $addError('max_length_exceeded', $col, "JSON в поле «{$col}» превышает {$colDef['max_length']} байт.");
                }
            }

            $kind = (string)($colDef['json_kind'] ?? '');
            if ($kind === 'tee_info' && is_array($decoded)) {
                if (array_is_list($decoded) && $decoded !== []) {
                    $addError('invalid_json_structure', $col, 'JsonTeeInfo должен быть JSON-объектом (skin, color_body, color_feet, custom_color).');
                }
            } elseif ($kind === 'dialog_list' && $decoded !== null && (!is_array($decoded) || (!array_is_list($decoded) && $decoded !== []))) {
                $addError('invalid_json_structure', $col, 'DialogData должен быть JSON-массивом реплик диалога.');
            } elseif ($kind === 'storage_data' && is_array($decoded)) {
                if (isset($decoded['position']) && (!is_array($decoded['position']) || !isset($decoded['position']['x'], $decoded['position']['y']))) {
                    $addError('invalid_json_structure', $col, 'StorageData.position должен содержать числовые поля x и y.');
                }
            } elseif ($kind === 'scenario' && is_array($decoded)) {
                $scCheck = scenario_validate_content($decoded, $db, $tempKnown, $deletedMap);
                foreach ($scCheck['errors'] as $scErr) {
                    $addError((string)$scErr['code'], "{$col}." . preg_replace('/^content\./', '', (string)$scErr['field']), (string)$scErr['message']);
                }
                foreach ($scCheck['warnings'] as $scWarn) {
                    $addWarning((string)$scWarn['code'], "{$col}." . preg_replace('/^content\./', '', (string)$scWarn['field']), (string)$scWarn['message']);
                }
            }
        }
    }

    // Проверка согласованности Group -> Type для items (предупреждение).
    if ($resourceKey === 'items' && isset($rawData['Group'], $rawData['Type']) && is_string($rawData['Group']) && is_string($rawData['Type'])) {
        $groupRules = $schema['group_type_rules'][$rawData['Group']] ?? ['Default'];
        if (!in_array($rawData['Type'], $groupRules, true)) {
            $addWarning('group_type_mismatch', 'Type', "Для группы «{$rawData['Group']}» обычно используются типы: " . implode(', ', $groupRules), ['recommended' => $groupRules]);
        }
    }

    // Взаимоисключающие флаги типа квеста (в quest_data.h проверяются через if / else if).
    if ($resourceKey === 'quests' && array_key_exists('Flags', $rawData) && $rawData['Flags'] !== null && $rawData['Flags'] !== '') {
        $flagsRaw = is_array($rawData['Flags']) ? $rawData['Flags'] : explode(',', (string)$rawData['Flags']);
        $flagsNorm = array_values(array_unique(array_filter(array_map('trim', array_map('strval', $flagsRaw)), static fn(string $v): bool => $v !== '')));
        $exclusive = ['Tutorial', 'Type main', 'Type side', 'Type daily', 'Type weekly', 'Type repeatable'];
        $picked = array_values(array_intersect($flagsNorm, $exclusive));
        if (count($picked) > 1) {
            $addError('quest_flags_conflict', 'Flags', 'Флаги типа квеста взаимоисключающие (' . implode(', ', $picked) . '): выберите только один.');
        }
    }

    // Доменная проверка для nodes_collecting (SwitchID 1..255, формат Properties, уникальность WorldID+Type+SwitchID).
    if ($resourceKey === 'nodes_collecting' && $rawData) {
        $merged = $rawData;
        if ($opType === 'update' && $db !== null && $recordId !== null) {
            $existing = crud_fetch_row($db, $R, $recordId);
            if ($existing) {
                $merged = array_merge($existing, $dbData);
            }
        }
        $checkWhole = ($opType === 'create')
            || array_key_exists('SwitchID', $rawData)
            || array_key_exists('Properties', $rawData)
            || array_key_exists('Type', $rawData)
            || array_key_exists('WorldID', $rawData);
        if ($checkWhole && ($opType === 'create' || $db !== null)) {
            $canRunFull = isset($merged['SwitchID'], $merged['Level'], $merged['Health'], $merged['WorldID'], $merged['Type'], $merged['Name'], $merged['Properties']);
            if ($canRunFull) {
                try {
                    validate_collecting_node($merged);
                } catch (Throwable $e) {
                    $msg = editor_safe_message($e);
                    $field = str_contains($msg, 'Properties') ? 'Properties'
                        : (str_contains($msg, 'Тип узла') ? 'Type'
                        : (str_contains($msg, 'Название') ? 'Name' : 'SwitchID'));
                    $addError('collecting_node_invalid', $field, $msg);
                }
                if ($db !== null) {
                    $wid = (int)$merged['WorldID'];
                    $ntype = (string)$merged['Type'];
                    $sid = (int)$merged['SwitchID'];
                    $excl = $opType === 'update' && $recordId !== null ? $recordId : -1;
                    $stmt = $db->prepare('SELECT `ID` FROM `tw_nodes_collecting` WHERE `WorldID` = ? AND `Type` = ? AND `SwitchID` = ? AND `ID` <> ? LIMIT 1');
                    if ($stmt) {
                        $stmt->bind_param('isii', $wid, $ntype, $sid, $excl);
                        $stmt->execute();
                        $dup = $stmt->get_result()->fetch_row();
                        $stmt->close();
                        if ($dup !== null) {
                            $addError('duplicate_unique_key', 'SwitchID', "Узел сбора для (WorldID={$wid}, Type={$ntype}, SwitchID={$sid}) уже существует (#{$dup[0]}).");
                        }
                    }
                }
            }
        }
    }

    // Проверка уникальности назначения квеста на доску (quest_board_links).
    if ($resourceKey === 'quest_board_links' && $db !== null && ($opType === 'create' || array_key_exists('QuestID', $rawData))) {
        $qId = $rawData['QuestID'] ?? null;
        if (is_numeric($qId) && !is_guest_temp_id((int)$qId) && (int)$qId > 0) {
            $questInt = (int)$qId;
            $excl = $opType === 'update' && $recordId !== null ? $recordId : -1;
            $stmt = $db->prepare('SELECT `ID` FROM `tw_quests_board_list` WHERE `QuestID` = ? AND `ID` <> ? LIMIT 1');
            if ($stmt) {
                $stmt->bind_param('ii', $questInt, $excl);
                $stmt->execute();
                $dup = $stmt->get_result()->fetch_row();
                $stmt->close();
                if ($dup !== null) {
                    $addError('duplicate_unique_key', 'QuestID', "Квест #{$questInt} уже назначен на доску квестов (запись #{$dup[0]}).");
                }
            }
        }
    }

    return ['errors' => $errors, 'warnings' => $warnings];
}

/**
 * Автоматический вывод метаданных колонок для любого нового ресурса,
 * добавленного в editor_resources(), без ручного дублирования логики.
 */
function ai_schema_auto_resource(string $resourceKey, array $R): array
{
    $required = array_values(array_map('strval', $R['required'] ?? []));
    $jsonSet = array_flip(array_map('strval', $R['json'] ?? []));
    $refs = is_array($R['refs'] ?? null) ? $R['refs'] : [];
    $cols = [];

    foreach ($R['columns'] ?? [] as $colName) {
        $colName = (string)$colName;
        if (isset($jsonSet[$colName])) {
            $cols[$colName] = [
                'type' => 'json',
                'nullable' => !in_array($colName, $required, true),
                'description' => "JSON field {$colName} (auto-discovered)",
            ];
            continue;
        }
        if (isset($refs[$colName])) {
            $cols[$colName] = [
                'type' => 'int',
                'nullable' => !in_array($colName, $required, true),
                'min' => 0,
                'ref' => (string)$refs[$colName],
                'description' => "Foreign key to {$refs[$colName]} (auto-discovered)",
            ];
            continue;
        }
        $lower = strtolower($colName);
        if (preg_match('/(^|_)(id|x|y|posx|posy|telex|teley|doorx|doory|level|power|number|respawn|radius|price|money|exp|step|multiple|validuntil|timelimit|health|currency)$/', $lower) === 1) {
            $cols[$colName] = [
                'type' => 'int',
                'nullable' => !in_array($colName, $required, true),
                'description' => "Integer column {$colName} (auto-discovered)",
            ];
            continue;
        }
        $cols[$colName] = [
            'type' => 'string',
            'nullable' => !in_array($colName, $required, true),
            'description' => "Column {$colName} (auto-discovered)",
        ];
    }

    return [
        'auto_discovered' => true,
        'required_on_create' => $required,
        'unique_keys' => [],
        'columns' => $cols,
    ];
}

function ai_schema_for_resource(string $resourceKey): ?array
{
    static $cache = [];
    if (array_key_exists($resourceKey, $cache)) {
        return $cache[$resourceKey];
    }
    $tables = ai_schema_tables();
    if (isset($tables[$resourceKey])) {
        return $cache[$resourceKey] = $tables[$resourceKey];
    }
    $R = editor_resources()[$resourceKey] ?? null;
    if ($R === null) {
        return $cache[$resourceKey] = null;
    }
    return $cache[$resourceKey] = ai_schema_auto_resource($resourceKey, $R);
}

/** Примеры корректных create/update_patch_json/copy операций для MRPG 0.6. */
function ai_schema_examples(): array
{
    static $cache = null;
    if ($cache !== null) {
        return $cache;
    }
    $cache = [
        'items' => [
            'create' => [
                'action' => 'create',
                'resource' => 'items',
                'data' => [
                    'Name' => 'Emerald Shard',
                    'Description' => 'Rare gem shard mined in deep caves.',
                    'Group' => 'Resource',
                    'Type' => 'Resource mineable',
                    'InitialPrice' => 120,
                    'Data' => ['bonus' => ['mining_fortune' => 2]],
                ],
            ],
            'update_patch_json' => [
                'action' => 'update',
                'resource' => 'items',
                'id' => 1,
                'patch_json' => ['Data' => ['bonus' => ['mining_fortune' => 4]]],
            ],
            'copy' => [
                'action' => 'copy',
                'resource' => 'items',
                'source_id' => 1,
                'data' => ['Name' => 'Pure Emerald Shard'],
            ],
        ],
        'scenarios' => [
            'create' => [
                'action' => 'create',
                'resource' => 'scenarios',
                'filename' => 'world_welcome_scene.json',
                'data' => [
                    'content' => [
                        'scenario_mode' => 'default',
                        'steps' => [
                            [
                                'id' => 'step_intro',
                                'components' => [
                                    ['type' => 'message', 'text' => 'Welcome to Green Valley!', 'delay' => 0, 'next_step_id' => 'step_reward'],
                                ],
                            ],
                            [
                                'id' => 'step_reward',
                                'components' => [
                                    ['type' => 'give_item', 'item_id' => 1, 'amount' => 1],
                                ],
                            ],
                        ],
                    ],
                ],
            ],
            'update_patch_json' => [
                'action' => 'update',
                'resource' => 'scenarios',
                'filename' => 'world_welcome_scene.json',
                'patch_json' => ['scenario_mode' => 'universal'],
            ],
        ],
        'quests' => [
            'create' => [
                'action' => 'create',
                'resource' => 'quests',
                'data' => ['Name' => 'Cave Supply', 'Money' => 250, 'Exp' => 150, 'Flags' => 'Type side'],
            ],
        ],
        'bots_npc' => [
            'create' => [
                'action' => 'create',
                'resource' => 'bots_npc',
                'data' => ['BotID' => 1, 'WorldID' => 0, 'PosX' => 640, 'PosY' => 480, 'Function' => 1, 'Static' => 1, 'Emote' => 'Happy'],
            ],
        ],
        'bots_mobs' => [
            'create' => [
                'action' => 'create',
                'resource' => 'bots_mobs',
                'data' => ['BotID' => 1, 'WorldID' => 0, 'PositionX' => 800, 'PositionY' => 640, 'Level' => 5, 'Power' => 25, 'Number' => 3],
            ],
        ],
        'crafts' => [
            'create' => [
                'action' => 'create',
                'resource' => 'crafts',
                'data' => ['GroupName' => '|Smithing|Gems', 'ItemID' => 1, 'ItemValue' => 1, 'RequiredItems' => '[1/3]', 'Price' => 50, 'WorldID' => 0],
            ],
        ],
        'warehouses' => [
            'create' => [
                'action' => 'create',
                'resource' => 'warehouses',
                'data' => ['Name' => 'Miner Shop', 'Type' => 'selling', 'PosX' => 600, 'PosY' => 480, 'WorldID' => 0, 'Currency' => 1, 'Trades' => "*Ores\n[1/1(50)]"],
            ],
        ],
        'nodes_collecting' => [
            'create' => [
                'action' => 'create',
                'resource' => 'nodes_collecting',
                'data' => ['SwitchID' => 12, 'Level' => 1, 'Health' => 4, 'Name' => 'Emerald Vein', 'Properties' => '[1/100]', 'Type' => 'ore', 'WorldID' => 0],
            ],
        ],
    ];
    return $cache;
}

function ai_schema_example_for(string $resourceKey, ?array $R = null, ?array $schema = null): array
{
    $examples = ai_schema_examples();
    if (isset($examples[$resourceKey])) {
        return $examples[$resourceKey];
    }
    $R ??= editor_resources()[$resourceKey] ?? null;
    if ($R === null) {
        return [];
    }
    $schema ??= ai_schema_for_resource($resourceKey) ?? ['required_on_create' => [], 'columns' => []];
    $sample = [];
    foreach ($schema['required_on_create'] ?? [] as $col) {
        $def = $schema['columns'][$col] ?? ['type' => 'string'];
        if (($def['type'] ?? '') === 'int' || ($def['type'] ?? '') === 'bool_int') {
            $sample[$col] = $def['default'] ?? ($def['min'] ?? 1);
        } elseif (($def['type'] ?? '') === 'enum' && !empty($def['enum'])) {
            $sample[$col] = $def['enum'][0];
        } elseif (($def['type'] ?? '') === 'set' && !empty($def['set'])) {
            $sample[$col] = $def['set'][0];
        } elseif (($def['type'] ?? '') === 'json') {
            $sample[$col] = [];
        } else {
            $sample[$col] = $def['default'] ?? ('Sample ' . $col);
        }
    }
    if ($sample === [] && !empty($R['label'])) {
        $sample[(string)$R['label']] = 'Sample ' . (string)$R['title'];
    }
    return [
        'create' => ['action' => 'create', 'resource' => $resourceKey, 'data' => $sample],
    ];
}

/**
 * Подбор ближайших допустимых значений (имена полей, ресурсы, enum/set)
 * для самовосстанавливающихся подсказок `did_you_mean` в ошибках AI API.
 *
 * @param array<int, string> $candidates
 * @return array<int, string>
 */
function ai_schema_suggest_closest(string $input, array $candidates, int $max = 3): array
{
    $needle = trim($input);
    if ($needle === '' || $candidates === []) {
        return [];
    }
    $needleLower = strtolower($needle);
    $needleCompact = preg_replace('/[^a-z0-9]+/', '', $needleLower) ?? $needleLower;

    // Частые MRPG-путаницы имён полей между таблицами (PositionX vs PosX vs TeleX vs DoorX и т.д.).
    $aliasGroups = [
        ['positionx', 'posx', 'telex', 'doorx', 'x'],
        ['positiony', 'posy', 'teley', 'doory', 'y'],
        ['worldid', 'world', 'mapid'],
        ['botid', 'bot'],
        ['itemid', 'item'],
        ['questid', 'quest', 'givequestid', 'nextquestid'],
        ['scenario', 'scenariodata'],
        ['dialog', 'dialogdata'],
        ['tasks', 'tasksdata'],
        ['storage', 'storagedata'],
    ];
    $aliasNeighbors = [];
    foreach ($aliasGroups as $group) {
        if (in_array($needleCompact, $group, true)) {
            foreach ($group as $g) {
                $aliasNeighbors[$g] = true;
            }
        }
    }

    $scored = [];
    foreach ($candidates as $cand) {
        $candStr = (string)$cand;
        if ($candStr === '') {
            continue;
        }
        $candLower = strtolower($candStr);
        $candCompact = preg_replace('/[^a-z0-9]+/', '', $candLower) ?? $candLower;
        $score = 0.0;

        if ($candLower === $needleLower || $candCompact === $needleCompact) {
            $score = 1.0;
        } elseif (isset($aliasNeighbors[$candCompact])) {
            $score = 0.92;
        } else {
            if (str_contains($candLower, $needleLower) || str_contains($needleLower, $candLower)) {
                $score = max($score, 0.78);
            }
            $maxLen = max(strlen($needleCompact), strlen($candCompact), 1);
            if ($maxLen <= 120) {
                $dist = levenshtein($needleCompact, $candCompact);
                $levScore = 1.0 - ($dist / $maxLen);
                if ($levScore > $score) {
                    $score = $levScore;
                }
            }
        }

        if ($score >= 0.45) {
            $scored[] = ['value' => $candStr, 'score' => $score];
        }
    }

    usort($scored, static function (array $a, array $b): int {
        if ($a['score'] !== $b['score']) {
            return $b['score'] <=> $a['score'];
        }
        return strcmp($a['value'], $b['value']);
    });

    return array_slice(array_column($scored, 'value'), 0, max(1, $max));
}

/**
 * Компактная шпаргалка (cookbook) по правилам и DSL Teeworlds-MRPG-0.6 для AI.
 * Публикуется в MCP resource `mrpg://cookbook`, в /llms.txt и при необходимости в manifest.
 */
function ai_schema_cookbook(): array
{
    static $cache = null;
    if ($cache !== null) {
        return $cache;
    }
    $cache = [
        'version' => '2.0',
        'coordinates' => [
            'unit' => 'world_pixels',
            'formula' => 'pixel = tile * 32 + 16 (tile center)',
            'columns_by_resource' => [
                'bots_mobs' => ['PositionX', 'PositionY'],
                'bots_npc' => ['PosX', 'PosY'],
                'quest_bots' => ['PosX', 'PosY'],
                'warehouses' => ['PosX', 'PosY'],
                'quest_boards' => ['PosX', 'PosY'],
                'aethers' => ['TeleX', 'TeleY'],
                'dungeons' => ['DoorX', 'DoorY'],
            ],
            'tip' => 'Always verify placement with analyze_map (point_check / nearest_standable_point) and visually confirm via get_map_screenshot before proposing NPCs, mobs, quest steps, shops, or teleports.',
        ],
        'symbolic_refs' => [
            'description' => 'Link newly created records inside one atomic Change Request without manually juggling numeric tempIds (2000000001..).',
            'declare' => 'Add "ref": "my_alias" (letters/digits/_/-) to any DB create/copy operation.',
            'reference' => 'Use "$my_alias" in FK columns ("ItemID": "$my_sword", "BotID": "$npc_bot"), inside MRPG DSL strings ("RequiredItems": "[$my_ore/3]", "Trades": "*Shop\\n[$my_sword/1(100)]"), or inside JSON fields (TasksData, ScenarioData, Data, scenarios).',
            'compatibility' => 'Explicit numeric tempId (2000000000..2000999999) remains fully supported.',
        ],
        'dsl_cheatsheet' => [
            'crafts.RequiredItems' => 'Comma-separated [ItemID/Qty] pairs, e.g. "[12/3],[45/1]" or "[$ore/5]"',
            'crafts.GroupName' => 'Pipe-prefixed hierarchy: "|Smithing|Weapons" or "|Alchemy|Potions:HP"',
            'warehouses.Trades' => "Multiline groups starting with *GroupName and entries [ItemID/Qty(Price)] or [ItemID/Qty(Price/AltItemID)], e.g.:\n*Weapons\n[12/1(500)]",
            'nodes_collecting.Properties' => 'One or more [ItemID/Chance] or [ItemID/Qty/Chance] entries, e.g. "[14/100] [15/2/12.5]"',
            'bots_mobs.it_drop_count' => '5 pipe-delimited integers: "|1|2|0|0|0|"',
            'bots_mobs.it_drop_chance' => '5 pipe-delimited floats: "|100|15.5|0|0|0|"',
        ],
        'filtering_and_sorting' => [
            'exact' => '{"WorldID": 0, "Group": "Equipment"}',
            'in_list' => '{"Group": ["Equipment", "Potion"]}',
            'operators' => '{"Level": {"gte": 5, "lte": 15}, "Name": {"contains": "Iron"}, "NextQuestID": {"not_null": true}}',
            'supported_operators' => ['eq', 'ne', 'gt', 'gte', 'lt', 'lte', 'contains', 'prefix', 'like', 'in', 'is_null', 'not_null'],
            'sort' => 'sort="ID:desc" or sort="-Level,Name:asc"',
        ],
        'safe_editing_rules' => [
            '1. Orient quickly using search_editor (cross-resource search) and get_context (complete world/quest/item/bot snapshot).',
            '2. Check duplicates before creating: find_similar (GET action=similar).',
            '3. Read multiple IDs at once via get_record with ids=[1,2,3] (GET action=get&ids=1,2,3).',
            '4. Prefer patch_json (RFC 7396 merge patch) over full replacement; verify read-only via apply_patch before proposing.',
            '5. Check scenario structure & missing deps via analyze_scenario; inspect maps & buildings via analyze_map + get_map_screenshot.',
            '6. Submit via validate -> preview -> propose (with confirm_dangerous=true only after reviewing dangerous_actions).',
            '7. Track review feedback, reply to admins, revise, or withdraw pending drafts via manage_change_request (REST action=request / request_reply / request_withdraw).',
        ],
    ];
    return $cache;
}

