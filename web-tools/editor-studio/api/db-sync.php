<?php
// Cross-connection sync API: schema comparison & safe apply of recorded operations.
// Endpoint: api/db-sync.php
// Actions:
//  - compare_schema   (POST) : compare source/target table structures
//  - apply_operations (POST) : safely replay recorded CRUD operations on target DB
//  - get_target       (GET)  : get configured target profile (safe — no passwords)
//  - save_target      (POST) : persist target profile
//  - list_profiles    (GET)  : list available DB profiles (safe — no passwords)

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';

bootstrap_editor_api(true);

// ── Target profiles storage ─────────────────────────────────────────────────

function sync_cfg_path(): string
{
    return dirname(__DIR__) . '/data/db-sync-config.json';
}

function load_sync_cfg(): array
{
    $path = sync_cfg_path();
    if (!is_file($path)) {
        return ['target_profile_id' => '', 'profiles' => []];
    }
    $raw = file_get_contents($path);
    if ($raw === false) {
        return ['target_profile_id' => '', 'profiles' => []];
    }
    $cfg = json_decode($raw, true);
    return is_array($cfg) ? $cfg : ['target_profile_id' => '', 'profiles' => []];
}

function save_sync_cfg(array $cfg): bool
{
    $path = sync_cfg_path();
    $dir  = dirname($path);
    if (!is_dir($dir) && !@mkdir($dir, 0775, true) && !is_dir($dir)) {
        return false;
    }
    
    // Validate config structure
    if (!isset($cfg['profiles']) || !is_array($cfg['profiles'])) {
        $cfg['profiles'] = [];
    }
    if (!isset($cfg['target_profile_id'])) {
        $cfg['target_profile_id'] = '';
    }
    
    // Sanitize profile data
    foreach ($cfg['profiles'] as &$profile) {
        if (!is_array($profile)) continue;
        $profile['id'] = trim((string)($profile['id'] ?? ''));
        $profile['name'] = trim((string)($profile['name'] ?? ''));
        $profile['host'] = trim((string)($profile['host'] ?? ''));
        $profile['port'] = max(1, min(65535, (int)($profile['port'] ?? 3306)));
        $profile['database'] = trim((string)($profile['database'] ?? ''));
        $profile['user'] = trim((string)($profile['user'] ?? ''));
        // password is kept as-is (already validated on save)
    }
    unset($profile);
    
    $json = json_encode($cfg, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT | JSON_THROW_ON_ERROR);
    return file_put_contents($path, $json, LOCK_EX) !== false;
}

// ── Connect to an arbitrary profile ─────────────────────────────────────────

function db_connect_profile(array $profile): mysqli
{
    $host = (string)($profile['host']     ?? '127.0.0.1');
    $port = (int)($profile['port']        ?? 3306);
    $user = (string)($profile['user']     ?? 'root');
    $pass = (string)($profile['password'] ?? '');
    $db   = (string)($profile['database'] ?? '');

    mysqli_report(MYSQLI_REPORT_OFF);
    $mysqli = @new mysqli($host, $user, $pass, '', $port);
    if ($mysqli->connect_errno) {
        throw new RuntimeException('Target DB connection failed: ' . $mysqli->connect_error);
    }
    $mysqli->set_charset('utf8mb4');
    if ($db !== '' && !$mysqli->select_db($db)) {
        $err = $mysqli->error;
        $mysqli->close();
        throw new RuntimeException('Target DB select failed: ' . $err);
    }
    return $mysqli;
}

// ── Schema comparison ───────────────────────────────────────────────────────

/**
 * Fetch table schema as normalized column definitions.
 * Returns [table_name => [column_name => ['type'=>..., 'nullable'=>..., 'default'=>..., 'key'=>...], ...]]
 */
