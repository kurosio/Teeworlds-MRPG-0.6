const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');

// ── Значения: NULL, '' и 0 означают одно и то же «нет значения» ──────────────
// Формы редакторов не различают NULL и пустое поле, поэтому обычное сохранение
// записи не должно превращаться в запрос «NULL → пусто» / «NULL → 0».

function utilsHarness() {
  const window = { addEventListener() {}, dispatchEvent() {}, setTimeout, clearTimeout };
  const context = vm.createContext({ window, console, setTimeout, clearTimeout, crypto: require('node:crypto') });
  vm.runInContext(source('utils.js'), context);
  return context.window.EditorCore.utils;
}

test('an empty database value equals the empty form value', () => {
  const { valuesEqual, isEmptyValue } = utilsHarness();

  // NULL из БД против того, что форма присылает на его месте.
  assert.equal(valuesEqual(null, ''), true);
  assert.equal(valuesEqual(null, 0), true);
  assert.equal(valuesEqual('', '0'), true);
  assert.equal(valuesEqual(null, undefined), true);
  assert.equal(valuesEqual(false, 0), true);
  assert.equal(valuesEqual(null, []), true);
  assert.equal(valuesEqual('[]', null), true);

  // Реальные значения по-прежнему различаются.
  assert.equal(valuesEqual(7, 0), false);
  assert.equal(valuesEqual('Material', ''), false);
  assert.equal(valuesEqual(null, 'Material'), false);
  assert.equal(valuesEqual(5, 7), false);
  assert.equal(valuesEqual('Text', 'text'), false);
  assert.equal(valuesEqual('{"a":1}', '{"a":2}'), false);
  assert.equal(valuesEqual('{"a":1}', '{"a": 1}'), true);
  assert.equal(valuesEqual(7, '7.00'), true);

  assert.equal(isEmptyValue('0.00'), true);
  assert.equal(isEmptyValue('10'), false);
});

// ── Гостевой черновик ───────────────────────────────────────────────────────

const DRAFT_KEY = 'editorStudio.guestDraft.v2:a1';
const ITEM_ROW = {
  ID: 7,
  Comment: null,
  Name: 'Material',
  Description: 'Required to improve weapons',
  Group: 'Currency',
  Type: 'Default',
  Flags: null,
  ScenarioData: null,
  ScenarioMode: 'universal',
  InitialPrice: 7,
  RequiresProducts: null,
  AT1: null,
  AT2: null,
  ATValue1: null,
  ATValue2: null,
  Data: null,
};

/** То, что items-editor.html отдаёт в modelToPayload: NULL уже стал '' и 0. */
const ITEM_PAYLOAD = {
  Comment: '',
  Name: 'Material',
  Description: 'Required to improve weapons',
  Group: 'Currency',
  Type: 'Default',
  Flags: null,
  ScenarioMode: 'universal',
  ScenarioData: null,
  InitialPrice: 7,
  RequiresProducts: 0,
  AT1: null,
  ATValue1: null,
  AT2: null,
  ATValue2: null,
  Data: null,
};

function guestHarness(seed = null) {
  const local = new Map();
  if (seed) local.set(DRAFT_KEY, JSON.stringify(seed));
  const localStorage = {
    getItem: (key) => local.get(key) ?? null,
    setItem: (key, value) => local.set(key, String(value)),
    removeItem: (key) => local.delete(key),
  };
  const session = new Map([
    ['editorStudio.role', 'guest'],
    ['editorStudio.guestAccount', JSON.stringify({ id: 'a1', login: 'guest', name: 'Guest', pages: ['items'] })],
  ]);
  const sessionStorage = {
    getItem: (key) => session.get(key) ?? null,
    setItem: (key, value) => session.set(key, String(value)),
    removeItem: (key) => session.delete(key),
  };
  const location = { pathname: '/index.html', search: '' };
  const window = {
    location, parent: null, addEventListener() {}, dispatchEvent() {}, setTimeout, clearTimeout,
    EditorCore: {
      Http: {
        postMessage: () => true,
        // auth.php?action=status — the role of the tab; everything else is not
        // needed to stage a draft operation.
        json: async (url) => (String(url).includes('action=status')
          ? { ok: true, data: { authorized: true, role: 'guest', guest: { id: 'a1', login: 'guest', name: 'Guest', pages: ['items'] } } }
          : { ok: false }),
      },
    },
  };
  window.parent = window;
  const context = vm.createContext({
    window, location, document: {}, localStorage, sessionStorage, console, setTimeout, clearTimeout,
    crypto: require('node:crypto'),
    CustomEvent: class CustomEvent { constructor(type, init = {}) { this.type = type; this.detail = init.detail; } },
  });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('guest-mode.js'), context);
  return { Guest: context.window.EditorCore.Guest, local };
}

