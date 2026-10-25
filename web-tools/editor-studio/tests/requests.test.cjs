const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');
const read = (name) => fs.readFileSync(path.join(root, name), 'utf8');

function guestHarness({ seed, onJson } = {}) {
  const draftKey = 'editorStudio.guestDraft.v2:a1';
  const local = new Map([[draftKey, JSON.stringify(seed)]]);
  const session = new Map([
    ['editorStudio.role', 'guest'],
    ['editorStudio.guestAccount', JSON.stringify({ id: 'a1', login: 'guest', name: 'Guest', pages: [] })],
  ]);
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
  const location = { pathname: '/index.html', search: '' };
  const window = {
    location, parent: null, addEventListener() {}, dispatchEvent() {}, setTimeout, clearTimeout,
    EditorCore: { Http: { postMessage: () => true, json: onJson || (async () => ({ ok: false })) } },
  };
  window.parent = window;
  const context = vm.createContext({
    window, location, document: {}, localStorage, sessionStorage, console, setTimeout, clearTimeout,
    crypto: require('node:crypto'),
  });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('guest-mode.js'), context);
  return { context, Guest: context.window.EditorCore.Guest, local, draftKey };
}

function requestHarness({ request, search, rawRows = [], rawRow = null, role = 'admin', onJson = null, rawGetError = false }) {
  const calls = [];
  const document = {
    readyState: 'loading', body: null,
    addEventListener() {}, querySelectorAll() { return []; },
  };
  const markers = { request: null, target: null };
  const window = {
    location: { pathname: '/items-editor.html', search },
    parent: null, addEventListener() {}, dispatchEvent() {}, setTimeout, clearTimeout,
    EditorCore: {
      Guest: { ready: async () => role },
      ChangeMarkers: {
        operationId: (op) => Number(op.previewId ?? (op.type === 'create' ? op.tempId : op.id)) || 0,
        setRequest(value, target) { markers.request = value; markers.target = target; },
        refresh() {},
      },
      Http: {
        json: async (url, options = {}) => {
          calls.push({ kind: 'json', url, options });
          if (onJson) return onJson(url, options);
          return { ok: true, request };
        },
        request: async (url, options = {}) => {
          calls.push({ kind: 'request', url, options });
          if (options.method === 'POST') throw new Error('A read-only preview attempted a write');
          if (url.includes('action=list')) return { ok: true, rows: rawRows };
          if (url.includes('action=get')) {
            if (rawGetError) throw new Error('404 record missing');
            return { ok: true, row: rawRow };
          }
          return { ok: true };
        },
        postMessage: () => true,
      },
    },
  };
  window.parent = window;
  const context = vm.createContext({
    window, document, location: window.location, console, URLSearchParams, URL,
    CustomEvent: class CustomEvent { constructor(type, options) { this.type = type; this.detail = options?.detail; } },
    MutationObserver: class { observe() {} }, setTimeout, clearTimeout,
  });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('request-context.js'), context);
  vm.runInContext(source('db-crud.js'), context);
  return { context, calls, markers, RequestContext: context.window.EditorCore.RequestContext, DBCrud: context.window.EditorCore.DBCrud };
}

