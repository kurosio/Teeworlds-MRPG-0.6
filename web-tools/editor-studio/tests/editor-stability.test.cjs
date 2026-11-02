const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { JSDOM } = require('jsdom');

const root = path.join(__dirname, '..');
const source = name => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');
const tick = () => new Promise(resolve => setTimeout(resolve, 25));
const validation = require('../editor-core/validation.js');

function dom(html = '<div id="root"></div>') {
  return new JSDOM(`<body>${html}</body>`, {
    url: 'https://studio.test/items-editor.html', runScripts: 'outside-only', pretendToBeVisual: true,
  });
}
function load(window, ...names) {
  for (const name of names) window.eval(source(name));
}
function formHarness(t, fields, data = {}) {
  const d = dom();
  t.after(() => d.window.close());
  const w = d.window;
  load(w, 'utils.js', 'validation.js', 'field-renderer.js', 'ui-manager.js', 'form-runtime.js');
  const changes = [];
  const host = w.document.getElementById('root');
  const form = w.EditorCore.FormRuntime.mount(host, {
    fields, data, onChange: (model, meta) => changes.push({ model, ...meta }),
  });
  return { w, host, form, changes };
}

test('validation: required, whitespace, finite numbers, bounds, zero and malformed rules', () => {
  const check = (value, spec, type = 'text') => validation.validateValue({ value, spec, inputType: type }).ok;
  assert.equal(check('  ', { required: true }), false);
  assert.equal(check([], { required: true }), false);
  assert.equal(check(0, { required: true, min: 0 }, 'number'), true, 'world 0 is a value');
  assert.equal(check('0', { required: true, min: 0 }, 'number'), true);
  assert.equal(check('', { min: 1 }, 'number'), true, 'optional empty field is not a bound violation');
  for (const value of [NaN, Infinity, 'abc', '']) assert.equal(check(value, { min: 0 }, 'number'), value === '');
  assert.equal(check(-1, { min: 0 }, 'number'), false);
  assert.equal(check(11, { max: 10 }, 'number'), false);
  assert.equal(check(1.5, { integer: true }, 'number'), false);
  assert.equal(check('12', { pattern: '^\\d+$' }), true);
  assert.equal(check('x', { pattern: '^\\d+$' }), false);
  assert.equal(check('x', { pattern: '[' }), false, 'broken schema must not silently pass');
  assert.equal(check('x', null), false);
  assert.equal(validation.validateValue({ value: '', spec: {}, badInput: true }).ok, false);
});

test('validation binds once, announces errors and ignores disabled/hidden controls', t => {
  const d = dom(`<div id="root">
    <div data-field-path="Name"><input data-path="Name" data-validate='{"required":true}' value=""><div class="hidden" data-hint-for="Name"></div></div>
    <input data-path="Hidden" data-validate='{"required":true}' value="" hidden>
    <input data-path="Disabled" data-validate='{"required":true}' value="" disabled>
  </div>`);
  t.after(() => d.window.close());
  const w = d.window;
  load(w, 'utils.js', 'validation.js');
  const host = w.document.getElementById('root');
  w.EditorCore.Validation.bind(host);
  const dispose = w.EditorCore.Validation.bind(host);
  let result = w.EditorCore.Validation.validateRoot(host);
  assert.equal(result.errors.length, 1);
  const input = host.querySelector('input');
  assert.equal(input.getAttribute('aria-invalid'), 'true');
  assert.equal(input.getAttribute('aria-required'), 'true');
  const hint = host.querySelector('[data-hint-for]');
  assert.equal(hint.textContent, 'Required field');
  assert.equal(input.getAttribute('aria-describedby'), hint.id);
  input.value = 'A';
  input.dispatchEvent(new w.Event('input', { bubbles: true }));
  input.dispatchEvent(new w.Event('change', { bubbles: true }));
  assert.equal(hint.textContent, '');
  assert.equal(input.hasAttribute('aria-describedby'), false);
  assert.equal(w.EditorCore.Validation.validateRoot(host).ok, true);
  dispose();
  dispose();
  assert.equal(host.__editorValidationDispose, undefined);
  input.value = '';
  input.dispatchEvent(new w.Event('input', { bubbles: true }));
  assert.equal(input.getAttribute('aria-invalid'), 'false', 'disposed listener must not run');
});

