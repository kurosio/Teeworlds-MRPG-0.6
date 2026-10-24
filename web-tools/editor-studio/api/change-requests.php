<?php
// Change requests API (guest → administrator).
// Endpoint: api/change-requests.php?action=...
//
// Guest actions (role guest):
//   config   (GET)  — limits & rules for the UI
//   preview  (POST) — {operations} → detailed analysis, nothing is stored
//   submit   (POST) — {nickname, message, operations} → stores a pending request
//   mine     (GET)  — own requests
//   get      (GET)  — id → own request details
//   withdraw (POST) — {id} → withdraw own pending request
//
// Admin actions (role admin) — additionally:
//   list, get, stats, approve {id, comment, force}, reject {id, comment},
//   delete {id}, get_settings, save_settings
//
// A guest never writes to the game DB: requests are stored in data/change-requests/
// and only cr_apply() (admin "approve") touches the database.

declare(strict_types=1);

require_once __DIR__ . '/change-requests-lib.php';

$ROLE = bootstrap_editor_api_roles([EDITOR_ROLE_ADMIN, EDITOR_ROLE_GUEST]);
$IS_ADMIN = $ROLE === EDITOR_ROLE_ADMIN;
$action = (string)($_GET['action'] ?? '');

guest_action_guard($ROLE, $action, ['config', 'preview', 'submit', 'mine', 'get', 'withdraw']);

$POST_ACTIONS = ['preview', 'submit', 'withdraw', 'approve', 'reject', 'delete', 'save_settings'];

function cr_read_body(): array
{
    $len = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
    if ($len > CR_MAX_BODY_BYTES) {
        respond(['ok' => false, 'error' => 'Слишком большой запрос (максимум 2 МБ).'], 413);
    }
    $raw = file_get_contents('php://input', false, null, 0, CR_MAX_BODY_BYTES + 1);
    if ($raw === false || $raw === '') return [];
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
        respond(['ok' => false, 'error' => 'Unauthorized'], 401);
    }
    return cr_guest_hash($key);
}

