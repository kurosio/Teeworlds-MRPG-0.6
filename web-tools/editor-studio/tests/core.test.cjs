// Run with: node --test web-tools/editor-studio/tests/*.test.cjs
const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const source = name => fs.readFileSync(path.join(__dirname, '../editor-core', name), 'utf8');
function http(fetch, overrides = {}) {
  const context = vm.createContext({ window: {}, fetch, AbortController, AbortSignal,
    URLSearchParams, setTimeout, clearTimeout, ...overrides });
  vm.runInContext(source('http.js'), context);
  return context.window.EditorCore.Http;
}
const reply = (body, status = 200) => ({ ok: status < 400, status, text: async () => body });
test('both API failure conventions are failures, not successful saves', async () => {
  for (const flag of ['ok', 'success']) {
    const result = await http(async () => reply(JSON.stringify({ [flag]: false, error: 'denied' }))).json('/api');
    assert.equal(result.ok, false);
    assert.equal(result.error, 'denied');
  }
});
test('HTTP status cannot be overwritten by payload fields', async () => {
  const result = await http(async () => reply('{"ok":true,"status":200}', 403)).json('/api');
  assert.equal(result.ok, false);
  assert.equal(result.status, 403);
});
test('invalid response and request serialization errors do not escape json()', async () => {
  const api = http(async () => reply('<html>error</html>'));
  assert.equal((await api.json('/api')).ok, false);
  const circular = {}; circular.self = circular;
  assert.equal((await api.json('/api', { body: circular })).ok, false);
  assert.equal((await api.text('/api', { body: circular })).ok, false);
});
test('fallback abort listeners are removed after success and failure', async () => {
  for (const fails of [false, true]) {
    const external = new AbortController();
    let count = 0;
    const add = external.signal.addEventListener.bind(external.signal);
    const remove = external.signal.removeEventListener.bind(external.signal);
    external.signal.addEventListener = (...args) => { count++; add(...args); };
    external.signal.removeEventListener = (...args) => { count--; remove(...args); };
    const api = http(async () => { if (fails) throw new Error('offline'); return reply('{}'); }, { AbortSignal: {} });
    await api.json('/api', { signal: external.signal });
    assert.equal(count, 0);
    await api.text('/api', { signal: external.signal });
    assert.equal(count, 0);
  }
});
test('missing clipboard is not reported as successful copying', async () => {
  const context = vm.createContext({ window: {}, navigator: {}, console });
  vm.runInContext(source('page-kit.js'), context);
  assert.equal(await context.window.EditorCore.Page.copyText('hello'), false);
});
test('Enter on Cancel never confirms a destructive dialog; Enter on OK does', async () => {
  for (const confirm of [false, true]) {
    const listeners = {};
    const elements = [];
    const document = {
      activeElement: null,
      getElementById: () => null,
      addEventListener: (name, fn) => { listeners[name] = fn; },
      removeEventListener: name => { delete listeners[name]; },
      createElement: tag => {
        const element = { tag, children: [], setAttribute() {},
          appendChild(child) { this.children.push(child); },
          append(...children) { this.children.push(...children); },
          addEventListener() {}, remove() {}, focus() { document.activeElement = this; } };
        elements.push(element);
        return element;
      },
    };
    document.head = document.createElement('head');
    document.body = document.createElement('body');
    const context = vm.createContext({ window: {}, document, console });
    vm.runInContext(source('page-kit.js'), context);
    const result = context.window.EditorCore.Page.confirm('Delete?', { danger: true });
    if (confirm) elements.find(el => el.className === 'ec-ok is-danger').focus();
    listeners.keydown({ key: 'Enter', preventDefault() {} });
    assert.equal(await result, confirm);
    assert.equal(listeners.keydown, undefined);
  }
});

test('UIManager waits for DOMContentLoaded before accessing body and fields', async () => {
  const listeners = {};
  let queries = 0;
  const document = {
    readyState: 'loading', body: null,
    addEventListener(name, fn) { listeners[name] = fn; },
    querySelectorAll() { queries++; return []; },
    querySelector() { return null; },
  };
  const context = vm.createContext({ window: { EditorCore: {} }, document,
    localStorage: { getItem: () => null, setItem() {} } });
  vm.runInContext(source('ui-manager.js'), context);
  const initialized = context.window.EditorCore.UIManager.init(document);
  assert.equal(queries, 0);
  assert.equal(typeof listeners.DOMContentLoaded, 'function');
  const modes = [];
  document.body = { dataset: {}, classList: { toggle: (...args) => modes.push(args) } };
  document.readyState = 'interactive';
  listeners.DOMContentLoaded();
  await initialized;
  assert.ok(queries > 0);
  assert.deepEqual(modes, [['editor-compact', false], ['editor-focus', false]]);
  await context.window.EditorCore.UIManager.init(document);
  assert.equal(modes.length, 2, 'productivity controls remain idempotent');
});

test('UIManager tolerates a missing body while an editor document is being replaced', async () => {
  const document = {
    readyState: 'interactive', body: null,
    addEventListener() {},
    querySelectorAll() { return []; },
    querySelector() { return null; },
  };
  const context = vm.createContext({ window: { EditorCore: {} }, document,
    localStorage: { getItem: () => null, setItem() {} } });
  vm.runInContext(source('ui-manager.js'), context);

  await assert.doesNotReject(context.window.EditorCore.UIManager.init(document));

  const modes = [];
  document.body = { dataset: {}, classList: { toggle: (...args) => modes.push(args) } };
  await context.window.EditorCore.UIManager.init(document);
  assert.deepEqual(modes, [['editor-compact', false], ['editor-focus', false]]);
});

test('read-only sync_map is not guarded as a JSON POST write', () => {
  const php = fs.readFileSync(path.join(__dirname, '../api/db-sync.php'), 'utf8');
  const guarded = php.match(/guard_write_actions\(\$action, \[([^\]]+)\]\)/)[1];
  assert.ok(!guarded.includes("'sync_map'"));
  for (const action of ['save_target', 'set_as_target', 'apply_operations', 'pull_from_target']) {
    assert.ok(guarded.includes(`'${action}'`));
  }
});

test('compiled utilities follow component styles in both page entry points', () => {
  for (const text of [source('page-loader.js'), fs.readFileSync(path.join(__dirname, '../index.html'), 'utf8')]) {
    const utilities = text.indexOf('href="editor-core/tailwind.css"');
    assert.ok(utilities > text.indexOf('href="editor-core/editor-theme.css"'));
    assert.ok(utilities > text.indexOf('href="editor-core/editor-template.css"'));
  }
  const css = source('editor-theme.css');
  assert.match(css, /\.editor-modal-backdrop\.hidden,\s*#editor-sync-modal\.hidden\s*\{\s*display:\s*none\s*!important;/);
  assert.match(source('tailwind.css'), /\.hidden\{display:none\}/);
});
