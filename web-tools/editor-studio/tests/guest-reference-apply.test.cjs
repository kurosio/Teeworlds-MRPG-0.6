const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');
const SCHEMA = require('../editor-core/reference-schema.json');
const { I, W, B } = require('./reference-fixtures.cjs');

const DRAFT_KEY = 'editorStudio.guestDraft.v2:a1';
const CONFIG = {
  reference_schema: SCHEMA,
  resources: {
    items: { title: 'Items', label: 'Name', columns: ['Name', 'Data'] },
    crafts: { title: 'Crafts', label: 'GroupName', columns: ['GroupName', 'ItemID', 'RequiredItems'] },
    bots_info: { title: 'Bots', label: 'Name', columns: ['Name', 'EquippedModules', 'SlotHammer'] },
    bots_npc: { title: 'NPC', columns: ['BotID', 'DialogData'] },
    worlds: { title: 'Worlds', label: 'Name', columns: ['Name', 'RespawnWorldID'] },
  },
};

/** One signed-in guest document; drafts are shared through localStorage. */
function guestDoc({ ops = [], nextTemp = I + 1, onJson = api([]) } = {}) {
  const local = new Map([[DRAFT_KEY, JSON.stringify({ v: 2, accountId: 'a1', nextTemp, ops })]]);
  const session = new Map([
    ['editorStudio.role', 'guest'],
    ['editorStudio.guestAccount', JSON.stringify({ id: 'a1', login: 'guest', name: 'Guest', pages: [] })],
  ]);
  const window = {
    location: { pathname: '/index.html', search: '' }, parent: null,
    addEventListener() {}, dispatchEvent() {}, setTimeout, clearTimeout,
    EditorCore: { Http: { postMessage: () => true, json: onJson } },
  };
  window.parent = window;
  const context = vm.createContext({
    window, location: window.location, document: {}, console, setTimeout, clearTimeout,
    crypto: require('node:crypto'),
    localStorage: {
      getItem: (k) => local.get(k) ?? null,
      setItem: (k, v) => local.set(k, String(v)),
      removeItem: (k) => local.delete(k),
    },
    sessionStorage: {
      getItem: (k) => session.get(k) ?? null,
      setItem: (k, v) => session.set(k, String(v)),
      removeItem: (k) => session.delete(k),
    },
  });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('reference-data.js'), context);
  vm.runInContext(source('guest-mode.js'), context);
  return { Guest: context.window.EditorCore.Guest, local };
}

function api(requests) { return async (url) => {
  if (String(url).includes('action=config')) return { ok: true, data: CONFIG };
  if (String(url).includes('action=mine')) return { ok: true, rows: requests.map((r) => ({ id: r.id, status: r.status, version: r.version || 1 })) };
  if (String(url).includes('action=get')) {
    const id = new URL(String(url), 'http://localhost').searchParams.get('id');
    const found = requests.find((r) => r.id === id);
    return found ? { ok: true, request: found } : { ok: false };
  }
  return { ok: false };
}; }

