const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');

const draftKeyFor = (accountId) => `editorStudio.guestDraft.v2:${accountId}`;

/** Гостевой режим: роль и аккаунт отдаёт сервер, черновик лежит в localStorage. */
function guestHarness({ seed = null, accountId = 'a1', pages = ['items'] } = {}) {
  const draftKey = draftKeyFor(accountId);
  const local = new Map();
  if (seed) local.set(draftKey, JSON.stringify(seed));
  const session = new Map();
  const localStorage = {
    getItem: (key) => local.get(key) ?? null,
    setItem: (key, value) => local.set(key, String(value)),
    removeItem: (key) => local.delete(key),
  };
  const sessionStorage = {
    getItem: (key) => session.get(key) ?? null,
    setItem: (key, value) => session.set(key, String(value)),
    removeItem: (key) => session.delete(key),
  };
  const toasts = [];
  const location = { pathname: '/index.html', search: '' };
  const window = {
    location, parent: null, addEventListener() {}, dispatchEvent() {}, setTimeout, clearTimeout,
    EditorCore: {
      Http: {
        postMessage: () => true,
        json: async (url) => (String(url).includes('action=status')
          ? { ok: true, data: { authorized: true, role: 'guest', guest: { id: accountId, login: 'artist', name: 'Artist', pages } } }
          : url.includes('action=config')
            ? { ok: true, data: { resources: { items: { title: 'Предметы', label: 'Name', columns: ['Comment', 'Name', 'RequiresProducts'] } } } }
            : { ok: false }),
      },
      UI: { toast: (text) => toasts.push(text) },
    },
  };
  window.parent = window;
  const context = vm.createContext({
    window, location, localStorage, sessionStorage, console,
    // guest-mode.js вешает слушатели на document (фокус, storage) и трогает DOM.
    document: { addEventListener() {}, querySelectorAll() { return []; }, body: null },
    setTimeout, clearTimeout, CustomEvent: class { constructor(type, options = {}) { this.type = type; this.detail = options.detail; } },
    crypto: require('node:crypto'),
  });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('guest-mode.js'), context);
  return { context, Guest: context.window.EditorCore.Guest, local, draftKey, toasts };
}

const draftSeed = (ops, extra = {}) => ({
  v: 2, accountId: 'a1', nextTemp: 2000000005, ops, ...extra,
});

test('reverting one field keeps the other fields of the draft operation', async () => {
  const { Guest, local, draftKey } = guestHarness({
    seed: draftSeed([{
      uid: 'u1', type: 'update', resource: 'items', id: 7,
      data: { Name: 'Renamed', Comment: 'note' }, label: 'Old', ts: 1, reqId: null,
    }]),
  });
  assert.equal(await Guest.ready(), 'guest');

  const result = await Guest.Draft.revertField('items', 7, 'Comment', { ID: 7, Name: 'Old', Comment: null });

  assert.deepEqual({ ...result }, { ok: true, removed: true, dropped: false });
  const stored = JSON.parse(local.get(draftKey));
  assert.equal(stored.ops.length, 1);
  assert.deepEqual(Object.keys(stored.ops[0].data), ['Name']);
  assert.equal(stored.ops[0].data.Name, 'Renamed');
  // Подпись операции пересчитывается по новому имени записи (Name — колонка подписи).
  assert.equal(stored.ops[0].label, 'Renamed');
});

test('reverting the last changed field drops the operation from the draft', async () => {
  const { Guest, local, draftKey } = guestHarness({
    seed: draftSeed([{
      uid: 'u1', type: 'update', resource: 'items', id: 7,
      data: { Name: 'Renamed' }, label: 'Renamed', ts: 1, reqId: null,
    }]),
  });
  await Guest.ready();

  const result = await Guest.Draft.revertField('items', 7, 'Name');

  assert.equal(result.dropped, true);
  assert.equal(JSON.parse(local.get(draftKey)).ops.length, 0);
  assert.equal(Guest.Draft.count(), 0);
});

