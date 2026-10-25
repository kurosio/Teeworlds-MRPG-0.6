const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const read = (name) => fs.readFileSync(path.join(root, name), 'utf8');

/**
 * Страница настроек целиком работает в VM: подменяем DOM и API, но исполняем
 * настоящий inline-скрипт settings.html. «Сервер» хранит пароль БД в переменной
 * serverPassword и отвечает на save_config так же, как api/db.php: пустое поле
 * без clear_password пароль сохраняет.
 */
function harness({ stored = null, activeId = null, password = 'secret' } = {}) {
  const scripts = [...read('settings.html').matchAll(/<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)<\/script>/gi)]
    .map((m) => m[1]);
  let source = scripts.find((s) => s.includes('bootstrapEditor'));
  assert.ok(source, 'inline-скрипт settings.html не найден');

  // init() вызывается в конце скрипта — перехватываем промис, чтобы дождаться
  // всех запросов, и открываем доступ к внутренним функциям страницы.
  source = source.replace(/\n\s*init\(\);\n/, '\n    globalThis.__initPromise = init();\n');
  source += `\nglobalThis.__test = { init, doSave, doTest, switchProfile, F, active, profiles: () => profiles };\n`;

  const elements = new Map();
  const element = () => ({
    value: '', checked: false, placeholder: '', disabled: false,
    innerHTML: '', textContent: '', dataset: {},
    classList: { add() {}, remove() {}, contains: () => false },
    addEventListener() {}, removeEventListener() {}, focus() {}, select() {},
    querySelectorAll: () => [], appendChild() {}, remove() {},
  });
  const byId = (id) => {
    if (!elements.has(id)) elements.set(id, element());
    return elements.get(id);
  };

  const local = new Map();
  if (stored) local.set('es_db_profiles', JSON.stringify(stored));
  if (activeId) local.set('es_db_active', activeId);
  const localStorage = {
    getItem: (key) => local.get(key) ?? null,
    setItem: (key, value) => local.set(key, String(value)),
    removeItem: (key) => local.delete(key),
  };

  const state = { password, expectedPassword: password, saves: [], tests: [] };

  const EditorCore = {
    bootstrapEditor: () => ({}),
    DB: {
      getConfig: async () => ({
        ok: true,
        config: {
          host: 'db.local', port: 3306, database: 'game', user: 'root',
          // Пароль не отдаём — только признак (как api/db.php).
          password_set: state.password !== '',
          skins_api: '', map_api: '', map_api_token_set: false,
          maps_dir: '', mapres_dir: '',
        },
      }),
      saveConfig: async (payload) => {
        state.saves.push({ ...payload });
        if (payload.clear_password) state.password = '';
        else if (payload.password) state.password = payload.password;
        return { ok: true, password_set: state.password !== '' };
      },
      // Редактор открывается после настроек: подключение живо, только если
      // пароль в конфиге сервера не сбит.
      testConnection: async () => {
        const ok = state.password === state.expectedPassword;
        state.tests.push(ok);
        return ok ? { ok: true, server: '8.0.36' } : { ok: false, error: 'Access denied' };
      },
      list: async () => ({ ok: true, items: [], has_more: false }),
      testSkins: async () => ({ ok: true }),
      testMapApi: async () => ({ ok: true, source: 'bridge' }),
    },
    Http: { json: async () => ({ ok: true, data: {} }), postMessage: () => true },
    Sync: {
      listProfiles: async () => ({ ok: true, profiles: [] }),
      syncMap: async () => ({ ok: true, tables: [] }),
      setAsTarget: async () => ({ ok: true }),
      saveTarget: async () => ({ ok: true }),
    },
    Page: {
      esc: String, toast() {}, confirm: async () => false,
      guardUnsaved() {}, alert() {},
    },
    utils: { setStatus() {} },
    theme: { get: () => 'dark', set() {} },
  };

  const window = { EditorCore, addEventListener() {}, location: {}, parent: null };
  window.parent = window;
  const document = {
    getElementById: byId,
    querySelectorAll: () => [],
    createElement: element,
    body: element(),
    addEventListener() {},
  };

  const sandbox = {
    window, document, EditorCore, localStorage, console,
    setTimeout: () => 0, clearTimeout() {},
  };
  const context = vm.createContext(sandbox);
  vm.runInContext(source, context, { filename: 'settings.html' });
  return { sandbox, state, byId, localStorage };
}

const profile = (overrides = {}) => ({
  id: 'a', name: 'Default', host: 'db.local', port: 3306, db: 'game',
  user: 'root', pass: '', nopass: false, skins: '',
  mapApi: '', mapsDir: '', mapresDir: '', mapTokenSet: false,
  status: 'idle', ...overrides,
});

