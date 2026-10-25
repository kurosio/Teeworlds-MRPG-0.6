(() => {
  const namespace = (window.EditorCore = window.EditorCore || {});
  if (namespace.RequestContext) return;

  const query = new URLSearchParams(window.location?.search || '');
  const requestId = String(query.get('request') || '').trim();
  const requestedResource = String(query.get('resource') || '').trim();
  const parsedRecord = Number(query.get('record'));
  const requestedRecord = Number.isSafeInteger(parsedRecord) && parsedRecord > 0 ? parsedRecord : 0;
  const requestedField = String(query.get('field') || '').trim();
  const active = !!(requestId && requestedResource);
  const editRequested = active && query.get('edit') === '1';
  const TEMP_ID_BASE = 2000000000;
  const TEMP_ID_MAX = 2000999999;
  const clone = (value) => namespace.utils.cloneData(value);
  const esc = (value) => namespace.utils.escapeHtml(value ?? '');

  let request = null;
  let operations = [];
  let target = null;
  let loadError = '';
  let editError = '';
  let editMode = false;
  let editBlocked = false;
  let banner = null;
  let maintenanceFrame = 0;
  let navigationFrame = 0;
  let navigationBusy = false;
  let navigationPending = false;
  let mutationQueue = Promise.resolve();
  let batchState = null;
  const expandedQuests = new WeakSet();

  const operationId = (op) => namespace.ChangeMarkers?.operationId(op)
    || Number(op?.previewId ?? (op?.type === 'create' ? op?.tempId : op?.id)) || 0;
  const operationsFor = (resource) => operations.filter((op) => op.resource === resource);
  const findOperation = (resource, id) => operationsFor(resource)
    .find((op) => operationId(op) === Number(id)) || null;

  // NULL, '' и 0 в редакторах означают одно и то же «нет значения»: сравнение
  // «было → станет» не должно считать такую пару изменением (см. utils.valuesEqual).
  const sameValue = (a, b) => namespace.utils.valuesEqual(a, b);
  const cleanData = (data) => Object.fromEntries(Object.entries(data || {})
    .filter(([column, value]) => column !== 'ID' && !column.startsWith('__') && value !== undefined)
    .map(([column, value]) => [column, clone(value)]));

  const payloadOperations = (source = operations) => source.map((op) => ({
    type: op.type,
    resource: op.resource,
    id: op.type === 'create' ? null : Number(op.id),
    tempId: op.type === 'create' ? Number(op.tempId) : null,
    data: clone(op.data || {}),
  }));
  const sameOperations = (a, b) => JSON.stringify(payloadOperations(a)) === JSON.stringify(payloadOperations(b));

  const openRequestPage = () => {
    const href = `change-requests.html?request=${encodeURIComponent(requestId)}`;
    if (window.parent && window.parent !== window) {
      namespace.Http.postMessage(window.parent, {
        type: 'editor-shell:open-editor', key: 'requests', params: { request: requestId },
      });
    } else {
      window.location.href = href;
    }
  };

  const renderBanner = () => {
    if (!active || !document.body) return;
    if (!banner) {
      banner = document.createElement('div');
      banner.className = 'request-preview-banner';
      banner.setAttribute('role', 'status');
      document.body.appendChild(banner);
      banner.addEventListener('click', (event) => {
        if (!event.target.closest('[data-request-open]')) return;
        event.preventDefault();
        openRequestPage();
      });
    }
    const typeLabel = { create: 'creation', update: 'change', delete: 'deletion' };
    const targetOp = target ? findOperation(target.resource, target.id) : null;
    const details = loadError
      ? esc(loadError)
      : request
        ? `${esc(requestId)} · v${Number(request.version) || 1}${targetOp ? ` · ${esc(typeLabel[targetOp.type] || targetOp.type)} #${operationId(targetOp)}` : ''}${target?.field ? ` · field <code>${esc(target.field)}</code>` : ''}`
        : `${esc(requestId)}  · loading…`;
    const editing = editMode && request?.status === 'pending';
    const hint = editError || (editing
      ? 'The editor\'s regular buttons save a new request version, not the game database.'
      : 'Direct database writes are disabled. Changes are made in the request itself.');
    banner.classList.toggle('request-preview-banner--editing', editing);
    banner.innerHTML = `
      <i class="fa-solid ${loadError || editError ? 'fa-triangle-exclamation' : editing ? 'fa-pen-to-square' : 'fa-code-pull-request'}"></i>
      <div class="request-preview-banner-text"><b>${loadError ? 'Could not open the request snapshot' : editing ? 'Editing request ' : 'Request view mode'}</b><span>${details}</span><small>${esc(hint)}</small></div>
      <a class="editor-btn editor-btn-secondary editor-btn--sm" href="change-requests.html?request=${encodeURIComponent(requestId)}" data-request-open>Open the request</a>`;
  };

  const syncView = ({ navigate: shouldNavigate = true, clearLive = true } = {}) => {
    namespace.ChangeMarkers?.setRequest(request ? { ...request, operations } : null, target, { clearLive });
    renderBanner();
    scheduleMaintenance();
    if (shouldNavigate) requestNavigation();
  };

  const prepareRequest = (payload, role = null, { preserveTarget = false, navigate = true } = {}) => {
    request = clone(payload);
    editMode = editRequested && role === 'admin' && request?.status === 'pending';
    editBlocked = false;
    editError = '';
    const idMap = request.review?.idMap || {};
    operations = (request.operations || []).map((source) => {
      const op = clone(source);
      if (op.type === 'create' && idMap[op.tempId]) op.previewId = Number(idMap[op.tempId]);
      return op;
    });
    if (!preserveTarget) {
      const targetOp = operations.find((op) => op.resource === requestedResource
        && (!requestedRecord || operationId(op) === requestedRecord
          || Number(op.tempId) === requestedRecord || Number(op.id) === requestedRecord))
        || (!requestedRecord ? operations.find((op) => op.resource === requestedResource) : null)
        || null;
      target = targetOp ? {
        resource: targetOp.resource,
        id: operationId(targetOp),
        field: requestedField,
      } : null;
    }
    syncView({ navigate });
    window.dispatchEvent(new CustomEvent('editor-core:request-preview-ready', {
      detail: { request, target },
    }));
  };

  // Loading the role together with the snapshot prevents a guest from
  // accidentally receiving admin edit controls by adding edit=1 to the URL.
  const rolePromise = active
    ? Promise.resolve(namespace.Guest?.ready?.()).catch(() => null)
    : Promise.resolve(null);
  const requestPromise = active
    ? Promise.resolve().then(() => namespace.Http.json(`api/change-requests.php?action=get&id=${encodeURIComponent(requestId)}&peek=1`, { timeout: 30000 }))
    : Promise.resolve(null);
  const readyPromise = active
    ? Promise.all([requestPromise, rolePromise])
      .then(([response, role]) => {
        if (!response?.ok || !response.request) throw new Error(response?.error || 'Request not found.');
        prepareRequest(response.request, role);
        return request;
      })
      .catch((error) => {
        loadError = error?.message || 'Failed to load the request.';
        editMode = false;
        renderBanner();
        return null;
      })
    : Promise.resolve(null);

  const overlayRows = (resource, rows) => {
    const relevant = operationsFor(resource);
    if (!relevant.length) return rows;
    const byId = new Map(relevant.map((op) => [operationId(op), op]));
    const found = new Set();
    let out = (rows || []).map((row) => {
      const id = Number(row?.ID);
      const op = byId.get(id);
      if (!op) return row;
      found.add(id);
      return op.type === 'update' || op.type === 'create'
        ? { ...row, ...clone(op.data || {}) }
        : row;
    });
    for (const op of relevant) {
      const id = operationId(op);
      if (!id || found.has(id)) continue;
      if (op.type === 'create') out.push({ ID: id, ...clone(op.data || {}) });
      else if (op.type === 'delete' && op.before) out.push({ ID: id, ...clone(op.before) });
    }
    if (target?.resource === resource) {
      out = out.slice().sort((a, b) => Number(Number(b?.ID) === Number(target.id)) - Number(Number(a?.ID) === Number(target.id)));
    }
    return out;
  };

  const overlayRow = (resource, id, row) => {
    const op = findOperation(resource, id);
    if (!op) return row;
    if (op.type === 'update' || op.type === 'create') {
      return { ...(row || op.before || {}), ID: operationId(op), ...clone(op.data || {}) };
    }
    if (!row && op.type === 'delete' && op.before) return { ID: operationId(op), ...clone(op.before) };
    return row;
  };

  const READ_ONLY_BUTTONS = [
    '[data-editor-role="save"]', '[data-editor-role="new"]', '[data-editor-role="delete"]',
    '[data-action="save"]', '[data-action="new"]', '[data-action="delete"]',
    '#btn-save', '#btn-new', '#btn-delete', '#btn-empty-new', '#saveBtn', '#newBtn',
    '#newQuestBtn', '#newStepBtn', '#deleteBtn', '[data-task-add]',
    '[data-step-action="delete"]', '[data-step-action="duplicate"]',
    '[data-action^="add-"]', '[data-action^="del-"]',
  ].join(',');

  const restoreControls = () => {
    document.querySelectorAll('[data-request-preview-disabled]').forEach((control) => {
      control.disabled = false;
      delete control.dataset.requestPreviewDisabled;
    });
    document.querySelectorAll('[data-request-preview-contenteditable]').forEach((control) => {
      control.setAttribute('contenteditable', control.dataset.requestPreviewContenteditable || 'true');
      delete control.dataset.requestPreviewContenteditable;
    });
  };

  const protectControls = () => {
    if (!active || !document.body) return;
    document.body.classList.add('request-preview-active');
    if (editMode && request?.status === 'pending') {
      restoreControls();
      return;
    }
    document.querySelectorAll('input, textarea, select').forEach((control) => {
      if (!control.disabled) {
        control.dataset.requestPreviewDisabled = '1';
        control.disabled = true;
      }
    });
    document.querySelectorAll('[contenteditable]:not([contenteditable="false"])').forEach((control) => {
      if (!control.hasAttribute('data-request-preview-contenteditable')) {
        control.dataset.requestPreviewContenteditable = control.getAttribute('contenteditable') || 'true';
      }
      control.setAttribute('contenteditable', 'false');
    });
    document.querySelectorAll(READ_ONLY_BUTTONS).forEach((button) => {
      if (!button.disabled) {
        button.dataset.requestPreviewDisabled = '1';
        button.disabled = true;
      }
    });
  };

  const scheduleMaintenance = () => {
    if (!active || maintenanceFrame) return;
    const raf = window.requestAnimationFrame || ((callback) => window.setTimeout(callback, 16));
    maintenanceFrame = raf(() => {
      maintenanceFrame = 0;
      protectControls();
    });
  };

  const targetRow = () => {
    if (!target) return null;
    const nodeType = target.resource === 'quests' ? 'quest' : target.resource === 'quest_bots' ? 'step' : '';
    const selector = nodeType ? `[data-node-type="${nodeType}"][data-id]` : '.editor-row[data-id]';
    return Array.from(document.querySelectorAll(selector)).find((row) => Number(row.dataset.id) === Number(target.id)) || null;
  };

  const expandTargetQuest = async () => {
    if (target?.resource !== 'quest_bots') return;
    const op = findOperation(target.resource, target.id);
    let questId = Number(op?.data?.QuestID) || 0;
    if (!questId && op?.type !== 'create') {
      try {
        const response = await namespace.DBCrud?.get('quest_bots', target.id);
        questId = Number(response?.row?.QuestID) || 0;
      } catch {
        return;
      }
    }
    if (!questId) return;
    const questOp = findOperation('quests', questId);
    if (questOp) questId = operationId(questOp);
    const quest = Array.from(document.querySelectorAll('[data-node-type="quest"][data-id]'))
      .find((row) => Number(row.dataset.id) === questId);
    if (!quest || expandedQuests.has(quest) || !quest.classList.contains('is-collapsed')) return;
    const toggle = quest.querySelector('[data-quest-toggle]');
    if (!toggle) return;
    expandedQuests.add(quest);
    toggle.click();
  };

  const navigate = async () => {
    navigationFrame = 0;
    if (!target || !navigationPending || navigationBusy) return;
    navigationBusy = true;
    try {
      await expandTargetQuest();
      const row = targetRow();
      if (!row) return;
      // Selection rebuilds the quest tree. Complete the jump before clicking,
      // otherwise the observer treats each new DOM node as an unvisited target
      // and keeps undoing the user's subsequent quest/step selection.
      navigationPending = false;
      row.scrollIntoView?.({ block: 'center', behavior: 'smooth' });
      const selected = ['editor-row-active', 'ring-2', 'tree-selected', 'active', 'is-active', 'selected']
        .some((name) => row.classList.contains(name));
      if (!selected) row.click();
      namespace.ChangeMarkers?.refresh();
    } finally {
      navigationBusy = false;
    }
  };

  const scheduleNavigation = () => {
    if (!active || !target || !navigationPending || navigationFrame) return;
    const raf = window.requestAnimationFrame || ((callback) => window.setTimeout(callback, 16));
    navigationFrame = raf(() => {
      navigate().catch((error) => console.warn('[requests] preview navigation failed', error));
    });
  };

  // Only an explicit navigation request may rearm the jump; DOM mutations
  // merely retry it while the target row is still loading.
  const requestNavigation = () => {
    navigationPending = !!target;
    scheduleNavigation();
  };

  const stateSnapshot = () => ({
    request: clone(request), operations: clone(operations), target: clone(target),
    editBlocked, editError,
  });
  const restoreSnapshot = (snapshot, { navigate = true, clearLive = false } = {}) => {
    request = snapshot.request;
    operations = snapshot.operations;
    target = snapshot.target;
    editBlocked = snapshot.editBlocked;
    editError = snapshot.editError;
    syncView({ navigate, clearLive });
  };
  const mutationError = (response) => {
    const error = new Error(response?.error || 'Failed to save the new request version.');
    error.code = response?.code || '';
    error.status = response?.status || 0;
    error.conflicts = response?.conflicts || [];
    throw error;
  };
  const conflictMessage = 'The request was changed by the other side. Reopen it and repeat the edit in the current version.';
  const closedMessage = 'The request was already processed in another window. Open its current version.';
  const isStaleError = (error) => error?.code === 'version_conflict' || error?.code === 'request_not_pending' || error?.status === 404;

  const commitSnapshot = async (snapshot, { navigate = true } = {}) => {
    if (sameOperations(snapshot.operations, operations)) {
      restoreSnapshot(snapshot, { navigate });
      return { ok: true, unchanged: true, request };
    }
    const response = await namespace.Http.json('api/change-requests.php?action=revise', {
      method: 'POST',
      body: { id: requestId, version: Number(snapshot.request?.version) || 1, operations: payloadOperations() },
      timeout: 30000,
    });
    if (!response?.ok || !response.request) mutationError(response);
    prepareRequest(response.request, 'admin', { preserveTarget: true, navigate });
    namespace.Http.postMessage(window.parent, { type: 'editor-shell:requests-changed' });
    return response;
  };

  const nextTempId = () => {
    const used = operations.map((op) => Number(op.tempId) || 0);
    const id = Math.max(TEMP_ID_BASE, ...used) + 1;
    if (id > TEMP_ID_MAX) throw new Error('Temporary IDs for new request records are exhausted.');
    return id;
  };

  const stageMutation = ({ action, resource, id = null, data = null, baseRow = null }) => {
    if (!request || request.status !== 'pending') throw new Error('Only a pending request can be edited.');
    const numericId = Number(id) || 0;
    if (action !== 'create' && (!Number.isSafeInteger(numericId) || numericId <= 0)) {
      throw new Error('Invalid record ID.');
    }
    if (action === 'create') {
      const tempId = nextTempId();
      operations.push({ type: 'create', resource, id: null, tempId, data: cleanData(data), before: null });
      target = { resource, id: tempId, field: '' };
      return { ok: true, id: tempId, draft: true, request: true };
    }

    let op = findOperation(resource, numericId);
    if (action === 'update') {
      const isNew = !op;
      if (!op) {
        op = { type: 'update', resource, id: numericId, tempId: null, data: {}, before: clone(baseRow) };
        operations.push(op);
      }
      if (op.type === 'create') {
        op.data = { ...op.data, ...cleanData(data) };
      } else {
        const base = baseRow || op.before || {};
        const merged = { ...(op.type === 'update' ? op.data : {}), ...cleanData(data) };
        for (const column of Object.keys(merged)) {
          if (column in base && sameValue(base[column], merged[column])) delete merged[column];
        }
        if (!Object.keys(merged).length) {
          // Saving a delete snapshot unchanged cancels that delete; returning
          // every changed field to its base value removes an update operation.
          operations = operations.filter((entry) => entry !== op);
          target = { resource, id: numericId, field: '' };
          return { ok: true, id: numericId, unchanged: isNew, request: true };
        }
        op.type = 'update';
        op.id = numericId;
        op.tempId = null;
        op.data = merged;
        if (!op.before && baseRow) op.before = clone(baseRow);
      }
      target = {
        resource, id: operationId(op),
        field: target?.resource === resource && Number(target.id) === numericId ? target.field : '',
      };
      return { ok: true, id: operationId(op), request: true };
    }

    if (action === 'remove') {
      if (op?.type === 'delete') return { ok: true, id: numericId, unchanged: true, request: true };
      if (op?.type === 'create') {
        operations = operations.filter((entry) => entry !== op);
      } else {
        if (!op) {
          op = { type: 'delete', resource, id: numericId, tempId: null, data: {}, before: clone(baseRow) };
          operations.push(op);
        } else {
          op.type = 'delete';
          op.id = numericId;
          op.tempId = null;
          op.data = {};
          if (baseRow) op.before = clone(baseRow);
        }
      }
      target = null;
      return { ok: true, id: numericId, request: true };
    }
    throw new Error('Unknown request operation.');
  };

  const assertEditable = () => {
    if (!editMode || request?.status !== 'pending') {
      throw new Error('A read-only snapshot of the request is open. Go back to the request and save the fixed version there.');
    }
    if (editBlocked) throw new Error(editError || conflictMessage);
  };

  const mutateRequest = (args) => {
    try { assertEditable(); } catch (error) { return Promise.reject(error); }
    if (batchState) {
      try { return Promise.resolve(stageMutation(args)); }
      catch (error) { return Promise.reject(error); }
    }
    const run = async () => {
      assertEditable();
      const snapshot = stateSnapshot();
      try {
        const result = stageMutation(args);
        await commitSnapshot(snapshot);
        return result;
      } catch (error) {
        restoreSnapshot(snapshot);
        if (isStaleError(error)) {
          editBlocked = true;
          editError = error.code === 'version_conflict' ? conflictMessage : closedMessage;
          renderBanner();
        }
        throw error;
      }
    };
    const pending = mutationQueue.then(run, run);
    mutationQueue = pending.catch(() => {});
    return pending;
  };

  /** Group several familiar editor mutations into one atomic request revision. */
  const batch = async (callback) => {
    await readyPromise;
    if (!editMode) return callback();
    await mutationQueue;
    assertEditable();
    if (batchState) return callback();
    const snapshot = stateSnapshot();
    batchState = { snapshot };
    try {
      const result = await callback();
      // Keep the user's original branch/record highlighted after a composite
      // save instead of jumping to whichever operation happened to be last.
      if (target) target = snapshot.target;
      await commitSnapshot(snapshot, { navigate: false });
      batchState = null;
      return result;
    } catch (error) {
      restoreSnapshot(snapshot, { navigate: false });
      batchState = null;
      if (isStaleError(error)) {
        editBlocked = true;
        editError = error.code === 'version_conflict' ? conflictMessage : closedMessage;
        renderBanner();
      }
      throw error;
    }
  };

  // Craft group names are a UI-level bulk operation. Keep the same semantics
  // as the guest draft helper, but stage every row in this request instead of
  // calling the direct database rename endpoint.
  const parseCraftGroup = (value) => {
    const raw = String(value || '').trim();
    let main = '';
    let nested = raw;
    if (raw.startsWith('|')) {
      const end = raw.indexOf('|', 1);
      if (end >= 0) { main = raw.slice(1, end).trim(); nested = raw.slice(end + 1); } else nested = raw.slice(1);
    }
    const sep = nested.indexOf(':');
    if (sep < 0) return { main, group: nested.trim(), sub: '' };
    return { main, group: nested.slice(0, sep).trim(), sub: nested.slice(sep + 1).trim() };
  };
  const joinCraftGroup = (main, group, sub) => {
    main = String(main || '').trim(); group = String(group || '').trim(); sub = String(sub || '').trim();
    const nested = group && sub ? `${group}:${sub}` : (group || sub);
    if (!main) return nested;
    return nested ? `|${main}|${nested}` : `|${main}|`;
  };
  const renameCraftGroup = async (rows, targetGroup, newName, mergeExisting) => {
    assertEditable();
    const level = String(targetGroup?.level || '');
    const mainGroup = String(targetGroup?.mainGroup || '').trim();
    const group = String(targetGroup?.group || '').trim();
    const subGroup = String(targetGroup?.subGroup || '').trim();
    const name = String(newName || '').trim();
    const current = level === 'main' ? mainGroup : level === 'group' ? group : subGroup;
    if (!['main', 'group', 'sub'].includes(level) || !current) throw new Error('Invalid craft group target');
    if (!name || name.includes(':') || name.includes('|') || name.length > 256) {
      throw new Error('Invalid new group name. Do not use : or |; 256 characters maximum.');
    }
    if (name === current) return { ok: true, changed: [] };
    const parentMatches = (p) => level === 'main' || (p.main === mainGroup && (level === 'group' || p.group === group));
    const matches = (p) => {
      if (level === 'main') return p.main === mainGroup;
      if (p.main !== mainGroup || p.group !== group) return false;
      return level === 'group' || p.sub === subGroup;
    };
    const levelName = (p) => level === 'main' ? p.main : level === 'group' ? p.group : p.sub;
    const updates = [];
    let merged = false;
    for (const row of rows || []) {
      const parsed = parseCraftGroup(row.GroupName);
      if (!parentMatches(parsed)) continue;
      if (levelName(parsed) === name) merged = true;
      if (!matches(parsed)) continue;
      const renamed = level === 'main' ? joinCraftGroup(name, parsed.group, parsed.sub)
        : level === 'group' ? joinCraftGroup(parsed.main, name, parsed.sub) : joinCraftGroup(parsed.main, parsed.group, name);
      updates.push({ ID: Number(row.ID), newGroupName: renamed });
    }
    if (updates.length && merged && mergeExisting !== true) {
      throw new Error('This name is already taken at this level. Check the warning and try again.');
    }
    return batch(async () => {
      for (const update of updates) {
        await mutateRequest({ action: 'update', resource: 'crafts', id: update.ID, data: { GroupName: update.newGroupName } });
      }
      return { ok: true, changed: updates };
    });
  };

  const start = () => {
    if (!active || !document.body) return;
    renderBanner();
    protectControls();
    const observer = new MutationObserver(() => {
      scheduleMaintenance();
      scheduleNavigation();
    });
    observer.observe(document.body, {
      childList: true, subtree: true, attributes: true, attributeFilter: ['disabled', 'contenteditable'],
    });
    document.addEventListener('submit', (event) => {
      event.preventDefault();
      event.stopPropagation();
    }, true);
    readyPromise.then(scheduleNavigation);
  };

  window.EditorCore.RequestContext = {
    active: () => active,
    isReadOnly: () => active && !(editMode && request?.status === 'pending'),
    isEditing: () => !!(editMode && request?.status === 'pending'),
    ready: () => readyPromise,
    hasResource: (resource) => operationsFor(resource).length > 0,
    overlayRows,
    overlayRow,
    operation: findOperation,
    target: () => target ? { ...target } : null,
    request: () => request,
    navigate: requestNavigation,
    mutate: mutateRequest,
    batch,
    renameCraftGroup,
  };

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start, { once: true });
  else start();
})();