test('a submitted change cannot be reverted by field', async () => {
  const { Guest, local, draftKey } = guestHarness({
    seed: draftSeed([{
      uid: 'u1', type: 'update', resource: 'items', id: 7,
      data: { Name: 'Renamed' }, label: 'Renamed', ts: 1, reqId: 'CR-9',
    }]),
  });
  await Guest.ready();

  await assert.rejects(
    () => Guest.Draft.revertField('items', 7, 'Name'),
    /awaiting review/,
  );
  // Черновик не изменился: операция по-прежнему в запросе.
  assert.equal(JSON.parse(local.get(draftKey)).ops[0].data.Name, 'Renamed');
});

test('reverting an unchanged or unknown field changes nothing', async () => {
  const seed = draftSeed([{
    uid: 'u1', type: 'update', resource: 'items', id: 7,
    data: { Name: 'Renamed' }, label: 'Renamed', ts: 1, reqId: null,
  }]);
  const { Guest, local, draftKey } = guestHarness({ seed });
  await Guest.ready();

  const missingField = await Guest.Draft.revertField('items', 7, 'Comment');
  const missingRecord = await Guest.Draft.revertField('items', 99, 'Name');

  assert.equal(missingField.removed, false);
  assert.equal(missingRecord.removed, false);
  assert.deepEqual(Object.keys(JSON.parse(local.get(draftKey)).ops[0].data), ['Name']);
});

test('an empty field name is rejected instead of clearing the operation', async () => {
  const { Guest } = guestHarness({
    seed: draftSeed([{
      uid: 'u1', type: 'update', resource: 'items', id: 7,
      data: { Name: 'Renamed' }, label: 'Renamed', ts: 1, reqId: null,
    }]),
  });
  await Guest.ready();

  await assert.rejects(() => Guest.Draft.revertField('items', 7, '   '), /No field to restore/);
});

/** Мини-DOM для deep-link.js: строки списка и поле формы. */
function deepLinkHarness(search) {
  const makeNode = (attrs = {}) => {
    const classes = new Set(String(attrs.class || '').split(' ').filter(Boolean));
    const node = {
      dataset: { ...(attrs.dataset || {}) },
      attributes: { ...(attrs.attributes || {}) },
      clicked: 0,
      scrolled: 0,
      classList: {
        contains: (name) => classes.has(name),
        add: (name) => classes.add(name),
        remove: (name) => classes.delete(name),
        toggle: (name, on) => (on ? classes.add(name) : classes.delete(name)),
      },
      setAttribute(name, value) { this.attributes[name] = String(value); },
      getAttribute(name) { return this.attributes[name] ?? null; },
      scrollIntoView() { this.scrolled += 1; },
      click() { this.clicked += 1; },
    };
    return node;
  };
  const rows = [];
  const fields = [];
  const document = {
    readyState: 'complete',
    addEventListener() {},
    querySelectorAll(selector) {
      if (selector.includes('[data-field-path]')) return fields;
      return rows;
    },
  };
  const timers = [];
  const intervals = [];
  const window = {
    location: { pathname: '/items-editor.html', search },
    addEventListener() {}, dispatchEvent() {},
    clearTimeout: () => {},
    clearInterval: () => {},
    setTimeout: (fn) => { timers.push(fn); return timers.length; },
    // Опрос «строки ещё нет» — отдельный список: тест сам решает, когда тикнуть.
    setInterval: (fn) => { intervals.push(fn); return intervals.length; },
  };
  const context = vm.createContext({ window, document, console, URLSearchParams });
  vm.runInContext(source('deep-link.js'), context);
  return {
    DeepLink: context.window.EditorCore.DeepLink,
    rows, fields, makeNode, document, window,
    tick() { intervals.slice().forEach((fn) => fn()); },
    // Выполняем только уже запланированные таймеры: обработчик может
    // запланировать новые (снятие подсветки) — их тест не ждёт.
    flushTimers() { const batch = timers.splice(0, timers.length); batch.forEach((fn) => fn()); },
  };
}

