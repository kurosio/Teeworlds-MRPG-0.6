(() => {
  // Cross-connection sync module.
  // Records CRUD operations and allows replaying them on a target DB.
  // Requires: EditorCore.UI, EditorCore.DBCrud, EditorCore.utils

  const API = 'api/db-sync.php';
  const MAINT_API = 'api/db-maintenance.php';
  const FETCH_TIMEOUT_MS = 30000;
  const PULL_PREVIEW_TIMEOUT_MS = 120000;
  const PULL_APPLY_TIMEOUT_MS = 900000;
  const BACKUP_TIMEOUT_MS = 300000;
  const qs = (...args) => window.EditorCore.utils.qs(...args);
  const STORAGE_KEY = 'es_sync_history';
  const MAX_HISTORY = 500;

  // ── Network layer ─────────────────────────────────────────────────────────

  /**
   * Запрос к API синхронизации. Ошибки бросаются исключением (их ловят
   * обработчики кнопок), таймаут задаётся на вызов: выгрузка базы идёт минуты.
   */
  const jsonFetch = async (url, options = {}) => {
    const { timeoutMs = FETCH_TIMEOUT_MS, ...rest } = options;
    const res = await window.EditorCore.Http.json(url, { ...rest, timeout: timeoutMs });
    if (!res.ok) throw new Error(res.error || 'The request failed');
    return res.data;
  };

  // ── Change history storage ────────────────────────────────────────────────

  const generateId = () => window.EditorCore.utils.uuid();

  /**
   * Extract a human-readable name from record data.
   * Tries common field names: Name, Code, Title, Path
   */
  const extractRecordName = (data, resource) => {
    if (!data || typeof data !== 'object') return '';
    
    // Try common name fields in order of preference
    const nameFields = ['Name', 'Code', 'Title', 'Path', 'name', 'code', 'title'];
    for (const field of nameFields) {
      if (data[field] && typeof data[field] === 'string' && data[field].trim()) {
        return data[field].trim();
      }
    }
    
    return '';
  };

  const loadHistory = () => {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (!raw) return [];
      const arr = JSON.parse(raw);
      if (!Array.isArray(arr)) return [];
      
      // Migration: add record_name field if missing
      let needsSave = false;
      for (const entry of arr) {
        if (!entry.record_name && entry.data) {
          entry.record_name = extractRecordName(entry.data, entry.resource);
          if (entry.record_name) needsSave = true;
        }
      }
      
      // Save migrated data if needed
      if (needsSave) {
        try {
          localStorage.setItem(STORAGE_KEY, JSON.stringify(arr));
        } catch {}
      }
      
      return arr;
    } catch {
      return [];
    }
  };

  const saveHistory = (history) => {
    try {
      // Keep only last MAX_HISTORY entries
      const trimmed = history.slice(-MAX_HISTORY);
      
      // Validate entries before saving
      const validated = trimmed.filter(entry => {
        return entry && typeof entry === 'object' && entry.id && entry.resource && entry.action;
      });
      
      localStorage.setItem(STORAGE_KEY, JSON.stringify(validated));
      return true;
    } catch (err) {
      // Storage full or unavailable — trim more aggressively
      console.warn('[Sync] Failed to save history, trimming aggressively:', err.message);
      try {
        const smaller = history.slice(-50);
        localStorage.setItem(STORAGE_KEY, JSON.stringify(smaller));
        return true;
      } catch (err2) {
        console.error('[Sync] Failed to save history even after aggressive trim:', err2.message);
        return false;
      }
    }
  };

  let _history = null;

  const getHistory = () => {
    if (_history === null) _history = loadHistory();
    return _history;
  };

  // ── Recording operations ──────────────────────────────────────────────────

  const createOperationEntry = (resource, action, { id = null, data = null, label = '', recordName = '' } = {}) => {
    if (!resource || typeof resource !== 'string' || !['create', 'update', 'delete'].includes(action)) {
      console.warn('[Sync] Invalid operation:', resource, action);
      return null;
    }
    return {
      id: generateId(), resource, action,
      data: data ? window.EditorCore.utils.cloneData(data) : null,
      record_id: id !== null ? Number(id) : null,
      record_name: recordName || extractRecordName(data, resource),
      label: label || `${action} ${resource}${id ? ' #' + id : ''}`,
      timestamp: Date.now(), applied: false, applied_at: null,
    };
  };

  const recordOperation = (resource, action, details = {}) => {
    const entry = createOperationEntry(resource, action, details);
    if (!entry) return null;
    const history = getHistory();
    history.push(entry);
    if (history.length > MAX_HISTORY) history.splice(0, history.length - MAX_HISTORY);
    saveHistory(history);
    return entry;
  };

  const recordOperations = (resource, action, operations = []) => {
    const entries = (Array.isArray(operations) ? operations : [])
      .map(details => createOperationEntry(resource, action, details || {}))
      .filter(Boolean);
    if (entries.length) {
      const history = getHistory();
      history.push(...entries);
      if (history.length > MAX_HISTORY) history.splice(0, history.length - MAX_HISTORY);
      saveHistory(history);
    }
    return entries.length;
  };

  const getUnappliedOperations = () => {
    return getHistory().filter(e => !e.applied);
  };

  const getUnappliedCount = () => {
    return getUnappliedOperations().length;
  };

  const markAsApplied = (entryIds) => {
    const history = getHistory();
    const idSet = new Set(Array.isArray(entryIds) ? entryIds : [entryIds]);
    const now = Date.now();
    for (const entry of history) {
      if (idSet.has(entry.id)) {
        entry.applied = true;
        entry.applied_at = now;
      }
    }
    saveHistory(history);
  };

  const clearHistory = ({ onlyApplied = false } = {}) => {
    const history = getHistory();
    if (onlyApplied) {
      _history = history.filter(e => !e.applied);
    } else {
      _history = [];
    }
    saveHistory(_history);
  };

  const removeEntries = (entryIds) => {
    const idSet = new Set(Array.isArray(entryIds) ? entryIds : [entryIds]);
    _history = getHistory().filter(e => !idSet.has(e.id));
    saveHistory(_history);
  };

  // ── API calls ─────────────────────────────────────────────────────────────

  const listProfiles = async () => {
    try {
      return await jsonFetch(`${API}?${qs({ action: 'list_profiles' })}`);
    } catch (err) {
      console.warn('[Sync] Failed to list profiles:', err.message);
      return { ok: false, error: err.message, profiles: [] };
    }
  };

  const getTarget = async () => {
    try {
      return await jsonFetch(`${API}?${qs({ action: 'get_target' })}`);
    } catch (err) {
      console.warn('[Sync] Failed to get target:', err.message);
      return { ok: false, error: err.message, target: null };
    }
  };

  const saveTarget = async (payload) => {
    return jsonFetch(`${API}?${qs({ action: 'save_target' })}`, {
      method: 'POST',
      body: JSON.stringify(payload),
    });
  };

  const setAsTarget = async (profileId) => {
    return jsonFetch(`${API}?${qs({ action: 'set_as_target' })}`, {
      method: 'POST',
      body: JSON.stringify({ profile_id: profileId }),
    });
  };

  const compareSchema = async ({ targetProfileId = '' } = {}) => {
    return jsonFetch(`${API}?${qs({ action: 'compare_schema' })}`, {
      method: 'POST',
      body: JSON.stringify({ target_profile_id: targetProfileId }),
    });
  };

  const applyOperations = async ({ operations, targetProfileId = '', confirmPhrase = '' }) => {
    return jsonFetch(`${API}?${qs({ action: 'apply_operations' })}`, {
      method: 'POST',
      body: JSON.stringify({
        operations,
        target_profile_id: targetProfileId,
        confirm_phrase: confirmPhrase,
      }),
    });
  };

  // Table map: which tables are synced in which direction (for the settings table).
  const syncMap = async () => {
    return jsonFetch(`${API}?${qs({ action: 'sync_map' })}`);
  };

  // Reverse sync (target → current DB): dry-run and apply.
  const pullPreview = async ({ targetProfileId = '' } = {}) => {
    return jsonFetch(`${API}?${qs({ action: 'pull_preview' })}`, {
      method: 'POST',
      timeoutMs: PULL_PREVIEW_TIMEOUT_MS,
      body: JSON.stringify({ target_profile_id: targetProfileId }),
    });
  };

  // Full dump of the current DB (db-maintenance.php), with a longer timeout than db.js uses.
  const createBackup = async ({ confirmDb, label = '' }) => {
    return jsonFetch(`${MAINT_API}?${qs({ action: 'create_dump' })}`, {
      method: 'POST',
      timeoutMs: BACKUP_TIMEOUT_MS,
      body: JSON.stringify({ confirm_db: confirmDb, label }),
    });
  };

  const pullFromTarget = async ({ targetProfileId = '', confirmPhrase = '' } = {}) => {
    return jsonFetch(`${API}?${qs({ action: 'pull_from_target' })}`, {
      method: 'POST',
      timeoutMs: PULL_APPLY_TIMEOUT_MS,
      body: JSON.stringify({ target_profile_id: targetProfileId, confirm_phrase: confirmPhrase }),
    });
  };

  // ── Build operations from history ─────────────────────────────────────────

  const buildOperationsFromHistory = (entries) => {
    return entries.map(e => {
      const op = {
        action: e.action,
        resource: e.resource,
      };
      if (e.record_id !== null && (e.action === 'update' || e.action === 'delete')) {
        op.id = e.record_id;
      }
      if (e.data && (e.action === 'create' || e.action === 'update')) {
        op.data = e.data;
      }
      return op;
    });
  };

  // ── UI: Sync button & modal ───────────────────────────────────────────────

  const toast = (msg, type) => {
    if (window.EditorCore?.UI?.toast) window.EditorCore.UI.toast(msg, type);
  };

  const escapeHtml = (value) => window.EditorCore.utils.escapeHtml(value);

  const RESOURCE_LABELS = {
    vouchers: 'Vouchers',
    bots_mobs: 'Mobs',
    bots_info: 'Bots',
    bots_npc: 'NPC',
    crafts: 'Crafts',
    warehouses: 'Warehouses',
    worlds: 'Worlds',
    nodes_collecting: 'Collecting nodes',
    aethers: 'Teleports',
    items: 'Items',
    dungeons: 'Dungeons',
    quests: 'Quests',
    quest_bots: 'Quest steps',
  };

  const ACTION_LABELS = {
    create: { text: 'Create', icon: 'fa-plus', color: '#34d399' },
    update: { text: 'Refresh', icon: 'fa-pen', color: '#60a5fa' },
    delete: { text: 'Delete', icon: 'fa-trash', color: '#f87171' },
  };

  /**
   * Generate a brief description of what was changed.
   * For create/delete: just the action
   * For update: list of changed fields
   */
  const getChangeDescription = (entry) => {
    if (entry.action === 'update' && entry.data && typeof entry.data === 'object') {
      const fields = Object.keys(entry.data);
      if (fields.length === 0) return '';
      if (fields.length <= 3) {
        return fields.join(', ');
      }
      return `${fields.slice(0, 3).join(', ')}  and  ${fields.length - 3}`;
    }
    return '';
  };

  const formatTimestamp = (ts) => {
    if (!ts) return '—';
    const d = new Date(ts);
    const pad = (n) => String(n).padStart(2, '0');
    return `${pad(d.getDate())}.${pad(d.getMonth() + 1)}.${d.getFullYear()} ${pad(d.getHours())}:${pad(d.getMinutes())}`;
  };

  /**
   * Create and mount the sync button in the editor toolbar.
   * @param {HTMLElement} container - the toolbar actions container
   * @param {object} cfg - { editorName: string }
   */
  const mountSyncButton = (container, cfg = {}) => {
    if (!container) return null;
    // Guests never sync databases (server side is admin-only as well).
    const guestApi = window.EditorCore?.Guest;
    if (guestApi?.cachedRole?.() === 'guest') return null;

    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'editor-btn editor-btn-sync';
    btn.title = 'Sync with another database';
    btn.innerHTML = `<i class="fa-solid fa-arrows-rotate"></i><span>Sync</span>`;
    btn.dataset.syncBtn = '1';

    // Badge for unapplied count
    const badge = document.createElement('span');
    badge.className = 'sync-badge hidden';
    badge.textContent = '0';
    btn.appendChild(badge);

    const updateBadge = () => {
      const count = getUnappliedCount();
      badge.textContent = String(count);
      badge.classList.toggle('hidden', count === 0);
    };

    btn.addEventListener('click', () => openSyncModal(cfg));
    container.appendChild(btn);
    guestApi?.ready?.().then((role) => { if (role === 'guest') btn.remove(); });

    updateBadge();

    // Expose updater so editors can call it after operations
    btn._updateBadge = updateBadge;

    return btn;
  };

  const openSyncModal = async (cfg = {}) => {
    const editorName = cfg.editorName || 'Editor';

    // Create modal
    const id = 'editor-sync-modal';
    let modal = document.getElementById(id);
    if (modal) modal.remove();

    modal = document.createElement('div');
    modal.id = id;
    modal.className = 'fixed inset-0 z-[600] items-center justify-center editor-modal-backdrop';
    modal.innerHTML = `
      <div class="editor-modal-content editor-sync-panel">
        <div class="editor-sync-header">
          <div>
            <h3 class="text-xl font-bold flex items-center gap-2">
              <i class="fa-solid fa-arrows-rotate text-accent"></i>
              Change sync
            </h3>
            <p class="editor-muted-text text-sm mt-1">Apply changes from “${escapeHtml(editorName)}” to another database.</p>
          </div>
          <button type="button" class="editor-icon-btn" data-sync-action="close" title="Close">
            <i class="fa-solid fa-xmark"></i>
          </button>
        </div>

        <div class="editor-sync-body">
          <!-- Target info -->
          <div class="editor-sync-target" data-sync-role="target-info">
            <div class="editor-muted-text">Loading…</div>
          </div>

          <!-- Schema comparison result -->
          <div class="editor-sync-schema" data-sync-role="schema-result"></div>

          <!-- Operations list -->
          <div class="editor-sync-ops" data-sync-role="operations">
            <div class="editor-muted-text">Loading…</div>
          </div>

          <!-- Apply form -->
          <div class="editor-sync-apply" data-sync-role="apply-form" style="display:none">
            <div class="editor-sync-confirm">
              <label class="editor-label">To confirm, type <strong>Apply</strong></label>
              <input type="text" class="w-full editor-input form-input" data-sync-role="confirm-input" placeholder="Apply" />
            </div>
          </div>

          <!-- Reverse sync: target → current DB -->
          <div class="editor-sync-reverse" data-sync-role="reverse">
            <div class="editor-sync-reverse-head">
              <i class="fa-solid fa-cloud-arrow-down"></i>
              <div>
                <div class="font-semibold">Reverse sync: target → current database</div>
                <div class="text-xs editor-muted-text">
                  Replaces the current database data with the target one. Accounts and related tables are untouched. A dump of the current database is created automatically before loading.
                </div>
              </div>
            </div>
            <div data-sync-role="pull-pending"></div>
            <div class="editor-sync-reverse-actions">
              <button type="button" class="editor-btn editor-btn-secondary" data-sync-action="pull-check" disabled>
                <i class="fa-solid fa-shield-halved"></i><span>Check the load</span>
              </button>
            </div>
            <div data-sync-role="pull-result"></div>
            <div class="editor-sync-pull-apply" data-sync-role="pull-apply-form" style="display:none">
              <div class="editor-sync-confirm">
                <label class="editor-label">To confirm, type <strong>Overwrite</strong></label>
                <input type="text" class="w-full editor-input form-input" data-sync-role="pull-confirm" placeholder="Overwrite" />
              </div>
              <button type="button" class="editor-btn editor-btn-danger" data-sync-action="pull-apply" disabled>
                <i class="fa-solid fa-cloud-arrow-down"></i><span>Load from the target database</span>
              </button>
            </div>
          </div>
        </div>

        <div class="editor-sync-footer">
          <button type="button" class="editor-btn editor-btn-secondary" data-sync-action="close">Close</button>
          <button type="button" class="editor-btn editor-btn-secondary" data-sync-action="check" disabled>
            <i class="fa-solid fa-shield-halved"></i><span>Check the schema</span>
          </button>
          <button type="button" class="editor-btn editor-btn-primary" data-sync-action="apply" disabled>
            <i class="fa-solid fa-play"></i><span>Apply</span>
          </button>
        </div>
      </div>
    `;
    document.body.appendChild(modal);

    // Refs
    const $ = (role) => modal.querySelector(`[data-sync-role="${role}"]`);
    const btnCheck = modal.querySelector('[data-sync-action="check"]');
    const btnApply = modal.querySelector('[data-sync-action="apply"]');
    const confirmInput = $('confirm-input');

    let schemaOk = false;
    let targetInfo = null;

    const close = () => {
      modal.remove();
    };

    // Close handlers
    modal.querySelectorAll('[data-sync-action="close"]').forEach(el => el.addEventListener('click', close));
    modal.addEventListener('click', (e) => { if (e.target === modal) close(); });

    // Load target
    const loadTarget = async () => {
      try {
        const res = await getTarget();
        if (res.ok && res.target) {
          targetInfo = res.target;
          $('target-info').innerHTML = `
            <div class="editor-sync-target-card">
              <div class="flex items-center gap-2">
                <i class="fa-solid fa-database text-emerald-400"></i>
                <div>
                  <div class="font-semibold">${escapeHtml(res.target.name || 'Target')}</div>
                  <div class="text-xs editor-muted-text">${escapeHtml(res.target.database || '')} @ ${escapeHtml(res.target.host || '')}:${res.target.port || 3306}</div>
                </div>
              </div>
              <span class="editor-sync-status-ok"><i class="fa-solid fa-circle-check"></i> Configured</span>
            </div>
          `;
          btnCheck.disabled = false;
        } else {
          targetInfo = null;
          $('target-info').innerHTML = `
            <div class="editor-sync-target-card editor-sync-target-empty">
              <div class="flex items-center gap-2">
                <i class="fa-solid fa-database editor-muted-text"></i>
                <div>
                  <div class="font-semibold">Target database is not configured</div>
                  <div class="text-xs editor-muted-text">Go to Settings → “Synchronization” to configure it.</div>
                </div>
              </div>
              <span class="editor-sync-status-err"><i class="fa-solid fa-circle-xmark"></i> Not configured</span>
            </div>
          `;
          btnCheck.disabled = true;
          btnApply.disabled = true;
        }
      } catch (err) {
        $('target-info').innerHTML = `<div class="text-red-400 text-sm">Load failed:  ${escapeHtml(err.message)}</div>`;
      }
    };

    // Render operations
    const renderOperations = () => {
      const ops = getUnappliedOperations();
      const opsContainer = $('operations');
      refreshReverseGate();

      if (!ops.length) {
        opsContainer.innerHTML = `
          <div class="editor-sync-ops-empty">
            <i class="fa-solid fa-circle-check text-emerald-400 text-2xl"></i>
            <div class="font-semibold">No unapplied changes</div>
            <div class="editor-muted-text text-sm">All changes are already synced or the history is empty.</div>
          </div>
        `;
        btnCheck.disabled = true;
        btnApply.disabled = true;
        return;
      }

      // Group by resource
      const grouped = {};
      for (const op of ops) {
        const key = op.resource;
        if (!grouped[key]) grouped[key] = [];
        grouped[key].push(op);
      }

      let html = `
        <div class="editor-sync-ops-header">
          <div class="font-semibold flex items-center gap-2">
            <i class="fa-solid fa-list-check text-accent"></i>
            Unapplied changes: <span class="text-accent">${ops.length}</span>
          </div>
          <button type="button" class="editor-btn editor-btn-secondary text-xs" data-sync-action="clear-history" style="padding:4px 10px">
            <i class="fa-solid fa-broom"></i><span>Clear history</span>
          </button>
        </div>
        <div class="editor-sync-ops-list">
      `;

      for (const [resource, entries] of Object.entries(grouped)) {
        const label = RESOURCE_LABELS[resource] || resource;
        html += `
          <div class="editor-sync-ops-group">
            <div class="editor-sync-ops-group-title">
              <span>${escapeHtml(label)}</span>
              <span class="editor-muted-text">${entries.length}  ops.</span>
            </div>
        `;
        for (const entry of entries) {
          const al = ACTION_LABELS[entry.action] || { text: entry.action, icon: 'fa-circle', color: '#94a3b8' };
          const description = getChangeDescription(entry);
          const recordName = entry.record_name || '';
          html += `
            <div class="editor-sync-op-row">
              <span class="editor-sync-op-icon" style="color:${al.color}"><i class="fa-solid ${al.icon}"></i></span>
              <span class="editor-sync-op-label">${escapeHtml(al.text)}</span>
              ${entry.record_id ? `<span class="editor-sync-op-id">#${entry.record_id}</span>` : ''}
              ${recordName ? `<span class="editor-sync-op-name">${escapeHtml(recordName)}</span>` : ''}
              ${description ? `<span class="editor-sync-op-desc">${escapeHtml(description)}</span>` : ''}
              <span class="editor-sync-op-time">${formatTimestamp(entry.timestamp)}</span>
              <button type="button" class="editor-icon-btn editor-icon-danger editor-sync-op-remove" data-sync-remove="${entry.id}" title="Remove from the queue">
                <i class="fa-solid fa-xmark"></i>
              </button>
            </div>
          `;
        }
        html += `</div>`;
      }

      html += `</div>`;
      opsContainer.innerHTML = html;

      // Bind remove buttons
      opsContainer.querySelectorAll('[data-sync-remove]').forEach(btn => {
        btn.addEventListener('click', () => {
          const entryId = btn.getAttribute('data-sync-remove');
          removeEntries([entryId]);
          renderOperations();
          updateAllBadges();
          if (getUnappliedCount() === 0) {
            $('schema-result').innerHTML = '';
            btnApply.disabled = true;
            btnCheck.disabled = true;
            schemaOk = false;
          }
        });
      });

      // Clear history button
      opsContainer.querySelector('[data-sync-action="clear-history"]')?.addEventListener('click', async () => {
        if (!await EditorCore.Page.confirm('Clear the entire change history? This cannot be undone.')) return;
        clearHistory();
        renderOperations();
        updateAllBadges();
        $('schema-result').innerHTML = '';
        btnApply.disabled = true;
        btnCheck.disabled = true;
        schemaOk = false;
      });

      // Enable check button
      btnCheck.disabled = !targetInfo;
    };

    // Check schema
    btnCheck.addEventListener('click', async () => {
      btnCheck.disabled = true;
      $('schema-result').innerHTML = `
        <div class="editor-sync-schema-loading">
          <i class="fa-solid fa-spinner fa-spin"></i>
          <span>Comparing database schemas…</span>
        </div>
      `;

      try {
        const res = await compareSchema({});
        if (!res.ok) throw new Error(res.error || 'Comparison failed');

        const c = res.comparison;
        let html = '';

        if (c.compatible) {
          schemaOk = true;
          html = `
            <div class="editor-sync-schema-ok">
              <i class="fa-solid fa-circle-check text-emerald-400"></i>
              <div>
                <div class="font-semibold text-emerald-300">Schemas are compatible</div>
                <div class="text-xs editor-muted-text">
                  Tables OK:  ${c.tables_ok.length}
                  ${c.tables_missing.length ? `, missing: ${c.tables_missing.length}` : ''}
                  ${c.issues.filter(i => i.level === 'warning').length ? `, warnings: ${c.issues.filter(i => i.level === 'warning').length}` : ''}
                </div>
              </div>
            </div>
          `;

          // Show warnings if any
          const warnings = c.issues.filter(i => i.level === 'warning');
          if (warnings.length) {
            html += `<div class="editor-sync-schema-warnings">`;
            for (const w of warnings) {
              html += `<div class="editor-sync-schema-warning"><i class="fa-solid fa-triangle-exclamation text-amber-400"></i> ${escapeHtml(w.message)}</div>`;
            }
            html += `</div>`;
          }

          btnApply.disabled = false;
          $('apply-form').style.display = '';

        } else {
          schemaOk = false;
          html = `
            <div class="editor-sync-schema-err">
              <i class="fa-solid fa-circle-xmark text-red-400"></i>
              <div>
                <div class="font-semibold text-red-300">Schemas are incompatible</div>
                <div class="text-xs editor-muted-text">Changes cannot be applied until the target database schema is updated.</div>
              </div>
            </div>
          `;

          if (c.issues.length) {
            html += `<div class="editor-sync-schema-issues">`;
            for (const issue of c.issues) {
              const icon = issue.level === 'critical'
                ? '<i class="fa-solid fa-circle-xmark text-red-400"></i>'
                : '<i class="fa-solid fa-triangle-exclamation text-amber-400"></i>';
              html += `<div class="editor-sync-schema-issue">${icon} ${escapeHtml(issue.message)}</div>`;
            }
            html += `</div>`;
          }

          btnApply.disabled = true;
        }

        $('schema-result').innerHTML = html;

      } catch (err) {
        schemaOk = false;
        $('schema-result').innerHTML = `
          <div class="editor-sync-schema-err">
            <i class="fa-solid fa-circle-xmark text-red-400"></i>
            <div>
              <div class="font-semibold text-red-300">Check failed</div>
              <div class="text-xs">${escapeHtml(err.message)}</div>
            </div>
          </div>
        `;
        btnApply.disabled = true;
      } finally {
        btnCheck.disabled = false;
      }
    });

    // Apply
    btnApply.addEventListener('click', async () => {
      if (!schemaOk) {
        toast('Check schema compatibility first.', 'error');
        return;
      }

      const confirm = confirmInput?.value?.trim() || '';
      if (confirm.toUpperCase() !== 'APPLY' && confirm.toUpperCase() !== 'APPLY') {
        toast('Type "Apply" to confirm.', 'error');
        confirmInput?.focus();
        return;
      }

      const entries = getUnappliedOperations();
      if (!entries.length) {
        toast('No unapplied operations.', 'warning');
        return;
      }

      const operations = buildOperationsFromHistory(entries);

      btnApply.disabled = true;
      btnApply.innerHTML = `<i class="fa-solid fa-spinner fa-spin"></i><span>Applying…</span>`;

      try {
        const res = await applyOperations({
          operations,
          confirmPhrase: confirm,
        });

        if (!res.ok) {
          // Show validation errors if any
          if (res.validation_errors) {
            let msg = 'Validation errors:\n';
            for (const ve of (res.validation_errors || [])) {
              msg += `  #${ve.index}: ${ve.error}\n`;
            }
            toast(msg, 'error');
          } else {
            toast(res.error || 'Apply failed', 'error');
          }

          // Show comparison if available
          if (res.comparison && !res.comparison.compatible) {
            schemaOk = false;
            btnCheck.click();
          }
          return;
        }

        // Success!
        const entryIds = entries.map(e => e.id);
        markAsApplied(entryIds);

        toast(`Applied successfully:  ${res.success}  operations in “${res.target?.name || 'Target'}»`, 'success');

        // Update UI
        renderOperations();
        updateAllBadges();
        $('apply-form').style.display = 'none';
        $('schema-result').innerHTML = `
          <div class="editor-sync-schema-ok">
            <i class="fa-solid fa-circle-check text-emerald-400"></i>
            <div>
              <div class="font-semibold text-emerald-300">Changes applied</div>
              <div class="text-xs editor-muted-text">
                Success:  ${res.success}  | Target:  ${escapeHtml(res.target?.database || '')}
              </div>
            </div>
          </div>
        `;

      } catch (err) {
        toast(err.message || 'Apply failed', 'error');
      } finally {
        btnApply.disabled = false;
        btnApply.innerHTML = `<i class="fa-solid fa-play"></i><span>Apply to  ${escapeHtml(targetInfo?.name || 'Target')}</span>`;
      }
    });

    // ── Reverse sync: target → current DB ──────────────────────────────────
    const btnPullCheck = modal.querySelector('[data-sync-action="pull-check"]');
    const btnPullApply = modal.querySelector('[data-sync-action="pull-apply"]');
    const pullConfirmInput = $('pull-confirm');
    const PULL_CONFIRM_WORDS = ['OVERWRITE', 'OVERWRITE'];
    let pullData = null;   // last successful preview
    let pullBusy = false;

    const setPullResult = (html) => { $('pull-result').innerHTML = html; };

    const setPullLoading = (text) => {
      setPullResult(`
        <div class="editor-sync-schema-loading">
          <i class="fa-solid fa-spinner fa-spin"></i>
          <span>${escapeHtml(text)}</span>
        </div>
      `);
    };

    // Enables/disables the reverse-sync controls. Called on every state change.
    const refreshReverseGate = () => {
      const pendingEl = $('pull-pending');
      if (!pendingEl) return;

      const pending = getUnappliedCount();
      const hasTarget = !!targetInfo;

      pendingEl.innerHTML = pending > 0 ? `
        <div class="editor-sync-warn">
          <i class="fa-solid fa-triangle-exclamation"></i>
          <div>
            There are ${pending} unapplied changes. Loading overwrites the current database, so apply them to the target database first or clear the history.
          </div>
        </div>
      `: '';

      if (pending > 0 || !hasTarget) pullData = null;

      const canApply = !!pullData?.comparison?.compatible && pending === 0 && hasTarget;
      $('pull-apply-form').style.display = canApply ? '' : 'none';

      const confirmOk = PULL_CONFIRM_WORDS.includes((pullConfirmInput?.value || '').trim().toUpperCase());

      btnPullCheck.disabled = pullBusy || pending > 0 || !hasTarget;
      btnPullApply.disabled = pullBusy || !canApply || !confirmOk;

      const applyLabel = btnPullApply.querySelector('span');
      if (applyLabel) applyLabel.textContent = `Load from  ${targetInfo?.name || 'target database'}`;
    };

    const renderPullPreview = (res) => {
      const c = res.comparison || { issues: [], compatible: false };
      const tables = res.tables || [];
      const skipped = res.skipped || [];
      const localOnly = res.local_only || [];
      const warnings = (c.issues || []).filter(i => i.level === 'warning');

      let html = `
        <div class="editor-sync-pull-summary">
          <div class="text-sm">
            Current database <b>${escapeHtml(res.local_database || '')}</b> will be replaced with data from
            <b>${escapeHtml(res.target?.name || 'Target')}</b> (${escapeHtml(res.target?.database || '')}).
          </div>
        </div>
      `;

      if (!c.compatible) {
        html += `
          <div class="editor-sync-schema-err">
            <i class="fa-solid fa-circle-xmark text-red-400"></i>
            <div>
              <div class="font-semibold text-red-300">Schemas are incompatible</div>
              <div class="text-xs editor-muted-text">Loading is impossible until the schemas match.</div>
            </div>
          </div>
          <div class="editor-sync-schema-issues">
            ${(c.issues || []).filter(i => i.level === 'critical').map(i =>
              `<div class="editor-sync-schema-issue"><i class="fa-solid fa-circle-xmark text-red-400"></i> ${escapeHtml(i.message)}</div>`
            ).join('')}
          </div>
        `;
      } else {
        const totalTarget = tables.reduce((sum, t) => sum + (t.rows_target || 0), 0);
        html += `
          <div class="editor-sync-schema-ok">
            <i class="fa-solid fa-circle-check text-emerald-400"></i>
            <div>
              <div class="font-semibold text-emerald-300">Loading is possible</div>
              <div class="text-xs editor-muted-text">Tables to overwrite:  ${tables.length}, rows in the target database:  ${totalTarget}</div>
            </div>
          </div>
        `;
      }

      if (warnings.length) {
        html += `<div class="editor-sync-schema-warnings">${warnings.map(w =>
          `<div class="editor-sync-schema-warning"><i class="fa-solid fa-triangle-exclamation text-amber-400"></i> ${escapeHtml(w.message)}</div>`
        ).join('')}</div>`;
      }

      if (tables.length) {
        html += `
          <details class="editor-sync-pull-details">
            <summary>Tables to overwrite (${tables.length})</summary>
            <div class="editor-sync-pull-table-wrap">
              <table class="editor-sync-pull-table">
                <thead><tr><th>Table</th><th>Now</th><th>After</th></tr></thead>
                <tbody>
                  ${tables.map(t => `
                    <tr>
                      <td class="font-mono">${escapeHtml(t.table)}</td>
                      <td>${t.rows_local}</td>
                      <td>${t.rows_target}</td>
                    </tr>`).join('')}
                </tbody>
              </table>
            </div>
          </details>
        `;
      }

      if (skipped.length) {
        html += `
          <details class="editor-sync-pull-details">
            <summary>Not affected (${skipped.length})</summary>
            <div class="editor-sync-pull-table-wrap">
              <table class="editor-sync-pull-table">
                <thead><tr><th>Table</th><th>Reason</th></tr></thead>
                <tbody>
                  ${skipped.map(s => `
                    <tr>
                      <td class="font-mono">${escapeHtml(s.table)}</td>
                      <td>${escapeHtml(s.reason || '')}</td>
                    </tr>`).join('')}
                </tbody>
              </table>
            </div>
          </details>
        `;
      }

      if (localOnly.length) {
        html += `<div class="text-xs editor-muted-text">Only in the current database (unchanged):  ${localOnly.map(escapeHtml).join(', ')}</div>`;
      }

      setPullResult(html);
    };

    btnPullCheck.addEventListener('click', async () => {
      pullBusy = true;
      refreshReverseGate();
      setPullLoading('Comparing the current and target databases…');
      try {
        const res = await pullPreview({ targetProfileId: '' });
        pullData = res;
        renderPullPreview(res);
      } catch (err) {
        pullData = null;
        setPullResult(`
          <div class="editor-sync-schema-err">
            <i class="fa-solid fa-circle-xmark text-red-400"></i>
            <div>
              <div class="font-semibold text-red-300">Check failed</div>
              <div class="text-xs">${escapeHtml(err.message)}</div>
            </div>
          </div>
        `);
      } finally {
        pullBusy = false;
        refreshReverseGate();
      }
    });

    btnPullApply.addEventListener('click', async () => {
      if (!pullData?.comparison?.compatible) {
        toast('Run the load check first.', 'error');
        return;
      }
      const phrase = (pullConfirmInput?.value || '').trim().toUpperCase();
      if (!PULL_CONFIRM_WORDS.includes(phrase)) {
        toast('Type “Overwrite” to confirm.', 'error');
        pullConfirmInput?.focus();
        return;
      }
      if (getUnappliedCount() > 0) {
        toast('There are unapplied changes. Apply or clear them first.', 'error');
        return;
      }

      pullBusy = true;
      refreshReverseGate();
      let backupFile = '';
      try {
        // 1) Full dump of the current DB — the only way back if the copy goes wrong.
        setPullLoading('Backing up the current database…');
        const dump = await createBackup({
          confirmDb: pullData.local_database,
          label: 'before_reverse_sync',
        });
        backupFile = dump?.file || '';

        // 2) Overwrite the current DB from the target.
        setPullLoading('Loading data from the target database…');
        const res = await pullFromTarget({ targetProfileId: '', confirmPhrase: pullConfirmInput.value.trim() });

        const warnings = (res.warnings || []).map(w => `<div class="editor-sync-schema-warning"><i class="fa-solid fa-triangle-exclamation text-amber-400"></i> ${escapeHtml(w)}</div>`).join('');
        setPullResult(`
          <div class="editor-sync-schema-ok">
            <i class="fa-solid fa-circle-check text-emerald-400"></i>
            <div>
              <div class="font-semibold text-emerald-300">Current database updated from the target</div>
              <div class="text-xs editor-muted-text">
                Tables:  ${(res.tables || []).length}, rows:  ${res.total_rows ?? 0}. Backup:  ${escapeHtml(backupFile || '—')}. The page will be reloaded.
              </div>
            </div>
          </div>
          ${warnings}
        `);
        toast(`Load complete:  ${res.total_rows ?? 0}  rows`, 'success');
        pullData = null;
        setTimeout(() => window.location.reload(), 1500);
      } catch (err) {
        toast(err.message || 'Loading failed', 'error');
        setPullResult(`
          <div class="editor-sync-schema-err">
            <i class="fa-solid fa-circle-xmark text-red-400"></i>
            <div>
              <div class="font-semibold text-red-300">Load failed</div>
              <div class="text-xs">${escapeHtml(err.message)}</div>
              ${backupFile ? `<div class="text-xs editor-muted-text mt-1">Backup saved: ${escapeHtml(backupFile)}</div>` : ''}
            </div>
          </div>
        `);
      } finally {
        pullBusy = false;
        refreshReverseGate();
      }
    });

    pullConfirmInput?.addEventListener('input', () => refreshReverseGate());

    // Init
    await loadTarget();
    renderOperations();
  };

  // ── Badge updater for all mounted sync buttons ───────────────────────────

  const updateAllBadges = () => {
    try {
      const count = getUnappliedCount();
      document.querySelectorAll('[data-sync-btn]').forEach(btn => {
        const badge = btn.querySelector('.sync-badge');
        if (badge) {
          badge.textContent = String(count);
          badge.classList.toggle('hidden', count === 0);
        }
      });
    } catch (err) {
      console.warn('[Sync] Failed to update badges:', err.message);
    }
  };

  // ── Public API ────────────────────────────────────────────────────────────

  const Sync = {
    record: recordOperation,
    recordMany: recordOperations,
    getHistory,
    getUnappliedOperations,
    getUnappliedCount,
    markAsApplied,
    clearHistory,
    removeEntries,

    mountSyncButton,
    openSyncModal,
    updateAllBadges,

    // API
    listProfiles,
    getTarget,
    saveTarget,
    setAsTarget,
    compareSchema,
    applyOperations,
    syncMap,
    pullPreview,
    pullFromTarget,
    createBackup,
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.Sync = Sync;
})();
