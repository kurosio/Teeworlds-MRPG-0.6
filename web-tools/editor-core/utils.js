(() => {
  const ensureUI = () => {
    if (!window.EditorCore?.UI) {
      throw new Error('EditorCore.UI is not available. Load ui.js before utils.js.');
    }
  };

  const uuid = () => ([1e7] + -1e3 + -4e3 + -8e3 + -1e11)
    .replace(/[018]/g, c => (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16));

  const debounce = (fn, delay = 200) => {
    let t;
    return (...args) => {
      clearTimeout(t);
      t = setTimeout(() => fn(...args), delay);
    };
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
    reader.onerror = () => reject(reader.error || new Error('Failed to read file'));
    reader.readAsText(file);
  });

  const fetchJson = async (url, options = {}) => {
    const res = await fetch(url, {
      headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
      ...options
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const text = await res.text();
    return text ? JSON.parse(text) : null;
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
    text = 'Данные не сохранены',
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
  } = {}) => {
    let dirty = false;
    let suspendCount = 0;

    const applyUI = () => {
      if (pill) {
        pill.classList.toggle('hidden', !dirty);
        pill.setAttribute('aria-hidden', dirty ? 'false' : 'true');
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
    rafThrottle,
    copyText,
    downloadText,
    downloadJson,
    readJsonFile,
    fetchJson,
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
