(() => {
  // ─────────────────────────────────────────────────────────────────────────
  // Guest mode for Editor Studio.
  //
  // A guest works in the same editors as an administrator, but every
  // create/update/delete is captured into a local DRAFT (localStorage) instead
  // of being sent to the database. Reads are overlaid with the draft so the
  // guest sees its own changes. The draft is then sent as a change request
  // (nickname + description) that an administrator reviews and applies.
  //
  // NOTE: this file is only UX. Security is enforced on the server:
  //   api/db-crud.php       — guest: read-only list/get
  //   api/change-requests.php — guest: preview/submit/mine/get/withdraw
  //   every other endpoint  — admin only
  // ─────────────────────────────────────────────────────────────────────────
  if (window.EditorCore?.Guest) return;

  const DRAFT_KEY = 'editorStudio.guestDraft.v1';
  const ROLE_CACHE_KEY = 'editorStudio.role';
  const GUEST_KEY = 'editorStudio.guestKey';
  const TEMP_ID_BASE = 2000000000;
  const TEMP_ID_MAX = 2000999999;
  const REQUESTS_PAGE = 'change-requests.html';

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

  // ── Role ────────────────────────────────────────────────────────────────
  const fetchStatus = async () => {
    try {
      const res = await fetch('api/auth.php?action=status', { credentials: 'same-origin', cache: 'no-store' });
      const json = await res.json().catch(() => null);
      if (!json?.ok) return { authorized: false, role: null };
      return json;
    } catch {
      return { authorized: false, role: null };
    }
  };

  const ready = () => {
    if (!statusPromise) {
      statusPromise = fetchStatus().then((s) => {
        status = s;
        try { safeSession?.setItem(ROLE_CACHE_KEY, s?.role || ''); } catch {}
        return s?.role || null;
      });
    }
    return statusPromise;
  };

  const refreshStatus = () => { statusPromise = null; return ready(); };
  const cachedRole = () => status?.role ?? (safeSession?.getItem(ROLE_CACHE_KEY) || null);
  const isGuest = () => cachedRole() === 'guest';

  const loadConfig = async () => {
    if (!configPromise) {
      configPromise = fetch('api/change-requests.php?action=config', { credentials: 'same-origin', cache: 'no-store' })
        .then(r => r.json())
        .then((j) => { if (j?.ok) config = j; return config; })
        .catch(() => null);
    }
    return configPromise;
  };

  // ── Draft storage ───────────────────────────────────────────────────────
  const emptyDraft = () => ({ v: 1, nextTemp: TEMP_ID_BASE + 1, ops: [] });

  const readDraft = () => {
    try {
      const raw = safeLocal?.getItem(DRAFT_KEY);
      if (!raw) return emptyDraft();
      const d = JSON.parse(raw);
      if (!d || !Array.isArray(d.ops)) return emptyDraft();
      d.nextTemp = Number(d.nextTemp) || TEMP_ID_BASE + 1;
      return d;
    } catch {
      return emptyDraft();
    }
  };

  const notify = () => {
    const d = readDraft();
    listeners.forEach((fn) => { try { fn(d); } catch {} });
    updateBanner();
    try { window.parent?.postMessage({ type: 'editor-shell:guest-draft-changed', count: d.ops.length }, '*'); } catch {}
  };

  const writeDraft = (d) => {
    try { safeLocal?.setItem(DRAFT_KEY, JSON.stringify(d)); }
    catch { throw new Error('Не удалось сохранить черновик (переполнено хранилище браузера).'); }
    notify();
  };

  window.addEventListener('storage', (e) => { if (e.key === DRAFT_KEY) notify(); });

  const isTempId = (id) => {
    const n = Number(id);
    return Number.isInteger(n) && n >= TEMP_ID_BASE && n <= TEMP_ID_MAX;
  };

  const clone = (v) => (v == null ? v : JSON.parse(JSON.stringify(v)));

  const canonical = (v) => {
    if (v === undefined || v === null) return null;
    if (typeof v === 'boolean') return v ? '1' : '0';
    if (typeof v === 'object') return JSON.stringify(v);
    const s = String(v);
    if (/^-?\d+(\.\d+)?$/.test(s)) return String(Number(s));
    const t = s.trim();
    if (t.startsWith('{') || t.startsWith('[')) {
      try { return JSON.stringify(JSON.parse(t)); } catch {}
    }
    return s;
  };
  const sameValue = (a, b) => canonical(a) === canonical(b);

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

  const uid = () => `${Date.now().toString(36)}${Math.random().toString(36).slice(2, 8)}`;

  const findIndex = (d, pred) => d.ops.findIndex(pred);

  const assertGuestResource = async (resource) => {
    await loadConfig();
    if (config?.resources && !config.resources[resource]) {
      throw new Error('Этот раздел недоступен для запросов на изменение.');
    }
  };

  const toastDraft = (text) => {
    try { window.EditorCore?.UI?.toast?.(text, 'info'); } catch {}
  };

  const Draft = {
    get: readDraft,
    count: () => readDraft().ops.length,
    clear() { writeDraft(emptyDraft()); },
    removeOp(opUid) {
      const d = readDraft();
      d.ops = d.ops.filter(o => o.uid !== opUid);
      writeDraft(d);
    },

    async create(resource, data) {
      await assertGuestResource(resource);
      const d = readDraft();
      const tempId = d.nextTemp++;
      if (tempId > TEMP_ID_MAX) throw new Error('Слишком много новых записей в черновике.');
      const clean = filterColumns(resource, clone(data));
      d.ops.push({ uid: uid(), type: 'create', resource, tempId, data: clean, label: rowLabel(resource, clean), ts: Date.now() });
      writeDraft(d);
      toastDraft(`Новая запись (${resourceTitle(resource)}) добавлена в черновик запроса`);
      return { ok: true, id: tempId, draft: true };
    },

    /** baseRow — current server row (to keep only really changed fields). */
    async update(resource, id, data, baseRow = null) {
      await assertGuestResource(resource);
      const d = readDraft();
      const clean = filterColumns(resource, clone(data));

      if (isTempId(id)) {
        const i = findIndex(d, o => o.type === 'create' && o.resource === resource && Number(o.tempId) === Number(id));
        if (i < 0) throw new Error('Новая запись не найдена в черновике (возможно, черновик был очищен).');
        d.ops[i].data = { ...d.ops[i].data, ...clean };
        d.ops[i].label = rowLabel(resource, d.ops[i].data);
        d.ops[i].ts = Date.now();
        writeDraft(d);
        toastDraft('Новая запись в черновике обновлена');
        return { ok: true, draft: true };
      }

      const nid = Number(id);
      if (d.ops.some(o => o.type === 'delete' && o.resource === resource && Number(o.id) === nid)) {
        throw new Error('Эта запись уже помечена на удаление в черновике. Уберите удаление в «Запросах», чтобы изменить её.');
      }
      const i = findIndex(d, o => o.type === 'update' && o.resource === resource && Number(o.id) === nid);
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
        toastDraft('Значения совпадают с текущими — изменение убрано из черновика');
        return { ok: true, draft: true, noop: true };
      }

      const label = rowLabel(resource, { ...(baseRow || {}), ...merged }) || rowLabel(resource, baseRow);
      if (i >= 0) {
        d.ops[i].data = merged;
        d.ops[i].label = label;
        d.ops[i].ts = Date.now();
      } else {
        d.ops.push({ uid: uid(), type: 'update', resource, id: nid, data: merged, label, ts: Date.now() });
      }
      writeDraft(d);
      toastDraft(`Изменение (${resourceTitle(resource)} #${nid}) добавлено в черновик запроса`);
      return { ok: true, draft: true };
    },

    async remove(resource, id, baseRow = null) {
      await assertGuestResource(resource);
      const d = readDraft();
      if (isTempId(id)) {
        d.ops = d.ops.filter(o => !(o.type === 'create' && o.resource === resource && Number(o.tempId) === Number(id)));
        writeDraft(d);
        toastDraft('Новая запись убрана из черновика');
        return { ok: true, draft: true };
      }
      const nid = Number(id);
      d.ops = d.ops.filter(o => !(o.resource === resource && Number(o.id) === nid));
      d.ops.push({ uid: uid(), type: 'delete', resource, id: nid, data: {}, label: rowLabel(resource, baseRow), ts: Date.now() });
      writeDraft(d);
      toastDraft(`Удаление (${resourceTitle(resource)} #${nid}) добавлено в черновик запроса`);
      return { ok: true, draft: true };
    },

    // ── Read overlays ─────────────────────────────────────────────────────
    overlayRows(resource, rows, { search = '', offset = 0 } = {}) {
      const d = readDraft();
      const ops = d.ops.filter(o => o.resource === resource);
      if (!ops.length) return rows;
      const deleted = new Set(ops.filter(o => o.type === 'delete').map(o => Number(o.id)));
      const updates = new Map(ops.filter(o => o.type === 'update').map(o => [Number(o.id), o.data]));
      let out = (rows || [])
        .filter(r => !deleted.has(Number(r?.ID)))
        .map(r => (updates.has(Number(r?.ID)) ? { ...r, ...clone(updates.get(Number(r.ID))) } : r));
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

    /** Payload for api/change-requests.php */
    toOperations() {
      return readDraft().ops.map(o => (o.type === 'create'
        ? { type: 'create', resource: o.resource, tempId: o.tempId, data: o.data }
        : o.type === 'update'
          ? { type: 'update', resource: o.resource, id: o.id, data: o.data }
          : { type: 'delete', resource: o.resource, id: o.id }));
    },

    onChange(fn) { listeners.add(fn); return () => listeners.delete(fn); },
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
      throw new Error('Некорректное новое имя группы. Не используйте : или |; максимум 256 символов.');
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
      throw new Error('Такое имя уже занято на этом уровне. Проверьте предупреждение и повторите.');
    }
    for (const u of updates) {
      await Draft.update('crafts', u.ID, { GroupName: u.newGroupName }, isTempId(u.ID) ? null : u.row);
    }
    return { ok: true, changed: updates.map(u => ({ ID: u.ID, newGroupName: u.newGroupName })) };
  };

  // ── Page access guard + banner ──────────────────────────────────────────
  const currentPage = () => {
    const file = (location.pathname.split('/').pop() || 'index.html').toLowerCase();
    return file || 'index.html';
  };

  const guestAllowedPages = () => {
    const groups = Array.isArray(window.STUDIO_MANIFEST) ? window.STUDIO_MANIFEST : [];
    const pages = new Set(['index.html', REQUESTS_PAGE]);
    groups.forEach(g => (g.items || []).forEach((it) => { if (it.guest) pages.add(String(it.src).toLowerCase()); }));
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
        <h2 style="font-size:20px;font-weight:700;margin-bottom:8px">Доступ запрещён</h2>
        <p style="color:var(--editor-text-muted,#9aa8bc);font-size:14px;line-height:1.5">
          Этот раздел недоступен для гостевого аккаунта. Гость может просматривать редакторы контента
          и предлагать изменения через запросы — их проверяет администратор.
        </p>
        <a href="${REQUESTS_PAGE}" class="editor-btn editor-btn-primary" style="margin-top:16px;display:inline-flex">
          <i class="fa-solid fa-code-pull-request"></i><span>Мои запросы</span>
        </a>
      </div>`;
    const mount = () => {
      document.body.appendChild(box);
      // Hide the actual page content so nothing is interactable.
      Array.from(document.body.children).forEach((el) => { if (el !== box) el.setAttribute('inert', ''); });
    };
    if (document.body) mount(); else document.addEventListener('DOMContentLoaded', mount, { once: true });
  };

  let bannerEl = null;
  const updateBanner = () => {
    if (!bannerEl) return;
    const n = Draft.count();
    const countEl = bannerEl.querySelector('[data-guest-count]');
    if (countEl) countEl.textContent = String(n);
    bannerEl.dataset.empty = n ? '0' : '1';
  };

  const openRequestsPage = () => {
    const inFrame = window.parent && window.parent !== window;
    if (inFrame) window.parent.postMessage({ type: 'editor-shell:open-editor', key: 'requests' }, '*');
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
      .guest-banner .guest-count{min-width:22px;padding:1px 7px;border-radius:999px;background:var(--editor-warning,#ffc27f);color:#1b1206;font-weight:700;text-align:center}
      .guest-banner[data-empty="1"] .guest-count{background:var(--editor-soft-bg-strong,rgba(255,255,255,.08));color:var(--editor-text-muted,#9aa8bc)}
      .guest-banner .editor-btn{height:32px;padding:0 10px}
      @media (max-width:640px){.guest-banner .guest-text{display:none}}`;
    document.head.appendChild(style);
    bannerEl = document.createElement('div');
    bannerEl.className = 'guest-banner';
    bannerEl.innerHTML = `
      <i class="fa-solid fa-user-secret" style="color:var(--editor-warning,#ffc27f)"></i>
      <span class="guest-text"><b>Гостевой режим</b> — изменения попадают в черновик запроса</span>
      <span class="guest-count" data-guest-count title="Изменений в черновике">0</span>
      <button type="button" class="editor-btn editor-btn-primary" data-guest-open><i class="fa-solid fa-paper-plane"></i><span>Запрос</span></button>`;
    bannerEl.querySelector('[data-guest-open]').addEventListener('click', openRequestsPage);
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
    const run = () => mountBanner();
    if (document.body) run(); else document.addEventListener('DOMContentLoaded', run, { once: true });
  };

  // Guest identity key (lets the guest see its previous requests on this browser).
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
    cachedRole,
    loadConfig,
    config: () => config,
    isTempId,
    Draft,
    renameCraftGroup,
    isPageAllowedForGuest,
    guestAllowedPages,
    openRequestsPage,
    GuestKey,
    resourceTitle,
  };

  if (currentPage() !== 'index.html') initPage();
})();
