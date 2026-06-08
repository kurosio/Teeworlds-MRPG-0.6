<?php
// Account management API for Editor Studio.
// Endpoint: api/accounts.php

declare(strict_types=1);

require_once __DIR__ . '/db-core.php';

bootstrap_editor_api(true);

const ACCOUNT_PASSWORD_MIN = 4;
const ACCOUNT_PASSWORD_MAX = 12;
const ACCOUNT_LOGIN_MIN = 4;
const ACCOUNT_LOGIN_MAX = 12;
const ACCOUNT_SALT_LENGTH = 24;
const ACCOUNT_RESET_PASSWORD_LENGTH = 10;

function account_hash_password(string $password, string $salt): string
{
    return hash('sha256', $salt . $password . $salt);
}

function account_random_string(int $length): string
{
    $alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789';
    $max = strlen($alphabet) - 1;
    $out = '';
    for ($i = 0; $i < $length; $i++) {
        $out .= $alphabet[random_int(0, $max)];
    }
    return $out;
}

function validate_account_id(mixed $value): int
{
    $id = (string)($value ?? '');
    if ($id === '' || !ctype_digit($id) || (int)$id <= 0) {
        respond(['ok' => false, 'error' => 'Invalid account id'], 400);
    }
    return (int)$id;
}

function validate_password(string $password): void
{
    $length = strlen($password);
    if ($length < ACCOUNT_PASSWORD_MIN || $length > ACCOUNT_PASSWORD_MAX) {
        respond([
            'ok' => false,
            'error' => 'Пароль должен содержать от ' . ACCOUNT_PASSWORD_MIN . ' до ' . ACCOUNT_PASSWORD_MAX . ' символов.',
        ], 400);
    }
}

function validate_login(string $login): void
{
    $length = strlen($login);
    if ($length < ACCOUNT_LOGIN_MIN || $length > ACCOUNT_LOGIN_MAX) {
        respond([
            'ok' => false,
            'error' => 'Логин должен содержать от ' . ACCOUNT_LOGIN_MIN . ' до ' . ACCOUNT_LOGIN_MAX . ' символов.',
        ], 400);
    }
}

function ensure_login_available(mysqli $mysqli, string $login, int $accountId): void
{
    $stmt = $mysqli->prepare('SELECT ID FROM tw_accounts WHERE Username = ? AND ID != ? LIMIT 1');
    if ($stmt === false) {
        throw new RuntimeException('Prepare failed');
    }
    $stmt->bind_param('si', $login, $accountId);
    $stmt->execute();
    $res = $stmt->get_result();
    $row = $res ? $res->fetch_assoc() : null;
    $stmt->close();

    if ($row) {
        respond(['ok' => false, 'error' => 'Этот логин уже используется другим аккаунтом.'], 409);
    }
}

function ensure_account_exists(mysqli $mysqli, int $accountId): array
{
    $stmt = $mysqli->prepare(
        'SELECT a.ID, a.Username, a.Password, d.Nick '
        . 'FROM tw_accounts a LEFT JOIN tw_accounts_data d ON d.ID = a.ID '
        . 'WHERE a.ID = ? LIMIT 1'
    );
    if ($stmt === false) {
        throw new RuntimeException('Prepare failed');
    }
    $stmt->bind_param('i', $accountId);
    $stmt->execute();
    $res = $stmt->get_result();
    $row = $res ? $res->fetch_assoc() : null;
    $stmt->close();

    if (!$row) {
        respond(['ok' => false, 'error' => 'Аккаунт не найден.'], 404);
    }
    return $row;
}

function update_account_password(mysqli $mysqli, int $accountId, string $password): void
{
    validate_password($password);
    $salt = account_random_string(ACCOUNT_SALT_LENGTH);
    $hash = account_hash_password($password, $salt);

    $stmt = $mysqli->prepare('UPDATE tw_accounts SET Password = ?, PasswordSalt = ? WHERE ID = ? LIMIT 1');
    if ($stmt === false) {
        throw new RuntimeException('Prepare failed');
    }
    $stmt->bind_param('ssi', $hash, $salt, $accountId);
    $stmt->execute();
    $stmt->close();
}

$action = (string)($_GET['action'] ?? '');