test('approval rewrites every typed reference of the still-pending draft to real IDs', async () => {
  const request = {
    id: 'CR-1', status: 'approved', version: 1,
    operations: [
      { type: 'create', resource: 'items', tempId: I, data: { Name: 'New sword' } },
      { type: 'create', resource: 'worlds', tempId: W, data: { Name: 'New world' } },
      { type: 'create', resource: 'bots_info', tempId: B, data: { Name: 'New bot' } },
    ],
    review: { idMap: { [I]: 81, [W]: 82, [B]: 83 } },
  };
  const pending = [
    { uid: 'p1', type: 'create', resource: 'items', tempId: I, data: { Name: 'New sword' }, ts: 1, reqId: 'CR-1' },
    { uid: 'p2', type: 'create', resource: 'worlds', tempId: W, data: { Name: 'New world' }, ts: 1, reqId: 'CR-1' },
    { uid: 'p3', type: 'create', resource: 'bots_info', tempId: B, data: { Name: 'New bot' }, ts: 1, reqId: 'CR-1' },
    { uid: 'p4', type: 'create', resource: 'crafts', tempId: I + 3,
      data: { GroupName: 'Recipe', ItemID: I, RequiredItems: `[${I}/2],[17/1]` }, ts: 2, reqId: null },
    { uid: 'p5', type: 'update', resource: 'bots_info', id: 9,
      data: { SlotHammer: I, EquippedModules: `${I}, ${B},17` }, ts: 2, reqId: null },
    { uid: 'p6', type: 'update', resource: 'bots_npc', id: 4,
      data: { BotID: B, DialogData: [{ left_speaker_id: B, text: `Take <item_${I}> in <world_${W}>` }] }, ts: 2, reqId: null },
    { uid: 'p7', type: 'update', resource: 'items', id: 12, data: { Data: { random_box: [{ item_id: I, value: 3 }] } }, ts: 2, reqId: null },
  ];
  const { Guest } = guestDoc({ ops: pending, nextTemp: I + 4, onJson: api([request]) });

  const result = await Guest.syncSubmitted({ silent: true });
  assert.deepEqual(Array.from(result.resolved, (r) => r.op.uid), ['p1', 'p2', 'p3']);

  const ops = Guest.Draft.get().ops;
  assert.equal(ops.length, 4);
  assert.deepEqual(JSON.parse(JSON.stringify(ops.find((o) => o.uid === 'p4').data)), { GroupName: 'Recipe', ItemID: 81, RequiredItems: '[81/2],[17/1]' });
  assert.deepEqual(JSON.parse(JSON.stringify(ops.find((o) => o.uid === 'p5').data)), { SlotHammer: 81, EquippedModules: '81, 83,17' });
  assert.deepEqual(JSON.parse(JSON.stringify(ops.find((o) => o.uid === 'p6').data)),
    { BotID: 83, DialogData: [{ left_speaker_id: 83, text: 'Take <item_81> in <world_82>' }] });
  assert.deepEqual(JSON.parse(JSON.stringify(ops.find((o) => o.uid === 'p7').data)), { Data: { random_box: [{ item_id: 81, value: 3 }] } });
  // The mapping is consumed: a second poll changes nothing.
  const again = await Guest.syncSubmitted({ silent: true });
  assert.deepEqual(Array.from(again.resolved), []);
  assert.deepEqual(Guest.Draft.get().ops.find((o) => o.uid === 'p4').data.ItemID, 81);
});

test('an approval without a complete ID mapping is retried instead of half-applied', async () => {
  const request = {
    id: 'CR-2', status: 'approved', version: 1,
    operations: [
      { type: 'create', resource: 'items', tempId: I, data: { Name: 'Sword' } },
      { type: 'create', resource: 'items', tempId: I + 1, data: { Name: 'Shield' } },
    ],
    review: { idMap: { [I]: 81 } }, // the second create is missing
  };
  const { Guest } = guestDoc({
    ops: [
      { uid: 'c1', type: 'create', resource: 'items', tempId: I, data: { Name: 'Sword' }, ts: 1, reqId: 'CR-2' },
      { uid: 'c2', type: 'create', resource: 'items', tempId: I + 1, data: { Name: 'Shield' }, ts: 1, reqId: 'CR-2' },
      { uid: 'p1', type: 'create', resource: 'crafts', tempId: I + 2, data: { ItemID: I }, ts: 2, reqId: null },
    ],
    nextTemp: I + 3,
    onJson: api([request]),
  });

  const result = await Guest.syncSubmitted({ silent: true });
  assert.deepEqual(Array.from(result.resolved), []);
  const ops = Guest.Draft.get().ops;
  assert.equal(ops.length, 3);
  assert.equal(ops.find((o) => o.uid === 'p1').data.ItemID, I, 'reserved ID stays until the mapping is complete');
});

test('a temporary ID of another request never rewrites local values', async () => {
  const foreign = 2000000500;
  const request = {
    id: 'CR-3', status: 'approved', version: 1,
    operations: [{ type: 'create', resource: 'items', tempId: I, data: { Name: 'Sword' } }],
    review: { idMap: { [I]: 81, [foreign]: 999 } },
  };
  const { Guest } = guestDoc({
    ops: [
      { uid: 'c1', type: 'create', resource: 'items', tempId: I, data: { Name: 'Sword' }, ts: 1, reqId: 'CR-3' },
      { uid: 'p1', type: 'create', resource: 'crafts', tempId: I + 1, data: { ItemID: foreign }, ts: 2, reqId: null },
    ],
    nextTemp: I + 2,
    onJson: api([request]),
  });

  await Guest.syncSubmitted({ silent: true });
  assert.equal(Guest.Draft.get().ops.find((o) => o.uid === 'p1').data.ItemID, foreign);
});

