<?php
// Change requests API (guest → administrator).
// Endpoint: api/change-requests.php?action=...
//
// Guest actions (role guest):
//   config   (GET)  — limits & rules for the UI
//   preview  (POST) — {operations} → detailed analysis, nothing is stored
//   submit   (POST) — {nickname, message, operations} → stores a pending request
//   mine     (GET)  — own requests (+ unread replies)
//   get      (GET)  — id → own request details (marks replies as read)
//   messages (GET)  — id → thread of messages with the administrator
//   message  (POST) — {id, body} → reply in the thread
//   withdraw (POST) — {id} → withdraw own pending request
//
// Admin actions (role admin) — additionally:
//   list, get, stats, approve {id, comment, force}, reject {id, comment},
//   delete {id}, get_settings, save_settings, messages, message
//
// A guest never writes to the game DB: requests and their message threads live in
// the SQLite store (data/editor-studio.sqlite), and only cr_apply() (admin
// "approve") touches the game database.

declare(strict_types=1);

require_once __DIR__ . '/change-requests-lib.php';

$ROLE = bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN, EDITOR_ROLE_GUEST]);
$IS_ADMIN = $ROLE === EDITOR_ROLE_ADMIN;
$action = (string)($_GET['action'] ?? '');

guest_action_guard($ROLE, $action, ['config', 'preview', 'submit', 'mine', 'get', 'messages', 'message', 'withdraw']);

$POST_ACTIONS = ['preview', 'submit', 'withdraw', 'approve', 'reject', 'delete', 'save_settings', 'message'];

function cr_read_body(): array
{
    $len = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
    if ($len > CR_MAX_BODY_BYTES) {
        respond(['ok' => false, 'error' => 'Слишком большой запрос (максимум 2 МБ).'], 413);
    }
    $raw = editor_request_raw_body(CR_MAX_BODY_BYTES + 1);
    if ($raw === '') return [];
    if (strlen($raw) > CR_MAX_BODY_BYTES) {
        respond(['ok' => false, 'error' => 'Слишком большой запрос (максимум 2 МБ).'], 413);
    }
    $data = json_decode($raw, true, 64);
    if (!is_array($data)) {
        respond(['ok' => false, 'error' => 'Некорректный JSON'], 400);
    }
    return $data;
}

function cr_guest_hash_current(): string
{
    $key = editor_guest_key();
    if ($key === '') {
        respond(['ok' => false, 'error' => 'Требуется вход в Editor Studio'], 401);
    }
    return cr_guest_hash($key);
}

/** Owner check for guests: returns request or 404 (no information leak). */
function cr_load_visible(string $id, bool $admin): array
{
    $req = cr_load($id);
    if (!$req) respond(['ok' => false, 'error' => 'Запрос не найден'], 404);
    if (!$admin && !cr_guest_owns($req)) {
        respond(['ok' => false, 'error' => 'Запрос не найден'], 404);
    }
    return $req;
}

/** Light throttle for previews (they hit the DB with read queries). */
function cr_preview_throttle(): void
{
    $now = time();
    $win = $_SESSION['editor_studio_cr_preview'] ?? ['t' => $now, 'n' => 0];
    if (!is_array($win) || $now - (int)$win['t'] > 60) $win = ['t' => $now, 'n' => 0];
    $win['n'] = (int)$win['n'] + 1;
    $_SESSION['editor_studio_cr_preview'] = $win;
    if ($win['n'] > 30) {
        respond(['ok' => false, 'error' => 'Слишком много проверок подряд. Подождите минуту.'], 429);
    }
}

