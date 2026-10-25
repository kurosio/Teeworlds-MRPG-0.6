// Shared light/dark theme preferences for all editors and Studio shell.

(function () {
  const THEME_KEY = 'editor-theme';
  const normalizeTheme = (theme) => (theme === 'light' ? 'light' : 'dark');

  const readTheme = () => {
    try {
      return localStorage.getItem(THEME_KEY);
    } catch {
      return null;
    }
  };

  const writeTheme = (value) => {
    try {
      localStorage.setItem(THEME_KEY, value);
    } catch {
      // noop
    }
  };

  const applyThemeToDoc = (doc, value) => {
    if (!doc) return;
    doc.documentElement?.setAttribute('data-editor-theme', value);
    if (doc.documentElement?.hasAttribute('data-theme')) {
      doc.documentElement.setAttribute('data-theme', value);
    }
    if (doc.body) {
      doc.body.setAttribute('data-editor-theme', value);
    }
  };

  const propagateTheme = (value) => {
    applyThemeToDoc(document, value);
    try {
      window.dispatchEvent(new CustomEvent('editor-core:theme-changed', { detail: { theme: value } }));
    } catch {}

    // Sync child iframes (Studio shell -> active editor, or editor -> map picker)
    try {
      document.querySelectorAll('iframe').forEach((frame) => {
        try {
          if (frame.contentDocument) {
            applyThemeToDoc(frame.contentDocument, value);
          }
          frame.contentWindow?.dispatchEvent?.(
            new CustomEvent('editor-core:theme-changed', { detail: { theme: value } })
          );
        } catch {}
      });
    } catch {}

    // Sync parent shell when switched from an embedded iframe (Settings / Command Palette)
    try {
      if (window.parent && window.parent !== window && window.parent.document) {
        applyThemeToDoc(window.parent.document, value);
        window.parent.dispatchEvent?.(
          new CustomEvent('editor-core:theme-changed', { detail: { theme: value } })
        );
      }
    } catch {}
  };

  const applyTheme = (theme, { persist = true } = {}) => {
    const value = normalizeTheme(theme);
    if (persist) writeTheme(value);
    propagateTheme(value);
    return value;
  };

  const getCurrentTheme = () =>
    normalizeTheme(readTheme() || document.documentElement?.getAttribute('data-editor-theme') || 'dark');

  const toggleTheme = () => {
    const next = getCurrentTheme() === 'dark' ? 'light' : 'dark';
    return applyTheme(next, { persist: true });
  };

  const initTheme = () => {
    const stored = readTheme();
    applyThemeToDoc(document, normalizeTheme(stored || 'dark'));
  };

  initTheme();
  document.addEventListener('DOMContentLoaded', () => {
    const stored = readTheme();
    applyThemeToDoc(
      document,
      normalizeTheme(stored || document.documentElement?.getAttribute('data-editor-theme') || 'dark')
    );
  }, { once: true });

  window.addEventListener('storage', (event) => {
    if (event.key && event.key !== THEME_KEY) return;
    propagateTheme(getCurrentTheme());
  });

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.theme = {
    key: THEME_KEY,
    list: ['dark', 'light'],
    get: getCurrentTheme,
    set: (theme) => applyTheme(theme, { persist: true }),
    toggle: toggleTheme,
    apply: (theme) => applyTheme(theme, { persist: false }),
    init: initTheme,
  };

  window.EditorCoreTheme = {
    STORAGE_KEY: THEME_KEY,
    getTheme: getCurrentTheme,
    setTheme: (theme) => applyTheme(theme, { persist: true }),
    toggleTheme,
    applyTheme: (theme) => applyTheme(theme, { persist: false }),
  };
})();
