(() => {
  const cssEscape = (...args) => window.EditorCore?.utils?.cssEscape?.(...args) ?? String(args[0] ?? '');
  const DEFAULT_ENDPOINT = 'api/db.php';
  const MAINT_ENDPOINT = 'api/db-maintenance.php';

  /**
   * Ответ сервера в форме, привычной этому модулю: `{ ok, error, ...поля }`.
   * Сеть, таймаут и 401 обрабатывает editor-core/http.js.
   */
  const fetchJson = async (url, options = {}) => {
    const res = await window.EditorCore.Http.json(url, options);
    if (!res.ok) return { ok: false, error: res.error, details: res.data };
    return res.data ?? { ok: true };
  };

  const cache = window.EditorCore.utils.createRequestCache({
    ttl: 5 * 60 * 1000,
    max: 200,
  });
  const cacheOptions = (signal) => ({
    dedupe: !signal,
    shouldCache: response => !!response?.ok,
  });

  const readFilter = (el) => {
    if (!el) return null;
    const key = String(el.dataset.dbFilterKey || '').trim();
    if (!key) return null;
    const rawValues = el.dataset.dbFilterValues;
    if (rawValues) {
      try {
        const parsed = JSON.parse(rawValues);
        if (Array.isArray(parsed)) {
          const values = parsed.map(v => String(v ?? '').trim()).filter(Boolean);
          if (values.length) return { key, values };
        }
      } catch {}
    }
    const value = String(el.dataset.dbFilterValue ?? '').trim();
    if (value === '') return null;
    return { key, values: [value] };
  };

  const getSelectedValue = (el, { bindInputPath, root }) => {
    if (bindInputPath) {
      const input = root.querySelector(`[data-path="${cssEscape(bindInputPath)}"]`);
      const v = input ? String(input.value || '') : '';
      return v === '0' ? '' : v;
    }
    // direct binding select
    const v = String(el.value || el.dataset.currentValue || '');
    return v === '0' ? '' : v;
  };

  const ensureCurrentOption = async (db, source, el, { root, bindInputPath, labelMode }) => {
    const cur = getSelectedValue(el, { bindInputPath, root });
    if (!cur) return;
    if (Array.from(el.options).some(o => o.value === cur)) return;

    const one = await db.getOne(source, cur);
    if (!one?.ok || !one.item) return;

    const val = String(one.item.value);
    const baseLabel = String(one.item.label || '');
    const label = labelMode === 'name' ? baseLabel : `${val}: ${baseLabel}`;
    const opt = new Option(label, val, false, false);
    el.appendChild(opt);
    if (el.multiple) {
      opt.selected = true;
    } else {
      el.value = val;
    }
  };

  const initSearchableSelect = async (db, source, el, root) => {
    const wrapper = el.closest('.editor-dbselect');
    if (wrapper) wrapper.setAttribute('data-db-state', 'connected');

    const searchInput = wrapper ? wrapper.querySelector('.editor-dbselect-search') : null;
    const moreBtn = wrapper ? wrapper.querySelector('.editor-dbselect-more') : null;
    const metaEl = wrapper ? wrapper.querySelector('.editor-dbselect-meta') : null;

    const bindInputPath = el.dataset.bindInputPath || '';
    const placeholder = el.dataset.placeholder || '— выберите —';
    const labelMode = (el.dataset.labelMode || 'id_name').toLowerCase();
    const pageSize = Number(el.dataset.dbLimit || 300);
    const multiple = !!el.multiple;
    const filter = readFilter(el);

    const state = { q: '', loaded: 0, total: null, hasMore: false };

    const updateMeta = () => {
      if (!metaEl) return;
      if (!state.loaded) { metaEl.textContent = ''; return; }
      if (state.total == null) metaEl.textContent = `Показано: ${state.loaded}`;
      else metaEl.textContent = `Показано: ${state.loaded} из ${state.total}`;
    };

    const renderOptions = (items, { append = false } = {}) => {
      const selectedVal = getSelectedValue(el, { bindInputPath, root });

      if (!append) {
        el.innerHTML = '';
        if (!multiple) el.appendChild(new Option(placeholder, '', false, !selectedVal));
      }

      const existing = new Set(Array.from(el.options, option => option.value));
      const fragment = document.createDocumentFragment();
      for (const it of (items || [])) {
        const val = String(it.value);
        if (existing.has(val)) continue;
        const baseLabel = String(it.label || '');
        const label = labelMode === 'name' ? baseLabel : `${val}: ${baseLabel}`;
        const opt = new Option(label, val);
        if (!multiple && selectedVal && val === String(selectedVal)) opt.selected = true;
        fragment.appendChild(opt);
        existing.add(val);
      }
      el.appendChild(fragment);
    };

    const syncSelectToInput = () => {
      if (!bindInputPath) return;
      const input = root.querySelector(`[data-path="${cssEscape(bindInputPath)}"]`);
      if (!input) return;
      input.value = (el.value === '' ? '0' : String(el.value));
      input.dispatchEvent(new Event('input', { bubbles: true }));
      input.dispatchEvent(new Event('change', { bubbles: true }));
    };

    const syncInputToSelect = () => {
      if (!bindInputPath) return;
      const input = root.querySelector(`[data-path="${cssEscape(bindInputPath)}"]`);
      if (!input) return;
      const v = String(input.value || '0');
      const has = Array.from(el.options).some(o => o.value === v);
      if (has) el.value = v;
      else if (v === '0') el.value = '';
    };

    if (bindInputPath && el.dataset.dbBindInit !== '1') {
      el.dataset.dbBindInit = '1';
      el.addEventListener('change', syncSelectToInput);

      const input = root.querySelector(`[data-path="${cssEscape(bindInputPath)}"]`);
      if (input && input.dataset.dbSelectSync !== '1') {
        input.dataset.dbSelectSync = '1';
        input.addEventListener('input', syncInputToSelect);
        input.addEventListener('change', syncInputToSelect);
      }
    }

    const loadPage = async ({ reset = false } = {}) => {
      const q = (searchInput ? searchInput.value : '').trim();

      if (reset || q !== state.q) {
        state.q = q;
        state.loaded = 0;
        state.total = null;
        state.hasMore = false;
      }

      el.disabled = true;
      const res = await db.list(source, {
        search: state.q,
        limit: pageSize,
        offset: state.loaded,
        withTotal: true,
        filterKey: filter?.key,
        filterValues: filter?.values
      });
      if (!res?.ok) {
        el.dataset.dbFailed = '1';
        el.dataset.dbLoaded = '0';
        el.innerHTML = '';
        el.appendChild(new Option('Ошибка загрузки', '', true, true));
        el.disabled = true;
        if (moreBtn) moreBtn.classList.add('hidden');
        if (metaEl) metaEl.textContent = '';
        return;
      }

      el.dataset.dbFailed = '0';
      // Mark as loaded so UIManager can reliably switch the wrapper to "connected" state.
      // (Searchable selects load pages incrementally, but from UX standpoint they are "ready".)
      el.dataset.dbLoaded = '1';

      renderOptions(res.items || [], { append: state.loaded > 0 });
      state.loaded += (res.items || []).length;
      state.total = (typeof res.total === 'number') ? res.total : null;
      state.hasMore = !!res.has_more;

      await ensureCurrentOption(db, source, el, { root, bindInputPath, labelMode });

      if (moreBtn) moreBtn.classList.toggle('hidden', !state.hasMore);
      updateMeta();
      el.disabled = false;
    };

    // Debounced search
    let t = null;
    const debounced = () => {
      if (t) clearTimeout(t);
      t = setTimeout(() => loadPage({ reset: true }), 200);
    };

    if (searchInput && el.dataset.dbSearchBind !== '1') {
      el.dataset.dbSearchBind = '1';
      searchInput.addEventListener('input', debounced);
      searchInput.addEventListener('focus', debounced);
    }
    if (moreBtn && moreBtn.dataset.dbMoreBind !== '1') {
      moreBtn.dataset.dbMoreBind = '1';
      moreBtn.addEventListener('click', () => loadPage({ reset: false }));
    }

    await loadPage({ reset: true });
  };

  const initBulkSelect = (source, els, root, items) => {
    for (const el of els) {
      el.dataset.dbFailed = '0';
      const wrapper = el.closest('.editor-dbselect');
      if (wrapper) wrapper.setAttribute('data-db-state', 'connected');

      const multiple = !!el.multiple;
      const placeholder = el.dataset.placeholder || '— выберите —';
      const labelMode = (el.dataset.labelMode || 'id_name').toLowerCase();
      const bindInputPath = el.dataset.bindInputPath || '';

      const prev = multiple
        ? (() => {
            const selected = Array.from(el.selectedOptions).map(o => String(o.value)).filter(v => v !== '');
            if (selected.length) return selected;
            // If options are not yet loaded but we rendered initial selected placeholders:
            const curRaw = el.dataset.currentValues;
            if (curRaw) {
              try {
                const arr = JSON.parse(curRaw);
                if (Array.isArray(arr)) return arr.map(x => String(x)).filter(v => v !== '' && v !== '0');
              } catch {}
            }
            return [];
          })()
        : (bindInputPath
          ? getSelectedValue(el, { bindInputPath, root })
          : String(el.value || el.dataset.currentValue || ''));

      el.innerHTML = '';
      if (!multiple) el.appendChild(new Option(placeholder, '', false, !prev));

      const previous = Array.isArray(prev) ? new Set(prev) : null;
      const fragment = document.createDocumentFragment();
      for (const it of (items || [])) {
        const val = String(it.value);
        const baseLabel = String(it.label || '');
        const label = labelMode === 'name' ? baseLabel : `${val}: ${baseLabel}`;
        const opt = new Option(label, val);
        if (multiple) opt.selected = previous?.has(val) || false;
        else opt.selected = !!prev && String(prev) === val;
        fragment.appendChild(opt);
      }
      el.appendChild(fragment);

      // fallback for unknown id
      if (!multiple && prev && !Array.from(el.options).some(o => o.value === String(prev))) {
        el.appendChild(new Option(`${prev}: (не найдено)`, String(prev), true, true));
      }

      el.disabled = false;
      el.dataset.dbLoaded = '1';

      // client filter
      const searchInput = wrapper ? wrapper.querySelector('.editor-dbselect-search') : null;
      const metaEl = wrapper ? wrapper.querySelector('.editor-dbselect-meta') : null;
      const moreBtn = wrapper ? wrapper.querySelector('.editor-dbselect-more') : null;
      if (moreBtn) moreBtn.classList.add('hidden');
      if (metaEl) {
        const cnt = multiple ? el.options.length : Math.max(0, el.options.length - 1);
        metaEl.textContent = cnt ? `Показано: ${cnt}` : '';
      }

      if (searchInput && el.dataset.dbClientFilter !== '1') {
        el.dataset.dbClientFilter = '1';
        const runFilter = () => {
          const q = (searchInput.value || '').trim().toLowerCase();
          for (const opt of Array.from(el.options)) {
            if (opt.value === '') { opt.hidden = false; continue; }
            if (!q) { opt.hidden = false; continue; }
            opt.hidden = !String(opt.text || '').toLowerCase().includes(q);
          }
        };
        searchInput.addEventListener('input', runFilter);
        searchInput.addEventListener('focus', runFilter);
      }

      // helper bind sync
      if (bindInputPath && el.dataset.dbBindInit !== '1') {
        el.dataset.dbBindInit = '1';
        const syncToInput = () => {
          const input = root.querySelector(`[data-path="${cssEscape(bindInputPath)}"]`);
          if (!input) return;
          input.value = (el.value === '' ? '0' : String(el.value));
          input.dispatchEvent(new Event('input', { bubbles: true }));
          input.dispatchEvent(new Event('change', { bubbles: true }));
        };
        el.addEventListener('change', syncToInput);

        const input = root.querySelector(`[data-path="${cssEscape(bindInputPath)}"]`);
        if (input && input.dataset.dbSelectSync !== '1') {
          input.dataset.dbSelectSync = '1';
          const syncToSelect = () => {
            const v = String(input.value || '0');
            const has = Array.from(el.options).some(o => o.value === v);
            if (has) el.value = v;
            else if (v === '0') el.value = '';
          };
          input.addEventListener('input', syncToSelect);
          input.addEventListener('change', syncToSelect);
        }
      }
    }
  };

  // New UI: single searchable input + dropdown ("combo") for db_select.
  // It binds to an underlying numeric input via data-bind-input-path.
  const initDbCombo = async (db, combo, root) => {
    if (!combo || combo.dataset.dbComboInited === '1') return;
    combo.dataset.dbComboInited = '1';

    const source = combo.dataset.datasource || '';
    if (!source) return;

    const input = combo.querySelector('.editor-dbcombo-input');
    const clearBtn = combo.querySelector('.editor-dbcombo-clear');
    const dropdownLocal = combo.querySelector('.editor-dbcombo-dropdown');

    // The portal is created only while open. This keeps document.body and the
    // global listener set bounded when forms are repeatedly rerendered.
    let dropdown = dropdownLocal;
    let portal = null;
    let removalObserver = null;
    if (dropdownLocal) dropdownLocal.style.display = 'none';
    const bindInputPath = combo.dataset.bindInputPath || '';
    const placeholder = combo.dataset.placeholder || '— выберите —';
    const labelMode = (combo.dataset.labelMode || 'id_name').toLowerCase();
    const pageSize = Number(combo.dataset.dbLimit || 300);
    const searchable = combo.dataset.dbSearchable === '1';
    const filter = readFilter(combo);
    const extraOptions = (() => {
      const raw = String(combo.dataset.extraOptions || '').trim();
      if (!raw) return [];
      try {
        const parsed = JSON.parse(raw);
        if (!Array.isArray(parsed)) return [];
        return parsed
          .filter((opt) => opt && typeof opt === 'object' && opt.value !== undefined)
          .map((opt) => ({ value: String(opt.value), label: String(opt.label ?? opt.value) }));
      } catch {
        return [];
      }
    })();

    const bound = bindInputPath
      ? root.querySelector(`[data-path="${cssEscape(bindInputPath)}"]`)
      : combo.querySelector('input[data-path]');

    const allowZero = combo.dataset.allowZero === '1' || source.toLowerCase() === 'worlds';
    const coerceSelected = (v) => {
      const s = String(v ?? '').trim();
      if (!s || (!allowZero && s === '0')) return '';
      return s;
    };

    const setDbState = (state) => {
      combo.setAttribute('data-db-state', state);
      // The numeric fallback remains available while the visual combo is down.
      if (input) input.disabled = state === 'disconnected';
    };

    const formatLabel = (val, baseLabel) => {
      const v = String(val);
      const l = String(baseLabel || '');
      return labelMode === 'name' ? l : `${v}: ${l}`;
    };

    const findExtraOption = (val) => {
      const key = String(val ?? '');
      return extraOptions.find((opt) => String(opt.value) === key) || null;
    };

    const filterExtraOptions = (query) => {
      if (!extraOptions.length) return [];
      const q = String(query || '').trim().toLowerCase();
      if (!q) return extraOptions;
      return extraOptions.filter((opt) => {
        const value = String(opt.value || '').toLowerCase();
        const label = String(opt.label || '').toLowerCase();
        return value.includes(q) || label.includes(q);
      });
    };

    const state = {
      open: false,
      q: '',
      items: null,      // for client filter mode
      activeIndex: -1,
      lastResults: [],
      querySeq: 0,
      queryController: null,
    };

    const placeDropdown = () => {
      if (!portal || !input || !dropdown) return;
      const rect = input.getBoundingClientRect();
      const margin = 6;
      const vw = window.innerWidth || document.documentElement.clientWidth || 0;
      const vh = window.innerHeight || document.documentElement.clientHeight || 0;
      const availBelow = vh - rect.bottom - margin - 12;
      const availAbove = rect.top - margin - 12;
      const preferAbove = availBelow < 160 && availAbove > availBelow;
      const maxHeight = Math.max(140, Math.min(320, preferAbove ? availAbove : availBelow));

      const left = Math.max(8, Math.min(rect.left, Math.max(8, vw - rect.width - 8)));

      dropdown.style.position = 'fixed';
      dropdown.style.left = `${left}px`;
      dropdown.style.right = 'auto';
      dropdown.style.width = `${Math.max(140, rect.width)}px`;
      dropdown.style.zIndex = '10000';
      dropdown.style.maxHeight = `${maxHeight}px`;

      if (preferAbove) {
        dropdown.style.top = '';
        dropdown.style.bottom = `${Math.max(8, vh - rect.top + margin)}px`;
      } else {
        const top = Math.min(Math.max(8, rect.bottom + margin), Math.max(8, vh - maxHeight - 8));
        dropdown.style.bottom = '';
        dropdown.style.top = `${top}px`;
      }
    };

    const onViewport = () => {
      if (state.open) placeDropdown();
    };
    const onOutside = (event) => {
      if (!combo.isConnected || (!combo.contains(event.target) && !(portal && portal.contains(event.target)))) close();
    };

    const close = () => {
      state.open = false;
      state.activeIndex = -1;
      state.querySeq += 1;
      state.queryController?.abort?.();
      state.queryController = null;
      debouncedQuery.cancel();
      removalObserver?.disconnect();
      removalObserver = null;
      window.removeEventListener('resize', onViewport);
      window.removeEventListener('scroll', onViewport, true);
      document.removeEventListener('mousedown', onOutside);
      dropdown?.classList.remove('open');
      if (portal) {
        portal.removeEventListener('click', onDropdownClick);
        portal.remove();
        portal = null;
        dropdown = dropdownLocal;
      }
    };

    const open = () => {
      if (state.open) {
        placeDropdown();
        return;
      }
      if (dropdownLocal && !portal) {
        try {
          portal = document.createElement('div');
          portal.className = 'editor-dbcombo-dropdown editor-dbcombo-dropdown-portal';
          portal.setAttribute('role', 'listbox');
          portal.addEventListener('click', onDropdownClick);
          document.body.appendChild(portal);
          dropdown = portal;
        } catch {
          portal = null;
          dropdown = dropdownLocal;
          dropdownLocal.style.display = '';
        }
      }
      state.open = true;
      if (portal && state.lastResults.length) renderList(state.lastResults);
      dropdown?.classList.add('open');
      window.addEventListener('resize', onViewport);
      window.addEventListener('scroll', onViewport, { capture: true, passive: true });
      document.addEventListener('mousedown', onOutside);
      if (typeof MutationObserver === 'function' && document.body) {
        removalObserver?.disconnect();
        removalObserver = new MutationObserver(() => {
          if (!combo.isConnected) close();
        });
        removalObserver.observe(document.body, { childList: true, subtree: true });
      }
      placeDropdown();
    };

    const dispatchBound = () => {
      if (!bound) return;
      bound.dispatchEvent(new Event('input', { bubbles: true }));
      bound.dispatchEvent(new Event('change', { bubbles: true }));
    };

    const syncClearButton = () => {
      if (!clearBtn) return;
      const hasVal = !!readBoundValue() || !!(input && String(input.value || '').trim());
      const disabled = !!(input && input.disabled);
      clearBtn.hidden = !hasVal || disabled;
    };

    const writeBoundValue = (val) => {
      if (!bound) return;
      const str = val !== undefined && val !== null ? String(val).trim() : '';
      bound.value = str ? str : (allowZero ? '' : '0');
      dispatchBound();
      syncClearButton();
    };

    const readBoundValue = () => {
      if (!bound) return '';
      return coerceSelected(bound.value);
    };

    const renderList = (items, { emptyText = 'Нет вариантов' } = {}) => {
      if (!dropdown) return;
      const arr = Array.isArray(items) ? items : [];
      const combined = [];
      const seen = new Set();
      const extras = filterExtraOptions(state.q);
      extras.forEach((opt) => {
        const value = String(opt.value);
        if (seen.has(value)) return;
        combined.push({ value, label: opt.label, isExtra: true });
        seen.add(value);
      });
      arr.forEach((it) => {
        const value = String(it.value);
        if (seen.has(value)) return;
        combined.push({ value, label: it.label, isExtra: false });
        seen.add(value);
      });
      state.lastResults = combined;
      state.activeIndex = -1;

      dropdown.replaceChildren();
      if (!combined.length) {
        const empty = document.createElement('div');
        empty.className = 'editor-dbcombo-empty';
        empty.textContent = emptyText;
        dropdown.appendChild(empty);
        return;
      }

      const fragment = document.createDocumentFragment();
      combined.forEach((item, index) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'editor-dbcombo-option';
        button.setAttribute('role', 'option');
        button.dataset.value = String(item.value);
        button.dataset.index = String(index);
        button.textContent = formatLabel(item.value, item.label);
        fragment.appendChild(button);
      });
      dropdown.appendChild(fragment);
    };

    // One delegated handler replaces a listener per option. Values and labels
    // are assigned through DOM properties, so DB content cannot inject HTML.
    const onDropdownClick = (event) => {
      const button = event.target?.closest?.('.editor-dbcombo-option');
      if (!button || !dropdown?.contains(button)) return;
      const value = String(button.dataset.value || '');
      if (!value) return;
      const picked = state.lastResults[Number(button.dataset.index)];
      writeBoundValue(value);
      if (input) input.value = picked ? formatLabel(picked.value, picked.label) : value;
      syncClearButton();
      close();
    };
    dropdownLocal?.addEventListener('click', onDropdownClick);

    const ensureDisplay = async () => {
      if (!input) return;
      const cur = readBoundValue();
      if (!cur) {
        input.value = '';
        input.placeholder = placeholder;
        syncClearButton();
        return;
      }
      const extra = findExtraOption(cur);
      if (extra) {
        input.value = formatLabel(extra.value, extra.label);
        syncClearButton();
        return;
      }
      try {
        const one = await db.getOne(source, cur);
        if (one?.ok && one.item) {
          input.value = formatLabel(one.item.value, one.item.label);
          syncClearButton();
          return;
        }
      } catch {}
      // Fallback: show raw id
      input.value = cur;
      syncClearButton();
    };

    if (clearBtn) {
      clearBtn.addEventListener('mousedown', (event) => {
        event.preventDefault();
      });
      clearBtn.addEventListener('click', (event) => {
        event.preventDefault();
        event.stopPropagation();
        if (input) {
          input.value = '';
          input.placeholder = placeholder;
        }
        writeBoundValue('');
        close();
      });
    }

    const loadClientItemsIfNeeded = async () => {
      if (state.items) return true;
      const res = await db.list(source, { limit: 5000, offset: 0, filterKey: filter?.key, filterValues: filter?.values });
      if (!res?.ok) return false;
      state.items = res.items || [];
      return true;
    };

    const query = async (qRaw) => {
      if (!input) return;
      const q = String(qRaw ?? '').trim();
      state.q = q;
      const querySeq = ++state.querySeq;
      state.queryController?.abort?.();
      state.queryController = new AbortController();
      const signal = state.queryController.signal;

      // If DB is down -> show fallback
      try {
        if (searchable) {
          const res = await db.list(source, {
            search: q,
            limit: pageSize,
            offset: 0,
            withTotal: false,
            filterKey: filter?.key,
            filterValues: filter?.values,
            signal
          });
          if (querySeq !== state.querySeq || signal.aborted) return;
          if (!res?.ok) {
            setDbState('disconnected');
            close();
            return;
          }
          setDbState('connected');
          renderList(res.items || [], { emptyText: q ? 'Ничего не найдено' : 'Нет вариантов' });
          return;
        }

        const ok = await loadClientItemsIfNeeded();
        if (querySeq !== state.querySeq || signal.aborted) return;
        if (!ok) {
          setDbState('disconnected');
          close();
          return;
        }
        setDbState('connected');
        const needle = q.toLowerCase();
        const filtered = !needle
          ? state.items.slice(0, pageSize)
          : state.items.filter((it) => {
              const v = String(it.value).toLowerCase();
              const l = String(it.label || '').toLowerCase();
              return v.includes(needle) || l.includes(needle) || `${v}: ${l}`.includes(needle);
            }).slice(0, pageSize);
        renderList(filtered, { emptyText: q ? 'Ничего не найдено' : 'Нет вариантов' });
      } catch (err) {
        if (signal.aborted || querySeq !== state.querySeq) return;
        setDbState('disconnected');
        close();
      }
    };

    // Events
    const debouncedQuery = window.EditorCore.utils.debounce(query, 150);

    if (input) {
      input.addEventListener('focus', () => {
        open();
        debouncedQuery(input.value);
      });
      input.addEventListener('click', () => {
        open();
        debouncedQuery(input.value);
      });
      input.addEventListener('input', () => {
        const raw = String(input.value || '').trim();
        if (!raw) {
          writeBoundValue('');
        } else {
          syncClearButton();
        }
        open();
        debouncedQuery(input.value);
      });
      input.addEventListener('keydown', (e) => {
        if (!dropdown) return;
        const opts = Array.from(dropdown.querySelectorAll('.editor-dbcombo-option'));

        if (e.key === 'Escape') {
          close();
          return;
        }
        if (e.key === 'ArrowDown') {
          if (!opts.length) return;
          e.preventDefault();
          state.activeIndex = Math.min(opts.length - 1, state.activeIndex + 1);
          opts.forEach(b => b.classList.remove('active'));
          opts[state.activeIndex]?.classList.add('active');
          opts[state.activeIndex]?.scrollIntoView?.({ block: 'nearest' });
          return;
        }
        if (e.key === 'ArrowUp') {
          if (!opts.length) return;
          e.preventDefault();
          state.activeIndex = Math.max(0, state.activeIndex - 1);
          opts.forEach(b => b.classList.remove('active'));
          opts[state.activeIndex]?.classList.add('active');
          opts[state.activeIndex]?.scrollIntoView?.({ block: 'nearest' });
          return;
        }
        if (e.key === 'Enter') {
          // If option is highlighted -> pick it.
          if (state.activeIndex >= 0 && opts[state.activeIndex]) {
            e.preventDefault();
            opts[state.activeIndex].click();
            return;
          }
          // If user typed a pure numeric id -> accept.
          const raw = String(input.value || '').trim();
          if (/^\d+$/.test(raw)) {
            e.preventDefault();
            writeBoundValue(raw);
            ensureDisplay();
            close();
          }
        }
      });
      input.addEventListener('blur', () => {
        // Small delay so click on option works.
        setTimeout(() => {
          const ae = document.activeElement;
          if (!combo.contains(ae) && !(portal && portal.contains(ae))) close();
        }, 150);
      });
    }

    // Keep display synced when underlying input changes
    if (bound && bound.dataset.dbComboSync !== '1') {
      bound.dataset.dbComboSync = '1';
      bound.addEventListener('input', ensureDisplay);
      bound.addEventListener('change', ensureDisplay);
    }

    // Initial label
    await ensureDisplay();
  };

  const DB = {
    endpoint: DEFAULT_ENDPOINT,

    resolveSource(dbKey) {
      const map = window.EditorCore?.DBMap || {};
      return map?.[dbKey]?.source || null;
    },

    async getConfig() {
      return fetchJson(`${this.endpoint}?action=get_config`);
    },

    async saveConfig(payload) {
      cache.clear();
      return fetchJson(`${this.endpoint}?action=save_config`, { method: 'POST', body: payload });
    },

    // Проверка соединения ждёт ответа сервера игровой БД — 20 секунд.
    async testConnection() {
      return fetchJson(`${this.endpoint}?action=test`, { method: 'POST', body: {}, timeout: 20000 });
    },

    async testSkins({ url = '' } = {}) {
      return fetchJson(`${this.endpoint}?action=test_skins`, { method: 'POST', body: { url }, timeout: 20000 });
    },

    // Мост к картам игрового сервера: адрес можно проверить до сохранения.
    async testMapApi({ url = '', token = '' } = {}) {
      return fetchJson(`${this.endpoint}?action=test_map_api`, { method: 'POST', body: { url, token }, timeout: 20000 });
    },

    async listDumps() {
      return fetchJson(`${MAINT_ENDPOINT}?action=list_dumps`);
    },

    // Дамп большой базы пишется и восстанавливается минутами — таймаут на вызов.
    async createDump(payload) {
      return fetchJson(`${MAINT_ENDPOINT}?action=create_dump`, { method: 'POST', body: payload, timeout: 300000 });
    },

    async restoreDump(payload) {
      return fetchJson(`${MAINT_ENDPOINT}?action=restore_dump`, { method: 'POST', body: payload, timeout: 900000 });
    },

    async deleteDump(payload) {
      return fetchJson(`${MAINT_ENDPOINT}?action=delete_dump`, { method: 'POST', body: payload });
    },

    async getOne(source, id) {
      const key = `one:${source}:${id}`;
      const qs = new URLSearchParams({ action: 'get_one', source, id: String(id) }).toString();
      return cache.load(key, () => fetchJson(`${this.endpoint}?${qs}`), cacheOptions());
    },

    async list(source, { search = '', limit = 1000, offset = 0, withTotal = false, filterKey = '', filterValue = '', filterValues = [], signal = null } = {}) {
      const resolvedFilterKey = String(filterKey || '');
      const normalizedValues = Array.isArray(filterValues)
        ? filterValues.map(v => String(v ?? '').trim()).filter(Boolean)
        : (filterValue ? [String(filterValue)] : []);
      const resolvedFilterValue = normalizedValues.join('|');
      const key = `list:${source}:${search}:${limit}:${offset}:${withTotal ? 1 : 0}:${resolvedFilterKey}:${resolvedFilterValue}`;
      const params = { action: 'list', source, search, limit: String(limit), offset: String(offset) };
      if (withTotal) params.with_total = '1';
      if (resolvedFilterKey && normalizedValues.length) {
        params.filter_key = resolvedFilterKey;
        if (normalizedValues.length > 1) {
          params.filter_values = JSON.stringify(normalizedValues);
        } else {
          params.filter_value = normalizedValues[0];
        }
      }
      const qs = new URLSearchParams(params).toString();
      return cache.load(key, () => fetchJson(`${this.endpoint}?${qs}`, { signal }), cacheOptions(signal));
    },

    async init(root = document) {
      const selects = Array.from(root.querySelectorAll('select[data-datasource]'));
      const combos = Array.from(root.querySelectorAll('.editor-dbcombo[data-datasource]'));
      if (!selects.length && !combos.length) return;

      const bySource = new Map();
      for (const el of selects) {
        const source = el.dataset.datasource;
        if (!source) continue;
        const filter = readFilter(el);
        const filterKey = filter ? `${filter.key}=${filter.values.join('|')}` : '';
        const groupKey = `${source}::${filterKey}`;
        if (!bySource.has(groupKey)) {
          bySource.set(groupKey, { source, filter, els: [] });
        }
        bySource.get(groupKey).els.push(el);
      }

      for (const { source, filter, els } of bySource.values()) {
        // show loading
        for (const el of els) {
          const current = el.value || el.dataset.currentValue || '';
          if (!el.dataset.dbLoaded) {
            const hasMeaningful = !!current && current !== '0';
            // If FieldRenderer already rendered a selected placeholder option, keep it so the user sees the value.
            // Otherwise show a loading marker.
            if (!hasMeaningful && (!el.options || el.options.length <= 1)) {
              el.innerHTML = '';
              el.appendChild(new Option('Загрузка…', '', true, true));
            }
          }
          el.disabled = true;
        }

        const anySearchable = els.some(e => e.dataset.dbSearchable === '1');

        if (anySearchable) {
          // Searchable selects are handled individually (they fetch their own pages).
          for (const el of els) {
            if (el.dataset.dbSearchable === '1') {
              await initSearchableSelect(this, source, el, root);
            }
          }
          // For non-searchable selects of the same source in the same root, also populate them (bulk).
          const bulkEls = els.filter(e => e.dataset.dbSearchable !== '1');
          if (bulkEls.length) {
            const res = await this.list(source, { limit: 5000, offset: 0, filterKey: filter?.key, filterValues: filter?.values });
            if (!res?.ok) {
              for (const el of bulkEls) {
                el.disabled = true;
                el.innerHTML = '';
                el.appendChild(new Option('Нет соединения с БД', '', true, true));
                const wrapper = el.closest('.editor-dbselect');
                if (wrapper) wrapper.setAttribute('data-db-state', 'disconnected');
              }
            } else {
              initBulkSelect(source, bulkEls, root, res.items || []);
            }
          }
          continue;
        }

        // Bulk mode
        const PAGE = 1000;
        const CAP = 5000;
        let items = [];
        let offset = 0;
        let ok = true;

        while (true) {
          const res = await this.list(source, { limit: PAGE, offset, filterKey: filter?.key, filterValues: filter?.values });
          if (!res?.ok) { ok = false; break; }
          const page = res.items || [];
          items = items.concat(page);
          if (!res.has_more) break;
          offset += page.length;
          if (items.length >= CAP) break;
          if (!page.length) break;
        }

        if (!ok) {
          for (const el of els) {
            el.dataset.dbFailed = '1';
            el.disabled = true;
            el.innerHTML = '';
            el.appendChild(new Option('Нет соединения с БД', '', true, true));
            const wrapper = el.closest('.editor-dbselect');
            if (wrapper) wrapper.setAttribute('data-db-state', 'disconnected');
          }
          continue;
        }

        initBulkSelect(source, els, root, items);
      }

      // Init new db_select combo widgets
      for (const combo of combos) {
        await initDbCombo(this, combo, root);
      }
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DB = DB;
})();
