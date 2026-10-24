/**
 * tests/db-editor-guest-revert.test.cjs — кнопка «вернуть как в базе» у поля,
 * помеченного «изм.», в гостевом черновике.
 *
 * Проверяется настоящий `DbEditor` (editor-core/db-editor-runtime.js) в DOM:
 * метки изменённых полей, наличие/отсутствие кнопки, клик по ней и значение,
 * которое попадает в форму. Форма подменена минимальной реализацией
 * `FormRuntime` — у рантайма с ней маленький контракт (mount/setData).
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

const DB_ROW = { ID: 7, Name: 'Material', Comment: null, RequiresProducts: null };

/** Значения полей формы «как из БД»: NULL → '' / 0 — так делает rowToModel. */
const rowToModel = (row) => ({
  ID: Number(row?.ID) || 0,
  Name: row?.Name ?? '',
  Comment: row?.Comment ?? '',
  RequiresProducts: Number(row?.RequiresProducts ?? 0),
});

/** Минимальная форма: один wrapper с data-field-path на каждое поле. */
const minimalFormRuntime = (window) => ({
  mount(host, { fields, data, onChange }) {
    const names = Object.keys(fields || {});
    const render = (model) => {
      host.innerHTML = '';
      for (const name of names) {
        const wrapper = window.document.createElement('div');
        wrapper.setAttribute('data-field-path', name);
        const input = window.document.createElement('input');
        input.type = 'text';
        input.value = String(model?.[name] ?? '');
        input.addEventListener('input', () => {
          model = { ...model, [name]: input.value };
          onChange?.(model, { path: name });
        });
        wrapper.appendChild(input);
        host.appendChild(wrapper);
      }
    };
    render(data);
    return {
      setData: (model) => render(model),
      destroy: () => { host.innerHTML = ''; },
    };
  },
});

function mountEditor({ draftOps = [], submitStatus = 'draft', rawRow = DB_ROW, search = '' } = {}) {
  const dom = new JSDOM(`<!DOCTYPE html><body><div id="root">
    <div data-editor-role="list"></div>
    <div data-editor-role="title"></div>
    <div data-editor-role="status"></div>
    <button type="button" data-editor-role="save"></button>
    <button type="button" data-editor-role="cancel"></button>
    <div data-editor-role="form"></div>
  </div></body>`, { url: `http://studio.local/items-editor.html${search}`, runScripts: 'outside-only' });
  const { window } = dom;
  const rootEl = window.document.getElementById('root');

  // Строка списка — как её отдаёт настоящий DBCrud: черновик гостя наложен.
  const overlayRow = {
    ...DB_ROW,
    ...(draftOps.find((op) => op.type === 'update')?.data || {}),
  };
  const calls = { list: 0, get: [], rawRow: [], revert: [], update: [] };
  const meta = {
    state: draftOps.some((op) => op.type === 'update') ? 'update' : (draftOps[0]?.type || null),
    status: submitStatus,
    reqId: submitStatus === 'submitted' ? 'CR-3' : null,
    fields: Object.keys(draftOps.find((op) => op.type === 'update')?.data || {}),
  };

  const Guest = {
    isGuest: () => true,
    ready: async () => 'guest',
    Draft: {
      rowMeta: () => (meta.state ? meta : null),
      revertField: async (resource, id, column, baseRow) => {
        calls.revert.push({ resource, id, column, baseRow });
        // Настоящий черновик убирает поле: дальше метка обязана исчезнуть.
        meta.fields = meta.fields.filter((field) => field !== column);
        return { ok: true, removed: true, dropped: meta.fields.length === 0 };
      },
    },
  };
  const DBCrud = {
    invalidateCache() {},
    list: async () => { calls.list += 1; return { ok: true, rows: [overlayRow] }; },
    // Записи, которой нет в списке, отдаём только через get (по ID).
    get: async (resource, id) => {
      calls.get.push(id);
      if (Number(id) === Number(overlayRow.ID)) return { ok: true, row: overlayRow };
      return { ok: true, row: { ...DB_ROW, ID: Number(id), Name: 'Fetched' } };
    },
    rawRow: async (resource, id) => { calls.rawRow.push({ resource, id }); return rawRow; },
    update: async (...args) => { calls.update.push(args); return { ok: true }; },
    create: async () => ({ ok: true, id: 8 }),
    remove: async () => ({ ok: true }),
  };

  // utils.js — обычный IIFE поверх window: выполняем прямо в окне jsdom.
  window.eval(source('utils.js'));
  window.EditorCore = window.EditorCore || {};
  Object.assign(window.EditorCore, {
    Guest,
    DBCrud,
    FormRuntime: minimalFormRuntime(window),
    RequestContext: { hasResource: () => false, isEditing: () => false, active: () => false },
    UI: { toast: () => {} },
    Page: { confirm: async () => true },
    defaults: { getFieldRenderOptions: () => ({}) },
    utils: window.EditorCore.utils,
  });
  // Ссылка «открыть запись» (?record=&field=) — так же, как её грузит страница.
  window.eval(source('deep-link.js'));
  window.eval(source('db-editor-runtime.js'));

  const editor = window.EditorCore.DbEditor.mount(rootEl, {
    resource: 'items',
    fields: { Name: { type: 'text' }, Comment: { type: 'text' }, RequiresProducts: { type: 'number' } },
    rowToModel,
    modelToPayload: (model) => ({ ...model }),
  });
  return { dom, window, rootEl, editor, calls, meta };
}

/** Дождаться, пока список загрузится и запись будет выбрана. */
const settle = () => new Promise((resolve) => setTimeout(resolve, 20));

