const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { JSDOM } = require('jsdom');
const source = (name) => fs.readFileSync(path.join(__dirname, '../editor-core', name), 'utf8');

function harness() {
  const dom = new JSDOM(`<body><div class="editor-dbcombo" data-datasource="items" data-db-searchable="1" data-bind-input-path="ItemID" data-current-value="1">
    <div class="editor-dbcombo-control"><input class="editor-dbcombo-input" type="search"></div>
    <div class="editor-dbcombo-dropdown"></div>
    <input class="editor-dbcombo-bound" data-path="ItemID" value="1" type="number"></div></body>`, {
    url: 'https://studio.test/crafts-editor.html', runScripts: 'outside-only',
  });
  const { window: w } = dom;
  w.eval(source('utils.js'));
  w.EditorCore.Http = { json: async () => ({ ok: true, data: { ok: true, items: [] } }) };
  w.eval(source('db.js'));
  const pending = [];
  w.EditorCore.DB.getOne = (source, id) => new Promise((resolve) => pending.push({ id, resolve }));
  return { dom, w, pending, input: w.document.querySelector('.editor-dbcombo-input'),
    bound: w.document.querySelector('.editor-dbcombo-bound'), db: w.EditorCore.DB };
}

test('late lookup labels cannot overwrite a more recent selected ID', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  const init = h.db.init();
  assert.equal(h.pending[0].id, '1');
  h.bound.value = '2'; h.bound.dispatchEvent(new h.w.Event('input'));
  h.pending[1].resolve({ ok: true, item: { value: 2, label: 'New choice' } });
  await new Promise((resolve) => setTimeout(resolve, 0));
  assert.equal(h.input.value, '2: New choice');
  h.pending[0].resolve({ ok: true, item: { value: 1, label: 'Old choice' } });
  await init;
  assert.equal(h.input.value, '2: New choice');
});

test('lookup completing while the user searches does not replace their search text', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  const init = h.db.init();
  h.input.focus();
  h.input.value = 'sword'; h.input.dispatchEvent(new h.w.Event('input', { bubbles: true }));
  h.pending[0].resolve({ ok: true, item: { value: 1, label: 'Old choice' } });
  await init;
  assert.equal(h.input.value, 'sword');
  h.input.dispatchEvent(new h.w.KeyboardEvent('keydown', { key: 'Escape' }));
});

test('independent combo labels initialize concurrently, not one network timeout per field', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  const second = h.w.document.querySelector('.editor-dbcombo').cloneNode(true);
  second.dataset.bindInputPath = 'OtherID';
  second.querySelector('.editor-dbcombo-bound').dataset.path = 'OtherID';
  h.w.document.body.appendChild(second);
  const init = h.db.init();
  assert.equal(h.pending.length, 2);
  h.pending.forEach(({ resolve }) => resolve({ ok: true, item: { value: 1, label: 'Item' } }));
  await init;
});

test('remount cancels old lookups and binds the replacement combo exactly once', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  const oldCombo = h.w.document.querySelector('.editor-dbcombo');
  const replacement = oldCombo.cloneNode(true);
  const first = h.db.init();
  oldCombo.replaceWith(replacement);
  h.db.cancelInit();
  const second = h.db.init();
  assert.equal(h.pending.length, 2);
  h.pending[1].resolve({ ok: true, item: { value: 1, label: 'Current' } });
  await second;
  const input = replacement.querySelector('.editor-dbcombo-input');
  assert.equal(input.value, '1: Current');
  h.pending[0].resolve({ ok: true, item: { value: 1, label: 'Stale' } });
  await first;
  assert.equal(input.value, '1: Current');
  const bound = replacement.querySelector('.editor-dbcombo-bound');
  bound.value = '2'; bound.dispatchEvent(new h.w.Event('input'));
  assert.equal(h.pending.length, 3, 'disposed bound-input listeners must not fetch again');
  h.pending[2].resolve({ ok: true, item: { value: 2, label: 'Two' } });
  await new Promise(resolve => setTimeout(resolve, 0));
  assert.equal(input.value, '2: Two');
  h.db.cancelInit();
});

test('a page-wide pass cannot steal or disable a nested form lookup', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  const form = h.w.document.createElement('div');
  h.w.document.querySelector('.editor-dbcombo').replaceWith(form);
  const combo = h.w.document.createElement('div');
  combo.className = 'editor-dbcombo';
  combo.dataset.datasource = 'items';
  combo.dataset.bindInputPath = 'ItemID';
  combo.innerHTML = '<input class="editor-dbcombo-input"><div class="editor-dbcombo-dropdown"></div><input data-path="ItemID" value="1">';
  form.appendChild(combo);
  const nested = h.db.init(form);
  const page = h.db.init(h.w.document);
  await page;
  assert.equal(h.pending.length, 1);
  assert.equal(h.db.isInitOwner(form, combo), true);
  assert.equal(h.db.isInitOwner(h.w.document, combo), false);
  h.pending[0].resolve({ ok: true, item: { value: 1, label: 'Nested' } });
  await nested;
  assert.equal(combo.querySelector('.editor-dbcombo-input').value, '1: Nested');
  h.db.cancelInit(form);
});

test('bulk lookup responses cannot restore options in a disposed form', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  const form = h.w.document.createElement('div');
  form.innerHTML = '<div class="editor-dbselect"><select data-datasource="items" data-bind-input-path="ItemID"></select></div><input data-path="ItemID" value="1">';
  h.w.document.body.replaceChildren(form);
  const finishes = [];
  const signals = [];
  h.db.list = (source, options) => {
    signals.push(options.signal);
    return new Promise(resolve => { finishes.push(resolve); });
  };
  const first = h.db.init(form);
  const oldSelect = form.querySelector('select');
  const oldOptions = Array.from(oldSelect.options).map(option => option.label);
  form.innerHTML = '<div class="editor-dbselect"><select data-datasource="items"></select></div>';
  const second = h.db.init(form);
  assert.equal(signals.length, 2);
  assert.equal(signals[0].aborted, true);
  finishes.forEach(finish => finish({ ok: true, items: [{ value: 1, label: 'One' }], has_more: false }));
  await Promise.all([first, second]);
  assert.deepEqual(Array.from(form.querySelector('select').options).map(option => option.label), ['— select —', '1: One']);
  assert.deepEqual(Array.from(oldSelect.options).map(option => option.label), oldOptions);
  h.db.cancelInit(form);
});
