<?php
// Editor Studio — единое хранилище (SQLite).
//
// Всё состояние редактора, кроме самой игровой базы, лежит в одном файле
// data/editor-studio.sqlite: настройки подключения, гостевые аккаунты,
// запросы гостей, переписка по ним, счётчики попыток входа и лимиты.
//
// Почему SQLite, а не JSON-файлы:
//  * одна транзакция вместо набора файлов — нет «полузаписанного» состояния;
//  * запросы и переписка выбираются по индексам (статус, автор, непрочитанное),
//    а не полным перебором каталога;
//  * блокировки и целостность обеспечивает сама СУБД (WAL + busy_timeout).
//
// Игровая БД (MySQL/MariaDB) остаётся отдельным подключением: SQLite хранит
// только служебные данные редактора.

declare(strict_types=1);

const EDITOR_STORE_SCHEMA = 3;
const EDITOR_STORE_FILE = 'editor-studio.sqlite';

/** Путь к файлу базы редактора (можно переопределить переменной окружения). */
function store_path(): string
{
    $custom = getenv('EDITOR_STUDIO_SQLITE');
    if (is_string($custom) && $custom !== '') {
        return $custom;
    }
    return dirname(__DIR__) . '/data/' . EDITOR_STORE_FILE;
}

function store_dir(): string
{
    $dir = dirname(store_path());
    if (!is_dir($dir) && !@mkdir($dir, 0775, true) && !is_dir($dir)) {
        throw new RuntimeException('Не удалось создать каталог данных: ' . $dir);
    }
    // Defense in depth: data/ никогда не должен отдаваться веб-сервером.
    $ht = $dir . '/.htaccess';
    if (!is_file($ht)) {
        @file_put_contents($ht, "<IfModule mod_authz_core.c>\n    Require all denied\n</IfModule>\n<IfModule !mod_authz_core.c>\n    Order allow,deny\n    Deny from all\n</IfModule>\n");
    }
    return $dir;
}

/** Единственное подключение на запрос. */
function store_db(): PDO
{
    static $pdo = null;
    if ($pdo instanceof PDO) {
        return $pdo;
    }

    if (!extension_loaded('pdo_sqlite')) {
        throw new RuntimeException('Не найдено расширение pdo_sqlite: хранилище Editor Studio требует SQLite.');
    }

    store_dir();
    $path = store_path();
    $fresh = !is_file($path);

    $pdo = new PDO('sqlite:' . $path, null, null, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    ]);
    // WAL лучше для параллельных чтений (несколько воркеров PHP). В тестах
    // каждый запрос — отдельный процесс с копией файла, поэтому там обычный
    // журнал: база остаётся одним самодостаточным файлом.
    $journal = getenv('EDITOR_STUDIO_TEST') === '1' ? 'DELETE' : 'WAL';
    try {
        $pdo->exec('PRAGMA journal_mode = ' . $journal);
    } catch (Throwable) {
        // ignore
    }
    $pdo->exec('PRAGMA foreign_keys = ON');
    $pdo->exec('PRAGMA busy_timeout = 5000');
    try {
        $pdo->exec('PRAGMA synchronous = NORMAL');
    } catch (Throwable) {
        // ignore
    }

    store_schema($pdo);
    store_init_network_rules($pdo);
    if ($fresh) {
        @chmod($path, 0640);
    }
    store_import_legacy($pdo);

    return $pdo;
}

/**
 * Одноразовая инициализация сетевых правил при запуске редактора.
 *
 * Ключи создаются только если их ещё нет (INSERT OR IGNORE): если значения
 * уже заданы в БД, они остаются неприкосновенными. Дальше этими правилами
 * управляет только база — API конфига и страницы редактора их не читают
 * на запись и не изменяют.
 */
function store_init_network_rules(PDO $pdo): void
{
    $now = store_now();
    $stmt = $pdo->prepare('INSERT OR IGNORE INTO settings(key, value, updated_at) VALUES(?, ?, ?)');
    $stmt->execute(['allow_remote_editor', 'false', $now]);
    $stmt->execute(['allowed_editor_ips', '[]', $now]);
}

