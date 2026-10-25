<?php
// Shared resource registry + CRUD helpers for DB editors.
// Used by api/db-crud.php (direct admin CRUD) and api/change-requests.php
// (guest change requests: validation, impact analysis and admin approval).
//
// IMPORTANT: only whitelisted tables/columns are accessible.

declare(strict_types=1);

/**
 * Temporary IDs used by guest drafts for records that do not exist yet.
 * They are replaced by real AUTO_INCREMENT ids when an admin applies a request.
 */
const GUEST_TEMP_ID_BASE = 2000000000;
const GUEST_TEMP_ID_MAX  = 2000999999;

function is_guest_temp_id(mixed $value): bool
{
    if (is_int($value)) {
        $n = $value;
    } elseif (is_string($value) && preg_match('/^\d{10}$/', $value) === 1) {
        $n = (int)$value;
    } elseif (is_float($value) && floor($value) === $value) {
        $n = (int)$value;
    } else {
        return false;
    }
    return $n >= GUEST_TEMP_ID_BASE && $n <= GUEST_TEMP_ID_MAX;
}

/**
 * Resource map.
 *  table/pk/columns/search/json/order — CRUD whitelist (as before)
 *  title  — human readable name (used in change requests)
 *  label  — column used to describe a row ("#12 «Iron Sword»")
 *  refs   — FK-like columns: column => referenced table
 *  guest  — readable by the guest account and allowed in change requests
 */