test('request links use the manifest and field matching covers nested quest fields', () => {
  const document = { readyState: 'loading', addEventListener() {} };
  const window = {
    location: { pathname: '/quests-editor.html', search: '' },
    STUDIO_MANIFEST: [{ items: [
      { key: 'items', src: 'items-editor.html' },
      { key: 'quests', src: 'quests-editor.html' },
    ] }],
    EditorCore: {},
  };
  const context = vm.createContext({ window, document, URLSearchParams, console });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('change-markers.js'), context);
  vm.runInContext(source('request-workbench.js'), context);

  const url = context.window.EditorCore.RequestWorkbench.editorUrl('quest_bots', {
    request: 'CR-1', record: 41, field: 'DialogData',
  });
  assert.ok(url.startsWith('quests-editor.html?'));
  const parsed = new URL(url, 'https://editor.test/');
  assert.equal(parsed.searchParams.get('request'), 'CR-1');
  assert.equal(parsed.searchParams.get('resource'), 'quest_bots');
  assert.equal(parsed.searchParams.get('record'), '41');
  assert.equal(parsed.searchParams.get('field'), 'DialogData');

  const markers = context.window.EditorCore.ChangeMarkers;
  assert.equal(markers.fieldMatches('Position.x', 'PosX'), true);
  assert.equal(markers.fieldMatches('Position.y', 'PosY'), true);
  assert.equal(markers.fieldMatches('Position.y', 'PosX'), false);
  assert.equal(markers.fieldMatches('DialogData.0.text', 'DialogData'), true);
  assert.equal(markers.operationId({ type: 'create', tempId: 2000000001, previewId: 55 }), 55);

  const jsonValue = context.window.EditorCore.RequestWorkbench._jsonFieldValue;
  assert.equal(jsonValue('"scalar"'), '"scalar"');
  assert.equal(jsonValue('true'), 'true');
  assert.equal(jsonValue('null'), 'null');
  assert.equal(JSON.stringify(jsonValue('{"x":1}')), JSON.stringify({ x: 1 }));
});

test('guest revisions replace only their own operations and advance temporary IDs', () => {
  const seed = {
    v: 2, accountId: 'a1', nextTemp: 2000000002,
    ops: [
      { uid: 'local', type: 'update', resource: 'items', id: 3, data: { Name: 'Local' }, ts: 1, reqId: null },
      { uid: 'sent', type: 'update', resource: 'items', id: 7, data: { Name: 'Before' }, ts: 2,
        reqId: 'CR-1', reqVersion: 1, reqDirty: false, submittedAt: '2026-01-01' },
    ],
  };
  const { Guest } = guestHarness({ seed });
  const request = {
    id: 'CR-1', version: 2,
    operations: [
      { type: 'update', resource: 'items', id: 7, tempId: null, data: { Name: 'Revised', Value: 4 } },
      { type: 'create', resource: 'items', id: null, tempId: 2000000050, data: { Name: 'Added by admin' } },
    ],
  };
  assert.equal(Guest.Draft.replaceRequest(request), true);
  request.operations[0].data.Name = 'External mutation';

  const draft = Guest.Draft.get();
  assert.equal(draft.ops.length, 3);
  assert.equal(draft.ops.find((op) => op.uid === 'local').data.Name, 'Local');
  const revised = draft.ops.find((op) => op.type === 'update' && op.id === 7);
  assert.equal(revised.data.Name, 'Revised');
  assert.equal(revised.data.Value, 4);
  assert.equal(revised.reqVersion, 2);
  assert.equal(revised.reqDirty, false);
  assert.equal(revised.uid, 'sent');
  assert.equal(draft.nextTemp, 2000000051);
});

test('guest polling refreshes a newer request version once without resolving it', async () => {
  const seed = {
    v: 2, accountId: 'a1', nextTemp: 2000000001,
    ops: [{ uid: 'sent', type: 'update', resource: 'items', id: 7, data: { Name: 'Before' }, ts: 1,
      reqId: 'CR-1', reqVersion: 1, reqDirty: false }],
  };
  let detailCalls = 0;
  const revisedRequest = {
    id: 'CR-1', status: 'pending', version: 2,
    operations: [{ type: 'update', resource: 'items', id: 7, tempId: null, data: { Name: 'Admin revision' } }],
  };
  const { Guest } = guestHarness({
    seed,
    onJson: async (url) => {
      if (url.includes('action=mine')) return { ok: true, rows: [{ id: 'CR-1', status: 'pending', version: 2 }] };
      if (url.includes('action=get')) { detailCalls++; return { ok: true, request: revisedRequest }; }
      return { ok: false };
    },
  });

  const first = await Guest.syncSubmitted({ silent: true });
  assert.equal(first.resolved.length, 0);
  assert.equal(JSON.stringify(first.revised), JSON.stringify([{ reqId: 'CR-1', version: 2 }]));
  assert.equal(Guest.Draft.get().ops[0].data.Name, 'Admin revision');
  assert.equal(Guest.Draft.get().ops[0].reqVersion, 2);
  const second = await Guest.syncSubmitted({ silent: true });
  assert.equal(second.revised.length, 0);
  assert.equal(detailCalls, 1);
});

