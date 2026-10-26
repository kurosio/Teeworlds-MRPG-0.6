/**
 * tests/record-navigation.test.cjs — навигация при активной работе:
 *
 * 1. Горячие клавиши оболочки (Ctrl+Tab, Alt+←/→…) срабатывают и когда фокус
 *    внутри редактора: редактор пересылает их студии (editor-shell:navigate),
 *    а студия ведёт историю переходов между редакторами.
 * 2. Над списком записей каждого редактора — кнопки «предыдущая/следующая»
 *    и позиция «N / M»; по списку ходят стрелками, Tab заходит один раз,
 *    фокус не теряется при перерисовке списка.
 * 3. DbEditor открывает последнюю запись, а не первую строку списка.
 *
 * Поведенческая часть нуждается в jsdom (`npm install --no-save jsdom`);
 * без него эти тесты пропускаются, статическая часть работает всегда.
 */
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');
const readRoot = (name) => fs.readFileSync(path.join(root, name), 'utf8');

let JSDOM = null;
try { ({ JSDOM } = require('jsdom')); } catch { JSDOM = null; }
const skip = JSDOM ? false : 'jsdom не установлен (npm install --no-save jsdom)';

const tick = async () => {
  await new Promise((resolve) => setTimeout(resolve, 10));
  await new Promise((resolve) => setTimeout(resolve, 10));
};

// ── Статика: оболочка и пересылка клавиш ────────────────────────────────────

test('shell accepts navigation commands from editor frames and keeps editor history', () => {
  const shell = readRoot('index.html');
  assert.ok(shell.includes("'editor-shell:navigate'"), 'index.html не обрабатывает editor-shell:navigate');
  assert.ok(shell.includes('walkNavHistory'), 'нет истории переходов между редакторами');
  assert.ok(shell.includes('pushNavHistory(e.key)'), 'openEditor не записывает переход в историю');
  assert.ok(shell.includes('id="navBackBtn"') && shell.includes('id="navForwardBtn"'), 'нет кнопок назад/вперёд');
  // Подсказка «сначала щёлкните шапку» больше не нужна и не должна вернуться:
  // клавиши теперь работают изнутри редактора.
  assert.ok(!shell.includes('Shortcuts work when the shell has focus'), 'осталась старая оговорка про фокус оболочки');

  const uiManager = source('ui-manager.js');
  assert.ok(uiManager.includes('forwardToShell'), 'редактор не пересылает клавиши оболочке');
  assert.ok(uiManager.includes('mountRecordNav'), 'нет навигации по записям над списком');

  const runtime = source('db-editor-runtime.js');
  assert.ok(runtime.includes('editor-core:last-record'), 'DbEditor не запоминает последнюю запись');
});

// ── Список записей: панель, стрелки, фокус ──────────────────────────────────

function mountListEditor(window) {
  const { document } = window;
  const list = document.querySelector('[data-editor-role="list"]');
  const renderRows = (activeId) => {
    list.innerHTML = `<div class="editor-list">${[1, 2, 3, 4, 5].map((id) => `
      <button type="button" class="editor-row${id === activeId ? ' editor-row-active' : ''}" data-id="${id}">Record ${id}</button>`).join('')}</div>`;
  };
  // Имитация редактора: клик по строке выбирает её и перерисовывает список.
  list.addEventListener('click', (event) => {
    const btn = event.target.closest('[data-id]');
    if (btn) renderRows(Number(btn.dataset.id));
  });
  renderRows(2);
  return { list, renderRows };
}

test('record nav: position counter, prev/next buttons and roving tabindex', { skip }, async () => {
  const dom = new JSDOM(`<!DOCTYPE html><body data-editor="items">
    <input data-editor-role="search" type="search">
    <div data-editor-role="list"></div>
  </body>`, { url: 'http://studio.local/items-editor.html', runScripts: 'outside-only' });
  const { window } = dom;
  const { document } = window;
  window.eval(source('utils.js'));
  mountListEditor(window);
  window.eval(source('ui-manager.js'));
  await window.EditorCore.UIManager.init(document);
  await tick();

  const bar = document.querySelector('[data-editor-record-nav]');
  assert.ok(bar, 'панель навигации по записям не появилась над списком');
  assert.equal(bar.querySelector('[data-record-nav-pos]').textContent, '2 / 5');

  const rows = [...document.querySelectorAll('.editor-row')];
  assert.equal(rows[1].getAttribute('tabindex'), '0', 'активная строка — единственная точка входа Tab');
  assert.equal(rows[0].getAttribute('tabindex'), '-1');
  assert.equal(rows[4].getAttribute('tabindex'), '-1');

  bar.querySelector('[data-record-nav="next"]').click();
  await tick();
  assert.ok(document.querySelector('.editor-row[data-id="3"]').classList.contains('editor-row-active'));
  assert.equal(bar.querySelector('[data-record-nav-pos]').textContent, '3 / 5');
});

