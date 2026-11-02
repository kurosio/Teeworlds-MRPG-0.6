'use strict';

/**
 * tests/db-history.test.cjs — вкладка «История изменений».
 *
 * Проверяется модель вкладки (editor-core/db-history.js) и связка страницы с
 * остальной студией:
 *
 *  1. подписи разделов, заголовок записи и автор изменения;
 *  2. различия по колонкам для создания, изменения и удаления — включая
 *     правило «NULL = '' = 0 = нет значения», чтобы в списке не появлялись
 *     ложные строки;
 *  3. когда запись можно отменить и что именно напишет подтверждение;
 *  4. страница db-history.html зарегистрирована в группе System и не делает
 *     ничего в обход общего ядра (никакого прямого fetch и нативного confirm);
 *  5. английские подписи вкладки переведены на русский.
 */

const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const read = (name) => fs.readFileSync(path.join(root, name), 'utf8');

function historyHarness() {
  const window = { addEventListener() {}, dispatchEvent() {} };
  const context = vm.createContext({ window, console, setTimeout, clearTimeout });
  context.window.window = context.window;
  vm.runInContext(read('editor-core/db-history.js'), context);
  return context.window.EditorCore.DbHistory;
}

const H = historyHarness();

// Значения из VM-контекста нельзя сравнивать deepStrictEqual (чужие
// прототипы) — сравниваем по JSON, как в остальных тестах студии.
const eq = (actual, expected, message) => assert.equal(JSON.stringify(actual), JSON.stringify(expected), message);

const entry = (patch = {}) => ({
  id: 10,
  at: '2026-11-02T09:15:00Z',
  actor_role: 'admin',
  actor_id: 'admin',
  actor_name: 'admin',
  approved_by: '',
  source: 'editor',
  request_id: '',
  resource: 'worlds',
  resource_title: 'Миры',
  table_name: 'tw_worlds',
  record_id: 3,
  op: 'update',
  label: 'Gridania',
  changed: ['Name'],
  changes: 1,
  before: { Name: 'Old' },
  after: { Name: 'New' },
  revert_of: 0,
  reverted_at: '',
  reverted_by: 0,
  revertable: true,
  can_revert: true,
  known_resource: true,
  ...patch,
});

test('подписи разделов: свои английские, иначе серверные', () => {
  assert.equal(H.resourceLabel(entry()), 'Worlds');
  assert.equal(H.resourceLabel(entry({ resource: 'quest_bots' })), 'Quest steps');
  // Неизвестный раздел: остаётся подпись сервера, а не «undefined».
  assert.equal(H.resourceLabel(entry({ resource: 'custom_table', resource_title: 'Своя таблица' })), 'Своя таблица');
});

test('заголовок записи и автор изменения', () => {
  assert.equal(H.entryTitle(entry()), 'Worlds · #3 · Gridania');
  assert.equal(H.entryTitle(entry({ label: '' })), 'Worlds · #3');
  // Мир 0 — настоящая запись, а не «нет записи».
  assert.equal(H.entryTitle(entry({ resource: 'worlds', record_id: 0, label: 'Tutorilishe' })), 'Worlds · #0 · Tutorilishe');

  assert.equal(H.actorText(entry()), 'Administrator · admin');
  assert.equal(
    H.actorText(entry({ actor_role: 'guest', actor_name: 'artist', approved_by: 'admin' })),
    'Guest · artist · approved by admin',
  );
  // Администратор и есть автор — «approved by» не дублируется.
  assert.equal(H.actorText(entry({ approved_by: 'admin' })), 'Administrator · admin');
});

test('различия по колонкам: создание показывает все значения как новые', () => {
  const rows = H.diffRows(entry({ op: 'create', before: {}, after: { Name: 'New', Path: 'main.map' } }));
  eq(rows.map((row) => [row.column, row.kind]), [
    ['Name', 'added'],
    ['Path', 'added'],
  ]);
  assert.equal(H.changedColumns(entry({ op: 'create', before: {}, after: { Name: 'New', Path: 'main.map' } })).join(','), 'Name,Path');
});

test('различия по колонкам: удаление показывает удалённые значения', () => {
  const rows = H.diffRows(entry({ op: 'delete', before: { Name: 'Old' }, after: {} }));
  eq(rows.map((row) => [row.column, row.kind, row.before, row.after]), [
    ['Name', 'removed', 'Old', ''],
  ]);
});

test('различия по колонкам: изменение и правило NULL = \'\' = 0', () => {
  const changed = H.diffRows(entry());
  eq(changed.map((row) => [row.column, row.kind, row.before, row.after]), [
    ['Name', 'changed', 'Old', 'New'],
  ]);

  // Пустое значение с обеих сторон — не изменение, даже если это NULL и ''.
  const empty = H.diffRows(entry({ before: { Description: null }, after: { Description: '' } }));
  assert.equal(empty[0].kind, 'unchanged');
  assert.equal(H.changedColumns(entry({ before: { Flags: 0 }, after: { Flags: '' } })).length, 0);
});

