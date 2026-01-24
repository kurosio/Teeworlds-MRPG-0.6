(() => {
  // Lightweight CRUD runtime for DB editors.
  // Requires: editor-core.bundle.js (DBCrud + FormRuntime + FieldRenderer + UIManager).

  const escapeHtml = (value) => String(value ?? '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');

  const queryRole = (root, role) => root.querySelector(`[data-editor-role="${role}"]`);

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
        del: queryRole(root, 'delete'),
        form: queryRole(root, 'form'),
        status: queryRole(root, 'status'),
        count: queryRole(root, 'count'),
      };

      const fieldOptions = cfg.fieldOptions || window.EditorCore?.defaults?.getFieldRenderOptions?.('event');
      const debounce = window.EditorCore?.utils?.debounce || ((fn) => fn);
      const toast = window.EditorCore?.UI?.toast || (() => {});

      const state = {
        rows: [],
        selectedId: 0,
        model: null,
        form: null,
        dirty: cfg.dirty || null,
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

      const setStatus = (html = '', tone = 'muted') => {
        if (!els.status) return;
        const cls = tone === 'ok'
          ? 'text-emerald-300'
          : tone === 'err'
            ? 'text-red-300'
            : 'editor-muted-text';
        els.status.className = `text-sm ${cls}`;
        els.status.innerHTML = html;
      };

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

      const setButtons = (enabled) => {
        if (els.save) els.save.disabled = !enabled;
        if (els.del) els.del.disabled = !enabled || !state.selectedId;
      };

      const renderForm = () => {
        if (!els.form) return;
        if (!state.model) {
          els.form.innerHTML = `<div class="editor-muted-text">Выберите запись слева</div>`;
          setButtons(false);
          return;
        }
        if (!state.form) {
          state.form = window.EditorCore.FormRuntime.mount(els.form, {
            fields: cfg.fields || {},
            data: state.model,
            fieldOptions,
            onChange: (data) => {
              state.model = data;
              state.dirty?.markDirty?.();
            }
          });
        } else {
          state.form.setData(state.model);
        }
        setButtons(true);
      };

      const renderList = () => {
        if (!els.list) return;
        const rows = state.rows || [];
        if (els.count) els.count.textContent = rows.length ? `${rows.length}` : '—';
        if (!rows.length) {
          els.list.innerHTML = `<div class="editor-empty-state">Нет данных</div>`;
          return;
        }

        const html = rows.map((row) => {
          const label = typeof cfg.listItem === 'function'
            ? cfg.listItem(row)
            : { title: row?.Name || `#${row?.ID}`, subtitle: row?.Path || '' };
          const active = Number(row?.ID) === Number(state.selectedId);
          return `
            <button type="button" class="w-full text-left editor-row ${active ? 'ring-2 ring-indigo-500/40' : ''}" data-id="${escapeHtml(row?.ID)}">
              <div class="font-semibold truncate">${escapeHtml(label.title || '')}</div>
              ${label.subtitle ? `<div class="text-xs editor-muted-text truncate">${escapeHtml(label.subtitle)}</div>` : ''}
            </button>
          `;
        }).join('');
        els.list.innerHTML = `<div class="editor-list">${html}</div>`;
      };

      const selectRow = (row) => {
        if (!row) {
          state.selectedId = 0;
          state.model = null;
          state.dirty?.setClean?.();
          renderForm();
          renderList();
          setHeader();
          return;
        }
        state.selectedId = Number(row.ID || 0);
        state.model = toModel(row);
        state.dirty?.setClean?.();
        renderForm();
        renderList();
        setHeader();
      };

      const loadList = async () => {
        const search = els.search?.value?.trim() || '';
        setStatus('Загрузка списка…');
        try {
          const res = await window.EditorCore.DBCrud.list(resource, { search, limit: 5000, offset: 0 });
          const rows = Array.isArray(res.rows) ? res.rows : [];
          state.rows = typeof cfg.transformList === 'function'
            ? cfg.transformList(rows, { search }) || []
            : rows;
          setStatus(state.rows.length ? 'Готово.' : 'Список пуст.', state.rows.length ? 'ok' : 'muted');
          renderList();
          if (state.selectedId) {
            const found = state.rows.find(r => Number(r.ID) === Number(state.selectedId));
            if (found) selectRow(found);
          } else if (!state.model && state.rows.length) {
            selectRow(state.rows[0]);
          }
        } catch (err) {
          const msg = err?.message || 'Ошибка загрузки';
          setStatus(msg, 'err');
          toast(msg, 'error');
        }
      };

      const createNew = () => {
        state.selectedId = 0;
        state.model = makeDefault();
        state.dirty?.setClean?.();
        renderForm();
        renderList();
        setHeader();
        setButtons(true);
      };

      const save = async () => {
        if (!state.model) return;
        const payload = toPayload(state.model);
        setStatus('Сохранение…');
        try {
          if (state.selectedId) {
            await window.EditorCore.DBCrud.update(resource, state.selectedId, payload);
            setStatus('Изменения сохранены.', 'ok');
            toast('Сохранено', 'success');
          } else {
            const res = await window.EditorCore.DBCrud.create(resource, payload);
            state.selectedId = Number(res.id || 0);
            setStatus('Запись создана.', 'ok');
            toast('Создано', 'success');
          }
          state.dirty?.setClean?.();
          await loadList();
        } catch (err) {
          const msg = err?.message || 'Ошибка сохранения';
          setStatus(msg, 'err');
          toast(msg, 'error');
        }
      };

      const remove = async () => {
        if (!state.selectedId) return;
        if (!confirm('Удалить запись?')) return;
        setStatus('Удаление…');
        try {
          await window.EditorCore.DBCrud.remove(resource, state.selectedId);
          toast('Удалено', 'success');
          setStatus('Удалено.', 'ok');
          selectRow(null);
          await loadList();
        } catch (err) {
          const msg = err?.message || 'Ошибка удаления';
          setStatus(msg, 'err');
          toast(msg, 'error');
        }
      };

      if (els.list) {
        els.list.addEventListener('click', (event) => {
          const btn = event.target?.closest?.('[data-id]');
          if (!btn) return;
          const id = Number(btn.getAttribute('data-id') || 0);
          const row = state.rows.find(r => Number(r.ID) === id);
          if (row) selectRow(row);
        });
      }

      els.refresh?.addEventListener('click', loadList);
      els.newBtn?.addEventListener('click', createNew);
      els.save?.addEventListener('click', save);
      els.del?.addEventListener('click', remove);

      if (els.search) {
        els.search.addEventListener('input', debounce(loadList, 250));
      }

      loadList();
      setHeader();

      return {
        refresh: loadList,
        selectRow,
        setStatus,
      };
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DbEditor = DbEditor;
})();
