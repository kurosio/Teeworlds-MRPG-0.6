<?php
/**
 * tools/init-db.php — создаёт рабочую базу редактора и заполняет её данными,
 * без которых нельзя войти (пароль администратора) и подключиться к игре.
 *
 *   php web-tools/editor-studio/tools/init-db.php
 *   php web-tools/editor-studio/tools/init-db.php --password=МойПароль --host=127.0.0.1 \
 *       --database=teeworlds_mrpg --db-user=root --db-password=secret
 *   php web-tools/editor-studio/tools/init-db.php --demo-guest   # демо-гость artist / artist123
 *
 * Что делает:
 *   1. создаёт data/editor-studio.sqlite со схемой (settings, guest_users,
 *      change_requests, request_messages, auth_throttle, rate_limits, meta);
 *   2. записывает параметры подключения к игровой базе;
 *   3. задаёт логин и пароль администратора (пароль хранится хешем);
 *   4. по желанию создаёт одного гостевого аккаунта, чтобы сразу проверить
 *      режим «гость → администратор → база».
 *
 * Повторный запуск безопасен: что не передано аргументом и уже есть в базе, то
 * остаётся как было; гостевые аккаунты и запросы не трогаются никогда.
 *
 * Файл базы не попадает в git (data/ закрыт .htaccess и исключён из репозитория).
 */

declare(strict_types=1);

// Скрипт создаёт базу и печатает пароль администратора — по HTTP его запускать
// нельзя. EDITOR_STUDIO_TEST=1 нужен автотестам (они поднимают PHP в эмуляторе,
// где SAPI отличается от cli).
if (PHP_SAPI !== 'cli' && getenv('EDITOR_STUDIO_TEST') !== '1') {
    http_response_code(403);
    echo "init-db.php запускается только из консоли.\n";
    exit(1);
}

require_once __DIR__ . '/../api/db-core.php';

const INIT_DEMO_GUEST_LOGIN = 'artist';
const INIT_DEMO_GUEST_PASSWORD = 'artist123';
const INIT_DEMO_GUEST_NAME = 'Аня';

function init_usage(): void
{
    echo <<<TXT
Создание базы Editor Studio (SQLite).

Использование:
  php tools/init-db.php [параметры]

Параметры:
  --password=ПАРОЛЬ        пароль администратора (по умолчанию генерируется)
  --user=ЛОГИН             логин администратора (по умолчанию admin)
  --host=АДРЕС             адрес MySQL-сервера игры (по умолчанию 127.0.0.1)
  --port=ПОРТ              порт MySQL (по умолчанию 3306)
  --database=ИМЯ           база игры (по умолчанию test — как в autoexec_server.cfg)
  --db-user=ПОЛЬЗОВАТЕЛЬ   пользователь MySQL (по умолчанию root)
  --db-password=ПАРОЛЬ     пароль MySQL (по умолчанию пустой)
  --demo-guest             создать гостевой аккаунт artist / artist123
  --guest=ЛОГИН:ПАРОЛЬ[:ИМЯ]  создать гостевой аккаунт с указанными данными
  --describe               только показать, что уже есть в базе
  --help                   эта справка

TXT;
}

/**
 * Разбор аргументов вида --ключ=значение и --флаг.
 * Своя функция, а не getopt(): getopt() доступен только в CLI-SAPI, а скрипт
 * должен одинаково работать и в консоли, и в тестовом окружении.
 */
function init_parse_args(array $argv): array
{
    $out = [];
    foreach (array_slice($argv, 1) as $arg) {
        if (!str_starts_with($arg, '--')) {
            continue;
        }
        $arg = substr($arg, 2);
        $pos = strpos($arg, '=');
        if ($pos === false) {
            $out[$arg] = true;
        } else {
            $out[substr($arg, 0, $pos)] = substr($arg, $pos + 1);
        }
    }
    return $out;
}

$options = init_parse_args($_SERVER['argv'] ?? []);

if (isset($options['help'])) {
    init_usage();
    exit(0);
}

$storeFile = store_path();
$describe = isset($options['describe']);