test('сложные значения (JSON) сравниваются по содержимому и показываются текстом', () => {
  const rows = H.diffRows(entry({
    before: { Data: '{"a":1}' },
    after: { Data: '{"a":2}' },
  }));
  assert.equal(rows[0].kind, 'changed');
  assert.equal(rows[0].before, '{"a":1}');

  const objectRows = H.diffRows(entry({
    before: { Data: { a: 1 } },
    after: { Data: { a: 1 } },
  }));
  assert.equal(objectRows[0].kind, 'unchanged');
  assert.equal(H.formatValue({ a: 1 }), '{"a":1}');
  assert.equal(H.formatValue(null), '');
  assert.equal(H.formatValue(0), '0');
});

test('в списке (без before/after) поля берутся из готового списка changed', () => {
  // Список истории приходит без before/after: сервер отдаёт только имена полей.
  const fromList = { op: 'update', changes: 2, changed: ['Name', 'Path'] };
  eq(H.changedColumns(fromList), ['Name', 'Path']);
  eq(H.changedCount(fromList), 2);
  // В окне записи есть сами значения — считаем по различиям.
  const full = {
    op: 'update',
    before: { Name: 'Old', Path: 'p', Flags: 1 },
    after: { Name: 'New', Path: 'p', Flags: 1 },
  };
  eq(H.changedColumns(full), ['Name']);
  eq(H.changedCount(full), 1);
  // Ничего не известно — пустой список, а не падение.
  eq(H.changedColumns({}), []);
  eq(H.changedColumns(null), []);
});

test('отменять можно только действующее изменение известного раздела', () => {
  assert.equal(H.canRevert(entry()), true);
  assert.equal(H.revertBlockReason(entry()), '');

  const undone = entry({ reverted_at: '2026-11-02T10:00:00Z', reverted_by: 11, can_revert: false });
  assert.equal(H.canRevert(undone), false);
  assert.match(H.revertBlockReason(undone), /already been undone/);

  const big = entry({ revertable: false, can_revert: false });
  assert.equal(H.canRevert(big), false);
  assert.match(H.revertBlockReason(big), /no full data/);

  const unknown = entry({ known_resource: false, revertable: false, can_revert: false });
  assert.match(H.revertBlockReason(unknown), /no longer editable/);

  const broken = entry({ op: 'rename', can_revert: false });
  assert.equal(H.canRevert(broken), false);
  assert.match(H.revertBlockReason(broken), /Unknown change type/);
});

test('подтверждение отмены описывает, что изменится', () => {
  assert.match(H.revertQuestion(entry({ op: 'create', before: {}, after: { Name: 'New' } })), /delete this record/);
  assert.match(H.revertQuestion(entry({ op: 'delete', before: { Name: 'Old' }, after: {} })), /restore the record/);
  assert.match(H.revertQuestion(entry()), /restore 1 previous field value/);
  const two = entry({ before: { Name: 'a', Path: 'b' }, after: { Name: 'c', Path: 'd' } });
  assert.match(H.revertQuestion(two), /restore 2 previous field value/);
});

test('записи группируются по дням, порядок сохраняется', () => {
  const groups = H.groupByDay([
    entry({ id: 3, at: '2026-11-02T10:00:00Z' }),
    entry({ id: 2, at: '2026-11-02T09:00:00Z' }),
    entry({ id: 1, at: '2026-10-31T20:00:00Z' }),
  ]);
  eq(groups.map((group) => group.key), ['2026-11-02', '2026-10-31']);
  eq(groups[0].rows.map((row) => row.id), [3, 2]);
});

test('страница истории зарегистрирована в группе System', () => {
  const window = {};
  const context = vm.createContext({ window, console });
  vm.runInContext(read('studio.manifest.js'), context);
  const groups = context.window.STUDIO_MANIFEST;
  const system = groups.find((group) => group.id === 'system');
  assert.ok(system, 'в манифесте нет группы System');
  const item = system.items.find((entry) => entry.key === 'history');
  assert.ok(item, 'в группе System нет вкладки истории');
  assert.equal(item.src, 'db-history.html');
  assert.equal(item.adminOnly, true, 'история БД не должна быть доступна гостю');
  assert.ok(!/history/i.test(system.items.find((entry) => entry.key === 'requests').title), 'история — отдельная вкладка');
});

