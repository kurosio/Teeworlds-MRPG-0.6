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