function editor_resources(): array
{
    static $resources = null;
    if ($resources !== null) {
        return $resources;
    }
    $resources = [
        'vouchers' => [
            'title' => 'Ваучеры',
            'table' => 'tw_voucher',
            'pk' => 'ID',
            'columns' => ['Code', 'Data', 'Multiple', 'ValidUntil'],
            'search' => ['Code', 'ID'],
            'json' => ['Data'],
            'order' => 'ID DESC',
            'label' => 'Code',
            // Voucher codes are secrets (they grant rewards) — never exposed to guests.
            'guest' => false,
        ],
        'bots_mobs' => [
            'title' => 'Мобы',
            'table' => 'tw_bots_mobs',
            'pk' => 'ID',
            'columns' => [
                'BotID','WorldID','PositionX','PositionY','Debuffs','Behavior','Level','Power','Number','Respawn','Radius','ActiveRadius','Boss',
                'it_drop_0','it_drop_1','it_drop_2','it_drop_3','it_drop_4','it_drop_count','it_drop_chance'
            ],
            'search' => ['ID','BotID'],
            'order' => 'ID DESC',
            'label' => null,
            'refs' => [
                'BotID' => 'tw_bots_info', 'WorldID' => 'tw_worlds',
                'it_drop_0' => 'tw_items_list', 'it_drop_1' => 'tw_items_list', 'it_drop_2' => 'tw_items_list',
                'it_drop_3' => 'tw_items_list', 'it_drop_4' => 'tw_items_list',
            ],
            'guest' => true,
        ],
        'bots_info' => [
            'title' => 'Боты',
            'table' => 'tw_bots_info',
            'pk' => 'ID',
            'columns' => ['Name','JsonTeeInfo','EquippedModules','SlotHammer','SlotGun','SlotShotgun','SlotGrenade','SlotRifle','SlotArmor'],
            'search' => ['ID', 'Name'],
            'order' => 'ID ASC',
            'label' => 'Name',
            'refs' => [
                'SlotHammer' => 'tw_items_list', 'SlotGun' => 'tw_items_list', 'SlotShotgun' => 'tw_items_list',
                'SlotGrenade' => 'tw_items_list', 'SlotRifle' => 'tw_items_list', 'SlotArmor' => 'tw_items_list',
            ],
            'guest' => true,
        ],
        'bots_npc' => [
            'title' => 'NPC',
            'table' => 'tw_bots_npc',
            'pk' => 'ID',
            'columns' => ['BotID','PosX','PosY','GiveQuestID','DialogData','Function','Static','Emote','WorldID'],
            'search' => ['ID', 'BotID', 'WorldID'],
            'json' => ['DialogData'],
            'order' => 'ID ASC',
            'label' => null,
            'refs' => ['BotID' => 'tw_bots_info', 'GiveQuestID' => 'tw_quests_list', 'WorldID' => 'tw_worlds'],
            'guest' => true,
        ],
        'crafts' => [
            'title' => 'Крафты',
            'table' => 'tw_crafts_list',
            'pk' => 'ID',
            'columns' => ['GroupName','ItemID','ItemValue','RequiredItems','Price','WorldID'],
            'search' => ['GroupName','ID','ItemID'],
            'order' => 'ID ASC',
            'label' => 'GroupName',
            'refs' => ['ItemID' => 'tw_items_list', 'WorldID' => 'tw_worlds'],
            'guest' => true,
        ],
        'warehouses' => [
            'title' => 'Склады (Warehouse)',
            'table' => 'tw_warehouses',
            'pk' => 'ID',
            'columns' => ['Name','Type','Trades','PosX','PosY','StorageData','Currency','WorldID'],
            'search' => ['ID','Name','Type','WorldID','Currency'],
            'order' => 'ID ASC',
            'label' => 'Name',
            'refs' => ['Currency' => 'tw_items_list', 'WorldID' => 'tw_worlds'],
            'guest' => true,
        ],
        'worlds' => [
            'title' => 'Миры',
            'table' => 'tw_worlds',
            'pk' => 'ID',
            'columns' => ['Name', 'Path', 'Type', 'Flags', 'RespawnWorldID', 'JailWorldID', 'RequiredLevel'],
            'search' => ['ID', 'Name', 'Path'],
            'order' => 'ID ASC',
            'label' => 'Name',
            'refs' => ['RespawnWorldID' => 'tw_worlds', 'JailWorldID' => 'tw_worlds'],
            'guest' => true,
        ],
        'nodes_collecting' => [
            'title' => 'Узлы ресурсов',
            'table' => 'tw_nodes_collecting',
            'pk' => 'ID',
            'columns' => ['SwitchID', 'Level', 'Health', 'Name', 'Properties', 'Type', 'WorldID'],
            'search' => ['ID', 'Name', 'Type', 'SwitchID', 'WorldID'],
            'order' => 'WorldID ASC, Type ASC, SwitchID ASC',
            'label' => 'Name',
            'refs' => ['WorldID' => 'tw_worlds'],
            'guest' => true,
        ],
        'aethers' => [
            'title' => 'Телепорты (Aether)',
            'table' => 'tw_aethers',
            'pk' => 'ID',
            'columns' => ['Name', 'WorldID', 'TeleX', 'TeleY'],
            'search' => ['ID', 'Name', 'WorldID'],
            'order' => 'ID ASC',
            'label' => 'Name',
            'refs' => ['WorldID' => 'tw_worlds'],
            'guest' => true,
        ],
        'items' => [
            'title' => 'Предметы',
            'table' => 'tw_items_list',
            'pk' => 'ID',
            'columns' => [
                'Comment','Name','Description','Group','Type','Flags','ScenarioMode','ScenarioData','InitialPrice',
                'RequiresProducts','AT1','AT2','ATValue1','ATValue2','Data',
            ],
            'search' => ['ID', 'Name', 'Description', 'Group', 'Type'],
            'json' => ['Data'],
            'order' => 'ID ASC',
            'label' => 'Name',
            'refs' => ['AT1' => 'tw_attributes', 'AT2' => 'tw_attributes'],
            'guest' => true,
        ],
        'dungeons' => [
            'title' => 'Подземелья',
            'table' => 'tw_dungeons',
            'pk' => 'ID',
            'columns' => ['Level', 'DoorX', 'DoorY', 'Scenario', 'WorldID', 'TimeLimit'],
            'search' => ['ID', 'Level', 'WorldID'],
            'order' => 'ID ASC',
            'label' => null,
            'refs' => ['WorldID' => 'tw_worlds'],
            'guest' => true,
        ],
        'quests' => [
            'title' => 'Квесты',
            'table' => 'tw_quests_list',
            'pk' => 'ID',
            'columns' => ['NextQuestID', 'Name', 'Money', 'Exp', 'Flags'],
            'search' => ['ID', 'Name', 'NextQuestID'],
            'order' => 'ID ASC',
            'label' => 'Name',
            'refs' => ['NextQuestID' => 'tw_quests_list'],
            'guest' => true,
        ],
        'quest_bots' => [
            'title' => 'Шаги квестов',
            'table' => 'tw_bots_quest',
            'pk' => 'ID',
            'columns' => ['BotID', 'QuestID', 'Step', 'WorldID', 'PosX', 'PosY', 'AutoFinish', 'DialogData', 'ScenarioData', 'TasksData'],
            'search' => ['ID', 'QuestID', 'BotID', 'Step'],
            'json' => ['DialogData', 'TasksData'],
            'order' => 'ID ASC',
            'label' => null,
            'refs' => ['BotID' => 'tw_bots_info', 'QuestID' => 'tw_quests_list', 'WorldID' => 'tw_worlds'],
            'guest' => true,
        ],
    ];
    return $resources;
}

