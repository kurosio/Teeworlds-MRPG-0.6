(() => {
  // Lightweight CRUD runtime for DB editors.
  // Requires: editor-core/*.js (DBCrud + FormRuntime + FieldRenderer + UIManager).

  const escapeHtml = (...args) => window.EditorCore.utils.escapeHtml(...args);

  const queryRole = (root, role) => root.querySelector(`[data-editor-role="${role}"]`);
  const cloneData = (...args) => window.EditorCore.utils.cloneData(...args);

  const DbEditor = {
    mount(root, cfg = {}) {
      const resource = String(cfg.resource || '').trim();
      if (!resource) throw new Error('DbEditor: resource is required');
      root.__editorDbRuntime?.destroy();
      const events = new AbortController();
      const timers = new Set();

      const els = {
        list: queryRole(root, 'list'),
        search: queryRole(root, 'search'),
        refresh: queryRole(root, 'refresh'),
        newBtn: queryRole(root, 'new'),
        title: queryRole(root, 'title'),
        subtitle: queryRole(root, 'subtitle'),
        save: queryRole(root, 'save'),
        cancel: queryRole(root, 'cancel'),
        del: queryRole(root, 'delete'),
        form: queryRole(root, 'form'),
        status: queryRole(root, 'status'),
        count: queryRole(root, 'count'),
      };

      const fieldOptions = cfg.fieldOptions || window.EditorCore?.defaults?.getFieldRenderOptions?.('event');
      const debounce = window.EditorCore.utils.debounce;
      const toast = window.EditorCore?.UI?.toast || (() => {});
      const Guest = window.EditorCore?.Guest || null;
      const isGuest = () => !!Guest?.isGuest?.();
      const isRequestEditing = () => !!window.EditorCore?.RequestContext?.isEditing?.();
      const guestMeta = (id) => (isGuest() && !window.EditorCore?.RequestContext?.hasResource?.(resource)
        ? Guest?.Draft?.rowMeta?.(resource, id) : null);

      const state = {
        rows: [],
        rowsRaw: [],
        search: '',
        selectedId: 0,
        model: null,
        persistedModel: null,
        lastSelectedIdBeforeCreate: 0,
        form: null,
        dirty: cfg.dirty || null,
        searchRequest: {
          token: 0,
          controller: null,
        },
        visibleCount: 250,
        operation: false,
        destroyed: false,
        selectionVersion: 0,
        discardPrompt: null,
      };
      let listRequestSeq = 0;
      let onSearch = null;
      // Ссылка «открыть запись» (?record=&field=) из вкладки «Запросы».
      const deepLink = window.EditorCore?.DeepLink || null;
      let deepLinkPending = !!deepLink?.active;
      // This runtime resolves links through CRUD, not a timed synthetic click.
      if (deepLinkPending) deepLink.markHandled?.();
      const hasSelectedRecord = () => !!state.model && (state.selectedId !== 0 || state.persistedModel !== null);

      /** Спрашивает подтверждение, если в форме есть несохранённые правки. */
      const canDiscardDirty = async () => {
        if (!state.dirty?.isDirty?.()) return true;
        if (!state.discardPrompt) {
          state.discardPrompt = Promise.resolve()
            .then(() => EditorCore.Page.confirm('You have unsaved changes. Continue without saving?'))
            .finally(() => { state.discardPrompt = null; });
        }
        return state.discardPrompt;
      };
      const later = (fn, delay) => {
        const timer = window.setTimeout(() => { timers.delete(timer); if (!state.destroyed) fn(); }, delay);
        timers.add(timer);
        return timer;
      };

      const makeDefault = () => {
        if (typeof cfg.defaults === 'function') return cfg.defaults();
        return cfg.defaults ? { ...cfg.defaults } : {};
      };

      const toModel = (row) => {
        if (typeof cfg.rowToModel === 'function') return cfg.rowToModel(row);
        return { ...(row || {}) };
      };

      const toPayload = (model) => {
        if (typeof cfg.modelToPayload === 'function') return cfg.modelToPayload(model);
        return { ...(model || {}) };
      };

      const setStatus = (message = '', tone = 'muted') =>
        window.EditorCore.utils.setStatus(els.status, message, tone);

      // Последняя открытая запись редактора: после перезагрузки кадра
      // (F5, вытеснение iframe оболочкой, повторный вход) редактор должен
      // открыться там же, где вы работали, а не на первой строке списка.
      const LAST_RECORD_KEY = `editor-core:last-record:${resource}`;
      const readLastRecordId = () => {
        try {
          const id = Number(localStorage.getItem(LAST_RECORD_KEY));
          return Number.isSafeInteger(id) && id > 0 ? id : 0;
        } catch { return 0; }
      };
      const storeLastRecordId = (id) => {
        try { if (id > 0) localStorage.setItem(LAST_RECORD_KEY, String(id)); } catch { /* ignore storage failures */ }
      };

      const setHeader = () => {
        if (!els.title && !els.subtitle) return;
        if (!state.model) {
          if (els.title) els.title.textContent = cfg.emptyTitle || 'Select a record';
          if (els.subtitle) els.subtitle.textContent = cfg.emptySubtitle || 'or create a new one';
          return;
        }
        const title = typeof cfg.getTitle === 'function'
          ? cfg.getTitle(state.model, state.selectedId, { existing: hasSelectedRecord() })
          : (state.model?.Name || `Record #${state.selectedId || 'new'}`);
        const subtitle = typeof cfg.getSubtitle === 'function'
          ? cfg.getSubtitle(state.model, state.selectedId, { existing: hasSelectedRecord() })
          : '';
        if (els.title) els.title.textContent = title;
        if (els.subtitle) els.subtitle.textContent = subtitle;
      };

      const guestBadgeHtml = (meta) => {
        if (!meta) return '';
        const map = {
          create: ['New', 'fa-plus'],
          update: ['Changed', 'fa-pen'],
          delete: ['Delete', 'fa-trash'],
        };
        const [text, icon] = map[meta.state] || map.update;
        const submitted = meta.status === 'submitted';
        return `<span class="guest-row-badge ${submitted ? 'is-submitted' : 'is-draft'}" title="${submitted ? 'Submitted in a request, awaiting administrator review' : 'Local change — not saved to the database'}"><i class="fa-solid ${submitted ? 'fa-hourglass-half' : icon}"></i>${escapeHtml(text)}</span>`;
      };

      let guestHintEl = null;
      const ensureGuestHint = () => {
        if (guestHintEl && guestHintEl.isConnected) return guestHintEl;
        const host = els.form?.parentElement;
        if (!host || !els.form) return null;
        guestHintEl = document.createElement('div');
        guestHintEl.className = 'guest-row-hint hidden';
        host.insertBefore(guestHintEl, els.form);
        return guestHintEl;
      };

      /**
       * Кнопка «вернуть как в базе» у изменённого поля. Одна и та же кнопка
       * переиспользуется при перерисовках, поэтому обработчик — делегированный.
       */
      const syncRevertButton = (node, column) => {
        const existing = node.querySelector(':scope > .guest-field-revert');
        node.classList.toggle('guest-field-revertable', !!column);
        if (!column) {
          existing?.remove();
          return;
        }
        if (existing) {
          if (existing.dataset.guestRevert !== column) existing.dataset.guestRevert = column;
          return;
        }
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'guest-field-revert';
        button.dataset.guestRevert = column;
        button.title = 'Restore the value from the database';
        button.setAttribute('aria-label', `Restore the original value of field  ${column}`);
        button.innerHTML = '<i class="fa-solid fa-rotate-left" aria-hidden="true"></i>';
        node.appendChild(button);
      };

      /**
       * Mark the form fields the guest really changed: the values in the form are
       * the guest's own, so each edited field must be visibly flagged instead of
       * looking like ordinary data from the database.
       *
       * Рядом с меткой «изм.» появляется маленькая кнопка отмены: она убирает
       * поле из черновика и возвращает значение из базы (см. revertGuestField).
       */
      const decorateGuestFields = (meta) => {
        if (!els.form) return;
        const fields = new Set(meta?.fields || []);
        const submitted = meta?.status === 'submitted';
        const revertable = !!meta && !submitted && meta.state === 'update';
        const note = submitted
          ? `Field changed in request  ${meta?.reqId || ''}  — awaiting admin review`
          : 'Field changed in your draft — not written to the database';
        els.form.querySelectorAll('[data-field-path]').forEach((node) => {
          const top = String(node.getAttribute('data-field-path') || '').split('.')[0];
          const changed = fields.has(top);
          node.classList.toggle('guest-field-changed', changed);
          node.classList.toggle('is-submitted', changed && submitted);
          if (changed) {
            if (node.dataset.guestPrevTitle === undefined) {
              node.dataset.guestPrevTitle = node.getAttribute('title') || '';
            }
            node.setAttribute('title', note);
          } else if (node.dataset.guestPrevTitle !== undefined) {
            const prev = node.dataset.guestPrevTitle;
            if (prev) node.setAttribute('title', prev); else node.removeAttribute('title');
            delete node.dataset.guestPrevTitle;
          }
          syncRevertButton(node, changed && revertable ? top : '');
        });
      };

      /**
       * Вернуть одно поле к значению из базы: убираем поле из черновика гостя
       * и подставляем в форму то, что реально хранится в строке БД.
       */
      const revertGuestField = async (column) => {
        const id = Number(state.selectedId || 0);
        const meta = id ? guestMeta(id) : null;
        if (!column || !id || state.operation) return false;
        if (!meta || meta.status !== 'draft' || meta.state !== 'update') return false;
        return runOperation(async () => {
          try {
            // Сначала читаем строку из базы: если значение недоступно, черновик
            // не трогаем — иначе форма и черновик разошлись бы.
            const baseRow = await window.EditorCore.DBCrud.rawRow(resource, id);
            const original = baseRow ? toModel(baseRow) : null;
            if (!original || !(column in original)) {
              throw new Error('Could not read the value from the database — refresh the list and try again.');
            }
            const result = await Guest.Draft.revertField(resource, id, column, baseRow);
            if (result?.removed) applyBaseValue(original[column], column);
            else updateGuestHint();
            renderList();
            setStatus(result?.dropped
              ? 'Field restored: no changes left in the draft.'
              : `Field ${column}  restored to the database value.`, 'ok');
            return true;
          } catch (error) {
            const msg = error?.message || 'Could not restore the original field value';
            setStatus(msg, 'err');
            toast(msg, 'error');
            return false;
          }
        });
      };

      /**
       * Подставить в форму исходное значение поля (в том же виде, что даёт
       * rowToModel). Остальные несохранённые правки формы не теряются.
       */
      const applyBaseValue = (baseValue, column) => {
        if (!state.form) return;
        const value = cloneData(baseValue);
        state.model = { ...state.model, [column]: value };
        state.persistedModel = { ...(state.persistedModel || {}), [column]: cloneData(value) };
        state.form.setData?.(state.model);
        const utils = window.EditorCore.utils;
        if (state.persistedModel && utils.valuesEqual(state.model, state.persistedModel)) state.dirty?.setClean?.();
        else state.dirty?.markDirty?.();
        setButtons(!!state.model);
        setHeader();
        updateGuestHint();
      };

      const updateGuestHint = () => {
        const el = guestHintEl;
        if (!el) return;
        if (!state.model || !state.selectedId || !isGuest()) {
          el.classList.add('hidden');
          decorateGuestFields(null);
          return;
        }
        const meta = guestMeta(state.selectedId);
        if (!meta) {
          el.classList.add('hidden');
          decorateGuestFields(null);
          return;
        }
        decorateGuestFields(meta);
        const submitted = meta.status === 'submitted';
        const title = meta.state === 'create' ? 'New record' : meta.state === 'delete' ? 'Marked for deletion' : 'Local change';
        const note = submitted
          ? `submitted in a request <b>${escapeHtml(meta.reqId || '')}</b> and awaits admin review`
          : 'request draft — nothing has been written to the database yet';
        const fields = meta.fields.length
          ? `<div class="guest-row-hint-fields">Changed fields: ${meta.fields.slice(0, 8).map((f) => `<code>${escapeHtml(f)}</code>`).join(' ')}${meta.fields.length > 8 ? ' …' : ''}</div>`
          : '';
        el.className = `guest-row-hint ${submitted ? 'is-submitted' : 'is-draft'}`;
        el.innerHTML = `<i class="fa-solid ${submitted ? 'fa-hourglass-half' : 'fa-pen-to-square'}"></i><div><b>${title}</b> — ${note}${fields}</div>`;
        el.classList.remove('hidden');
      };

      const setButtons = (enabled) => {
        const canEdit = enabled && !state.operation;
        const canCancel = canEdit && !!state.dirty?.isDirty?.();
        if (els.save) els.save.disabled = !canEdit;
        if (els.cancel) {
          els.cancel.disabled = !canCancel;
          els.cancel.classList.toggle('hidden', !canCancel);
        }
        if (els.del) els.del.disabled = !canEdit || !hasSelectedRecord();
        if (els.newBtn) els.newBtn.disabled = state.operation;
        if (els.refresh) els.refresh.disabled = state.operation;
        if (els.form) {
          els.form.inert = state.operation;
          els.form.setAttribute('aria-busy', String(state.operation));
        }
      };

      const runOperation = async (task) => {
        if (state.operation) return false;
        state.operation = true;
        setButtons(false);
        try {
          return await task();
        } finally {
          state.operation = false;
          if (!state.destroyed) setButtons(!!state.model);
        }
      };

      const renderForm = () => {
        if (!els.form) return;
        ensureGuestHint();
        if (!state.model) {
          state.form?.destroy?.();
          state.form = null;
          els.form.innerHTML = `<div class="editor-empty-state editor-field--span-all">Select a record on the left or create a new one to start editing.</div>`;
          if (guestHintEl) guestHintEl.classList.add('hidden');
          setButtons(false);
          return;
        }
        if (!state.form) {
          state.form = window.EditorCore.FormRuntime.mount(els.form, {
            fields: cfg.fields || {},
            data: state.model,
            fieldOptions,
            onChange: (data, meta) => {
              if (state.operation || state.destroyed) return;
              state.model = data;
              state.dirty?.markDirty?.();

              // Some forms have conditional fields (e.g. item group -> attributes).
              // If caller specifies rerender triggers, force a form rerender.
              const paths = cfg.rerenderOnChangePaths;
              const shouldRerender = (() => {
                if (Array.isArray(paths) && meta?.path) return paths.includes(meta.path);
                if (typeof cfg.rerenderOnChange === 'function') return !!cfg.rerenderOnChange(state.model, meta);
                return false;
              })();

              if (shouldRerender) {
                // keep model as source of truth
                state.form?.setData?.(state.model);
              }
              setButtons(true);
            }
          });
        } else {
          state.form.setData(state.model);
        }
        setButtons(true);
        updateGuestHint();
      };

      const renderList = () => {
        if (state.destroyed || !els.list) return;
        const rows = state.rows || [];
        const requestTarget = window.EditorCore?.RequestContext?.target?.();
        const targetId = requestTarget?.resource === resource ? Number(requestTarget.id) : 0;
        const orderedRows = targetId && rows.some((row) => Number(row?.ID) === targetId)
          ? [...rows.filter((row) => Number(row?.ID) === targetId), ...rows.filter((row) => Number(row?.ID) !== targetId)]
          : rows;
        const rawTotal = Array.isArray(state.rowsRaw) ? state.rowsRaw.length : 0;
        if (els.count) {
          if (state.search && rawTotal > rows.length) {
            els.count.textContent = `${rows.length}  of  ${rawTotal}`;
          } else {
            els.count.textContent = rows.length ? `${rows.length}` : '0';
          }
        }
        // Перерисовка через innerHTML сбрасывает прокрутку списка в начало:
        // при массовой работе список «прыгает» после каждого выбора/сохранения.
        const prevScrollTop = els.list.scrollTop || 0;
        if (!rows.length) {
          // Пустой список должен объяснять, что делать дальше.
          const message = state.search
            ? `Nothing found for “${escapeHtml(state.search)}”. Change the query or clear the search.`
            : `No records yet — press “${escapeHtml(cfg.newLabel || (els.newBtn?.textContent || '').trim() || 'New')}” to create the first one.`;
          els.list.innerHTML = `<div class="editor-empty-state">${message}</div>`;
          els.list.scrollTop = prevScrollTop;
          return;
        }

        const visibleRows = orderedRows.slice(0, state.visibleCount);
        const html = visibleRows.map((row) => {
          const label = typeof cfg.listItem === 'function'
            ? cfg.listItem(row)
            : { title: row?.Name || `#${row?.ID}`, subtitle: row?.Path || '' };
          const active = hasSelectedRecord() && Number(row?.ID) === Number(state.selectedId);
          const meta = guestMeta(row?.ID);
          const badge = meta ? guestBadgeHtml(meta) : '';
          return `
            <button type="button" class="w-full text-left editor-row ${active ? 'editor-row-active' : ''}" data-id="${escapeHtml(row?.ID)}">
              <div class="flex items-center gap-2">
                ${label.icon ? `<i class="fa-solid ${escapeHtml(label.icon)} editor-primary-text shrink-0" aria-hidden="true"></i>` : ''}
                <div class="font-semibold truncate">${escapeHtml(label.title || '')}</div>
                ${badge}
              </div>
              ${label.subtitle ? `<div class="text-xs editor-muted-text truncate">${escapeHtml(label.subtitle)}</div>` : ''}
            </button>
          `;
        }).join('');
        const hasMore = orderedRows.length > visibleRows.length;
        const more = hasMore
          ? `<div class="pt-2"><button type="button" class="editor-btn editor-btn-secondary w-full" data-editor-role="list-more">Show more (${orderedRows.length - visibleRows.length})</button></div>`
          : '';
        els.list.innerHTML = `<div class="editor-list">${html}${more}</div>`;
        els.list.scrollTop = prevScrollTop;
      };


      const searchCfg = cfg.searchOptions || {};
      const localMinLength = Number(searchCfg.localMinLength ?? 2);

      const normalizeSearchText = (value) => String(value || '').trim();

      const filterRowsLocally = (rows, search) => {
        const q = normalizeSearchText(search).toLowerCase();
        if (!q) return rows.slice();
        return rows.filter((row) => {
          const name = String(row?.Name || '').toLowerCase();
          const id = String(row?.ID || '').toLowerCase();
          if (name.includes(q) || id.includes(q)) return true;
          if (typeof cfg.listItem === 'function') {
            try {
              const item = cfg.listItem(row) || {};
              const title = String(item.title || '').toLowerCase();
              const subtitle = String(item.subtitle || '').toLowerCase();
              if (title.includes(q) || subtitle.includes(q)) return true;
            } catch {}
          }
          return false;
        });
      };

      /**
       * Открыть запись по ссылке из «Запросов». Обычно строку достаточно
       * нажать (это делает deep-link.js), но в длинном каталоге она может
       * не попасть в загруженную часть списка — тогда читаем её по ID.
       */
      const openDeepLinkRecord = async () => {
        deepLinkPending = false;
        const listVersion = listRequestSeq;
        const selection = state.selectionVersion;
        const id = Number(deepLink?.record) || 0;
        if (!deepLink?.active) return false;
        const inList = state.rows.find((row) => Number(row?.ID) === id) || null;
        let row = inList;
        if (!row) {
          const response = await window.EditorCore.DBCrud.get(resource, id).catch(() => null);
          row = response?.row || null;
        }
        if (state.destroyed || listVersion !== listRequestSeq || selection !== state.selectionVersion || state.model) return false;
        if (!row) {
          setStatus('The linked record was not found.', 'err');
          toast('The linked record was not found.', 'error');
          return false;
        }
        deepLink?.markHandled?.();
        if (!inList) {
          // Записи не было в списке — показываем её в списке, чтобы форма и
          // подсветка строки не расходились.
          state.rows = [row, ...state.rows];
          renderList();
        }
        await selectRow(row, { confirmDirty: false });
        later(() => deepLink?.revealField?.(), 250);
        return true;
      };

      const applyRows = async (rows, { search, isRaw = false, isCurrent = () => true } = {}) => {
        if (state.destroyed) return;
        const safeRows = Array.isArray(rows) ? rows : [];
        if (isRaw) state.rowsRaw = safeRows;
        state.search = normalizeSearchText(search ?? state.search);
        state.rows = safeRows;
        state.visibleCount = 250;
        setStatus(state.rows.length ? '' : 'The list is empty.', 'muted');
        renderList();

        // Обновление списка не должно затирать несохранённые правки в форме
        // (раньше оно молча перевыбирало запись) и не должно спрашивать
        // подтверждение на каждый фоновый перезапрос.
        const requestTarget = window.EditorCore?.RequestContext?.target?.();
        const requestedRow = !state.model && requestTarget?.resource === resource
          ? state.rows.find((row) => Number(row.ID) === Number(requestTarget.id)) : null;
        if (requestedRow) {
          await selectRow(requestedRow, { confirmDirty: false });
        } else if (deepLinkPending && !state.model) {
          await openDeepLinkRecord();
          if (!isCurrent()) return;
        } else if (hasSelectedRecord()) {
          const found = state.rows.find(r => Number(r.ID) === Number(state.selectedId));
          if (found && !state.dirty?.isDirty?.()) await selectRow(found, { confirmDirty: false, refresh: true });
        } else if (!state.model && state.rows.length) {
          // Первый выбор после загрузки: сначала запись, на которой вы
          // остановились в прошлый раз, и только если её нет — первая строка.
          const lastId = readLastRecordId();
          const remembered = lastId ? state.rows.find(r => Number(r.ID) === lastId) : null;
          await selectRow(remembered || state.rows[0], { confirmDirty: false });
        }
      };

      const selectRow = async (row, options = {}) => {
        if (state.destroyed || (state.operation && !options.internal)) return false;
        const version = ++state.selectionVersion;
        const confirmDirty = options?.confirmDirty !== false;
        const nextId = Number(row?.ID || 0);
        const currentId = Number(state.selectedId || 0);

        if (row && hasSelectedRecord() && nextId === currentId && !options.refresh) return true;
        if (confirmDirty && !await canDiscardDirty()) return false;
        if (state.destroyed || (state.operation && !options.internal) || version !== state.selectionVersion) return false;

        if (!row) {
          state.selectedId = 0;
          state.model = null;
          state.persistedModel = null;
          state.dirty?.setClean?.();
          renderForm();
          renderList();
          setHeader();
          return true;
        }
        state.selectedId = Number(row.ID || 0);
        state.model = toModel(row);
        state.persistedModel = cloneData(state.model);
        state.dirty?.setClean?.();
        storeLastRecordId(state.selectedId);
        renderForm();
        renderList();
        setHeader();
        return true;
      };

      const loadList = async (options = {}) => {
        if (state.destroyed) return;
        const requestId = ++listRequestSeq;
        const { forceServer = false, silent = false } = options;
        const search = normalizeSearchText(els.search?.value);
        state.search = search;
        const current = () => !state.destroyed && requestId === listRequestSeq;
        const canUseLocal = !forceServer && state.rowsRaw.length && search.length < localMinLength;
        state.searchRequest.controller?.abort?.();
        state.searchRequest.controller = null;
        els.list?.setAttribute('aria-busy', 'false');
        try {
          if (canUseLocal) {
            const filtered = filterRowsLocally(state.rowsRaw, search);
            const transformed = cfg.transformList?.(filtered, { search }) || filtered;
            if (current()) await applyRows(transformed, { search, isCurrent: current });
            return;
          }
          const token = ++state.searchRequest.token;
          const controller = new AbortController();
          state.searchRequest.controller = controller;
          if (!silent) setStatus(search ? 'Searching…' : 'Loading list…');
          els.list?.setAttribute('aria-busy', 'true');
          const res = await window.EditorCore.DBCrud.list(resource, {
            search, limit: 2000, offset: 0, signal: controller.signal,
          });
          if (!current() || token !== state.searchRequest.token || controller.signal.aborted) return;
          const rows = Array.isArray(res?.rows) ? res.rows : [];
          const transformed = cfg.transformList?.(rows, { search }) || rows;
          if (current()) await applyRows(transformed, { search, isRaw: true, isCurrent: current });
        } catch (error) {
          if (!current() || state.searchRequest.controller?.signal.aborted) return;
          const message = error?.message || 'Loading failed';
          setStatus(message, 'err');
          toast(message, 'error');
        } finally {
          if (current()) els.list?.setAttribute('aria-busy', 'false');
        }
      };

      const createNew = async () => {
        if (state.destroyed || state.operation) return false;
        const version = ++state.selectionVersion;
        if (!await canDiscardDirty() || state.destroyed || state.operation || version !== state.selectionVersion) return false;
        state.lastSelectedIdBeforeCreate = Number(state.selectedId || 0);
        state.selectedId = 0;
        state.model = cloneData(makeDefault());
        state.persistedModel = null;
        state.dirty?.setClean?.();
        renderForm();
        renderList();
        setHeader();
        setButtons(true);
        return true;
      };

      const cancelLocalChanges = async () => {
        if (state.destroyed || state.operation || !state.model) return false;
        const version = ++state.selectionVersion;

        // cancel creation of local draft
        if (!hasSelectedRecord()) {
          const fallbackId = Number(state.lastSelectedIdBeforeCreate || 0);
          const fallbackRow = fallbackId
            ? state.rows.find((row) => Number(row?.ID || 0) === fallbackId)
              || state.rowsRaw.find((row) => Number(row?.ID || 0) === fallbackId)
            : null;
          await (fallbackRow
            ? selectRow(fallbackRow, { confirmDirty: false })
            : selectRow(null, { confirmDirty: false }));
          setStatus('Record creation cancelled.', 'ok');
          return true;
        }

        if (!state.dirty?.isDirty?.()) return false;
        if (!await canDiscardDirty() || state.destroyed || state.operation || version !== state.selectionVersion) return false;
        state.model = cloneData(state.persistedModel || state.model);
        state.dirty?.setClean?.();
        renderForm();
        renderList();
        setHeader();
        setButtons(true);
        setStatus('Local changes discarded.', 'ok');
        return true;
      };

      const onDirtyStatusRequest = (event) => {
        if (!window.EditorCore.Http.isSameOriginMessage(event, window.parent)) return;
        const data = event.data || {};
        if (data?.type !== 'editor-shell:request-dirty-status') return;
        window.EditorCore.Http.postMessage(window.parent, {
          type: 'editor-shell:dirty-status',
          requestId: data.requestId,
          dirty: state.operation || !!state.dirty?.isDirty?.(),
        });
      };

      const save = async () => {
        if (state.destroyed || state.operation || !state.model) return false;
        const validation = state.form?.validate?.({ focus: true });
        if (validation && !validation.ok) {
          const message = validation.errors[0]?.message || 'Check the highlighted fields.';
          setStatus(message, 'err');
          toast(message, 'error');
          return false;
        }
        const validationError = typeof cfg.validateModel === 'function'
          ? cfg.validateModel(state.model)
          : '';
        if (validationError) {
          const message = String(validationError);
          setStatus(message, 'err');
          toast(message, 'error');
          return false;
        }
        const existing = hasSelectedRecord();
        const id = state.selectedId;
        const version = state.selectionVersion;
        const snapshot = cloneData(state.model);
        let payload;
        try {
          payload = cloneData(toPayload(snapshot));
        } catch (error) {
          setStatus(error?.message || 'Save failed', 'err');
          return false;
        }
        return runOperation(async () => {
          setStatus('Saving…');
          try {
            if (existing) {
              // DBCrud auto-records for sync
              await window.EditorCore.DBCrud.update(resource, id, payload);
              if (state.destroyed || version !== state.selectionVersion) return false;
              state.persistedModel = cloneData(snapshot);
              if (isRequestEditing()) {
                setStatus('The new request version was saved. The game database was not changed.', 'ok');
                toast('The request version has been updated', 'success');
              } else if (isGuest()) {
                setStatus('Draft changes are written to the database only after admin approval.', 'ok');
                toast('Added to the request draft', 'info');
              } else {
                setStatus('Changes saved.', 'ok');
                toast('Saved', 'success');
              }
            } else {
              const res = await window.EditorCore.DBCrud.create(resource, payload);
              const createdId = Number(res?.id);
              if (res?.id == null || String(res.id).trim() === ''
                || !Number.isSafeInteger(createdId) || createdId < 0 || (createdId === 0 && resource !== 'worlds')) {
                throw new Error('The server did not confirm the new record ID. Refresh the list before retrying.');
              }
              if (state.destroyed || version !== state.selectionVersion) return false;
              state.selectedId = createdId;
              state.persistedModel = cloneData(snapshot);
              state.lastSelectedIdBeforeCreate = 0;
              if (isRequestEditing()) {
                setStatus('The new record was added to the new request version. The game database was not changed.', 'ok');
                toast('The new record was added to the request', 'success');
              } else if (isGuest()) {
                setStatus('New record in the request draft — submit it to the administrator.', 'ok');
                toast('New record in the draft', 'info');
              } else {
                setStatus('Record created.', 'ok');
                toast('Created', 'success');
              }
            }
            if (state.destroyed || version !== state.selectionVersion) return false;
            state.dirty?.setClean?.();
            storeLastRecordId(state.selectedId);
            renderList();
            setHeader();
            await loadList({ forceServer: true });
            updateGuestHint();
          } catch (err) {
            const msg = err?.message || 'Save failed';
            setStatus(msg, 'err');
            toast(msg, 'error');
            return false;
          }
        });
      };

      const remove = async () => {
        const id = Number(state.selectedId || 0);
        if (state.destroyed || state.operation || !hasSelectedRecord()) return false;
        const version = state.selectionVersion;
        if (!await EditorCore.Page.confirm('Delete the record?')) return false;
        // A second confirmation or a background refresh must not delete another
        // record (or ID 0) after the selection changed while the dialog was open.
        if (state.destroyed || state.operation || version !== state.selectionVersion || Number(state.selectedId || 0) !== id) return false;
        return runOperation(async () => {
          setStatus('Deleting…');
          try {
            // DBCrud auto-records for sync
            await window.EditorCore.DBCrud.remove(resource, id);
            if (state.destroyed || version !== state.selectionVersion) return false;
            if (isRequestEditing()) {
              toast('The deletion was added to the new request version', 'success');
              setStatus('The deletion was saved in the request. The game database was not changed.', 'ok');
            } else if (isGuest()) {
              toast('Deletion added to the request draft', 'info');
              setStatus('Deletion is drafted — the record is removed only after admin approval.', 'ok');
            } else {
              toast('Deleted', 'success');
              setStatus('Deleted.', 'ok');
            }
            await selectRow(null, { confirmDirty: false, internal: true });
            await loadList({ forceServer: true });
          } catch (err) {
            const msg = err?.message || 'Delete failed';
            setStatus(msg, 'err');
            toast(msg, 'error');
            return false;
          }
        });
      };

      // Кнопка «вернуть как в базе» живёт внутри поля формы, поэтому обработчик
      // делегирован: он переживает перерисовку полей.
      els.form?.addEventListener('click', (event) => {
        const button = event.target?.closest?.('[data-guest-revert]');
        if (!button) return;
        event.preventDefault();
        event.stopPropagation();
        revertGuestField(button.dataset.guestRevert || '');
      }, { signal: events.signal });

      if (els.list) {
        els.list.addEventListener('click', async (event) => {
          if (state.operation) return;
          const moreBtn = event.target?.closest?.('[data-editor-role="list-more"]');
          if (moreBtn) {
            state.visibleCount += 250;
            renderList();
            return;
          }
          const btn = event.target?.closest?.('[data-id]');
          if (!btn) return;
          const id = Number(btn.getAttribute('data-id') || 0);
          const row = state.rows.find(r => Number(r.ID) === id);
          if (row) await selectRow(row);
        }, { signal: events.signal });
      }

      els.refresh?.addEventListener('click', loadList, { signal: events.signal });
      els.newBtn?.addEventListener('click', createNew, { signal: events.signal });
      els.save?.addEventListener('click', save, { signal: events.signal });
      els.cancel?.addEventListener('click', cancelLocalChanges, { signal: events.signal });
      els.del?.addEventListener('click', remove, { signal: events.signal });

      if (els.search) {
        const clearSearch = () => {
          if (!els.search.value) return;
          els.search.value = '';
          loadList({ silent: true });
        };

        const clearBtn = root.querySelector('[data-editor-role="search-clear"]');
        clearBtn?.addEventListener('click', clearSearch, { signal: events.signal });

        els.search.addEventListener('keydown', (event) => {
          if (event.key !== 'Escape') return;
          event.preventDefault();
          clearSearch();
        }, { signal: events.signal });

        onSearch = debounce(() => loadList(), 250);
        els.search.addEventListener('input', onSearch, { signal: events.signal });
      }

      window.addEventListener('message', onDirtyStatusRequest, { signal: events.signal });

      // Guest mode: the role and the local draft are resolved asynchronously.
      const onGuestUpdated = () => { if (!state.destroyed) { renderList(); updateGuestHint(); } };
      const onGuestReady = () => { if (state.destroyed) return; renderList(); updateGuestHint(); if (!state.model) loadList({ forceServer: true }); };
      const onGuestResolved = () => {
        if (state.destroyed || !isGuest()) return;
        window.EditorCore?.DBCrud?.invalidateCache?.();
        loadList({ forceServer: true });
        updateGuestHint();
      };
      window.addEventListener('editor-core:guest-updated', onGuestUpdated);
      window.addEventListener('editor-core:guest-ready', onGuestReady);
      window.addEventListener('editor-core:guest-request-resolved', onGuestResolved);
      window.addEventListener('editor-core:guest-request-revised', onGuestResolved, { signal: events.signal });

      // Mount sync button in the toolbar if actions area exists
      const actionsArea = root.querySelector('.editor-page-actions') || root.querySelector('.flex.flex-wrap.gap-2');
      if (actionsArea && window.EditorCore?.Sync?.mountSyncButton) {
        const editorName = cfg.editorName || cfg.title || resource;
        window.EditorCore.Sync.mountSyncButton(actionsArea, { editorName });
      }

      setHeader();
      const ready = loadList();

      const api = {
        ready,
        getModel: () => cloneData(state.model),
        refresh: loadList,
        selectRow,
        setStatus,
        destroy: () => {
          if (state.destroyed) return;
          state.destroyed = true;
          state.selectionVersion += 1;
          events.abort();
          timers.forEach(timer => window.clearTimeout(timer));
          timers.clear();
          listRequestSeq += 1;
          onSearch?.cancel?.();
          state.searchRequest.controller?.abort?.();
          state.form?.destroy?.();
          window.removeEventListener('message', onDirtyStatusRequest);
          window.removeEventListener('editor-core:guest-updated', onGuestUpdated);
          window.removeEventListener('editor-core:guest-ready', onGuestReady);
          window.removeEventListener('editor-core:guest-request-resolved', onGuestResolved);
          window.removeEventListener('editor-core:guest-request-revised', onGuestResolved);
          guestHintEl?.remove();
          if (root.__editorDbRuntime === api) delete root.__editorDbRuntime;
        },
      };
      root.__editorDbRuntime = api;
      return api;
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DbEditor = DbEditor;
})();