test('saving a record without edits creates no change request operation', async () => {
  const { Guest, local } = guestHarness();
  const result = await Guest.Draft.update('items', ITEM_ROW.ID, { ...ITEM_PAYLOAD }, ITEM_ROW);

  assert.equal(result.noop, true);
  assert.equal(Guest.Draft.count(), 0);
  assert.equal(JSON.parse(local.get(DRAFT_KEY) ?? '{"ops":[]}').ops.length, 0);
});

test('only the field the guest really changed gets into the draft', async () => {
  const { Guest } = guestHarness();
  await Guest.Draft.update('items', ITEM_ROW.ID, { ...ITEM_PAYLOAD, Name: 'Material (refined)' }, ITEM_ROW);

  const [op] = Guest.Draft.get().ops;
  assert.equal(op.type, 'update');
  assert.deepEqual(Array.from(Object.keys(op.data)), ['Name']);
  assert.equal(op.data.Name, 'Material (refined)');
});

test('returning a field to its original value removes the operation from the draft', async () => {
  const { Guest } = guestHarness();
  await Guest.Draft.update('items', ITEM_ROW.ID, { ...ITEM_PAYLOAD, Comment: 'Improve weapons' }, ITEM_ROW);
  assert.equal(Guest.Draft.count(), 1);
  assert.deepEqual(Array.from(Object.keys(Guest.Draft.get().ops[0].data)), ['Comment']);

  // The guest clears the comment again — the base row has NULL, the form sends ''.
  const result = await Guest.Draft.update('items', ITEM_ROW.ID, { ...ITEM_PAYLOAD, Comment: '' }, ITEM_ROW);
  assert.equal(result.noop, true);
  assert.equal(Guest.Draft.count(), 0);
});

test('a field cleared in the database still counts as a real change', async () => {
  const { Guest } = guestHarness();
  // Base value 7 → the guest sets 0: that is a real edit, not an empty one.
  await Guest.Draft.update('items', ITEM_ROW.ID, { ...ITEM_PAYLOAD, InitialPrice: 0 }, ITEM_ROW);
  const [op] = Guest.Draft.get().ops;
  assert.deepEqual(Array.from(Object.keys(op.data)), ['InitialPrice']);
  assert.equal(op.data.InitialPrice, 0);
});

test('explicitly clearing a filled text field stays in the draft', async () => {
  const { Guest } = guestHarness();
  const base = { ...ITEM_ROW, Comment: 'Old comment' };
  await Guest.Draft.update('items', ITEM_ROW.ID, { ...ITEM_PAYLOAD, Comment: '' }, base);
  const [op] = Guest.Draft.get().ops;
  assert.deepEqual(Array.from(Object.keys(op.data)), ['Comment']);
  assert.equal(op.data.Comment, '');
});

// ── Правка запроса администратором (RequestContext) ─────────────────────────

