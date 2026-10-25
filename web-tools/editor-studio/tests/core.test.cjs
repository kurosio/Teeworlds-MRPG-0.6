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
test('shell messaging is pinned to the expected origin and source', () => {
  const window = { location: { origin: 'https://editor.test' } };
  const api = http(async () => reply('{}'), { window });
  const target = { messages: [], postMessage(data, origin) { this.messages.push({ data, origin }); } };
  const message = { type: 'editor-shell:test' };
  assert.equal(api.postMessage(target, message), true);
  assert.deepEqual(target.messages, [{ data: message, origin: 'https://editor.test' }]);
  assert.equal(api.isSameOriginMessage({ origin: 'https://editor.test', source: target }, target), true);
  assert.equal(api.isSameOriginMessage({ origin: 'https://attacker.test', source: target }, target), false);
  assert.equal(api.isSameOriginMessage({ origin: 'https://editor.test', source: {} }, target), false);
  window.messages = [];
  window.postMessage = (data, origin) => window.messages.push({ data, origin });
  assert.equal(api.postMessage(window, { type: 'editor-shell:auth-required' }), true);
  assert.equal(window.messages[0].origin, 'https://editor.test');

  const opaque = http(async () => reply('{}'), { window: { location: { origin: 'null' } } });
  assert.equal(opaque.postMessage(target, message), false);
  assert.equal(opaque.isSameOriginMessage({ origin: 'null', source: target }, target), false);
  assert.equal(target.messages.length, 1);
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

test('shared form paths repair containers and block prototype pollution', () => {
  const context = vm.createContext({ window: {}, console });
  vm.runInContext(source('utils.js'), context);
  const utils = context.window.EditorCore.utils;
  const model = { profile: 'old' };
  utils.setAtPath(model, 'profile.name', 'Tee');
  utils.setAtPath(model, 'items.0.id', 42);
  assert.equal(model.profile.name, 'Tee');
  assert.ok(Array.isArray(model.items));
  assert.equal(utils.getAtPath(model, 'items.0.id'), 42);
  utils.setAtPath(model, '__proto__.polluted', true);
  utils.setAtPath(model, 'constructor.prototype.polluted', true);
  assert.equal({}.polluted, undefined);
});

test('request cache de-duplicates work, bounds memory and rejects stale writes', async () => {
  const context = vm.createContext({ window: {}, console });
  vm.runInContext(source('utils.js'), context);
  const utils = context.window.EditorCore.utils;
  let calls = 0;
  const cache = utils.createRequestCache({ ttl: 1000, max: 2 });
  const loader = async () => ({ ok: true, value: ++calls });
  const [first, second] = await Promise.all([cache.load('a', loader), cache.load('a', loader)]);
  assert.equal(calls, 1);
  assert.equal(first, second);
  assert.equal((await cache.load('a', loader)).value, 1);
  await cache.load('b', loader);
  await cache.load('c', loader);
  assert.equal(cache.size(), 2);

  let finishOld;
  const oldRequest = cache.load('key', () => new Promise(resolve => { finishOld = resolve; }));
  await Promise.resolve();
  cache.clear();
  await cache.load('key', async () => 'fresh');
  finishOld('stale');
  assert.equal(await oldRequest, 'stale');
  assert.equal(cache.get('key'), 'fresh');
});

test('guest draft snapshots stay isolated and failed storage writes do not mutate cache', () => {
  const draftKey = 'editorStudio.guestDraft.v2:a1';
  const seed = {
    v: 2, accountId: 'a1', nextTemp: 2000000001,
    ops: [{ uid: 'op-1', type: 'update', resource: 'mobs', id: 7,
      data: { Name: 'Original', Meta: { Rank: 1 } }, label: 'Mob', ts: 1, reqId: null }],
  };
  let failWrites = true;
  const local = new Map([[draftKey, JSON.stringify(seed)]]);
  const localStorage = {
    getItem: key => local.get(key) ?? null,
    setItem(key, value) {
      if (key === draftKey && failWrites) throw new Error('quota');
      local.set(key, String(value));
    },
    removeItem: key => local.delete(key),
  };
  const session = new Map([
    ['editorStudio.role', 'guest'],
    ['editorStudio.guestAccount', JSON.stringify({ id: 'a1', login: 'guest', pages: [] })],
  ]);
  const sessionStorage = {
    getItem: key => session.get(key) ?? null,
    setItem: (key, value) => session.set(key, String(value)),
    removeItem: key => session.delete(key),
  };
  const location = { pathname: '/index.html', search: '' };
  const window = {
    location, parent: null,
    addEventListener() {}, dispatchEvent() {},
    EditorCore: { Http: { postMessage: () => true } },
  };
  window.parent = window;
  const context = vm.createContext({ window, location, document: {}, localStorage, sessionStorage, console });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('guest-mode.js'), context);
  const draft = context.window.EditorCore.Guest.Draft;

  const snapshot = draft.get();
  snapshot.ops[0].data.Name = 'External';
  assert.equal(draft.get().ops[0].data.Name, 'Original');
  const operations = draft.toOperations();
  operations[0].data.Meta.Rank = 99;
  assert.equal(draft.get().ops[0].data.Meta.Rank, 1);
  const fields = draft.pendingFields('mobs', 7);
  fields.Meta.value.Rank = 77;
  assert.equal(draft.get().ops[0].data.Meta.Rank, 1);

  assert.throws(() => draft.removeOp('op-1'), /Could not save the draft/);
  assert.equal(draft.count(), 1);
  assert.equal(JSON.parse(local.get(draftKey)).ops.length, 1);

  failWrites = false;
  draft.removeOp('op-1');
  assert.equal(draft.count(), 0);
  assert.equal(JSON.parse(local.get(draftKey)).ops.length, 0);
});

test('DB combo portals and global listeners exist only while open', async () => {
  const element = (tag = 'div') => {
    const node = {
      tag, children: [], dataset: {}, style: {}, listeners: {}, className: '',
      value: '', disabled: false, parentNode: null, isConnected: true,
      classList: {
        add(...names) {
          const values = new Set(node.className.split(/\s+/).filter(Boolean));
          names.forEach(name => values.add(name));
          node.className = [...values].join(' ');
        },
        remove(...names) {
          const removed = new Set(names);
          node.className = node.className.split(/\s+/).filter(name => name && !removed.has(name)).join(' ');
        },
        contains(name) { return node.className.split(/\s+/).includes(name); },
      },
      setAttribute() {},
      addEventListener(type, fn) { (node.listeners[type] ||= []).push(fn); },
      removeEventListener(type, fn) {
        node.listeners[type] = (node.listeners[type] || []).filter(handler => handler !== fn);
      },
      emit(type, event = {}) {
        for (const handler of [...(node.listeners[type] || [])]) handler({ target: node, ...event });
      },
      appendChild(child) {
        if (child.isFragment) {
          for (const item of child.children) node.appendChild(item);
          child.children = [];
          return child;
        }
        child.parentNode = node;
        node.children.push(child);
        return child;
      },
      replaceChildren(...children) { node.children = []; children.forEach(child => node.appendChild(child)); },
      querySelectorAll(selector) {
        return selector === '.editor-dbcombo-option'
          ? node.children.filter(child => child.classList.contains('editor-dbcombo-option')) : [];
      },
      querySelector() { return null; },
      contains(other) { return other === node || node.children.includes(other); },
      getBoundingClientRect() { return { left: 10, right: 110, top: 20, bottom: 40, width: 100 }; },
      remove() {
        if (!node.parentNode) return;
        node.parentNode.children = node.parentNode.children.filter(child => child !== node);
        node.parentNode = null;
      },
    };
    return node;
  };
  const body = element('body');
  const document = element('document');
  Object.assign(document, {
    body, documentElement: { clientWidth: 800, clientHeight: 600 }, activeElement: null,
    createElement: tag => element(tag),
    createDocumentFragment: () => Object.assign(element('fragment'), { isFragment: true }),
  });
  const window = element('window');
  Object.assign(window, { innerWidth: 800, innerHeight: 600, EditorCore: {} });
  const context = vm.createContext({ window, document, console, AbortController, setTimeout, clearTimeout });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('db.js'), context);

  const input = element('input');
  const bound = element('input');
  bound.value = '0';
  const local = element('div');
  local.className = 'editor-dbcombo-dropdown';
  const combo = element('div');
  combo.dataset.datasource = 'mobs';
  combo.appendChild(input);
  combo.appendChild(local);
  combo.querySelector = selector => selector === '.editor-dbcombo-input' ? input
    : selector === '.editor-dbcombo-dropdown' ? local
      : selector === 'input[data-path]' ? bound : null;
  const root = {
    querySelectorAll: selector => selector === '.editor-dbcombo[data-datasource]' ? [combo] : [],
    querySelector: () => null,
  };
  const db = context.window.EditorCore.DB;
  db.list = async () => ({ ok: true, items: [{ value: 1, label: 'One' }, { value: 2, label: 'Two' }] });
  await db.init(root);
  assert.equal(body.children.length, 0);

  input.emit('focus');
  assert.equal(body.children.length, 1);
  const portal = body.children[0];
  assert.equal(portal.classList.contains('open'), true);
  await new Promise(resolve => setTimeout(resolve, 180));
  assert.equal(portal.children.length, 2);
  assert.equal((window.listeners.resize || []).length, 1);
  assert.equal((document.listeners.mousedown || []).length, 1);

  input.emit('keydown', { key: 'Escape' });
  assert.equal(body.children.length, 0);
  assert.equal((window.listeners.resize || []).length, 0);
  assert.equal((window.listeners.scroll || []).length, 0);
  assert.equal((document.listeners.mousedown || []).length, 0);

  input.emit('focus');
  assert.equal(body.children.length, 1);
  input.emit('keydown', { key: 'Escape' });
  assert.equal(body.children.length, 0);
});