test('form remount/destroy owns one set of handlers and keeps conditional-edit focus', t => {
  const fields = { Name: { type: 'text', label: 'Name' }, Count: { type: 'number', ui: { min: 1 } } };
  const h = formHarness(t, fields, { Name: 'Old', Count: 1 });
  const input = h.host.querySelector('[data-path="Name"]');
  input.focus();
  input.value = 'New';
  input.setSelectionRange(2, 2);
  input.dispatchEvent(new h.w.Event('input', { bubbles: true }));
  h.form.setData(h.form.getData());
  assert.equal(h.changes.length, 1, 'remount must remove the previous form handlers');
  assert.equal(h.w.document.activeElement.dataset.path, 'Name');
  assert.equal(h.w.document.activeElement.selectionStart, 2);
  h.form.destroy();
  h.form.destroy();
  h.host.querySelector('[data-path="Name"]').dispatchEvent(new h.w.Event('input', { bubbles: true }));
  assert.equal(h.changes.length, 1);
});

test('list add/remove updates model once and validates nested numeric fields', t => {
  const h = formHarness(t, { Items: { type: 'list', itemDefault: { count: 1 }, itemFields: {
    count: { type: 'number', validate: { min: 1 } },
  } } }, { Items: [] });
  h.host.querySelector('[data-list-action="add"]').click();
  assert.deepEqual(JSON.parse(JSON.stringify(h.form.getData().Items)), [{ count: 1 }]);
  assert.equal(h.changes.length, 1);
  const count = h.host.querySelector('[data-path="Items.0.count"]');
  count.value = '-1';
  count.dispatchEvent(new h.w.Event('input', { bubbles: true }));
  assert.equal(h.form.validate().ok, false);
  h.host.querySelector('[data-list-action="remove"]').click();
  assert.deepEqual(h.form.getData().Items, []);
  assert.equal(h.form.validate().ok, true);
});

// Drive HTTP deadlines deterministically: a busy CI process must not decide
// whether a 15 ms timer wins a microtask race.
function clock() {
  let now = 0;
  let sequence = 0;
  const timers = new Map();
  return {
    now: () => now,
    setNow(value) { now = value; },
    setTimeout(fn, delay) { const id = ++sequence; timers.set(id, { at: now + delay, fn }); return id; },
    clearTimeout(id) { timers.delete(id); },
    advance(ms) {
      now += ms;
      for (const [id, timer] of [...timers].sort((a, b) => a[1].at - b[1].at)) {
        if (timer.at <= now && timers.delete(id)) timer.fn();
      }
    },
  };
}
function httpHarness(fetch, t, extra = {}) {
  const time = clock();
  const context = vm.createContext({ window: {}, AbortController, AbortSignal, DOMException,
    URL, URLSearchParams, Headers, fetch,
    setTimeout: time.setTimeout, clearTimeout: time.clearTimeout,
    Date: { now: time.now, parse: Date.parse }, ...extra,
  });
  vm.runInContext(source('http.js'), context);
  t.after(() => time.advance(60000));
  return { api: context.window.EditorCore.Http, time };
}
const abortable = (signal, onData) => new Promise((resolve, reject) => {
  const abort = () => reject(new DOMException('aborted', 'AbortError'));
  if (signal.aborted) abort();
  else signal.addEventListener('abort', abort, { once: true });
  onData?.(resolve);
});

test('HTTP timeout includes token preparation and does not send a late POST', async t => {
  const calls = [];
  const { api, time } = httpHarness((url, options) => {
    calls.push(String(url));
    return abortable(options.signal);
  }, t);
  const pending = api.json('api/db-crud.php?action=create', { method: 'POST', body: {}, timeout: 100 });
  await tick();
  time.advance(100);
  const result = await pending;
  assert.equal(result.ok, false);
  assert.equal(result.cancelled, false);
  assert.match(result.error, /too long/);
  assert.deepEqual(calls, ['api/auth.php?action=csrf']);
});

