const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { JSDOM } = require('jsdom');
const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');

function harness({ guest = false, pages = [], reply = true } = {}) {
  const dom = new JSDOM('<body><input id="draft" value="unsaved"><button id="trigger">link</button></body>', {
    url: 'https://studio.test/crafts-editor.html', runScripts: 'outside-only',
  });
  const { window: w } = dom;
  w.HTMLDialogElement.prototype.showModal = function () { this.open = true; };
  w.HTMLDialogElement.prototype.close = function () { this.open = false; };
  const calls = { confirm: 0, refresh: 0, forwarded: 0, messages: [] };
  let approved = false;
  let dirty = false;
  w.eval(fs.readFileSync(path.join(root, 'studio.manifest.js'), 'utf8'));
  w.EditorCore = {
    Guest: { ready: async () => guest ? 'guest' : 'admin', isGuest: () => guest, accountPages: () => pages },
    DB: { invalidateCache: () => calls.refresh++ },
    UI: { toast: (text) => calls.messages.push(text) },
    Page: { confirm: async () => { calls.confirm++; return approved; } },
    Http: {
      isSameOriginMessage: (event, child) => event.origin === w.location.origin && event.source === child,
      postMessage: (child, data) => {
        if (data.type !== 'editor-shell:request-dirty-status' || !reply) return;
        w.dispatchEvent(new w.MessageEvent('message', { origin: w.location.origin, source: child,
          data: { type: 'editor-shell:dirty-status', requestId: data.requestId, dirty } }));
      },
    },
  };
  // Keep the fail-closed timeout test fast without changing production timing.
  const originalTimeout = w.setTimeout.bind(w);
  w.setTimeout = (fn, ms) => originalTimeout(fn, ms === 1800 ? 10 : ms);
  w.eval(source('related-records.js'));
  return { dom, w, api: w.EditorCore.RelatedRecords, calls,
    dirty: (v) => { dirty = v; }, approve: (v) => { approved = v; } };
}

test('all editable lookup sources resolve; invalid IDs and read-only catalogues do not', (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  for (const [source, key] of Object.entries({ items: 'items', bots: 'bots_info', worlds: 'worlds', quests: 'quests', quest_boards: 'quest_boards' })) {
    assert.equal(h.api.targetFor(source, 42).editor.key, key);
  }
  assert.equal(h.api.targetFor('worlds', 0).id, 0);
  for (const value of ['', ' ', null, undefined, -1, 1.5, Infinity, 'bad', Number.MAX_SAFE_INTEGER + 1, 0]) {
    assert.equal(h.api.targetFor('items', value), null);
  }
  for (const source of ['attributes', 'skills', '__proto__', 'constructor', 'https://evil.test']) {
    assert.equal(h.api.targetFor(source, 1), null);
  }
});

test('dynamic controls are linked once; links follow current values, including manual mode and world 0', (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  h.w.document.body.insertAdjacentHTML('beforeend', `
    <div class="editor-dbcombo" data-datasource="items"><input class="editor-dbcombo-bound" value="42"></div>
    <select data-position-world-select><option value="0">Main</option></select>
    <select data-datasource="quests"><option value="">None</option><option value="9">Quest</option></select>
    <div class="editor-dbcombo" data-datasource="skills"><input class="editor-dbcombo-bound" value="1"></div>`);
  h.api.init(); h.api.init();
  const buttons = h.w.document.querySelectorAll('.related-record-link');
  assert.equal(buttons.length, 3);
  assert.deepEqual(Array.from(buttons, (b) => b.disabled), [false, false, true]);
  const input = h.w.document.querySelector('.editor-dbcombo-bound');
  input.value = '0'; input.dispatchEvent(new h.w.Event('input'));
  assert.equal(buttons[0].disabled, true);
  input.value = '77'; h.api.init();
  assert.equal(buttons[0].disabled, false, 'programmatic updates can refresh without dirtying the form');
});