test('request preview overlays records and blocks every DB mutation path', async () => {
  const tempId = 2000000001;
  const request = {
    id: 'CR-1', status: 'pending', version: 3,
    operations: [
      { type: 'update', resource: 'items', id: 7, tempId: null, data: { Name: 'Requested' } },
      { type: 'create', resource: 'items', id: null, tempId, data: { Name: 'Virtual create' } },
      { type: 'delete', resource: 'items', id: 9, tempId: null, data: {}, before: { Name: 'Deleted snapshot' } },
    ],
  };
  const harness = requestHarness({
    request,
    search: '?request=CR-1&resource=items&record=7&field=Name',
    rawRows: [{ ID: 3, Name: 'Other' }, { ID: 7, Name: 'Database', Value: 1 }],
    rawRow: { ID: 7, Name: 'Database', Value: 1 },
  });
  await harness.RequestContext.ready();
  assert.equal(harness.RequestContext.isReadOnly(), true);
  assert.equal(JSON.stringify(harness.markers.target), JSON.stringify({ resource: 'items', id: 7, field: 'Name' }));

  const list = await harness.DBCrud.list('items', { limit: 2000 });
  assert.equal(list.rows[0].ID, 7, 'the target is visible in the first generic page');
  assert.equal(list.rows[0].Name, 'Requested');
  assert.equal(list.rows[0].Value, 1);
  assert.ok(list.rows.some((row) => row.ID === tempId && row.Name === 'Virtual create'));
  assert.ok(list.rows.some((row) => row.ID === 9 && row.Name === 'Deleted snapshot'));

  const beforeGetCalls = harness.calls.length;
  const created = await harness.DBCrud.get('items', tempId);
  assert.equal(created.row.Name, 'Virtual create');
  assert.equal(harness.calls.length, beforeGetCalls, 'a virtual create must not issue a real DB get');
  const updated = await harness.DBCrud.get('items', 7);
  assert.equal(updated.row.Name, 'Requested');
  assert.equal(updated.row.Value, 1);

  await assert.rejects(harness.DBCrud.create('items', { Name: 'Nope' }), /read-only/);
  await assert.rejects(harness.DBCrud.update('items', 7, { Name: 'Nope' }), /read-only/);
  await assert.rejects(harness.DBCrud.remove('items', 7), /read-only/);
  await assert.rejects(harness.DBCrud.renameCraftGroup({}, 'Nope'), /read-only/);
  assert.equal(harness.calls.some((call) => call.options?.method === 'POST'), false);
});

test('a missing database row still opens from the request before snapshot', async () => {
  const request = {
    id: 'CR-MISSING', status: 'pending', version: 1,
    operations: [{ type: 'delete', resource: 'items', id: 9, tempId: null, data: {}, before: { ID: 9, Name: 'Snapshot' } }],
  };
  const harness = requestHarness({
    request, search: '?request=CR-MISSING&resource=items&record=9', rawGetError: true,
  });
  await harness.RequestContext.ready();
  const result = await harness.DBCrud.get('items', 9);
  assert.equal(result.ok, true);
  assert.equal(result.row.Name, 'Snapshot');
});