function fetch_schema(mysqli $mysqli): array
{
    $schema = [];

    $tablesRes = $mysqli->query("SHOW FULL TABLES WHERE Table_type = 'BASE TABLE'");
    if (!$tablesRes) {
        return $schema;
    }

    $tables = [];
    while ($row = $tablesRes->fetch_array(MYSQLI_NUM)) {
        $tables[] = $row[0];
    }
    $tablesRes->free();

    foreach ($tables as $table) {
        $escaped = $mysqli->real_escape_string($table);
        $colsRes = $mysqli->query("SHOW COLUMNS FROM `{$escaped}`");
        if (!$colsRes) {
            continue;
        }

        $cols = [];
        while ($col = $colsRes->fetch_assoc()) {
            $cols[$col['Field']] = [
                'type'     => normalize_type((string)$col['Type']),
                'nullable' => ($col['Null'] === 'YES'),
                'default'  => $col['Default'],
                'key'      => (string)($col['Key'] ?? ''),
                'extra'    => (string)($col['Extra'] ?? ''),
            ];
        }
        $colsRes->free();

        // Fetch primary key columns
        $pkRes = $mysqli->query("SHOW INDEX FROM `{$escaped}` WHERE Key_name = 'PRIMARY'");
        $pkCols = [];
        if ($pkRes) {
            while ($idx = $pkRes->fetch_assoc()) {
                $pkCols[] = $idx['Column_name'];
            }
            $pkRes->free();
        }

        $schema[$table] = [
            'columns' => $cols,
            'pk'      => $pkCols,
        ];
    }

    return $schema;
}

/**
 * Normalize MySQL type to a comparable canonical form.
 * e.g. "int(11) unsigned" → "int unsigned"
 */
function normalize_type(string $raw): string
{
    $t = strtolower(trim($raw));
    // Remove display widths: int(11) → int, varchar(255) stays
    $t = preg_replace('/(tinyint|smallint|mediumint|int|bigint)\(\d+\)/', '$1', $t);
    // Collapse whitespace
    $t = preg_replace('/\s+/', ' ', $t);
    return $t;
}

/**
 * Compare source and target schemas for the given set of whitelisted tables.
 * Returns ['compatible' => bool, 'issues' => [...], 'tables_ok' => [...], 'tables_missing' => [...], 'tables_mismatch' => [...]]
 */
function compare_schemas(array $sourceSchema, array $targetSchema, array $whitelistedTables): array
{
    $issues = [];
    $tablesOk = [];
    $tablesMissing = [];
    $tablesMismatch = [];

    foreach ($whitelistedTables as $tableName) {
        if (!isset($sourceSchema[$tableName])) {
            // Source doesn't have this table — skip silently (it's not in source, nothing to sync)
            continue;
        }

        if (!isset($targetSchema[$tableName])) {
            $tablesMissing[] = $tableName;
            $issues[] = [
                'table'   => $tableName,
                'level'   => 'critical',
                'message' => "Таблица '{$tableName}' отсутствует в целевой БД.",
            ];
            continue;
        }

        $srcCols = $sourceSchema[$tableName]['columns'] ?? [];
        $tgtCols = $targetSchema[$tableName]['columns'] ?? [];
        $tableOk = true;

        // Check that all source columns exist in target with compatible types
        foreach ($srcCols as $colName => $srcDef) {
            if (!isset($tgtCols[$colName])) {
                $tableOk = false;
                $issues[] = [
                    'table'   => $tableName,
                    'column'  => $colName,
                    'level'   => 'critical',
                    'message' => "Колонка '{$colName}' отсутствует в таблице '{$tableName}' целевой БД.",
                ];
                continue;
            }

            $tgtDef = $tgtCols[$colName];

            // Compare normalized types
            if ($srcDef['type'] !== $tgtDef['type']) {
                // Check if they're at least in the same family
                $srcBase = preg_replace('/\s.*/', '', $srcDef['type']);
                $tgtBase = preg_replace('/\s.*/', '', $tgtDef['type']);

                if ($srcBase !== $tgtBase) {
                    $tableOk = false;
                    $issues[] = [
                        'table'   => $tableName,
                        'column'  => $colName,
                        'level'   => 'critical',
                        'message' => "Несовместимый тип колонки '{$colName}' в '{$tableName}': "
                            . "источник='{$srcDef['type']}', цель='{$tgtDef['type']}'.",
                    ];
                } else {
                    // Same base type, different size — warning
                    $issues[] = [
                        'table'   => $tableName,
                        'column'  => $colName,
                        'level'   => 'warning',
                        'message' => "Размер типа колонки '{$colName}' в '{$tableName}' отличается: "
                            . "источник='{$srcDef['type']}', цель='{$tgtDef['type']}'.",
                    ];
                }
            }

            // Check nullable compatibility
            if (!$srcDef['nullable'] && $tgtDef['nullable']) {
                // Source is NOT NULL but target allows NULL — OK, no data loss
            } elseif ($srcDef['nullable'] && !$tgtDef['nullable']) {
                // Source allows NULL but target doesn't — potential issue
                $issues[] = [
                    'table'   => $tableName,
                    'column'  => $colName,
                    'level'   => 'warning',
                    'message' => "Колонка '{$colName}' в '{$tableName}': источник допускает NULL, цель — нет.",
                ];
            }
        }

        // Check PK compatibility
        $srcPk = $sourceSchema[$tableName]['pk'] ?? [];
        $tgtPk = $targetSchema[$tableName]['pk'] ?? [];
        if ($srcPk !== $tgtPk) {
            $issues[] = [
                'table'   => $tableName,
                'level'   => 'warning',
                'message' => "Первичный ключ таблицы '{$tableName}' отличается: "
                    . "источник=[" . implode(',', $srcPk) . "], цель=[" . implode(',', $tgtPk) . "].",
            ];
        }

        if ($tableOk) {
            $tablesOk[] = $tableName;
        } else {
            $tablesMismatch[] = $tableName;
        }
    }

    $criticalIssues = array_filter($issues, fn($i) => $i['level'] === 'critical');
    $compatible = count($criticalIssues) === 0;

    return [
        'compatible'     => $compatible,
        'issues'         => array_values($issues),
        'tables_ok'      => $tablesOk,
        'tables_missing' => $tablesMissing,
        'tables_mismatch' => $tablesMismatch,
    ];
}