const row = (harness, id, extra = {}) => {
  const node = harness.makeNode({ class: 'editor-row', dataset: { id: String(id) }, ...extra });
  harness.rows.push(node);
  return node;
};

test('deep link opens the requested record and highlights the field', () => {
  const harness = deepLinkHarness('?resource=items&record=42&field=Comment');
  assert.equal(harness.DeepLink.active, true);
  assert.equal(harness.DeepLink.record, 42);

  // До отрисовки списка строки нет — выбор не происходит.
  harness.DeepLink.revealField();
  assert.equal(harness.rows.length, 0);

  const first = row(harness, 7);
  const second = row(harness, 42);
  const target = harness.makeNode({ dataset: { fieldPath: 'Comment' } });
  harness.fields.push(target);

  // Тот же путь, что у пользователя: клик по строке.
  harness.tick();
  harness.flushTimers();
  assert.equal(second.clicked, 1);
  assert.equal(first.clicked, 0);
  assert.equal(target.classList.contains('field-link-target'), true);
});

test('deep link leaves an already selected record alone', () => {
  const harness = deepLinkHarness('?resource=items&record=42');
  const active = row(harness, 42, { class: 'editor-row editor-row-active' });
  harness.tick();
  harness.flushTimers();
  assert.equal(active.clicked, 0);
});

test('deep link ignores the URL of a request snapshot', () => {
  const harness = deepLinkHarness('?request=CR-1&resource=items&record=42&field=Comment');
  assert.equal(harness.DeepLink.active, false);
  assert.equal(harness.DeepLink.record, 0);
  assert.equal(harness.DeepLink.field, '');
});

test('deep link without record or with a broken record does nothing', () => {
  for (const search of ['', '?resource=items', '?record=abc', '?record=-5', '?record=0']) {
    const harness = deepLinkHarness(search);
    assert.equal(harness.DeepLink.active, false, search);
    const node = row(harness, 42);
    harness.tick();
    harness.flushTimers();
    assert.equal(node.clicked, 0, search);
  }
});

/** Модуль ссылок: манифест редакторов и window — минимум для editorUrl. */
function workbenchHarness() {
  const window = {
    STUDIO_MANIFEST: [{ items: [{ key: 'items', src: 'items-editor.html' }, { key: 'quests', src: 'quests-editor.html' }] }],
    EditorCore: { utils: {} },
  };
  const context = vm.createContext({ window, URLSearchParams, console, document: { addEventListener() {} } });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('request-workbench.js'), context);
  return context.window.EditorCore.RequestWorkbench;
}

test('guest record links open the editor, not the request snapshot', () => {
  const workbench = workbenchHarness();
  const attrs = workbench.recordLinkAttrs({
    op: { type: 'update', resource: 'items', id: 42 },
    requestId: 'CR-1', field: 'Comment', mode: 'guest',
  });

  // Амперсанды в атрибуте экранированы — так же, как в ссылках администратора.
  assert.match(attrs, /href="items-editor\.html\?resource=items&amp;record=42&amp;field=Comment"/);
  assert.match(attrs, /data-guest-record="1"/);
  assert.equal(attrs.includes('data-request'), false);
  assert.equal(attrs.includes('request=CR-1'), false);
});

test('guest record links cover new records and skip deleted ones', () => {
  const workbench = workbenchHarness();
  const created = workbench.recordLinkAttrs({
    op: { type: 'create', resource: 'quests', tempId: 2000000003 }, mode: 'guest',
  });
  assert.match(created, /href="quests-editor\.html\?resource=quests&amp;record=2000000003"/);

  const removed = workbench.recordLinkAttrs({ op: { type: 'delete', resource: 'items', id: 42 }, mode: 'guest' });
  assert.equal(removed, '');
});