test('a rejected request keeps local references untouched', async () => {
  const request = {
    id: 'CR-4', status: 'rejected', version: 1,
    operations: [{ type: 'create', resource: 'items', tempId: I, data: { Name: 'Sword' } }],
    review: { comment: 'no' },
  };
  const { Guest } = guestDoc({
    ops: [
      { uid: 'c1', type: 'create', resource: 'items', tempId: I, data: { Name: 'Sword' }, ts: 1, reqId: 'CR-4' },
      { uid: 'p1', type: 'create', resource: 'crafts', tempId: I + 1, data: { ItemID: I }, ts: 2, reqId: null },
    ],
    nextTemp: I + 2,
    onJson: api([request]),
  });

  const result = await Guest.syncSubmitted({ silent: true });
  assert.deepEqual(Array.from(result.resolved, (r) => r.status), ['rejected']);
  assert.equal(Guest.Draft.get().ops.find((o) => o.uid === 'p1').data.ItemID, I);
});

test('a new record still referenced by the draft cannot be dropped silently', async () => {
  const { Guest } = guestDoc({
    ops: [
      { uid: 'c1', type: 'create', resource: 'items', tempId: I, data: { Name: 'Sword' }, ts: 1, reqId: null },
      { uid: 'p1', type: 'create', resource: 'crafts', tempId: I + 1, data: { ItemID: I, RequiredItems: `[${I}/1]` }, ts: 2, reqId: null },
    ],
    nextTemp: I + 2,
    onJson: api([]),
  });
  await Guest.loadConfig();

  assert.throws(() => Guest.Draft.removeOp('c1'), /still referenced by another pending change/);
  assert.equal(Guest.Draft.count(), 2);
  // Removing the reference first makes the record removable again.
  Guest.Draft.removeOp('p1');
  Guest.Draft.removeOp('c1');
  assert.equal(Guest.Draft.count(), 0);
});

test('references inside serialized lists are detected too', async () => {
  const { Guest } = guestDoc({
    ops: [
      { uid: 'c1', type: 'create', resource: 'items', tempId: I, data: { Name: 'Sword' }, ts: 1, reqId: null },
      { uid: 'p1', type: 'create', resource: 'bots_info', tempId: I + 1, data: { EquippedModules: `17,${I}` }, ts: 2, reqId: null },
    ],
    nextTemp: I + 2,
    onJson: api([]),
  });
  await Guest.loadConfig();
  assert.throws(() => Guest.Draft.removeOp('c1'), /still referenced by another pending change/);
});

test('world 0 is a real record for the draft, other sections still reject it', () => {
  const worldOp = { uid: 'w', type: 'update', resource: 'worlds', id: 0, data: { Name: 'Main' }, ts: 1, reqId: null };
  const itemOp = { uid: 'i', type: 'update', resource: 'items', id: 0, data: { Name: 'Nope' }, ts: 1, reqId: null };
  const negative = { uid: 'n', type: 'update', resource: 'items', id: -3, data: { Name: 'Nope' }, ts: 1, reqId: null };
  const { Guest, local } = guestDoc({ ops: [worldOp, itemOp, negative] });
  const ops = Guest.Draft.get().ops;
  assert.deepEqual(Array.from(ops, (o) => o.uid), ['w']);
  assert.equal(ops[0].id, 0);
  assert.equal(JSON.parse(local.get(DRAFT_KEY)).ops.length, 3, 'a broken stored op is never rewritten in place');
});

test('temporary IDs are never reused, even when the stored counter is stale', async () => {
  const { Guest, local } = guestDoc({
    ops: [{ uid: 'c1', type: 'create', resource: 'items', tempId: I + 5, data: { Name: 'Sword' }, ts: 1, reqId: null }],
    nextTemp: I, // older than the highest used temp ID
  });
  const created = await Guest.Draft.create('items', { Name: 'Shield' });
  assert.equal(created.id, I + 6);
  assert.equal(Guest.Draft.get().ops.filter((o) => o.type === 'create').length, 2);
  assert.ok(JSON.parse(local.get(DRAFT_KEY)).nextTemp > I + 6);
});
