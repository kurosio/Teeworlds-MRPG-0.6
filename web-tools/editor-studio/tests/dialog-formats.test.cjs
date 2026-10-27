/**
 * tests/dialog-formats.test.cjs — форматы реплики в редакторе диалогов.
 *
 * Проверяется настоящий `DialogEditor` (editor-core/dialog-editor.js) в DOM:
 * панель форматов над полем, всплывающий выбор записи, чипы подстановок под
 * полем и счётчик длины. Подстановки — те, что заменяет игровой сервер:
 * `<bot_12>`, `<item_31>`, `<world_3>`, `<player>`, `<time>`, `<here>`,
 * `<eidolon>` (src/game/server/core/components/Bots/DialogsData.cpp).
 *
 * Нужен пакет jsdom (`npm install --no-save jsdom`); без него тест пропускается.
 */
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');

let JSDOM = null;
try { ({ JSDOM } = require('jsdom')); } catch { JSDOM = null; }
const skip = JSDOM ? false : 'jsdom не установлен (npm install --no-save jsdom)';

const escapeHtml = (value) => String(value ?? '').replace(/[&<>"']/g,
  (ch) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[ch]));

const getValueAtPath = (data, fieldPath) => String(fieldPath)
  .split('.')
  .reduce((node, key) => (node === null || node === undefined ? undefined : node[key]), data);

/** Минимальный FieldRenderer: текст — textarea, остальные поля — input. */
const minimalFieldRenderer = {
  renderField: ({ path: fieldPath, data }) => (fieldPath.endsWith('.text')
    ? `<textarea data-path="${fieldPath}" rows="2">${escapeHtml(getValueAtPath(data, fieldPath))}</textarea>`
    : `<input type="text" data-path="${fieldPath}" value="" />`),
  getInputValue: (input) => input.value,
  setValueAtPath: (data, fieldPath, value) => {
    const parts = String(fieldPath).split('.');
    let node = data;
    for (let i = 0; i < parts.length - 1; i += 1) {
      if (node[parts[i]] === undefined || node[parts[i]] === null) node[parts[i]] = {};
      node = node[parts[i]];
    }
    node[parts[parts.length - 1]] = value;
  },
};

const BOTS = new Map([['1', 'Elder'], ['2', 'Blacksmith'], ['12', 'Mira']]);
const WORLDS = new Map([['3', 'Gridania']]);
const DB_ITEMS = [{ value: '31', label: 'Iron Sword' }, { value: '32', label: 'Potion' }];

const tick = () => new Promise((resolve) => setTimeout(resolve, 0));

/** Окно редактора: модуль загружается один раз, как на настоящей странице. */
const createWindow = (dbCalls = []) => {
  const dom = new JSDOM('<!doctype html><html><body></body></html>', {
    runScripts: 'outside-only',
    url: 'http://localhost/bots-npc-editor.html',
  });
  const { window } = dom;
  window.EditorCore = {
    FieldRenderer: minimalFieldRenderer,
    UIManager: { init() {} },
    utils: { escapeHtml },
    DB: {
      async list(dbSource) {
        dbCalls.push(dbSource);
        if (dbSource === 'items') return { ok: true, items: DB_ITEMS };
        throw new Error('no database');
      },
    },
  };
  window.eval(source('dialog-editor.js'));
  window.__dbCalls = dbCalls;
  return window;
};

const mount = ({ lines = [''], formats = { bot: BOTS, world: WORLDS }, window: existing = null } = {}) => {
  const dbCalls = existing ? existing.__dbCalls : [];
  const window = existing || createWindow(dbCalls);
  const host = window.document.createElement('div');
  host.setAttribute('data-host', '1');
  window.document.body.appendChild(host);
  const changes = [];
  const editor = window.EditorCore.DialogEditor.mount(host, {
    data: { DialogData: lines.map((text) => ({ text })) },
    fieldOptions: {},
    sideOptions: [{ value: 'default', label: 'Default' }],
    formats,
    onChange: (data, meta) => changes.push({ data, meta }),
  });

  const textarea = (index = 0) => host.querySelector(`textarea[data-path="DialogData.${index}.text"]`);
  const chips = (index = 0) => Array.from(host.querySelectorAll(`[data-dialog-tokens="${index}"] [data-dialog-token]`));
  const picker = () => window.document.body.querySelector('.dlg-picker');
  const press = (element, key) => element.dispatchEvent(new window.KeyboardEvent('keydown', { key, bubbles: true, cancelable: true }));
  const click = (element) => element.dispatchEvent(new window.MouseEvent('click', { bubbles: true, cancelable: true }));
  const caretTo = (position, index = 0) => {
    const ta = textarea(index);
    ta.setSelectionRange(position, position);
    ta.dispatchEvent(new window.Event('select'));
    return ta;
  };
  const search = (value) => {
    const input = picker().querySelector('.dlg-picker-search');
    input.value = value;
    input.dispatchEvent(new window.Event('input', { bubbles: true }));
    return input;
  };
  const options = () => Array.from(picker()?.querySelectorAll('.dlg-picker-option') || []);

  return { window, host, editor, changes, textarea, chips, picker, press, click, caretTo, search, options, dbCalls };
};