test('HTTP checks its deadline even if the parent timer is delayed', async t => {
  const calls = [];
  let release;
  const { api, time } = httpHarness((url, options) => {
    calls.push(String(url));
    return String(url).includes('auth.php')
      ? abortable(options.signal, resolve => { release = resolve; })
      : Promise.resolve({ ok: true, status: 200, text: async () => '{}' });
  }, t);
  const pending = api.json('api/db-crud.php?action=create', { method: 'POST', body: {}, timeout: 100 });
  await tick();
  // Simulate a blocked event loop without running either scheduled timer.
  time.setNow(101);
  release({ ok: true, status: 200, text: async () => JSON.stringify({ csrf: 'a'.repeat(64) }) });
  assert.match((await pending).error, /too long/);
  assert.equal(calls.length, 1);
});

test('HTTP reads the body under the timeout and propagates cancellation metadata', async t => {
  const { api, time } = httpHarness((url, options) => abortable(options.signal, resolve => {
    resolve({ ok: true, status: 200, text: () => abortable(options.signal) });
  }), t);
  const pending = api.json('api/db.php', { timeout: 100 });
  await tick();
  time.advance(100);
  assert.match((await pending).error, /too long/);
  const external = new AbortController();
  const cancelled = api.request('api/db.php', { signal: external.signal });
  external.abort();
  await assert.rejects(cancelled, error => error.cancelled === true && error.status === 0);
  const alreadyCancelled = new AbortController();
  alreadyCancelled.abort();
  await assert.rejects(api.request('api/db.php', { signal: alreadyCancelled.signal }), error => error.cancelled);
});

test('HTTP preserves Headers/content type and never replays a cancelled CSRF retry', async t => {
  const posts = [];
  let refreshes = 0;
  const { api } = httpHarness((url, options) => {
    if (String(url).includes('auth.php')) {
      refreshes++;
      const reply = { ok: true, status: 200, text: async () => JSON.stringify({ csrf: 'b'.repeat(64) }) };
      return refreshes === 1 ? Promise.resolve(reply) : abortable(options.signal);
    }
    posts.push(options);
    return Promise.resolve({ ok: false, status: 403,
      text: async () => JSON.stringify({ ok: false, code: 'csrf_token' }) });
  }, t, { sessionStorage: { getItem: () => 'a'.repeat(64), setItem() {}, removeItem() {} } });
  const headers = new Headers({ 'content-type': 'application/json', 'X-Trace': 'save' });
  assert.equal((await api.json('api/db-crud.php?action=update', {
    method: 'POST', body: { data: {} }, headers,
  })).ok, false);
  assert.equal(posts.length, 2, 'a stale token is retried exactly once');
  assert.equal(posts[0].headers['content-type'], 'application/json');
  assert.equal(posts[0].headers['Content-Type'], undefined);
  assert.equal(posts[0].headers['x-trace'], 'save');
  assert.equal(headers.get('x-editor-csrf'), null, 'caller Headers are not mutated');
  const external = new AbortController();
  const cancelled = api.json('api/db-crud.php?action=update', {
    method: 'POST', body: {}, signal: external.signal,
  });
  await tick();
  external.abort();
  assert.equal((await cancelled).cancelled, true);
  assert.equal(posts.length, 3, '403 must not resend an explicitly cancelled write');
});

