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
  };
})();