test('dynamic page loader awaits every dependency and runs inline bootstrap last', async () => {
  const loaderSource = source('page-loader.js');
  const expected = [...loaderSource.matchAll(/'<script src="([^"]+)"><\/script>'/g)].map(match => match[1]);
  const executions = [];
  let bootstrapDone;
  let timeout;
  const done = new Promise((resolve, reject) => {
    bootstrapDone = () => { clearTimeout(timeout); resolve(); };
    timeout = setTimeout(() => reject(new Error('inline bootstrap was not executed')), 2000);
  });
  const parseAttributes = text => [...text.matchAll(/([\w-]+)(?:="([^"]*)")?/g)]
    .map(match => ({ name: match[1], value: match[2] || '' }));
  const head = { appendChild() {} };
  let context;
  const body = {
    appendChild(script) {
      if (!script.src) {
        executions.push('inline');
        vm.runInContext(script.textContent, context);
        return;
      }
      executions.push(`start:${script.src}`);
      setTimeout(() => {
        executions.push(`load:${script.src}`);
        script.listeners.load?.();
      }, 0);
    },
  };
  const document = {
    readyState: 'interactive', head, body,
    createElement(tag) {
      if (tag === 'template') {
        return {
          content: { firstChild: null },
          set innerHTML(html) {
            const script = html.match(/^<script([^>]*)>([\s\S]*)<\/script>$/);
            this.content.firstChild = script
              ? { attributes: parseAttributes(script[1]), textContent: script[2] }
              : { attributes: parseAttributes(html), textContent: '' };
          },
        };
      }
      if (tag === 'script') {
        return {
          src: '', textContent: '', listeners: {},
          setAttribute(name, value) { if (name === 'src') this.src = value; },
          addEventListener(name, fn) { this.listeners[name] = fn; },
        };
      }
      throw new Error(`Unexpected element: ${tag}`);
    },
  };
  const window = {
    EditorCore: {
      bootstrapEditor: () => {
        executions.push('bootstrap');
        bootstrapDone();
        return { ready: true };
      },
    },
    setTimeout: () => 0,
    addEventListener() {},
  };
  context = vm.createContext({ window, document, console });
  vm.runInContext(loaderSource, context);
  await done;

  const networkSequence = executions.filter(value => value !== 'inline' && value !== 'bootstrap');
  assert.deepEqual(networkSequence, expected.flatMap(src => [`start:${src}`, `load:${src}`]));
  assert.deepEqual(executions.slice(-2), ['inline', 'bootstrap']);
  assert.equal(context.window.EditorCoreBootstrap.ready, true);
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