try {
    if ($action === '') {
        respond(['ok' => false, 'error' => 'Missing action'], 400);
    }

    $mysqli = db_connect();

    if ($action === 'list') {
        $search = trim((string)($_GET['search'] ?? ''));
        $limit = max(1, min(200, (int)($_GET['limit'] ?? 50)));
        $fetchLimit = min(201, $limit + 1);

        if ($search === '') {
            $stmt = $mysqli->prepare(
                'SELECT a.ID, a.Username, a.Password, a.TimeoutCode, d.Nick '
                . 'FROM tw_accounts a LEFT JOIN tw_accounts_data d ON d.ID = a.ID '
                . 'ORDER BY a.ID DESC LIMIT ?'
            );
            if ($stmt === false) {
                throw new RuntimeException('Prepare failed');
            }
            $stmt->bind_param('i', $fetchLimit);
        } else {
            $like = '%' . $search . '%';
            if (ctype_digit($search)) {
                $id = (int)$search;
                $stmt = $mysqli->prepare(
                    'SELECT a.ID, a.Username, a.Password, a.TimeoutCode, d.Nick '
                    . 'FROM tw_accounts a LEFT JOIN tw_accounts_data d ON d.ID = a.ID '
                    . 'WHERE a.ID = ? OR a.Username LIKE ? OR d.Nick LIKE ? '
                    . 'ORDER BY a.ID DESC LIMIT ?'
                );
                if ($stmt === false) {
                    throw new RuntimeException('Prepare failed');
                }
                $stmt->bind_param('issi', $id, $like, $like, $fetchLimit);
            } else {
                $stmt = $mysqli->prepare(
                    'SELECT a.ID, a.Username, a.Password, a.TimeoutCode, d.Nick '
                    . 'FROM tw_accounts a LEFT JOIN tw_accounts_data d ON d.ID = a.ID '
                    . 'WHERE a.Username LIKE ? OR d.Nick LIKE ? '
                    . 'ORDER BY a.ID DESC LIMIT ?'
                );
                if ($stmt === false) {
                    throw new RuntimeException('Prepare failed');
                }
                $stmt->bind_param('ssi', $like, $like, $fetchLimit);
            }
        }

        $stmt->execute();
        $res = $stmt->get_result();
        $rows = [];
        while ($row = $res->fetch_assoc()) {
            $rows[] = [
                'ID' => (int)$row['ID'],
                'Username' => (string)($row['Username'] ?? ''),
                'Nick' => (string)($row['Nick'] ?? ''),
                'Password' => (string)($row['Password'] ?? ''),
                'HasTimeoutCode' => ((string)($row['TimeoutCode'] ?? '') !== ''),
            ];
        }
        $stmt->close();
        $hasMore = count($rows) > $limit;
        if ($hasMore) {
            $rows = array_slice($rows, 0, $limit);
        }
        $mysqli->close();
        respond(['ok' => true, 'rows' => $rows, 'has_more' => $hasMore]);
    }

    if ($action === 'change_password') {
        $body = read_json_body();
        $accountId = validate_account_id($body['account_id'] ?? null);
        $password = (string)($body['password'] ?? '');
        $account = ensure_account_exists($mysqli, $accountId);
        update_account_password($mysqli, $accountId, $password);
        $mysqli->close();
        respond(['ok' => true, 'account' => $account]);
    }

    if ($action === 'change_login') {
        $body = read_json_body();
        $accountId = validate_account_id($body['account_id'] ?? null);
        $login = trim((string)($body['login'] ?? ''));
        $account = ensure_account_exists($mysqli, $accountId);
        validate_login($login);
        ensure_login_available($mysqli, $login, $accountId);

        $stmt = $mysqli->prepare('UPDATE tw_accounts SET Username = ? WHERE ID = ? LIMIT 1');
        if ($stmt === false) {
            throw new RuntimeException('Prepare failed');
        }
        $stmt->bind_param('si', $login, $accountId);
        $stmt->execute();
        $stmt->close();
        $mysqli->close();
        $account['Username'] = $login;
        respond(['ok' => true, 'account' => $account]);
    }

    if ($action === 'reset_password') {
        $body = read_json_body();
        $accountId = validate_account_id($body['account_id'] ?? null);
        $account = ensure_account_exists($mysqli, $accountId);
        $password = account_random_string(ACCOUNT_RESET_PASSWORD_LENGTH);
        update_account_password($mysqli, $accountId, $password);
        $mysqli->close();
        respond(['ok' => true, 'account' => $account, 'password' => $password]);
    }

    if ($action === 'reset_timeout_code') {
        $body = read_json_body();
        $accountId = validate_account_id($body['account_id'] ?? null);
        $account = ensure_account_exists($mysqli, $accountId);
        $stmt = $mysqli->prepare("UPDATE tw_accounts SET TimeoutCode = '' WHERE ID = ? LIMIT 1");
        if ($stmt === false) {
            throw new RuntimeException('Prepare failed');
        }
        $stmt->bind_param('i', $accountId);
        $stmt->execute();
        $stmt->close();
        $mysqli->close();
        respond(['ok' => true, 'account' => $account]);
    }

    $mysqli->close();
    respond(['ok' => false, 'error' => 'Unknown action'], 400);
} catch (Throwable $e) {
    respond(['ok' => false, 'error' => $e->getMessage()], 500);
}
