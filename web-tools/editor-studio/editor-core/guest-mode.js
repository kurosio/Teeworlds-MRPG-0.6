(() => {
  // ─────────────────────────────────────────────────────────────────────────
  // Guest accounts for Editor Studio (client side).
  //
  // Guests are personal accounts created by the administrator (see
  // api/guests.php). Each account has its own password and its own set of
  // allowed editor tabs. A guest works in the same editors as an administrator,
  // but every create/update/delete is captured into a LOCAL draft instead of
  // being sent to the database.
  //
  // Two kinds of draft operations are kept locally:
  //   * draft       (no reqId) — prepared, not sent yet;
  //   * submitted   (reqId)    — sent as a change request and waiting for review.
  // Both are overlaid on top of the DB data, so a record edited by the guest is
  // never shown in its original form: the guest sees its own values, marked as
  // "draft" or "in request CR-…". When the administrator approves or rejects the
  // request, the submitted operations are reconciled away (the DB already has
  // them / they never happened).
  //
  // NOTE: this file is UX only. Security is enforced on the server:
  //   api/db-crud.php          — guest: read-only, only resources of allowed tabs
  //   api/change-requests.php  — guest: preview/submit/mine/get/withdraw only
  //   api/guests.php           — guest accounts management (admin only)
  //   every other endpoint     — admin only
  // ─────────────────────────────────────────────────────────────────────────
  if (window.EditorCore?.Guest) return;

  const DRAFT_KEY_PREFIX = 'editorStudio.guestDraft.v2';
  const LEGACY_DRAFT_KEY = 'editorStudio.guestDraft.v1';
  const ROLE_CACHE_KEY = 'editorStudio.role';
  const ACCOUNT_CACHE_KEY = 'editorStudio.guestAccount';
  const GUEST_KEY = 'editorStudio.guestKey';
  const DRAFT_SCHEMA = 2;
  const TEMP_ID_BASE = 2000000000;
  const TEMP_ID_MAX = 2000999999;
  const REQUESTS_PAGE = 'change-requests.html';
  const SYNC_MIN_INTERVAL = 15000;

  const safeLocal = (() => {
    try { const k = '__es_guest__'; localStorage.setItem(k, '1'); localStorage.removeItem(k); return localStorage; } catch { return null; }
  })();
  const safeSession = (() => {
    try { const k = '__es_guest__'; sessionStorage.setItem(k, '1'); sessionStorage.removeItem(k); return sessionStorage; } catch { return null; }
  })();

  const listeners = new Set();
  let statusPromise = null;
  let status = null;
  let configPromise = null;
  let config = null;
  let syncPromise = null;
  let lastSyncAt = 0;
  let reconcileTimer = null;
  let draftCache = null;
  let draftCacheKey = '';
  // Raw JSON the cached snapshot was parsed from. The same draft is written by
  // several documents of one tab (the Studio shell and the editor it embeds),
  // so a snapshot cannot be trusted just because the account key matches.
  let draftCacheRaw = '';

  // ── Role / account ──────────────────────────────────────────────────────
  const fetchStatus = async () => {
    try {
      const res = await window.EditorCore.Http.json('api/auth.php?action=status');
      if (!res.ok) return { authorized: false, role: null };
      return res.data || { authorized: false, role: null };
    } catch {
      return { authorized: false, role: null };
    }
  };

  const ready = () => {
    if (!statusPromise) {
      statusPromise = fetchStatus().then((s) => {
        status = s;
        try {
          safeSession?.setItem(ROLE_CACHE_KEY, s?.role || '');
          if (s?.role === 'guest' && s.guest?.id) {
            safeSession?.setItem(ACCOUNT_CACHE_KEY, JSON.stringify({
              id: s.guest.id,
              login: s.guest.login || '',
              name: s.guest.name || s.guest.login || '',
              pages: Array.isArray(s.guest.pages) ? s.guest.pages : [],
            }));
          } else {
            safeSession?.removeItem(ACCOUNT_CACHE_KEY);
          }
        } catch {}
        // Do not reuse another account's in-memory draft after login/logout.
        if (draftCacheKey && draftCacheKey !== draftKey()) {
          draftCache = null;
          draftCacheKey = '';
          draftCacheRaw = '';
        }
        if (s?.role === 'guest' && s.guest?.id) {
          startAutoReconcile();
          scheduleSubmittedSync(0);
        }
        return s?.role || null;
      });
    }
    return statusPromise;
  };

  const refreshStatus = () => { statusPromise = null; return ready(); };
  const cachedRole = () => status?.role ?? (safeSession?.getItem(ROLE_CACHE_KEY) || null);
  const isGuest = () => cachedRole() === 'guest';
  const isAdmin = () => cachedRole() === 'admin';
  /** Current guest account. Falls back to the session cache while a refresh is in flight. */
  const account = () => {
    if (status?.role === 'guest') return status.guest || null;
    if (cachedRole() !== 'guest') return null;
    try {
      const cached = JSON.parse(safeSession?.getItem(ACCOUNT_CACHE_KEY) || 'null');
      return cached && cached.id ? cached : null;
    } catch {
      return null;
    }
  };
  const accountId = () => String(account()?.id || '');
  const accountPages = () => (Array.isArray(account()?.pages) ? account().pages.map(String) : []);
  const accountLabel = () => account()?.name || account()?.login || 'Guest';

  const loadConfig = async (force = false) => {
    if (force) { configPromise = null; config = null; }
    if (!configPromise) {
      configPromise = window.EditorCore.Http.json('api/change-requests.php?action=config')
        .then((res) => { if (res.ok) config = res.data; return config; });
    }
    return configPromise;
  };

  // ── Draft storage (per guest account) ───────────────────────────────────
  const emptyDraft = () => ({ v: DRAFT_SCHEMA, accountId: accountId(), nextTemp: TEMP_ID_BASE + 1, ops: [] });
  const draftKey = () => {
    const id = accountId();
    if (!id || !isGuest()) return '';
    return `${DRAFT_KEY_PREFIX}:${id}`;
  };

  const normalizeOp = (o) => {
    if (!o || typeof o !== 'object') return null;
    if (!['create', 'update', 'delete'].includes(o.type)) return null;
    if (typeof o.resource !== 'string' || !o.resource) return null;
    const op = {
      uid: typeof o.uid === 'string' && o.uid ? o.uid : uid(),
      type: o.type,
      resource: o.resource,
      id: o.type === 'create' ? undefined : Number(o.id),
      tempId: o.type === 'create' ? Number(o.tempId) : undefined,
      data: o.data && typeof o.data === 'object' ? clone(o.data) : {},
      label: typeof o.label === 'string' ? o.label : '',
      ts: Number(o.ts) || Date.now(),
      reqId: typeof o.reqId === 'string' && o.reqId ? o.reqId : null,
      reqVersion: Math.max(1, Number(o.reqVersion) || 1),
      reqDirty: !!o.reqDirty,
      submittedAt: o.submittedAt ? String(o.submittedAt) : null,
    };
    if (op.type !== 'create' && (!Number.isInteger(op.id) || op.id <= 0)) return null;
    if (op.type === 'create' && !isTempId(op.tempId)) return null;
    return op;
  };

  const readDraft = () => {
    const key = draftKey();
    if (!key) return emptyDraft();
    const cached = () => (draftCache && draftCacheKey === key ? draftCache : emptyDraft());

    let raw;
    try {
      raw = safeLocal ? safeLocal.getItem(key) : null;
    } catch {
      // A transient storage error must not cache an empty draft over good data.
      return cached();
    }
    // The draft is written by every studio document of the tab (the shell, the
    // embedded editor, another tab). Trust the snapshot only while the stored
    // value is the very same string it was parsed from — otherwise a count or
    // an overlay would keep showing values that were already deleted/edited.
    if (draftCache && draftCacheKey === key && draftCacheRaw === (raw || '')) return draftCache;

    let draft;
    if (raw) {
      try {
        const parsed = JSON.parse(raw);
        draft = parsed && Array.isArray(parsed.ops) ? {
          v: DRAFT_SCHEMA,
          accountId: accountId(),
          nextTemp: Number(parsed.nextTemp) || TEMP_ID_BASE + 1,
          ops: parsed.ops.map(normalizeOp).filter(Boolean),
        } : emptyDraft();
      } catch {
        return cached();
      }
    } else {
      draft = migrateLegacyDraft(key);
      // The migration writes the draft; keep the stamp in sync with what is
      // actually stored so the next read does not parse it again.
      try { raw = safeLocal ? safeLocal.getItem(key) : null; } catch { raw = null; }
    }
    draftCache = draft;
    draftCacheKey = key;
    draftCacheRaw = raw || '';
    return draft;
  };

  /** One-time move of the pre-account draft (v1) to the current guest account. */
  const migrateLegacyDraft = (key) => {
    try {
      const raw = safeLocal?.getItem(LEGACY_DRAFT_KEY);
      if (!raw) return emptyDraft();
      const legacy = JSON.parse(raw);
      if (!legacy || !Array.isArray(legacy.ops)) return emptyDraft();
      const ops = legacy.ops.map(normalizeOp).filter(Boolean);
      const draft = {
        v: DRAFT_SCHEMA,
        accountId: accountId(),
        nextTemp: Number(legacy.nextTemp) || TEMP_ID_BASE + 1,
        ops,
      };
      safeLocal?.setItem(key, JSON.stringify(draft));
      safeLocal?.removeItem(LEGACY_DRAFT_KEY);
      return draft;
    } catch {
      return emptyDraft();
    }
  };

  const emitUpdated = () => {
    try { window.dispatchEvent(new CustomEvent('editor-core:guest-updated', { detail: summary() })); } catch {}
    window.EditorCore.Http.postMessage(window.parent, {
      type: 'editor-shell:guest-draft-changed',
      accountId: accountId(),
      count: Draft.count(),
      pending: Draft.pendingCount(),
      submitted: Draft.submittedCount(),
    });
  };

  const notify = () => {
    const d = clone(readDraft());
    listeners.forEach((fn) => { try { fn(d); } catch {} });
    updateBanner();
    emitUpdated();
  };

  const writeDraft = (d) => {
    const key = draftKey();
    if (!key) throw new Error('Guest account is not defined — reload the page and sign in again.');
    // Silent data loss is the worst option: if the browser blocks localStorage the
    // guest must be told instead of believing the change was stored.
    if (!safeLocal) {
      throw new Error('The browser blocks local storage (localStorage) — changes cannot be saved. Allow data storage for this site and reload the page.');
    }
    const raw = JSON.stringify(d);
    try { safeLocal.setItem(key, raw); }
    catch { throw new Error('Could not save the draft (browser storage is full).'); }
    // Keep the cached snapshot isolated from objects returned to callers, and
    // remember the exact stored string so readDraft() can detect foreign writes.
    draftCache = clone(d);
    draftCacheKey = key;
    draftCacheRaw = raw;
    notify();
  };

  window.addEventListener('storage', (e) => {
    const key = draftKey();
    // e.key === null means the whole storage was cleared by another document.
    if (key && (e.key === key || e.key === null)) {
      draftCache = null;
      draftCacheKey = '';
      draftCacheRaw = '';
      notify();
    }
  });

  const isTempId = (id) => {
    const n = Number(id);
    return Number.isInteger(n) && n >= TEMP_ID_BASE && n <= TEMP_ID_MAX;
  };

  const clone = (value) => window.EditorCore.utils.cloneData(value);
  // Mutations work on a copy: a failed localStorage write must not leak into
  // the in-memory snapshot and masquerade as a saved draft.
  const draftForWrite = () => clone(readDraft());

  // NULL, '' и 0 в редакторах — одно и то же «нет значения»: форма не умеет их
  // различать, поэтому сравнение с базовой строкой не должно записывать такие
  // пары как изменение (см. EditorCore.utils.valuesEqual).
  const sameValue = (a, b) => window.EditorCore.utils.valuesEqual(a, b);

  /** Keep only whitelisted columns (from server config) — never send unknown fields. */
  const filterColumns = (resource, data) => {
    const cols = config?.resources?.[resource]?.columns;
    const out = {};
    for (const [k, v] of Object.entries(data || {})) {
      if (k === 'ID' || k.startsWith('__')) continue;
      if (Array.isArray(cols) && !cols.includes(k)) continue;
      out[k] = v;
    }
    return out;
  };

  const resourceTitle = (resource) => config?.resources?.[resource]?.title || resource;
  const rowLabel = (resource, row) => {
    const col = config?.resources?.[resource]?.label;
    const v = col && row ? String(row[col] ?? '').trim() : '';
    return v || '';
  };

  const uid = () => window.EditorCore.utils.uuid();

  /** Компактный отпечаток текста (не криптография — только сравнение составов). */
  const hashText = (text) => {
    let hash = 5381;
    const value = String(text || '');
    for (let i = 0; i < value.length; i++) hash = ((hash << 5) + hash + value.charCodeAt(i)) | 0;
    return (hash >>> 0).toString(16).padStart(8, '0');
  };

  const assertGuestResource = async (resource) => {
    await loadConfig();
    if (config?.resources && !config.resources[resource]) {
      throw new Error('This section is not enabled for your guest account.');
    }
  };

  const toastInfo = (text, tone = 'info') => {
    try { window.EditorCore?.UI?.toast?.(text, tone); } catch {}
  };

  const opRowKey = (resource, id) => `${resource}:${Number(id)}`;
  const hasSubmitted = (d, resource, id) => d.ops.some(o => o.reqId && o.resource === resource && Number(o.id) === Number(id));
  const submittedReqIds = (ops) => [...new Set(ops.filter(o => o.reqId).map(o => o.reqId))];

  const Draft = {
    // External consumers receive a snapshot and cannot mutate the cached draft
    // without going through writeDraft().
    get: () => clone(readDraft()),

    /**
     * Forget the in-memory snapshot; the next call re-reads the stored draft.
     * readDraft() already compares the stored value with the cached one, so this
     * is only needed by callers that want to be explicit (e.g. the shell after
     * an editor frame reported a change).
     */
    invalidate() {
      draftCache = null;
      draftCacheKey = '';
      draftCacheRaw = '';
    },

    /** Refresh from storage and return a snapshot. */
    reload() {
      Draft.invalidate();
      return clone(readDraft());
    },

    count: () => readDraft().ops.length,
    pendingCount: () => readDraft().ops.filter(o => !o.reqId).length,
    submittedCount: () => readDraft().ops.filter(o => o.reqId).length,
    submittedRequests: () => submittedReqIds(readDraft().ops),

    /** Remove not-yet-sent operations (sent ones can only be withdrawn by the admin/guest request page). */
    clear() {
      const d = draftForWrite();
      const before = d.ops.length;
      d.ops = d.ops.filter(o => o.reqId);
      writeDraft(d);
      if (before !== d.ops.length) toastInfo('Unsent changes removed from the draft');
      return d.ops.length;
    },

    removeOp(opUid) {
      const d = draftForWrite();
      const op = d.ops.find(o => o.uid === opUid);
      if (op?.reqId) throw new Error('This change is already submitted in a request and awaits admin review.');
      d.ops = d.ops.filter(o => o.uid !== opUid);
      writeDraft(d);
      return true;
    },

    async create(resource, data) {
      await assertGuestResource(resource);
      const d = draftForWrite();
      const tempId = d.nextTemp++;
      if (tempId > TEMP_ID_MAX) throw new Error('Too many new records in the draft.');
      const clean = filterColumns(resource, clone(data));
      d.ops.push({ uid: uid(), type: 'create', resource, tempId, data: clean, label: rowLabel(resource, clean), ts: Date.now(), reqId: null });
      writeDraft(d);
      toastInfo(`New record (${resourceTitle(resource)}) added to the request draft`);
      return { ok: true, id: tempId, draft: true };
    },

    /** baseRow — current server row (to keep only really changed fields). */
    async update(resource, id, data, baseRow = null) {
      await assertGuestResource(resource);
      const d = draftForWrite();
      const clean = filterColumns(resource, clone(data));

      if (isTempId(id)) {
        const op = d.ops.find(o => o.type === 'create' && o.resource === resource && Number(o.tempId) === Number(id));
        if (!op) throw new Error('The new record was not found in the draft (the draft may have been cleared).');
        if (op.reqId) throw new Error('The new record is already submitted in a request and awaits admin review.');
        op.data = { ...op.data, ...clean };
        op.label = rowLabel(resource, op.data);
        op.ts = Date.now();
        writeDraft(d);
        toastInfo('New record updated in the draft');
        return { ok: true, draft: true };
      }

      const nid = Number(id);
      if (hasSubmitted(d, resource, nid)) {
        throw new Error('This record is already submitted in a request — it cannot be edited right now.');
      }
      if (d.ops.some(o => o.type === 'delete' && o.resource === resource && Number(o.id) === nid)) {
        throw new Error('This record is already marked for deletion in the draft. Remove the deletion in “Requests” to edit it.');
      }
      const i = d.ops.findIndex(o => o.type === 'update' && o.resource === resource && Number(o.id) === nid);
      const merged = i >= 0 ? { ...d.ops[i].data, ...clean } : clean;

      // Drop fields equal to the server value: the request lists only real changes.
      if (baseRow) {
        for (const k of Object.keys(merged)) {
          if (k in baseRow && sameValue(baseRow[k], merged[k])) delete merged[k];
        }
      }

      if (!Object.keys(merged).length) {
        if (i >= 0) d.ops.splice(i, 1);
        writeDraft(d);
        toastInfo('The values match the current ones — the change was removed from the draft');
        return { ok: true, draft: true, noop: true };
      }

      const label = rowLabel(resource, { ...(baseRow || {}), ...merged }) || rowLabel(resource, baseRow);
      if (i >= 0) {
        d.ops[i].data = merged;
        d.ops[i].label = label;
        d.ops[i].ts = Date.now();
      } else {
        d.ops.push({ uid: uid(), type: 'update', resource, id: nid, data: merged, label, ts: Date.now(), reqId: null });
      }
      writeDraft(d);
      toastInfo(`Update (${resourceTitle(resource)} #${nid}) added to the request draft`);
      return { ok: true, draft: true };
    },

    /**
     * Убрать одно поле из черновика: значение вернётся к тому, что в базе.
     * baseRow — текущая строка из БД, нужна только чтобы пересчитать подпись
     * операции (например, если отменено изменение `Name`).
     * Возвращает { ok, removed, dropped }: dropped — операция исчезла целиком,
     * потому что в ней не осталось изменённых полей.
     */
    async revertField(resource, id, column, baseRow = null) {
      await assertGuestResource(resource);
      const col = String(column || '').trim();
      if (!col) throw new Error('No field to restore was specified.');
      const d = draftForWrite();
      const nid = Number(id);
      const i = d.ops.findIndex(o => o.type === 'update' && o.resource === resource && Number(o.id) === nid);
      // Записи нет в черновике — отменять нечего: это не ошибка.
      if (i < 0) return { ok: true, removed: false, dropped: false };
      const op = d.ops[i];
      if (op.reqId) {
        throw new Error('The change is already in a request awaiting review — this field cannot be reverted. Withdraw the request.');
      }
      if (!op.data || !(col in op.data)) return { ok: true, removed: false, dropped: false };
      delete op.data[col];
      let dropped = false;
      if (!Object.keys(op.data).length) {
        d.ops.splice(i, 1);
        dropped = true;
      } else {
        op.label = (baseRow ? rowLabel(resource, { ...baseRow, ...op.data }) : '') || op.label || '';
        op.ts = Date.now();
      }
      writeDraft(d);
      toastInfo(dropped
        ? 'Change removed from the draft — the record matches the database'
        : `Field ${col}  restored to the database value`);
      return { ok: true, removed: true, dropped };
    },

    async remove(resource, id, baseRow = null) {
      await assertGuestResource(resource);
      const d = draftForWrite();
      if (isTempId(id)) {
        const op = d.ops.find(o => o.type === 'create' && o.resource === resource && Number(o.tempId) === Number(id));
        if (op?.reqId) throw new Error('The new record is already submitted in a request and awaits admin review.');
        d.ops = d.ops.filter(o => !(o.type === 'create' && o.resource === resource && Number(o.tempId) === Number(id)));
        writeDraft(d);
        toastInfo('New record removed from the draft');
        return { ok: true, draft: true };
      }
      const nid = Number(id);
      if (hasSubmitted(d, resource, nid)) {
        throw new Error('This record is already submitted in a request — it cannot be deleted right now.');
      }
      d.ops = d.ops.filter(o => !(o.resource === resource && Number(o.id) === nid));
      d.ops.push({ uid: uid(), type: 'delete', resource, id: nid, data: {}, label: rowLabel(resource, baseRow), ts: Date.now(), reqId: null });
      writeDraft(d);
      toastInfo(`Delete (${resourceTitle(resource)} #${nid}) added to the request draft`);
      return { ok: true, draft: true };
    },

    // ── Read overlays ─────────────────────────────────────────────────────
    overlayRows(resource, rows, { search = '', offset = 0 } = {}) {
      const d = readDraft();
      const ops = d.ops.filter(o => o.resource === resource);
      if (!ops.length) return rows;
      const deleted = new Set(ops.filter(o => o.type === 'delete').map(o => Number(o.id)));
      const updates = new Map(ops.filter(o => o.type === 'update').map(o => [Number(o.id), o]));
      let out = (rows || [])
        .filter(r => !deleted.has(Number(r?.ID)))
        .map((r) => {
          const up = updates.get(Number(r?.ID));
          return up ? { ...r, ...clone(up.data) } : r;
        });
      if (!offset) {
        const q = String(search || '').trim().toLowerCase();
        const created = ops
          .filter(o => o.type === 'create')
          .map(o => ({ ID: o.tempId, ...clone(o.data) }))
          .filter(r => !q || Object.values(r).some(v => String(typeof v === 'object' ? JSON.stringify(v) : v ?? '').toLowerCase().includes(q)));
        out = out.concat(created);
      }
      return out;
    },

    overlayRow(resource, id, row) {
      const d = readDraft();
      if (isTempId(id)) {
        const op = d.ops.find(o => o.type === 'create' && o.resource === resource && Number(o.tempId) === Number(id));
        return op ? { ID: op.tempId, ...clone(op.data) } : null;
      }
      const nid = Number(id);
      if (d.ops.some(o => o.type === 'delete' && o.resource === resource && Number(o.id) === nid)) return null;
      const up = d.ops.find(o => o.type === 'update' && o.resource === resource && Number(o.id) === nid);
      return row && up ? { ...row, ...clone(up.data) } : row;
    },

    /** 'create' | 'update' | 'delete' | null */
    rowState(resource, id) {
      const d = readDraft();
      const op = isTempId(id)
        ? d.ops.find(o => o.type === 'create' && o.resource === resource && Number(o.tempId) === Number(id))
        : d.ops.find(o => o.resource === resource && Number(o.id) === Number(id));
      return op ? op.type : null;
    },

    /**
     * Local state of a row for badges:
     * { state, status: 'draft'|'submitted', reqId, fields: [columns] } or null.
     */
    rowMeta(resource, id) {
      const d = readDraft();
      const op = isTempId(id)
        ? d.ops.find(o => o.type === 'create' && o.resource === resource && Number(o.tempId) === Number(id))
        : d.ops.find(o => o.resource === resource && Number(o.id) === Number(id));
      if (!op) return null;
      return {
        state: op.type,
        status: op.reqId ? 'submitted' : 'draft',
        reqId: op.reqId || null,
        fields: op.type === 'delete' ? [] : Object.keys(op.data || {}),
      };
    },

    /** Column-level markers for the currently selected record. */
    pendingFields(resource, id) {
      const meta = Draft.rowMeta(resource, id);
      if (!meta) return {};
      const d = readDraft();
      const op = isTempId(id)
        ? d.ops.find(o => o.type === 'create' && o.resource === resource && Number(o.tempId) === Number(id))
        : d.ops.find(o => o.resource === resource && Number(o.id) === Number(id));
      const out = {};
      for (const col of meta.fields) {
        out[col] = { value: clone(op?.data?.[col]), status: meta.status, reqId: meta.reqId };
      }
      return out;
    },

    /** Payload for api/change-requests.php — only operations that were not sent yet. */
    toOperations() {
      return clone(readDraft().ops.filter(o => !o.reqId).map(o => (o.type === 'create'
        ? { type: 'create', resource: o.resource, tempId: o.tempId, data: o.data }
        : o.type === 'update'
          ? { type: 'update', resource: o.resource, id: o.id, data: o.data }
          : { type: 'delete', resource: o.resource, id: o.id })));
    },

    /** Отпечаток неотправленной части черновика: меняется при любой правке. */
    fingerprint() {
      const pending = readDraft().ops.filter(o => !o.reqId);
      return hashText(JSON.stringify(pending.map(o => [o.uid, o.type, o.resource, o.id || o.tempId, o.data])));
    },

    /**
     * Ключ идемпотентности для отправки текущего состава черновика.
     * Стабилен между попытками отправки одного и того же состава (двойной
     * клик, потерянный ответ, повтор после кулдауна) — сервер вернёт уже
     * созданный запрос вместо дубликата. Любая правка черновика меняет ключ.
     */
    submitKey() {
      const storeKey = `editorStudio.guestSubmitKey:${accountId()}`;
      const fingerprint = Draft.fingerprint();
      try {
        const saved = JSON.parse(safeLocal?.getItem(storeKey) || 'null');
        if (saved?.fingerprint === fingerprint && /^[A-Za-z0-9-]{16,80}$/.test(saved.key || '')) return saved.key;
      } catch { /* повреждённое значение — выпустим новый ключ */ }
      const key = uid();
      try { safeLocal?.setItem(storeKey, JSON.stringify({ key, fingerprint })); } catch { /* ключ всё равно вернём */ }
      return key;
    },

    clearSubmitKey() {
      try { safeLocal?.removeItem(`editorStudio.guestSubmitKey:${accountId()}`); } catch { /* ignore */ }
    },

    /**
     * Черновик одним файлом: резервная копия неотправленных правок.
     * Уже отправленные операции не включаются — они принадлежат запросам
     * на сервере и восстанавливаются входом в тот же аккаунт.
     */
    exportDraft() {
      const pending = readDraft().ops.filter(o => !o.reqId);
      return JSON.stringify({
        app: 'editor-studio-guest-draft',
        version: 1,
        exportedAt: new Date().toISOString(),
        account: accountLabel(),
        ops: pending.map(o => ({
          type: o.type, resource: o.resource,
          id: o.type === 'create' ? undefined : o.id,
          tempId: o.type === 'create' ? o.tempId : undefined,
          data: clone(o.data), label: o.label || '',
        })),
      }, null, 2);
    },

    /**
     * Импорт черновика из exportDraft(). Операции проверяются так же, как
     * при чтении из хранилища, разделы вне доступа аккаунта пропускаются,
     * временные ID при коллизиях перевыпускаются (ссылки внутри импортируемых
     * данных переписываются), строки, уже занятые черновиком, не затираются.
     * @returns {Promise<{imported:number, skipped:number}>}
     */
    async importDraft(text) {
      let parsed;
      try { parsed = JSON.parse(String(text || '')); }
      catch { throw new Error('The file is not valid JSON.'); }
      if (!parsed || parsed.app !== 'editor-studio-guest-draft' || !Array.isArray(parsed.ops)) {
        throw new Error('This file is not an Editor Studio guest draft.');
      }
      await loadConfig();
      const d = draftForWrite();
      const existingKeys = new Set(d.ops.map(o => `${o.type}:${o.resource}:${o.type === 'create' ? o.tempId : o.id}`));
      const busyRows = new Set(d.ops.filter(o => o.type !== 'create').map(o => `${o.resource}:${o.id}`));
      const usedTemps = new Set(d.ops.filter(o => o.type === 'create').map(o => Number(o.tempId)));

      const incoming = [];
      let skipped = 0;
      for (const raw of parsed.ops.slice(0, 500)) {
        const op = normalizeOp(raw && typeof raw === 'object' ? { ...raw, uid: '', reqId: null } : raw);
        if (!op) { skipped++; continue; }
        if (config?.resources && !config.resources[op.resource]) { skipped++; continue; }
        op.reqId = null;
        op.reqDirty = false;
        op.submittedAt = null;
        incoming.push(op);
      }

      let nextTemp = Number(d.nextTemp) || TEMP_ID_BASE + 1;
      const allocTemp = () => {
        while (usedTemps.has(nextTemp)) nextTemp++;
        const allocated = nextTemp++;
        if (allocated > TEMP_ID_MAX) throw new Error('Too many new records in the draft.');
        usedTemps.add(allocated);
        return allocated;
      };
      const remap = new Map();
      const seenCreates = new Set();
      for (const op of incoming) {
        if (op.type !== 'create') continue;
        const original = Number(op.tempId);
        if (usedTemps.has(original) || seenCreates.has(original)) {
          const fresh = allocTemp();
          if (!remap.has(original)) remap.set(original, fresh);
          op.tempId = fresh;
        } else {
          usedTemps.add(original);
        }
        seenCreates.add(original);
      }
      const remapValue = (value) => {
        if (typeof value === 'number' && remap.has(value)) return remap.get(value);
        if (Array.isArray(value)) return value.map(remapValue);
        if (value && typeof value === 'object') {
          const out = {};
          for (const [k, v] of Object.entries(value)) out[k] = remapValue(v);
          return out;
        }
        if (typeof value === 'string' && remap.size) {
          return value.replace(/200\d{7}/g, (m) => (remap.has(Number(m)) ? String(remap.get(Number(m))) : m));
        }
        return value;
      };

      let imported = 0;
      for (const op of incoming) {
        op.data = remapValue(op.data);
        const key = `${op.type}:${op.resource}:${op.type === 'create' ? op.tempId : op.id}`;
        if (existingKeys.has(key) || (op.type !== 'create' && busyRows.has(`${op.resource}:${op.id}`))) {
          skipped++;
          continue;
        }
        d.ops.push(op);
        existingKeys.add(key);
        imported++;
      }
      d.nextTemp = Math.max(nextTemp, TEMP_ID_BASE + 1,
        ...d.ops.filter(o => o.type === 'create').map(o => Number(o.tempId) + 1));
      if (imported) writeDraft(d);
      return { imported, skipped };
    },

    /** Replace one pending request after a server-validated revision. */
    replaceRequest(request) {
      const requestId = String(request?.id || '');
      if (!requestId || !Array.isArray(request?.operations)) return false;
      const d = draftForWrite();
      const previous = new Map(d.ops.filter(o => o.reqId === requestId)
        .map(o => [`${o.type}:${o.resource}:${o.type === 'create' ? o.tempId : o.id}`, o]));
      const now = new Date().toISOString();
      d.ops = d.ops.filter(o => o.reqId !== requestId);
      for (const op of request.operations) {
        const key = `${op.type}:${op.resource}:${op.type === 'create' ? op.tempId : op.id}`;
        const old = previous.get(key);
        const data = clone(op.data || {});
        d.ops.push({
          uid: old?.uid || uid(), type: op.type, resource: op.resource,
          id: op.type === 'create' ? undefined : Number(op.id),
          tempId: op.type === 'create' ? Number(op.tempId) : undefined,
          data,
          label: rowLabel(op.resource, op.type === 'delete' ? (op.before || data) : data) || old?.label || '',
          ts: old?.ts || Date.now(), reqId: requestId,
          reqVersion: Math.max(1, Number(request.version) || 1), reqDirty: false,
          submittedAt: old?.submittedAt || now,
        });
      }
      d.nextTemp = Math.max(Number(d.nextTemp) || TEMP_ID_BASE + 1,
        ...d.ops.filter(o => o.type === 'create').map(o => Number(o.tempId) + 1));
      writeDraft(d);
      return true;
    },

    /** Freeze the current draft as a sent request: the changes stay visible locally. */
    markSubmitted(requestId) {
      const id = String(requestId || '');
      if (!id) return 0;
      const d = draftForWrite();
      const now = new Date().toISOString();
      let n = 0;
      for (const op of d.ops) {
        if (!op.reqId) {
          op.reqId = id;
          op.reqVersion = 1;
          op.reqDirty = false;
          op.submittedAt = now;
          n++;
        }
      }
      writeDraft(d);
      Draft.clearSubmitKey();
      return n;
    },

    /**
     * Reconcile locally tracked requests with their server status.
     * Approved/rejected/withdrawn/deleted requests drop their local operations:
     * after approval the DB already contains the values, after rejection the
     * original values must be shown again.
     */
    async syncSubmitted({ silent = true } = {}) {
      if (syncPromise) return syncPromise;
      if (!isGuest()) return { ok: false, resolved: [] };
      const ids = submittedReqIds(readDraft().ops);
      if (!ids.length) return { ok: true, resolved: [] };

      syncPromise = (async () => {
        const mine = await window.EditorCore.Http.json('api/change-requests.php?action=mine');
        if (!mine.ok) return { ok: false, resolved: [] };
        const rows = Array.isArray(mine.rows) ? mine.rows : [];
        const byId = new Map(rows.map(r => [String(r.id), r]));
        const resolved = [];
        const revised = [];

        // An administrator (or the other open tab) may have revised a pending
        // request. Refresh only when the server version is newer: ordinary
        // polling must not re-download every request or overwrite a stale
        // workbench implicitly.
        const localVersions = new Map();
        for (const op of readDraft().ops) {
          if (op.reqId) localVersions.set(op.reqId, Math.max(localVersions.get(op.reqId) || 1, Number(op.reqVersion) || 1));
        }
        for (const row of rows) {
          const requestId = String(row.id || '');
          if (row.status !== 'pending' || Number(row.version) <= (localVersions.get(requestId) || 1)) continue;
          const localOps = readDraft().ops.filter(op => op.reqId === requestId);
          if (localOps.some(op => op.reqDirty)) continue;
          try {
            const detail = await window.EditorCore.Http.json(`api/change-requests.php?action=get&id=${encodeURIComponent(requestId)}&peek=1`);
            if (!detail.ok || !detail.request || Number(detail.request.version) <= (localVersions.get(requestId) || 1)) continue;
            Draft.replaceRequest(detail.request);
            revised.push({ reqId: requestId, version: Number(detail.request.version) || 1 });
          } catch { /* The next poll retries; the current local snapshot stays intact. */ }
        }

        const d = draftForWrite();
        const keep = [];
        for (const op of d.ops) {
          if (!op.reqId) { keep.push(op); continue; }
          const req = byId.get(op.reqId);
          if (req && req.status === 'pending') { keep.push(op); continue; }
          resolved.push({ reqId: op.reqId, status: req?.status || 'deleted', op });
        }
        if (revised.length) {
          try { window.EditorCore?.DBCrud?.invalidateCache?.(); } catch {}
          if (!silent) toastInfo(`Request contents updated (${revised.length})`, 'info');
          for (const item of revised) {
            try { window.dispatchEvent(new CustomEvent('editor-core:guest-request-revised', { detail: item })); } catch {}
          }
        }
        if (!resolved.length) return { ok: true, resolved: [], revised };

        d.ops = keep;
        writeDraft(d);
        // Server data changed (approved) or must be shown again (rejected) → drop list caches.
        try { window.EditorCore?.DBCrud?.invalidateCache?.(); } catch {}
        try { configPromise = null; } catch {}

        if (resolved.length) {
          const approved = resolved.filter(r => r.status === 'approved').length;
          const rejected = resolved.filter(r => r.status !== 'approved' && r.status !== 'pending').length;
          if (!silent) {
            if (approved) toastInfo(`Request approved: changes applied to the database (${approved})`, 'success');
            if (rejected) toastInfo(`Request rejected: original data restored (${rejected})`, 'warning');
          }
          for (const r of resolved) {
            try { window.dispatchEvent(new CustomEvent('editor-core:guest-request-resolved', { detail: r })); } catch {}
          }
        }
        return { ok: true, resolved, revised };
      })().finally(() => { syncPromise = null; lastSyncAt = Date.now(); });

      return syncPromise;
    },

    onChange(fn) { listeners.add(fn); return () => listeners.delete(fn); },
  };

  const summary = () => ({
    account: accountLabel(),
    total: Draft.count(),
    pending: Draft.pendingCount(),
    submitted: Draft.submittedCount(),
  });

  const scheduleSubmittedSync = (delay = 0) => {
    clearTimeout(reconcileTimer);
    reconcileTimer = setTimeout(() => {
      if (Date.now() - lastSyncAt < SYNC_MIN_INTERVAL) return;
      Draft.syncSubmitted({ silent: true }).catch(() => {});
    }, Math.max(0, delay));
  };

  const startAutoReconcile = () => {
    if (startAutoReconcile.started) return;
    startAutoReconcile.started = true;
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState !== 'visible' || !isGuest()) return;
      scheduleSubmittedSync(0);
      if (Date.now() - lastSyncAt < SYNC_MIN_INTERVAL) return;
      refreshStatus().then((role) => {
        if (role !== 'guest' && role !== 'admin') {
          // Account disabled/revoked → ask the shell to re-authenticate.
          window.EditorCore.Http.postMessage(window.parent, { type: 'editor-shell:auth-required' });
          return;
        }
        if (role === 'guest' && !isPageAllowedForGuest()) {
          // The administrator removed this tab while the page was open.
          showAccessDenied();
        }
      }).catch(() => {});
    }, { passive: true });
  };

  // ── Crafts group rename (port of the server logic, produces updates) ────
  const parseCraftGroup = (value) => {
    const raw = String(value || '').trim();
    let main = '';
    let nested = raw;
    if (raw.startsWith('|')) {
      const end = raw.indexOf('|', 1);
      if (end >= 0) { main = raw.slice(1, end).trim(); nested = raw.slice(end + 1); } else { nested = raw.slice(1); }
    }
    const sep = nested.indexOf(':');
    if (sep < 0) return { main, group: nested.trim(), sub: '' };
    return { main, group: nested.slice(0, sep).trim(), sub: nested.slice(sep + 1).trim() };
  };
  const joinCraftGroup = (main, group, sub) => {
    main = String(main || '').trim(); group = String(group || '').trim(); sub = String(sub || '').trim();
    const nested = group && sub ? `${group}:${sub}` : (group || sub);
    if (!main) return nested;
    return nested ? `|${main}|${nested}` : `|${main}|`;
  };

  const renameCraftGroup = async (rows, target, newName, mergeExisting) => {
    const level = String(target?.level || '');
    const mainGroup = String(target?.mainGroup || '').trim();
    const group = String(target?.group || '').trim();
    const subGroup = String(target?.subGroup || '').trim();
    const name = String(newName || '').trim();
    const current = level === 'main' ? mainGroup : level === 'group' ? group : subGroup;
    if (!['main', 'group', 'sub'].includes(level) || !current) throw new Error('Invalid craft group target');
    if (!name || name.includes(':') || name.includes('|') || name.length > 256) {
      throw new Error('Invalid new group name. Do not use : or |; 256 characters maximum.');
    }
    if (name === current) return { ok: true, changed: [] };
    const parentMatches = p => level === 'main' || (p.main === mainGroup && (level === 'group' || p.group === group));
    const matches = p => {
      if (level === 'main') return p.main === mainGroup;
      if (p.main !== mainGroup || p.group !== group) return false;
      return level === 'group' || p.sub === subGroup;
    };
    const levelName = p => (level === 'main' ? p.main : level === 'group' ? p.group : p.sub);
    const updates = [];
    let merged = false;
    for (const row of rows) {
      const p = parseCraftGroup(row.GroupName);
      if (!parentMatches(p)) continue;
      if (levelName(p) === name) merged = true;
      if (!matches(p)) continue;
      const renamed = level === 'main' ? joinCraftGroup(name, p.group, p.sub)
        : level === 'group' ? joinCraftGroup(p.main, name, p.sub) : joinCraftGroup(p.main, p.group, name);
      updates.push({ ID: row.ID, row, newGroupName: renamed });
    }
    if (updates.length && merged && mergeExisting !== true) {
      throw new Error('This name is already taken at this level. Check the warning and try again.');
    }
    for (const u of updates) {
      await Draft.update('crafts', u.ID, { GroupName: u.newGroupName }, isTempId(u.ID) ? null : u.row);
    }
    return { ok: true, changed: updates.map(u => ({ ID: u.ID, newGroupName: u.newGroupName })) };
  };

  // ── Page access guard (server-driven per-account tabs) ──────────────────
  const currentPage = () => {
    const file = (location.pathname.split('/').pop() || 'index.html').toLowerCase();
    return file || 'index.html';
  };

  const manifestItems = () => {
    const groups = Array.isArray(window.STUDIO_MANIFEST) ? window.STUDIO_MANIFEST : [];
    return groups.flatMap(g => (g.items || []));
  };

  /** Allowed page files: the tabs granted by the administrator + the requests page. */
  const guestAllowedPages = () => {
    const pages = new Set(['index.html', REQUESTS_PAGE]);
    const granted = accountPages();
    manifestItems().forEach((it) => {
      if (granted.includes(String(it.key))) pages.add(String(it.src).toLowerCase());
    });
    return pages;
  };

  // Embedded helpers opened from an allowed editor (e.g. the scenario editor overlay for ScenarioData fields)
  // only hand data back to the parent editor via postMessage; their own file storage (api.php) stays admin-only.
  const EMBED_HELPER_PAGES = new Set(['scenario-editor.html']);
  const isEmbeddedHelper = (page) => {
    try {
      return EMBED_HELPER_PAGES.has(page)
        && new URLSearchParams(location.search).get('embed') === '1'
        && window.parent && window.parent !== window;
    } catch { return false; }
  };

  const isPageAllowedForGuest = (page = currentPage()) => guestAllowedPages().has(page) || isEmbeddedHelper(page);

  const showAccessDenied = () => {
    if (document.getElementById('guest-access-denied')) return;
    const box = document.createElement('div');
    box.id = 'guest-access-denied';
    box.setAttribute('role', 'alert');
    box.style.cssText = 'position:fixed;inset:0;z-index:2147483000;display:flex;align-items:center;justify-content:center;padding:24px;background:var(--editor-bg,#0b0f14);color:var(--editor-text,#edf3fb);font-family:var(--editor-font,sans-serif)';
    box.innerHTML = `
      <div style="max-width:460px;text-align:center;border:1px solid var(--editor-border,#273142);border-radius:18px;padding:28px;background:var(--editor-surface,#121821)">
        <div style="font-size:40px;color:var(--editor-danger,#ff7c87);margin-bottom:10px"><i class="fa-solid fa-lock"></i></div>
        <h2 style="font-size:20px;font-weight:700;margin-bottom:8px">Tab disabled</h2>
        <p style="color:var(--editor-text-muted,#9aa8bc);font-size:14px;line-height:1.5">
          The administrator has not enabled this editor for the guest account <b>${escapeHtml(accountLabel())}</b>. Available tabs are listed in “My requests”.
        </p>
        <a href="${REQUESTS_PAGE}" class="editor-btn editor-btn-primary" style="margin-top:16px;display:inline-flex">
          <i class="fa-solid fa-code-pull-request"></i><span>My requests</span>
        </a>
      </div>`;
    const mount = () => {
      document.body.appendChild(box);
      // Hide the actual page content so nothing is interactable.
      Array.from(document.body.children).forEach((el) => { if (el !== box) el.setAttribute('inert', ''); });
    };
    if (document.body) mount(); else document.addEventListener('DOMContentLoaded', mount, { once: true });
  };

  const escapeHtml = (value) => window.EditorCore.utils.escapeHtml(value);

  let bannerEl = null;
  const updateBanner = () => {
    if (!bannerEl) return;
    const pending = Draft.pendingCount();
    const submitted = Draft.submittedCount();
    const total = pending + submitted;
    const countEl = bannerEl.querySelector('[data-guest-count]');
    if (countEl) countEl.textContent = String(total);
    const pendingEl = bannerEl.querySelector('[data-guest-pending]');
    if (pendingEl) pendingEl.textContent = String(pending);
    const submittedEl = bannerEl.querySelector('[data-guest-submitted]');
    if (submittedEl) submittedEl.textContent = String(submitted);
    bannerEl.dataset.empty = total ? '0' : '1';

    // Одна понятная фраза вместо трёх чисел: новичок должен сразу понимать,
    // ждут ли его правки проверки и что нажать.
    const stateEl = bannerEl.querySelector('[data-guest-state]');
    if (stateEl) {
      if (!total) stateEl.textContent = 'No changes';
      else if (pending && submitted) stateEl.textContent = `${pending}  unsent,  ${submitted}  under review`;
      else if (pending) stateEl.textContent = `${pending}  awaiting submission`;
      else stateEl.textContent = `${submitted}  awaiting admin review`;
    }
    const pendingPill = bannerEl.querySelector('[data-guest-pending-pill]');
    if (pendingPill) pendingPill.hidden = !pending;
    const submittedPill = bannerEl.querySelector('[data-guest-submitted-pill]');
    if (submittedPill) submittedPill.hidden = !submitted;
    const sendBtn = bannerEl.querySelector('[data-guest-open] span');
    if (sendBtn) sendBtn.textContent = submitted && !pending ? 'Under review' : 'My changes';
  };

  const openRequestsPage = () => {
    const inFrame = window.parent && window.parent !== window;
    if (inFrame) window.EditorCore.Http.postMessage(window.parent, { type: 'editor-shell:open-editor', key: 'requests' });
    else location.href = REQUESTS_PAGE;
  };

  const mountBanner = () => {
    if (bannerEl || currentPage() === REQUESTS_PAGE || currentPage() === 'index.html') return;
    const style = document.createElement('style');
    style.textContent = `
      .guest-banner{position:fixed;right:16px;bottom:16px;z-index:9000;display:flex;align-items:center;gap:10px;padding:8px 8px 8px 14px;border-radius:14px;
        background:var(--editor-toolbar-bg,rgba(13,18,26,.92));border:1px solid color-mix(in srgb,var(--editor-warning,#ffc27f) 45%,transparent);
        box-shadow:var(--editor-toolbar-shadow,0 20px 46px rgba(0,0,0,.34));backdrop-filter:blur(14px);font-size:13px;color:var(--editor-text,#edf3fb)}
      .guest-banner b{color:var(--editor-warning,#ffc27f)}
      .guest-banner .guest-state{display:block;font-size:11.5px;color:var(--editor-text-muted,#9aa8bc)}
      .guest-banner .guest-pill[hidden]{display:none}
      .guest-banner .guest-count{min-width:22px;padding:1px 7px;border-radius:999px;background:var(--editor-warning,#ffc27f);color:#1b1206;font-weight:700;text-align:center}
      .guest-banner .guest-pill{display:inline-flex;align-items:center;gap:4px;font-size:11.5px;color:var(--editor-text-muted,#9aa8bc)}
      .guest-banner .guest-pill b{color:var(--editor-text,#edf3fb)}
      .guest-banner[data-empty="1"] .guest-count{background:var(--editor-soft-bg-strong,rgba(255,255,255,.08));color:var(--editor-text-muted,#9aa8bc)}
      .guest-banner .editor-btn{height:32px;padding:0 10px}
      @media (max-width:720px){.guest-banner .guest-text{display:none}}`;
    document.head.appendChild(style);
    bannerEl = document.createElement('div');
    bannerEl.className = 'guest-banner';
    bannerEl.innerHTML = `
      <i class="fa-solid fa-user-secret" style="color:var(--editor-warning,#ffc27f)"></i>
      <span class="guest-text"><b>Guest:  ${escapeHtml(accountLabel())}</b>
        <span class="guest-state" data-guest-state>No changes</span>
      </span>
      <span class="guest-pill" title="Changed but not yet sent to the administrator" data-guest-pending-pill><i class="fa-solid fa-pen"></i><b data-guest-pending>0</b></span>
      <span class="guest-pill" title="Sent to the administrator and awaiting review" data-guest-submitted-pill><i class="fa-solid fa-hourglass-half"></i><b data-guest-submitted>0</b></span>
      <span class="guest-count" data-guest-count title="Total changes">0</span>
      <button type="button" class="editor-btn editor-btn-primary" data-guest-open><i class="fa-solid fa-paper-plane"></i><span>My changes</span></button>`;
    const openBtn = bannerEl.querySelector('[data-guest-open]');
    if (openBtn) openBtn.addEventListener('click', openRequestsPage);
    document.body.appendChild(bannerEl);
    updateBanner();
  };

  const initPage = async () => {
    const role = await ready();
    if (role !== 'guest') return;
    document.documentElement.classList.add('editor-guest-mode');
    loadConfig();
    if (!isPageAllowedForGuest()) {
      showAccessDenied();
      return;
    }
    if (isEmbeddedHelper(currentPage())) return;
    Draft.syncSubmitted({ silent: true }).catch(() => {});
    const run = () => mountBanner();
    if (document.body) run(); else document.addEventListener('DOMContentLoaded', run, { once: true });
    // Editors may have rendered before the role was known → let them refresh badges.
    try { window.dispatchEvent(new CustomEvent('editor-core:guest-ready', { detail: summary() })); } catch {}
  };

  // Guest identity key (deprecated: request ownership is defined by the
  // personal account id on the server; kept only for API compatibility).
  const GuestKey = {
    get: () => safeLocal?.getItem(GUEST_KEY) || '',
    set: (k) => { try { if (/^[a-f0-9]{48}$/.test(k)) safeLocal?.setItem(GUEST_KEY, k); } catch {} },
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.Guest = {
    TEMP_ID_BASE,
    TEMP_ID_MAX,
    REQUESTS_PAGE,
    ready,
    refreshStatus,
    status: () => status,
    isGuest,
    isAdmin,
    cachedRole,
    account,
    accountId,
    accountPages,
    accountLabel,
    loadConfig,
    config: () => config,
    isTempId,
    Draft,
    summary,
    syncSubmitted: (opts) => Draft.syncSubmitted(opts),
    renameCraftGroup,
    isPageAllowedForGuest,
    guestAllowedPages,
    openRequestsPage,
    GuestKey,
    resourceTitle,
  };

  if (currentPage() !== 'index.html') initPage();
})();