test('CSRF refresh is shared, survives one caller cancellation, and never leaks to foreign origins', async () => {
  let release;
  let refreshes = 0;
  const posts = [];
  const listeners = {};
  const session = { getItem: () => null, setItem() {}, removeItem() {} };
  const token = 'a'.repeat(64);
  const reply = body => ({ ok: true, status: 200, text: async () => JSON.stringify(body) });
  const window = { location: { href: 'https://studio.test/editor/items-editor.html', origin: 'https://studio.test' },
    addEventListener: (name, fn) => { listeners[name] = fn; }, top: null };
  const context = vm.createContext({ window, URL, URLSearchParams, sessionStorage: session,
    AbortController, AbortSignal, DOMException, setTimeout, clearTimeout,
    fetch: async (url, options) => {
      if (String(url).includes('auth.php')) {
        refreshes++;
        return new Promise(resolve => { release = () => resolve(reply({ ok: true, csrf: token })); });
      }
      posts.push(options);
      return reply({ ok: true });
    },
  });
  vm.runInContext(source('http.js'), context);
  const api = context.window.EditorCore.Http;
  const external = new AbortController();
  const first = api.json('/editor/api/db-crud.php?action=create', { method: 'POST', body: {}, signal: external.signal });
  const second = api.json('/editor/api/db-crud.php?action=create', { method: 'POST', body: {} });
  await tick();
  assert.equal(refreshes, 1);
  external.abort();
  assert.equal((await first).cancelled, true);
  release();
  assert.equal((await second).ok, true);
  assert.equal(posts.length, 1);
  assert.equal(posts[0].headers['X-Editor-CSRF'], token);
  await api.json('https://foreign.test/api/db.php', { method: 'POST', body: {} });
  assert.equal(posts[1].headers['X-Editor-CSRF'], undefined);
  await api.json('/editor/api.php', { method: 'POST', body: {} });
  assert.equal(posts[2].headers['X-Editor-CSRF'], token, 'legacy root API is still supported');
  const rotated = 'b'.repeat(64);
  listeners.storage({ key: 'editorStudio.csrf', newValue: rotated, storageArea: session });
  assert.equal(api.csrf.get(), rotated);
});

test('toasts are bounded, accessible, dismissible and removed without transitionend', async t => {
  const d = dom(); t.after(() => d.window.close());
  const w = d.window;
  load(w, 'utils.js', 'ui.js');
  for (let i = 0; i < 7; i++) w.EditorCore.utils.showToast(`Message ${i}`, 'error', { ttl: 30 });
  const container = w.document.getElementById('toast-container');
  assert.equal(container.children.length, 5);
  assert.equal(container.lastElementChild.getAttribute('role'), 'alert');
  assert.equal(container.lastElementChild.getAttribute('aria-atomic'), 'true');
  container.lastElementChild.querySelector('button').click();
  assert.equal(container.children.length, 4);
  await new Promise(resolve => setTimeout(resolve, 60));
  assert.equal(container.children.length, 0);
});

test('nested confirmation: Escape closes only the top dialog and restores focus', async t => {
  const d = dom('<button id="source">Open</button>'); t.after(() => d.window.close());
  const w = d.window;
  load(w, 'utils.js', 'page-kit.js');
  const button = w.document.getElementById('root') || w.document.getElementById('source');
  button.focus();
  let outerFinished = false;
  const outer = w.EditorCore.Page.confirm('Outer').then(value => { outerFinished = true; return value; });
  const inner = w.EditorCore.Page.confirm('Inner');
  const key = key => w.document.dispatchEvent(new w.KeyboardEvent('keydown', { key, bubbles: true, cancelable: true }));
  key('Escape');
  assert.equal(await inner, false);
  await tick();
  assert.equal(outerFinished, false);
  assert.equal(w.document.querySelectorAll('[role="alertdialog"]').length, 1);
  key('Enter');
  assert.equal(await outer, false, 'underlying destructive action is not accidentally confirmed');
  assert.equal(w.document.activeElement, button);
});

test('bootstrap is idempotent and reports initialization failures without unhandled rejection', async () => {
  const errors = [];
  const toasts = [];
  let mounts = 0;
  let inits = 0;
  const window = { EditorCore: {
    UI: { mountScenarioEditorUI: () => { mounts++; } },
    defaults: { getFieldRenderOptions: () => ({}) },
    UIManager: { init: async () => { inits++; throw new Error('offline'); } },
    Page: { report: (...args) => errors.push(args), toast: (...args) => toasts.push(args) },
  } };
  const context = vm.createContext({ window, document: { readyState: 'complete' } });
  vm.runInContext(source('bootstrap.js'), context);
  const first = window.EditorCore.bootstrapEditor();
  const second = window.EditorCore.bootstrapEditor();
  assert.equal(first, second);
  await first.ready;
  await second.ready;
  assert.equal(mounts, 1);
  assert.equal(inits, 1);
  assert.equal(errors.length, 1);
  assert.equal(toasts[0][1], 'error');
});

