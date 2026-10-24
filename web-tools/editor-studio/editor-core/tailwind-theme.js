// Shared light/dark theme preferences for all editors.

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
  const applyTheme = (theme, { persist = true } = {}) => {
    const value = normalizeTheme(theme);
    document.documentElement?.setAttribute('data-editor-theme', value);
    if (document.body) {
      document.body.setAttribute('data-editor-theme', value);
    }
    if (persist) writeTheme(value);
    return value;
  };
  const initTheme = () => {
    const stored = readTheme();
    applyTheme(stored || 'dark', { persist: false });
  };

  initTheme();
  document.addEventListener('DOMContentLoaded', () => {
    const stored = readTheme();
    applyTheme(stored || document.documentElement.getAttribute('data-editor-theme') || 'dark', { persist: false });
  }, { once: true });

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.theme = {
    key: THEME_KEY,
    list: ['dark', 'light'],
    get: () => normalizeTheme(readTheme() || document.documentElement.getAttribute('data-editor-theme') || 'dark'),
    set: (theme) => applyTheme(theme, { persist: true }),
    apply: (theme) => applyTheme(theme, { persist: false }),
    init: initTheme,
  };
})();
