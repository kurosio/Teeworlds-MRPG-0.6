(() => {
  const namespace = (window.EditorCore = window.EditorCore || {});
  if (namespace.ChangeMarkers) return;

  const ROW_SELECTOR = '.editor-row[data-id], [data-node-type="quest"][data-id], [data-node-type="step"][data-id]';
  const ACTIVE_CLASSES = ['editor-row-active', 'ring-2', 'tree-selected', 'active', 'is-active', 'selected'];
  const TYPE_LABEL = { create: 'New', update: 'Changed', delete: 'Delete' };

  let guestOps = [];
  let request = null;
  let target = null;
  let frame = 0;
  let started = false;
  let scrolledTarget = false;
  let selectionKey = '';

  const pageResource = () => {
    const src = String(window.location?.pathname || '').split('/').pop();
    for (const group of window.STUDIO_MANIFEST || []) {
      const item = (group.items || []).find((entry) => entry.src === src);
      if (item && !['requests', 'guests', 'settings', 'database', 'scenario'].includes(item.key)) return item.key;
    }
    return '';
  };

  const operationId = (op) => Number(op?.previewId ?? (op?.type === 'create' ? op?.tempId : op?.id)) || 0;
  const rowResource = (row) => {
    if (row?.dataset?.nodeType === 'step') return 'quest_bots';
    if (row?.dataset?.nodeType === 'quest') return 'quests';
    return pageResource();
  };
  const findOperation = (ops, resource, id) => ops.find((op) => (
    op.resource === resource && operationId(op) === Number(id)
  )) || null;
  const independentGuestOp = (requestOp, guestOp) => (
    requestOp && guestOp?.reqId && guestOp.reqId === request?.id ? null : guestOp
  );

  const fieldMatches = (path, column) => {
    const value = String(path || '');
    const top = value.split('.')[0];
    if (top === column) return true;
    if (column === 'PosX') return value === 'Position.x';
    if (column === 'PosY') return value === 'Position.y';
    return false;
  };

  const operationFields = (op) => Object.keys(
    op?.type === 'delete' ? (op.before || {}) : (op.data || {}),
  );

  const setBadge = (row, kind, op, suppressed = false) => {
    const selector = `[data-change-row-marker="${kind}"]`;
    let badge = Array.from(row.children || []).find((child) => child.matches?.(selector));
    const key = op ? `${op.type}:${op.reqId || request?.id || ''}:${op.reqVersion || request?.version || ''}` : '';
    if (!op || suppressed) {
      badge?.remove();
      return;
    }
    if (badge?.dataset.key === key) return;
    badge?.remove();
    badge = document.createElement('span');
    badge.className = `change-row-marker change-row-marker--${kind} ${op.reqId || kind === 'request' ? 'is-submitted' : 'is-draft'}`;
    badge.dataset.changeRowMarker = kind;
    badge.dataset.key = key;
    badge.title = kind === 'request'
      ? `${TYPE_LABEL[op.type] || 'Changed'}  in request  ${request?.id || ''}`
      : op.reqId ? `Sent in request  ${op.reqId}` : 'Local request draft';
    badge.innerHTML = `<i class="fa-solid ${kind === 'request' ? 'fa-code-pull-request' : op.reqId ? 'fa-hourglass-half' : 'fa-pen'}"></i>${kind === 'request' ? 'request' : op.reqId ? 'review' : 'draft'}`;
    row.appendChild(badge);
  };

  const clearField = (node, kind) => {
    const marker = kind === 'request' ? 'changeRequestField' : 'changeGuestField';
    if (node.dataset[marker] !== '1') return;
    if (kind === 'request') {
      node.classList.remove('request-field-changed', 'request-field-target');
      if (node.dataset.changeRequestPrevTitle !== undefined) {
        const previous = node.dataset.changeRequestPrevTitle;
        if (previous) node.setAttribute('title', previous); else node.removeAttribute('title');
        delete node.dataset.changeRequestPrevTitle;
      }
    } else {
      node.classList.remove('guest-field-changed', 'is-submitted');
    }
    delete node.dataset[marker];
  };

  const markField = (node, kind, op, isTarget) => {
    const marker = kind === 'request' ? 'changeRequestField' : 'changeGuestField';
    if (kind === 'guest' && node.classList.contains('guest-field-changed')
      && node.dataset.guestPrevTitle !== undefined && node.dataset[marker] !== '1') {
      // The generic DbEditor already owns this marker and its tooltip.
      return;
    }
    node.dataset[marker] = '1';
    if (kind === 'request') {
      node.classList.add('request-field-changed');
      node.classList.toggle('request-field-target', isTarget);
      if (node.dataset.changeRequestPrevTitle === undefined) {
        node.dataset.changeRequestPrevTitle = node.getAttribute('title') || '';
      }
      node.title = `Field changed in request  ${request?.id || ''}  · version  ${request?.version || 1}`;
    } else {
      node.classList.add('guest-field-changed');
      node.classList.toggle('is-submitted', !!op.reqId);
    }
  };

  const clearLiveMarkers = () => {
    document.querySelectorAll('[data-guest-live-field]').forEach((node) => {
      node.classList.remove('guest-field-editing');
      delete node.dataset.guestLiveField;
    });
    document.querySelectorAll('.guest-shared-row-editing').forEach((row) => row.classList.remove('guest-shared-row-editing'));
  };

  const selectedRow = () => Array.from(document.querySelectorAll(ROW_SELECTOR))
    .find((row) => ACTIVE_CLASSES.some((name) => row.classList.contains(name))) || null;

  const onFieldEdit = (event) => {
    const requestEditing = !!namespace.RequestContext?.isEditing?.();
    if ((!requestEditing && request) || (!requestEditing && !namespace.Guest?.isGuest?.())) return;
    const field = event.target?.closest?.('[data-field-path]');
    if (!field) return;
    field.dataset.guestLiveField = '1';
    field.classList.add('guest-field-editing');
    selectedRow()?.classList.add('guest-shared-row-editing');
  };

  const decorateFields = (selectedRow) => {
    const nextSelectionKey = selectedRow ? `${rowResource(selectedRow)}:${selectedRow.dataset.id}` : '';
    if (nextSelectionKey !== selectionKey) {
      clearLiveMarkers();
      selectionKey = nextSelectionKey;
    }
    document.querySelectorAll('[data-field-path]').forEach((node) => {
      clearField(node, 'request');
      clearField(node, 'guest');
    });
    if (!selectedRow) return;

    const resource = rowResource(selectedRow);
    const id = Number(selectedRow.dataset.id);
    const requestOp = findOperation(request?.operations || [], resource, id);
    const guestOp = independentGuestOp(requestOp, findOperation(guestOps, resource, id));
    const fields = document.querySelectorAll('[data-field-path]');

    if (requestOp) {
      const columns = operationFields(requestOp);
      fields.forEach((node) => {
        const path = node.getAttribute('data-field-path');
        const column = columns.find((name) => fieldMatches(path, name));
        if (!column) return;
        const isTarget = target && resource === target.resource && id === Number(target.id)
          && fieldMatches(path, target.field || column);
        markField(node, 'request', requestOp, !!isTarget);
        if (isTarget && !scrolledTarget && (node.getClientRects?.().length ?? 1) > 0) {
          scrolledTarget = true;
          window.setTimeout(() => node.scrollIntoView?.({ block: 'center', behavior: 'smooth' }), 80);
        }
      });
    }
    if (guestOp) {
      const columns = operationFields(guestOp);
      fields.forEach((node) => {
        const path = node.getAttribute('data-field-path');
        if (!columns.some((name) => fieldMatches(path, name))) return;
        node.classList.remove('guest-field-editing');
        delete node.dataset.guestLiveField;
        markField(node, 'guest', guestOp, false);
      });
    }
  };

  const refreshNow = () => {
    frame = 0;
    if (!document.body) return;
    const rows = Array.from(document.querySelectorAll(ROW_SELECTOR));
    let selectedRow = null;
    for (const row of rows) {
      const resource = rowResource(row);
      const id = Number(row.dataset.id);
      const requestOp = findOperation(request?.operations || [], resource, id);
      const guestOp = independentGuestOp(requestOp, findOperation(guestOps, resource, id));
      const isTarget = !!(requestOp && target && resource === target.resource && id === Number(target.id));
      row.classList.toggle('request-row-changed', !!requestOp);
      row.classList.toggle('request-row-target', isTarget);
      row.classList.toggle('guest-shared-row-changed', !!guestOp);
      row.classList.toggle('is-submitted', !!guestOp?.reqId);
      if (requestOp) row.dataset.requestType = requestOp.type; else delete row.dataset.requestType;
      setBadge(row, 'request', requestOp);
      setBadge(row, 'guest', guestOp, !!row.querySelector('.guest-row-badge'));
      if (!selectedRow && ACTIVE_CLASSES.some((name) => row.classList.contains(name))) selectedRow = row;
    }
    decorateFields(selectedRow);
  };

  const refresh = () => {
    if (frame) return;
    const raf = window.requestAnimationFrame || ((callback) => window.setTimeout(callback, 16));
    frame = raf(refreshNow);
  };

  const setRequest = (nextRequest, nextTarget = null, { clearLive = true } = {}) => {
    request = nextRequest || null;
    target = nextTarget;
    scrolledTarget = false;
    if (request && clearLive) clearLiveMarkers();
    refresh();
  };

  const refreshGuest = async () => {
    try {
      const role = await namespace.Guest?.ready?.();
      guestOps = role === 'guest' ? (namespace.Guest.Draft.get().ops || []) : [];
    } catch {
      guestOps = [];
    }
    refresh();
  };

  const start = () => {
    if (started || !document.body) return;
    started = true;
    const observer = new MutationObserver(refresh);
    observer.observe(document.body, { childList: true, subtree: true });
    window.addEventListener('editor-core:guest-updated', () => { clearLiveMarkers(); refreshGuest(); });
    window.addEventListener('editor-core:guest-ready', refreshGuest);
    window.addEventListener('editor-core:guest-request-resolved', refreshGuest);
    document.addEventListener('input', onFieldEdit, true);
    document.addEventListener('change', onFieldEdit, true);
    document.addEventListener('click', (event) => {
      if (event.target?.closest?.('[data-editor-role="cancel"], [data-action="cancel"], #btn-cancel')) clearLiveMarkers();
      window.setTimeout(refresh, 0);
    }, true);
    refreshGuest();
    refresh();
  };

  namespace.ChangeMarkers = {
    setRequest,
    refresh,
    clearLive: clearLiveMarkers,
    operationId,
    fieldMatches,
    _rowResource: rowResource,
  };

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start, { once: true });
  else start();
})();
