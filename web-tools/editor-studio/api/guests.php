<?php
// Guest accounts API — administrator only.
// Endpoint: api/guests.php?action=...
//
//   list     (GET)  — guest accounts + editor tab catalogue
//   create   (POST) — {login, name, password, pages[], enabled, limits?, expires_at?}
//   update   (POST) — {id, login?, name?, pages[]?, enabled?, limits?, expires_at?}
//   password (POST) — {id, password}         (also revokes the account sessions)
//   revoke   (POST) — {id}                   (ends all active sessions of the account)
//   delete   (POST) — {id}
//
// A guest account only allows READ access to the assigned tabs and creates
// change requests — the database is written exclusively by an administrator
// after review (strict guest → admin → DB).

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';

$ROLE = bootstrap_editor_api(true); // admin only

$action = (string)($_GET['action'] ?? 'list');

const GUEST_LOGIN_RE = '/^[a-z0-9_.-]{3,32}$/';
const GUEST_PASSWORD_MIN = 10;
const GUEST_PASSWORD_MAX = 72; // байт: граница bcrypt (см. editor_password_policy_error)
const GUEST_NAME_MAX = 48;

function guests_read_body(): array
{
    $len = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
    if ($len > 65536) {
        respond(['ok' => false, 'error' => 'Слишком большой запрос.'], 413);
    }
    $raw = editor_request_raw_body(65537);
    if ($raw === '') {
        return [];
    }
    $data = json_decode($raw, true, 16);
    if (!is_array($data)) {
        respond(['ok' => false, 'error' => 'Некорректный JSON'], 400);
    }
    return $data;
}

function guests_clean_name(mixed $raw): string
{
    $s = trim((string)(is_scalar($raw) ? $raw : ''));
    if (function_exists('mb_substr')) {
        return mb_substr($s, 0, GUEST_NAME_MAX, 'UTF-8');
    }
    return substr($s, 0, GUEST_NAME_MAX);
}

function guests_valid_login(mixed $raw): string
{
    $login = strtolower(trim((string)(is_scalar($raw) ? $raw : '')));
    if (preg_match(GUEST_LOGIN_RE, $login) !== 1) {
        respond(['ok' => false, 'error' => 'Логин: 3–32 символа, латиница/цифры и - _ . (без пробелов).'], 400);
    }
    return $login;
}

function guests_valid_password(mixed $raw): string
{
    $password = (string)(is_scalar($raw) ? $raw : '');
    $error = editor_password_policy_error($password, GUEST_PASSWORD_MIN);
    if ($error !== null) {
        respond(['ok' => false, 'error' => $error], 400);
    }
    return $password;
}

/**
 * Индивидуальные лимиты аккаунта из тела запроса.
 * null — наследовать глобальные правила; массив — только заданные ключи.
 */
function guests_valid_limits(mixed $raw): ?array
{
    if ($raw === null || $raw === '') {
        return null;
    }
    if (!is_array($raw)) {
        respond(['ok' => false, 'error' => 'Лимиты: ожидается объект {max_pending, cooldown, max_operations} или null.'], 400);
    }
    $unknown = array_diff(array_keys($raw), ['max_pending', 'cooldown', 'max_operations']);
    if ($unknown) {
        respond(['ok' => false, 'error' => 'Неизвестные поля лимитов: ' . implode(', ', array_map('strval', $unknown))], 400);
    }
    foreach ($raw as $key => $value) {
        if ($value !== null && $value !== '' && !is_numeric($value)) {
            respond(['ok' => false, 'error' => "Лимит {$key}: ожидается число."], 400);
        }
    }
    return guest_limits_normalize($raw);
}

/**
 * Срок действия аккаунта: '' — бессрочно, дата YYYY-MM-DD — до конца дня UTC,
 * иначе полная дата-время ISO. Возвращает ISO-строку для хранения.
 */
function guests_valid_expires(mixed $raw): string
{
    $value = trim((string)(is_scalar($raw) ? $raw : ''));
    if ($value === '') {
        return '';
    }
    if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $value) === 1) {
        $ts = strtotime($value . 'T23:59:59Z');
    } else {
        $ts = strtotime($value);
    }
    if ($ts === false) {
        respond(['ok' => false, 'error' => 'Срок действия: укажите дату (ГГГГ-ММ-ДД) или оставьте пустым.'], 400);
    }
    return gmdate('Y-m-d\TH:i:s\Z', $ts);
}