test('admin edits a pending request through the familiar DBCrud editor path', async () => {
  const original = {
    id: 'CR-EDIT', status: 'pending', version: 3,
    operations: [{ type: 'update', resource: 'items', id: 7, tempId: null, data: { Name: 'Requested' } }],
  };
  let revised = null;
  const harness = requestHarness({
    request: original,
    search: '?request=CR-EDIT&resource=items&record=7&field=Name&edit=1',
    rawRow: { ID: 7, Name: 'Database', Value: 1 },
    onJson: async (url, options) => {
      if (url.includes('action=revise')) {
        revised = { ...original, version: 4, operations: options.body.operations };
        return { ok: true, request: revised };
      }
      return { ok: true, request: original };
    },
  });
  await harness.RequestContext.ready();
  assert.equal(harness.RequestContext.isEditing(), true);
  assert.equal(harness.RequestContext.isReadOnly(), false);
  await harness.DBCrud.update('items', 7, { Name: 'Admin correction', Value: 1 });
  assert.ok(revised);
  const call = harness.calls.find((entry) => entry.kind === 'json' && entry.url.includes('action=revise'));
  assert.equal(call.options.body.version, 3);
  assert.equal(JSON.stringify(call.options.body.operations[0].data), JSON.stringify({ Name: 'Admin correction' }));
  assert.equal(harness.RequestContext.request().version, 4);
  assert.equal(harness.RequestContext.operation('items', 7).data.Name, 'Admin correction');
  assert.equal(harness.calls.some((entry) => entry.kind === 'request' && entry.options?.method === 'POST'), false);
});

test('admin create and subsequent edit stay inside the pending request', async () => {
  const original = {
    id: 'CR-CREATE', status: 'pending', version: 1,
    operations: [{ type: 'update', resource: 'items', id: 7, tempId: null, data: { Name: 'Existing' } }],
  };
  let current = original;
  const harness = requestHarness({
    request: original,
    search: '?request=CR-CREATE&resource=items&record=7&edit=1',
    onJson: async (url, options) => {
      if (url.includes('action=revise')) {
        current = { ...current, version: current.version + 1, operations: options.body.operations };
        return { ok: true, request: current };
      }
      return { ok: true, request: current };
    },
  });
  await harness.RequestContext.ready();
  const created = await harness.DBCrud.create('items', { Name: 'New record' });
  assert.ok(created.id >= 2000000000);
  assert.equal(current.operations.length, 2);
  await harness.DBCrud.update('items', created.id, { Name: 'Corrected new record' });
  const operation = harness.RequestContext.operation('items', created.id);
  assert.equal(operation.type, 'create');
  assert.equal(operation.data.Name, 'Corrected new record');
  const row = await harness.DBCrud.get('items', created.id);
  assert.equal(row.row.Name, 'Corrected new record');
  assert.equal(harness.calls.some((entry) => entry.kind === 'request' && entry.options?.method === 'POST'), false);
});

test('saving an unchanged delete snapshot removes that operation from the request', async () => {
  const original = {
    id: 'CR-CANCEL', status: 'pending', version: 1,
    operations: [
      { type: 'delete', resource: 'items', id: 9, tempId: null, data: {}, before: { ID: 9, Name: 'Snapshot' } },
      { type: 'update', resource: 'items', id: 7, tempId: null, data: { Name: 'Keep' } },
    ],
  };
  let revised;
  const harness = requestHarness({
    request: original,
    search: '?request=CR-CANCEL&resource=items&record=9&edit=1',
    rawRow: { ID: 9, Name: 'Snapshot' },
    onJson: async (url, options) => {
      if (url.includes('action=revise')) {
        revised = { ...original, version: 2, operations: options.body.operations };
        return { ok: true, request: revised };
      }
      return { ok: true, request: original };
    },
  });
  await harness.RequestContext.ready();
  await harness.DBCrud.update('items', 9, { Name: 'Snapshot' });
  assert.equal(revised.operations.length, 1);
  assert.equal(revised.operations[0].id, 7);
});

test('composite admin request edits commit one revision in a batch', async () => {
  const original = {
    id: 'CR-BATCH', status: 'pending', version: 2,
    operations: [{ type: 'update', resource: 'items', id: 7, tempId: null, data: { Name: 'Requested' } }],
  };
  let revisions = 0;
  const harness = requestHarness({
    request: original,
    search: '?request=CR-BATCH&resource=items&record=7&edit=1',
    rawRow: { ID: 7, Name: 'Database', Value: 1 },
    onJson: async (url, options) => {
      if (url.includes('action=revise')) {
        revisions++;
        return { ok: true, request: { ...original, version: 3, operations: options.body.operations } };
      }
      return { ok: true, request: original };
    },
  });
  await harness.RequestContext.ready();
  await harness.RequestContext.batch(async () => {
    await harness.DBCrud.update('items', 7, { Name: 'One' });
    await harness.DBCrud.update('items', 7, { Value: 4 });
  });
  assert.equal(revisions, 1);
  assert.equal(JSON.stringify(harness.RequestContext.operation('items', 7).data), JSON.stringify({ Name: 'One', Value: 4 }));
});