test('record nav: arrow keys move through the list and focus survives re-render', { skip }, async () => {
  const dom = new JSDOM(`<!DOCTYPE html><body data-editor="items">
    <input data-editor-role="search" type="search">
    <div data-editor-role="list"></div>
  </body>`, { url: 'http://studio.local/items-editor.html', runScripts: 'outside-only' });
  const { window } = dom;
  const { document } = window;
  window.eval(source('utils.js'));
  mountListEditor(window);
  window.eval(source('ui-manager.js'));
  await window.EditorCore.UIManager.init(document);
  await tick();

  const pressOnActive = async (key) => {
    const active = document.querySelector('.editor-row-active');
    active.focus();
    active.dispatchEvent(new window.KeyboardEvent('keydown', { key, bubbles: true }));
    await tick();
  };

  await pressOnActive('ArrowDown');
  let active = document.querySelector('.editor-row-active');
  assert.equal(active.dataset.id, '3', 'ArrowDown не выбрал следующую запись');
  // Список был перерисован (innerHTML) — сфокусированная строка уничтожена,
  // но фокус обязан вернуться на новую активную строку, а не на body.
  assert.equal(document.activeElement, active, 'фокус потерян после перерисовки списка');

  await pressOnActive('End');
  active = document.querySelector('.editor-row-active');
  assert.equal(active.dataset.id, '5');
  assert.equal(document.querySelector('[data-record-nav="next"]').disabled, true, 'на последней записи «вперёд» недоступна');

  await pressOnActive('PageUp');
  active = document.querySelector('.editor-row-active');
  assert.equal(active.dataset.id, '1', 'PageUp должен прыгать через 10 записей (с клампом к началу)');
});

test('shell shortcuts are forwarded from inside the editor, but not out of text fields', { skip }, async () => {
  const dom = new JSDOM(`<!DOCTYPE html><body data-editor="items">
    <input data-editor-role="search" type="search">
    <div data-editor-role="list"></div>
  </body>`, { url: 'http://studio.local/items-editor.html', runScripts: 'outside-only' });
  const { window } = dom;
  const { document } = window;
  window.eval(source('utils.js'));
  mountListEditor(window);

  const sent = [];
  window.EditorCore.Http = { postMessage: (target, data) => { sent.push(data); return true; } };
  Object.defineProperty(window, 'parent', { value: { shell: true }, configurable: true });
  assert.notEqual(window.parent, window, 'не удалось подменить parent: тест пересылки бессмыслен');

  window.eval(source('ui-manager.js'));
  await window.EditorCore.UIManager.init(document);
  await tick();

  const press = (el, init) => el.dispatchEvent(new window.KeyboardEvent('keydown', { bubbles: true, ...init }));

  // Объекты созданы в realm jsdom — сравниваем через JSON, а не deepStrictEqual.
  const lastSent = () => JSON.parse(JSON.stringify(sent.at(-1)));

  press(document.body, { key: 'Tab', ctrlKey: true });
  assert.deepEqual(lastSent(), { type: 'editor-shell:navigate', action: 'cycle-tab', dir: 1 });

  press(document.body, { key: 'ArrowLeft', altKey: true });
  assert.deepEqual(lastSent(), { type: 'editor-shell:navigate', action: 'history-back' });

  // В поле поиска Alt+← — переход по словам, а не «назад по редакторам».
  const before = sent.length;
  const search = document.querySelector('[data-editor-role="search"]');
  search.focus();
  press(search, { key: 'ArrowLeft', altKey: true });
  assert.equal(sent.length, before, 'Alt+← из поля ввода пересылаться не должен');
});

// ── DbEditor: последняя запись ──────────────────────────────────────────────

test('DbEditor reopens the last record and remembers the next selection', { skip }, async () => {
  const rows = [
    { ID: 1, Name: 'First' },
    { ID: 2, Name: 'Second' },
    { ID: 3, Name: 'Third' },
  ];
  const dom = new JSDOM(`<!DOCTYPE html><body><div id="root">
    <div data-editor-role="list"></div>
    <div data-editor-role="title"></div>
    <div data-editor-role="status"></div>
    <button type="button" data-editor-role="save"></button>
    <div data-editor-role="form"></div>
  </div></body>`, { url: 'http://studio.local/items-editor.html', runScripts: 'outside-only' });
  const { window } = dom;
  window.localStorage.setItem('editor-core:last-record:items', '3');

  window.eval(source('utils.js'));
  window.EditorCore = window.EditorCore || {};
  Object.assign(window.EditorCore, {
    DBCrud: {
      list: async () => ({ ok: true, rows }),
      get: async (resource, id) => ({ ok: true, row: rows.find((r) => r.ID === Number(id)) }),
      rawRow: async (resource, id) => rows.find((r) => r.ID === Number(id)),
    },
    FormRuntime: {
      mount(host, { data }) {
        host.textContent = data?.Name || '';
        return { setData: (model) => { host.textContent = model?.Name || ''; }, destroy: () => {} };
      },
    },
    Page: { confirm: async () => true },
    UI: { toast: () => {} },
  });
  window.eval(source('db-editor-runtime.js'));

  const dirty = { isDirty: () => false, setClean() {}, markDirty() {} };
  window.EditorCore.DbEditor.mount(window.document.getElementById('root'), {
    resource: 'items',
    fields: { Name: {} },
    dirty,
  });
  await tick();

  const title = window.document.querySelector('[data-editor-role="title"]');
  assert.equal(title.textContent, 'Third', 'редактор открылся не на последней записи');

  window.document.querySelector('button[data-id="1"]').click();
  await tick();
  assert.equal(title.textContent, 'First');
  assert.equal(window.localStorage.getItem('editor-core:last-record:items'), '1', 'новый выбор не запомнился');
});