$randomPassword = static function (): string {
    $alphabet = 'abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789';
    $out = '';
    for ($i = 0; $i < 12; $i++) {
        $out .= $alphabet[random_int(0, strlen($alphabet) - 1)];
    }
    return $out;
};

try {
    $cfg = store_settings_load();
} catch (Throwable $e) {
    fwrite(STDERR, "Не удалось открыть базу {$storeFile}:\n  " . $e->getMessage() . "\n");
    fwrite(STDERR, "Проверьте, что в PHP включено расширение pdo_sqlite, а каталог data/ доступен на запись.\n");
    exit(1);
}

$hasPassword = trim((string)($cfg['editor_password'] ?? '')) !== '';

if ($describe) {
    echo "Файл базы: {$storeFile}" . (is_file($storeFile) ? ' (существует)' : ' (ещё не создан)') . "\n";
    echo 'Логин администратора: ' . (string)($cfg['editor_user'] ?? 'admin') . "\n";
    echo 'Пароль администратора: ' . ($hasPassword ? 'задан' : 'НЕ ЗАДАН — вход невозможен') . "\n";
    echo 'Подключение к игре: ' . sprintf(
        '%s:%s / %s (пользователь %s)',
        (string)($cfg['host'] ?? '127.0.0.1'),
        (string)($cfg['port'] ?? 3306),
        (string)($cfg['database'] ?? '') !== '' ? (string)$cfg['database'] : 'база не выбрана',
        (string)($cfg['user'] ?? 'root')
    ) . "\n";
    $guests = store_guest_users_all();
    echo 'Гостевых аккаунтов: ' . count($guests) . "\n";
    foreach ($guests as $guest) {
        echo sprintf(
            "  - %s (%s), %s, вкладок: %d\n",
            (string)$guest['login'],
            (string)($guest['name'] !== '' ? $guest['name'] : 'без имени'),
            !empty($guest['enabled']) ? 'включён' : 'выключен',
            count($guest['pages'] ?? [])
        );
    }
    echo 'Гостевой вход: ' . (!empty($cfg['guest_enabled']) ? 'включён' : 'выключен') . "\n";
    exit($hasPassword ? 0 : 1);
}

$passwordWasGenerated = false;

// ── 1. Параметры подключения к игровой базе ─────────────────────────────────

// Приоритет: аргумент командной строки → уже сохранённое значение → умолчание.
$next = $cfg;
$next['host']     = (string)($options['host']     ?? ($next['host']     ?? '127.0.0.1'));
$next['port']     = (int)   ($options['port']     ?? ($next['port']     ?? 3306));
// Умолчание «test» совпадает с autoexec_server.cfg этого сервера.
$next['database'] = (string)($options['database'] ?? ($next['database'] ?? 'test'));
$next['user']     = (string)($options['db-user']  ?? ($next['user']     ?? 'root'));
$next['password'] = (string)($options['db-password'] ?? ($next['password'] ?? ''));

// ── 2. Логин и пароль администратора ────────────────────────────────────────

$next['editor_user'] = trim((string)($options['user'] ?? ($next['editor_user'] ?? 'admin')));
if ($next['editor_user'] === '') {
    $next['editor_user'] = 'admin';
}

$plainPassword = '';
if (isset($options['password'])) {
    $plainPassword = (string)$options['password'];
} elseif (!$hasPassword) {
    $plainPassword = $randomPassword();
    $passwordWasGenerated = true;
}

if ($plainPassword !== '') {
    if (strlen($plainPassword) < 6) {
        fwrite(STDERR, "Пароль администратора слишком короткий: нужно не меньше 6 символов.\n");
        exit(1);
    }
    $next['editor_password'] = password_hash($plainPassword, PASSWORD_DEFAULT);
}

// ── 3. Значения по умолчанию для гостевого режима ───────────────────────────

