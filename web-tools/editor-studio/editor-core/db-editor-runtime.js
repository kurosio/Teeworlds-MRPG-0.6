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
      };
      let listRequestSeq = 0;
      let onSearch = null;
      // Ссылка «открыть запись» (?record=&field=) из вкладки «Запросы».
      const deepLink = window.EditorCore?.DeepLink || null;
      let deepLinkPending = !!deepLink?.active;

      /** Спрашивает подтверждение, если в форме есть несохранённые правки. */
      const canDiscardDirty = async () => {
        if (!state.dirty?.isDirty?.()) return true;
        return EditorCore.Page.confirm('Есть несохранённые изменения. Продолжить без сохранения?');
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

      const setHeader = () => {
        if (!els.title && !els.subtitle) return;
        if (!state.model) {
          if (els.title) els.title.textContent = cfg.emptyTitle || 'Выберите запись';
          if (els.subtitle) els.subtitle.textContent = cfg.emptySubtitle || 'или создайте новую';
          return;
        }
        const title = typeof cfg.getTitle === 'function'
          ? cfg.getTitle(state.model, state.selectedId)
          : (state.model?.Name || `Запись #${state.selectedId || 'new'}`);
        const subtitle = typeof cfg.getSubtitle === 'function'
          ? cfg.getSubtitle(state.model, state.selectedId)
          : '';
        if (els.title) els.title.textContent = title;
        if (els.subtitle) els.subtitle.textContent = subtitle;
      };

      const guestBadgeHtml = (meta) => {
        if (!meta) return '';
        const map = {
          create: ['Новая', 'fa-plus'],
          update: ['Изменено', 'fa-pen'],
          delete: ['Удаление', 'fa-trash'],
        };
        const [text, icon] = map[meta.state] || map.update;
        const submitted = meta.status === 'submitted';
        return `<span class="guest-row-badge ${submitted ? 'is-submitted' : 'is-draft'}" title="${submitted ? 'Отправлено в запросе, ждёт проверки администратора' : 'Локальное изменение — в базу не записано'}"><i class="fa-solid ${submitted ? 'fa-hourglass-half' : icon}"></i>${escapeHtml(text)}</span>`;
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
        button.title = 'Вернуть исходное значение из базы';
        button.setAttribute('aria-label', `Вернуть исходное значение поля ${column}`);
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
          ? `Поле изменено в запросе ${meta?.reqId || ''} — ждёт проверки администратора`
          : 'Поле изменено в вашем черновике — в базу не записано';
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
              throw new Error('Не удалось прочитать значение из базы — обновите список и повторите.');
            }
            const result = await Guest.Draft.revertField(resource, id, column, baseRow);
            if (result?.removed) applyBaseValue(original[column], column);
            else updateGuestHint();
            renderList();
            setStatus(result?.dropped
              ? 'Поле возвращено: изменений в черновике больше нет.'
              : `Поле ${column} возвращено к значению из базы.`, 'ok');
            return true;
          } catch (error) {
            const msg = error?.message || 'Не удалось вернуть поле к исходному значению';
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
        const title = meta.state === 'create' ? 'Новая запись' : meta.state === 'delete' ? 'Помечена на удаление' : 'Локальное изменение';
        const note = submitted
          ? `отправлено в запросе <b>${escapeHtml(meta.reqId || '')}</b> и ждёт проверки администратора`
          : 'черновик запроса — в базу данных пока ничего не записано';
        const fields = meta.fields.length
          ? `<div class="guest-row-hint-fields">Изменённые поля: ${meta.fields.slice(0, 8).map((f) => `<code>${escapeHtml(f)}</code>`).join(' ')}${meta.fields.length > 8 ? ' …' : ''}</div>`
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
        if (els.del) els.del.disabled = !canEdit || !state.selectedId;
      };

      const runOperation = async (task) => {
        if (state.operation) return false;
        state.operation = true;
        setButtons(false);
        try {
          await task();
          return true;
        } finally {
          state.operation = false;
          setButtons(!!state.model);
        }
      };

      const renderForm = () => {
        if (!els.form) return;
        ensureGuestHint();
        if (!state.model) {
          els.form.innerHTML = `<div class="editor-empty-state editor-field--span-all">Выберите запись слева или создайте новую, чтобы начать редактирование.</div>`;
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
        if (!els.list) return;
        const rows = state.rows || [];
        const requestTarget = window.EditorCore?.RequestContext?.target?.();
        const targetId = requestTarget?.resource === resource ? Number(requestTarget.id) : 0;
        const orderedRows = targetId && rows.some((row) => Number(row?.ID) === targetId)
          ? [...rows.filter((row) => Number(row?.ID) === targetId), ...rows.filter((row) => Number(row?.ID) !== targetId)]
          : rows;
        const rawTotal = Array.isArray(state.rowsRaw) ? state.rowsRaw.length : 0;
        if (els.count) {
          if (state.search && rawTotal > rows.length) {
            els.count.textContent = `${rows.length} из ${rawTotal}`;
          } else {
            els.count.textContent = rows.length ? `${rows.length}` : '0';
          }
        }
        if (!rows.length) {
          // Пустой список должен объяснять, что делать дальше.
          const message = state.search
            ? `Ничего не найдено по запросу «${escapeHtml(state.search)}». Измените запрос или очистите поиск.`
            : `Записей пока нет — нажмите «${escapeHtml(cfg.newLabel || (els.newBtn?.textContent || '').trim() || 'Новый')}», чтобы создать первую.`;
          els.list.innerHTML = `<div class="editor-empty-state">${message}</div>`;
          return;
        }

        const visibleRows = orderedRows.slice(0, state.visibleCount);
        const html = visibleRows.map((row) => {
          const label = typeof cfg.listItem === 'function'
            ? cfg.listItem(row)
            : { title: row?.Name || `#${row?.ID}`, subtitle: row?.Path || '' };
          const active = Number(row?.ID) === Number(state.selectedId);
          const meta = guestMeta(row?.ID);
          const badge = meta ? guestBadgeHtml(meta) : '';
          return `
            <button type="button" class="w-full text-left editor-row ${active ? 'editor-row-active' : ''}" data-id="${escapeHtml(row?.ID)}">
              <div class="flex items-center gap-2">
                <div class="font-semibold truncate">${escapeHtml(label.title || '')}</div>
                ${badge}
              </div>
              ${label.subtitle ? `<div class="text-xs editor-muted-text truncate">${escapeHtml(label.subtitle)}</div>` : ''}
            </button>
          `;
        }).join('');
        const hasMore = orderedRows.length > visibleRows.length;
        const more = hasMore
          ? `<div class="pt-2"><button type="button" class="editor-btn editor-btn-secondary w-full" data-editor-role="list-more">Показать ещё (${orderedRows.length - visibleRows.length})</button></div>`
          : '';
        els.list.innerHTML = `<div class="editor-list">${html}${more}</div>`;
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
        const id = Number(deepLink?.record) || 0;
        if (!id) return false;
        const inList = state.rows.find((row) => Number(row?.ID) === id) || null;
        let row = inList;
        if (!row) {
          const response = await window.EditorCore.DBCrud.get(resource, id).catch(() => null);
          row = response?.row || null;
        }
        if (!row) return false;
        deepLink?.markHandled?.();
        if (!inList) {
          // Записи не было в списке — показываем её в списке, чтобы форма и
          // подсветка строки не расходились.
          state.rows = [row, ...state.rows];
          renderList();
        }
        await selectRow(row, { confirmDirty: false });
        window.setTimeout(() => deepLink?.revealField?.(), 250);
        return true;
      };

      const applyRows = async (rows, { search, isRaw = false } = {}) => {
        const safeRows = Array.isArray(rows) ? rows : [];
        if (isRaw) state.rowsRaw = safeRows;
        state.search = normalizeSearchText(search ?? state.search);
        state.rows = safeRows;
        state.visibleCount = 250;
        setStatus(state.rows.length ? '' : 'Список пуст.', 'muted');
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
        } else if (state.selectedId) {
          const found = state.rows.find(r => Number(r.ID) === Number(state.selectedId));
          if (found && !state.dirty?.isDirty?.()) await selectRow(found, { confirmDirty: false, refresh: true });
        } else if (!state.model && state.rows.length) {
          await selectRow(state.rows[0], { confirmDirty: false });
        }
      };

      const selectRow = async (row, options = {}) => {
        const confirmDirty = options?.confirmDirty !== false;
        const nextId = Number(row?.ID || 0);
        const currentId = Number(state.selectedId || 0);

        if (nextId && nextId === currentId && !options.refresh) return true;
        if (confirmDirty && (!await canDiscardDirty() || state.operation)) return false;

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
        renderForm();
        renderList();
        setHeader();
        return true;
      };

      const loadList = async (options = {}) => {
        const requestId = ++listRequestSeq;
        const { forceServer = false, silent = false } = options;
        const search = normalizeSearchText(els.search?.value);
        state.search = search;

        const canUseLocal = !forceServer
          && Array.isArray(state.rowsRaw)
          && state.rowsRaw.length
          && search.length < localMinLength;

        // Even local filtering supersedes the pending server search.
        state.searchRequest.controller?.abort?.();
        els.list?.setAttribute('aria-busy', 'false');

        if (canUseLocal) {
          const filtered = filterRowsLocally(state.rowsRaw, search);
          const transformed = typeof cfg.transformList === 'function'
            ? cfg.transformList(filtered, { search }) || []
            : filtered;
          await applyRows(transformed, { search });
          return;
        }

        const token = state.searchRequest.token + 1;
        state.searchRequest.token = token;
        state.searchRequest.controller?.abort?.();
        const controller = new AbortController();
        state.searchRequest.controller = controller;

        if (!silent) setStatus(search ? 'Идёт поиск…' : 'Загрузка списка…');
        els.list?.setAttribute('aria-busy', 'true');
        try {
          const res = await window.EditorCore.DBCrud.list(resource, {
            search,
            limit: 2000,
            offset: 0,
            signal: controller.signal,
          });
          if (requestId !== listRequestSeq) return;
          if (token !== state.searchRequest.token) return;

          const rows = Array.isArray(res.rows) ? res.rows : [];
          const transformed = typeof cfg.transformList === 'function'
            ? cfg.transformList(rows, { search }) || []
            : rows;
          await applyRows(transformed, { search, isRaw: true });
        } catch (err) {
          if (requestId !== listRequestSeq || controller.signal.aborted || token !== state.searchRequest.token) return;
          const msg = err?.message || 'Ошибка загрузки';
          setStatus(msg, 'err');
          toast(msg, 'error');
        } finally {
          if (requestId === listRequestSeq) els.list?.setAttribute('aria-busy', 'false');
        }
      };

      const createNew = async () => {
        if (state.operation || !await canDiscardDirty() || state.operation) return false;
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
        if (state.operation || !state.model) return false;

        // cancel creation of local draft
        if (!state.selectedId) {
          const fallbackId = Number(state.lastSelectedIdBeforeCreate || 0);
          const fallbackRow = fallbackId
            ? state.rows.find((row) => Number(row?.ID || 0) === fallbackId)
              || state.rowsRaw.find((row) => Number(row?.ID || 0) === fallbackId)
            : null;
          await (fallbackRow
            ? selectRow(fallbackRow, { confirmDirty: false })
            : selectRow(null, { confirmDirty: false }));
          setStatus('Создание записи отменено.', 'ok');
          return true;
        }

        if (!state.dirty?.isDirty?.()) return false;
        state.model = cloneData(state.persistedModel || state.model);
        state.dirty?.setClean?.();
        renderForm();
        renderList();
        setHeader();
        setButtons(true);
        setStatus('Локальные изменения отменены.', 'ok');
        return true;
      };

      const onDirtyStatusRequest = (event) => {
        if (!window.EditorCore.Http.isSameOriginMessage(event, window.parent)) return;
        const data = event.data || {};
        if (data?.type !== 'editor-shell:request-dirty-status') return;
        window.EditorCore.Http.postMessage(window.parent, {
          type: 'editor-shell:dirty-status',
          requestId: data.requestId,
          dirty: !!state.dirty?.isDirty?.(),
        });
      };

      const save = async () => {
        if (!state.model) return;
        const validationError = typeof cfg.validateModel === 'function'
          ? cfg.validateModel(state.model)
          : '';
        if (validationError) {
          const message = String(validationError);
          setStatus(message, 'err');
          toast(message, 'error');
          return;
        }
        const payload = toPayload(state.model);
        return runOperation(async () => {
          setStatus('Сохранение…');
          try {
            if (state.selectedId) {
              // DBCrud auto-records for sync
              await window.EditorCore.DBCrud.update(resource, state.selectedId, payload);
              if (isRequestEditing()) {
                setStatus('Новая версия запроса сохранена. Игровая база не изменялась.', 'ok');
                toast('Версия запроса обновлена', 'success');
              } else if (isGuest()) {
                setStatus('Изменения в черновике запроса — в базу они попадут только после одобрения администратора.', 'ok');
                toast('Добавлено в черновик запроса', 'info');
              } else {
                setStatus('Изменения сохранены.', 'ok');
                toast('Сохранено', 'success');
              }
            } else {
              const res = await window.EditorCore.DBCrud.create(resource, payload);
              state.selectedId = Number(res.id || 0);
              state.lastSelectedIdBeforeCreate = 0;
              if (isRequestEditing()) {
                setStatus('Создание добавлено в новую версию запроса. Игровая база не изменялась.', 'ok');
                toast('Создание добавлено в запрос', 'success');
              } else if (isGuest()) {
                setStatus('Новая запись в черновике запроса — отправьте её администратору.', 'ok');
                toast('Новая запись в черновике', 'info');
              } else {
                setStatus('Запись создана.', 'ok');
                toast('Создано', 'success');
              }
            }
            state.dirty?.setClean?.();
            await loadList({ forceServer: true });
            updateGuestHint();
          } catch (err) {
            const msg = err?.message || 'Ошибка сохранения';
            setStatus(msg, 'err');
            toast(msg, 'error');
          }
        });
      };

      const remove = async () => {
        const id = Number(state.selectedId || 0);
        if (state.operation || !id) return false;
        if (!await EditorCore.Page.confirm('Удалить запись?')) return false;
        // A second confirmation or a background refresh must not delete another
        // record (or ID 0) after the selection changed while the dialog was open.
        if (state.operation || Number(state.selectedId || 0) !== id) return false;
        return runOperation(async () => {
          setStatus('Удаление…');
          try {
            // DBCrud auto-records for sync
            await window.EditorCore.DBCrud.remove(resource, id);
            if (isRequestEditing()) {
              toast('Удаление добавлено в новую версию запроса', 'success');
              setStatus('Удаление сохранено в запросе. Игровая база не изменялась.', 'ok');
            } else if (isGuest()) {
              toast('Удаление добавлено в черновик запроса', 'info');
              setStatus('Удаление в черновике — запись удалится только после одобрения администратора.', 'ok');
            } else {
              toast('Удалено', 'success');
              setStatus('Удалено.', 'ok');
            }
            await selectRow(null, { confirmDirty: false });
            await loadList({ forceServer: true });
          } catch (err) {
            const msg = err?.message || 'Ошибка удаления';
            setStatus(msg, 'err');
            toast(msg, 'error');
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
      });

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
        });
      }

      els.refresh?.addEventListener('click', loadList);
      els.newBtn?.addEventListener('click', createNew);
      els.save?.addEventListener('click', save);
      els.cancel?.addEventListener('click', cancelLocalChanges);
      els.del?.addEventListener('click', remove);

      if (els.search) {
        const clearSearch = () => {
          if (!els.search.value) return;
          els.search.value = '';
          loadList({ silent: true });
        };

        const clearBtn = root.querySelector('[data-editor-role="search-clear"]');
        clearBtn?.addEventListener('click', clearSearch);

        els.search.addEventListener('keydown', (event) => {
          if (event.key !== 'Escape') return;
          event.preventDefault();
          clearSearch();
        });

        onSearch = debounce(() => loadList(), 250);
        els.search.addEventListener('input', onSearch);
      }

      window.addEventListener('message', onDirtyStatusRequest);

      // Guest mode: the role and the local draft are resolved asynchronously.
      const onGuestUpdated = () => { renderList(); updateGuestHint(); };
      const onGuestReady = () => { renderList(); updateGuestHint(); if (state.selectedId === 0) loadList({ forceServer: true }); };
      const onGuestResolved = () => {
        if (!isGuest()) return;
        window.EditorCore?.DBCrud?.invalidateCache?.();
        loadList({ forceServer: true });
        updateGuestHint();
      };
      window.addEventListener('editor-core:guest-updated', onGuestUpdated);
      window.addEventListener('editor-core:guest-ready', onGuestReady);
      window.addEventListener('editor-core:guest-request-resolved', onGuestResolved);
      window.addEventListener('editor-core:guest-request-revised', onGuestResolved);

      // Mount sync button in the toolbar if actions area exists
      const actionsArea = root.querySelector('.editor-page-actions') || root.querySelector('.flex.flex-wrap.gap-2');
      if (actionsArea && window.EditorCore?.Sync?.mountSyncButton) {
        const editorName = cfg.editorName || cfg.title || resource;
        window.EditorCore.Sync.mountSyncButton(actionsArea, { editorName });
      }

      loadList();
      setHeader();

      return {
        refresh: loadList,
        selectRow,
        setStatus,
        destroy: () => {
          listRequestSeq += 1;
          onSearch?.cancel?.();
          state.searchRequest.controller?.abort?.();
          state.form?.destroy?.();
          window.removeEventListener('message', onDirtyStatusRequest);
          window.removeEventListener('editor-core:guest-updated', onGuestUpdated);
          window.removeEventListener('editor-core:guest-ready', onGuestReady);
          window.removeEventListener('editor-core:guest-request-resolved', onGuestResolved);
          window.removeEventListener('editor-core:guest-request-revised', onGuestResolved);
        },
      };
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DbEditor = DbEditor;
})();