try {
    if ($action === '') respond(['ok' => false, 'error' => 'Не указано действие'], 400);
    if (in_array($action, $POST_ACTIONS, true)) {
        require_same_origin_json_post();
    } elseif (strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET')) !== 'GET') {
        respond(['ok' => false, 'error' => 'Метод запроса не поддерживается'], 405);
    }

    $guest = guest_cfg();

    // ── Shared ──────────────────────────────────────────────────────────────
    if ($action === 'config') {
        // A guest sees only its own tabs; an administrator sees everything.
        $allowedResources = $IS_ADMIN
            ? array_keys(editor_resources())
            : editor_guest_write_resources_current();
        $resources = [];
        foreach (editor_resources() as $key => $R) {
            if (empty($R['guest']) || !in_array($key, $allowedResources, true)) continue;
            $resources[$key] = [
                'title' => $R['title'], 'pk' => $R['pk'], 'label' => $R['label'] ?? null,
                'columns' => $R['columns'], 'json' => $R['json'] ?? [],
                'refs' => $R['refs'] ?? new stdClass(),
            ];
        }
        $account = $IS_ADMIN ? null : editor_guest_user();
        $unread = $IS_ADMIN
            ? cr_msg_unread_map('admin')
            : cr_msg_unread_map('guest', $account ? [(string)$account['id']] : []);
        respond([
            'ok' => true,
            'role' => $ROLE,
            'unread' => $unread['total'],
            'account' => $account ? [
                'id' => $account['id'],
                'login' => $account['login'],
                'name' => $account['name'],
                'pages' => $account['pages'],
            ] : null,
            'limits' => [
                'nickname' => ['min' => CR_NICK_MIN, 'max' => CR_NICK_MAX],
                'message' => ['min' => CR_MESSAGE_MIN, 'max' => CR_MESSAGE_MAX],
                'max_operations' => $guest['max_operations'],
                'max_pending' => $guest['max_pending'],
                'cooldown' => $guest['cooldown'],
            ],
            'temp_id' => ['base' => GUEST_TEMP_ID_BASE, 'max' => GUEST_TEMP_ID_MAX],
            'resources' => $resources,
        ]);
    }

    if ($action === 'preview') {
        cr_preview_throttle();
        $body = cr_read_body();
        $maxOps = $IS_ADMIN ? 1000 : $guest['max_operations'];
        $ops = cr_normalize_ops($body['operations'] ?? null, $maxOps);
        if (!$IS_ADMIN) {
            cr_assert_guest_resources($ops);
        }
        $db = db_connect();
        try {
            $analysis = cr_analyze($db, $ops);
        } finally {
            $db->close();
        }
        respond(['ok' => true, 'analysis' => cr_public_analysis($analysis)]);
    }

    if ($action === 'get') {
        $req = cr_load_visible((string)($_GET['id'] ?? ''), $IS_ADMIN);
        $view = cr_full_view($req, $IS_ADMIN);
        // Открытие запроса = прочтение ответов другой стороны.
        cr_msg_mark_read((string)$req['id'], $IS_ADMIN ? 'admin' : 'guest');
        $view['messages'] = cr_msg_list((string)$req['id'], $IS_ADMIN ? 'admin' : 'guest');
        $view['unread'] = 0;
        if ($IS_ADMIN && $req['status'] === 'pending') {
            // Live check against the current DB: did anything change since the guest created it?
            try {
                $db = db_connect();
                try {
                    $ops = $req['operations'] ?? [];
                    $view['conflicts'] = cr_detect_conflicts($db, $ops);
                    $live = cr_analyze($db, array_map(fn($o) => array_diff_key($o, ['before' => 1]), $ops));
                    $view['liveAnalysis'] = cr_public_analysis($live);
                } finally {
                    $db->close();
                }
            } catch (Throwable $e) {
                // Проверка «вживую» — вспомогательная: ошибку базы показываем
                // по-человечески, без имён таблиц и SQL.
                editor_log_error($e);
                $view['liveError'] = 'Не удалось проверить данные по базе. Попробуйте обновить страницу.';
            }
        }
        respond(['ok' => true, 'request' => $view]);
    }

    if ($action === 'messages') {
        $id = (string)($_GET['id'] ?? '');
        $req = cr_load_visible($id, $IS_ADMIN);
        $role = $IS_ADMIN ? 'admin' : 'guest';
        // Пока диалог открыт на экране, чужие сообщения считаются прочитанными.
        cr_msg_mark_read((string)$req['id'], $role);
        respond(['ok' => true, 'messages' => cr_msg_list((string)$req['id'], $role)]);
    }

    if ($action === 'message') {
        $body = cr_read_body();
        $id = (string)($body['id'] ?? '');
        $req = cr_load_visible($id, $IS_ADMIN);
        $text = cr_msg_validate($body['body'] ?? '');
        $role = $IS_ADMIN ? 'admin' : 'guest';

        // Простая защита от флуда: не чаще одного сообщения в 3 секунды.
        $account = $IS_ADMIN ? null : editor_guest_user();
        $authorId = $IS_ADMIN
            ? (string)($_SESSION['editor_studio_user'] ?? 'admin')
            : (string)($account['id'] ?? '');
        $authorName = $IS_ADMIN
            ? ($authorId !== '' ? $authorId : 'admin')
            : (string)($account['name'] ?? $req['author']['guestName'] ?? 'guest');
        $rateKey = 'cr-msg|' . $role . '|' . ($authorId !== '' ? $authorId : get_request_ip());
        $now = time();
        $last = store_rate_get($rateKey);
        if ($last > 0 && $last + 3 > $now) {
            $left = $last + 3 - $now;
            respond(['ok' => false, 'error' => "Слишком быстро — подождите {$left} сек."], 429);
        }

        $message = cr_with_lock(function () use ($req, $role, $authorId, $authorName, $text) {
            $message = cr_msg_add((string)$req['id'], $role, $authorId, $authorName, $text);
            // Автор сообщения уже видел диалог — его ответы помечаем прочитанными.
            cr_msg_mark_read((string)$req['id'], $role);
            // Запрос «оживает»: администратор видит, что появился новый ответ.
            $fresh = cr_load((string)$req['id']) ?? $req;
            $fresh['updatedAt'] = cr_now();
            cr_save($fresh);
            return $message;
        });
        store_rate_touch($rateKey, $now);

        respond(['ok' => true, 'message' => $message]);
    }

    // ── Guest ───────────────────────────────────────────────────────────────
    if ($action === 'submit') {
        if ($IS_ADMIN) respond(['ok' => false, 'error' => 'Администратор вносит изменения напрямую в редакторах.'], 400);
        $guestHash = cr_guest_hash_current();
        $body = cr_read_body();
        $nickname = cr_validate_nickname($body['nickname'] ?? '');
        $message = cr_validate_message($body['message'] ?? '');
        $ops = cr_normalize_ops($body['operations'] ?? null, $guest['max_operations']);
        cr_assert_guest_resources($ops);
        $account = editor_guest_user();
        if ($account === null) {
            respond(['ok' => false, 'error' => 'Гостевая сессия недействительна. Войдите заново.'], 401);
        }

        // Session cooldown (checked early; IP cooldown is checked under lock).
        $last = (int)($_SESSION['editor_studio_guest_last_submit'] ?? 0);
        if ($guest['cooldown'] > 0 && $last + $guest['cooldown'] > time()) {
            $left = $last + $guest['cooldown'] - time();
            respond(['ok' => false, 'error' => "Подождите {$left} сек. перед отправкой следующего запроса."], 429);
        }

        $db = db_connect();
        try {
            [$finalOps, $analysis] = cr_prepare_for_submit($db, $ops);
        } finally {
            $db->close();
        }

        $ip = get_request_ip();
        $req = cr_with_lock(function () use ($guest, $guestHash, $nickname, $message, $finalOps, $analysis, $ip, $account) {
            $all = cr_all();
            $pendingTotal = 0;
            $pendingMine = 0;
            foreach ($all as $r) {
                if (($r['status'] ?? '') !== 'pending') continue;
                $pendingTotal++;
                if (cr_guest_owns($r, $account)) $pendingMine++;
            }
            if ($pendingMine >= $guest['max_pending']) {
                throw new CrInputError("У вас уже {$pendingMine} запрос(ов) на рассмотрении (лимит {$guest['max_pending']}). Дождитесь решения администратора или отзовите запрос.", 429);
            }
            if ($pendingTotal >= CR_GLOBAL_PENDING_MAX) {
                throw new CrInputError('Очередь запросов переполнена. Попробуйте позже.', 429);
            }
            $left = cr_ip_cooldown_left($ip, $guest['cooldown'], true);
            if ($left > 0) {
                throw new CrInputError("Подождите {$left} сек. перед отправкой следующего запроса.", 429);
            }

            $now = cr_now();
            $req = [
                'id' => cr_new_id(),
                'version' => 1,
                'status' => 'pending',
                'createdAt' => $now,
                'updatedAt' => $now,
                'author' => [
                    'nickname' => $nickname,
                    'guestId' => $account['id'],
                    'guestLogin' => $account['login'],
                    'guestName' => $account['name'],
                    'guestHash' => $guestHash,
                    'ip' => $ip,
                    'userAgent' => cr_substr(cr_clean_text($_SERVER['HTTP_USER_AGENT'] ?? '', false), 200),
                ],
                'message' => $message,
                'operations' => $finalOps,
                'analysis' => cr_public_analysis($analysis),
                'review' => null,
            ];
            cr_save($req);
            return $req;
        });

        $_SESSION['editor_studio_guest_last_submit'] = time();
        respond(['ok' => true, 'request' => cr_list_item($req, false)]);
    }

    if ($action === 'mine') {
        if ($IS_ADMIN) respond(['ok' => true, 'rows' => []]);
        $account = editor_guest_user();
        if ($account === null) respond(['ok' => false, 'error' => 'Требуется вход в Editor Studio'], 401);
        $pending = [];
        $history = [];
        $unread = cr_msg_unread_map('guest', [(string)$account['id']]);
        foreach (cr_all() as $req) {
            if (!cr_guest_owns($req, $account)) continue;
            $item = cr_list_item($req, false, (int)($unread['by_request'][(string)$req['id']] ?? 0));
            // Pending requests are always returned first: the local draft reconciles against them.
            if (($req['status'] ?? '') === 'pending') $pending[] = $item;
            elseif (count($history) < 200) $history[] = $item;
            if (count($pending) >= 100 && count($history) >= 200) break;
        }
        respond(['ok' => true, 'rows' => array_merge($pending, $history)]);
    }

    if ($action === 'withdraw') {
        $body = cr_read_body();
        $id = (string)($body['id'] ?? '');
        $req = cr_with_lock(function () use ($id, $IS_ADMIN) {
            $req = cr_load_visible($id, $IS_ADMIN);
            if ($req['status'] !== 'pending') throw new CrInputError('Можно отозвать только запрос на рассмотрении.', 409);
            $req['status'] = 'withdrawn';
            $req['updatedAt'] = cr_now();
            cr_save($req);
            return $req;
        });
        respond(['ok' => true, 'request' => cr_list_item($req, $IS_ADMIN)]);
    }

    // ── Admin only below (guest_action_guard already rejected guests) ───────
    if (!$IS_ADMIN) respond(['ok' => false, 'error' => 'Действие доступно только администратору'], 403);

    if ($action === 'stats') {
        $counts = array_fill_keys(CR_STATUSES, 0);
        $byGuest = [];
        foreach (cr_all() as $req) {
            $s = (string)($req['status'] ?? '');
            if (isset($counts[$s])) $counts[$s]++;
            if ($s === 'pending') {
                $gid = (string)($req['author']['guestId'] ?? '');
                if ($gid !== '') $byGuest[$gid] = ($byGuest[$gid] ?? 0) + 1;
            }
        }
        $unread = cr_msg_unread_map('admin');
        respond([
            'ok' => true,
            'counts' => $counts,
            'unread' => $unread['total'],
            'unread_by_guest' => $unread['by_guest'] ?: new stdClass(),
            'by_guest' => $byGuest ?: new stdClass(),
            'guest_enabled' => $guest['enabled'] && $guest['users_count'] > 0,
            'guest_accounts' => $guest['users_count'],
        ]);
    }

    if ($action === 'list') {
        $status = (string)($_GET['status'] ?? 'pending');
        $q = function_exists('mb_strtolower') ? mb_strtolower(trim((string)($_GET['q'] ?? ''))) : strtolower(trim((string)($_GET['q'] ?? '')));
        $rows = [];
        $counts = array_fill_keys(CR_STATUSES, 0);
        $unread = cr_msg_unread_map('admin');
        foreach (cr_all() as $req) {
            $s = (string)($req['status'] ?? '');
            if (isset($counts[$s])) $counts[$s]++;
            if ($status !== 'all' && $s !== $status) continue;
            if ($q !== '') {
                $hay = implode(' ', [
                    $req['id'],
                    $req['author']['nickname'] ?? '',
                    $req['author']['guestName'] ?? '',
                    $req['author']['guestLogin'] ?? '',
                    $req['message'] ?? '',
                    implode(' ', array_keys($req['analysis']['summary']['resources'] ?? [])),
                ]);
                $hay = function_exists('mb_strtolower') ? mb_strtolower($hay) : strtolower($hay);
                if (!str_contains($hay, $q)) continue;
            }
            $rows[] = cr_list_item($req, true, (int)($unread['by_request'][(string)$req['id']] ?? 0));
            if (count($rows) >= 500) break;
        }
        respond(['ok' => true, 'rows' => $rows, 'counts' => $counts]);
    }

    if ($action === 'approve') {
        $body = cr_read_body();
        $id = (string)($body['id'] ?? '');
        $force = ($body['force'] ?? false) === true;
        $comment = cr_substr(cr_clean_text($body['comment'] ?? '', true), CR_COMMENT_MAX);
        $adminUser = (string)($_SESSION['editor_studio_user'] ?? 'admin');

        $result = cr_with_lock(function () use ($id, $force, $comment, $adminUser) {
            $req = cr_load_visible($id, true);
            if ($req['status'] !== 'pending') throw new CrInputError('Запрос уже обработан (статус: ' . $req['status'] . ').', 409);
            $db = db_connect();
            try {
                $res = cr_apply($db, $req, $force);
            } finally {
                $db->close();
            }
            $req['status'] = 'approved';
            $req['updatedAt'] = cr_now();
            $req['review'] = [
                'by' => $adminUser,
                'at' => cr_now(),
                'comment' => $comment,
                'force' => $force,
                'idMap' => $res['idMap'],
                'skipped' => $res['skipped'],
                'conflicts' => $res['conflicts'],
            ];
            try {
                cr_save($req);
            } catch (Throwable $e) {
                // The transaction is already committed: never let the admin retry
                // blindly (a repeated create would duplicate rows).
                throw new CrInputError(
                    'Изменения в базе данных применены, но запрос не удалось пометить как принятый. '
                    . 'Не применяйте его повторно — удалите запрос и проверьте данные вручную. (' . editor_safe_message($e) . ')',
                    500,
                    ['applied' => $res['applied']]
                );
            }
            return ['request' => $req, 'applied' => $res['applied'], 'skipped' => $res['skipped']];
        });

        respond([
            'ok' => true,
            'request' => cr_list_item($result['request'], true),
            'applied' => $result['applied'],
            'skipped' => $result['skipped'],
        ]);
    }

    if ($action === 'reject') {
        $body = cr_read_body();
        $id = (string)($body['id'] ?? '');
        $comment = cr_substr(cr_clean_text($body['comment'] ?? '', true), CR_COMMENT_MAX);
        $adminUser = (string)($_SESSION['editor_studio_user'] ?? 'admin');
        $req = cr_with_lock(function () use ($id, $comment, $adminUser) {
            $req = cr_load_visible($id, true);
            if ($req['status'] !== 'pending') throw new CrInputError('Запрос уже обработан (статус: ' . $req['status'] . ').', 409);
            $req['status'] = 'rejected';
            $req['updatedAt'] = cr_now();
            $req['review'] = ['by' => $adminUser, 'at' => cr_now(), 'comment' => $comment];
            cr_save($req);
            return $req;
        });
        respond(['ok' => true, 'request' => cr_list_item($req, true)]);
    }

    if ($action === 'delete') {
        $body = cr_read_body();
        $id = (string)($body['id'] ?? '');
        cr_with_lock(function () use ($id) {
            cr_load_visible($id, true);
            cr_delete($id);
        });
        respond(['ok' => true]);
    }

    if ($action === 'get_settings') {
        respond(['ok' => true, 'settings' => [
            'guest_enabled' => $guest['enabled'],
            'guest_allow_remote' => $guest['allow_remote'],
            'guest_max_pending' => $guest['max_pending'],
            'guest_submit_cooldown' => $guest['cooldown'],
            'guest_max_operations' => $guest['max_operations'],
            'guest_accounts' => $guest['users_count'],
        ]]);
    }

    if ($action === 'save_settings') {
        // Global switchboard only. Guest accounts (logins, passwords, tabs) are
        // managed in «Гостевые пользователи» (api/guests.php).
        $body = cr_read_body();
        $saved = mutate_cfg(function (array $cfg) use ($body) {
            $cfg['guest_enabled'] = (bool)($body['guest_enabled'] ?? false);
            $cfg['guest_allow_remote'] = (bool)($body['guest_allow_remote'] ?? false);
            $cfg['guest_max_pending'] = clamp_int($body['guest_max_pending'] ?? null, 1, 50, 5);
            $cfg['guest_submit_cooldown'] = clamp_int($body['guest_submit_cooldown'] ?? null, 0, 3600, 60);
            $cfg['guest_max_operations'] = clamp_int($body['guest_max_operations'] ?? null, 1, 500, 100);
            // Legacy single-guest password is no longer used.
            unset($cfg['guest_password']);
            return $cfg;
        });
        if (!$saved) respond(['ok' => false, 'error' => 'Не удалось сохранить настройки'], 500);
        respond(['ok' => true]);
    }

    respond(['ok' => false, 'error' => 'Неизвестное действие'], 400);
} catch (CrInputError $e) {
    respond(['ok' => false, 'error' => $e->getMessage()] + $e->extra, $e->httpCode);
} catch (Throwable $e) {
    editor_internal_error($e);
}
