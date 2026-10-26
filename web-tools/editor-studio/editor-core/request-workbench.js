(() => {
  if (window.EditorCore?.RequestWorkbench) return;
  const namespace = (window.EditorCore = window.EditorCore || {});
  const esc = (value) => namespace.utils.escapeHtml(value ?? '');
  const clone = (value) => namespace.utils.cloneData(value);

  // The manifest is the single source of editor URLs. Only the quest step
  // resource needs an alias because quests and their steps share one editor.
  const RESOURCE_PAGES = Object.fromEntries((window.STUDIO_MANIFEST || [])
    .flatMap((group) => group.items || [])
    .filter((item) => item?.key && item?.src)
    .map((item) => [item.key, { key: item.key, src: item.src }]));
  if (RESOURCE_PAGES.quests) RESOURCE_PAGES.quest_bots = { ...RESOURCE_PAGES.quests };
  if (RESOURCE_PAGES.quest_boards) RESOURCE_PAGES.quest_board_links = { ...RESOURCE_PAGES.quest_boards };

  const linkParams = ({ request = '', resource = '', record = '', field = '', edit = '' } = {}) => {
    const params = { request, resource, record, field, edit };
    Object.keys(params).forEach((key) => {
      params[key] = String(params[key] ?? '');
      if (!params[key]) delete params[key];
    });
    return params;
  };

  const editorUrl = (resource, options = {}) => {
    const page = RESOURCE_PAGES[resource];
    if (!page) return '';
    const query = new URLSearchParams(linkParams({ ...options, resource })).toString();
    return page.src + (query ? `?${query}` : '');
  };

  const openEditor = (resource, options = {}) => {
    const page = RESOURCE_PAGES[resource];
    if (!page) {
      namespace.UI?.toast?.('This section has no dedicated editor', 'warning');
      return false;
    }
    const params = linkParams({ ...options, resource });
    if (window.parent && window.parent !== window) {
      return namespace.Http.postMessage(window.parent, {
        type: 'editor-shell:open-editor', key: page.key, params,
      });
    }
    window.location.href = editorUrl(resource, options);
    return true;
  };

  const fieldState = (value, json) => ({
    raw: value === null || value === undefined ? ''
      : json ? JSON.stringify(value, null, 2) : String(value),
    isNull: value === null || value === undefined,
    numeric: typeof value === 'number',
    json: !!json,
  });

  const operationState = (op, resources) => {
    const jsonCols = resources[op.resource]?.json || [];
    const fields = {};
    Object.entries(op.data || {}).forEach(([column, value]) => {
      fields[column] = fieldState(value, jsonCols.includes(column));
    });
    return {
      type: op.type, resource: op.resource, id: op.id ?? null, tempId: op.tempId ?? null,
      before: clone(op.before || null), fields,
    };
  };

  const displayValue = (value) => {
    if (value === null || value === undefined) return 'NULL';
    if (typeof value === 'object') {
      try { return JSON.stringify(value); } catch { return String(value); }
    }
    return String(value);
  };

  const jsonFieldValue = (raw) => {
    const parsed = JSON.parse(raw);
    // Objects/arrays are encoded by the server. Keep scalar JSON as source
    // text so strings, booleans and JSON null survive a revision round-trip.
    return parsed !== null && typeof parsed === 'object' ? parsed : raw.trim();
  };

  const open = (request, { config, onSaved } = {}) => {
    if (!request || request.status !== 'pending' || document.querySelector('.cr-workbench-modal')) return false;
    const resources = config?.resources || {};
    let operations = (request.operations || []).map((op) => operationState(op, resources));
    let version = Math.max(1, Number(request.version) || 1);
    let dirty = false;
    let saving = false;
    const previousFocus = document.activeElement;

    const modal = document.createElement('div');
    modal.className = 'fixed inset-0 z-[800] flex items-center justify-center editor-modal-backdrop cr-workbench-modal';
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-label', `Editing request  ${request.id}`);
    modal.innerHTML = `
      <div class="editor-modal-content cr-workbench">
        <div class="cr-workbench-head">
          <div class="min-w-0">
            <h3 class="text-xl font-bold">Editing the request <span class="cr-col">${esc(request.id)}</span></h3>
            <div class="text-sm cr-muted mt-1">Version <b data-version>${version}</b> · changes are saved in the request, not in the game database.</div>
          </div>
          <button type="button" class="editor-icon-btn" data-close title="Close"><i class="fa-solid fa-xmark"></i></button>
        </div>
        <div class="cr-note cr-note--info"><i class="fa-solid fa-circle-info"></i><div>
          You can fix values and add missing fields or operations. After saving, the diff and warnings are recalculated. The administrator sees the updated version before accepting it.
        </div></div>
        <div class="cr-workbench-add">
          <select class="editor-input form-select" data-new-resource aria-label="Section">
            ${Object.entries(resources).map(([key, resource]) => `<option value="${esc(key)}">${esc(resource.title || key)}</option>`).join('')}
          </select>
          <select class="editor-input form-select" data-new-type aria-label="Operation type">
            <option value="update">Update</option><option value="create">Create</option><option value="delete">Delete</option>
          </select>
          <input class="editor-input form-input" data-new-id type="number" min="1" step="1" value="1" aria-label="Record ID" />
          <button type="button" class="editor-btn editor-btn-secondary" data-add-operation><i class="fa-solid fa-plus"></i><span>Add</span></button>
        </div>
        <div class="cr-workbench-ops" data-operations></div>
        <div class="cr-workbench-foot">
          <div class="text-sm" data-status aria-live="polite">Changes are applied to the request only after “Save”.</div>
          <div class="flex gap-2 flex-wrap">
            <button type="button" class="editor-btn editor-btn-secondary" data-close>Cancel</button>
            <button type="button" class="editor-btn editor-btn-primary" data-save><i class="fa-solid fa-floppy-disk"></i><span>Save version</span></button>
          </div>
        </div>
      </div>`;
    document.body.appendChild(modal);
    document.body.classList.add('cr-workbench-open');

    const operationsEl = modal.querySelector('[data-operations]');
    const statusEl = modal.querySelector('[data-status]');
    const typeEl = modal.querySelector('[data-new-type]');
    const idEl = modal.querySelector('[data-new-id]');
    const setStatus = (text, error = false) => {
      statusEl.textContent = text;
      statusEl.classList.toggle('editor-tone-danger', error);
    };
    const markDirty = () => {
      dirty = true;
      setStatus('The request has unsaved changes.');
    };

    const render = () => {
      if (!operations.length) {
        operationsEl.innerHTML = '<div class="cr-empty"><i class="fa-regular fa-folder-open"></i>Add at least one operation.</div>';
        return;
      }
      operationsEl.innerHTML = operations.map((op, index) => {
        const resource = resources[op.resource] || { title: op.resource, columns: [], json: [] };
        const missing = resource.columns.filter((column) => !(column in op.fields));
        const fields = Object.entries(op.fields).map(([column, field]) => {
          const before = op.before && Object.prototype.hasOwnProperty.call(op.before, column)
            ? `<div class="cr-workbench-before">Was:  ${esc(displayValue(op.before[column]))}</div>` : '';
          const input = field.json || field.raw.includes('\n') || field.raw.length > 120
            ? `<textarea class="editor-input form-textarea font-mono" rows="${field.json ? 6 : 3}" data-field="${esc(column)}" ${field.isNull ? 'disabled' : ''}>${esc(field.raw)}</textarea>`
            : `<input class="editor-input form-input ${field.numeric || field.json ? 'font-mono' : ''}" data-field="${esc(column)}" value="${esc(field.raw)}" ${field.isNull ? 'disabled' : ''} />`;
          return `<div class="cr-workbench-field" data-field-row="${esc(column)}">
            <div class="cr-workbench-field-head">
              <div class="min-w-0"><div class="cr-col">${esc(column)}${field.json ? ' <span class="cr-chip">JSON</span>' : ''}</div>${before}</div>
              <div class="flex items-center gap-2">
                <label class="text-xs cr-muted flex items-center gap-1"><input type="checkbox" data-null="${esc(column)}" ${field.isNull ? 'checked' : ''} /> NULL</label>
                <button type="button" class="editor-icon-btn" data-remove-field="${esc(column)}" data-index="${index}" title="Remove  field  of  operations"><i class="fa-solid fa-xmark"></i></button>
              </div>
            </div>
            ${input}
          </div>`;
        }).join('');
        const record = op.type === 'create' ? `new · temporary #${op.tempId}` : `#${op.id}`;
        return `<article class="cr-workbench-op" data-op="${index}">
          <div class="cr-workbench-op-head">
            <div><b>${esc(TYPE_LABEL[op.type] || op.type)}: ${esc(resource.title)}</b><span class="cr-muted"> · ${esc(record)}</span></div>
            <button type="button" class="editor-icon-btn" data-remove-operation="${index}" title="Remove the operation from the request"><i class="fa-solid fa-trash"></i></button>
          </div>
          ${op.type === 'delete' ? '<div class="text-sm cr-muted">The record will be deleted. Field values are not required.</div>' : `
            <div class="cr-workbench-fields">${fields || '<div class="text-sm cr-muted">No fields yet.</div>'}</div>
            <div class="cr-workbench-add-field">
              <select class="editor-input form-select" data-field-choice>${missing.map((column) => `<option value="${esc(column)}">${esc(column)}</option>`).join('')}</select>
              <button type="button" class="editor-btn editor-btn-secondary editor-btn--sm" data-add-field="${index}" ${missing.length ? '' : 'disabled'}><i class="fa-solid fa-plus"></i><span>Field</span></button>
            </div>`}
        </article>`;
      }).join('');
    };

    const close = async (force = false) => {
      if (saving) return;
      if (!force && dirty && !await namespace.Page.confirm('Close without saving the request changes?')) return;
      modal.remove();
      document.body.classList.remove('cr-workbench-open');
      document.removeEventListener('keydown', onKeydown);
      previousFocus?.focus?.();
    };
    const onKeydown = (event) => {
      if (event.key === 'Escape') { event.preventDefault(); close(); }
    };
    document.addEventListener('keydown', onKeydown);

    const syncType = () => { idEl.disabled = typeEl.value === 'create'; };
    typeEl.addEventListener('change', syncType);
    syncType();
    modal.addEventListener('input', (event) => {
      const input = event.target.closest('[data-field]');
      if (!input) return;
      const op = operations[Number(input.closest('[data-op]')?.dataset.op)];
      const field = op?.fields[input.dataset.field];
      if (!field) return;
      field.raw = input.value;
      markDirty();
    });
    modal.addEventListener('change', (event) => {
      const input = event.target.closest('[data-null]');
      if (!input) return;
      const op = operations[Number(input.closest('[data-op]')?.dataset.op)];
      const field = op?.fields[input.dataset.null];
      if (!field) return;
      field.isNull = input.checked;
      const valueInput = input.closest('[data-field-row]')?.querySelector('[data-field]');
      if (valueInput) valueInput.disabled = field.isNull;
      markDirty();
    });
    modal.addEventListener('click', async (event) => {
      const closeButton = event.target.closest('[data-close]');
      if (closeButton) return close();
      if (event.target.closest('[data-add-operation]')) {
        const resource = modal.querySelector('[data-new-resource]').value;
        const type = typeEl.value;
        const id = Number(idEl.value);
        if (!resources[resource]) return setStatus('Select a section.', true);
        if (type !== 'create' && (!Number.isSafeInteger(id) || id <= 0 || id >= 2000000000)) {
          return setStatus('Enter a valid existing record ID.', true);
        }
        if (operations.some((op) => op.resource === resource && op.type !== 'create' && type !== 'create' && Number(op.id) === id)) {
          return setStatus('This record is already in the request.', true);
        }
        const used = operations.map((op) => Number(op.tempId) || 0);
        const tempId = Math.max(2000000000, ...used) + 1;
        if (type === 'create' && tempId > 2000999999) return setStatus('Temporary IDs are exhausted.', true);
        operations.push({ type, resource, id: type === 'create' ? null : id, tempId: type === 'create' ? tempId : null, before: null, fields: {} });
        markDirty();
        render();
        return;
      }
      const removeOperation = event.target.closest('[data-remove-operation]');
      if (removeOperation) {
        operations.splice(Number(removeOperation.dataset.removeOperation), 1);
        markDirty();
        render();
        return;
      }
      const addField = event.target.closest('[data-add-field]');
      if (addField) {
        const index = Number(addField.dataset.addField);
        const op = operations[index];
        const column = addField.closest('.cr-workbench-add-field')?.querySelector('[data-field-choice]')?.value;
        if (!op || !column) return;
        const json = (resources[op.resource]?.json || []).includes(column);
        op.fields[column] = fieldState('', json);
        markDirty();
        render();
        return;
      }
      const removeField = event.target.closest('[data-remove-field]');
      if (removeField) {
        const op = operations[Number(removeField.dataset.index)];
        if (op) delete op.fields[removeField.dataset.removeField];
        markDirty();
        render();
        return;
      }
      if (!event.target.closest('[data-save]') || saving) return;

      const payload = [];
      try {
        if (!operations.length) throw new Error('Add at least one operation.');
        for (const op of operations) {
          const data = {};
          if (op.type !== 'delete' && !Object.keys(op.fields).length) {
            throw new Error(`The operation “${resources[op.resource]?.title || op.resource}” has no fields.`);
          }
          for (const [column, field] of Object.entries(op.fields)) {
            if (field.isNull) { data[column] = null; continue; }
            if (field.json) {
              if (!field.raw.trim()) throw new Error(`Field ${column}: enter JSON or choose NULL.`);
              try { data[column] = jsonFieldValue(field.raw); }
              catch { throw new Error(`Field ${column}: invalid JSON.`); }
            } else if (field.numeric) {
              if (!field.raw.trim() || !Number.isFinite(Number(field.raw))) throw new Error(`Field ${column}: not a valid number.`);
              data[column] = Number(field.raw);
            } else {
              data[column] = field.raw;
            }
          }
          payload.push({ type: op.type, resource: op.resource, id: op.id, tempId: op.tempId, data });
        }
      } catch (error) {
        setStatus(error.message, true);
        return;
      }

      saving = true;
      const saveButton = modal.querySelector('[data-save]');
      saveButton.disabled = true;
      setStatus('Checking and saving the new version…');
      const response = await namespace.Http.json('api/change-requests.php?action=revise', {
        method: 'POST', body: { id: request.id, version, operations: payload }, timeout: 30000,
      });
      saving = false;
      saveButton.disabled = false;
      if (!response.ok) {
        const conflict = response.code === 'version_conflict';
        setStatus((response.error || 'Failed to save the request.')
          + (conflict ? ' Local fields were left on screen; close the window, reload the request and move the changes into the current version.' : ''), true);
        // Never rebase a stale snapshot automatically: the next click must not
        // overwrite another person's revision merely by using the new version.
        if (conflict) saveButton.disabled = true;
        return;
      }
      dirty = false;
      let localRefreshFailed = false;
      try {
        await onSaved?.(response.request);
      } catch (error) {
        localRefreshFailed = true;
        // The server has already committed the new version. Do not re-submit it
        // as a second revision merely because a local cache/view refresh failed.
        console.warn('[requests] local revision refresh failed', error);
        namespace.UI?.toast?.('The version was saved on the server, but the local view was not updated. Reload the page.', 'warning');
      }
      close(true);
      if (!localRefreshFailed) namespace.UI?.toast?.(`Request updated: version ${response.request?.version || version + 1}`, 'success');
    });

    render();
    return { close, element: modal };
  };

  const TYPE_LABEL = { create: 'Create', update: 'Update', delete: 'Delete' };

  /**
   * Атрибуты ссылки на запись в редакторе. Два режима:
   *   • 'request' — снимок запроса: администратор проверяет и правит запрос;
   *   • 'guest'   — обычный редактор: гость открывает свою запись (черновик
   *                 показывается поверх строки БД) и продолжает работу.
   * Запись, помеченную на удаление, открывать некуда: в черновике её уже нет.
   * Возвращает готовую строку атрибутов или '' — вызывающий код сам решает,
   * рисовать ссылку или обычный текст.
   */
  const recordLinkAttrs = ({ op, requestId = '', field = '', edit = false, mode = 'request' } = {}) => {
    if (!op?.resource) return '';
    const record = op.type === 'create' ? op.tempId : op.id;
    if (!record) return '';
    if (mode === 'guest') {
      if (op.type === 'delete') return '';
      const guestHref = editorUrl(op.resource, { record, field });
      if (!guestHref) return '';
      return `href="${esc(guestHref)}" data-open-record data-guest-record="1"`
        + ` data-resource="${esc(op.resource)}" data-record="${esc(record)}" data-field="${esc(field)}"`;
    }
    if (!requestId) return '';
    const href = editorUrl(op.resource, { request: requestId, record, field, edit: edit ? 1 : '' });
    if (!href) return '';
    return `href="${esc(href)}" data-open-record data-request="${esc(requestId)}"`
      + ` data-resource="${esc(op.resource)}" data-record="${esc(record)}" data-field="${esc(field)}"`
      + (edit ? ' data-request-edit="1"' : '');
  };

  window.EditorCore.RequestWorkbench = {
    open, openEditor, editorUrl, recordLinkAttrs, RESOURCE_PAGES, _jsonFieldValue: jsonFieldValue,
  };
})();
