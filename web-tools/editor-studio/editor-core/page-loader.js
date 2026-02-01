(() => {
  if (window.__editorCorePageLoader) return;
  window.__editorCorePageLoader = true;

  const currentScript = document.currentScript;
  const mode = currentScript?.dataset?.mode || currentScript?.getAttribute('data-mode');
  const bootstrapMode = mode || 'scenario';

  const linkTags = [
    '<link rel="preconnect" href="https://fonts.googleapis.com">',
    '<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>',
    '<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">',
    '<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css">',
    '<link rel="stylesheet" href="editor-core/editor-theme.css">',
    '<link rel="stylesheet" href="editor-core/editor-template.css">'
  ];

  const scriptTags = [
    '<script src="editor-core/tailwind-theme.js"></script>',
    '<script src="https://cdn.tailwindcss.com?plugins=forms"></script>',
    '<script src="editor-core/registry.js"></script>',
    '<script src="editor-core/core.js"></script>',
    '<script src="editor-core/field-renderer.js"></script>',
    '<script src="editor-core/form-runtime.js"></script>',
    '<script src="editor-core/dialog-editor.js"></script>',
    '<script src="editor-core/db-map.js"></script>',
    '<script src="editor-core/db.js"></script>',
    '<script src="editor-core/db-crud.js"></script>',
    '<script src="editor-core/ui.js"></script>',
    '<script src="editor-core/ui-manager.js"></script>',
    '<script src="editor-core/defaults.js"></script>',
    '<script src="editor-core/utils.js"></script>',
    '<script src="editor-core/bootstrap.js"></script>',
    '<script src="editor-core/db-editor-runtime.js"></script>',
    `<script>
      if (window.EditorCore?.bootstrapEditor) {
        window.EditorCoreBootstrap = window.EditorCore.bootstrapEditor({ mode: ${JSON.stringify(bootstrapMode)} });
      }
    </script>`
  ];

  if (document.readyState === 'loading') {
    document.write([...linkTags, ...scriptTags].join('\n'));
    return;
  }

  const head = document.head || document.getElementsByTagName('head')[0] || document.documentElement;
  const body = document.body || document.getElementsByTagName('body')[0] || document.documentElement;

  const appendHtml = (html, parent) => {
    const template = document.createElement('template');
    template.innerHTML = html.trim();
    parent.appendChild(template.content.firstChild);
  };

  linkTags.forEach(tag => appendHtml(tag, head));
  scriptTags.forEach(tag => appendHtml(tag, body));
})();