/** Whitelist of editor tabs, returns valid keys only. */
function guests_valid_pages(mixed $raw): array
{
    $keys = editor_guest_page_keys();
    if (!is_array($raw)) {
        return [];
    }
    $pages = array_values(array_unique(array_map('strval', $raw)));
    return array_values(array_intersect($keys, $pages));
}

function guests_assert_login_free(array $cfg, string $login, string $exceptId = ''): void
{
    $existing = guest_user_by_login($cfg, $login);
    if ($existing !== null && $existing['id'] !== $exceptId) {
        respond(['ok' => false, 'error' => 'Такой логин уже занят другим гостевым аккаунтом.'], 409);
    }
}

function guests_pages_catalog(): array
{
    $out = [];
    foreach (editor_guest_page_catalog() as $key => $page) {
        $out[] = [
            'key' => $key,
            'title' => $page['title'],
            'resources' => $page['write'],
        ];
    }
    return $out;
}

try {
    if (in_array($action, ['create', 'update', 'password', 'revoke', 'delete'], true)) {
        require_same_origin_json_post();
    } elseif (strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET')) !== 'GET') {
        respond(['ok' => false, 'error' => 'Метод запроса не поддерживается'], 405);
    }

    if ($action === 'list') {
        $cfg = load_cfg();
        $guest = guest_cfg($cfg);
        $users = array_values(array_map('guest_user_public', guest_users($cfg)));
        usort($users, fn($a, $b) => strcmp($a['name'] ?: $a['login'], $b['name'] ?: $b['login']));
        respond([
            'ok' => true,
            'users' => $users,
            'pages' => guests_pages_catalog(),
            // A leftover shared password of the old single-guest mode is ignored;
            // the UI tells the administrator to create personal accounts.
            'legacy_password' => (string)($cfg['guest_password'] ?? '') !== '',
            'settings' => [
                'guest_enabled' => $guest['enabled'],
                'guest_allow_remote' => $guest['allow_remote'],
                'guest_max_pending' => $guest['max_pending'],
                'guest_submit_cooldown' => $guest['cooldown'],
                'guest_max_operations' => $guest['max_operations'],
                'accounts' => $guest['users_count'],
            ],
            'password_policy' => ['min' => GUEST_PASSWORD_MIN, 'max_bytes' => GUEST_PASSWORD_MAX],
        ]);
    }

    if ($action === 'create') {
        $body = guests_read_body();
        $login = guests_valid_login($body['login'] ?? '');
        $password = guests_valid_password($body['password'] ?? '');
        $name = guests_clean_name($body['name'] ?? '');
        $pages = guests_valid_pages($body['pages'] ?? []);
        $enabled = ($body['enabled'] ?? true) !== false;
        $limits = guests_valid_limits($body['limits'] ?? null);
        $expiresAt = guests_valid_expires($body['expires_at'] ?? '');
        $now = gmdate('Y-m-d\TH:i:s\Z');
        $newId = guest_new_id();

        if (store_guest_user_by_login($login) !== null) {
            respond(['ok' => false, 'error' => 'Такой логин уже занят другим гостевым аккаунтом.'], 409);
        }
        try {
            store_guest_user_insert([
                'id' => $newId,
                'login' => $login,
                'name' => $name !== '' ? $name : $login,
                'enabled' => $enabled,
                'pages' => $pages,
                'passwordHash' => password_hash($password, PASSWORD_DEFAULT),
                'sessionVersion' => 1,
                'createdAt' => $now,
                'updatedAt' => $now,
                'lastLoginAt' => '',
                'limits' => $limits,
                'expiresAt' => $expiresAt,
            ]);
        } catch (PDOException $e) {
            // Unique index on login: the same answer as the explicit check above.
            if (str_contains($e->getMessage(), 'idx_guest_login') || str_contains($e->getMessage(), 'UNIQUE')) {
                respond(['ok' => false, 'error' => 'Такой логин уже занят другим гостевым аккаунтом.'], 409);
            }
            throw $e;
        }
        $user = store_guest_user_get($newId);
        editor_audit('guests.create', '', ['guest_id' => $newId, 'login' => $login, 'pages' => $pages]);
        respond(['ok' => true, 'user' => $user ? guest_user_public(guest_user_normalize($user) ?? $user) : null]);
    }

    if ($action === 'update') {
        $body = guests_read_body();
        $id = strtolower(trim((string)($body['id'] ?? '')));
        $current = store_guest_user_get($id);
        if ($current === null) {
            respond(['ok' => false, 'error' => 'Гостевой аккаунт не найден.'], 404);
        }
        $fields = ['updatedAt' => gmdate('Y-m-d\TH:i:s\Z')];

        if (array_key_exists('login', $body)) {
            $login = guests_valid_login($body['login']);
            $other = store_guest_user_by_login($login);
            if ($other !== null && $other['id'] !== $id) {
                respond(['ok' => false, 'error' => 'Такой логин уже занят другим гостевым аккаунтом.'], 409);
            }
            $fields['login'] = $login;
        }
        if (array_key_exists('name', $body)) {
            $name = guests_clean_name($body['name']);
            $fields['name'] = $name !== '' ? $name : (string)($fields['login'] ?? $current['login']);
        }
        if (array_key_exists('pages', $body)) {
            $fields['pages'] = guests_valid_pages($body['pages']);
        }
        if (array_key_exists('enabled', $body)) {
            $enabled = (bool)$body['enabled'];
            $fields['enabled'] = $enabled;
            // Disabling an account immediately ends its active sessions.
            if (!$enabled) {
                $fields['sessionVersion'] = (int)$current['sessionVersion'] + 1;
            }
        }
        if (array_key_exists('limits', $body)) {
            $fields['limits'] = guests_valid_limits($body['limits']);
        }
        if (array_key_exists('expires_at', $body)) {
            $fields['expiresAt'] = guests_valid_expires($body['expires_at']);
        }
        store_guest_user_update($id, $fields);
        editor_audit('guests.update', '', ['guest_id' => $id, 'fields' => array_keys($fields)]);
        $user = store_guest_user_get($id);
        respond(['ok' => true, 'user' => $user ? guest_user_public(guest_user_normalize($user) ?? $user) : null]);
    }

    if ($action === 'password') {
        $body = guests_read_body();
        $id = strtolower(trim((string)($body['id'] ?? '')));
        $password = guests_valid_password($body['password'] ?? '');
        $current = store_guest_user_get($id);
        if ($current === null) {
            respond(['ok' => false, 'error' => 'Гостевой аккаунт не найден.'], 404);
        }
        store_guest_user_update($id, [
            'passwordHash' => password_hash($password, PASSWORD_DEFAULT),
            'sessionVersion' => (int)$current['sessionVersion'] + 1,
            'updatedAt' => gmdate('Y-m-d\TH:i:s\Z'),
        ]);
        editor_audit('guests.password', '', ['guest_id' => $id]);
        respond(['ok' => true]);
    }

    if ($action === 'revoke') {
        $body = guests_read_body();
        $id = strtolower(trim((string)($body['id'] ?? '')));
        $current = store_guest_user_get($id);
        if ($current === null) {
            respond(['ok' => false, 'error' => 'Гостевой аккаунт не найден.'], 404);
        }
        store_guest_user_update($id, ['sessionVersion' => (int)$current['sessionVersion'] + 1]);
        editor_audit('guests.revoke', '', ['guest_id' => $id]);
        respond(['ok' => true]);
    }

    if ($action === 'delete') {
        $body = guests_read_body();
        $id = strtolower(trim((string)($body['id'] ?? '')));
        if (!store_guest_user_delete($id)) {
            respond(['ok' => false, 'error' => 'Гостевой аккаунт не найден.'], 404);
        }
        editor_audit('guests.delete', '', ['guest_id' => $id]);
        respond(['ok' => true]);
    }

    respond(['ok' => false, 'error' => 'Неизвестное действие'], 400);
} catch (Throwable $e) {
    editor_internal_error($e);
}
