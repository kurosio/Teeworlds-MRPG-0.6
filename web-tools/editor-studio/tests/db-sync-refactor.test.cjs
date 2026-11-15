const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { JSDOM } = require('jsdom');

const source = name => fs.readFileSync(path.join(__dirname, '../editor-core', name), 'utf8');
const plain = value => JSON.parse(JSON.stringify(value));

function harness(t) {
  const dom = new JSDOM('', { url: 'https://studio.test/', runScripts: 'outside-only' });
  t.after(() => dom.window.close());
  const w = dom.window;
  w.eval(source('utils.js'));
  const calls = [];
  w.EditorCore.Http = { json: async (url, options) => {
    calls.push({ url, options: plain(options) });
    return { ok: true, data: { ok: true } };
  } };
  w.eval(source('db-sync.js'));
  return { w, calls, sync: w.EditorCore.Sync };
}

test('sync API preserves methods, payloads and long-operation timeouts', async t => {
  const { calls, sync } = harness(t);
  await sync.listProfiles();
  await sync.getTarget();
  await sync.syncMap();
  await sync.saveTarget({ profile_id: 8 });
  await sync.setAsTarget(8);
  await sync.compareSchema();
  await sync.applyOperations({ operations: [{ action: 'delete', id: 2 }], targetProfileId: '8', confirmPhrase: 'yes' });
  await sync.pullPreview({ targetProfileId: '8' });
  await sync.pullFromTarget({ targetProfileId: '8', confirmPhrase: 'yes' });
  await sync.createBackup({ confirmDb: 'game', label: 'before' });
  assert.deepEqual(calls.map(({ url, options }) => [url, options.method || 'GET', options.timeout, options.body && JSON.parse(options.body)]), [
    ['api/db-sync.php?action=list_profiles', 'GET', 30000, undefined],
    ['api/db-sync.php?action=get_target', 'GET', 30000, undefined],
    ['api/db-sync.php?action=sync_map', 'GET', 30000, undefined],
    ['api/db-sync.php?action=save_target', 'POST', 30000, { profile_id: 8 }],
    ['api/db-sync.php?action=set_as_target', 'POST', 30000, { profile_id: 8 }],
    ['api/db-sync.php?action=compare_schema', 'POST', 30000, { target_profile_id: '' }],
    ['api/db-sync.php?action=apply_operations', 'POST', 30000, { operations: [{ action: 'delete', id: 2 }], target_profile_id: '8', confirm_phrase: 'yes' }],
    ['api/db-sync.php?action=pull_preview', 'POST', 120000, { target_profile_id: '8' }],
    ['api/db-sync.php?action=pull_from_target', 'POST', 900000, { target_profile_id: '8', confirm_phrase: 'yes' }],
    ['api/db-maintenance.php?action=create_dump', 'POST', 300000, { confirm_db: 'game', label: 'before' }],
  ]);
});

test('sync API keeps read fallbacks and rejects write failures', async t => {
  const { w, sync } = harness(t);
  w.EditorCore.Http.json = async () => ({ ok: false, error: 'offline' });
  assert.deepEqual(plain(await sync.listProfiles()), { ok: false, error: 'offline', profiles: [] });
  assert.deepEqual(plain(await sync.getTarget()), { ok: false, error: 'offline', target: null });
  await assert.rejects(sync.saveTarget({}), /offline/);
  await assert.rejects(sync.applyOperations(), /Cannot destructure/);
});

test('sync history records individual and batched entries once, snapshots data and bounds storage', t => {
  const { w, sync } = harness(t);
  const data = { Name: 'Initial' };
  const one = sync.record('worlds', 'update', { id: 4, data });
  data.Name = 'Changed';
  assert.equal(one.data.Name, 'Initial');
  assert.equal(sync.recordMany('worlds', 'create', [{ data: { Name: 'First' } }, { data: { Name: 'Second' } }]), 2);
  assert.equal(sync.getUnappliedCount(), 3);
  sync.markAsApplied(one.id);
  assert.equal(sync.getUnappliedCount(), 2);
  sync.clearHistory({ onlyApplied: true });
  assert.equal(sync.getHistory().length, 2);
  assert.equal(sync.recordMany('worlds', 'create', Array.from({ length: 505 }, () => ({}))), 505);
  assert.equal(sync.getHistory().length, 500);
  assert.equal(JSON.parse(w.localStorage.getItem('es_sync_history')).length, 500);
});

test('sync history recovers valid entries and migrates names around corrupt records', t => {
  const { w, sync } = harness(t);
  w.localStorage.setItem('es_sync_history', JSON.stringify([
    null, 42, { id: 'a', resource: 'worlds', action: 'update', data: { Name: '  World  ' } },
    { id: 'missing-action', resource: 'worlds' }, { id: 'b', resource: 'items', action: 'create' },
  ]));
  assert.deepEqual(plain(sync.getHistory()).map(entry => [entry.id, entry.record_name || '']), [['a', 'World'], ['b', '']]);
  assert.equal(JSON.parse(w.localStorage.getItem('es_sync_history')).length, 2);
  assert.equal(sync.getUnappliedCount(), 2);
});

test('quota fallback persists only valid history entries', t => {
  const { w, sync } = harness(t);
  const storage = w.Storage.prototype;
  const original = storage.setItem;
  let attempts = 0, persisted;
  storage.setItem = function (key, value) {
    if (key === 'es_sync_history') {
      if (++attempts === 1) throw new Error('quota');
      persisted = JSON.parse(value);
    }
    return original.call(this, key, value);
  };
  // getHistory is public; tolerate a malformed entry added by an older caller.
  sync.getHistory().push(null);
  sync.record('worlds', 'create', { data: { Name: 'Safe' } });
  assert.equal(attempts, 2);
  assert.equal(persisted.length, 1);
  assert.equal(persisted[0].record_name, 'Safe');
});