/** Owner check for guests: returns request or 404 (no information leak). */
function cr_load_visible(string $id, bool $admin): array
{
    $req = cr_load($id);
    if (!$req) respond(['ok' => false, 'error' => 'Запрос не найден'], 404);
    if (!$admin && !hash_equals((string)($req['author']['guestHash'] ?? ''), cr_guest_hash_current())) {
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
    if ($action === '') respond(['ok' => false, 'error' => 'Missing action'], 400);
    if (in_array($action, $POST_ACTIONS, true)) {
        require_same_origin_json_post();
    } elseif (strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET')) !== 'GET') {
        respond(['ok' => false, 'error' => 'Method not allowed'], 405);
    }

    $guest = guest_cfg();

    // ── Shared ──────────────────────────────────────────────────────────────
    if ($action === 'config') {
        $resources = [];
        foreach (editor_resources() as $key => $R) {
            if (empty($R['guest'])) continue;
            $resources[$key] = [
                'title' => $R['title'], 'pk' => $R['pk'], 'label' => $R['label'] ?? null,
                'columns' => $R['columns'], 'json' => $R['json'] ?? [],
                'refs' => $R['refs'] ?? new stdClass(),
            ];
        }
        respond([
            'ok' => true,
            'role' => $ROLE,
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
                $view['liveError'] = $e->getMessage();
            }
        }
        respond(['ok' => true, 'request' => $view]);
    }

    // ── Guest ───────────────────────────────────────────────────────────────
    if ($action === 'submit') {
        if ($IS_ADMIN) respond(['ok' => false, 'error' => 'Администратор вносит изменения напрямую в редакторах.'], 400);
        $guestHash = cr_guest_hash_current();
        $body = cr_read_body();
        $nickname = cr_validate_nickname($body['nickname'] ?? '');
        $message = cr_validate_message($body['message'] ?? '');
        $ops = cr_normalize_ops($body['operations'] ?? null, $guest['max_operations']);

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
        $req = cr_with_lock(function () use ($guest, $guestHash, $nickname, $message, $finalOps, $analysis, $ip) {
            $all = cr_all();
            $pendingTotal = 0;
            $pendingMine = 0;
            foreach ($all as $r) {
                if (($r['status'] ?? '') !== 'pending') continue;
                $pendingTotal++;
                if (hash_equals((string)($r['author']['guestHash'] ?? ''), $guestHash)) $pendingMine++;
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
        $guestHash = cr_guest_hash_current();
        $rows = [];
        foreach (cr_all() as $req) {
            if (!hash_equals((string)($req['author']['guestHash'] ?? ''), $guestHash)) continue;
            $rows[] = cr_list_item($req, false);
            if (count($rows) >= 100) break;
        }
        respond(['ok' => true, 'rows' => $rows]);
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
    if (!$IS_ADMIN) respond(['ok' => false, 'error' => 'Forbidden'], 403);

    if ($action === 'stats') {
        $counts = array_fill_keys(CR_STATUSES, 0);
        foreach (cr_all() as $req) {
            $s = (string)($req['status'] ?? '');
            if (isset($counts[$s])) $counts[$s]++;
        }
        respond(['ok' => true, 'counts' => $counts, 'guest_enabled' => $guest['enabled']]);
    }

    if ($action === 'list') {
        $status = (string)($_GET['status'] ?? 'pending');
        $q = function_exists('mb_strtolower') ? mb_strtolower(trim((string)($_GET['q'] ?? ''))) : strtolower(trim((string)($_GET['q'] ?? '')));
        $rows = [];
        $counts = array_fill_keys(CR_STATUSES, 0);
        foreach (cr_all() as $req) {
            $s = (string)($req['status'] ?? '');
            if (isset($counts[$s])) $counts[$s]++;
            if ($status !== 'all' && $s !== $status) continue;
            if ($q !== '') {
                $hay = implode(' ', [$req['id'], $req['author']['nickname'] ?? '', $req['message'] ?? '', implode(' ', array_keys($req['analysis']['summary']['resources'] ?? []))]);
                $hay = function_exists('mb_strtolower') ? mb_strtolower($hay) : strtolower($hay);
                if (!str_contains($hay, $q)) continue;
            }
            $rows[] = cr_list_item($req, true);
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
            cr_save($req);
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
            cr_delete_file($id);
        });
        respond(['ok' => true]);
    }

    if ($action === 'get_settings') {
        respond(['ok' => true, 'settings' => [
            'guest_enabled' => $guest['enabled'],
            'guest_password_set' => $guest['password'] !== '',
            'guest_allow_remote' => $guest['allow_remote'],
            'guest_max_pending' => $guest['max_pending'],
            'guest_submit_cooldown' => $guest['cooldown'],
            'guest_max_operations' => $guest['max_operations'],
        ]]);
    }

    if ($action === 'save_settings') {
        $body = cr_read_body();
        $cfg = load_cfg();
        $cfg['guest_enabled'] = (bool)($body['guest_enabled'] ?? false);
        $cfg['guest_allow_remote'] = (bool)($body['guest_allow_remote'] ?? false);
        $cfg['guest_max_pending'] = clamp_int($body['guest_max_pending'] ?? null, 1, 50, 5);
        $cfg['guest_submit_cooldown'] = clamp_int($body['guest_submit_cooldown'] ?? null, 0, 3600, 60);
        $cfg['guest_max_operations'] = clamp_int($body['guest_max_operations'] ?? null, 1, 500, 100);
        if (($body['guest_password_clear'] ?? false) === true) {
            $cfg['guest_password'] = '';
        } elseif (is_string($body['guest_password'] ?? null) && $body['guest_password'] !== '') {
            $pw = (string)$body['guest_password'];
            if (strlen($pw) < 4 || strlen($pw) > 128) {
                respond(['ok' => false, 'error' => 'Гостевой пароль: от 4 до 128 символов.'], 400);
            }
            $cfg['guest_password'] = password_hash($pw, PASSWORD_DEFAULT);
        }
        if (!save_cfg($cfg)) respond(['ok' => false, 'error' => 'Не удалось сохранить настройки'], 500);
        respond(['ok' => true]);
    }

    respond(['ok' => false, 'error' => 'Unknown action'], 400);
} catch (CrInputError $e) {
    respond(['ok' => false, 'error' => $e->getMessage()] + $e->extra, $e->httpCode);
} catch (Throwable $e) {
    respond(['ok' => false, 'error' => $e->getMessage()], 500);
}
