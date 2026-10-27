(() => {
  if (window.__editorCorePageLoader) return;
  window.__editorCorePageLoader = true;


  // Иконки — локальные (vendor/), шрифты — системные: редактор работает
  // офлайн и не ходит на CDN за стилями и шрифтами.
  const linkTags = [
    '<link rel="stylesheet" href="vendor/fontawesome/css/all.min.css">',
    '<link rel="stylesheet" href="editor-core/editor-theme.css">',
    '<link rel="stylesheet" href="editor-core/position-picker.css">',
    '<link rel="stylesheet" href="editor-core/editor-template.css">',
    '<link rel="stylesheet" href="editor-core/tailwind.css">'
  ];

  /**
   * Теги языковых файлов строятся по манифесту `localization/index.js`, а не
   * перечислены здесь: добавили `lang_de.js` в манифест — он загрузится сам.
   */
  const languageTags = (registry) => {
    const files = Array.isArray(registry?.files) ? registry.files : [];
    return files.map((file) => `<script src="localization/${file}"></script>`);
  };

  // В document.write-пути манифест уже прочитан (index.js объявлен выше по
  // списку), поэтому языковые теги можно записать прямо в поток разбора.
  const LANGUAGE_PLACEHOLDER = '<script>'
    + 'window.EditorCoreLocalization && window.EditorCoreLocalization.files.forEach(function (file) {'
    + '  document.write("<script src=\\"localization/" + file + "\\"><\\/script>");'
    + '});'
    + '</script>';

  const scriptTags = [
    '<script src="editor-core/tailwind-theme.js"></script>',
    // Сетевой слой подключается первым: им пользуются все остальные модули.
    '<script src="editor-core/http.js"></script>',
    '<script src="editor-core/utils.js"></script>',
    '<script src="studio.manifest.js"></script>',
    '<script src="editor-core/reference-data.js"></script>',
    '<script src="editor-core/guest-mode.js"></script>',
    '<script src="editor-core/change-markers.js"></script>',
    '<script src="editor-core/request-context.js"></script>',
    // Ссылки «открыть запись» из вкладки «Запросы» (?record=&field=).
    '<script src="editor-core/deep-link.js"></script>',
    '<script src="editor-core/registry.js"></script>',
    '<script src="editor-core/core.js"></script>',
    '<script src="editor-core/field-renderer.js"></script>',
    '<script src="editor-core/dependent-select.js"></script>',
    '<script src="editor-core/form-runtime.js"></script>',
    '<script src="editor-core/dialog-editor.js"></script>',
    '<script src="editor-core/db-map.js"></script>',
    '<script src="editor-core/related-records.js"></script>',
    '<script src="editor-core/db.js"></script>',
    '<script src="editor-core/db-crud.js"></script>',
    '<script src="editor-core/ui.js"></script>',
    '<script src="editor-core/ui-manager.js"></script>',
    '<script src="editor-core/defaults.js"></script>',
    // Языки интерфейса: манифест папки `localization/` (какие файлы грузить,
    // какой язык по умолчанию), затем сами языковые файлы из манифеста, затем
    // движок перевода. Новый язык = файл `lang_xx.js` + строка в манифесте.
    '<script src="localization/index.js"></script>',
    LANGUAGE_PLACEHOLDER,
    '<script src="editor-core/i18n.js"></script>',
    '<script src="editor-core/page-kit.js"></script>',
    '<script src="editor-core/help.js"></script>',
    '<script src="editor-core/bootstrap.js"></script>',
    '<script src="editor-core/db-editor-runtime.js"></script>',
    // Выбор позиции: общие данные о мирах, окно карты и единый UI-элемент.
    '<script src="editor-core/map-worlds.js"></script>',
    '<script src="editor-core/point-map.js"></script>',
    '<script src="editor-core/position-picker.js"></script>',
    '<script src="editor-core/db-sync.js"></script>',
    '<script src="editor-core/request-workbench.js"></script>',
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
    box.innerHTML = '<span aria-hidden="true">⚠</span><span>Failed to load the local styles.  '
      + 'The editor still works, but the interface may look simplified.</span>';
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
    const text = 'Something went wrong. Reload the page; if the error repeats, tell the administrator.';
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

  const appendScript = (tag) => new Promise((resolve, reject) => {
    const template = document.createElement('template');
    template.innerHTML = tag.trim();
    const source = template.content.firstChild;
    const script = document.createElement('script');
    for (const attr of source.attributes) script.setAttribute(attr.name, attr.value);
    script.textContent = source.textContent;
    if (!script.src) {
      body.appendChild(script);
      resolve();
      return;
    }
    script.async = false;
    script.addEventListener('load', resolve, { once: true });
    script.addEventListener('error', () => reject(new Error(`Could not load ${script.src}`)), { once: true });
    body.appendChild(script);
  });

  const loadScripts = async () => {
    // Dynamic scripts are async by default; awaiting each one preserves the
    // same dependency order as the document.write path above.
    for (const tag of scriptTags) {
      if (tag !== LANGUAGE_PLACEHOLDER) { await appendScript(tag); continue; }
      // Языковые файлы берём из манифеста уже загруженного реестра.
      for (const languageTag of languageTags(window.EditorCoreLocalization)) {
        await appendScript(languageTag);
      }
    }
  };

  linkTags.forEach(tag => appendHtml(tag, head));
  loadScripts().catch(reportUnexpectedError);
  window.setTimeout(warnIfStylingMissing, 4000);
})();