test('a stale admin editor cannot overwrite a newer request revision', async () => {
  const original = {
    id: 'CR-STALE', status: 'pending', version: 2,
    operations: [{ type: 'update', resource: 'items', id: 7, tempId: null, data: { Name: 'Requested' } }],
  };
  let revisions = 0;
  const harness = requestHarness({
    request: original,
    search: '?request=CR-STALE&resource=items&record=7&edit=1',
    rawRow: { ID: 7, Name: 'Database', Value: 1 },
    onJson: async (url) => {
      if (url.includes('action=revise')) {
        revisions++;
        return { ok: false, status: 409, code: 'version_conflict', error: 'Запрос уже изменён другой стороной.' };
      }
      return { ok: true, request: original };
    },
  });
  await harness.RequestContext.ready();
  await assert.rejects(harness.DBCrud.update('items', 7, { Name: 'Local' }), /другой стороной/);
  assert.equal(harness.RequestContext.request().version, 2);
  await assert.rejects(harness.DBCrud.update('items', 7, { Name: 'Again' }), /Reopen it/);
  assert.equal(revisions, 1);
});

test('an approved create deep link resolves its temporary ID to the real record', async () => {
  const request = {
    id: 'CR-2', status: 'approved', version: 1,
    review: { idMap: { 2000000001: 55 } },
    operations: [{ type: 'create', resource: 'items', id: null, tempId: 2000000001, data: { Name: 'Requested name' } }],
  };
  const harness = requestHarness({
    request,
    search: '?request=CR-2&resource=items&record=2000000001&field=Name',
    rawRows: [{ ID: 55, Name: 'Later database value' }],
  });
  await harness.RequestContext.ready();
  assert.equal(harness.RequestContext.target().id, 55);
  const rows = harness.RequestContext.overlayRows('items', [{ ID: 55, Name: 'Later database value' }]);
  assert.equal(rows.length, 1);
  assert.equal(rows[0].ID, 55);
  assert.equal(rows[0].Name, 'Requested name');
});

test('PHP revision and approval contracts are versioned and apply stored request operations', () => {
  const php = read('api/change-requests.php');
  const revise = php.split("if ($action === 'revise')")[1].split('// ── Admin only')[0];
  assert.ok(revise.includes("'code' => 'version_conflict'"));
  assert.ok(revise.includes('cr_prepare_for_submit'));
  assert.ok(revise.includes("cr_assert_guest_resources"));
  assert.ok(revise.includes("cr_msg_add"));

  const approve = php.split("if ($action === 'approve')")[1].split("if ($action === 'reject')")[0];
  assert.ok(approve.includes("$version = (int)($body['version'] ?? 0)"));
  assert.ok(approve.includes('if ($version !== $currentVersion)'));
  assert.ok(approve.includes('cr_apply($db, $req, $force)'));
  const lib = read('api/change-requests-lib.php');
  assert.ok(lib.includes("$view['review']['idMap'] = $req['review']['idMap']"));

  const workbench = source('request-workbench.js');
  assert.ok(workbench.includes('if (conflict) saveButton.disabled = true'));
  const page = read('change-requests.html');
  assert.ok(page.includes('version: r.version'));
  assert.ok(page.includes('data-open-record'));
  assert.ok(page.includes('openRequestInEditor(A.detail)'));
  assert.ok(!page.includes('RequestWorkbench.open(A.detail)'));
  // Адрес записи и флаг правки запроса собирает общий модуль ссылок.
  assert.ok(workbench.includes('data-request-edit'));
  assert.ok(page.includes('recordLinkAttrs'));
});
