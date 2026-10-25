(() => {
  const ensureUI = () => {
    if (!window.EditorCore?.UI) {
      throw new Error('EditorCore.UI is not available. Load ui.js before utils.js.');
    }
  };

  const uuid = () => {
    if (typeof crypto.randomUUID === 'function') return crypto.randomUUID();
    const bytes = crypto.getRandomValues(new Uint8Array(16));
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    const hex = Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join('');
    return `${hex.slice(0, 8)}-${hex.slice(8, 12)}-${hex.slice(12, 16)}-${hex.slice(16, 20)}-${hex.slice(20)}`;
  };

  const debounce = (fn, delay = 200) => {
    let timer = null;
    function debounced(...args) {
      clearTimeout(timer);
      timer = setTimeout(() => {
        timer = null;
        fn.apply(this, args);
      }, delay);
    }
    debounced.cancel = () => {
      clearTimeout(timer);
      timer = null;
    };
    return debounced;
  };

  // Safe clone for editable snapshots. Keeps editor-specific code independent
  // from structuredClone support in older embedded webviews.
  const cloneData = (value) => {
    if (value === null || value === undefined) return value;
    if (typeof structuredClone === 'function') return structuredClone(value);
    try { return JSON.parse(JSON.stringify(value)); }
    catch { return value; }
  };

  // Shared path access for schema forms. Numeric segments create arrays and
  // incompatible containers are repaired instead of throwing mid-edit.
  const parsePath = (path) => String(path ?? '').split('.')
    .map(part => (/^\d+$/.test(part) ? Number(part) : part));

  const getAtPath = (data, path) => {
    if (!path) return undefined;
    return parsePath(path).reduce((current, key) => current == null ? undefined : current[key], data);
  };

  const setAtPath = (data, path, value) => {
    if (!path || data == null) return;
    const keys = parsePath(path);
    if (keys.some(key => key === '__proto__' || key === 'prototype' || key === 'constructor')) return;
    let current = data;
    for (let index = 0; index < keys.length; index++) {
      const key = keys[index];
      if (index === keys.length - 1) {
        current[key] = value;
        return;
      }
      const needsArray = typeof keys[index + 1] === 'number';
      if (current[key] == null || typeof current[key] !== 'object'
        || (needsArray && !Array.isArray(current[key]))) {
        current[key] = needsArray ? [] : {};
      }
      current = current[key];
    }
  };

  /**
   * Small bounded TTL cache with request de-duplication. clear() also advances
   * a generation, so an older request cannot repopulate stale data after a
   * mutation invalidated the cache.
   */
  const createRequestCache = ({ ttl = 30000, max = 100, now = () => Date.now() } = {}) => {
    const values = new Map();
    const pending = new Map();
    let generation = 0;

    const get = (key) => {
      const entry = values.get(key);
      if (!entry) return undefined;
      if (now() - entry.ts >= ttl) {
        values.delete(key);
        return undefined;
      }
      // Map iteration order doubles as an inexpensive LRU order.
      values.delete(key);
      values.set(key, entry);
      return entry.value;
    };

    const set = (key, value) => {
      values.delete(key);
      values.set(key, { ts: now(), value });
      while (values.size > max) values.delete(values.keys().next().value);
      return value;
    };

    const clear = () => {
      generation += 1;
      values.clear();
      pending.clear();
    };

    const load = async (key, loader, { cache = true, dedupe = true, shouldCache = () => true } = {}) => {
      if (cache) {
        const cached = get(key);
        if (cached !== undefined) return cached;
      }
      if (dedupe && pending.has(key)) return pending.get(key);

      const requestGeneration = generation;
      const promise = Promise.resolve().then(loader);
      if (dedupe) pending.set(key, promise);
      try {
        const value = await promise;
        if (cache && generation === requestGeneration && shouldCache(value)) set(key, value);
        return value;
      } finally {
        if (pending.get(key) === promise) pending.delete(key);
      }
    };

    return { get, set, clear, load, size: () => values.size };
  };

  const toFiniteNumber = (value, fallback = 0) => {
    const number = Number(value);
    return Number.isFinite(number) ? number : fallback;
  };

  const toNullableInt = (value, fallback = null) => {
    if (value === null || value === undefined || value === '') return fallback;
    const number = Number(value);
    return Number.isFinite(number) ? Math.trunc(number) : fallback;
  };

  /**
   * «Отсутствующее» значение в редакторах.
   *
   * Формы не различают NULL и пустое поле: `rowToModel` каждого редактора
   * приводит NULL к '' (текст) или 0 (число), а `modelToPayload` отправляет эти
   * значения обратно. Поэтому сравнение «было → станет» не может опираться на
   * точное равенство: иначе обычное открытие записи и нажатие «Сохранить»
   * записывает в запрос изменения вида `NULL → пусто` и `NULL → 0`, а возврат
   * поля к исходному значению не убирает операцию из черновика.
   *
   * Пустыми считаются только значения, которые действительно означают «нет
   * значения»: NULL/undefined, false, 0, пустая строка и пустые JSON-структуры.
   */
  const isEmptyValue = (value) => {
    if (value === null || value === undefined) return true;
    if (typeof value === 'boolean') return value === false;
    if (typeof value === 'number') return value === 0;
    if (typeof value === 'string') {
      const text = value.trim();
      if (text === '') return true;
      // Числовая строка ('0', '0.00', '-0') в числовом поле — тот же ноль.
      if (/^-?\d+(\.\d+)?$/.test(text)) return Number(text) === 0;
      // Пустая JSON-структура, пришедшая строкой из БД.
      if (text === '{}' || text === '[]') return true;
      return false;
    }
    if (Array.isArray(value)) return value.length === 0;
    if (typeof value === 'object') return Object.keys(value).length === 0;
    return false;
  };

  /**
   * Каноническое представление значения для сравнения: числа и числовые строки,
   * JSON и текст с одинаковым смыслом дают одинаковую строку.
   */
  const canonicalValue = (value) => {
    if (value === undefined || value === null) return null;
    if (typeof value === 'boolean') return value ? '1' : '0';
    if (typeof value === 'object') {
      try { return JSON.stringify(value); } catch { return String(value); }
    }
    const text = String(value);
    if (/^-?\d+(\.\d+)?$/.test(text)) return String(Number(text));
    const trimmed = text.trim();
    if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
      try { return JSON.stringify(JSON.parse(trimmed)); } catch { /* keep the raw value */ }
    }
    return text;
  };

  /** Равенство значений «было → станет»: NULL, '' и 0 означают одно и то же. */
  const valuesEqual = (a, b) => {
    if (isEmptyValue(a) && isEmptyValue(b)) return true;
    return canonicalValue(a) === canonicalValue(b);
  };

  const parseStringList = (value, { separator = ',', allowed = null, unique = false } = {}) => {
    const source = Array.isArray(value) ? value : String(value ?? '').split(separator);
    const canonical = Array.isArray(allowed) && allowed.length
      ? new Map(allowed.map(item => [String(item).toLowerCase(), String(item)]))
      : null;
    const seen = new Set();
    return source.reduce((result, item) => {
      let normalized = String(item ?? '').trim();
      if (!normalized) return result;
      if (canonical) normalized = canonical.get(normalized.toLowerCase()) || normalized;
      const key = normalized.toLowerCase();
      if (unique && seen.has(key)) return result;
      seen.add(key);
      result.push(normalized);
      return result;
    }, []);
  };

  const stringifyStringList = (value, { separator = ',', empty = null } = {}) => {
    const list = parseStringList(Array.isArray(value) ? value : [], { unique: false });
    return list.length ? list.join(separator) : empty;
  };

  const splitComposite = (value, separator = ':') => {
    const raw = String(value ?? '').trim();
    if (!raw) return { group: '', sub: '' };
    const index = raw.indexOf(separator);
    if (index < 0) return { group: raw, sub: '' };
    return {
      group: raw.slice(0, index).trim(),
      sub: raw.slice(index + separator.length).trim(),
    };
  };

  const joinComposite = (group, sub, { separator = ':', fallback = '' } = {}) => {
    const left = String(group ?? '').trim();
    const right = String(sub ?? '').trim();
    if (!left && !right) return fallback;
    return left && right ? `${left}${separator}${right}` : (left || right);
  };

  // GroupName hierarchy: |Main group|Group:Subgroup.
  // Old Group:Subgroup values intentionally remain unchanged.
  const splitGroupHierarchy = (value) => {
    const raw = String(value ?? '').trim();
    if (!raw.startsWith('|')) return { main: '', ...splitComposite(raw) };
    const end = raw.indexOf('|', 1);
    if (end < 0) return { main: '', ...splitComposite(raw) };
    const main = raw.slice(1, end).trim();
    const nested = splitComposite(raw.slice(end + 1));
    return { main, group: nested.group, sub: nested.sub };
  };

  const joinGroupHierarchy = (main, group, sub, { fallback = '' } = {}) => {
    const mainName = String(main ?? '').trim();
    const nested = joinComposite(group, sub);
    if (!mainName) return nested || fallback;
    if (!nested) return `|${mainName}|`;
    return `|${mainName}|${nested}`;
  };

  const STATUS_TONES = {
    muted: 'editor-status--muted', info: 'editor-status--muted',
    ok: 'editor-status--success', success: 'editor-status--success',
    err: 'editor-status--error', error: 'editor-status--error',
    warn: 'editor-status--warning', warning: 'editor-status--warning',
  };

  // One status renderer for every editor. HTML is opt-in because most status
  // messages contain API data and should be inserted as plain text.
  const setStatus = (element, message = '', tone = 'muted', options = {}) => {
    if (!element) return;
    const { html = false, className = '' } = options;
    element.className = ['editor-status', STATUS_TONES[tone] || STATUS_TONES.muted, className]
      .filter(Boolean).join(' ');
    element.setAttribute('role', tone === 'err' || tone === 'error' ? 'alert' : 'status');
    element.setAttribute('aria-live', tone === 'err' || tone === 'error' ? 'assertive' : 'polite');
    if (html) element.innerHTML = String(message || '');
    else element.textContent = String(message || '');
  };

  const escapeAttr = (value) => String(value).replace(/"/g, '&quot;');

  /** Строка запроса из объекта: пустые значения пропускаются. */
  const qs = (obj) => Object.entries(obj || {})
    .filter(([, value]) => value !== undefined && value !== null && value !== '')
    .map(([key, value]) => `${encodeURIComponent(key)}=${encodeURIComponent(String(value))}`)
    .join('&');

  const escapeHtml = (s) => String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');

  const cssEscape = (value) => {
    const raw = String(value ?? '');
    if (window.CSS?.escape) return window.CSS.escape(raw);
    // Conservative fallback for старых браузеров/webview без CSS.escape.
    return raw.replace(/["\\#.:;,[\](){}<>*+~|=^$`'\/\s]/g, '\\$&');
  };

  const rafThrottle = (fn) => {
    let ticking = false;
    let lastArgs = null;
    return (...args) => {
      lastArgs = args;
      if (ticking) return;
      ticking = true;
      requestAnimationFrame(() => {
        ticking = false;
        fn(...(lastArgs || []));
      });
    };
  };

  const copyText = async (text) => {
    if (navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(String(text));
      return true;
    }
    const ta = document.createElement('textarea');
    ta.value = String(text);
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.focus();
    ta.select();
    const ok = document.execCommand('copy');
    ta.remove();
    return ok;
  };

  const downloadText = (filename, text, mime = 'text/plain') => {
    const blob = new Blob([String(text)], { type: mime });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  };

  const downloadJson = (filename, obj) => downloadText(filename, JSON.stringify(obj, null, 2), 'application/json');

  const readJsonFile = (file) => new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => {
      try { resolve(JSON.parse(String(reader.result || 'null'))); }
      catch (e) { reject(e); }
    };
    reader.onerror = () => reject(reader.error || new Error('Could not read the file'));
    reader.readAsText(file);
  });

  /**
   * JSON-запрос с исключением при ошибке. Таймаут, 401 и разбор ответа —
   * в editor-core/http.js; здесь только привычное для старых вызовов поведение.
   */
  const fetchJson = async (url, options = {}) => {
    const { body, ...rest } = options;
    const res = await window.EditorCore.Http.json(url, { ...rest, body });
    if (!res.ok) throw new Error(res.error || 'The operation failed');
    return res.data;
  };

  /** То же, но дополнительно проверяет `{ ok: false }` в теле ответа. */
  const requestJson = async (url, options = {}) => {
    const { requireOk = true, ...fetchOptions } = options;
    const json = await fetchJson(url, fetchOptions);
    if (requireOk && !json?.ok) throw new Error(json?.error || 'The operation failed');
    return json;
  };

  const renderSkinApiUrl = (baseUrl, { name, body, foot } = {}) => {
    const trimmed = String(baseUrl || '').trim();
    if (!trimmed) return '';
    const [base, query = ''] = trimmed.split('?');
    const params = new URLSearchParams(query);
    if (name != null && name !== '') params.set('name', String(name));
    if (body != null && body !== '') params.set('body', String(body));
    if (foot != null && foot !== '') params.set('foot', String(foot));
    const qs = params.toString();
    return qs ? `${base}?${qs}` : base;
  };

  const parseSkinInfo = (raw) => {
    if (!raw) return null;
    try {
      const data = typeof raw === 'string' ? JSON.parse(raw) : raw;
      if (!data || typeof data !== 'object') return null;
      const name = data.skin || 'default';
      const custom = Number(data.custom_color ?? data.customColor ?? 0);
      const body = custom ? (data.color_body ?? data.colorBody ?? '') : '';
      const foot = custom ? (data.color_feet ?? data.color_feer ?? data.colorFeet ?? '') : '';
      return { name, body, foot };
    } catch {
      return null;
    }
  };

  const loadLookupMap = async (source, { limit = 5000, offset = 0, silent = true } = {}) => {
    if (!window.EditorCore?.DB?.list) return new Map();
    try {
      const response = await window.EditorCore.DB.list(source, { limit, offset });
      const items = response?.ok && Array.isArray(response.items) ? response.items : [];
      return new Map(items.map(item => [String(item.value), String(item.label || '')]));
    } catch (error) {
      if (!silent) throw error;
      return new Map();
    }
  };

  const getBotSkinsMap = async () => {
    if (!window.EditorCore?.DBCrud) return new Map();
    try {
      const res = await window.EditorCore.DBCrud.list('bots_info', { limit: 5000, offset: 0 });
      const rows = Array.isArray(res.rows) ? res.rows : [];
      const map = new Map();
      for (const row of rows) {
        const skin = parseSkinInfo(row.JsonTeeInfo);
        if (skin) map.set(String(row.ID), skin);
      }
      return map;
    } catch {
      return new Map();
    }
  };

  const getSkinsApi = async () => {
    if (!window.EditorCore?.DB?.getConfig) return '';
    try {
      const res = await window.EditorCore.DB.getConfig();
      return res?.ok ? String(res.config?.skins_api || '') : '';
    } catch {
      return '';
    }
  };

  // PairList helpers
  // Used by multiple DB tables where a list of pairs is stored as string:
  //   "[ItemID/Count],[ItemID/Count]"
  // Also supports JSON array inputs like: [[id,count],[id,count]] or [{id,value}].
  const PairList = {
    parse(input) {
      if (input == null) return [];
      if (Array.isArray(input)) {
        return input
          .map((x) => {
            if (Array.isArray(x) && x.length >= 2) return { id: Number(x[0]), value: Number(x[1]) };
            if (x && typeof x === 'object') return { id: Number(x.id ?? x.ItemID ?? x.item_id ?? x.key), value: Number(x.value ?? x.count ?? x.qty ?? x.amount) };
            return null;
          })
          .filter((x) => x && Number.isFinite(x.id) && x.id > 0 && Number.isFinite(x.value) && x.value > 0);
      }

      const s = String(input).trim();
      if (!s) return [];

      // JSON array support
      if (s[0] === '[') {
        try {
          const parsed = JSON.parse(s);
          if (Array.isArray(parsed)) return PairList.parse(parsed);
        } catch { /* ignore */ }
      }

      // String format: [id/value],[id/value]
      const out = [];
      const re = /\[(\s*\d+\s*)\/(\s*\d+\s*)\]/g;
      let m;
      while ((m = re.exec(s))) {
        const id = Number(String(m[1]).trim());
        const value = Number(String(m[2]).trim());
        if (Number.isFinite(id) && id > 0 && Number.isFinite(value) && value > 0) {
          out.push({ id, value });
        }
      }
      return out;
    },

    stringify(list) {
      if (!Array.isArray(list)) return '';
      return list
        .map((x) => ({ id: Number(x?.id), value: Number(x?.value) }))
        .filter((x) => Number.isFinite(x.id) && x.id > 0 && Number.isFinite(x.value) && x.value > 0)
        .map((x) => `[${x.id}/${x.value}]`)
        .join(',');
    }
  };

  const showToast = (message, type = 'success', opts = {}) => {
    ensureUI();
    const container = window.EditorCore.UI.mountToastContainer({ id: opts.containerId || 'toast-container' });
    const toast = document.createElement('div');
    const base = opts.baseClass || 'toast text-white font-semibold py-3 px-5 rounded-lg shadow-lg transition-all duration-300 transform translate-x-full';
    const cls = type === 'error'
      ? (opts.errorClass || 'bg-red-500')
      : type === 'warning'
        ? (opts.warningClass || 'bg-yellow-500')
        : type === 'info'
          ? (opts.infoClass || 'bg-sky-600')
          : (opts.successClass || 'bg-green-500');
    toast.className = `${base} ${cls}`;
    toast.textContent = String(message);
    container.appendChild(toast);
    setTimeout(() => toast.classList.remove('translate-x-full'), 10);
    const ttl = typeof opts.ttl === 'number' ? opts.ttl : 3000;
    setTimeout(() => {
      toast.classList.add('translate-x-full');
      toast.addEventListener('transitionend', () => toast.remove(), { once: true });
    }, ttl);
  };

  // ------------------------------------------------------------
  // Dirty (unsaved changes) tracker
  // ------------------------------------------------------------
  // Shared helper for DB editors: shows a small "Данные не сохранены" pill
  // and warns on page unload. Editors can call setClean() after load/save.
  const ensureDirtyPill = ({
    id = 'dirty-pill',
    text = 'Data not saved',
    saveButton = null,
    container = null,
  } = {}) => {
    let pill = document.getElementById(id);
    if (pill) return pill;

    pill = document.createElement('span');
    pill.id = id;
    pill.className = 'editor-dirty-pill hidden';
    pill.textContent = text;
    pill.setAttribute('aria-hidden', 'true');

    // Prefer inserting next to the save button.
    if (saveButton?.parentElement) {
      saveButton.parentElement.insertBefore(pill, saveButton);
      return pill;
    }
    if (container) {
      container.appendChild(pill);
      return pill;
    }
    return pill;
  };

  const createDirtyTracker = ({
    root = document,
    pill = null,
    beforeUnload = true,
    onDirtyChange = null,
    cancelButtons = null,
  } = {}) => {
    let dirty = false;
    let suspendCount = 0;
    const resolvedCancelButtons = (() => {
      if (Array.isArray(cancelButtons)) return cancelButtons.filter(Boolean);
      if (cancelButtons) return [cancelButtons].filter(Boolean);
      if (!pill?.parentElement) return [];
      return Array.from(pill.parentElement.querySelectorAll('[data-editor-role="cancel"], #cancelBtn, #btn-cancel'));
    })();

    const applyUI = () => {
      if (pill) {
        pill.classList.toggle('hidden', !dirty);
        pill.setAttribute('aria-hidden', dirty ? 'false' : 'true');
      }
      if (resolvedCancelButtons.length) {
        resolvedCancelButtons.forEach((btn) => {
          btn.classList.toggle('hidden', !dirty);
          btn.disabled = !dirty;
        });
      }
      if (typeof onDirtyChange === 'function') onDirtyChange(dirty);
    };

    const setDirty = (v) => {
      const next = !!v;
      if (next === dirty) return;
      dirty = next;
      applyUI();
    };

    const markDirty = () => {
      if (suspendCount > 0) return;
      setDirty(true);
    };

    const setClean = () => setDirty(false);

    const suspend = (fn) => {
      suspendCount++;
      try { return fn?.(); }
      finally { suspendCount = Math.max(0, suspendCount - 1); }
    };

    // Capture phase so it works even if editors stopPropagation in handlers.
    const onAnyInput = () => markDirty();
    const onAnyClick = (e) => {
      const t = e.target;
      if (!t) return;
      // Buttons that mutate state via core widgets / list actions.
      if (t.closest?.('[data-list-action],[data-tag-add],[data-tag-remove],[data-items-action]')) {
        markDirty();
      }
    };

    root.addEventListener('input', onAnyInput, true);
    root.addEventListener('change', onAnyInput, true);
    root.addEventListener('click', onAnyClick, true);

    const onBeforeUnload = (e) => {
      if (!dirty) return;
      e.preventDefault();
      // Chrome requires returnValue to be set.
      e.returnValue = '';
    };
    if (beforeUnload) window.addEventListener('beforeunload', onBeforeUnload);

    return {
      isDirty: () => dirty,
      setDirty,
      markDirty,
      setClean,
      suspend,
      destroy: () => {
        root.removeEventListener('input', onAnyInput, true);
        root.removeEventListener('change', onAnyInput, true);
        root.removeEventListener('click', onAnyClick, true);
        if (beforeUnload) window.removeEventListener('beforeunload', onBeforeUnload);
      }
    };
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.utils = {
    uuid,
    debounce,
    cloneData,
    parsePath,
    getAtPath,
    setAtPath,
    createRequestCache,
    toFiniteNumber,
    toNullableInt,
    isEmptyValue,
    canonicalValue,
    valuesEqual,
    parseStringList,
    stringifyStringList,
    splitComposite,
    joinComposite,
    splitGroupHierarchy,
    joinGroupHierarchy,
    setStatus,
    escapeAttr,
    escapeHtml,
    qs,
    cssEscape,
    rafThrottle,
    copyText,
    downloadText,
    downloadJson,
    readJsonFile,
    fetchJson,
    requestJson,
    renderSkinApiUrl,
    parseSkinInfo,
    loadLookupMap,
    getBotSkinsMap,
    getSkinsApi,
    showToast,
    ensureDirtyPill,
    createDirtyTracker,
  };

  // Convenience alias (so editors can do EditorCore.Dirty.create(...))
  window.EditorCore.Dirty = {
    ensurePill: ensureDirtyPill,
    create: createDirtyTracker,
  };

  // Also expose at top-level for convenience in editors.
  window.EditorCore.PairList = PairList;
})();
