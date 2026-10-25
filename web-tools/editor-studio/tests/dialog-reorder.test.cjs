/**
 * tests/dialog-reorder.test.cjs — перестановка реплик диалога.
 *
 * Проверяется настоящий `DialogEditor` (editor-core/dialog-editor.js) в DOM:
 * порядок реплик меняют кнопки ↑/↓, клавиатура на рукоятке и перетаскивание.
 * Ни `window.Sortable`, ни сеть не нужны: на странице NPC-редактора внешняя
 * библиотека не подключена, и раньше из-за этого реплики не двигались.
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

const LINES = ['First', 'Second', 'Third'];
const ITEM_HEIGHT = 100;

/** Минимальный FieldRenderer: одно поле — один input с data-path. */
const minimalFieldRenderer = {
  renderField: ({ path: fieldPath }) => `<input type="text" data-path="${fieldPath}" value="" />`,
  getInputValue: (input) => input.value,
  setValueAtPath: () => {},
};

const mountEditor = ({ lines = LINES } = {}) => {
  const dom = new JSDOM('<!doctype html><html><body><div id="host"></div></body></html>', {
    runScripts: 'outside-only',
    url: 'http://localhost/bots-npc-editor.html',
  });
  const { window } = dom;
  window.EditorCore = {
    FieldRenderer: minimalFieldRenderer,
    UIManager: { init() {} },
    utils: { escapeHtml: (v) => String(v ?? '') },
  };
  window.eval(source('dialog-editor.js'));

  const host = window.document.getElementById('host');
  const changes = [];
  const editor = window.EditorCore.DialogEditor.mount(host, {
    data: { DialogData: lines.map((text) => ({ text })) },
    fieldOptions: {},
    sideOptions: [{ value: 'default', label: 'Default' }],
    onChange: (data, meta) => changes.push({ data, meta }),
  });

  const listEl = host.querySelector('[data-dialog-items="list"]');
  // jsdom не считает раскладку: карточки получают стабильные «пиксели»,
  // поэтому перетаскивание можно проверить по-настоящему.
  const layout = () => {
    const items = Array.from(listEl.querySelectorAll('[data-dialog-item]'));
    items.forEach((el) => {
      el.getBoundingClientRect = () => {
        const index = Array.from(listEl.querySelectorAll('[data-dialog-item]')).indexOf(el);
        return {
          top: index * ITEM_HEIGHT,
          bottom: index * ITEM_HEIGHT + ITEM_HEIGHT,
          height: ITEM_HEIGHT,
          left: 0,
          right: 100,
          width: 100,
          x: 0,
          y: index * ITEM_HEIGHT,
        };
      };
    });
  };
  layout();

  return { window, host, editor, changes, layout, listEl };
};

const texts = (editor) => Array.from(editor.getData().DialogData, (item) => item.text);
const domTexts = (host) => Array.from(host.querySelectorAll('.editor-item-summary'))
  .map((el) => el.textContent);

test('кнопки ↑/↓ меняют порядок реплик и сообщают об изменении', { skip }, () => {
  const { window, host, editor, changes } = mountEditor();

  const buttons = host.querySelectorAll('[data-dialog-move]');
  assert.equal(buttons.length, LINES.length * 2, 'у каждой реплики две кнопки перемещения');
  assert.equal(host.querySelector('[data-dialog-move="up"]').disabled, true, 'первая реплика не идёт выше');
  assert.equal(
    host.querySelectorAll('[data-dialog-item]')[LINES.length - 1]
      .querySelector('[data-dialog-move="down"]').disabled,
    true,
    'последняя реплика не идёт ниже',
  );

  host.querySelector('[data-dialog-item="0"] [data-dialog-move="down"]')
    .dispatchEvent(new window.MouseEvent('click', { bubbles: true }));

  assert.deepEqual(texts(editor), ['Second', 'First', 'Third']);
  assert.deepEqual(domTexts(host), ['Second', 'First', 'Third'], 'список перерисован в новом порядке');
  assert.equal(changes.length, 1);
  assert.equal(changes[0].meta.path, 'DialogData');
  assert.deepEqual(Array.from(changes[0].meta.value, (item) => item.text), ['Second', 'First', 'Third']);
  assert.deepEqual(Array.from(changes[0].meta.oldValue, (item) => item.text), LINES);
});

test('клавиатура на рукоятке двигает реплику', { skip }, () => {
  const { window, host, editor } = mountEditor();
  const handle = host.querySelector('[data-dialog-item="2"] [data-dialog-handle]');
  assert.ok(handle, 'у реплики есть рукоятка');
  assert.equal(handle.getAttribute('tabindex'), '0', 'рукоятка доступна с клавиатуры');

  handle.dispatchEvent(new window.KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true, cancelable: true }));
  assert.deepEqual(texts(editor), ['First', 'Third', 'Second']);
  assert.equal(
    window.document.activeElement,
    host.querySelector('[data-dialog-item="1"] [data-dialog-handle]'),
    'фокус остаётся на перемещённой реплике',
  );

  host.querySelector('[data-dialog-item="0"] [data-dialog-handle]')
    .dispatchEvent(new window.KeyboardEvent('keydown', { key: 'ArrowDown', bubbles: true, cancelable: true }));
  assert.deepEqual(texts(editor), ['Third', 'First', 'Second']);
});

test('перетаскивание за рукоятку работает без Sortable с CDN', { skip }, () => {
  const { window, host, editor, changes, layout } = mountEditor();
  assert.equal(window.Sortable, undefined, 'внешняя библиотека не нужна');

  const dragItem = (fromSelector, pointerY) => {
    const handle = host.querySelector(`${fromSelector} [data-dialog-handle]`);
    handle.dispatchEvent(new window.MouseEvent('pointerdown', { bubbles: true, cancelable: true, clientY: 0 }));
    window.dispatchEvent(new window.MouseEvent('pointermove', { bubbles: true, cancelable: true, clientY: pointerY }));
    layout();
    window.dispatchEvent(new window.MouseEvent('pointerup', { bubbles: true, cancelable: true, clientY: pointerY }));
  };

  // 260 — ниже середины третьей карточки: первая реплика уезжает в конец.
  dragItem('[data-dialog-item="0"]', LINES.length * ITEM_HEIGHT - 40);

  assert.deepEqual(texts(editor), ['Second', 'Third', 'First']);
  assert.deepEqual(domTexts(host), ['Second', 'Third', 'First']);
  assert.equal(changes.length, 1, 'перетаскивание сохраняется одним изменением формы');
  assert.equal(changes[0].meta.path, 'DialogData');
});

test('случайный клик по рукоятке не меняет порядок', { skip }, () => {
  const { window, host, editor, changes } = mountEditor();
  const handle = host.querySelector('[data-dialog-item="1"] [data-dialog-handle]');
  handle.dispatchEvent(new window.MouseEvent('pointerdown', { bubbles: true, cancelable: true, clientY: 10 }));
  window.dispatchEvent(new window.MouseEvent('pointerup', { bubbles: true, cancelable: true, clientY: 10 }));
  assert.deepEqual(texts(editor), LINES);
  assert.equal(changes.length, 0);
});