// ── Operation validation & application ──────────────────────────────────────

// Whitelist of allowed resources (must match db-crud.php $RESOURCES)
// We re-declare it here to avoid coupling; this is intentional for security.
function get_resource_whitelist(): array
{
    return [
        'vouchers'   => ['table' => 'tw_voucher',    'pk' => 'ID', 'columns' => ['Code', 'Data', 'Multiple', 'ValidUntil'], 'json' => ['Data']],
        'bots_mobs'  => ['table' => 'tw_bots_mobs',  'pk' => 'ID', 'columns' => ['BotID','WorldID','PositionX','PositionY','Debuffs','Behavior','Level','Power','Number','Respawn','Radius','ActiveRadius','Boss','it_drop_0','it_drop_1','it_drop_2','it_drop_3','it_drop_4','it_drop_count','it_drop_chance']],
        'bots_info'  => ['table' => 'tw_bots_info',  'pk' => 'ID', 'columns' => ['Name','JsonTeeInfo','EquippedModules','SlotHammer','SlotGun','SlotShotgun','SlotGrenade','SlotRifle','SlotArmor']],
        'bots_npc'   => ['table' => 'tw_bots_npc',   'pk' => 'ID', 'columns' => ['BotID','PosX','PosY','GiveQuestID','DialogData','Function','Static','Emote','WorldID'], 'json' => ['DialogData']],
        'crafts'     => ['table' => 'tw_crafts_list','pk' => 'ID', 'columns' => ['GroupName','ItemID','ItemValue','RequiredItems','Price','WorldID']],
        'warehouses' => ['table' => 'tw_warehouses', 'pk' => 'ID', 'columns' => ['Name','Type','Trades','PosX','PosY','StorageData','Currency','WorldID']],
        'worlds'     => ['table' => 'tw_worlds',     'pk' => 'ID', 'columns' => ['Name','Path','Type','Flags','RespawnWorldID','JailWorldID','RequiredLevel']],
        'aethers'    => ['table' => 'tw_aethers',    'pk' => 'ID', 'columns' => ['Name','WorldID','TeleX','TeleY']],
        'items'      => ['table' => 'tw_items_list', 'pk' => 'ID', 'columns' => ['Comment','Name','Description','Group','Type','Flags','ScenarioMode','ScenarioData','InitialPrice','RequiresProducts','AT1','AT2','ATValue1','ATValue2','Data'], 'json' => ['Data']],
        'dungeons'   => ['table' => 'tw_dungeons',   'pk' => 'ID', 'columns' => ['Level','DoorX','DoorY','Scenario','WorldID','TimeLimit']],
        'quests'     => ['table' => 'tw_quests_list','pk' => 'ID', 'columns' => ['NextQuestID','Name','Money','Exp','Flags']],
        'quest_bots' => ['table' => 'tw_bots_quest', 'pk' => 'ID', 'columns' => ['BotID','QuestID','Step','WorldID','PosX','PosY','AutoFinish','DialogData','ScenarioData','TasksData'], 'json' => ['DialogData','TasksData']],
    ];
}

