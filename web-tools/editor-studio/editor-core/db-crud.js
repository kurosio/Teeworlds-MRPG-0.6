(() => {
  // Generic CRUD client for DB-backed editors.
  // Requires api/db-crud.php on server.
  const API = 'api/db-crud.php';
  const FETCH_TIMEOUT_MS = 10000;
  const LOG_ERRORS = false;
  const LIST_CACHE_TTL_MS = 30 * 1000;
  const listCache = new Map();
  const inflight = new Map();

  const normalizeError = (err) => {
    if (err?.name === 'AbortError') return new Error('Request timed out');
    if (err instanceof TypeError) return new Error('Network error');
    if (err instanceof Error) return err;
    return new Error('Network error');
  };

  const notifyAuthRequired = () => {
    try {
      window.top?.postMessage({ type: 'editor-shell:auth-required' }, '*');
    } catch {}
  };

  const jsonFetch = async (url, options = {}) => {
    const timeoutController = new AbortController();
    const externalSignal = options.signal;
    const signal = (!externalSignal)
      ? timeoutController.signal
      : (typeof AbortSignal !== 'undefined' && typeof AbortSignal.any === 'function')
        ? AbortSignal.any([timeoutController.signal, externalSignal])
        : (() => {
            const fallback = new AbortController();
            const onAbort = () => fallback.abort();
            if (externalSignal.aborted) fallback.abort();
            else externalSignal.addEventListener('abort', onAbort, { once: true });
            timeoutController.signal.addEventListener('abort', onAbort, { once: true });
            return fallback.signal;
          })();
    const timeoutId = setTimeout(() => timeoutController.abort(), FETCH_TIMEOUT_MS);
    try {
      const res = await fetch(url, {
        credentials: 'same-origin',
        headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
        ...options,
        signal,
      });
      const text = await res.text();
      let payload = null;
      try { payload = text ? JSON.parse(text) : null; } catch { payload = { ok: false, error: 'Invalid JSON response', raw: text }; }
      if (res.status === 401) {
        notifyAuthRequired();
      }
      if (!res.ok) throw new Error(payload?.error || `HTTP ${res.status}`);
      if (payload && payload.ok === false) throw new Error(payload.error || 'Request failed');
      return payload;
    } catch (err) {
      if (LOG_ERRORS) console.warn('[db-crud] fetch failed', err);
      throw normalizeError(err);
    } finally {
      clearTimeout(timeoutId);
    }
  };

  const qs = (obj) => Object.entries(obj)
    .filter(([, v]) => v !== undefined && v !== null)
    .map(([k, v]) => `${encodeURIComponent(k)}=${encodeURIComponent(String(v))}`)
    .join('&');

  const DBCrud = {
    async list(resource, { search = '', limit = 100, offset = 0, signal } = {}) {
      const key = `list:${resource}:${search}:${limit}:${offset}`;
      const now = Date.now();
      const cached = listCache.get(key);
      if (cached && (now - cached.ts) < LIST_CACHE_TTL_MS) {
        return cached.value;
      }
      if (!signal && inflight.has(key)) {
        return inflight.get(key);
      }
      const req = jsonFetch(`${API}?${qs({ action: 'list', resource, search, limit, offset })}`, { signal })
        .then((res) => {
          if (res?.ok) listCache.set(key, { ts: Date.now(), value: res });
          return res;
        })
        .finally(() => inflight.delete(key));
      if (!signal) inflight.set(key, req);
      return req;
    },
    async get(resource, id) {
      return jsonFetch(`${API}?${qs({ action: 'get', resource, id })}`);
    },
    async create(resource, data) {
      listCache.clear();
      return jsonFetch(`${API}?${qs({ action: 'create', resource })}`, { method: 'POST', body: JSON.stringify({ data }) });
    },
    async update(resource, id, data) {
      listCache.clear();
      return jsonFetch(`${API}?${qs({ action: 'update', resource, id })}`, { method: 'POST', body: JSON.stringify({ data }) });
    },
    async remove(resource, id) {
      listCache.clear();
      return jsonFetch(`${API}?${qs({ action: 'delete', resource, id })}`, { method: 'POST', body: JSON.stringify({}) });
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DBCrud = DBCrud;
})();