test('открытие настроек не сбрасывает пароль БД (первый запуск, пароль задан на сервере)', async () => {
  const h = harness({ password: 'secret' });
  await h.sandbox.__initPromise;

  // Страница отправила конфиг один раз, но без password и без clear_password.
  assert.equal(h.state.saves.length, 1);
  assert.equal('password' in h.state.saves[0], false);
  assert.equal('clear_password' in h.state.saves[0], false);
  assert.equal(h.state.password, 'secret');

  // Редактор после настроек по-прежнему подключается к БД.
  assert.deepEqual(h.state.tests, [true]);
  const editorTest = await h.sandbox.EditorCore.DB.testConnection();
  assert.equal(editorTest.ok, true);

  // И поле показывает, что пароль сохранён на сервере.
  assert.match(h.byId('f-pass').placeholder, /stored on the server/);
  assert.equal(h.sandbox.__test.active().hasServerPassword, true);
});

test('старый профиль без признаков пароля тоже не стирает пароль сервера', async () => {
  // Профиль из предыдущей версии: нет ни pass, ни hasServerPassword,
  // ни passwordless — раньше именно он слал clear_password.
  const h = harness({ stored: [profile()], activeId: 'a', password: 'secret' });
  await h.sandbox.__initPromise;

  assert.equal(h.state.password, 'secret');
  assert.ok(h.state.saves.every((s) => !('clear_password' in s) && !('password' in s)));
  assert.equal(h.sandbox.__test.active().hasServerPassword, true);
});

test('при открытии настроек устаревший пароль из localStorage не перезаписывает серверный', async () => {
  // Пароль сменили в другом браузере, а в этом остался старый профиль.
  const h = harness({
    stored: [profile({ pass: 'old-secret', hasServerPassword: true })],
    activeId: 'a', password: 'new-secret',
  });
  await h.sandbox.__initPromise;

  assert.ok(h.state.saves.every((s) => !('password' in s) && !('clear_password' in s)));
  assert.equal(h.state.password, 'new-secret');
  assert.deepEqual(h.state.tests, [true]);
});

test('сохранение без ввода пароля оставляет пароль сервера', async () => {
  const h = harness({ password: 'secret' });
  await h.sandbox.__initPromise;
  h.state.saves.length = 0;

  await h.sandbox.__test.doSave();

  assert.equal(h.state.saves.length, 1);
  assert.equal('clear_password' in h.state.saves[0], false);
  assert.equal('password' in h.state.saves[0], false);
  assert.equal(h.state.password, 'secret');
});

test('явный чекбокс «Без пароля» всё ещё очищает пароль', async () => {
  const h = harness({ password: 'secret' });
  await h.sandbox.__initPromise;
  h.state.saves.length = 0;
  h.state.expectedPassword = '';

  h.byId('f-nopass').checked = true;
  h.byId('f-pass').value = '';
  await h.sandbox.__test.doSave();

  assert.equal(h.state.saves[0].clear_password, true);
  assert.equal(h.state.password, '');
  assert.equal(h.sandbox.__test.active().hasServerPassword, false);
});

test('новый пароль из поля сохраняется на сервере', async () => {
  const h = harness({ password: 'secret' });
  await h.sandbox.__initPromise;
  h.state.saves.length = 0;
  h.state.expectedPassword = 'new-secret';

  h.byId('f-pass').value = 'new-secret';
  await h.sandbox.__test.doSave();

  assert.equal(h.state.saves[0].password, 'new-secret');
  assert.equal('clear_password' in h.state.saves[0], false);
  assert.equal(h.state.password, 'new-secret');
});

test('переключение профилей: беспарольный профиль сбрасывает пароль, профиль с паролем — возвращает', async () => {
  const withPassword = profile({ id: 'a', pass: 'secret', hasServerPassword: true, passwordless: false });
  const withoutPassword = profile({ id: 'b', db: 'local', passwordless: true });
  const h = harness({ stored: [withPassword, withoutPassword], activeId: 'a', password: 'secret' });
  await h.sandbox.__initPromise;
  h.state.saves.length = 0;

  h.state.expectedPassword = '';
  await h.sandbox.__test.switchProfile('b');
  assert.equal(h.state.saves[0].clear_password, true);
  assert.equal(h.state.password, '');

  h.state.expectedPassword = 'secret';
  await h.sandbox.__test.switchProfile('a');
  assert.equal(h.state.saves[1].password, 'secret');
  assert.equal(h.state.password, 'secret');
});

test('API отдаёт только признак пароля и не очищает его без явного флага', () => {
  const php = read('api/db.php');
  const getConfig = php.slice(php.indexOf('function handle_get_config'), php.indexOf('function handle_save_config'));
  assert.match(getConfig, /'password_set'\s*=>\s*\(\(string\)\(\$c\['password'\]/);
  assert.doesNotMatch(getConfig, /'password'\s*=>/);

  const saveConfig = php.slice(php.indexOf('function handle_save_config'), php.indexOf('function handle_test('));
  assert.match(saveConfig, /\$body\['clear_password'\]\s*\?\?\s*false/);
  assert.match(saveConfig, /\$body\['clear_map_api_token'\]\s*\?\?\s*false/);
  assert.match(saveConfig, /respond\(\['ok' => true, 'password_set' => \$passwordSet\]\)/);
});