function editorHarness(t, rows = [{ ID: 1, Name: 'One' }, { ID: 2, Name: 'Two' }]) {
  const d = dom(`<div id="root"><div data-editor-role="list"></div><div data-editor-role="title"></div>
    <div data-editor-role="status"></div><button data-editor-role="save"></button>
    <button data-editor-role="new"></button><button data-editor-role="delete"></button>
    <button data-editor-role="refresh"></button><div data-editor-role="form"></div></div>`);
  t.after(() => d.window.close());
  const w = d.window;
  load(w, 'utils.js');
  const calls = { update: [], remove: [], list: 0 };
  let confirm = async () => true;
  Object.assign(w.EditorCore, {
    DBCrud: {
      list: async () => { calls.list++; return { ok: true, rows }; },
      update: async (...args) => { calls.update.push(args); return { ok: true }; },
      create: async () => ({ ok: true, id: 3 }),
      remove: async (...args) => { calls.remove.push(args); return { ok: true }; },
      invalidateCache() {},
    },
    FormRuntime: { mount(host, { data, onChange }) {
      let model = data;
      const input = w.document.createElement('input');
      input.addEventListener('input', () => { model.Name = input.value; onChange(model, { path: 'Name' }); });
      host.replaceChildren(input);
      return { setData(next) { model = next; input.value = next?.Name || ''; },
        getData: () => model, validate: () => ({ ok: true }), destroy() { host.replaceChildren(); } };
    } },
    Page: { confirm: (...args) => confirm(...args) }, UI: { toast() {} },
    Http: { isSameOriginMessage: () => false },
  });
  load(w, 'db-editor-runtime.js');
  let dirty = false;
  const host = w.document.getElementById('root');
  const editor = w.EditorCore.DbEditor.mount(host, { resource: 'items', dirty: {
    isDirty: () => dirty, markDirty: () => { dirty = true; }, setClean: () => { dirty = false; },
  } });
  return { w, host, editor, calls, setConfirm: fn => { confirm = fn; }, isDirty: () => dirty };
}

test('record selection: only the newest confirmation wins and destroy cancels pending navigation', async t => {
  const h = editorHarness(t); await h.editor.ready;
  let finish;
  let prompts = 0;
  h.setConfirm(() => { prompts++; return new Promise(resolve => { finish = resolve; }); });
  h.host.querySelector('input').value = 'Unsaved';
  h.host.querySelector('input').dispatchEvent(new h.w.Event('input', { bubbles: true }));
  const older = h.editor.selectRow({ ID: 2, Name: 'Two' });
  const newer = h.editor.selectRow({ ID: 3, Name: 'Three' });
  await tick();
  assert.equal(prompts, 1, 'overlapping navigation shares one confirmation');
  finish(true);
  assert.equal(await older, false);
  assert.equal(await newer, true);
  assert.equal(h.editor.getModel().ID, 3);
  assert.equal(h.isDirty(), false);
  h.editor.destroy();
  assert.equal(await h.editor.selectRow({ ID: 1 }), false);
});