test('admin record links keep the request snapshot and the edit flag', () => {
  const workbench = workbenchHarness();
  const attrs = workbench.recordLinkAttrs({
    op: { type: 'update', resource: 'items', id: 42 },
    requestId: 'CR-7', field: 'Name', edit: true,
  });

  assert.match(attrs, /href="items-editor\.html\?request=CR-7&amp;resource=items&amp;record=42&amp;field=Name&amp;edit=1"/);
  assert.match(attrs, /data-request="CR-7"/);
  assert.match(attrs, /data-request-edit="1"/);
  // Запрос не передан — ссылки нет (раньше здесь рисовался пустой адрес).
  assert.equal(workbench.recordLinkAttrs({ op: { type: 'update', resource: 'items', id: 42 } }), '');
  assert.equal(workbench.recordLinkAttrs({ op: { type: 'update', resource: 'unknown', id: 42 }, requestId: 'CR-7' }), '');
});

/** Execute the page's renderers in their original lexical scope, without starting API requests. */
function draftPageHarness(ops, analysis = null) {
  const elements = new Map();
  const document = {
    getElementById(id) {
      if (!elements.has(id)) elements.set(id, {
        innerHTML: '', textContent: '', value: '',
        classList: { toggle() {} }, querySelector() { return null; },
      });
      return elements.get(id);
    },
  };
  const EditorCore = {
    RequestWorkbench: workbenchHarness(),
    Guest: {
      resourceTitle: (resource) => resource,
      Draft: {
        get: () => ({ ops }),
        pendingCount: () => ops.filter(o => !o.reqId).length,
        submittedCount: () => ops.filter(o => o.reqId).length,
      },
    },
  };
  const context = vm.createContext({ window: { EditorCore }, EditorCore, document, console, analysis });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('page-kit.js'), context);
  const page = fs.readFileSync(path.join(root, 'change-requests.html'), 'utf8');
  const script = page.match(/<script>([\s\S]*?)<\/script>/)[1];
  const renderers = script.slice(0, script.indexOf('    let previewTimer = null;'));
  vm.runInContext(`${renderers}\nG.analysis = analysis; renderGuestDraft();\n})();`, context);
  return document.getElementById('g-ops').innerHTML;
}

for (const analysis of [null, { operations: [], summary: {} }]) {
  test(`submitted operations render guest links without scope errors (${analysis ? 'analysed' : 'unanalyzed'})`, () => {
    const html = draftPageHarness([
      { type: 'update', resource: 'items', id: 42, data: { Name: 'Updated' }, reqId: 'CR-1' },
      { type: 'create', resource: 'quests', tempId: 2000000003, data: { Name: 'New' }, reqId: 'CR-1' },
      { type: 'delete', resource: 'items', id: 43, reqId: 'CR-1' },
    ], analysis);
    assert.match(html, /Awaiting admin review/);
    assert.equal((html.match(/In request  CR-1/g) || []).length, 3);
    assert.match(html, /href="items-editor\.html\?resource=items&amp;record=42"/);
    assert.match(html, /href="quests-editor\.html\?resource=quests&amp;record=2000000003"/);
    assert.equal((html.match(/data-guest-record="1"/g) || []).length, 2);
    assert.doesNotMatch(html, /data-request=|data-remove-op=|record=43/);
  });
}

test('mixed guest draft keeps editable draft links and submitted operation links', () => {
  const html = draftPageHarness([
    { uid: 'u1', type: 'update', resource: 'items', id: 7, data: { Name: 'Draft' } },
    { type: 'update', resource: 'items', id: 42, reqId: 'CR-1' },
  ]);
  assert.match(html, /href="items-editor\.html\?resource=items&amp;record=7"/);
  assert.match(html, /data-remove-op="u1"/);
  assert.match(html, /href="items-editor\.html\?resource=items&amp;record=42"/);
  assert.match(html, /In request  CR-1/);
});