test('каждую запись истории можно открыть в её редакторе', () => {
  const window = { addEventListener() {}, dispatchEvent() {} };
  const context = vm.createContext({ window, console, setTimeout, clearTimeout });
  context.window.window = context.window;
  vm.runInContext(read('studio.manifest.js'), context);
  vm.runInContext(read('editor-core/db-history.js'), context);
  const model = context.window.EditorCore.DbHistory;
  const manifest = context.window.STUDIO_MANIFEST;

  // Все ключи вкладок должны существовать в манифесте — иначе кнопка
  // «Открыть в редакторе» ведёт в никуда.
  const keys = new Set(manifest.flatMap((group) => (group.items || []).map((item) => item.key)));
  for (const [resource, key] of Object.entries(model.RESOURCE_EDITORS)) {
    assert.ok(keys.has(key), `раздел ${resource} ссылается на неизвестную вкладку ${key}`);
  }

  const page = (resource) => model.editorPage({ resource });
  assert.equal(page('items'), 'items-editor.html');
  assert.equal(page('warehouses'), 'warehouse-editor.html');
  assert.equal(page('bots_mobs'), 'mobs-editor.html');
  assert.equal(page('bots_info'), 'bots-info-editor.html');
  // Несколько разделов живут в одном редакторе.
  assert.equal(model.editorFor({ resource: 'quest_bots' }), 'quests');
  assert.equal(model.editorFor({ resource: 'quest_board_links' }), 'quest_boards');
  // Неизвестный раздел открывать некуда, но и падать нельзя.
  assert.equal(model.editorPage({ resource: 'removed_section' }), '');
  assert.equal(model.editorPage(null), '');
});

test('все редактируемые разделы умеют открываться в редакторе', () => {
  // Карта ресурсов сервера ↔ карта вкладок: добавили раздел в один файл —
  // он должен появиться и в другом, иначе из истории в него не перейти.
  const server = read('api/db-resources.php');
  // Только карта ресурсов, без соседней карты обратных ссылок (она тоже
  // перечисляет таблицы).
  const start = server.indexOf('function editor_resources');
  const end = server.indexOf('function editor_reverse_refs');
  assert.ok(start > 0 && end > start, 'не найдена карта ресурсов');
  const resources = [...server.slice(start, end).matchAll(/^        '([a-z_]+)' => \[/gm)].map((m) => m[1]);
  assert.ok(resources.length >= 15, `разобрано разделов: ${resources.length}`);
  const missing = resources.filter((resource) => !H.RESOURCE_EDITORS[resource]);
  eq(missing, [], `разделы без вкладки: ${missing.join(', ')}`);
});

test('страница использует общее ядро и не пишет в БД напрямую', () => {
  const html = read('db-history.html');
  assert.ok(html.includes('data-editor="db-history"'));
  assert.ok(html.includes('editor-page-header'));
  assert.ok(html.includes('api/db-history.php'));
  assert.ok(html.includes("method: 'POST'"), 'отмена должна идти POST-запросом');
  assert.ok(html.includes('Page.confirm('), 'отмена подтверждается общим диалогом');
  assert.ok(html.includes('editor-core/db-history.js') || read('editor-core/page-loader.js').includes('editor-core/db-history.js'),
    'модуль истории должен подключаться загрузчиком страниц');
  assert.ok(!/(?<![\w.$])fetch\s*\(/.test(html), 'на странице не должно быть прямого fetch');
  assert.ok(!/(?<![\w.$])confirm\s*\(/.test(html), 'на странице не должно быть нативного confirm');
  // Кнопка отмены есть у каждой отменяемой записи и в окне записи.
  assert.ok(html.includes('data-undo='));
  assert.ok(html.includes('entry-undo'));
});

test('подписи вкладки переведены на русский', () => {
  const window = { EditorCoreLocalization: {} };
  const context = vm.createContext({ window, console });
  context.window.window = context.window;
  vm.runInContext(read('localization/index.js'), context);
  const registry = context.window.EditorCoreLocalization;
  for (const file of registry.files) vm.runInContext(read(`localization/${file}`), context);
  const strings = registry.strings('ru');

  const required = [
    'Change history',
    'Database changes and undo',
    'Every change made in the editors and every approved guest request. Click an entry to undo it.',
    'Recorded changes',
    'Undo this change',
    'Open in editor',
    'Undo',
    'Undone',
    'Undo anyway',
    'Change undone',
    'Could not undo the change',
    'This change has already been undone.',
    'The journal has no full data for this change, so it cannot be undone automatically.',
    'Undo the edit: restore {0} previous field value(s)?',
    'Undoable entries: {0} · {1} in total',
    'Empty',
    'Fields',
  ];
  const missing = required.filter((key) => !strings[key]);
  eq(missing, [], `нет перевода: ${missing.join(', ')}`);
  // Формы числительных для «entries».
  eq(registry.languages.ru.plurals.entry, ['запись', 'записи', 'записей']);
});