const fieldNode = (rootEl, name) => rootEl.querySelector(`[data-field-path="${name}"]`);
const fieldValue = (rootEl, name) => fieldNode(rootEl, name)?.querySelector('input')?.value;

test('a changed draft field gets a revert button, untouched fields do not', { skip }, async () => {
  const { rootEl, editor } = mountEditor({
    draftOps: [{ type: 'update', resource: 'items', id: 7, data: { Name: 'Material v2' } }],
  });
  await settle();

  const changed = fieldNode(rootEl, 'Name');
  assert.equal(changed.classList.contains('guest-field-changed'), true);
  assert.equal(changed.classList.contains('guest-field-revertable'), true);
  assert.equal(!!changed.querySelector(':scope > .guest-field-revert'), true);

  for (const untouched of ['Comment', 'RequiresProducts']) {
    const node = fieldNode(rootEl, untouched);
    assert.equal(node.classList.contains('guest-field-changed'), false, untouched);
    assert.equal(node.querySelector('.guest-field-revert'), null, untouched);
  }
  // Значение формы — своё (из черновика), а не из базы.
  assert.equal(fieldValue(rootEl, 'Name'), 'Material v2');
  editor.destroy();
});

test('clicking revert restores the database value and drops the field from the draft', { skip }, async () => {
  const { rootEl, editor, calls } = mountEditor({
    draftOps: [{ type: 'update', resource: 'items', id: 7, data: { Name: 'Material v2', Comment: 'note' } }],
  });
  await settle();

  const commentField = fieldNode(rootEl, 'Comment');
  assert.equal(fieldValue(rootEl, 'Comment'), 'note');
  commentField.querySelector('.guest-field-revert')
    .dispatchEvent(new rootEl.ownerDocument.defaultView.MouseEvent('click', { bubbles: true }));
  await settle();

  assert.equal(calls.rawRow.length, 1, 'строка читается из базы, а не из черновика');
  assert.deepEqual(calls.rawRow[0], { resource: 'items', id: 7 });
  assert.equal(calls.revert.length, 1);
  assert.equal(calls.revert[0].column, 'Comment');
  assert.equal(calls.revert[0].baseRow.Comment, null, 'в черновик передаётся строка из базы');
  // Форма показывает значение из базы (NULL → '' в модели), метка снята.
  assert.equal(fieldValue(rootEl, 'Comment'), '');
  assert.equal(fieldNode(rootEl, 'Comment').classList.contains('guest-field-changed'), false);
  // Второе поле осталось изменённым и кнопку сохранило.
  assert.equal(fieldValue(rootEl, 'Name'), 'Material v2');
  assert.equal(!!fieldNode(rootEl, 'Name').querySelector(':scope > .guest-field-revert'), true);
  editor.destroy();
});

test('submitted and new records get no revert button', { skip }, async () => {
  const submitted = mountEditor({
    draftOps: [{ type: 'update', resource: 'items', id: 7, data: { Name: 'Material v2' } }],
    submitStatus: 'submitted',
  });
  await settle();
  assert.equal(fieldNode(submitted.rootEl, 'Name').classList.contains('guest-field-changed'), true);
  assert.equal(fieldNode(submitted.rootEl, 'Name').querySelector('.guest-field-revert'), null);
  submitted.editor.destroy();

  const created = mountEditor({
    draftOps: [{ type: 'create', resource: 'items', tempId: 2000000001, data: { Name: 'New item' } }],
  });
  await settle();
  assert.equal(fieldNode(created.rootEl, 'Name').querySelector('.guest-field-revert'), null);
  created.editor.destroy();
});

test('a failed database read leaves the draft untouched', { skip }, async () => {
  const { rootEl, editor, calls } = mountEditor({
    draftOps: [{ type: 'update', resource: 'items', id: 7, data: { Name: 'Material v2' } }],
    rawRow: null,
  });
  await settle();

  fieldNode(rootEl, 'Name').querySelector('.guest-field-revert')
    .dispatchEvent(new rootEl.ownerDocument.defaultView.MouseEvent('click', { bubbles: true }));
  await settle();

  assert.equal(calls.revert.length, 0, 'без значения из базы черновик не меняется');
  assert.equal(fieldValue(rootEl, 'Name'), 'Material v2');
  assert.equal(!!fieldNode(rootEl, 'Name').querySelector(':scope > .guest-field-revert'), true);
  editor.destroy();
});

test('a record link opens the record and highlights the field', { skip }, async () => {
  const { rootEl, editor } = mountEditor({
    draftOps: [{ type: 'update', resource: 'items', id: 7, data: { Name: 'Material v2' } }],
    search: '?resource=items&record=7&field=Comment',
  });
  await new Promise((resolve) => setTimeout(resolve, 400));

  assert.equal(fieldValue(rootEl, 'Name'), 'Material v2', 'открыта запись из ссылки');
  assert.equal(fieldNode(rootEl, 'Comment').classList.contains('field-link-target'), true);
  assert.equal(fieldNode(rootEl, 'Name').classList.contains('field-link-target'), false);
  editor.destroy();
});

test('a record link to a record outside the loaded list is fetched by id', { skip }, async () => {
  const { rootEl, editor, calls } = mountEditor({
    draftOps: [{ type: 'update', resource: 'items', id: 7, data: { Name: 'Material v2' } }],
    search: '?resource=items&record=99&field=Name',
  });
  await settle();

  // В списке только запись 7 — ссылку на 99 рантайм дочитывает по ID.
  assert.deepEqual(calls.get, [99]);
  assert.equal(fieldValue(rootEl, 'Name'), 'Fetched');
  editor.destroy();
});