test('saving the form unchanged sends no revision at all', async () => {
  const request = {
    id: 'CR-2', status: 'pending', version: 1,
    operations: [{ type: 'update', resource: 'items', id: ITEM_ROW.ID, tempId: null, data: { Name: 'Requested' } }],
  };
  let revisions = 0;
  const document = {
    readyState: 'loading', body: null,
    addEventListener() {}, querySelectorAll() { return []; },
  };
  const window = {
    location: { pathname: '/items-editor.html', search: `?request=CR-2&resource=items&record=${ITEM_ROW.ID}&edit=1` },
    parent: null, addEventListener() {}, dispatchEvent() {}, setTimeout, clearTimeout,
    EditorCore: {
      Guest: { ready: async () => 'admin' },
      ChangeMarkers: { operationId: (op) => Number(op.type === 'create' ? op.tempId : op.id) || 0, setRequest() {}, refresh() {} },
      Http: {
        json: async (url) => {
          if (url.includes('action=revise')) revisions += 1;
          return { ok: true, request };
        },
        request: async (url) => {
          if (url.includes('action=get')) return { ok: true, row: ITEM_ROW };
          return { ok: true, rows: [] };
        },
        postMessage: () => true,
      },
    },
  };
  window.parent = window;
  const context = vm.createContext({
    window, document, location: window.location, console, URLSearchParams, URL,
    CustomEvent: class CustomEvent { constructor(type, options = {}) { this.type = type; this.detail = options.detail; } },
    MutationObserver: class { observe() {} }, setTimeout, clearTimeout,
  });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('request-context.js'), context);
  vm.runInContext(source('db-crud.js'), context);

  const { RequestContext, DBCrud } = context.window.EditorCore;
  await RequestContext.ready();
  // Only the NULL → '' / 0 normalisation differs from the stored request.
  await DBCrud.update('items', ITEM_ROW.ID, { ...ITEM_PAYLOAD, Name: 'Requested' });
  assert.equal(revisions, 0);
  assert.deepEqual(Array.from(Object.keys(RequestContext.operation('items', ITEM_ROW.ID).data)), ['Name']);
});

test('an admin revision does not turn NULL columns into empty values', async () => {
  const request = {
    id: 'CR-1', status: 'pending', version: 1,
    operations: [{ type: 'update', resource: 'items', id: ITEM_ROW.ID, tempId: null, data: { Name: 'Requested' } }],
  };
  let revised = null;
  const document = {
    readyState: 'loading', body: null,
    addEventListener() {}, querySelectorAll() { return []; },
  };
  const window = {
    location: { pathname: '/items-editor.html', search: `?request=CR-1&resource=items&record=${ITEM_ROW.ID}&edit=1` },
    parent: null, addEventListener() {}, dispatchEvent() {}, setTimeout, clearTimeout,
    EditorCore: {
      Guest: { ready: async () => 'admin' },
      ChangeMarkers: { operationId: (op) => Number(op.type === 'create' ? op.tempId : op.id) || 0, setRequest() {}, refresh() {} },
      Http: {
        json: async (url, options = {}) => {
          if (url.includes('action=revise')) {
            revised = { ...request, version: 2, operations: options.body.operations };
            return { ok: true, request: revised };
          }
          return { ok: true, request };
        },
        request: async (url) => {
          if (url.includes('action=list')) return { ok: true, rows: [] };
          if (url.includes('action=get')) return { ok: true, row: ITEM_ROW };
          return { ok: true };
        },
        postMessage: () => true,
        isSameOriginMessage: () => false,
      },
    },
  };
  window.parent = window;
  const context = vm.createContext({
    window, document, location: window.location, console, URLSearchParams, URL,
    CustomEvent: class CustomEvent { constructor(type, options = {}) { this.type = type; this.detail = options.detail; } },
    MutationObserver: class { observe() {} }, setTimeout, clearTimeout,
  });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('request-context.js'), context);
  vm.runInContext(source('db-crud.js'), context);

  const { RequestContext, DBCrud } = context.window.EditorCore;
  await RequestContext.ready();
  // The editor saves its whole form: NULL columns arrive as '' and 0 while the
  // guest really edits the name.
  await DBCrud.update('items', ITEM_ROW.ID, { ...ITEM_PAYLOAD, Name: 'Requested v2' });

  assert.ok(revised, 'the revision was sent');
  const [operation] = revised.operations;
  assert.equal(operation.data.Name, 'Requested v2');
  assert.equal('Comment' in operation.data, false);
  assert.equal('RequiresProducts' in operation.data, false);
});
