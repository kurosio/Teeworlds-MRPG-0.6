const c = (v) => `var(${v})`;
module.exports = {
    theme: {
      extend: {
        fontFamily: {
          sans: [
            'system-ui',
            '-apple-system',
            'Segoe UI',
            'Roboto',
            'Helvetica Neue',
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


module.exports.content = ['./*.html', './editor-core/**/*.js', './scenarios/**/*.{html,js}'];
module.exports.plugins = [require('@tailwindcss/forms')];
