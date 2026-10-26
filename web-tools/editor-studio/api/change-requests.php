<?php
// Change requests API (guest → administrator).
// Endpoint: api/change-requests.php?action=...
//
// Guest actions (role guest):
//   config    (GET)  — limits & rules for the UI (effective per-account limits)
//   preview   (POST) — {operations} → detailed analysis, nothing is stored
//   submit    (POST) — {nickname, message, operations, idempotency_key?} → stores a pending request
//   mine      (GET)  — own requests (+ unread replies), paginated (limit/offset)
//   get       (GET)  — id → own request details (side-effect free; see mark_read)
//   messages  (GET)  — id → thread of messages with the administrator (side-effect free)
//   mark_read (POST) — {id} → mark the other side's messages as read
//   message   (POST) — {id, body} → reply in the thread
//   revise    (POST) — {id, version, operations} → edit pending request (owner guest or admin)
//   withdraw  (POST) — {id} → withdraw own pending request
//
// Admin actions (role admin) — additionally:
//   list (paginated, resource filter), get, stats, approve {id, version, comment, force},
//   reject {id, comment}, delete {id}, bulk {ids, op: reject|delete, comment},
//   audit (GET, paginated), get_settings, save_settings, messages, message
//
// A guest never writes to the game DB: requests and their message threads live in
// the SQLite store (data/editor-studio.sqlite), and only cr_apply() (admin
// "approve") touches the game database.

declare(strict_types=1);

require_once __DIR__ . '/change-requests-lib.php';

$ROLE = bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN, EDITOR_ROLE_GUEST]);
$IS_ADMIN = $ROLE === EDITOR_ROLE_ADMIN;
$action = (string)($_GET['action'] ?? '');

guest_action_guard($ROLE, $action, ['config', 'preview', 'submit', 'mine', 'get', 'messages', 'mark_read', 'message', 'revise', 'withdraw']);

$POST_ACTIONS = ['preview', 'submit', 'revise', 'withdraw', 'approve', 'reject', 'delete', 'bulk', 'save_settings', 'message', 'mark_read'];

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

/** Идентичность для лимитов частоты: аккаунт (или администратор), не сессия. */
function cr_rate_identity(bool $isAdmin): string
{
    if ($isAdmin) {
        return 'admin|' . (string)($_SESSION['editor_studio_user'] ?? 'admin');
    }
    $account = editor_guest_user();
    return 'guest|' . (string)($account['id'] ?? get_request_ip());
}

