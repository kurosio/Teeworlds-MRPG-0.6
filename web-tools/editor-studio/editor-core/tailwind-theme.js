// Shared appearance preferences for all editors and the Studio shell:
// theme mode (dark / light / system), accent color and interface density.
// Preferences live in localStorage and are mirrored to data-editor-* attributes
// on <html> and <body>, so editor-theme.css can style every page consistently.
// The legacy `editor-theme` key keeps holding the *resolved* theme for older code.

(function () {
  const THEME_KEY = 'editor-theme';
  const MODE_KEY = 'editor-theme-mode';
  const ACCENT_KEY = 'editor-accent';
  const DENSITY_KEY = 'editor-density';

  const MODES = ['dark', 'light', 'system'];
  const ACCENTS = ['indigo', 'teal', 'violet', 'amber'];
  const DENSITIES = ['comfortable', 'compact'];

  const read = (key) => {
    try { return localStorage.getItem(key); } catch { return null; }
  };
  const write = (key, value) => {
    try { localStorage.setItem(key, value); } catch { /* noop */ }
  };

  const normalizeTheme = (theme) => (theme === 'light' ? 'light' : 'dark');
  const oneOf = (list, value, fallback) => (list.includes(value) ? value : fallback);

  const getPrefs = () => {
    // Migration: before modes existed, `editor-theme` stored dark/light directly.
    const mode = oneOf(MODES, read(MODE_KEY), null) || normalizeTheme(read(THEME_KEY) || 'dark');
    return {
      mode,
      accent: oneOf(ACCENTS, read(ACCENT_KEY), 'indigo'),
      density: oneOf(DENSITIES, read(DENSITY_KEY), 'comfortable'),
    };
  };

  const systemTheme = () => (
    typeof window.matchMedia === 'function'
    && window.matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark'
  );
  const resolveTheme = (prefs) => (prefs.mode === 'system' ? systemTheme() : normalizeTheme(prefs.mode));

  const applyPrefsToDoc = (doc, prefs) => {
    if (!doc) return;
    const theme = resolveTheme(prefs);
    const root = doc.documentElement;
    if (root) {
      root.setAttribute('data-editor-theme', theme);
      if (root.hasAttribute('data-theme')) root.setAttribute('data-theme', theme);
      root.setAttribute('data-editor-accent', prefs.accent);
      root.setAttribute('data-editor-density', prefs.density);
    }
    if (doc.body) {
      doc.body.setAttribute('data-editor-theme', theme);
      doc.body.setAttribute('data-editor-accent', prefs.accent);
      doc.body.setAttribute('data-editor-density', prefs.density);
    }
  };

  const dispatch = (targetWindow, prefs) => {
    try {
      const detail = { theme: resolveTheme(prefs), mode: prefs.mode, accent: prefs.accent, density: prefs.density };
      targetWindow?.dispatchEvent?.(new CustomEvent('editor-core:theme-changed', { detail }));
      targetWindow?.dispatchEvent?.(new CustomEvent('editor-core:prefs-changed', { detail }));
    } catch { /* noop */ }
  };

  const propagatePrefs = (prefs) => {
    applyPrefsToDoc(document, prefs);
    dispatch(window, prefs);

    // Sync child iframes (Studio shell -> editors, or editor -> map picker)
    try {
      document.querySelectorAll('iframe').forEach((frame) => {
        try {
          if (frame.contentDocument) applyPrefsToDoc(frame.contentDocument, prefs);
          dispatch(frame.contentWindow, prefs);
        } catch { /* cross-origin guard */ }
      });
    } catch { /* noop */ }

    // Sync parent shell when switched from an embedded iframe (Settings / palette)
    try {
      if (window.parent && window.parent !== window && window.parent.document) {
        applyPrefsToDoc(window.parent.document, prefs);
        dispatch(window.parent, prefs);
      }
    } catch { /* noop */ }
  };

  const applyPrefs = (partial = {}, { persist = true } = {}) => {
    const current = getPrefs();
    const prefs = {
      mode: oneOf(MODES, partial.mode, current.mode),
      accent: oneOf(ACCENTS, partial.accent, current.accent),
      density: oneOf(DENSITIES, partial.density, current.density),
    };
    if (persist) {
      write(MODE_KEY, prefs.mode);
      write(ACCENT_KEY, prefs.accent);
      write(DENSITY_KEY, prefs.density);
      write(THEME_KEY, resolveTheme(prefs));
    }
    propagatePrefs(prefs);
    return prefs;
  };

  // ── Legacy theme API (dark/light on the resolved value) ───────────────────
  const applyTheme = (theme, { persist = true } = {}) => {
    if (theme === 'system') {
      applyPrefs({ mode: 'system' }, { persist });
      return resolveTheme(getPrefs());
    }
    const value = normalizeTheme(theme);
    applyPrefs({ mode: value }, { persist });
    return value;
  };

  const getCurrentTheme = () => resolveTheme(getPrefs());

  const toggleTheme = () => applyTheme(getCurrentTheme() === 'dark' ? 'light' : 'dark', { persist: true });

  const initPrefs = () => applyPrefsToDoc(document, getPrefs());

  initPrefs();
  document.addEventListener('DOMContentLoaded', initPrefs, { once: true });

  window.addEventListener('storage', (event) => {
    if (event.key && ![THEME_KEY, MODE_KEY, ACCENT_KEY, DENSITY_KEY].includes(event.key)) return;
    propagatePrefs(getPrefs());
  });

  // Follow the OS while the mode is "system".
  try {
    window.matchMedia?.('(prefers-color-scheme: light)')?.addEventListener?.('change', () => {
      const prefs = getPrefs();
      if (prefs.mode === 'system') {
        write(THEME_KEY, resolveTheme(prefs));
        propagatePrefs(prefs);
      }
    });
  } catch { /* noop */ }

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.theme = {
    key: THEME_KEY,
    list: ['dark', 'light'],
    get: getCurrentTheme,
    set: (theme) => applyTheme(theme, { persist: true }),
    toggle: toggleTheme,
    apply: (theme) => applyTheme(theme, { persist: false }),
    init: initPrefs,
  };

  window.EditorCore.prefs = {
    modes: MODES,
    accents: ACCENTS,
    densities: DENSITIES,
    keys: { mode: MODE_KEY, accent: ACCENT_KEY, density: DENSITY_KEY },
    get: getPrefs,
    set: (partial) => applyPrefs(partial, { persist: true }),
    apply: (partial) => applyPrefs(partial, { persist: false }),
    resolveTheme,
  };

  window.EditorCoreTheme = {
    STORAGE_KEY: THEME_KEY,
    getTheme: getCurrentTheme,
    setTheme: (theme) => applyTheme(theme, { persist: true }),
    toggleTheme,
    applyTheme: (theme) => applyTheme(theme, { persist: false }),
  };
})();
