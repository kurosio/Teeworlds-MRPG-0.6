const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');

const DRAFT_KEY = 'editorStudio.guestDraft.v2:a1';
const ACCOUNT = { id: 'a1', login: 'guest', name: 'Guest', pages: ['items', 'crafts'] };

const seedDraft = (ops = [], nextTemp = 2000000001) => JSON.stringify({
  v: 2, accountId: 'a1', nextTemp, ops,
});
const deleteOp = (id) => ({
  uid: `uid-${id}`, type: 'delete', resource: 'items', id, data: {}, label: `item ${id}`,
  ts: 1700000000000, reqId: null, reqVersion: 1, reqDirty: false, submittedAt: null,
});

/**
 * Один документ студии (оболочка или редактор внутри iframe). Все документы
 * одной вкладки делят localStorage, как в браузере, — поэтому хранилище
 * передаётся снаружи.
 */
function makeDocument({ storage, role = 'guest', pathname = '/index.html', json } = {}) {
  const posted = [];
  const listeners = new Map();
  const get = (type) => listeners.get(type) || [];
  const window = {
    location: { pathname, search: '', origin: 'http://localhost' },
    parent: null,
    addEventListener: (type, fn) => listeners.set(type, [...get(type), fn]),
    removeEventListener: () => {},
    dispatchEvent: (event) => get(event?.type).forEach((fn) => fn(event)),
    setTimeout,
    clearTimeout,
    EditorCore: {
      Http: {
        postMessage: (target, data) => { posted.push(data); return true; },
        json: json || (async () => ({ ok: false })),
      },
    },
  };
  window.parent = window;
  const session = {
    getItem: (key) => (key === 'editorStudio.role' ? role : key === 'editorStudio.guestAccount' ? JSON.stringify(ACCOUNT) : null),
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
  vm.runInContext(source('reference-data.js'), context);
  vm.runInContext(source('guest-mode.js'), context);
  return { window, posted, context, Guest: context.window.EditorCore.Guest, listeners: get };
}

const memoryStorage = (initial = new Map()) => ({
  map: initial,
  getItem(key) { return this.map.has(key) ? this.map.get(key) : null; },
  setItem(key, value) { this.map.set(key, String(value)); },
  removeItem(key) { this.map.delete(key); },
});

test('guest draft counts follow writes made by another studio document', async () => {
  const storage = memoryStorage(new Map([[DRAFT_KEY, seedDraft([deleteOp(5)])]]));
  // The shell renders the badge first: it caches a snapshot of the draft.
  const shell = makeDocument({ storage });
  assert.equal(shell.Guest.Draft.count(), 1);

  // An editor inside the shell frame deletes one more record.
  const editor = makeDocument({ storage, pathname: '/items-editor.html' });
  await editor.Guest.Draft.remove('items', 7);
  assert.equal(editor.Guest.Draft.count(), 2);

  // The stored draft changed → the shell must not keep serving its snapshot.
  assert.equal(shell.Guest.Draft.count(), 2);
  // Array.from builds a host-realm array: drafts live in another vm context.
  assert.deepEqual(Array.from(shell.Guest.Draft.get().ops, (op) => op.id), [5, 7]);
});

test('overlays and row metadata use the latest draft, not a cached one', () => {
  const storage = memoryStorage(new Map([[DRAFT_KEY, seedDraft([deleteOp(5)])]]));
  const shell = makeDocument({ storage });

  assert.equal(shell.Guest.Draft.overlayRows('items', [{ ID: 5 }, { ID: 6 }]).length, 1);
  assert.equal(shell.Guest.Draft.rowState('items', 5), 'delete');

  // Another document deletes record 6 as well.
  storage.setItem(DRAFT_KEY, seedDraft([deleteOp(5), deleteOp(6)]));

  assert.deepEqual(shell.Guest.Draft.overlayRows('items', [{ ID: 5 }, { ID: 6 }]), []);
  assert.equal(shell.Guest.Draft.rowState('items', 6), 'delete');
});

test('invalidate() drops the snapshot and reload() returns a fresh copy', () => {
  const storage = memoryStorage(new Map([[DRAFT_KEY, seedDraft([deleteOp(5)])]]));
  const shell = makeDocument({ storage });
  assert.equal(shell.Guest.Draft.count(), 1);

  storage.setItem(DRAFT_KEY, seedDraft([deleteOp(5), deleteOp(6), deleteOp(7)]));
  shell.Guest.Draft.invalidate();
  const snapshot = shell.Guest.Draft.reload();
  assert.equal(snapshot.ops.length, 3);

  // reload() hands out a copy: a caller cannot corrupt the cached draft.
  snapshot.ops.push({ type: 'delete', resource: 'items', id: 99 });
  assert.equal(shell.Guest.Draft.reload().ops.length, 3);
});

test('a storage event from another tab drops the snapshot and notifies subscribers', () => {
  const storage = memoryStorage(new Map([[DRAFT_KEY, seedDraft([deleteOp(5)])]]));
  const shell = makeDocument({ storage });
  const seen = [];
  shell.Guest.Draft.onChange((draft) => seen.push(draft.ops.length));
  assert.equal(shell.Guest.Draft.count(), 1);

  storage.setItem(DRAFT_KEY, seedDraft([deleteOp(5), deleteOp(6)]));
  const [onStorage] = shell.listeners('storage');
  assert.equal(typeof onStorage, 'function');
  onStorage({ key: DRAFT_KEY });

  assert.deepEqual(seen, [2]);
  assert.equal(shell.Guest.Draft.count(), 2);
});

test('the change notification carries the counts the shell needs', async () => {
  const storage = memoryStorage(new Map([[DRAFT_KEY, seedDraft([deleteOp(5)])]]));
  const doc = makeDocument({ storage, pathname: '/items-editor.html' });
  await doc.Guest.Draft.remove('items', 8);

  const message = doc.posted.filter((item) => item?.type === 'editor-shell:guest-draft-changed').pop();
  assert.ok(message, 'a draft change message is posted');
  assert.equal(message.accountId, 'a1');
  assert.equal(message.count, 2);
  assert.equal(message.pending, 2);
  assert.equal(message.submitted, 0);
});

test('drafts of another account are never mixed in', async () => {
  const storage = memoryStorage(new Map([
    [DRAFT_KEY, seedDraft([deleteOp(5)])],
    ['editorStudio.guestDraft.v2:a2', seedDraft([deleteOp(9), deleteOp(10)])],
  ]));
  const doc = makeDocument({ storage });
  assert.equal(doc.Guest.Draft.count(), 1);
  storage.setItem('editorStudio.guestDraft.v2:a2', seedDraft([deleteOp(9)]));
  assert.equal(doc.Guest.Draft.count(), 1, 'another account key must not affect this one');
});