/** Параметры постраничной навигации из запроса (с безопасными границами). */
function cr_pagination(int $defaultLimit = 100, int $maxLimit = 500): array
{
    $limit = clamp_int($_GET['limit'] ?? null, 1, $maxLimit, $defaultLimit);
    $offset = clamp_int($_GET['offset'] ?? null, 0, 1000000, 0);
    return [$limit, $offset];
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
        $effective = $IS_ADMIN
            ? ['max_pending' => $guest['max_pending'], 'cooldown' => $guest['cooldown'], 'max_operations' => $guest['max_operations'], 'custom' => false]
            : guest_effective_limits($account);
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
                'max_operations' => $effective['max_operations'],
                'max_pending' => $effective['max_pending'],
                'cooldown' => $effective['cooldown'],
                'custom' => $effective['custom'],
            ],
            'features' => ['idempotent_submit' => true, 'mark_read' => true, 'bulk' => $IS_ADMIN, 'audit' => $IS_ADMIN],
            'temp_id' => ['base' => GUEST_TEMP_ID_BASE, 'max' => GUEST_TEMP_ID_MAX],
            'resources' => $resources,
        ]);
    }

    if ($action === 'preview') {
        cr_preview_throttle($IS_ADMIN);
        $body = cr_read_body();
        $maxOps = $IS_ADMIN ? 1000 : guest_effective_limits(editor_guest_user())['max_operations'];
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
        // GET больше ничего не меняет: отметки о прочтении ставит отдельный
        // POST mark_read, когда диалог действительно открыт на экране.
        // Параметр peek принимается для совместимости и игнорируется.
        $view['messages'] = cr_msg_list((string)$req['id'], $IS_ADMIN ? 'admin' : 'guest');
        $view['unread'] = cr_msg_unread_for((string)$req['id'], $IS_ADMIN ? 'admin' : 'guest');
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
        respond([
            'ok' => true,
            'messages' => cr_msg_list((string)$req['id'], $role),
            'unread' => cr_msg_unread_for((string)$req['id'], $role),
        ]);
    }

    if ($action === 'mark_read') {
        $body = cr_read_body();
        $id = (string)($body['id'] ?? '');
        $req = cr_load_visible($id, $IS_ADMIN);
        $role = $IS_ADMIN ? 'admin' : 'guest';
        cr_msg_mark_read((string)$req['id'], $role);
        $account = $IS_ADMIN ? null : editor_guest_user();
        $total = $IS_ADMIN
            ? cr_msg_unread_map('admin')['total']
            : cr_msg_unread_map('guest', $account ? [(string)$account['id']] : [])['total'];
        respond(['ok' => true, 'unread' => 0, 'unread_total' => $total]);
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
            respond_rate_limited($last + 3 - $now, 'Слишком быстро — подождите несколько секунд.');
        }
        // Часовой бюджет сообщений и上限 длины одного диалога: переписка —
        // не чат, а защита от флуда не должна мешать обычной работе.
        editor_rate_limit_guard('cr-msg-hour', cr_rate_identity($IS_ADMIN), 120, 3600, 600, 'Слишком много сообщений за последний час. Попробуйте позже.');
        if (count(cr_msg_list((string)$req['id'], $role)) >= 1000) {
            respond(['ok' => false, 'error' => 'Диалог по этому запросу достиг предела (1000 сообщений).'], 409);
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
        $body = cr_read_body();
        $account = editor_guest_user();
        if ($account === null) {
            respond(['ok' => false, 'error' => 'Гостевая сессия недействительна. Войдите заново.'], 401);
        }
        $limits = guest_effective_limits($account);

        // Идемпотентность: повторная отправка того же черновика (двойной клик,
        // потерянный ответ сети, повтор после кулдауна) возвращает уже
        // созданный запрос, а не плодит дубликаты в очереди.
        $idempotencyKey = trim((string)($body['idempotency_key'] ?? ''));
        if ($idempotencyKey !== '' && preg_match('/^[A-Za-z0-9-]{16,80}$/', $idempotencyKey) !== 1) {
            respond(['ok' => false, 'error' => 'Некорректный ключ идемпотентности.'], 400);
        }
        if ($idempotencyKey !== '') {
            $existingId = store_submit_key_get(hash('sha256', $account['id'] . '|' . $idempotencyKey));
            if ($existingId !== null) {
                $existing = cr_load($existingId);
                if ($existing && cr_guest_owns($existing, $account)) {
                    respond(['ok' => true, 'duplicate' => true, 'request' => cr_list_item($existing, false)]);
                }
            }
        }

        $nickname = cr_validate_nickname($body['nickname'] ?? '');
        $message = cr_validate_message($body['message'] ?? '');
        $ops = cr_normalize_ops($body['operations'] ?? null, $limits['max_operations']);
        cr_assert_guest_resources($ops);

        // Session cooldown (checked early; IP cooldown is checked under lock).
        $last = (int)($_SESSION['editor_studio_guest_last_submit'] ?? 0);
        if ($limits['cooldown'] > 0 && $last + $limits['cooldown'] > time()) {
            respond_rate_limited($last + $limits['cooldown'] - time(), 'Подождите перед отправкой следующего запроса.');
        }

        $db = db_connect();
        try {
            [$finalOps, $analysis] = cr_prepare_for_submit($db, $ops);
        } finally {
            $db->close();
        }

        $ip = get_request_ip();
        $keyHash = $idempotencyKey !== '' ? hash('sha256', $account['id'] . '|' . $idempotencyKey) : '';
        $result = cr_with_lock(function () use ($limits, $nickname, $message, $finalOps, $analysis, $ip, $account, $keyHash) {
            // Повторная проверка под блокировкой: параллельный запрос мог
            // успеть создать запрос с тем же ключом первым.
            if ($keyHash !== '') {
                $existingId = store_submit_key_get($keyHash);
                if ($existingId !== null) {
                    $existing = cr_load($existingId);
                    if ($existing && cr_guest_owns($existing, $account)) {
                        return ['request' => $existing, 'duplicate' => true];
                    }
                }
            }
            $pendingMine = cr_pending_count((string)$account['id']);
            if ($pendingMine >= $limits['max_pending']) {
                throw new CrInputError("У вас уже {$pendingMine} запрос(ов) на рассмотрении (лимит {$limits['max_pending']}). Дождитесь решения администратора или отзовите запрос.", 429, ['code' => 'too_many_pending']);
            }
            if (cr_pending_count() >= CR_GLOBAL_PENDING_MAX) {
                throw new CrInputError('Очередь запросов переполнена. Попробуйте позже.', 429, ['code' => 'queue_full']);
            }
            $left = cr_ip_cooldown_left($ip, $limits['cooldown'], true);
            if ($left > 0) {
                throw new CrInputError('Подождите перед отправкой следующего запроса.', 429, ['code' => 'rate_limited', 'retry_after' => $left]);
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
                    'ip' => $ip,
                    'userAgent' => cr_substr(cr_clean_text($_SERVER['HTTP_USER_AGENT'] ?? '', false), 200),
                ],
                'message' => $message,
                'operations' => $finalOps,
                'analysis' => cr_public_analysis($analysis),
                'review' => null,
            ];
            cr_save($req);
            if ($keyHash !== '') {
                store_submit_key_put($keyHash, (string)$req['id'], (string)$account['id']);
                store_submit_keys_prune();
            }
            return ['request' => $req, 'duplicate' => false];
        });

        $_SESSION['editor_studio_guest_last_submit'] = time();
        if (!$result['duplicate']) {
            editor_audit('cr.submit', (string)$result['request']['id'], [
                'operations' => count($finalOps),
                'nickname' => $nickname,
            ]);
        }
        respond(['ok' => true, 'duplicate' => $result['duplicate'], 'request' => cr_list_item($result['request'], false)]);
    }

    if ($action === 'mine') {
        if ($IS_ADMIN) respond(['ok' => true, 'rows' => [], 'total' => 0]);
        $account = editor_guest_user();
        if ($account === null) respond(['ok' => false, 'error' => 'Требуется вход в Editor Studio'], 401);
        [$limit, $offset] = cr_pagination(300, 500);
        $unread = cr_msg_unread_map('guest', [(string)$account['id']]);
        // Pending requests are always returned first: the local draft reconciles against them.
        $list = cr_query_list([
            'guest_id' => (string)$account['id'],
            'status' => (string)($_GET['status'] ?? 'all'),
            'pending_first' => true,
        ], $limit, $offset);
        $rows = array_map(
            fn($req) => cr_list_item($req, false, (int)($unread['by_request'][(string)$req['id']] ?? 0)),
            $list['rows']
        );
        respond(['ok' => true, 'rows' => $rows, 'total' => $list['total'], 'limit' => $limit, 'offset' => $offset]);
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
        editor_audit('cr.withdraw', (string)$req['id']);
        respond(['ok' => true, 'request' => cr_list_item($req, $IS_ADMIN)]);
    }

    if ($action === 'revise') {
        $body = cr_read_body();
        $id = (string)($body['id'] ?? '');
        $version = (int)($body['version'] ?? 0);
        $actor = $IS_ADMIN ? null : editor_guest_user();
        if (!$IS_ADMIN && $actor === null) {
            respond(['ok' => false, 'error' => 'Гостевая сессия недействительна. Войдите заново.'], 401);
        }
        $actorRole = $IS_ADMIN ? 'admin' : 'guest';
        $actorId = $IS_ADMIN
            ? (string)($_SESSION['editor_studio_user'] ?? 'admin')
            : (string)$actor['id'];
        $actorName = $IS_ADMIN ? $actorId : (string)$actor['name'];

        $req = cr_with_lock(function () use ($id, $version, $body, $IS_ADMIN, $guest, $actorRole, $actorId, $actorName) {
            $req = cr_load_visible($id, $IS_ADMIN);
            if ($req['status'] !== 'pending') {
                throw new CrInputError('Редактировать можно только запрос на рассмотрении.', 409, ['code' => 'request_not_pending']);
            }
            $currentVersion = max(1, (int)($req['version'] ?? 1));
            if ($version !== $currentVersion) {
                throw new CrInputError('Запрос уже изменён другой стороной. Обновите его перед редактированием.', 409, [
                    'code' => 'version_conflict', 'version' => $currentVersion,
                ]);
            }

            $rawOps = $body['operations'] ?? null;
            $ops = cr_normalize_ops($rawOps, $IS_ADMIN ? 1000 : guest_effective_limits(editor_guest_user())['max_operations']);
            if (!$IS_ADMIN) cr_assert_guest_resources($ops);
            $db = db_connect();
            try {
                [$finalOps, $analysis] = cr_prepare_for_submit($db, $ops);
            } finally {
                $db->close();
            }

            $now = cr_now();
            $nextVersion = $currentVersion + 1;
            $req['operations'] = $finalOps;
            $req['analysis'] = cr_public_analysis($analysis);
            $req['version'] = $nextVersion;
            $req['updatedAt'] = $now;
            $req['lastRevision'] = [
                'version' => $nextVersion, 'role' => $actorRole,
                'by' => $actorName, 'at' => $now,
            ];
            $req['revisions'] = array_slice(array_merge($req['revisions'] ?? [], [[
                'version' => $nextVersion, 'role' => $actorRole, 'by' => $actorName, 'at' => $now,
                'operations' => count($finalOps), 'fields' => (int)($req['analysis']['summary']['fields'] ?? 0),
            ]]), -20);
            cr_save($req);
            cr_msg_add($id, $actorRole, $actorId, $actorName,
                "Обновлён состав запроса: версия {$nextVersion}, операций: " . count($finalOps)
                . ', изменённых полей: ' . (int)($req['analysis']['summary']['fields'] ?? 0) . '.');
            return $req;
        });
        editor_audit('cr.revise', (string)$req['id'], ['version' => (int)($req['version'] ?? 0)]);
        respond(['ok' => true, 'request' => cr_full_view($req, $IS_ADMIN)]);
    }

    // ── Admin only below (guest_action_guard already rejected guests) ───────
    if (!$IS_ADMIN) respond(['ok' => false, 'error' => 'Действие доступно только администратору'], 403);

    if ($action === 'stats') {
        $counts = cr_counts_by_status();
        $byGuest = cr_pending_by_guest();
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
        [$limit, $offset] = cr_pagination(100, 500);
        $unread = cr_msg_unread_map('admin');
        $list = cr_query_list([
            'status' => (string)($_GET['status'] ?? 'pending'),
            'q' => trim((string)($_GET['q'] ?? '')),
            'resource' => trim((string)($_GET['resource'] ?? '')),
            'guest_id' => trim((string)($_GET['guest'] ?? '')),
        ], $limit, $offset);
        $rows = array_map(
            fn($req) => cr_list_item($req, true, (int)($unread['by_request'][(string)$req['id']] ?? 0)),
            $list['rows']
        );
        respond(['ok' => true, 'rows' => $rows, 'counts' => cr_counts_by_status(),
            'total' => $list['total'], 'limit' => $limit, 'offset' => $offset]);
    }

    if ($action === 'approve') {
        $body = cr_read_body();
        $id = (string)($body['id'] ?? '');
        $force = ($body['force'] ?? false) === true;
        $version = (int)($body['version'] ?? 0);
        $comment = cr_substr(cr_clean_text($body['comment'] ?? '', true), CR_COMMENT_MAX);
        // Принудительное применение поверх изменившихся данных — только с
        // объяснением: оно останется у гостя и в журнале аудита.
        if ($force && $comment === '') {
            respond(['ok' => false, 'error' => 'При принудительном применении укажите причину в комментарии для гостя.', 'code' => 'force_comment_required'], 400);
        }
        $adminUser = (string)($_SESSION['editor_studio_user'] ?? 'admin');

        $result = cr_with_lock(function () use ($id, $force, $version, $comment, $adminUser) {
            $req = cr_load_visible($id, true);
            if ($req['status'] !== 'pending') throw new CrInputError('Запрос уже обработан (статус: ' . $req['status'] . ').', 409);
            $currentVersion = max(1, (int)($req['version'] ?? 1));
            if ($version !== $currentVersion) {
                throw new CrInputError('Запрос изменился после открытия. Проверьте последнюю версию перед принятием.', 409, [
                    'code' => 'version_conflict', 'version' => $currentVersion,
                ]);
            }
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
                'version' => $currentVersion,
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

        editor_audit('cr.approve', $id, [
            'applied' => count($result['applied']), 'skipped' => count($result['skipped']), 'force' => $force,
        ]);
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
        editor_audit('cr.reject', $id);
        respond(['ok' => true, 'request' => cr_list_item($req, true)]);
    }

    if ($action === 'delete') {
        $body = cr_read_body();
        $id = (string)($body['id'] ?? '');
        cr_with_lock(function () use ($id) {
            cr_load_visible($id, true);
            cr_delete($id);
        });
        editor_audit('cr.delete', $id);
        respond(['ok' => true]);
    }

    if ($action === 'bulk') {
        // Массовое отклонение/удаление хвоста очереди. Одобрения здесь нет
        // намеренно: каждое применение требует проверки конфликтов.
        $body = cr_read_body();
        $op = (string)($body['op'] ?? '');
        if (!in_array($op, ['reject', 'delete'], true)) {
            respond(['ok' => false, 'error' => 'Поддерживаются только массовые операции reject и delete.'], 400);
        }
        $ids = $body['ids'] ?? null;
        if (!is_array($ids) || $ids === [] || count($ids) > 50) {
            respond(['ok' => false, 'error' => 'Укажите от 1 до 50 запросов.'], 400);
        }
        $comment = cr_substr(cr_clean_text($body['comment'] ?? '', true), CR_COMMENT_MAX);
        $adminUser = (string)($_SESSION['editor_studio_user'] ?? 'admin');
        $results = cr_with_lock(function () use ($ids, $op, $comment, $adminUser) {
            $out = [];
            foreach ($ids as $rawId) {
                $id = (string)$rawId;
                try {
                    $req = cr_load($id);
                    if (!$req) throw new CrInputError('Запрос не найден', 404);
                    if ($op === 'reject') {
                        if ($req['status'] !== 'pending') throw new CrInputError('Запрос уже обработан (статус: ' . $req['status'] . ').', 409);
                        $req['status'] = 'rejected';
                        $req['updatedAt'] = cr_now();
                        $req['review'] = ['by' => $adminUser, 'at' => cr_now(), 'comment' => $comment];
                        cr_save($req);
                    } else {
                        cr_delete($id);
                    }
                    $out[] = ['id' => $id, 'ok' => true];
                } catch (Throwable $e) {
                    $out[] = ['id' => $id, 'ok' => false, 'error' => editor_safe_message($e)];
                }
            }
            return $out;
        });
        $done = count(array_filter($results, fn($r) => $r['ok']));
        editor_audit('cr.bulk_' . $op, '', ['total' => count($results), 'done' => $done]);
        respond(['ok' => true, 'results' => $results, 'done' => $done]);
    }

    if ($action === 'audit') {
        [$limit, $offset] = cr_pagination(100, 500);
        $list = store_audit_list([
            'q' => trim((string)($_GET['q'] ?? '')),
            'action' => trim((string)($_GET['audit_action'] ?? '')),
            'request_id' => trim((string)($_GET['request'] ?? '')),
        ], $limit, $offset);
        respond(['ok' => true, 'rows' => $list['rows'], 'total' => $list['total'], 'limit' => $limit, 'offset' => $offset]);
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
            // Отсутствующее поле тела запроса не сбрасывает настройку,
            // а сохраняет текущее значение из БД.
            $cfg['guest_enabled'] = (bool)($body['guest_enabled'] ?? ($cfg['guest_enabled'] ?? false));
            $cfg['guest_allow_remote'] = (bool)($body['guest_allow_remote'] ?? ($cfg['guest_allow_remote'] ?? false));
            $cfg['guest_max_pending'] = clamp_int($body['guest_max_pending'] ?? null, 1, 50,
                clamp_int($cfg['guest_max_pending'] ?? null, 1, 50, 5));
            $cfg['guest_submit_cooldown'] = clamp_int($body['guest_submit_cooldown'] ?? null, 0, 3600,
                clamp_int($cfg['guest_submit_cooldown'] ?? null, 0, 3600, 60));
            $cfg['guest_max_operations'] = clamp_int($body['guest_max_operations'] ?? null, 1, 500,
                clamp_int($cfg['guest_max_operations'] ?? null, 1, 500, 100));
            // Legacy single-guest password is no longer used.
            unset($cfg['guest_password']);
            return $cfg;
        });
        if (!$saved) respond(['ok' => false, 'error' => 'Не удалось сохранить настройки'], 500);
        editor_audit('cr.settings', '', [
            'guest_enabled' => (bool)($body['guest_enabled'] ?? false),
            'guest_allow_remote' => (bool)($body['guest_allow_remote'] ?? false),
        ]);
        respond(['ok' => true]);
    }

    respond(['ok' => false, 'error' => 'Неизвестное действие'], 400);
} catch (CrInputError $e) {
    if ($e->httpCode === 429 && !empty($e->extra['retry_after'])) {
        header('Retry-After: ' . (int)$e->extra['retry_after']);
    }
    respond(['ok' => false, 'error' => $e->getMessage()] + $e->extra, $e->httpCode);
} catch (Throwable $e) {
    editor_internal_error($e);
}
