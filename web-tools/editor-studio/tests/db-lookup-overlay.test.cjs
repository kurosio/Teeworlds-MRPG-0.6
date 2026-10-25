const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const source = (name) => fs.readFileSync(path.join(__dirname, '..', 'editor-core', name), 'utf8');

function harness({ request = null, guestOps = [], role = 'admin', respond } = {}) {
  const calls = [];
  const getId = (op) => Number(request?.review?.idMap?.[op.tempId])
    || Number(op.type === 'create' ? op.tempId : op.id);
  const requestContext = {
    active: () => !!request,
    ready: async () => request,
    hasResource: (resource) => (request?.operations || []).some((op) => op.resource === resource),
    request: () => request,
    overlayRows(resource, rows) {
      const ops = (request?.operations || []).filter((op) => op.resource === resource);
      const byId = new Map(ops.map((op) => [getId(op), op]));
      const found = new Set();
      const out = rows.map((row) => {
        const op = byId.get(Number(row.ID));
        if (!op) return row;
        found.add(Number(row.ID));
        return op.type === 'create' || op.type === 'update' ? { ...row, ...op.data } : row;
      });
      for (const op of ops) {
        const id = getId(op);
        if (op.type === 'create' && !found.has(id)) out.push({ ID: id, ...op.data });
      }
      return out;
    },
    overlayRow(resource, id, row) {
      const op = (request?.operations || []).find((entry) => entry.resource === resource && getId(entry) === Number(id));
      if (!op) return row;
      if (op.type === 'delete') return null;
      return { ...(row || op.before || {}), ID: getId(op), ...op.data };
    },
  };
  const guest = {
    ready: async () => role,
    Draft: {
      get: () => ({ ops: guestOps }),
      overlayRows(resource, rows, { offset = 0 } = {}) {
        const ops = guestOps.filter((op) => op.resource === resource);
        const deleted = new Set(ops.filter((op) => op.type === 'delete').map((op) => Number(op.id)));
        const updates = new Map(ops.filter((op) => op.type === 'update').map((op) => [Number(op.id), op]));
        let out = rows.filter((row) => !deleted.has(Number(row.ID))).map((row) => {
          const op = updates.get(Number(row.ID));
          return op ? { ...row, ...op.data } : row;
        });
        if (!offset) {
          out = out.concat(ops.filter((op) => op.type === 'create')
            .map((op) => ({ ID: op.tempId, ...op.data })));
        }
        return out;
      },
      overlayRow(resource, id, row) {
        const op = guestOps.find((entry) => entry.resource === resource
          && Number(entry.type === 'create' ? entry.tempId : entry.id) === Number(id));
        if (!op) return row;
        if (op.type === 'delete') return null;
        return { ...(row || op.before || {}), ID: Number(op.type === 'create' ? op.tempId : op.id), ...op.data };
      },
    },
  };
  const window = {
    EditorCore: {
      RequestContext: requestContext,
      Guest: guest,
      utils: {
        createRequestCache: () => ({
          clear() {},
          async load(_key, fetcher) { return fetcher(); },
        }),
      },
      Http: {
        async json(url, options = {}) {
          calls.push({ url, options });
          const data = respond ? await respond(url, options) : { ok: true, items: [], total: 0, has_more: false };
          return { ok: true, data };
        },
      },
    },
  };
  const context = vm.createContext({ window, URLSearchParams, console });
  vm.runInContext(source('db.js'), context);
  return { DB: window.EditorCore.DB, calls };
}

const listResponse = (items = [], total = items.length) => ({ ok: true, items, total, has_more: false });

test('pending request creates appear in item lookups and are marked separately', async () => {
  const tempId = 2000000123;
  const request = {
    id: 'CR-LOOKUP', status: 'pending', version: 1,
    operations: [{ type: 'create', resource: 'items', id: null, tempId, data: { Name: 'Moonstone shard' } }],
  };
  const h = harness({
    request,
    respond: (url) => {
      const query = new URL(url, 'https://editor.test').searchParams;
      if (query.get('action') === 'list') {
        return query.get('search') === 'Moonstone' ? listResponse([], 0) : listResponse([{ value: '7', label: 'Iron bar' }], 1);
      }
      return { ok: true, item: null };
    },
  });

  const filtered = await h.DB.list('items', { search: 'Moonstone', withTotal: true, limit: 100, offset: 0 });
  assert.equal(filtered.items.length, 1);
  assert.equal(filtered.items[0].value, String(tempId));
  assert.equal(filtered.items[0].label, 'Moonstone shard');
  assert.equal(filtered.items[0].__editorChangeKind, 'request');
  assert.equal(filtered.total, 1);

  const all = await h.DB.list('items', { limit: 100, offset: 0 });
  assert.deepEqual(Array.from(all.items, (item) => item.value), ['7', String(tempId)]);
  const callsBeforeGetOne = h.calls.length;
  const one = await h.DB.getOne('items', tempId);
  assert.deepEqual(one.item.value, String(tempId));
  assert.equal(one.item.label, 'Moonstone shard');
  assert.equal(one.item.__editorChangeKind, 'request');
  assert.equal(h.calls.length, callsBeforeGetOne, 'temporary create is resolved locally, not queried as a DB ID');
});

test('guest draft-created items are available and visibly tagged as drafts', async () => {
  const tempId = 2000000456;
  const h = harness({
    role: 'guest',
    guestOps: [
      { type: 'create', resource: 'items', tempId, data: { Name: 'Drafted item' }, reqId: null },
      { type: 'create', resource: 'items', tempId: tempId + 1, data: { Name: 'Submitted item' }, reqId: 'CR-GUEST' },
    ],
    respond: () => listResponse([], 0),
  });
  const result = await h.DB.list('items', { search: 'Drafted', withTotal: true, limit: 100, offset: 0 });
  assert.equal(result.items.length, 1);
  assert.equal(result.items[0].value, String(tempId));
  assert.equal(result.items[0].__editorChangeKind, 'draft');
  assert.equal(result.total, 1);

  const submitted = await h.DB.list('items', { search: 'Submitted', withTotal: true, limit: 100, offset: 0 });
  assert.equal(submitted.items[0].value, String(tempId + 1));
  assert.equal(submitted.items[0].__editorChangeKind, 'request');
  assert.equal(submitted.items[0].__editorRequestId, 'CR-GUEST');
});

test('approved creates use the assigned database ID and no longer appear pending', async () => {
  const tempId = 2000000789;
  const realId = 81;
  const request = {
    id: 'CR-APPROVED', status: 'approved', version: 1,
    review: { idMap: { [tempId]: realId } },
    operations: [{ type: 'create', resource: 'items', id: null, tempId, data: { Name: 'Applied item' } }],
  };
  const h = harness({
    request,
    respond: (url) => {
      const query = new URL(url, 'https://editor.test').searchParams;
      if (query.get('action') === 'list') return listResponse([{ value: String(realId), label: 'Applied item' }], 1);
      return { ok: true, item: null };
    },
  });
  const result = await h.DB.list('items', { limit: 100, offset: 0 });
  assert.deepEqual(Array.from(result.items, (item) => item.value), [String(realId)]);
  assert.equal(result.items[0].__editorChangeKind, undefined);
  const one = await h.DB.getOne('items', realId);
  assert.equal(one.item.value, String(realId));
  assert.equal(one.item.label, 'Applied item');
});