$next += [
    'skins_api'               => '',
    'allow_remote_editor'     => false,
    'allowed_editor_ips'      => [],
    'guest_enabled'           => false,
    'guest_allow_remote'      => false,
    'guest_max_pending'       => 5,
    'guest_submit_cooldown'   => 60,
    'guest_max_operations'    => 100,
];
$next['editor_password'] ??= '';

if (!save_cfg($next)) {
    fwrite(STDERR, "Не удалось сохранить настройки в {$storeFile}.\n");
    exit(1);
}

// ── 4. Гостевой аккаунт для проверки режима заявок ──────────────────────────

$guestSpec = null;
if (isset($options['guest'])) {
    $guestSpec = (string)$options['guest'];
} elseif (isset($options['demo-guest'])) {
    $guestSpec = INIT_DEMO_GUEST_LOGIN . ':' . INIT_DEMO_GUEST_PASSWORD . ':' . INIT_DEMO_GUEST_NAME;
}

$guestReport = null;
if ($guestSpec !== null) {
    $parts = explode(':', $guestSpec, 3);
    $login = strtolower(trim($parts[0] ?? ''));
    $guestPassword = (string)($parts[1] ?? '');
    $name = trim((string)($parts[2] ?? ''));

    if (preg_match('/^[a-z0-9._-]{3,32}$/', $login) !== 1) {
        fwrite(STDERR, "Логин гостя должен быть 3–32 символа: латиница, цифры, точка, дефис, подчёркивание.\n");
        exit(1);
    }
    if (strlen($guestPassword) < 6) {
        fwrite(STDERR, "Пароль гостя слишком короткий: нужно не меньше 6 символов.\n");
        exit(1);
    }

    if (store_guest_user_by_login($login) !== null) {
        $guestReport = "Гостевой аккаунт «{$login}» уже существует — оставлен без изменений.";
    } else {
        // Демо-гостю открываем первые три вкладки из каталога — остальные
        // администратор включит сам в разделе «Гостевые пользователи».
        $pages = array_slice(editor_guest_page_keys(), 0, 3);
        $now = store_now();
        $created = store_guest_user_insert([
            'id'             => 'g' . bin2hex(random_bytes(8)),
            'login'          => $login,
            'name'           => $name !== '' ? $name : $login,
            'enabled'        => true,
            'pages'          => $pages,
            'passwordHash'   => password_hash($guestPassword, PASSWORD_DEFAULT),
            'sessionVersion' => 1,
            'createdAt'      => $now,
            'updatedAt'      => $now,
            'lastLoginAt'    => '',
        ]);
        $guestReport = $created
            ? "Создан гостевой аккаунт «{$login}» (пароль: {$guestPassword}, вкладки: " . implode(', ', $pages) . ').'
            : "Не удалось создать гостевой аккаунт «{$login}».";
        store_settings_set('guest_enabled', true);
    }
}

// ── 5. Отчёт ────────────────────────────────────────────────────────────────

echo "База редактора готова: {$storeFile}\n\n";
echo 'Логин администратора: ' . $next['editor_user'] . "\n";
if ($plainPassword !== '') {
    echo 'Пароль администратора: ' . $plainPassword . "\n";
} else {
    echo "Пароль администратора: без изменений (задан ранее).\n";
}
echo sprintf(
    "Игровая база: %s:%d / %s (пользователь %s)\n",
    (string)$next['host'],
    (int)$next['port'],
    (string)$next['database'] !== '' ? (string)$next['database'] : 'не выбрана',
    (string)$next['user']
);
if ($guestReport !== null) {
    echo $guestReport . "\n";
}
echo "\nДальше:\n";
echo "  1. откройте index.html редактора и войдите как администратор;\n";
echo "  2. раздел «Настройки» → «Проверить соединение» — убедитесь, что игровая база доступна;\n";
echo "  3. «Правила доступа» и «Гостевые пользователи» — включите гостевой вход и создайте аккаунты.\n";
if ($passwordWasGenerated) {
    echo "\nПароль сгенерирован автоматически: сохраните его и смените в «Настройках», когда будет удобно.\n";
}
echo "Забыли пароль — задайте новый: php tools/init-db.php --password=НОВЫЙ\n";