test('drill-down keeps the source and ancestor frames alive; return restores focus and refreshes labels only', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  const trigger = h.w.document.getElementById('trigger');
  await h.api.open('items', 42, trigger);
  const dialog = h.w.document.querySelector('dialog');
  const first = dialog.querySelector('iframe');
  assert.match(first.src, /items-editor.html\?resource=items&record=42$/);
  first.contentDocument.open();
  first.contentDocument.write('<body><input value="unfinished linked draft"></body>');
  first.contentDocument.close();
  await h.api.open('worlds', 0, first.contentDocument.querySelector('input'));
  assert.equal(h.w.document.querySelectorAll('dialog').length, 1);
  assert.equal(dialog.querySelectorAll('iframe').length, 2);
  assert.equal(first.hidden, true);
  assert.match(dialog.querySelector('strong').textContent, /Items #42 › Worlds #0/);
  await h.api.close();
  assert.equal(first.hidden, false);
  assert.equal(first.contentDocument.querySelector('input').value, 'unfinished linked draft');
  await h.api.close();
  assert.equal(h.api.hasOpenEditors(), false);
  assert.equal(h.w.document.getElementById('draft').value, 'unsaved');
  assert.equal(h.w.document.activeElement, trigger);
  assert.equal(h.calls.confirm, 0);
  assert.equal(h.calls.refresh, 2);
});

test('dirty linked record: cancel keeps it open, confirm returns without touching the source', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  await h.api.open('quests', 9);
  h.dirty(true);
  await h.api.close();
  assert.equal(h.calls.confirm, 1);
  assert.equal(h.api.hasOpenEditors(), true);
  h.approve(true); await h.api.close();
  assert.equal(h.api.hasOpenEditors(), false);
  assert.equal(h.w.document.getElementById('draft').value, 'unsaved');
});

test('a missing dirty reply is not interpreted as permission to discard', async (t) => {
  const h = harness({ reply: false }); t.after(() => h.dom.window.close());
  await h.api.open('bots', 1); await h.api.close();
  assert.equal(h.calls.confirm, 1);
  assert.equal(h.api.hasOpenEditors(), true);
});

test('guest lookup access is not editor access; rapid clicks cannot open duplicate frames', async (t) => {
  const h = harness({ guest: true, pages: ['crafts', 'worlds'] }); t.after(() => h.dom.window.close());
  assert.equal(await h.api.open('items', 1), false);
  assert.equal(h.api.hasOpenEditors(), false);
  await Promise.all([h.api.open('worlds', 1), h.api.open('worlds', 1)]);
  assert.equal(h.w.document.querySelectorAll('iframe').length, 1);
});

test('only the owned active iframe can request Back; spoofed messages are ignored', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  await h.api.open('items', 1);
  const frame = h.w.document.querySelector('iframe');
  const send = (origin, source) => h.w.dispatchEvent(new h.w.MessageEvent('message', {
    origin, source, data: { type: 'editor-shell:navigate', action: 'history-back' },
  }));
  send('https://evil.test', frame.contentWindow);
  send(h.w.location.origin, h.w);
  assert.equal(h.api.hasOpenEditors(), true);
  send(h.w.location.origin, frame.contentWindow);
  await new Promise((resolve) => setTimeout(resolve, 0));
  assert.equal(h.api.hasOpenEditors(), false);
});

test('Page.confirm is mounted inside the topmost native dialog', async (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  delete h.w.EditorCore.Page;
  h.w.eval(source('page-kit.js'));
  h.w.document.body.insertAdjacentHTML('beforeend', '<dialog open id="outer"></dialog><dialog open id="inner"></dialog>');
  const result = h.w.EditorCore.Page.confirm('Delete?', { danger: true });
  assert.ok(h.w.document.querySelector('#inner .ec-dialog-backdrop'));
  assert.equal(h.w.document.querySelector('#outer .ec-dialog-backdrop'), null);
  h.w.document.querySelector('.ec-cancel').click();
  assert.equal(await result, false);
});

test('comma-separated module IDs have individual links and never change the stored CSV', (t) => {
  const h = harness(); t.after(() => h.dom.window.close());
  h.w.document.body.insertAdjacentHTML('beforeend', '<input data-related-list-source="items" value="90,91,35,90,0,bad">');
  h.api.init(); h.api.init();
  assert.equal(h.w.document.querySelectorAll('.related-record-list').length, 1);
  assert.equal(h.w.document.querySelectorAll('.related-record-list button').length, 3);
  const input = h.w.document.querySelector('[data-related-list-source]');
  assert.equal(input.value, '90,91,35,90,0,bad');
  input.value = '42'; input.dispatchEvent(new h.w.Event('input'));
  assert.equal(h.w.document.querySelectorAll('.related-record-list button').length, 1);
});
