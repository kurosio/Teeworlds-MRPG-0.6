const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');

const DRAFT_KEY = 'editorStudio.guestDraft.v2:a1';
const SUBMIT_KEY = 'editorStudio.guestSubmitKey:a1';
const ACCOUNT = { id: 'a1', login: 'guest', name: 'Guest', pages: ['items', 'crafts'] };
const CONFIG = { resources: { items: { title: 'Items' }, crafts: { title: 'Crafts' } } };

const seedDraft = (ops = [], nextTemp = 2000000001) => JSON.stringify({
  v: 2, accountId: 'a1', nextTemp, ops,
});
const op = (over = {}) => ({
  uid: 'uid-1', type: 'update', resource: 'items', id: 7, data: { Name: 'Shield' },
  label: 'item 7', ts: 1700000000000, reqId: null, reqVersion: 1, reqDirty: false,
  submittedAt: null, ...over,
});

const memoryStorage = (initial = new Map()) => ({
  map: initial,
  getItem(key) { return this.map.has(key) ? this.map.get(key) : null; },
  setItem(key, value) { this.map.set(key, String(value)); },
  removeItem(key) { this.map.delete(key); },
});

/** One studio document with a signed-in guest (same pattern as guest-draft-sync). */
function makeDocument({ storage, json } = {}) {
  const listeners = new Map();
  const get = (type) => listeners.get(type) || [];
  const configJson = async (url) => (String(url).includes('action=config')
    ? { ok: true, data: CONFIG }
    : { ok: false });
  const window = {
    location: { pathname: '/index.html', search: '', origin: 'http://localhost' },
    parent: null,
    addEventListener: (type, fn) => listeners.set(type, [...get(type), fn]),
    removeEventListener: () => {},
    dispatchEvent: (event) => get(event?.type).forEach((fn) => fn(event)),
    setTimeout,
    clearTimeout,
    EditorCore: {
      Http: {
        postMessage: () => true,
        json: json || configJson,
      },
    },
  };
  window.parent = window;
  const session = {
    getItem: (key) => (key === 'editorStudio.role' ? 'guest'
      : key === 'editorStudio.guestAccount' ? JSON.stringify(ACCOUNT) : null),
    setItem: () => {},
    removeItem: () => {},
  };
  const context = vm.createContext({
    window,
    location: window.location,
    document: { addEventListener: () => {}, removeEventListener: () => {}, body: null, documentElement: { classList: { add: () => {} } } },
    localStorage: storage,
    sessionStorage: session,
    console,
    setTimeout,
    clearTimeout,
    crypto: require('node:crypto'),
    CustomEvent: class CustomEvent {
      constructor(type, init = {}) { this.type = type; this.detail = init.detail; }
    },
  });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('guest-mode.js'), context);
  return { Guest: context.window.EditorCore.Guest };
}

test('export contains only unsent operations and round-trips into a fresh draft', async () => {
  const storageA = memoryStorage(new Map([[DRAFT_KEY, seedDraft([
    op({ uid: 'u1' }),
    op({ uid: 'u2', type: 'create', id: undefined, tempId: 2000000001, data: { Name: 'Sword' } }),
    op({ uid: 'u3', type: 'delete', id: 9, reqId: 'CR-1' }),
  ], 2000000002)]]));
  const a = makeDocument({ storage: storageA });

  const text = a.Guest.Draft.exportDraft();
  const parsed = JSON.parse(text);
  assert.equal(parsed.app, 'editor-studio-guest-draft');
  assert.equal(parsed.version, 1);
  assert.equal(parsed.ops.length, 2);
  assert.ok(parsed.ops.every((o) => o.type !== 'delete'));

  const storageB = memoryStorage(new Map([[DRAFT_KEY, seedDraft([])]]));
  const b = makeDocument({ storage: storageB });
  const result = await b.Guest.Draft.importDraft(text);
  assert.equal(result.imported, 2);
  assert.equal(result.skipped, 0);

  const draft = b.Guest.Draft.get();
  assert.equal(draft.ops.length, 2);
  const created = draft.ops.find((o) => o.type === 'create');
  assert.equal(created.tempId, 2000000001);
  assert.equal(created.reqId, null);
  assert.ok(draft.nextTemp > 2000000001);
});