/** Human readable info about tables (also tables not editable in Studio). */
const EDITOR_TABLE_META = [
    'tw_items_list'         => ['title' => 'Предмет',            'label' => 'Name'],
    'tw_worlds'             => ['title' => 'Мир',                'label' => 'Name'],
    'tw_bots_info'          => ['title' => 'Бот',                'label' => 'Name'],
    'tw_quests_list'        => ['title' => 'Квест',              'label' => 'Name'],
    'tw_attributes'         => ['title' => 'Атрибут',            'label' => 'Name'],
    'tw_skills_list'        => ['title' => 'Скилл',              'label' => 'Name'],
    'tw_nodes_collecting'   => ['title' => 'Узел ресурсов', 'label' => 'Name'],
    'tw_aethers'            => ['title' => 'Aether',             'label' => 'Name'],
    'tw_dungeons'           => ['title' => 'Подземелье',         'label' => null],
    'tw_bots_mobs'          => ['title' => 'Моб',                'label' => null],
    'tw_bots_npc'           => ['title' => 'NPC',                'label' => null],
    'tw_bots_quest'         => ['title' => 'Шаг квеста',         'label' => null],
    'tw_crafts_list'        => ['title' => 'Крафт',              'label' => 'GroupName'],
    'tw_warehouses'         => ['title' => 'Склад',              'label' => 'Name'],
    'tw_voucher'            => ['title' => 'Ваучер',             'label' => null],
    'tw_accounts_items'     => ['title' => 'Предметы в инвентарях игроков', 'label' => null],
    'tw_accounts_quests'    => ['title' => 'Прогресс квестов игроков',      'label' => null],
    'tw_accounts_aethers'   => ['title' => 'Открытые игроками Aether',      'label' => null],
    'tw_accounts_data'      => ['title' => 'Данные аккаунтов игроков',      'label' => null],
    'tw_auction_slots'      => ['title' => 'Лоты аукциона',      'label' => null],
    'tw_guilds_decorations' => ['title' => 'Декорации гильдий',  'label' => null],
    'tw_guilds_houses'      => ['title' => 'Дома гильдий',       'label' => null],
    'tw_houses'             => ['title' => 'Дома игроков',       'label' => null],
    'tw_dungeons_door'      => ['title' => 'Двери подземелий',   'label' => null],
    'tw_dungeons_records'   => ['title' => 'Рекорды подземелий', 'label' => null],
];

function editor_table_title(string $table): string
{
    return EDITOR_TABLE_META[$table]['title'] ?? $table;
}

