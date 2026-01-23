(() => {
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
      // 0) TagSelect widgets (chips + search)
      // Reusable across editors:
      // - static options via ui.options
      // - optional DB-backed options via data-tags-datasource (same source as DB Select)
      // Value is stored in a hidden input (data-value-type json_array / json_array_number).

      const safeJson = (raw, fallback) => {
        if (!raw) return fallback;
        try { return JSON.parse(raw); } catch { return fallback; }
      };

      const escapeHtml = (s) => String(s)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');

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
        const hint = path ? root.querySelector(`[data-hint-for="${CSS.escape(path)}"]`) : null;

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
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.UIManager = UIManager;
})();