test('save keeps a stable payload/ID, locks edits and reports failed operations as failure', async t => {
  const h = editorHarness(t); await h.editor.ready;
  let finish;
  h.w.EditorCore.DBCrud.update = (...args) => {
    h.calls.update.push(args); return new Promise(resolve => { finish = resolve; });
  };
  const input = h.host.querySelector('input'); input.value = 'Saved name';
  input.dispatchEvent(new h.w.Event('input', { bubbles: true }));
  // Invoke through the actual click path, then inspect inert/busy state.
  h.host.querySelector('[data-editor-role="save"]').click();
  await tick();
  assert.equal(h.host.querySelector('[data-editor-role="form"]').inert, true);
  assert.equal(h.host.querySelector('[data-editor-role="new"]').disabled, true);
  assert.equal(await h.editor.selectRow({ ID: 2, Name: 'Two' }), false);
  finish({ ok: true }); await tick(); await tick();
  assert.equal(h.calls.update[0][1], 1);
  assert.equal(h.calls.update[0][2].Name, 'Saved name');
  assert.equal(h.editor.getModel().ID, 1);
  assert.equal(h.host.querySelector('[data-editor-role="form"]').inert, false);
  h.w.EditorCore.DBCrud.update = async () => { throw new Error('denied'); };
  h.host.querySelector('[data-editor-role="save"]').click(); await tick();
  assert.equal(h.host.querySelector('[data-editor-role="status"]').textContent, 'denied');
  assert.equal(h.host.querySelector('[data-editor-role="save"]').disabled, false);
});

test('save rejects invalid fields and unconfirmed create IDs without enabling a duplicate write', async t => {
  const h = editorHarness(t); await h.editor.ready;
  h.w.EditorCore.FormRuntime.mount = () => ({
    setData() {}, getData: () => ({}), validate: () => ({ ok: false, errors: [{ message: 'Invalid count' }] }), destroy() {},
  });
  await h.editor.selectRow(null, { confirmDirty: false });
  await h.editor.selectRow({ ID: 2, Name: 'Two' }, { confirmDirty: false });
  h.host.querySelector('[data-editor-role="save"]').click();
  await tick();
  assert.equal(h.calls.update.length, 0);
  assert.equal(h.host.querySelector('[data-editor-role="status"]').textContent, 'Invalid count');
  assert.equal(h.host.querySelector('[data-editor-role="form"]').inert, false);
  await h.editor.selectRow(null, { confirmDirty: false });
  h.w.EditorCore.FormRuntime.mount = () => ({
    setData() {}, getData: () => ({}), validate: () => ({ ok: true }), destroy() {},
  });
  h.host.querySelector('[data-editor-role="new"]').click(); await tick();
  for (const id of [undefined, null, '', -1, 1.5, 0]) {
    h.w.EditorCore.DBCrud.create = async () => ({ ok: true, id });
    h.host.querySelector('[data-editor-role="save"]').click(); await tick();
    assert.equal(h.editor.getModel().ID, undefined);
    assert.match(h.host.querySelector('[data-editor-role="status"]').textContent, /did not confirm/);
    assert.equal(h.host.querySelector('[data-editor-role="save"]').disabled, false);
  }
});

test('a superseded list response cannot replace the current search results', async t => {
  const h = editorHarness(t); await h.editor.ready;
  await h.editor.selectRow(null, { confirmDirty: false });
  const replies = [];
  h.w.EditorCore.DBCrud.list = () => new Promise(resolve => replies.push(resolve));
  const older = h.editor.refresh({ forceServer: true });
  const newer = h.editor.refresh({ forceServer: true });
  replies[1]({ ok: true, rows: [{ ID: 2, Name: 'Current' }] });
  await newer;
  replies[0]({ ok: true, rows: [{ ID: 1, Name: 'Stale' }] });
  await older;
  assert.equal(h.editor.getModel().ID, 2);
  assert.match(h.host.querySelector('[data-editor-role="list"]').textContent, /Current/);
  assert.doesNotMatch(h.host.querySelector('[data-editor-role="list"]').textContent, /Stale/);
});

test('delete clears the deleted record even during the operation; malformed list response is safe', async t => {
  const h = editorHarness(t); await h.editor.ready;
  h.host.querySelector('[data-editor-role="delete"]').click();
  await tick(); await tick();
  assert.deepEqual(h.calls.remove, [['items', 1]]);
  assert.equal(h.editor.getModel(), null);
  assert.equal(h.host.querySelector('[data-editor-role="save"]').disabled, true);
  h.w.EditorCore.DBCrud.list = async () => ({ ok: true, rows: null });
  await assert.doesNotReject(h.editor.refresh());
  h.w.EditorCore.DbEditor.mount(h.host, { resource: 'items' });
  h.editor.destroy();
});
