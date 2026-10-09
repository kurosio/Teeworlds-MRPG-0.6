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
    // Flag to prevent double-recording when runtime already records
    _syncRecordEnabled: true,

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
    async renameCraftGroup(target, newGroup, mergeExisting = false) {
      listCache.clear();
      const res = await jsonFetch(`${API}?${qs({ action: 'rename_group', resource: 'crafts' })}`, {
        method: 'POST',
        body: JSON.stringify({ ...target, newGroup, mergeExisting }),
      });
      const changed = Array.isArray(res?.changed) ? res.changed : [];
      const sync = window.EditorCore?.Sync;
      if (DBCrud._syncRecordEnabled && sync && changed.length) {
        const operations = changed.map(row => ({
          id: row.ID,
          data: { GroupName: row.newGroupName },
          label: `Переименование группы #${row.ID}`,
        }));
        if (sync.recordMany) sync.recordMany('crafts', 'update', operations);
        else operations.forEach(operation => sync.record('crafts', 'update', operation));
        sync.updateAllBadges();
      }
      return res;
    },
    async get(resource, id) {
      return jsonFetch(`${API}?${qs({ action: 'get', resource, id })}`);
    },
    async create(resource, data, { _skipSyncRecord = false } = {}) {
      listCache.clear();
      
      // Validate inputs
      if (!resource || !data) {
        throw new Error('Resource and data are required for create');
      }
      
      const res = await jsonFetch(`${API}?${qs({ action: 'create', resource })}`, { method: 'POST', body: JSON.stringify({ data }) });
      
      // Auto-record for sync (if not explicitly skipped by the runtime which records itself)
      if (!_skipSyncRecord && DBCrud._syncRecordEnabled && window.EditorCore?.Sync && res?.id) {
        try {
          window.EditorCore.Sync.record(resource, 'create', {
            id: res.id,
            data: data,
            label: `Создание ${resource} #${res.id}`
          });
          window.EditorCore.Sync.updateAllBadges();
        } catch (err) {
          console.warn('[DBCrud] Failed to record create operation:', err.message);
        }
      }
      return res;
    },
    async update(resource, id, data, { _skipSyncRecord = false } = {}) {
      listCache.clear();
      
      // Validate inputs
      if (!resource || !id || !data) {
        throw new Error('Resource, id, and data are required for update');
      }
      
      const res = await jsonFetch(`${API}?${qs({ action: 'update', resource, id })}`, { method: 'POST', body: JSON.stringify({ data }) });
      
      // Auto-record for sync
      if (!_skipSyncRecord && DBCrud._syncRecordEnabled && window.EditorCore?.Sync) {
        try {
          window.EditorCore.Sync.record(resource, 'update', {
            id: id,
            data: data,
            label: `Обновление ${resource} #${id}`
          });
          window.EditorCore.Sync.updateAllBadges();
        } catch (err) {
          console.warn('[DBCrud] Failed to record update operation:', err.message);
        }
      }
      return res;
    },
    async remove(resource, id, { _skipSyncRecord = false } = {}) {
      listCache.clear();
      
      // Validate inputs
      if (!resource || !id) {
        throw new Error('Resource and id are required for remove');
      }
      
      const res = await jsonFetch(`${API}?${qs({ action: 'delete', resource, id })}`, { method: 'POST', body: JSON.stringify({}) });
      
      // Auto-record for sync
      if (!_skipSyncRecord && DBCrud._syncRecordEnabled && window.EditorCore?.Sync) {
        try {
          window.EditorCore.Sync.record(resource, 'delete', {
            id: id,
            label: `Удаление ${resource} #${id}`
          });
          window.EditorCore.Sync.updateAllBadges();
        } catch (err) {
          console.warn('[DBCrud] Failed to record delete operation:', err.message);
        }
      }
      return res;
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DBCrud = DBCrud;
})();