/** Схема создаётся идемпотентно; версия хранится в PRAGMA user_version. */
function store_schema(PDO $pdo): void
{
    $version = (int)$pdo->query('PRAGMA user_version')->fetchColumn();
    if ($version >= EDITOR_STORE_SCHEMA) {
        return;
    }

    store_tx(function () use ($pdo): void {
        $pdo->exec(<<<'SQL'
            CREATE TABLE IF NOT EXISTS meta (
                key   TEXT PRIMARY KEY,
                value TEXT
            );

            -- Настройки подключения и правила: одна строка на параметр,
            -- значение — JSON. Так видно, что именно изменилось.
            CREATE TABLE IF NOT EXISTS settings (
                key        TEXT PRIMARY KEY,
                value      TEXT NOT NULL,
                updated_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS guest_users (
                id              TEXT PRIMARY KEY,
                login           TEXT NOT NULL,
                name            TEXT NOT NULL DEFAULT '',
                enabled         INTEGER NOT NULL DEFAULT 1,
                pages           TEXT NOT NULL DEFAULT '[]',
                password_hash   TEXT NOT NULL DEFAULT '',
                session_version INTEGER NOT NULL DEFAULT 1,
                created_at      TEXT NOT NULL DEFAULT '',
                updated_at      TEXT NOT NULL DEFAULT '',
                last_login_at   TEXT NOT NULL DEFAULT ''
            );
            CREATE UNIQUE INDEX IF NOT EXISTS idx_guest_login ON guest_users(login COLLATE NOCASE);

            -- Запрос гостя. payload — исходный JSON документа (его читают
            -- анализ и применение), отдельные колонки — для выборок и фильтров.
            CREATE TABLE IF NOT EXISTS change_requests (
                id          TEXT PRIMARY KEY,
                status      TEXT NOT NULL,
                guest_id    TEXT NOT NULL DEFAULT '',
                guest_login TEXT NOT NULL DEFAULT '',
                nickname    TEXT NOT NULL DEFAULT '',
                message     TEXT NOT NULL DEFAULT '',
                created_at  TEXT NOT NULL,
                updated_at  TEXT NOT NULL,
                payload     TEXT NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_cr_status ON change_requests(status, created_at DESC);
            CREATE INDEX IF NOT EXISTS idx_cr_guest ON change_requests(guest_id, created_at DESC);

            -- Переписка по запросу: гость и администратор видят один диалог.
            CREATE TABLE IF NOT EXISTS request_messages (
                id              INTEGER PRIMARY KEY AUTOINCREMENT,
                request_id      TEXT NOT NULL,
                author_role     TEXT NOT NULL,
                author_id       TEXT NOT NULL DEFAULT '',
                author_name     TEXT NOT NULL DEFAULT '',
                body            TEXT NOT NULL,
                created_at      TEXT NOT NULL,
                read_by_admin_at TEXT NOT NULL DEFAULT '',
                read_by_guest_at TEXT NOT NULL DEFAULT '',
                FOREIGN KEY (request_id) REFERENCES change_requests(id) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_msg_request ON request_messages(request_id, id);
            CREATE INDEX IF NOT EXISTS idx_msg_reader ON request_messages(read_by_admin_at, read_by_guest_at);

            -- Счётчики неудачных попыток входа (подбор пароля).
            CREATE TABLE IF NOT EXISTS auth_throttle (
                key      TEXT PRIMARY KEY,
                count    INTEGER NOT NULL DEFAULT 0,
                first_at INTEGER NOT NULL DEFAULT 0,
                until_at INTEGER NOT NULL DEFAULT 0
            );

            -- Простые ограничения частоты (отправка запросов, preview).
            CREATE TABLE IF NOT EXISTS rate_limits (
                key TEXT PRIMARY KEY,
                ts  INTEGER NOT NULL
            );
            SQL);

        $pdo->exec('PRAGMA user_version = ' . EDITOR_STORE_SCHEMA);
    });
}

function store_meta_get(string $key, ?string $default = null): ?string
{
    $stmt = store_db()->prepare('SELECT value FROM meta WHERE key = ?');
    $stmt->execute([$key]);
    $value = $stmt->fetchColumn();
    return $value === false ? $default : (string)$value;
}

function store_meta_set(string $key, string $value): void
{
    $stmt = store_db()->prepare(
        'INSERT INTO meta(key, value) VALUES(?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value'
    );
    $stmt->execute([$key, $value]);
}

function store_now(): string
{
    return gmdate('Y-m-d\TH:i:s\Z');
}

/**
 * Транзакция с поддержкой вложенности: внутренние вызовы (например,
 * сохранение настроек внутри mutate_cfg) не открывают вторую BEGIN.
 */
function store_tx(callable $fn, bool $writer = true): mixed
{
    static $depth = 0;
    $pdo = store_db();
    if ($depth > 0) {
        return $fn();
    }
    $pdo->exec($writer ? 'BEGIN IMMEDIATE' : 'BEGIN');
    $depth++;
    try {
        $result = $fn();
        $pdo->exec('COMMIT');
        return $result;
    } catch (Throwable $e) {
        try { $pdo->exec('ROLLBACK'); } catch (Throwable) {}
        throw $e;
    } finally {
        $depth--;
    }
}

// ── Настройки (key → JSON value) ─────────────────────────────────────────────

/** @return array<string, mixed> */
function store_settings_load(): array
{
    $out = [];
    foreach (store_db()->query('SELECT key, value FROM settings') as $row) {
        $decoded = json_decode((string)$row['value'], true);
        $out[(string)$row['key']] = $decoded;
    }
    return $out;
}

/**
 * Полная замена набора настроек в одной транзакции.
 * Гостевые аккаунты живут в отдельной таблице и здесь не сохраняются.
 */
function store_settings_save(array $cfg): bool
{
    unset($cfg['guest_users']);
    $pdo = store_db();
    $now = store_now();
    return store_tx(function () use ($pdo, $cfg, $now): bool {
        $existing = [];
        foreach ($pdo->query('SELECT key FROM settings') as $row) {
            $existing[(string)$row['key']] = true;
        }
        $upsert = $pdo->prepare(
            'INSERT INTO settings(key, value, updated_at) VALUES(?, ?, ?)
             ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = excluded.updated_at'
        );
        foreach ($cfg as $key => $value) {
            $key = (string)$key;
            $upsert->execute([$key, json_encode($value, JSON_UNESCAPED_UNICODE), $now]);
            unset($existing[$key]);
        }
        if ($existing) {
            $del = $pdo->prepare('DELETE FROM settings WHERE key = ?');
            foreach (array_keys($existing) as $key) {
                $del->execute([$key]);
            }
        }
        return true;
    });
}

function store_settings_set(string $key, mixed $value): void
{
    $stmt = store_db()->prepare(
        'INSERT INTO settings(key, value, updated_at) VALUES(?, ?, ?)
         ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = excluded.updated_at'
    );
    $stmt->execute([$key, json_encode($value, JSON_UNESCAPED_UNICODE), store_now()]);
}

// ── Гостевые аккаунты ───────────────────────────────────────────────────────

/** @return list<array> строки таблицы в том же виде, что и раньше в конфиге */
function store_guest_users_all(): array
{
    $rows = [];
    foreach (store_db()->query('SELECT * FROM guest_users ORDER BY created_at, id') as $row) {
        $rows[] = [
            'id'             => (string)$row['id'],
            'login'          => (string)$row['login'],
            'name'           => (string)$row['name'],
            'enabled'        => (bool)(int)$row['enabled'],
            'pages'          => json_decode((string)$row['pages'], true) ?: [],
            'passwordHash'   => (string)$row['password_hash'],
            'sessionVersion' => (int)$row['session_version'],
            'createdAt'      => (string)$row['created_at'],
            'updatedAt'      => (string)$row['updated_at'],
            'lastLoginAt'    => (string)$row['last_login_at'],
        ];
    }
    return $rows;
}

function store_guest_user_get(string $id): ?array
{
    foreach (store_guest_users_all() as $user) {
        if ($user['id'] === strtolower(trim($id))) {
            return $user;
        }
    }
    return null;
}

function store_guest_user_by_login(string $login): ?array
{
    $login = strtolower(trim($login));
    if ($login === '') {
        return null;
    }
    foreach (store_guest_users_all() as $user) {
        if ($user['login'] === $login) {
            return $user;
        }
    }
    return null;
}

function store_guest_user_insert(array $user): bool
{
    $stmt = store_db()->prepare(
        'INSERT INTO guest_users(id, login, name, enabled, pages, password_hash, session_version, created_at, updated_at, last_login_at)
         VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)'
    );
    return $stmt->execute([
        (string)$user['id'],
        strtolower((string)$user['login']),
        (string)($user['name'] ?? ''),
        !empty($user['enabled']) ? 1 : 0,
        json_encode(array_values($user['pages'] ?? []), JSON_UNESCAPED_UNICODE),
        (string)($user['passwordHash'] ?? ''),
        (int)($user['sessionVersion'] ?? 1),
        (string)($user['createdAt'] ?? store_now()),
        (string)($user['updatedAt'] ?? store_now()),
        (string)($user['lastLoginAt'] ?? ''),
    ]);
}

/** Частичное обновление: передавайте только те поля, которые нужно изменить. */
function store_guest_user_update(string $id, array $fields): bool
{
    $id = strtolower(trim($id));
    $map = [
        'login'          => 'login',
        'name'           => 'name',
        'enabled'        => 'enabled',
        'pages'          => 'pages',
        'passwordHash'   => 'password_hash',
        'sessionVersion' => 'session_version',
        'updatedAt'      => 'updated_at',
        'lastLoginAt'    => 'last_login_at',
    ];
    $sets = [];
    $values = [];
    foreach ($map as $field => $column) {
        if (!array_key_exists($field, $fields)) {
            continue;
        }
        $value = $fields[$field];
        if ($field === 'pages') {
            $value = json_encode(array_values((array)$value), JSON_UNESCAPED_UNICODE);
        } elseif ($field === 'enabled') {
            $value = $value ? 1 : 0;
        }
        $sets[] = $column . ' = ?';
        $values[] = $value;
    }
    if (!$sets) {
        return true;
    }
    $values[] = $id;
    $stmt = store_db()->prepare('UPDATE guest_users SET ' . implode(', ', $sets) . ' WHERE id = ?');
    $stmt->execute($values);
    return $stmt->rowCount() > 0;
}

function store_guest_user_delete(string $id): bool
{
    $stmt = store_db()->prepare('DELETE FROM guest_users WHERE id = ?');
    $stmt->execute([strtolower(trim($id))]);
    return $stmt->rowCount() > 0;
}

function store_guest_users_count(): int
{
    return (int)store_db()->query('SELECT COUNT(*) FROM guest_users')->fetchColumn();
}

// ── Счётчики входа ──────────────────────────────────────────────────────────

/** @return array<string, array{count:int, first:int, until:int}> */
function store_throttle_all(): array
{
    $map = [];
    foreach (store_db()->query('SELECT key, count, first_at, until_at FROM auth_throttle') as $row) {
        // Формат записи совпадает с прежним файловым хранилищем (n/first/until).
        $map[(string)$row['key']] = [
            'n' => (int)$row['count'],
            'first' => (int)$row['first_at'],
            'until' => (int)$row['until_at'],
        ];
    }
    return $map;
}

/**
 * Атомарное изменение счётчиков: SQLite блокирует базу на время транзакции,
 * поэтому подбор пароля нельзя разогнать параллельными запросами.
 *
 * @param callable(array<string, array>, int): (array<string, array>|null) $fn
 */
function store_throttle_mutate(callable $fn): mixed
{
    $pdo = store_db();
    return store_tx(function () use ($pdo, $fn): mixed {
        $map = store_throttle_all();
        $out = $fn($map, time());
        // Контракт: ['map' => array, 'result' => mixed]; null — ничего не менять.
        $next = is_array($out) ? ($out['map'] ?? null) : null;
        if (is_array($next)) {
            $pdo->exec('DELETE FROM auth_throttle');
            $insert = $pdo->prepare('INSERT INTO auth_throttle(key, count, first_at, until_at) VALUES(?, ?, ?, ?)');
            foreach ($next as $key => $row) {
                $insert->execute([
                    (string)$key,
                    (int)($row['n'] ?? 0),
                    (int)($row['first'] ?? 0),
                    (int)($row['until'] ?? 0),
                ]);
            }
        }
        return is_array($out) ? ($out['result'] ?? null) : null;
    });
}

// ── Ограничение частоты (cooldown отправки запросов) ─────────────────────────

function store_rate_touch(string $key, int $ts): void
{
    $stmt = store_db()->prepare(
        'INSERT INTO rate_limits(key, ts) VALUES(?, ?) ON CONFLICT(key) DO UPDATE SET ts = excluded.ts'
    );
    $stmt->execute([$key, $ts]);
}

function store_rate_get(string $key): int
{
    $stmt = store_db()->prepare('SELECT ts FROM rate_limits WHERE key = ?');
    $stmt->execute([$key]);
    $ts = $stmt->fetchColumn();
    return $ts === false ? 0 : (int)$ts;
}

function store_rate_prune(int $olderThan): void
{
    $stmt = store_db()->prepare('DELETE FROM rate_limits WHERE ts < ?');
    $stmt->execute([$olderThan]);
}

// ── Миграция со старого (файлового) хранилища ───────────────────────────────

/**
 * Одноразовый перенос данных из data/*.json и data/change-requests/*.json.
 * Файлы не удаляются: они остаются резервной копией и игнорируются после
 * установки флага migrated_from_json.
 */
function store_import_legacy(PDO $pdo): void
{
    if (store_meta_get('migrated_from_json') === '1') {
        return;
    }

    $dir = dirname(store_path());
    $cfgFile = $dir . '/db-config.json';
    $requestsDir = $dir . '/change-requests';
    $throttleFile = $dir . '/auth-throttle.json';
    $syncCfgFile = $dir . '/db-sync-config.json';

    $hasConfig = is_file($cfgFile);
    $hasSyncCfg = is_file($syncCfgFile);
    $hasRequests = is_dir($requestsDir) && (glob($requestsDir . '/CR-*.json') ?: []);
    $hasThrottle = is_file($throttleFile);

    if (!$hasConfig && !$hasRequests && !$hasThrottle && !$hasSyncCfg) {
        store_meta_set('migrated_from_json', '1');
        return;
    }

    store_tx(function () use ($pdo, $hasConfig, $cfgFile, $hasRequests, $requestsDir, $hasThrottle, $throttleFile, $hasSyncCfg, $syncCfgFile): void {
        $insert = $pdo->prepare(
            'INSERT INTO settings(key, value, updated_at) VALUES(?, ?, ?)
             ON CONFLICT(key) DO UPDATE SET value = excluded.value'
        );
        if ($hasConfig) {
            $cfg = json_decode((string)@file_get_contents($cfgFile), true);
            if (is_array($cfg)) {
                $users = is_array($cfg['guest_users'] ?? null) ? $cfg['guest_users'] : [];
                unset($cfg['guest_users']);
                $now = store_now();
                foreach ($cfg as $key => $value) {
                    $insert->execute([(string)$key, json_encode($value, JSON_UNESCAPED_UNICODE), $now]);
                }
                $guestInsert = $pdo->prepare(
                    'INSERT OR IGNORE INTO guest_users(id, login, name, enabled, pages, password_hash, session_version, created_at, updated_at, last_login_at)
                     VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)'
                );
                foreach ($users as $user) {
                    if (!is_array($user) || (string)($user['id'] ?? '') === '') {
                        continue;
                    }
                    $guestInsert->execute([
                        (string)$user['id'],
                        strtolower((string)($user['login'] ?? '')),
                        (string)($user['name'] ?? ''),
                        !empty($user['enabled']) ? 1 : 0,
                        json_encode(array_values((array)($user['pages'] ?? [])), JSON_UNESCAPED_UNICODE),
                        (string)($user['passwordHash'] ?? ''),
                        max(1, (int)($user['sessionVersion'] ?? 1)),
                        (string)($user['createdAt'] ?? ''),
                        (string)($user['updatedAt'] ?? ''),
                        (string)($user['lastLoginAt'] ?? ''),
                    ]);
                }
            }
        }

        if ($hasRequests) {
            $insert = $pdo->prepare(
                'INSERT OR IGNORE INTO change_requests(id, status, guest_id, guest_login, nickname, message, created_at, updated_at, payload)
                 VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)'
            );
            foreach (glob($requestsDir . '/CR-*.json') ?: [] as $file) {
                $req = json_decode((string)@file_get_contents($file), true);
                if (!is_array($req) || (string)($req['id'] ?? '') === '') {
                    continue;
                }
                $author = is_array($req['author'] ?? null) ? $req['author'] : [];
                $insert->execute([
                    (string)$req['id'],
                    (string)($req['status'] ?? 'pending'),
                    (string)($author['guestId'] ?? ''),
                    (string)($author['guestLogin'] ?? ''),
                    (string)($author['nickname'] ?? ''),
                    (string)($req['message'] ?? ''),
                    (string)($req['createdAt'] ?? store_now()),
                    (string)($req['updatedAt'] ?? ($req['createdAt'] ?? store_now())),
                    json_encode($req, JSON_UNESCAPED_UNICODE),
                ]);
            }
        }

        if ($hasSyncCfg) {
            $sync = json_decode((string)@file_get_contents($syncCfgFile), true);
            if (is_array($sync)) {
                $insert->execute(['sync_config', json_encode($sync, JSON_UNESCAPED_UNICODE), store_now()]);
            }
        }

        if ($hasThrottle) {
            $map = json_decode((string)@file_get_contents($throttleFile), true);
            if (is_array($map)) {
                $insert = $pdo->prepare(
                    'INSERT OR REPLACE INTO auth_throttle(key, count, first_at, until_at) VALUES(?, ?, ?, ?)'
                );
                foreach ($map as $key => $row) {
                    if (!is_array($row)) {
                        continue;
                    }
                    $insert->execute([
                        (string)$key,
                        (int)($row['n'] ?? $row['count'] ?? 0),
                        (int)($row['first'] ?? 0),
                        (int)($row['until'] ?? 0),
                    ]);
                }
            }
        }

    });

    store_meta_set('migrated_from_json', '1');
    store_meta_set('migrated_at', store_now());
}
