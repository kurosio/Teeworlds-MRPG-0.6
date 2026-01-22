(() => {
  // Generic CRUD client for DB-backed editors.
  // Requires api/db-crud.php on server.
  const API = 'api/db-crud.php';

  const jsonFetch = async (url, options = {}) => {
    const res = await fetch(url, {
      credentials: 'same-origin',
      headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
      ...options
    });
    const text = await res.text();
    let payload = null;
    try { payload = text ? JSON.parse(text) : null; } catch { payload = { ok: false, error: 'Invalid JSON response', raw: text }; }
    if (!res.ok) throw new Error(payload?.error || `HTTP ${res.status}`);
    if (payload && payload.ok === false) throw new Error(payload.error || 'Request failed');
    return payload;
  };

  const qs = (obj) => Object.entries(obj)
    .filter(([, v]) => v !== undefined && v !== null)
    .map(([k, v]) => `${encodeURIComponent(k)}=${encodeURIComponent(String(v))}`)
    .join('&');

  const DBCrud = {
    async list(resource, { search = '', limit = 100, offset = 0 } = {}) {
      return jsonFetch(`${API}?${qs({ action: 'list', resource, search, limit, offset })}`);
    },
    async get(resource, id) {
      return jsonFetch(`${API}?${qs({ action: 'get', resource, id })}`);
    },
    async create(resource, data) {
      return jsonFetch(`${API}?${qs({ action: 'create', resource })}`, { method: 'POST', body: JSON.stringify({ data }) });
    },
    async update(resource, id, data) {
      return jsonFetch(`${API}?${qs({ action: 'update', resource, id })}`, { method: 'POST', body: JSON.stringify({ data }) });
    },
    async remove(resource, id) {
      return jsonFetch(`${API}?${qs({ action: 'delete', resource, id })}`, { method: 'POST', body: JSON.stringify({}) });
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DBCrud = DBCrud;
})();
