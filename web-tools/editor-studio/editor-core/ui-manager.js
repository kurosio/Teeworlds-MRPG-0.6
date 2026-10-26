(() => {
  const cssEscape = (...args) => window.EditorCore?.utils?.cssEscape?.(...args) ?? String(args[0] ?? '');
  const parseValidateSpec = (raw) => {
    if (!raw) return null;
    try {
      return JSON.parse(raw);
    } catch {
      return null;
    }
  };

  const setHint = (hintEl, message) => {
    if (!hintEl) return;
    if (!message) {
      hintEl.textContent = '';
      hintEl.classList.add('hidden');
      return;
    }
    hintEl.textContent = message;
    hintEl.classList.remove('hidden');
  };

  const Productivity = (() => {
    const LS_KEY = 'editor-core:productivity-prefs:v1';
    const esc = (value) => window.EditorCore?.utils?.escapeHtml
      ? window.EditorCore.utils.escapeHtml(value)
      : String(value ?? '');

    const getPrefs = () => {
      try {
        const parsed = JSON.parse(localStorage.getItem(LS_KEY) || '{}');
        return {
          compact: parsed.compact === true,
          focus: parsed.focus === true,
        };
      } catch {
        return { compact: false, focus: false };
      }
    };

    const savePrefs = (prefs) => {
      try {
        localStorage.setItem(LS_KEY, JSON.stringify({
          compact: !!prefs.compact,
          focus: !!prefs.focus,
        }));
      } catch {
        // ignore storage failures
      }
    };

    const findActions = (root = document) => {
      const bySelectors = (selectors) => {
        for (const sel of selectors) {
          const el = root.querySelector(sel) || document.querySelector(sel);
          if (el) return el;
        }
        return null;
      };

      return {
        save: bySelectors(['[data-editor-role="save"]', '[data-action="save"]', '#saveBtn', '#btn-save', '#save-button']),
        newItem: bySelectors(['[data-editor-role="new"]', '[data-action="new"]', '#newBtn', '#btn-new', '#newQuestBtn']),
        refresh: bySelectors(['[data-editor-role="refresh"]', '[data-action="refresh"]', '#refreshBtn', '#btn-refresh']),
        remove: bySelectors(['[data-editor-role="delete"]', '[data-action="delete"]', '#deleteBtn', '#btn-delete']),
        cancel: bySelectors(['[data-editor-role="cancel"]', '[data-action="cancel"]', '#cancelBtn', '#btn-cancel']),
        search: bySelectors([
          '[data-editor-role="search"]',
          '[data-bind="search"]',
          '#searchInput',
          '#search',
          'input[type="search"]',
          '.editor-search input',
        ]),
        list: bySelectors(['[data-editor-role="list"]', '[data-bind="list"]', '#list', '#tree', '.editor-sidebar-list']),
      };
    };

    const clickIfEnabled = (el) => {
      if (!el || el.disabled || el.classList?.contains('hidden')) return false;
      el.click();
      return true;
    };

    const focusSearch = (el) => {
      if (!el) return false;
      el.focus();
      el.select?.();
      return true;
    };

    // Строки списков записей во всех редакторах: CRUD-списки (.editor-row),
    // дерево квестов (.quest-node/.step-node) и очередь запросов
    // (.cr-list-item). Классы активной строки — тот же набор, что в
    // change-markers.js/deep-link.js: единый вид списков.
    const ROW_SELECTOR = '.editor-row, .quest-node, .step-node, .cr-list-item';
    const ACTIVE_ROW_CLASSES = ['editor-row-active', 'tree-selected', 'ring-2', 'active', 'is-active', 'selected'];

    const isRowVisible = (el) => {
      if (el.offsetParent !== null || el.getClientRects?.().length) return true;
      // Строка без геометрии: либо спрятана (display:none у неё или родителя),
      // либо среда без layout вовсе (jsdom в тестах) — различаем по стилям.
      for (let node = el; node && node !== document.body; node = node.parentElement) {
        if (node.hidden) return false;
        let display = '';
        try { display = window.getComputedStyle?.(node)?.display || ''; } catch { /* нет стилей — считаем видимой */ }
        if (display === 'none') return false;
      }
      return true;
    };

    const listRows = (listRoot) => {
      if (!listRoot?.querySelectorAll) return [];
      return Array.from(listRoot.querySelectorAll(ROW_SELECTOR)).filter(isRowVisible);
    };

    const activeRowIndex = (rows) => rows.findIndex((row) => (
      ACTIVE_ROW_CLASSES.some((cls) => row.classList?.contains(cls))
    ));

    const clickRow = (row) => {
      if (!row) return false;
      const target = row.querySelector?.('.quest-node-title, .step-node-header') || row;
      target?.click?.();
      row.scrollIntoView?.({ block: 'nearest' });
      return true;
    };

    const moveSelectionTo = (listRoot, index) => {
      const rows = listRows(listRoot);
      if (!rows.length) return false;
      const next = Math.max(0, Math.min(rows.length - 1, index));
      if (next === activeRowIndex(rows)) {
        rows[next].scrollIntoView?.({ block: 'nearest' });
        return true;
      }
      return clickRow(rows[next]);
    };

    const moveSelectionBy = (listRoot, delta) => {
      const rows = listRows(listRoot);
      if (!rows.length) return false;
      const active = activeRowIndex(rows);
      return moveSelectionTo(listRoot, active < 0 ? (delta < 0 ? rows.length - 1 : 0) : active + delta);
    };

    const moveSelection = (listRoot, direction = 1) => moveSelectionBy(listRoot, direction);

    const syncToolsButtonState = (prefs) => {
      const btn = document.querySelector?.('[data-editor-quick-tools="1"]');
      if (!btn) return;
      const active = !!(prefs?.compact || prefs?.focus);
      btn.setAttribute('data-active-modes', active ? '1' : '0');
    };

    const mountPalette = (root = document) => {
      let modal = document.getElementById('editor-command-palette');
      if (modal) return modal;
      modal = document.createElement('div');
      modal.id = 'editor-command-palette';
      modal.className = 'hidden fixed inset-0 editor-modal-backdrop z-[550] items-start justify-center p-4';
      modal.innerHTML = `
        <div class="editor-modal-content w-full max-w-xl mt-10 p-0 overflow-hidden">
          <div class="editor-modal-head flex items-center justify-between gap-3">
            <div>
              <div class="editor-modal-title flex items-center gap-2">
                <i class="fa-solid fa-sliders editor-tone-accent"></i>
                <span>Quick actions and view</span>
              </div>
              <div class="editor-muted-text text-xs mt-1">Shortcuts: Ctrl+S save · Alt+N create · / search · ↑↓ records · Ctrl+Tab switch editor</div>
            </div>
            <button type="button" class="editor-icon-btn" data-role="close" title="Close (Esc)">
              <i class="fa-solid fa-xmark"></i>
            </button>
          </div>
          <div class="editor-modal-body grid gap-2" data-role="actions"></div>
        </div>
      `;
      (document.body || root).appendChild(modal);
      return modal;
    };

    const setMode = (mode, enabled, prefs) => {
      // The loader can run while the document is being replaced (for example,
      // when Studio changes the editor iframe). There is no body to update in
      // that case; keep initialization and keyboard handlers non-fatal.
      const body = document.body;
      if (!body) return;

      if (mode === 'compact') {
        body.classList.toggle('editor-compact', enabled);
        prefs.compact = enabled;
      }
      if (mode === 'focus') {
        body.classList.toggle('editor-focus', enabled);
        prefs.focus = enabled;
      }
      savePrefs(prefs);
      syncToolsButtonState(prefs);
    };

    const closePalette = (palette) => {
      if (!palette) return;
      palette.classList.add('hidden');
      palette.classList.remove('flex');
    };

    const renderPalette = (palette, root, prefs) => {
      const actionsWrap = palette.querySelector('[data-role="actions"]');
      if (!actionsWrap) return;
      const actions = findActions(root);
      const curTheme = window.EditorCoreTheme?.getTheme?.() === 'light' ? 'light' : 'dark';
      const rows = [
        {
          id: 'save',
          icon: 'fa-floppy-disk',
          label: 'Save changes',
          kbd: 'Ctrl+S',
          disabled: !actions.save || actions.save.disabled || actions.save.classList?.contains('hidden'),
          closeOnRun: true,
          run: () => clickIfEnabled(actions.save),
        },
        {
          id: 'new',
          icon: 'fa-plus',
          label: 'Create new record',
          kbd: 'Alt+N',
          disabled: !actions.newItem || actions.newItem.disabled || actions.newItem.classList?.contains('hidden'),
          closeOnRun: true,
          run: () => clickIfEnabled(actions.newItem),
        },
        {
          id: 'refresh',
          icon: 'fa-rotate',
          label: 'Refresh the list',
          kbd: 'Alt+R',
          disabled: !actions.refresh || actions.refresh.disabled,
          closeOnRun: true,
          run: () => clickIfEnabled(actions.refresh),
        },
        {
          id: 'search',
          icon: 'fa-magnifying-glass',
          label: 'Go to search',
          kbd: '/',
          disabled: !actions.search,
          closeOnRun: true,
          run: () => focusSearch(actions.search),
        },
        // Переключение между редакторами доступно только внутри оболочки;
        // у редактора, открытого отдельной вкладкой браузера, студии нет.
        ...(inShellFrame() ? [
          {
            id: 'switch-editor',
            icon: 'fa-arrow-right-arrow-left',
            label: 'Find and switch editor',
            kbd: 'Ctrl+Tab',
            closeOnRun: true,
            run: () => forwardToShell('palette'),
          },
          {
            id: 'prev-editor',
            icon: 'fa-arrow-left',
            label: 'Back to the previous editor',
            kbd: 'Alt+←',
            closeOnRun: true,
            run: () => forwardToShell('history-back'),
          },
        ] : []),
        {
          id: 'prev',
          icon: 'fa-arrow-up',
          label: 'Previous record in the list',
          kbd: 'Alt+↑',
          disabled: !actions.list,
          closeOnRun: true,
          run: () => moveSelection(actions.list, -1),
        },
        {
          id: 'next',
          icon: 'fa-arrow-down',
          label: 'Next record in the list',
          kbd: 'Alt+↓',
          disabled: !actions.list,
          closeOnRun: true,
          run: () => moveSelection(actions.list, +1),
        },
        {
          id: 'compact',
          icon: 'fa-compress',
          label: prefs.compact ? 'Compact mode: on (turn off)' : 'Compact mode: off (turn on)',
          kbd: prefs.compact ? 'ON' : 'OFF',
          closeOnRun: false,
          run: () => setMode('compact', !prefs.compact, prefs),
        },
        {
          id: 'focus',
          icon: 'fa-expand',
          label: prefs.focus ? 'Focus mode without the list: on (show the list)' : 'Focus mode: hide the side list',
          kbd: prefs.focus ? 'ON' : 'OFF',
          closeOnRun: false,
          run: () => setMode('focus', !prefs.focus, prefs),
        },
        {
          id: 'theme',
          icon: curTheme === 'dark' ? 'fa-sun' : 'fa-moon',
          label: curTheme === 'dark' ? 'Switch to the light theme' : 'Switch to the dark theme',
          kbd: curTheme === 'dark' ? 'Dark' : 'Light',
          closeOnRun: false,
          run: () => window.EditorCoreTheme?.toggleTheme?.(),
        },
        {
          id: 'help',
          icon: 'fa-circle-question',
          label: 'Open the editor help',
          kbd: '?',
          disabled: !window.EditorCore?.Help?.open,
          closeOnRun: true,
          run: () => window.EditorCore?.Help?.open?.(),
        },
      ];

      actionsWrap.innerHTML = rows
        .map((r) => `
          <button type="button" class="editor-btn editor-btn-secondary justify-between text-left w-full" data-action="${esc(r.id)}" ${r.disabled ? 'disabled' : ''}>
            <span class="flex items-center gap-2.5 min-w-0 truncate">
              <i class="fa-solid ${esc(r.icon)} editor-muted-text"></i>
              <span class="truncate">${esc(r.label)}</span>
            </span>
            <span class="editor-badge text-xs shrink-0">${esc(r.kbd || '')}</span>
          </button>`)
        .join('');

      actionsWrap.querySelectorAll('[data-action]').forEach((btn) => {
        btn.addEventListener('click', () => {
          const id = btn.getAttribute('data-action');
          const row = rows.find((r) => r.id === id);
          if (!row || row.disabled) return;
          if (row.closeOnRun) closePalette(palette);
          row.run();
          if (!row.closeOnRun) renderPalette(palette, root, prefs);
        });
      });
    };

    const mountQuickTools = (root = document, prefs) => {
      if (document.body?.dataset?.editor === 'home') return;
      const host = root.querySelector('.editor-page-header .editor-page-actions')
        || document.querySelector('.editor-page-header .editor-page-actions')
        || root.querySelector('.editor-page-actions')
        || document.querySelector('.editor-page-actions');
      if (!host || host.querySelector('[data-editor-quick-tools="1"]')) return;
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'editor-btn editor-btn-secondary editor-tools-btn';
      btn.setAttribute('data-editor-quick-tools', '1');
      btn.title = 'Quick actions and view mode (Ctrl+K)';
      btn.innerHTML = '<i class="fa-solid fa-sliders"></i><span>View</span><span class="editor-tools-dot"></span>';
      host.appendChild(btn);
      syncToolsButtonState(prefs);

      const palette = mountPalette(root);
      const close = () => closePalette(palette);
      const open = () => {
        renderPalette(palette, root, prefs);
        palette.classList.remove('hidden');
        palette.classList.add('flex');
      };
      btn.addEventListener('click', open);
      palette.querySelector('[data-role="close"]')?.addEventListener('click', close);
      palette.addEventListener('click', (e) => {
        if (e.target === palette) close();
      });
    };

    const isEditableTarget = (el) => {
      if (!el) return false;
      const tag = String(el.tagName || '').toLowerCase();
      return tag === 'input' || tag === 'textarea' || tag === 'select' || !!el.isContentEditable;
    };

    // ── Навигация оболочки изнутри редактора ──────────────────────────────
    // Редактор живёт в iframe, поэтому горячие клавиши студии (Ctrl+Tab,
    // Alt+1…9, история Alt+←/→) сами до оболочки не доходят: при активной
    // работе фокус почти всегда в редакторе, и переключение «не работает»,
    // пока не щёлкнешь шапку студии. Пересылаем эти команды родителю —
    // оболочка принимает их только от своих кадров (editor-shell:navigate).
    const inShellFrame = () => {
      try { return !!window.parent && window.parent !== window; } catch { return false; }
    };

    const forwardToShell = (action, extra = {}) => {
      if (!inShellFrame()) return false;
      const http = window.EditorCore?.Http;
      if (!http?.postMessage) return false;
      return http.postMessage(window.parent, { type: 'editor-shell:navigate', action, ...extra });
    };

    // ── Навигация по списку записей ───────────────────────────────────────
    // Один маленький блок над списком каждого редактора: предыдущая и
    // следующая запись кнопками и счётчик «N / M» — видно, где вы в списке,
    // и не нужно целиться мышью в соседнюю строку при массовой правке.
    // Плюс поведение списка как в файловых менеджерах: ↑/↓/Home/End/PgUp/
    // PgDn, когда фокус в списке; Tab заходит в список один раз (roving
    // tabindex), а не идёт по каждой записи; активная строка сама
    // подкручивается в видимую область; фокус не теряется, когда редактор
    // перерисовывает список после выбора строки.
    const recordNav = { sync: null };

    const mountRecordNav = (root = document) => {
      if (document.body?.dataset?.editor === 'home') return;
      const list = findActions(root).list;
      if (!list || list.dataset.recordNav === '1' || !list.parentElement) return;
      list.dataset.recordNav = '1';

      // Страница со своей контекстной навигацией (дерево квестов: полоса
      // «квест/шаги/цепочка» в тулбаре) помечает список data-record-nav-off:
      // общая планка «вверх/вниз + N/M» ей не нужна и только дублирует
      // управление, а клавиатура, roving tabindex и возврат фокуса остаются.
      const customNav = !!list.closest?.('[data-record-nav-off]');

      const bar = customNav ? null : document.createElement('div');
      if (bar) {
        bar.className = 'editor-record-nav';
        bar.setAttribute('data-editor-record-nav', '1');
        bar.innerHTML = `
        <button type="button" class="editor-icon-btn editor-record-nav-btn" data-record-nav="prev" title="Previous record (Alt+↑)" aria-label="Previous record"><i class="fa-solid fa-chevron-up" aria-hidden="true"></i></button>
        <button type="button" class="editor-icon-btn editor-record-nav-btn" data-record-nav="next" title="Next record (Alt+↓)" aria-label="Next record"><i class="fa-solid fa-chevron-down" aria-hidden="true"></i></button>
        <span class="editor-record-nav-pos" data-record-nav-pos></span>`;
        list.parentElement.insertBefore(bar, list);
      }

      const prevBtn = bar?.querySelector('[data-record-nav="prev"]') || null;
      const nextBtn = bar?.querySelector('[data-record-nav="next"]') || null;
      const posEl = bar?.querySelector('[data-record-nav-pos]') || null;

      let lastActiveKey = null;
      let hadFocus = false;
      let syncQueued = false;

      const rowKey = (row, index) => {
        const id = row?.getAttribute?.('data-id');
        return id != null && id !== '' ? `${row.getAttribute('data-node-type') || 'row'}:${id}` : `i:${index}`;
      };

      const focusActiveRow = () => {
        const rows = listRows(list);
        const idx = activeRowIndex(rows);
        if (idx >= 0) rows[idx].focus?.({ preventScroll: true });
      };

      const sync = () => {
        const rows = listRows(list);
        const idx = activeRowIndex(rows);
        bar?.classList.toggle('hidden', !rows.length);
        const pos = idx >= 0 ? `${idx + 1} / ${rows.length}` : `— / ${rows.length}`;
        if (posEl && posEl.textContent !== pos) posEl.textContent = pos;
        if (prevBtn) prevBtn.disabled = idx <= 0;
        if (nextBtn) nextBtn.disabled = idx >= 0 && idx >= rows.length - 1;
        // Tab входит в список один раз — на активную (или первую) строку.
        const tabStop = idx >= 0 ? idx : 0;
        rows.forEach((row, i) => {
          const value = i === tabStop ? '0' : '-1';
          if (row.getAttribute?.('tabindex') !== value) row.setAttribute?.('tabindex', value);
        });
        const key = idx >= 0 ? rowKey(rows[idx], idx) : null;
        if (key !== null && key !== lastActiveKey) rows[idx].scrollIntoView?.({ block: 'nearest' });
        lastActiveKey = key;
        // Перерисовка списка (innerHTML) уничтожает сфокусированную строку
        // и фокус падает на body — возвращаем его на активную строку, чтобы
        // стрелки продолжали работать без лишнего клика.
        if (hadFocus && idx >= 0 && !list.contains(document.activeElement)) focusActiveRow();
      };

      const queueSync = () => {
        if (syncQueued) return;
        syncQueued = true;
        const run = () => { syncQueued = false; sync(); };
        if (typeof window.requestAnimationFrame === 'function') window.requestAnimationFrame(run);
        else setTimeout(run, 0);
      };
      recordNav.sync = queueSync;

      prevBtn?.addEventListener('click', () => { moveSelectionBy(list, -1); queueSync(); });
      nextBtn?.addEventListener('click', () => { moveSelectionBy(list, +1); queueSync(); });

      list.addEventListener('focusin', () => { hadFocus = true; });
      list.addEventListener('focusout', (event) => {
        // Фокус ушёл на конкретный элемент вне списка — пользователь дальше
        // работает там, фокус не возвращаем. relatedTarget === null обычно
        // означает, что сфокусированная строка удалена перерисовкой.
        if (event.relatedTarget && !list.contains(event.relatedTarget)) hadFocus = false;
      });
      document.addEventListener('pointerdown', (event) => {
        if (!list.contains(event.target) && !bar?.contains(event.target)) hadFocus = false;
      }, true);
      list.addEventListener('click', () => queueSync());

      list.addEventListener('keydown', (event) => {
        if (event.ctrlKey || event.metaKey || event.altKey) return;
        if (isEditableTarget(event.target)) return;
        const rowEl = event.target?.closest?.(ROW_SELECTOR);
        if (!rowEl || !list.contains(rowEl)) return;
        const jump = { ArrowDown: 1, ArrowUp: -1, PageDown: 10, PageUp: -10 }[event.key];
        if (jump) {
          if (moveSelectionBy(list, jump)) {
            event.preventDefault();
            queueSync();
            setTimeout(focusActiveRow, 0);
          }
          return;
        }
        if (event.key === 'Home' || event.key === 'End') {
          if (moveSelectionTo(list, event.key === 'Home' ? 0 : listRows(list).length - 1)) {
            event.preventDefault();
            queueSync();
            setTimeout(focusActiveRow, 0);
          }
          return;
        }
        // Строка-не-кнопка (узел дерева) сама по Enter не «нажимается».
        if ((event.key === 'Enter' || event.key === ' ') && event.target === rowEl) {
          const tag = String(rowEl.tagName || '').toLowerCase();
          if (tag !== 'button' && tag !== 'a') {
            event.preventDefault();
            clickRow(rowEl);
            queueSync();
          }
        }
      });

      if (typeof MutationObserver === 'function') {
        new MutationObserver(queueSync).observe(list, {
          childList: true, subtree: true, attributes: true, attributeFilter: ['class'],
        });
      }
      sync();
    };

    const bindShortcuts = (root = document, prefs) => {
      if (root.__editorShortcutsBound === true) return;
      root.__editorShortcutsBound = true;

      document.addEventListener('keydown', (e) => {
        const key = String(e.key || '').toLowerCase();
        const ctrl = e.ctrlKey || e.metaKey;
        const actions = findActions(root);

        // Команды оболочки студии: сами по себе они внутри iframe не
        // срабатывают, пересылаем их родителю (см. forwardToShell).
        if (e.ctrlKey && e.key === 'Tab') {
          if (forwardToShell('cycle-tab', { dir: e.shiftKey ? -1 : 1 })) e.preventDefault();
          return;
        }
        if (ctrl && e.shiftKey && key === 't') {
          if (forwardToShell('reopen-tab')) e.preventDefault();
          return;
        }
        // Alt+цифра и Alt+стрелки влево/вправо не отнимаем у полей ввода:
        // там Alt+←/→ — переход по словам. Ctrl+Tab в полях безопасен.
        if (!isEditableTarget(e.target)) {
          if (e.altKey && /^[1-9]$/.test(e.key)) {
            if (forwardToShell('tab-index', { index: Number(e.key) - 1 })) e.preventDefault();
            return;
          }
          if (e.altKey && e.key === 'ArrowLeft') {
            if (forwardToShell('history-back')) e.preventDefault();
            return;
          }
          if (e.altKey && e.key === 'ArrowRight') {
            if (forwardToShell('history-forward')) e.preventDefault();
            return;
          }
          if (ctrl && !e.altKey && key === 'b' && !e.target?.isContentEditable) {
            if (forwardToShell('toggle-sidebar')) e.preventDefault();
            return;
          }
        }

        // Ctrl+S is handled centrally by Help module when loaded; fallback only if Help is absent.
        if (ctrl && !e.altKey && key === 's') {
          if (document.body?.dataset?.ownSaveShortcut === '1' || window.EditorCore?.Help) return;
          e.preventDefault();
          clickIfEnabled(actions.save);
          return;
        }
        if (e.altKey && !ctrl && key === 'n') {
          if (clickIfEnabled(actions.newItem)) e.preventDefault();
          return;
        }
        if (e.altKey && !ctrl && key === 'r') {
          if (clickIfEnabled(actions.refresh)) e.preventDefault();
          return;
        }
        if (!ctrl && !e.altKey && e.key === '/' && !isEditableTarget(e.target)) {
          if (focusSearch(actions.search)) e.preventDefault();
          return;
        }
        if (ctrl && key === 'k') {
          e.preventDefault();
          const palette = mountPalette(root);
          if (!palette.classList.contains('hidden')) {
            closePalette(palette);
            return;
          }
          renderPalette(palette, root, prefs);
          palette.classList.remove('hidden');
          palette.classList.add('flex');
          return;
        }
        if (e.altKey && key === 'arrowdown') {
          if (moveSelection(actions.list, +1)) { e.preventDefault(); recordNav.sync?.(); }
          return;
        }
        if (e.altKey && key === 'arrowup') {
          if (moveSelection(actions.list, -1)) { e.preventDefault(); recordNav.sync?.(); }
          return;
        }
        if (key === 'escape') {
          const palette = document.getElementById('editor-command-palette');
          if (palette && !palette.classList.contains('hidden')) {
            e.preventDefault();
            closePalette(palette);
            return;
          }
        }
      });
    };

    const init = (root = document) => {
      const body = document.body;
      // Do not bind global DOM helpers while the document has no body yet.
      if (!body || body.dataset.editorProductivityKit === '1') return;
      body.dataset.editorProductivityKit = '1';
      const prefs = getPrefs();
      setMode('compact', prefs.compact, prefs);
      setMode('focus', prefs.focus, prefs);
      mountQuickTools(root, prefs);
      mountRecordNav(root);
      bindShortcuts(root, prefs);
    };

    return {
      init,
      getPrefs,
      setMode: (mode, enabled) => setMode(mode, enabled, getPrefs()),
      // Перемещение по списку записей: палитра, клавиши и тесты ходят сюда.
      moveSelection,
      moveSelectionBy,
      moveSelectionTo,
      listRows,
      activeRowIndex,
      forwardToShell,
    };
  })();

  const validateValue = ({ value, spec, inputType }) => {
    if (!spec) return { ok: true, message: '' };
    const message = spec.message || 'Invalid value';

    // Required
    if (spec.required) {
      const empty = value == null || value === '' || (Array.isArray(value) && value.length === 0);
      if (empty) return { ok: false, message: spec.requiredMessage || 'Required field' };
    }

    // Number bounds
    if (inputType === 'number' || typeof value === 'number') {
      const num = typeof value === 'number' ? value : (value === '' ? NaN : Number(value));
      if (Number.isFinite(num)) {
        if (spec.min !== undefined && num < spec.min) return { ok: false, message: spec.minMessage || `Minimum:  ${spec.min}` };
        if (spec.max !== undefined && num > spec.max) return { ok: false, message: spec.maxMessage || `Maximum:  ${spec.max}` };
      }
    }

    // Pattern
    if (spec.pattern) {
      try {
        const re = new RegExp(spec.pattern);
        if (!re.test(String(value ?? ''))) return { ok: false, message };
      } catch {
        // ignore broken regex
      }
    }

    return { ok: true, message: '' };
  };

  const UIManager = {
    async init(root = document) {
      // Scripts in <head> can initialize us before the page has a body.
      // Wait for the whole DOM so toolbar actions and fields are present too.
      if (document.readyState === 'loading') {
        await new Promise(resolve => document.addEventListener('DOMContentLoaded', resolve, { once: true }));
      }
      // 0) TagSelect widgets (chips + search)
      // Reusable across editors:
      // - static options via ui.options
      // - optional DB-backed options via data-tags-datasource (same source as DB Select)
      // Value is stored in a hidden input (data-value-type json_array / json_array_number).

      const safeJson = (raw, fallback) => {
        if (!raw) return fallback;
        try { return JSON.parse(raw); } catch { return fallback; }
      };

      const escapeHtml = (...args) => window.EditorCore.utils.escapeHtml(...args);

      const normalizeOptions = (raw) => {
        const arr = Array.isArray(raw) ? raw : [];
        return arr
          .map((o) => {
            if (o && typeof o === 'object') {
              return { value: String(o.value ?? o.label ?? ''), label: String(o.label ?? o.value ?? '') };
            }
            return { value: String(o), label: String(o) };
          })
          .filter((o) => o.value);
      };

      const initTagsWidget = (widget) => {
        if (!widget || widget.dataset.tagsInited === '1') return;
        widget.dataset.tagsInited = '1';

        const hidden = widget.querySelector('input[type="hidden"][data-path]');
        const selectedWrap = widget.querySelector('.editor-tags-selected');
        const optionsWrap = widget.querySelector('.editor-tags-options');
        const search = widget.querySelector('.editor-tags-search');

        const valueType = (widget.dataset.tagsValueType || 'string').toLowerCase();
        const allowCreate = widget.dataset.tagsAllowCreate === '1';

        const coerce = (v) => {
          if (valueType === 'number') {
            const n = Number(v);
            return Number.isFinite(n) ? n : null;
          }
          const s = String(v ?? '').trim();
          return s ? s : null;
        };

        const readValue = () => {
          const v = window.EditorCore?.FieldRenderer?.getInputValue
            ? window.EditorCore.FieldRenderer.getInputValue(hidden)
            : [];
          if (!Array.isArray(v)) return [];
          return v
            .map((x) => (valueType === 'number' ? String(x) : String(x)))
            .filter(Boolean);
        };

        const writeValue = (arr) => {
          if (!hidden) return;
          const uniq = Array.from(
            new Set((Array.isArray(arr) ? arr : []).map((x) => String(x)).filter(Boolean))
          );
          // Keep numeric arrays numeric for storage if requested
          const payload = valueType === 'number'
            ? uniq.map((x) => Number(x)).filter((n) => Number.isFinite(n))
            : uniq;
          hidden.value = JSON.stringify(payload);
          hidden.dispatchEvent(new Event('input', { bubbles: true }));
          hidden.dispatchEvent(new Event('change', { bubbles: true }));
        };

        const getAllOptions = () => normalizeOptions(safeJson(widget.dataset.tagsOptions, []));

        const pickLabel = (allOptions, v) => {
          const key = String(v).toLowerCase();
          const opt = allOptions.find((o) => String(o.value).toLowerCase() === key);
          return opt ? opt.label : String(v);
        };

        const state = { activeIndex: -1 };

        const render = () => {
          if (!hidden || !selectedWrap || !optionsWrap) return;

          const allOptions = getAllOptions();
          const qRaw = String(search?.value || '').trim();
          const q = qRaw.toLowerCase();
          const selected = readValue();
          const selectedSet = new Set(selected.map((x) => String(x).toLowerCase()));

          // Selected chips
          if (!selected.length) {
            selectedWrap.innerHTML = `<div class="editor-tags-empty">Nothing selected</div>`;
          } else {
            selectedWrap.innerHTML = selected
              .map((v) => {
                const label = pickLabel(allOptions, v);
                return `
                  <button type="button" class="editor-tag" data-tag-remove="${escapeHtml(v)}" title="Remove ">
                    <span class="editor-tag-label">${escapeHtml(label)}</span>
                    <span class="editor-tag-x">×</span>
                  </button>`;
              })
              .join('');
          }

          // Options list
          const filtered = allOptions.filter((o) => {
            const valKey = String(o.value).toLowerCase();
            if (selectedSet.has(valKey)) return false;
            if (!q) return true;
            return String(o.label).toLowerCase().includes(q) || valKey.includes(q);
          });

          let extraCreate = '';
          if (allowCreate && qRaw) {
            const exists = allOptions.some((o) => {
              const v = String(o.value).toLowerCase();
              const l = String(o.label).toLowerCase();
              return v === q || l === q;
            });
            if (!exists) {
              extraCreate = `
                <button type="button" class="editor-tag-option" data-tag-add="${escapeHtml(qRaw)}" data-tag-option-index="0">
                  + Add: <span class="editor-tag-option-strong">${escapeHtml(qRaw)}</span>
                </button>`;
            }
          }

          const btns = filtered
            .map((o, idx) => {
              const i = (extraCreate ? 1 : 0) + idx;
              return `
                <button type="button" class="editor-tag-option" data-tag-add="${escapeHtml(o.value)}" data-tag-option-index="${i}">
                  ${escapeHtml(o.label)}
                </button>`;
            })
            .join('');

          const emptyHtml = (!extraCreate && !filtered.length)
            ? `<div class="editor-tags-empty">No options</div>`
            : '';

          optionsWrap.innerHTML = `
            <div class="editor-tags-options-inner">
              ${extraCreate}
              ${btns}
              ${emptyHtml}
            </div>`;

          // Active option styling (keyboard)
          const optionButtons = Array.from(optionsWrap.querySelectorAll('[data-tag-add]'));
          optionButtons.forEach((b) => b.classList.remove('active'));
          if (state.activeIndex >= 0 && optionButtons[state.activeIndex]) {
            optionButtons[state.activeIndex].classList.add('active');
          }

          // Bind remove/add
          selectedWrap.querySelectorAll('[data-tag-remove]').forEach((btn) => {
            btn.addEventListener('click', () => {
              const v = btn.getAttribute('data-tag-remove');
              writeValue(readValue().filter((x) => String(x).toLowerCase() !== String(v).toLowerCase()));
              render();
            });
          });

          optionButtons.forEach((btn) => {
            btn.addEventListener('click', () => {
              const raw = String(btn.getAttribute('data-tag-add') || '').trim();
              const coerced = coerce(raw);
              if (coerced == null) return;
              writeValue(readValue().concat([String(coerced)]));
              if (search) search.value = '';
              state.activeIndex = -1;
              render();
              if (search) search.focus();
            });
          });
        };

        // Keyboard UX: arrows to navigate options, Enter to add, Backspace to remove last.
        if (search) {
          search.addEventListener('input', () => {
            state.activeIndex = -1;
            render();
          });
          search.addEventListener('change', () => {
            state.activeIndex = -1;
            render();
          });
          search.addEventListener('keydown', (e) => {
            const optionsWrapLocal = widget.querySelector('.editor-tags-options');
            const optionButtons = Array.from(optionsWrapLocal?.querySelectorAll('[data-tag-add]') || []);

            if (e.key === 'ArrowDown') {
              if (!optionButtons.length) return;
              e.preventDefault();
              state.activeIndex = Math.min(optionButtons.length - 1, state.activeIndex + 1);
              render();
              optionButtons[state.activeIndex]?.scrollIntoView?.({ block: 'nearest' });
              return;
            }
            if (e.key === 'ArrowUp') {
              if (!optionButtons.length) return;
              e.preventDefault();
              state.activeIndex = Math.max(0, state.activeIndex - 1);
              render();
              optionButtons[state.activeIndex]?.scrollIntoView?.({ block: 'nearest' });
              return;
            }
            if (e.key === 'Enter') {
              if (!optionButtons.length) return;
              e.preventDefault();
              const btn = optionButtons[state.activeIndex >= 0 ? state.activeIndex : 0];
              btn?.click();
              return;
            }
            if (e.key === 'Backspace') {
              const raw = String(search.value || '');
              if (raw.trim() !== '') return;
              const cur = readValue();
              if (!cur.length) return;
              e.preventDefault();
              writeValue(cur.slice(0, cur.length - 1));
              render();
              return;
            }
            if (e.key === 'Escape') {
              if (search.value) {
                e.preventDefault();
                search.value = '';
                state.activeIndex = -1;
                render();
              }
            }
          });
        }

        // Expose a tiny API for late option hydration (e.g. from DB)
        widget._tagsRender = render;
        widget._tagsSetOptions = (opts) => {
          widget.dataset.tagsOptions = JSON.stringify(opts || []);
          state.activeIndex = -1;
          render();
        };

        render();
      };

      // init all tag widgets
      root.querySelectorAll('.editor-tags').forEach(initTagsWidget);

      // Hydrate options from DB if requested
      const hydrateTagsFromDB = async () => {
        const widgets = Array.from(root.querySelectorAll('.editor-tags[data-tags-datasource]'));
        if (!widgets.length) return;
        const DB = window.EditorCore?.DB;
        if (!DB?.list) return;

        const bySource = new Map();
        for (const w of widgets) {
          const source = String(w.dataset.tagsDatasource || '').trim();
          if (!source) continue;
          if (!bySource.has(source)) bySource.set(source, []);
          bySource.get(source).push(w);
        }

        const PAGE = 1000;
        const CAP = 5000;

        for (const [source, ws] of bySource.entries()) {
          // show lightweight loading marker
          ws.forEach((w) => {
            w.dataset.tagsDbState = 'loading';
          });

          let items = [];
          let offset = 0;
          let ok = true;
          while (true) {
            const res = await DB.list(source, { limit: PAGE, offset });
            if (!res?.ok) { ok = false; break; }
            const page = res.items || [];
            items = items.concat(page);
            if (!res.has_more) break;
            offset += page.length;
            if (items.length >= CAP) break;
            if (!page.length) break;
          }

          if (!ok) {
            ws.forEach((w) => {
              w.dataset.tagsDbState = 'disconnected';
              // keep static options (if any)
              w._tagsRender?.();
            });
            continue;
          }

          // apply label mode formatting per widget
          ws.forEach((w) => {
            const labelMode = String(w.dataset.tagsLabelMode || 'id_name').toLowerCase();
            const opts = (items || []).map((it) => {
              const val = String(it.value);
              const base = String(it.label || '');
              const label = labelMode === 'name' ? base : `${val}: ${base}`;
              return { value: val, label };
            });
            w.dataset.tagsDbState = 'connected';
            w._tagsSetOptions?.(opts);
          });
        }
      };

      // Fire and forget inside init (awaited so first render gets real labels if DB is fast)
      await hydrateTagsFromDB();

      // 0.1) Scenario field launcher (opens embedded scenario editor overlay)
      // `root` may be a document (bootstrap calls UIManager.init(document)).
      if (root?.dataset && root.dataset.scenarioOverlayBound !== '1') {
        root.dataset.scenarioOverlayBound = '1';
        root.addEventListener('click', (event) => {
          const btn = event.target?.closest?.('[data-action="open-scenario-editor"]');
          if (!btn) return;
          const field = btn.closest('[data-scenario-field="1"]');
          if (!field) return;

          const hidden = field.querySelector('input[type="hidden"][data-scenario-value="1"]');
          if (!hidden) return;

          const requestedMode = String(field.dataset.scenarioMode || 'universal').toLowerCase();
          const mode = requestedMode === 'dungeon' || requestedMode === 'world'
            ? requestedMode
            : 'universal';

          const fieldLabel = field.dataset.scenarioLabel || 'Scenario';

          // Мир записи: компоненты сценария стоят в мире предмета/подземелья/
          // квеста, и карта «Местоположения» должна открыться именно в нём.
          const worldPath = String(field.dataset.mapWorldPath || '').trim();
          const worldNames = worldPath
            ? [worldPath]
            : ['WorldID', 'DataScenarioWorldID', 'world_id', 'WorldId', 'worldId'];
          const readWorldId = () => {
            const ancestors = new Map();
            let node = field;
            let depth = 0;
            while (node) {
              ancestors.set(node, depth++);
              node = node.parentElement;
            }
            const distance = (candidate) => {
              node = candidate;
              let candidateDepth = 0;
              while (node) {
                if (ancestors.has(node)) return ancestors.get(node) + candidateDepth;
                candidateDepth++;
                node = node.parentElement;
              }
              return Number.POSITIVE_INFINITY;
            };
            for (const name of worldNames) {
              const candidates = Array.from(document.querySelectorAll(`[data-path="${name}"]`))
                .filter((input) => input !== field && !field.contains(input))
                .sort((a, b) => distance(a) - distance(b));
              if (candidates.length) {
                const raw = candidates[0].value;
                if (raw === null || raw === undefined || String(raw).trim() === '') return null;
                const value = Number(raw);
                return Number.isSafeInteger(value) && value >= 0 ? value : null;
              }
            }
            return 0;
          };

          window.EditorCore?.UI?.openScenarioFieldOverlay?.({
            title: `Editing:  ${fieldLabel}`,
            mode,
            worldId: readWorldId(),
            scenarioText: String(hidden.value || ''),
            onApply: (scenarioText) => {
              hidden.value = String(scenarioText || '');
              hidden.dispatchEvent(new Event('input', { bubbles: true }));
              hidden.dispatchEvent(new Event('change', { bubbles: true }));

              const status = field.querySelector('[data-scenario-status]');
              if (status) {
                const hasValue = String(hidden.value || '').trim().length > 0;
                status.textContent = hasValue ? 'JSON set' : 'JSON not set';
              }
            }
          });
        });
      }

      // 1) Dynamic DB selects
      if (window.EditorCore?.DB?.init) {
        await window.EditorCore.DB.init(root);
      }

      // 1.1) Adaptive DB selects: toggle helper dropdown vs manual input
      root.querySelectorAll('.editor-dbselect').forEach((wrapper) => {
        const sel = wrapper.querySelector('select[data-datasource]');
        if (!sel) {
          wrapper.dataset.dbState = 'disconnected';
          return;
        }
        const failed = sel.dataset.dbFailed === '1';
        if (failed) {
          wrapper.dataset.dbState = 'disconnected';
          return;
        }

        // Searchable selects load incrementally; treat them as "connected" once DB.init has run.
        if (sel.dataset.dbSearchable === '1') {
          wrapper.dataset.dbState = 'connected';
          return;
        }

        // Bulk selects set dbLoaded. As an extra guard, consider it loaded if it has options.
        const loaded = sel.dataset.dbLoaded === '1' || (sel.options && sel.options.length > (sel.multiple ? 0 : 1));
        wrapper.dataset.dbState = loaded ? 'connected' : 'disconnected';
      });

      // 2) Toggle state sync (for CSS styling)
      root.querySelectorAll('.editor-toggle input[type=checkbox]').forEach((cb) => {
        const label = cb.closest('.editor-toggle');
        const sync = () => { if (label) label.dataset.checked = cb.checked ? '1' : '0'; };
        cb.addEventListener('change', sync);
        sync();
      });

      // 2) Validation hints
      const inputs = Array.from(root.querySelectorAll('[data-validate]'));
      for (const el of inputs) {
        const spec = parseValidateSpec(el.getAttribute('data-validate'));
        if (!spec) continue;
        const path = el.getAttribute('data-path') || el.getAttribute('name') || el.getAttribute('data-key');
        const hint = path ? root.querySelector(`[data-hint-for="${cssEscape(path)}"]`) : null;

        const run = () => {
          const value = window.EditorCore?.FieldRenderer?.getInputValue
            ? window.EditorCore.FieldRenderer.getInputValue(el)
            : (el.type === 'checkbox' ? el.checked : el.value);

          const { ok, message } = validateValue({ value, spec, inputType: el.type });
          el.classList.toggle('editor-invalid', !ok);
          setHint(hint, ok ? '' : message);
        };

        el.addEventListener('input', run);
        el.addEventListener('change', run);
        run();
      }

      // 3) Global productivity utilities (hotkeys + command palette + view modes)
      Productivity.init(root);
    },
    getPrefs: () => Productivity.getPrefs(),
    setMode: (mode, enabled) => Productivity.setMode(mode, enabled),
    moveSelection: (...args) => Productivity.moveSelection(...args),
    moveSelectionBy: (...args) => Productivity.moveSelectionBy(...args),
    moveSelectionTo: (...args) => Productivity.moveSelectionTo(...args),
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.UIManager = UIManager;
})();
