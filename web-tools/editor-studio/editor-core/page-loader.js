(() => {
  if (window.__editorCorePageLoader) return;
  window.__editorCorePageLoader = true;


  const linkTags = [
    '<link rel="preconnect" href="https://fonts.googleapis.com">',
    '<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>',
    '<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">',
    '<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css">',
    '<link rel="stylesheet" href="editor-core/editor-theme.css">',
    '<link rel="stylesheet" href="editor-core/editor-template.css">',
    '<link rel="stylesheet" href="editor-core/tailwind.css">'
  ];

  const scriptTags = [
    '<script src="editor-core/tailwind-theme.js"></script>',
    // Сетевой слой подключается первым: им пользуются все остальные модули.
    '<script src="editor-core/http.js"></script>',
    '<script src="studio.manifest.js"></script>',
    '<script src="editor-core/guest-mode.js"></script>',
    '<script src="editor-core/registry.js"></script>',
    '<script src="editor-core/core.js"></script>',
    '<script src="editor-core/field-renderer.js"></script>',
    '<script src="editor-core/dependent-select.js"></script>',
    '<script src="editor-core/form-runtime.js"></script>',
    '<script src="editor-core/dialog-editor.js"></script>',
    '<script src="editor-core/db-map.js"></script>',
    '<script src="editor-core/db.js"></script>',
    '<script src="editor-core/db-crud.js"></script>',
    '<script src="editor-core/ui.js"></script>',
    '<script src="editor-core/ui-manager.js"></script>',
    '<script src="editor-core/defaults.js"></script>',
    '<script src="editor-core/utils.js"></script>',
    '<script src="editor-core/i18n-en.js"></script>',
    '<script src="editor-core/i18n.js"></script>',
    '<script src="editor-core/page-kit.js"></script>',
    '<script src="editor-core/help.js"></script>',
    '<script src="editor-core/bootstrap.js"></script>',
    '<script src="editor-core/db-editor-runtime.js"></script>',
    // Окно «Местоположение» (карта мира) для любых координат X/Y.
    '<script src="editor-core/point-map.js"></script>',
    '<script src="editor-core/db-sync.js"></script>',
    `<script>
      if (window.EditorCore?.bootstrapEditor) {
        window.EditorCoreBootstrap = window.EditorCore.bootstrapEditor();
      }
    </script>`
  ];

  // Check the compiled stylesheet rather than a CDN runtime global.
  const isTailwindApplied = () => {
    const probe = document.createElement('div');
    probe.className = 'flex items-center';
    probe.style.cssText = 'position:absolute;left:-9999px;top:0';
    (document.body || document.documentElement).appendChild(probe);
    const applied = window.getComputedStyle(probe).display === 'flex';
    probe.remove();
    return applied;
  };

  const warnIfStylingMissing = () => {
    if (window.__editorStylingWarned) return;
    if (!document.body || isTailwindApplied()) return;
    window.__editorStylingWarned = true;
    const box = document.createElement('div');
    box.setAttribute('role', 'status');
    box.style.cssText = 'position:fixed;left:12px;bottom:12px;z-index:2147483001;max-width:420px;padding:10px 12px;'
      + 'border-radius:12px;font:13px/1.45 system-ui,sans-serif;color:#ffd7a3;background:#2a1f10;border:1px solid #7c5a25;'
      + 'display:flex;gap:8px;align-items:flex-start';
    box.innerHTML = '<span aria-hidden="true">⚠</span><span>Не удалось загрузить локальное оформление. '
      + 'Редактор работает, но интерфейс может выглядеть упрощённо.</span>';
    box.addEventListener('click', () => box.remove());
    document.body?.appendChild(box);
    // Медленная сеть: если оформление всё же догрузилось — убираем предупреждение.
    window.setTimeout(() => { if (isTailwindApplied()) box.remove(); }, 12000);
  };

  // ── Непредвиденные ошибки ────────────────────────────────────────────────
  // Ошибка в одном обработчике не должна оставлять пользователя с «мёртвой»
  // кнопкой и пустым экраном: показываем одну понятную плашку, подробности
  // остаются в консоли разработчика.
  let reportedErrors = 0;
  const reportUnexpectedError = (detail) => {
    if (reportedErrors >= 3) return;
    reportedErrors += 1;
    console.error('[editor-studio]', detail);
    const toast = window.EditorCore?.UI?.toast;
    const text = 'Что-то пошло не так. Обновите страницу; если ошибка повторится — сообщите администратору.';
    if (typeof toast === 'function') toast(text, 'error');
  };

  if (!window.__editorErrorHooks) {
    window.__editorErrorHooks = true;
    window.addEventListener('unhandledrejection', (event) => {
      reportUnexpectedError(event?.reason || 'unhandled rejection');
    });
    window.addEventListener('error', (event) => {
      // Ресурсные ошибки (картинка, шрифт) не интересны пользователю.
      if (!(event instanceof ErrorEvent) || !event.message) return;
      reportUnexpectedError(event.error || event.message);
    });
  }

  if (document.readyState === 'loading') {
    document.write([...linkTags, ...scriptTags].join('\n'));
    window.setTimeout(warnIfStylingMissing, 4000);
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
  window.setTimeout(warnIfStylingMissing, 4000);
})();
