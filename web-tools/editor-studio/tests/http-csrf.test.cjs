const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');

const TOKEN_A = 'a'.repeat(64);
const TOKEN_B = 'b'.repeat(64);
const TOKEN_C = 'c'.repeat(64);

const memorySession = (initial = {}) => {
  const map = new Map(Object.entries(initial));
  return {
    map,
    getItem: (k) => (map.has(k) ? map.get(k) : null),
    setItem: (k, v) => map.set(k, String(v)),
    removeItem: (k) => map.delete(k),
  };
};

const reply = (body, status = 200, headers = {}) => ({
  ok: status < 400,
  status,
  headers: { get: (name) => headers[String(name).toLowerCase()] ?? null },
  text: async () => (typeof body === 'string' ? body : JSON.stringify(body)),
});

/** Http layer with a scripted fetch: router(url, opts) decides every reply. */
function makeHttp(router, session = memorySession()) {
  const calls = [];
  const fetch = async (url, opts = {}) => {
    const entry = { url: String(url), method: opts.method || 'GET', headers: opts.headers || {} };
    calls.push(entry);
    return router(entry, calls);
  };
  const context = vm.createContext({
    window: {}, fetch, AbortController, AbortSignal,
    URLSearchParams, setTimeout, clearTimeout, sessionStorage: session,
  });
  vm.runInContext(source('http.js'), context);
  return { Http: context.window.EditorCore.Http, calls, session };
}

const csrfOk = (token) => (entry) => (entry.url.includes('action=csrf')
  ? reply({ ok: true, csrf: token })
  : reply({ ok: true }));

test('csrf: first POST to the studio API prefetches a token and sends the header', async () => {
  const { Http, calls, session } = makeHttp(csrfOk(TOKEN_A));
  const res = await Http.json('api/change-requests.php?action=submit', { method: 'POST', body: { x: 1 } });
  assert.equal(res.ok, true);
  assert.equal(calls.length, 2);
  assert.equal(calls[0].url, 'api/auth.php?action=csrf');
  assert.equal(calls[0].method, 'GET');
  assert.equal(calls[1].method, 'POST');
  assert.equal(calls[1].headers['X-Editor-CSRF'], TOKEN_A);
  assert.equal(session.getItem('editorStudio.csrf'), TOKEN_A);

  // Cached: the next POST does not prefetch again.
  await Http.json('api/change-requests.php?action=submit', { method: 'POST', body: { x: 2 } });
  assert.equal(calls.length, 3);
  assert.equal(calls[2].headers['X-Editor-CSRF'], TOKEN_A);
});

test('csrf: a token stored in sessionStorage is reused without prefetch', async () => {
  const session = memorySession({ 'editorStudio.csrf': TOKEN_B });
  const { Http, calls } = makeHttp(() => reply({ ok: true }), session);
  await Http.json('api/db-crud.php?action=create', { method: 'POST', body: {} });
  assert.equal(calls.length, 1);
  assert.equal(calls[0].headers['X-Editor-CSRF'], TOKEN_B);
});

test('csrf: auth responses rotate the stored token', async () => {
  const session = memorySession({ 'editorStudio.csrf': TOKEN_A });
  const { Http, session: stored } = makeHttp(() => reply({ ok: true, csrf: TOKEN_B }), session);
  await Http.json('api/auth.php?action=login', { method: 'POST', body: { user: 'admin' } });
  assert.equal(Http.csrf.get(), TOKEN_B);
  assert.equal(stored.getItem('editorStudio.csrf'), TOKEN_B);
});

test('csrf: a stale token is refreshed and the request retried exactly once', async () => {
  let posts = 0;
  const session = memorySession({ 'editorStudio.csrf': TOKEN_A });
  const { Http, calls } = makeHttp((entry) => {
    if (entry.url.includes('action=csrf')) return reply({ ok: true, csrf: TOKEN_C });
    posts += 1;
    return posts === 1
      ? reply({ ok: false, code: 'csrf_token', error: 'stale' }, 403)
      : reply({ ok: true });
  }, session);
  const res = await Http.json('api/guests.php?action=create', { method: 'POST', body: {} });
  assert.equal(res.ok, true);
  const postCalls = calls.filter((c) => c.method === 'POST');
  assert.equal(postCalls.length, 2);
  assert.equal(postCalls[0].headers['X-Editor-CSRF'], TOKEN_A);
  assert.equal(postCalls[1].headers['X-Editor-CSRF'], TOKEN_C);
});

test('csrf: persistent 403 does not loop', async () => {
  const session = memorySession({ 'editorStudio.csrf': TOKEN_A });
  const { Http, calls } = makeHttp((entry) => (entry.url.includes('action=csrf')
    ? reply({ ok: true, csrf: TOKEN_C })
    : reply({ ok: false, code: 'csrf_token', error: 'stale' }, 403)), session);
  const res = await Http.json('api/guests.php?action=create', { method: 'POST', body: {} });
  assert.equal(res.ok, false);
  assert.equal(res.status, 403);
  assert.equal(calls.filter((c) => c.method === 'POST').length, 2);
});

test('csrf: GET requests and foreign URLs never prefetch or send the header', async () => {
  const { Http, calls } = makeHttp(() => reply({ ok: true }));
  await Http.json('api/db-crud.php?action=list');
  assert.equal(calls.length, 1);
  assert.equal(calls[0].headers['X-Editor-CSRF'], undefined);

  await Http.json('https://example.test/api', { method: 'POST', body: {} });
  assert.equal(calls.length, 2);
  assert.equal(calls[1].headers['X-Editor-CSRF'], undefined);
});

test('csrf: token validation and manual controls', async () => {
  const { Http, session } = makeHttp(() => reply({ ok: true }));
  Http.csrf.set('not-a-token');
  assert.equal(Http.csrf.get(), '');
  Http.csrf.set('d'.repeat(64));
  assert.equal(Http.csrf.get(), 'd'.repeat(64));
  assert.equal(session.getItem('editorStudio.csrf'), 'd'.repeat(64));
  Http.csrf.clear();
  assert.equal(Http.csrf.get(), '');
  assert.equal(session.getItem('editorStudio.csrf'), null);
});

test('retry-after: header is parsed, body retry_after wins', async () => {
  const session = memorySession({ 'editorStudio.csrf': TOKEN_A });
  const { Http } = makeHttp((entry, calls) => (calls.length === 1
    ? reply({ ok: false, error: 'slow down' }, 429, { 'retry-after': '30' })
    : reply({ ok: false, error: 'slow down', retry_after: 45 }, 429, { 'retry-after': '30' })), session);
  const first = await Http.json('api/change-requests.php?action=submit', { method: 'POST', body: {} });
  assert.equal(first.retryAfter, 30);
  const second = await Http.json('api/change-requests.php?action=submit', { method: 'POST', body: {} });
  assert.equal(second.retryAfter, 45);
});