test('import remaps colliding temp ids and rewrites references inside data', async () => {
  const storageB = memoryStorage(new Map([[DRAFT_KEY, seedDraft([
    op({ uid: 'existing', type: 'create', id: undefined, tempId: 2000000001, data: { Name: 'Existing' } }),
  ], 2000000002)]]));
  const b = makeDocument({ storage: storageB });

  const file = JSON.stringify({
    app: 'editor-studio-guest-draft', version: 1,
    ops: [
      { type: 'create', resource: 'items', tempId: 2000000001, data: { Name: 'Imported sword' } },
      { type: 'create', resource: 'crafts', tempId: 2000000002, data: { Name: 'Recipe', RequiredItems: '[2000000001/2],[5/1]', ItemID: 2000000001 } },
    ],
  });
  const result = await b.Guest.Draft.importDraft(file);
  assert.equal(result.imported, 2);
  assert.equal(result.skipped, 0);

  const draft = b.Guest.Draft.get();
  const existing = draft.ops.find((o) => o.uid === 'existing');
  assert.equal(existing.tempId, 2000000001);
  const sword = draft.ops.find((o) => o.data.Name === 'Imported sword');
  const recipe = draft.ops.find((o) => o.data.Name === 'Recipe');
  assert.notEqual(sword.tempId, 2000000001);
  assert.equal(recipe.data.ItemID, sword.tempId);
  assert.equal(recipe.data.RequiredItems, `[${sword.tempId}/2],[5/1]`);
  assert.notEqual(recipe.tempId, sword.tempId);
});

test('import skips busy rows and resources outside the account access', async () => {
  const storage = memoryStorage(new Map([[DRAFT_KEY, seedDraft([op({ uid: 'mine' })])]]));
  const doc = makeDocument({ storage });

  const file = JSON.stringify({
    app: 'editor-studio-guest-draft', version: 1,
    ops: [
      { type: 'update', resource: 'items', id: 7, data: { Name: 'Overwrite attempt' } },
      { type: 'update', resource: 'worlds', id: 3, data: { Name: 'No access' } },
      { type: 'update', resource: 'items', id: 8, data: { Name: 'Fine' } },
    ],
  });
  const result = await doc.Guest.Draft.importDraft(file);
  assert.equal(result.imported, 1);
  assert.equal(result.skipped, 2);

  const draft = doc.Guest.Draft.get();
  assert.equal(draft.ops.find((o) => o.id === 7).data.Name, 'Shield');
  assert.equal(draft.ops.find((o) => o.id === 8).data.Name, 'Fine');
});

test('import rejects foreign files', async () => {
  const doc = makeDocument({ storage: memoryStorage() });
  await assert.rejects(doc.Guest.Draft.importDraft('not json'), /not valid JSON/);
  await assert.rejects(doc.Guest.Draft.importDraft('{"app":"other","ops":[]}'),
    /not an Editor Studio guest draft/);
});

test('submit key is stable for the same draft, changes with it, cleared on submit', () => {
  const storage = memoryStorage(new Map([[DRAFT_KEY, seedDraft([op({})])]]));
  const doc = makeDocument({ storage });

  const first = doc.Guest.Draft.submitKey();
  assert.match(first, /^[A-Za-z0-9-]{16,80}$/);
  assert.equal(doc.Guest.Draft.submitKey(), first);

  // The same key is seen from another document sharing the browser storage.
  const other = makeDocument({ storage });
  assert.equal(other.Guest.Draft.submitKey(), first);

  // Any edit of the pending set produces a new key.
  storage.setItem(DRAFT_KEY, seedDraft([
    op({}), op({ uid: 'u2', id: 8 }),
  ]));
  doc.Guest.Draft.invalidate();
  const second = doc.Guest.Draft.submitKey();
  assert.notEqual(second, first);

  // After submitting, the key is gone: the next draft gets a fresh one.
  doc.Guest.Draft.markSubmitted('CR-9');
  assert.equal(storage.getItem(SUBMIT_KEY), null);
  assert.notEqual(doc.Guest.Draft.submitKey(), second);
});
