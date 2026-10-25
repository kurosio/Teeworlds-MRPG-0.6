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

    const moveSelection = (listRoot, direction = 1) => {
      if (!listRoot) return false;
      const rows = Array.from(listRoot.querySelectorAll('.editor-row, .quest-node, .step-node'))
        .filter((el) => el.offsetParent !== null || el.getClientRects?.().length);
      if (!rows.length) return false;
      const active = rows.findIndex((r) => (
        r.classList.contains('editor-row-active')
        || r.classList.contains('tree-selected')
        || r.classList.contains('ring-2')
      ));
      const next = Math.max(0, Math.min(rows.length - 1, active < 0 ? 0 : active + direction));
      const target = rows[next]?.querySelector?.('.quest-node-title, .step-node-header') || rows[next];
      target?.click?.();
      rows[next]?.scrollIntoView?.({ block: 'nearest' });
      return true;
    };

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
                <span>Быстрые действия и вид</span>
              </div>
              <div class="editor-muted-text text-xs mt-1">Горячие клавиши: Ctrl+S сохранить · Alt+N создать · / поиск · Ctrl+K палитра</div>
            </div>
            <button type="button" class="editor-icon-btn" data-role="close" title="Закрыть (Esc)">
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
          label: 'Сохранить изменения',
          kbd: 'Ctrl+S',
          disabled: !actions.save || actions.save.disabled || actions.save.classList?.contains('hidden'),
          closeOnRun: true,
          run: () => clickIfEnabled(actions.save),
        },
        {
          id: 'new',
          icon: 'fa-plus',
          label: 'Создать новую запись',
          kbd: 'Alt+N',
          disabled: !actions.newItem || actions.newItem.disabled || actions.newItem.classList?.contains('hidden'),
          closeOnRun: true,
          run: () => clickIfEnabled(actions.newItem),
        },
        {
          id: 'refresh',
          icon: 'fa-rotate',
          label: 'Обновить список',
          kbd: 'Alt+R',
          disabled: !actions.refresh || actions.refresh.disabled,
          closeOnRun: true,
          run: () => clickIfEnabled(actions.refresh),
        },
        {
          id: 'search',
          icon: 'fa-magnifying-glass',
          label: 'Перейти к поиску',
          kbd: '/',
          disabled: !actions.search,
          closeOnRun: true,
          run: () => focusSearch(actions.search),
        },
        {
          id: 'prev',
          icon: 'fa-arrow-up',
          label: 'Предыдущая запись в списке',
          kbd: 'Alt+↑',
          disabled: !actions.list,
          closeOnRun: true,
          run: () => moveSelection(actions.list, -1),
        },
        {
          id: 'next',
          icon: 'fa-arrow-down',
          label: 'Следующая запись в списке',
          kbd: 'Alt+↓',
          disabled: !actions.list,
          closeOnRun: true,
          run: () => moveSelection(actions.list, +1),
        },
        {
          id: 'compact',
          icon: 'fa-compress',
          label: prefs.compact ? 'Компактный режим: включён (выключить)' : 'Компактный режим: выключен (включить)',
          kbd: prefs.compact ? 'ВКЛ' : 'ВЫКЛ',
          closeOnRun: false,
          run: () => setMode('compact', !prefs.compact, prefs),
        },
        {
          id: 'focus',
          icon: 'fa-expand',
          label: prefs.focus ? 'Режим фокуса без списка: включён (показать список)' : 'Режим фокуса: скрыть боковой список',
          kbd: prefs.focus ? 'ВКЛ' : 'ВЫКЛ',
          closeOnRun: false,
          run: () => setMode('focus', !prefs.focus, prefs),
        },
        {
          id: 'theme',
          icon: curTheme === 'dark' ? 'fa-sun' : 'fa-moon',
          label: curTheme === 'dark' ? 'Переключить на светлую тему' : 'Переключить на тёмную тему',
          kbd: curTheme === 'dark' ? 'Тёмная' : 'Светлая',
          closeOnRun: false,
          run: () => window.EditorCoreTheme?.toggleTheme?.(),
        },
        {
          id: 'help',
          icon: 'fa-circle-question',
          label: 'Открыть справку по редактору',
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
      btn.title = 'Быстрые действия и режим вида (Ctrl+K)';
      btn.innerHTML = '<i class="fa-solid fa-sliders"></i><span>Вид</span><span class="editor-tools-dot"></span>';
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

    const bindShortcuts = (root = document, prefs) => {
      if (root.__editorShortcutsBound === true) return;
      root.__editorShortcutsBound = true;

      document.addEventListener('keydown', (e) => {
        const key = String(e.key || '').toLowerCase();
        const ctrl = e.ctrlKey || e.metaKey;
        const actions = findActions(root);

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
          if (moveSelection(actions.list, +1)) e.preventDefault();
          return;
        }
        if (e.altKey && key === 'arrowup') {
          if (moveSelection(actions.list, -1)) e.preventDefault();
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
      bindShortcuts(root, prefs);
    };

    return { init, getPrefs, setMode: (mode, enabled) => setMode(mode, enabled, getPrefs()) };
  })();

  const validateValue = ({ value, spec, inputType }) => {
    if (!spec) return { ok: true, message: '' };
    const message = spec.message || 'Некорректное значение';

    // Required
    if (spec.required) {
      const empty = value == null || value === '' || (Array.isArray(value) && value.length === 0);
      if (empty) return { ok: false, message: spec.requiredMessage || 'Обязательное поле' };
    }

    // Number bounds
    if (inputType === 'number' || typeof value === 'number') {
      const num = typeof value === 'number' ? value : (value === '' ? NaN : Number(value));
      if (Number.isFinite(num)) {
        if (spec.min !== undefined && num < spec.min) return { ok: false, message: spec.minMessage || `Минимум: ${spec.min}` };
        if (spec.max !== undefined && num > spec.max) return { ok: false, message: spec.maxMessage || `Максимум: ${spec.max}` };
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
            selectedWrap.innerHTML = `<div class="editor-tags-empty">Ничего не выбрано</div>`;
          } else {
            selectedWrap.innerHTML = selected
              .map((v) => {
                const label = pickLabel(allOptions, v);
                return `
                  <button type="button" class="editor-tag" data-tag-remove="${escapeHtml(v)}" title="Убрать">
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
                  + Добавить: <span class="editor-tag-option-strong">${escapeHtml(qRaw)}</span>
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
            ? `<div class="editor-tags-empty">Нет вариантов</div>`
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

          const fieldLabel = field.dataset.scenarioLabel || 'Сценарий';

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
            title: `Редактирование: ${fieldLabel}`,
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
                status.textContent = hasValue ? 'JSON задан' : 'JSON не задан';
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
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.UIManager = UIManager;
})();