/**
 * Who references a table (from mmprpg schema). action:
 *   cascade  — rows are deleted together with the referenced row
 *   set_null — the column becomes NULL
 *   none     — no FK: the reference becomes dangling (server may misbehave)
 */
function editor_reverse_refs(string $table): array
{
    static $map = [
        'tw_items_list' => [
            ['table' => 'tw_accounts_items',     'column' => 'ItemID',      'action' => 'cascade'],
            ['table' => 'tw_auction_slots',      'column' => 'ItemID',      'action' => 'cascade'],
            ['table' => 'tw_bots_info',          'column' => 'SlotHammer',  'action' => 'cascade'],
            ['table' => 'tw_bots_info',          'column' => 'SlotGun',     'action' => 'cascade'],
            ['table' => 'tw_bots_info',          'column' => 'SlotShotgun', 'action' => 'cascade'],
            ['table' => 'tw_bots_info',          'column' => 'SlotGrenade', 'action' => 'cascade'],
            ['table' => 'tw_bots_info',          'column' => 'SlotRifle',   'action' => 'cascade'],
            ['table' => 'tw_bots_info',          'column' => 'SlotArmor',   'action' => 'cascade'],
            ['table' => 'tw_bots_mobs',          'column' => 'it_drop_0',   'action' => 'set_null'],
            ['table' => 'tw_bots_mobs',          'column' => 'it_drop_1',   'action' => 'set_null'],
            ['table' => 'tw_bots_mobs',          'column' => 'it_drop_2',   'action' => 'set_null'],
            ['table' => 'tw_bots_mobs',          'column' => 'it_drop_3',   'action' => 'set_null'],
            ['table' => 'tw_bots_mobs',          'column' => 'it_drop_4',   'action' => 'set_null'],
            ['table' => 'tw_crafts_list',        'column' => 'ItemID',      'action' => 'cascade'],
            ['table' => 'tw_guilds_decorations', 'column' => 'ItemID',      'action' => 'cascade'],
            ['table' => 'tw_warehouses',         'column' => 'Currency',    'action' => 'cascade'],
        ],
        'tw_worlds' => [
            ['table' => 'tw_accounts_data',      'column' => 'WorldID',        'action' => 'set_null'],
            ['table' => 'tw_nodes_collecting',   'column' => 'WorldID', 'action' => 'none'],
            ['table' => 'tw_aethers',            'column' => 'WorldID',        'action' => 'cascade'],
            ['table' => 'tw_bots_mobs',          'column' => 'WorldID',        'action' => 'set_null'],
            ['table' => 'tw_bots_npc',           'column' => 'WorldID',        'action' => 'set_null'],
            ['table' => 'tw_bots_quest',         'column' => 'WorldID',        'action' => 'set_null'],
            ['table' => 'tw_crafts_list',        'column' => 'WorldID',        'action' => 'cascade'],
            ['table' => 'tw_guilds_decorations', 'column' => 'WorldID',        'action' => 'cascade'],
            ['table' => 'tw_guilds_houses',      'column' => 'WorldID',        'action' => 'cascade'],
            ['table' => 'tw_houses',             'column' => 'WorldID',        'action' => 'cascade'],
            ['table' => 'tw_dungeons',           'column' => 'WorldID',        'action' => 'none'],
            ['table' => 'tw_warehouses',         'column' => 'WorldID',        'action' => 'none'],
            ['table' => 'tw_worlds',             'column' => 'RespawnWorldID', 'action' => 'none'],
            ['table' => 'tw_worlds',             'column' => 'JailWorldID',    'action' => 'none'],
        ],
        'tw_bots_info' => [
            ['table' => 'tw_bots_mobs',     'column' => 'BotID', 'action' => 'cascade'],
            ['table' => 'tw_bots_npc',      'column' => 'BotID', 'action' => 'cascade'],
            ['table' => 'tw_bots_quest',    'column' => 'BotID', 'action' => 'cascade'],
            ['table' => 'tw_dungeons_door', 'column' => 'BotID', 'action' => 'cascade'],
        ],
        'tw_quests_list' => [
            ['table' => 'tw_accounts_quests', 'column' => 'QuestID',     'action' => 'cascade'],
            ['table' => 'tw_bots_npc',        'column' => 'GiveQuestID', 'action' => 'cascade'],
            ['table' => 'tw_bots_quest',      'column' => 'QuestID',     'action' => 'cascade'],
            ['table' => 'tw_quests_list',     'column' => 'NextQuestID', 'action' => 'none'],
        ],
        'tw_aethers' => [
            ['table' => 'tw_accounts_aethers', 'column' => 'AetherID', 'action' => 'cascade'],
        ],
        'tw_dungeons' => [
            ['table' => 'tw_dungeons_door',    'column' => 'DungeonID', 'action' => 'cascade'],
            ['table' => 'tw_dungeons_records', 'column' => 'DungeonID', 'action' => 'cascade'],
        ],
    ];
    return $map[$table] ?? [];
}

