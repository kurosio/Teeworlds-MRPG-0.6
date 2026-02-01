// Shared Tailwind theme overrides for all editors.
// IMPORTANT: Load this file BEFORE the Tailwind CDN script.

(function () {
  window.tailwind = window.tailwind || {};
  const c = (v) => `var(${v})`;

  // Keep Tailwind's default palette intact.
  // We only add an `editor.*` color namespace + font.
  window.tailwind.config = {
    theme: {
      extend: {
        fontFamily: {
          sans: [
            'Inter',
            'ui-sans-serif',
            'system-ui',
            'Segoe UI',
            'Roboto',
            'Arial',
            'sans-serif'
          ],
        },
        colors: {
          editor: {
            bg: c('--editor-bg'),
            surface: c('--editor-surface'),
            surfaceAlt: c('--editor-surface-alt'),
            panel: c('--editor-panel'),
            border: c('--editor-border'),
            primary: c('--editor-primary'),
            secondary: c('--editor-secondary'),
            accent: c('--editor-accent'),
            danger: c('--editor-danger'),
            warning: c('--editor-warning'),
            text: c('--editor-text'),
            muted: c('--editor-text-muted'),
          },
        },
      },
    },
  };

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