test('панель форматов перечисляет все подстановки игрового сервера', { skip }, () => {
  const { host } = mount();
  const kinds = Array.from(host.querySelectorAll('[data-dialog-insert]'))
    .map((btn) => btn.getAttribute('data-dialog-insert'));
  assert.deepEqual(kinds, ['bot', 'item', 'world', 'player', 'time', 'here', 'eidolon']);
  // Простые форматы подписаны самим токеном — их не нужно угадывать.
  assert.match(host.querySelector('[data-dialog-insert="player"]').textContent, /<player>/);
  assert.equal(host.querySelector('[data-dialog-insert="bot"]').getAttribute('title'), 'Insert a bot name');
});

test('простой формат вставляется в каретку', { skip }, () => {
  const { click, caretTo, textarea, host } = mount({ lines: ['Hello there!'] });
  caretTo(5);
  click(host.querySelector('[data-dialog-insert="player"]'));

  assert.equal(textarea().value, 'Hello<player> there!');
  assert.equal(host.querySelector('[data-dialog-tokens="0"] .dlg-token-code').textContent, '<player>');
});

test('имя бота вставляется из всплывающего списка и показывается чипом', { skip }, async () => {
  const { host, click, press, picker, options, search, textarea, chips } = mount({ lines: ['Talk to '] });

  click(host.querySelector('[data-dialog-insert="bot"]'));
  await tick();
  assert.ok(picker(), 'список открылся');
  assert.deepEqual(options().map((btn) => btn.querySelector('.dlg-picker-label').textContent),
    ['Elder', 'Blacksmith', 'Mira']);

  search('mir');
  assert.deepEqual(options().map((btn) => btn.querySelector('.dlg-picker-label').textContent), ['Mira']);

  press(picker().querySelector('.dlg-picker-search'), 'Enter');
  await tick();
  assert.equal(picker(), null, 'список закрылся после выбора');
  assert.equal(textarea().value, 'Talk to <bot_12>');

  const [chip] = chips();
  assert.equal(chip.querySelector('.dlg-token-label').textContent, 'Mira');
  assert.equal(chip.querySelector('.dlg-token-id').textContent, '#12');
  assert.equal(chip.getAttribute('data-token-kind'), 'bot');
});

test('чип записи меняется через тот же список и убирается крестиком', { skip }, async () => {
  const { host, click, press, picker, options, textarea, chips } = mount({ lines: ['Hi <bot_1>!'] });

  assert.equal(chips()[0].querySelector('.dlg-token-label').textContent, 'Elder');
  click(chips()[0].querySelector('[data-dialog-token-edit]'));
  await tick();
  assert.ok(picker().querySelector('.dlg-picker-option.is-current'), 'текущая запись отмечена');

  // Вторая запись списка — «Blacksmith» (первая уже выбрана).
  const target = options().find((btn) => btn.querySelector('.dlg-picker-label').textContent === 'Blacksmith');
  click(target);
  await tick();
  assert.equal(textarea().value, 'Hi <bot_2>!');
  assert.equal(chips()[0].querySelector('.dlg-token-label').textContent, 'Blacksmith');

  click(chips()[0].querySelector('[data-dialog-token-remove]'));
  assert.equal(textarea().value, 'Hi !');
  assert.equal(chips().length, 0);
});

test('«Убрать формат» в списке удаляет подстановку', { skip }, async () => {
  const { click, chips, picker, textarea } = mount({ lines: ['Bye <bot_1>'] });
  click(chips()[0].querySelector('[data-dialog-token-edit]'));
  await tick();
  click(picker().querySelector('[data-dialog-picker-remove]'));
  await tick();
  assert.equal(textarea().value, 'Bye ');
});

test('записи, которых нет в списке, помечаются как неизвестные', { skip }, () => {
  const { chips } = mount({ lines: ['<bot_99> and <world_3>'] });
  const [unknown, known] = chips();
  assert.equal(unknown.classList.contains('is-invalid'), true);
  assert.equal(unknown.querySelector('.dlg-token-main').getAttribute('title'), 'Unknown record #99');
  assert.equal(known.classList.contains('is-invalid'), false);
  assert.equal(known.querySelector('.dlg-token-label').textContent, 'Gridania');
});