function editor_identifier(string $name): string
{
    if ($name === '' || preg_match('/^[a-zA-Z_][a-zA-Z0-9_]*$/', $name) !== 1) {
        throw new RuntimeException('Invalid identifier: ' . $name);
    }
    return $name;
}

// ── JSON / binding / crafts helpers (shared with db-crud.php) ──────────────

function decode_json_cols(array $row, array $jsonCols): array {
  foreach ($jsonCols as $col) {
    if (!array_key_exists($col, $row)) continue;
    $val = $row[$col];
    if ($val == null) continue;
    if (is_array($val) || is_object($val)) continue;
    $s = (string)$val;
    $trim = trim($s);
    if ($trim === '') continue;
    $decoded = json_decode($s, true);
    if (json_last_error() === JSON_ERROR_NONE) {
      $row[$col] = $decoded;
    }
  }
  return $row;
}

function encode_json_cols(array $data, array $jsonCols): array {
  foreach ($jsonCols as $col) {
    if (!array_key_exists($col, $data)) continue;
    $v = $data[$col];
    if (is_array($v) || is_object($v)) {
      $data[$col] = json_encode($v, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
    }
  }
  return $data;
}

function sanitize_columns(array $data, array $allowed): array {
  $out = [];
  foreach ($allowed as $col) {
    if (array_key_exists($col, $data)) $out[$col] = $data[$col];
  }
  return $out;
}

// ── Сравнение значений «было → станет» ──────────────────────────────────────
//
// Формы редакторов не различают NULL и пустое поле: при чтении записи NULL
// приводится к '' (текст) или 0 (число), при сохранении эти значения уходят
// обратно в payload. Если сравнивать значения «в лоб», то:
//   * открытие записи и нажатие «Сохранить» без правок создаёт запрос с
//     изменениями `NULL → пусто` и `NULL → 0`;
//   * одобрение такого запроса записывает '' / 0 на место NULL в игровой БД;
//   * возврат поля к исходному значению не убирает операцию из черновика.
//
// Поэтому NULL, '', 0 и false считаются одним и тем же «нет значения».

/** Значение не несёт данных: NULL, '', 0, false, пустые массивы/объекты. */
function editor_value_is_empty(mixed $v): bool
{
    if ($v === null) return true;
    if (is_bool($v)) return $v === false;
    if (is_int($v) || is_float($v)) return (float)$v === 0.0;
    if (is_string($v)) {
        $t = trim($v);
        if ($t === '') return true;
        if (preg_match('/^-?\d+(\.\d+)?$/', $t) === 1) return (float)$t === 0.0;
        if ($t === '[]' || $t === '{}') return true;
        return false;
    }
    if (is_array($v)) return $v === [];
    return false;
}

/** Значение JSON-колонки в виде структуры (или исходная строка). */
function editor_decode_json_value(mixed $v): mixed
{
    if (!is_string($v)) return $v;
    $t = trim($v);
    if ($t === '' || ($t[0] !== '{' && $t[0] !== '[')) return $v;
    $d = json_decode($t, true);
    return json_last_error() === JSON_ERROR_NONE ? $d : $v;
}

/** Каноническая строка значения: 7, '7' и '7.00' совпадают, JSON — как структура. */
function editor_value_canonical(mixed $v, bool $isJson = false): ?string
{
    if ($v === null) return null;
    if ($isJson) {
        $d = editor_decode_json_value($v);
        if (is_array($d)) {
            $j = json_encode($d, JSON_UNESCAPED_UNICODE);
            return $j === false ? null : $j;
        }
    }
    if (is_bool($v)) return $v ? '1' : '0';
    if (is_float($v) || is_int($v)) {
        return rtrim(rtrim(number_format((float)$v, 10, '.', ''), '0'), '.') ?: '0';
    }
    if (is_array($v)) {
        $j = json_encode($v, JSON_UNESCAPED_UNICODE);
        return $j === false ? null : $j;
    }
    $s = (string)$v;
    if (is_numeric($s) && preg_match('/^-?\d+(\.\d+)?$/', $s) === 1) {
        return rtrim(rtrim(number_format((float)$s, 10, '.', ''), '0'), '.') ?: '0';
    }
    return $s;
}

/**
 * Одинаково ли значение в БД и в присланных данных.
 * NULL и пустая строка/ноль равны: редактор не может их различить.
 */
function editor_values_equal(mixed $a, mixed $b, bool $isJson = false): bool
{
    if (editor_value_is_empty($a) && editor_value_is_empty($b)) return true;
    return editor_value_canonical($a, $isJson) === editor_value_canonical($b, $isJson);
}

function bind_type_for_value(mixed $value): string {
  if ($value === null) return 's';
  if (is_int($value)) return 'i';
  if (is_float($value)) return 'd';
  if (is_bool($value)) return 'i';
  if (is_string($value) && preg_match('/^-?\d+$/', $value) === 1) return 'i';
  if (is_string($value) && is_numeric($value)) return 'd';
  return 's';
}

function normalize_bind_value(mixed $value, string $type): mixed {
  if ($value === null) return null;
  if ($type === 'i') return (int)$value;
  if ($type === 'd') return (float)$value;
  return (string)$value;
}

function parse_craft_group_name(string $value): array {
  $raw = trim($value);
  $main = '';
  $nested = $raw;

  if (str_starts_with($raw, '|')) {
    $end = strpos($raw, '|', 1);
    if ($end !== false) {
      $main = trim(substr($raw, 1, $end - 1));
      $nested = substr($raw, $end + 1);
    } else {
      $nested = substr($raw, 1);
    }
  }

  $separator = strpos($nested, ':');
  if ($separator === false) {
    return ['main' => $main, 'group' => trim($nested), 'sub' => ''];
  }

  return [
    'main' => $main,
    'group' => trim(substr($nested, 0, $separator)),
    'sub' => trim(substr($nested, $separator + 1)),
  ];
}

function join_craft_group_name(string $main, string $group, string $sub): string {
  $main = trim($main);
  $group = trim($group);
  $sub = trim($sub);
  $nested = $group !== '' && $sub !== '' ? "$group:$sub" : ($group !== '' ? $group : $sub);

  if ($main === '') return $nested;
  return $nested === '' ? "|$main|" : "|$main|$nested";
}

function craft_text_length(string $value): int {
  if (function_exists('mb_strlen')) return mb_strlen($value, 'UTF-8');
  $matches = [];
  $count = preg_match_all('/./us', $value, $matches);
  return $count === false ? strlen($value) : $count;
}

function fetch_craft_group_rows(mysqli $mysqli, bool $forUpdate = false): array {
  $lockSql = $forUpdate ? ' FOR UPDATE' : '';
  $stmt = $mysqli->prepare("SELECT `ID`, `GroupName` FROM `tw_crafts_list` ORDER BY `ID` ASC$lockSql");
  if ($stmt === false) throw new RuntimeException('Ошибка подготовки запроса к базе');
  $stmt->execute();
  $res = $stmt->get_result();
  $rows = [];
  while ($row = $res->fetch_assoc()) $rows[] = $row;
  $stmt->close();
  return $rows;
}

function craft_rename_parent_matches(array $parts, string $level, string $main, string $group): bool {
  if ($level === 'main') return true;
  return $parts['main'] === $main && ($level === 'group' || $parts['group'] === $group);
}

function craft_rename_matches(array $parts, string $level, string $main, string $group, string $sub): bool {
  if ($level === 'main') return $parts['main'] === $main;
  if ($parts['main'] !== $main || $parts['group'] !== $group) return false;
  return $level === 'group' || $parts['sub'] === $sub;
}

function craft_rename_name(array $parts, string $level): string {
  return $level === 'main' ? $parts['main'] : ($level === 'group' ? $parts['group'] : $parts['sub']);
}

function craft_renamed_group_name(array $parts, string $level, string $newName): string {
  if ($level === 'main') return join_craft_group_name($newName, $parts['group'], $parts['sub']);
  if ($level === 'group') return join_craft_group_name($parts['main'], $newName, $parts['sub']);
  return join_craft_group_name($parts['main'], $parts['group'], $newName);
}

// ── Generic row helpers (prepared statements only) ───────────────────────────

function crud_column_sql(array $R): string
{
    return '`' . $R['pk'] . '`, ' . implode(', ', array_map(fn($c) => '`' . $c . '`', $R['columns']));
}

function crud_fetch_row(mysqli $mysqli, array $R, int $id, bool $forUpdate = false): ?array
{
    $sql = 'SELECT ' . crud_column_sql($R) . ' FROM `' . $R['table'] . '` WHERE `' . $R['pk'] . '` = ? LIMIT 1'
        . ($forUpdate ? ' FOR UPDATE' : '');
    $stmt = $mysqli->prepare($sql);
    if ($stmt === false) throw new RuntimeException('Ошибка подготовки запроса к базе: ' . $mysqli->error);
    $stmt->bind_param('i', $id);
    $stmt->execute();
    $res = $stmt->get_result();
    $row = $res ? $res->fetch_assoc() : null;
    $stmt->close();
    return $row ?: null;
}

function crud_bind_values(array $data): array
{
    $types = '';
    $vals = [];
    foreach ($data as $v) {
        $t = bind_type_for_value($v);
        $types .= $t;
        $vals[] = normalize_bind_value($v, $t);
    }
    return [$types, $vals];
}

/** Validate collecting definitions on direct CRUD, approval and sync writes. */
function validate_collecting_node(array $data): void
{
    foreach (['SwitchID' => [1, 255], 'Level' => [1, 2147483647], 'Health' => [1, 2147483647], 'WorldID' => [0, 2147483647]] as $key => [$min, $max]) {
        $value = $data[$key] ?? null;
        if (!is_scalar($value) || !preg_match('/^\d+$/D', (string)$value) || (float)$value < $min || (float)$value > $max) {
            throw new RuntimeException("Некорректное поле узла: $key");
        }
    }
    if (!in_array($data['Type'] ?? null, ['ore', 'plant', 'fish'], true)) {
        throw new RuntimeException('Тип узла должен быть ore, plant или fish');
    }
    $name = $data['Name'] ?? null;
    if (!is_string($name) || trim($name) === '' || preg_match('/^.{1,128}$/usD', $name) !== 1) {
        throw new RuntimeException('Название узла: от 1 до 128 символов');
    }
    $properties = $data['Properties'] ?? null;
    if (!is_string($properties) || trim($properties) === '') {
        throw new RuntimeException('Укажите список добычи в Properties');
    }
    $seen = [];
    foreach (explode(',', $properties) as $entry) {
        if (!preg_match('/^\[(\d+)\/(?:(\d+)\/)?(\d+(?:\.\d+)?(?:e[+-]?\d+)?)\]$/iD', trim($entry), $m)) {
            throw new RuntimeException('Properties: ожидается [item/chance] или [item/amount/chance]');
        }
        $item = (int)$m[1];
        $amount = $m[2] === '' ? null : (float)$m[2];
        $chance = (float)$m[3];
        if ($item < 1 || $item > 2147483647 || isset($seen[$item]) ||
            ($amount !== null && ($amount < 1 || $amount > 1000000)) || !is_finite($chance) || $chance <= 0 || $chance > 100) {
            throw new RuntimeException('Properties: проверьте предметы, количества и шансы (0 < шанс <= 100); предметы не должны повторяться');
        }
        $seen[$item] = true;
    }
}

/** $data must be sanitized + JSON-encoded already. Returns the new id. */
function crud_insert(mysqli $mysqli, array $R, array $data): int
{
    if (!$data) throw new RuntimeException('Пустые данные: нечего сохранять');
    if ($R['table'] === 'tw_nodes_collecting') validate_collecting_node($data);
    $keys = array_keys($data);
    $placeholders = implode(',', array_fill(0, count($keys), '?'));
    $colSql = implode(',', array_map(fn($c) => '`' . $c . '`', $keys));
    $stmt = $mysqli->prepare('INSERT INTO `' . $R['table'] . "` ($colSql) VALUES ($placeholders)");
    if ($stmt === false) throw new RuntimeException('Ошибка подготовки запроса к базе: ' . $mysqli->error);
    [$types, $vals] = crud_bind_values($data);
    $stmt->bind_param($types, ...$vals);
    if (!$stmt->execute()) {
        $err = $stmt->error;
        $stmt->close();
        throw new RuntimeException('Не удалось добавить запись: ' . $err);
    }
    $newId = (int)$mysqli->insert_id;
    $stmt->close();
    return $newId;
}

function crud_update(mysqli $mysqli, array $R, int $id, array $data): int
{
    if (!$data) throw new RuntimeException('Пустые данные: нечего сохранять');
    if ($R['table'] === 'tw_nodes_collecting') {
        $current = crud_fetch_row($mysqli, $R, $id);
        if (!$current) throw new RuntimeException('Узел ресурсов не найден');
        validate_collecting_node(array_replace($current, $data));
    }
    $keys = array_keys($data);
    $setSql = implode(', ', array_map(fn($c) => '`' . $c . '` = ?', $keys));
    $stmt = $mysqli->prepare('UPDATE `' . $R['table'] . "` SET $setSql WHERE `" . $R['pk'] . '` = ? LIMIT 1');
    if ($stmt === false) throw new RuntimeException('Ошибка подготовки запроса к базе: ' . $mysqli->error);
    [$types, $vals] = crud_bind_values($data);
    $types .= 'i';
    $vals[] = $id;
    $stmt->bind_param($types, ...$vals);
    if (!$stmt->execute()) {
        $err = $stmt->error;
        $stmt->close();
        throw new RuntimeException('Не удалось сохранить изменения: ' . $err);
    }
    $affected = $stmt->affected_rows;
    $stmt->close();
    return $affected;
}

function crud_delete(mysqli $mysqli, array $R, int $id): int
{
    $stmt = $mysqli->prepare('DELETE FROM `' . $R['table'] . '` WHERE `' . $R['pk'] . '` = ? LIMIT 1');
    if ($stmt === false) throw new RuntimeException('Ошибка подготовки запроса к базе: ' . $mysqli->error);
    $stmt->bind_param('i', $id);
    if (!$stmt->execute()) {
        $err = $stmt->error;
        $stmt->close();
        throw new RuntimeException('Не удалось удалить запись: ' . $err);
    }
    $affected = $stmt->affected_rows;
    $stmt->close();
    return $affected;
}