function validate_operation(array $op, array $whitelist): ?string
{
    $action   = (string)($op['action'] ?? '');
    $resource = (string)($op['resource'] ?? '');

    if (!in_array($action, ['create', 'update', 'delete'], true)) {
        return "Недопустимая операция: '{$action}'";
    }

    if (!isset($whitelist[$resource])) {
        return "Недопустимый ресурс: '{$resource}'";
    }

    $R = $whitelist[$resource];

    if ($action === 'update' || $action === 'delete') {
        $id = $op['id'] ?? null;
        if ($id === null || !is_numeric($id) || (int)$id <= 0) {
            return "Недопустимый ID для операции '{$action}': " . var_export($id, true);
        }
    }

    if ($action === 'create' || $action === 'update') {
        $data = $op['data'] ?? null;
        if (!is_array($data) || empty($data)) {
            return "Пустые данные для операции '{$action}'";
        }
        // Check that all data keys are in the whitelist
        $allowedCols = $R['columns'];
        foreach (array_keys($data) as $col) {
            if (!in_array($col, $allowedCols, true)) {
                return "Недопустимая колонка '{$col}' для ресурса '{$resource}'";
            }
        }
    }

    return null; // valid
}

function encode_json_cols_sync(array $data, array $jsonCols): array
{
    foreach ($jsonCols as $col) {
        if (!array_key_exists($col, $data)) continue;
        $v = $data[$col];
        if (is_array($v) || is_object($v)) {
            $data[$col] = json_encode($v, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
        }
    }
    return $data;
}

function bind_type_for_value_sync(mixed $value): string
{
    if ($value === null) return 's';
    if (is_int($value)) return 'i';
    if (is_float($value)) return 'd';
    if (is_bool($value)) return 'i';
    if (is_string($value) && preg_match('/^-?\d+$/', $value) === 1) return 'i';
    if (is_string($value) && is_numeric($value)) return 'd';
    return 's';
}

function normalize_bind_value_sync(mixed $value, string $type): mixed
{
    if ($value === null) return null;
    if ($type === 'i') return (int)$value;
    if ($type === 'd') return (float)$value;
    return (string)$value;
}

function apply_operations(mysqli $mysqli, array $operations, array $whitelist): array
{
    $results = [];
    $successCount = 0;
    $failCount = 0;

    // Use transaction for atomicity
    $mysqli->begin_transaction(MYSQLI_TRANS_START_READ_WRITE);

    try {
        foreach ($operations as $idx => $op) {
            $action   = (string)($op['action'] ?? '');
            $resource = (string)($op['resource'] ?? '');
            $R = $whitelist[$resource];
            $table = $R['table'];
            $pk = $R['pk'];
            $cols = $R['columns'];
            $jsonCols = $R['json'] ?? [];

            try {
                if ($action === 'create') {
                    $data = $op['data'] ?? [];
                    // Sanitize: only whitelisted columns
                    $data = array_intersect_key($data, array_flip($cols));
                    $data = encode_json_cols_sync($data, $jsonCols);

                    if (empty($data)) {
                        throw new RuntimeException("Пустые данные для создания");
                    }

                    $keys = array_keys($data);
                    $placeholders = implode(',', array_fill(0, count($keys), '?'));
                    $colSql = implode(',', array_map(fn($c) => '`' . $c . '`', $keys));
                    $sql = "INSERT INTO `{$table}` ({$colSql}) VALUES ({$placeholders})";
                    $stmt = $mysqli->prepare($sql);
                    if (!$stmt) throw new RuntimeException("Prepare failed: " . $mysqli->error);

                    $types = '';
                    $vals = [];
                    foreach ($keys as $k) {
                        $v = $data[$k];
                        $t = bind_type_for_value_sync($v);
                        $types .= $t;
                        $vals[] = normalize_bind_value_sync($v, $t);
                    }
                    $stmt->bind_param($types, ...$vals);
                    if (!$stmt->execute()) {
                        throw new RuntimeException("Insert failed: " . $stmt->error);
                    }
                    $newId = (int)$mysqli->insert_id;
                    $stmt->close();

                    $results[] = [
                        'index'    => $idx,
                        'action'   => $action,
                        'resource' => $resource,
                        'ok'       => true,
                        'new_id'   => $newId,
                    ];
                    $successCount++;

                } elseif ($action === 'update') {
                    $id = (int)$op['id'];
                    $data = $op['data'] ?? [];
                    $data = array_intersect_key($data, array_flip($cols));
                    $data = encode_json_cols_sync($data, $jsonCols);

                    if (empty($data)) {
                        throw new RuntimeException("Пустые данные для обновления");
                    }

                    $keys = array_keys($data);
                    $setSql = implode(', ', array_map(fn($c) => '`' . $c . '` = ?', $keys));
                    $sql = "UPDATE `{$table}` SET {$setSql} WHERE `{$pk}` = ? LIMIT 1";
                    $stmt = $mysqli->prepare($sql);
                    if (!$stmt) throw new RuntimeException("Prepare failed: " . $mysqli->error);

                    $types = '';
                    $vals = [];
                    foreach ($keys as $k) {
                        $v = $data[$k];
                        $t = bind_type_for_value_sync($v);
                        $types .= $t;
                        $vals[] = normalize_bind_value_sync($v, $t);
                    }
                    $types .= 'i';
                    $vals[] = $id;

                    $stmt->bind_param($types, ...$vals);
                    if (!$stmt->execute()) {
                        throw new RuntimeException("Update failed: " . $stmt->error);
                    }
                    $affected = $stmt->affected_rows;
                    $stmt->close();

                    $results[] = [
                        'index'    => $idx,
                        'action'   => $action,
                        'resource' => $resource,
                        'id'       => $id,
                        'ok'       => true,
                        'affected' => $affected,
                    ];
                    $successCount++;

                } elseif ($action === 'delete') {
                    $id = (int)$op['id'];
                    $sql = "DELETE FROM `{$table}` WHERE `{$pk}` = ? LIMIT 1";
                    $stmt = $mysqli->prepare($sql);
                    if (!$stmt) throw new RuntimeException("Prepare failed: " . $mysqli->error);
                    $stmt->bind_param('i', $id);
                    if (!$stmt->execute()) {
                        throw new RuntimeException("Delete failed: " . $stmt->error);
                    }
                    $affected = $stmt->affected_rows;
                    $stmt->close();

                    $results[] = [
                        'index'    => $idx,
                        'action'   => $action,
                        'resource' => $resource,
                        'id'       => $id,
                        'ok'       => true,
                        'affected' => $affected,
                    ];
                    $successCount++;
                }

            } catch (Throwable $e) {
                $results[] = [
                    'index'    => $idx,
                    'action'   => $action,
                    'resource' => $resource,
                    'id'       => $op['id'] ?? null,
                    'ok'       => false,
                    'error'    => $e->getMessage(),
                ];
                $failCount++;
            }
        }

        if ($failCount > 0) {
            // Rollback on any failure
            $mysqli->rollback();
            return [
                'ok'        => false,
                'error'     => "Применение прервано: {$failCount} из " . count($operations) . " операций завершились ошибкой. Все изменения откачены.",
                'results'   => $results,
                'success'   => $successCount,
                'failed'    => $failCount,
                'committed' => false,
            ];
        }

        $mysqli->commit();
        return [
            'ok'        => true,
            'results'   => $results,
            'success'   => $successCount,
            'failed'    => 0,
            'committed' => true,
        ];

    } catch (Throwable $e) {
        $mysqli->rollback();
        return [
            'ok'        => false,
            'error'     => 'Критическая ошибка: ' . $e->getMessage(),
            'results'   => $results,
            'success'   => $successCount,
            'failed'    => $failCount,
            'committed' => false,
        ];
    }
}

// ── Action handlers ─────────────────────────────────────────────────────────

function handle_list_profiles(): never
{
    $syncCfg = load_sync_cfg();

    // Also load main profiles from the settings (stored client-side, but we
    // store target profile data server-side for security).
    $profiles = [];
    foreach (($syncCfg['profiles'] ?? []) as $p) {
        $profiles[] = [
            'id'       => $p['id'] ?? '',
            'name'     => $p['name'] ?? '',
            'host'     => $p['host'] ?? '',
            'port'     => (int)($p['port'] ?? 3306),
            'database' => $p['database'] ?? '',
            'user'     => $p['user'] ?? '',
            // NO password
        ];
    }

    respond([
        'ok'                  => true,
        'profiles'            => $profiles,
        'target_profile_id'   => (string)($syncCfg['target_profile_id'] ?? ''),
    ]);
}

function handle_save_target(): never
{
    $body = read_json_body();

    $profileId   = trim((string)($body['profile_id'] ?? ''));
    $profileName = trim((string)($body['profile_name'] ?? ''));
    $host        = trim((string)($body['host'] ?? ''));
    $port        = max(1, min(65535, (int)($body['port'] ?? 3306)));
    $database    = trim((string)($body['database'] ?? ''));
    $user        = trim((string)($body['user'] ?? ''));
    $password    = (string)($body['password'] ?? '');
    $isRemove    = (bool)($body['remove'] ?? false);

    $syncCfg = load_sync_cfg();

    if ($isRemove) {
        // Remove profile
        $syncCfg['profiles'] = array_values(array_filter(
            ($syncCfg['profiles'] ?? []),
            fn($p) => ($p['id'] ?? '') !== $profileId
        ));
        if (($syncCfg['target_profile_id'] ?? '') === $profileId) {
            $syncCfg['target_profile_id'] = '';
        }
        save_sync_cfg($syncCfg);
        respond(['ok' => true]);
    }

    if ($profileId === '' || $host === '' || $database === '' || $user === '') {
        respond(['ok' => false, 'error' => 'Заполните обязательные поля: host, database, user'], 400);
    }

    // Check if profile already exists
    $profiles = $syncCfg['profiles'] ?? [];
    $existingProfile = null;
    foreach ($profiles as $p) {
        if (($p['id'] ?? '') === $profileId) {
            $existingProfile = $p;
            break;
        }
    }

    // If password not provided and profile exists, use stored password for testing
    if ($password === '' && $existingProfile !== null) {
        $password = (string)($existingProfile['password'] ?? '');
    }

    // Test connection before saving
    $testProfile = [
        'host'     => $host,
        'port'     => $port,
        'user'     => $user,
        'password' => $password,
        'database' => $database,
    ];

    try {
        $testConn = db_connect_profile($testProfile);
        $serverInfo = $testConn->server_info;
        $testConn->close();
    } catch (Throwable $e) {
        respond(['ok' => false, 'error' => 'Не удалось подключиться: ' . $e->getMessage()], 400);
    }

    // Update or create profile
    $found = false;
    foreach ($profiles as &$p) {
        if (($p['id'] ?? '') === $profileId) {
            $p['name']     = $profileName ?: $p['name'];
            $p['host']     = $host;
            $p['port']     = $port;
            $p['database'] = $database;
            $p['user']     = $user;
            if ($password !== '') {
                $p['password'] = $password;
            }
            $found = true;
            break;
        }
    }
    unset($p);

    if (!$found) {
        $profiles[] = [
            'id'       => $profileId,
            'name'     => $profileName ?: 'Target',
            'host'     => $host,
            'port'     => $port,
            'database' => $database,
            'user'     => $user,
            'password' => $password,
        ];
    }

    $syncCfg['profiles'] = $profiles;

    // Set as active target if requested
    if (!empty($body['set_as_target'])) {
        $syncCfg['target_profile_id'] = $profileId;
    }

    save_sync_cfg($syncCfg);

    respond([
        'ok'     => true,
        'server' => $serverInfo ?? '',
    ]);
}

function handle_get_target(): never
{
    $syncCfg = load_sync_cfg();
    $targetId = (string)($syncCfg['target_profile_id'] ?? '');

    if ($targetId === '') {
        respond(['ok' => true, 'target' => null]);
    }

    $profiles = $syncCfg['profiles'] ?? [];
    foreach ($profiles as $p) {
        if (($p['id'] ?? '') === $targetId) {
            respond(['ok' => true, 'target' => [
                'id'       => $p['id'],
                'name'     => $p['name'] ?? '',
                'host'     => $p['host'] ?? '',
                'port'     => (int)($p['port'] ?? 3306),
                'database' => $p['database'] ?? '',
            ]]);
        }
    }

    respond(['ok' => true, 'target' => null]);
}

function handle_set_as_target(): never
{
    $body = read_json_body();
    $profileId = trim((string)($body['profile_id'] ?? ''));

    if ($profileId === '') {
        respond(['ok' => false, 'error' => 'Profile ID is required'], 400);
    }

    $syncCfg = load_sync_cfg();

    // Verify profile exists
    $found = false;
    foreach (($syncCfg['profiles'] ?? []) as $p) {
        if (($p['id'] ?? '') === $profileId) {
            $found = true;
            break;
        }
    }

    if (!$found) {
        respond(['ok' => false, 'error' => 'Профиль не найден'], 404);
    }

    $syncCfg['target_profile_id'] = $profileId;
    save_sync_cfg($syncCfg);

    respond(['ok' => true]);
}

function handle_compare_schema(): never
{
    $body = read_json_body();

    // Get target profile - use provided ID or fall back to configured target
    $syncCfg = load_sync_cfg();
    $targetId = trim((string)($body['target_profile_id'] ?? ''));
    
    // If empty string provided, use configured target
    if ($targetId === '') {
        $targetId = (string)($syncCfg['target_profile_id'] ?? '');
    }

    if ($targetId === '') {
        respond(['ok' => false, 'error' => 'Целевое подключение не настроено.'], 400);
    }

    $targetProfile = null;
    foreach (($syncCfg['profiles'] ?? []) as $p) {
        if (($p['id'] ?? '') === $targetId) {
            $targetProfile = $p;
            break;
        }
    }

    if (!$targetProfile) {
        respond(['ok' => false, 'error' => 'Целевой профиль не найден.'], 404);
    }

    // Get whitelisted tables
    $whitelist = get_resource_whitelist();
    $tableNames = array_unique(array_map(fn($r) => $r['table'], $whitelist));

    // Fetch source schema
    try {
        $sourceConn = db_connect();
        $sourceSchema = fetch_schema($sourceConn);
        $sourceConn->close();
    } catch (Throwable $e) {
        respond(['ok' => false, 'error' => 'Ошибка подключения к исходной БД: ' . $e->getMessage()], 500);
    }

    // Fetch target schema
    try {
        $targetConn = db_connect_profile($targetProfile);
        $targetSchema = fetch_schema($targetConn);
        $targetConn->close();
    } catch (Throwable $e) {
        respond(['ok' => false, 'error' => 'Ошибка подключения к целевой БД: ' . $e->getMessage()], 500);
    }

    $comparison = compare_schemas($sourceSchema, $targetSchema, $tableNames);

    respond([
        'ok'         => true,
        'comparison' => $comparison,
        'target'     => [
            'name'     => $targetProfile['name'] ?? '',
            'database' => $targetProfile['database'] ?? '',
        ],
    ]);
}

function handle_apply_operations(): never
{
    $body = read_json_body();

    // Get target profile - use provided ID or fall back to configured target
    $syncCfg = load_sync_cfg();
    $targetId = trim((string)($body['target_profile_id'] ?? ''));
    
    // If empty string provided, use configured target
    if ($targetId === '') {
        $targetId = (string)($syncCfg['target_profile_id'] ?? '');
    }

    if ($targetId === '') {
        respond(['ok' => false, 'error' => 'Целевое подключение не настроено.'], 400);
    }

    $targetProfile = null;
    foreach (($syncCfg['profiles'] ?? []) as $p) {
        if (($p['id'] ?? '') === $targetId) {
            $targetProfile = $p;
            break;
        }
    }

    if (!$targetProfile) {
        respond(['ok' => false, 'error' => 'Целевой профиль не найден.'], 404);
    }

    $operations = $body['operations'] ?? [];
    if (!is_array($operations) || empty($operations)) {
        respond(['ok' => false, 'error' => 'Нет операций для применения.'], 400);
    }

    // Safety limit
    if (count($operations) > 1000) {
        respond(['ok' => false, 'error' => 'Слишком много операций (максимум 1000 за раз).'], 400);
    }

    // Confirmation required
    $confirmPhrase = trim((string)($body['confirm_phrase'] ?? ''));
    if (mb_strtoupper($confirmPhrase) !== 'ПРИМЕНИТЬ' && mb_strtoupper($confirmPhrase) !== 'APPLY') {
        respond(['ok' => false, 'error' => 'Требуется подтверждение: введите "Применить".'], 400);
    }

    $whitelist = get_resource_whitelist();

    // Validate ALL operations first (before any execution)
    $validationErrors = [];
    foreach ($operations as $idx => $op) {
        $err = validate_operation($op, $whitelist);
        if ($err !== null) {
            $validationErrors[] = ['index' => $idx, 'error' => $err];
        }
    }

    if (!empty($validationErrors)) {
        respond([
            'ok'    => false,
            'error' => 'Обнаружены недопустимые операции. Применение отменено.',
            'validation_errors' => $validationErrors,
        ], 400);
    }

    // Pre-check: compare schemas for involved tables
    $involvedTables = [];
    foreach ($operations as $op) {
        $resource = (string)($op['resource'] ?? '');
        if (isset($whitelist[$resource])) {
            $involvedTables[] = $whitelist[$resource]['table'];
        }
    }
    $involvedTables = array_unique($involvedTables);

    try {
        $sourceConn = db_connect();
        $sourceSchema = fetch_schema($sourceConn);
        $sourceConn->close();

        $targetConn = db_connect_profile($targetProfile);
        $targetSchema = fetch_schema($targetConn);

        $comparison = compare_schemas($sourceSchema, $targetSchema, $involvedTables);
        if (!$comparison['compatible']) {
            $targetConn->close();
            respond([
                'ok'         => false,
                'error'      => 'Структуры БД несовместимы. Применение невозможно.',
                'comparison' => $comparison,
            ], 400);
        }

        // Apply operations
        $result = apply_operations($targetConn, $operations, $whitelist);
        $targetConn->close();

        $result['comparison'] = $comparison;
        $result['target'] = [
            'name'     => $targetProfile['name'] ?? '',
            'database' => $targetProfile['database'] ?? '',
        ];

        respond($result);

    } catch (Throwable $e) {
        respond(['ok' => false, 'error' => $e->getMessage()], 500);
    }
}

// ── Router ──────────────────────────────────────────────────────────────────

$action = (string)($_GET['action'] ?? '');

$handlers = [
    'list_profiles'    => 'handle_list_profiles',
    'save_target'      => 'handle_save_target',
    'get_target'       => 'handle_get_target',
    'set_as_target'    => 'handle_set_as_target',
    'compare_schema'   => 'handle_compare_schema',
    'apply_operations' => 'handle_apply_operations',
];

try {
    if (isset($handlers[$action])) {
        $handlers[$action]();
    } else {
        respond(['ok' => false, 'error' => 'Unknown action'], 400);
    }
} catch (Throwable $e) {
    respond(['ok' => false, 'error' => $e->getMessage()], 500);
}