test('Escape закрывает список, не меняя текст', { skip }, async () => {
  const { host, click, press, picker, textarea } = mount({ lines: ['Keep me'] });
  click(host.querySelector('[data-dialog-insert="world"]'));
  await tick();
  assert.ok(picker());
  press(picker().querySelector('.dlg-picker-search'), 'Escape');
  await tick();
  assert.equal(picker(), null);
  assert.equal(textarea().value, 'Keep me');
});

test('цифры в поиске дают готовый ID, даже если записи нет в списке', { skip }, async () => {
  const { host, click, search, options, press, picker, textarea } = mount({ lines: [''] });
  click(host.querySelector('[data-dialog-insert="bot"]'));
  await tick();
  search('99');
  assert.equal(options().length, 1);
  assert.match(options()[0].textContent, /Use ID 99/);
  press(picker().querySelector('.dlg-picker-search'), 'Enter');
  await tick();
  assert.equal(textarea().value, '<bot_99>');
});

test('предметы подгружаются из БД лениво и кэшируются', { skip }, async () => {
  const { host, click, picker, options, dbCalls } = mount({ lines: [''] });
  click(host.querySelector('[data-dialog-insert="item"]'));
  await tick();
  await tick();
  assert.deepEqual(options().map((btn) => btn.querySelector('.dlg-picker-label').textContent),
    ['Iron Sword', 'Potion']);
  assert.deepEqual(dbCalls, ['items']);

  click(picker().querySelector('[data-dialog-picker-remove]') || options()[0]);
  await tick();
  click(host.querySelector('[data-dialog-insert="item"]'));
  await tick();
  assert.deepEqual(dbCalls, ['items'], 'повторно список не запрашивается');
});

test('счётчик считает байты и подсвечивает переполнение', { skip }, () => {
  const { textarea, host } = mount({ lines: ['Привет'] });
  const counter = () => host.querySelector('[data-dialog-counter="0"]');
  assert.equal(counter().textContent, '12/1024');

  const ta = textarea();
  ta.value = 'a'.repeat(1100);
  ta.dispatchEvent(new host.ownerDocument.defaultView.Event('input', { bubbles: true }));
  assert.equal(counter().textContent, '1100/1024');
  assert.equal(counter().classList.contains('is-over'), true);
});

test('«Дублировать реплику» копирует текст и подстановки', { skip }, () => {
  const { host, click, editor, changes } = mount({ lines: ['Hi <player>', 'Bye'] });
  click(host.querySelector('[data-dialog-copy="0"]'));
  const lines = Array.from(editor.getData().DialogData, (line) => line.text);
  assert.deepEqual(lines, ['Hi <player>', 'Hi <player>', 'Bye']);
  assert.equal(changes.length, 1);
  assert.equal(changes[0].meta.path, 'DialogData');
});

test('подписи предметов подставляются, когда список догрузился из БД', { skip }, async () => {
  const { chips, dbCalls } = mount({ lines: ['Take <item_31>'], formats: {} });
  assert.equal(chips()[0].querySelector('.dlg-token-label').textContent, '#31');
  await tick();
  await tick();
  assert.equal(chips()[0].querySelector('.dlg-token-label').textContent, 'Iron Sword');
  assert.deepEqual(dbCalls, ['items']);
});

test('новая форма закрывает список, оставшийся от прежней', { skip }, async () => {
  const window = createWindow();
  const first = mount({ window, lines: [''] });
  first.click(first.host.querySelector('[data-dialog-insert="bot"]'));
  await tick();
  assert.ok(first.picker(), 'список первой формы открыт');

  const second = mount({ window, lines: [''] });
  assert.equal(second.picker(), null);
  assert.equal(window.document.body.querySelectorAll('.dlg-picker').length, 0, 'старый список закрыт');
});

test('record format links open the referenced editor without replacing or removing the token', { skip }, () => {
  const { window, host, textarea, changes, click } = mount({ lines: ['<item_31> <world_0> <bot_12>'] });
  const opened = [];
  window.EditorCore.RelatedRecords = { open: (...args) => opened.push(args.slice(0, 2)) };
  const original = textarea().value;
  host.querySelectorAll('[data-dialog-token-open]').forEach(click);
  assert.deepEqual(opened, [['items', 31], ['worlds', 0], ['bots', 12]]);
  assert.equal(textarea().value, original);
  assert.equal(changes.length, 0);
});
