(() => {
  // Cross-connection sync module.
  // Records CRUD operations and allows replaying them on a target DB.
  // Requires: EditorCore.UI, EditorCore.DBCrud, EditorCore.utils

  const API = 'api/db-sync.php';
  const FETCH_TIMEOUT_MS = 30000;
  const STORAGE_KEY = 'es_sync_history';
  const MAX_HISTORY = 500;

  // ── Network layer ─────────────────────────────────────────────────────────

  const notifyAuthRequired = () => {
    try { window.top?.postMessage({ type: 'editor-shell:auth-required' }, '*'); } catch {}
  };

  const jsonFetch = async (url, options = {}) => {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS);
    try {
      const res = await fetch(url, {
        credentials: 'same-origin',
        headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
        ...options,
        signal: controller.signal,
      });
      const text = await res.text();
      let payload = null;
      try { payload = text ? JSON.parse(text) : null; } catch { payload = { ok: false, error: 'Invalid JSON response', raw: text }; }
      if (res.status === 401) notifyAuthRequired();
      if (!res.ok) throw new Error(payload?.error || `HTTP ${res.status}`);
      if (payload && payload.ok === false) throw new Error(payload.error || 'Request failed');
      return payload;
    } catch (err) {
      if (err?.name === 'AbortError') throw new Error('Превышено время ожидания запроса');
      if (err instanceof TypeError) throw new Error('Ошибка сети');
      throw err;
    } finally {
      clearTimeout(timeoutId);
    }
  };

  const qs = (obj) => Object.entries(obj)
    .filter(([, v]) => v !== undefined && v !== null)
    .map(([k, v]) => `${encodeURIComponent(k)}=${encodeURIComponent(String(v))}`)
    .join('&');

  // ── Change history storage ────────────────────────────────────────────────

  const generateId = () => Date.now().toString(36) + Math.random().toString(36).slice(2, 8);

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
      data: data ? JSON.parse(JSON.stringify(data)) : null,
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

  const escapeHtml = (s) => {
    const d = document.createElement('div');
    d.textContent = String(s ?? '');
    return d.innerHTML;
  };

  const RESOURCE_LABELS = {
    vouchers: 'Ваучеры',
    bots_mobs: 'Мобы',
    bots_info: 'Боты',
    bots_npc: 'NPC',
    crafts: 'Крафты',
    warehouses: 'Склады',
    worlds: 'Миры',
    aethers: 'Телепорты',
    items: 'Предметы',
    dungeons: 'Подземелья',
    quests: 'Квесты',
    quest_bots: 'Шаги квестов',
  };

  const ACTION_LABELS = {
    create: { text: 'Создание', icon: 'fa-plus', color: '#34d399' },
    update: { text: 'Обновление', icon: 'fa-pen', color: '#60a5fa' },
    delete: { text: 'Удаление', icon: 'fa-trash', color: '#f87171' },
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
      return `${fields.slice(0, 3).join(', ')} и ещё ${fields.length - 3}`;
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

    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'editor-btn editor-btn-sync';
    btn.title = 'Синхронизация с другой БД';
    btn.innerHTML = `<i class="fa-solid fa-arrows-rotate"></i><span>Синхро</span>`;
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

    updateBadge();

    // Expose updater so editors can call it after operations
    btn._updateBadge = updateBadge;

    return btn;
  };

  const openSyncModal = async (cfg = {}) => {
    const editorName = cfg.editorName || 'Редактор';

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
              Синхронизация изменений
            </h3>
            <p class="editor-muted-text text-sm mt-1">Применение изменений из «${escapeHtml(editorName)}» в другую базу данных.</p>
          </div>
          <button type="button" class="editor-icon-btn" data-sync-action="close" title="Закрыть">
            <i class="fa-solid fa-xmark"></i>
          </button>
        </div>

        <div class="editor-sync-body">
          <!-- Target info -->
          <div class="editor-sync-target" data-sync-role="target-info">
            <div class="editor-muted-text">Загрузка…</div>
          </div>

          <!-- Schema comparison result -->
          <div class="editor-sync-schema" data-sync-role="schema-result"></div>

          <!-- Operations list -->
          <div class="editor-sync-ops" data-sync-role="operations">
            <div class="editor-muted-text">Загрузка…</div>
          </div>

          <!-- Apply form -->
          <div class="editor-sync-apply" data-sync-role="apply-form" style="display:none">
            <div class="editor-sync-confirm">
              <label class="editor-label">Для подтверждения введите <strong>Применить</strong></label>
              <input type="text" class="w-full editor-input form-input" data-sync-role="confirm-input" placeholder="Применить" />
            </div>
          </div>
        </div>

        <div class="editor-sync-footer">
          <button type="button" class="editor-btn editor-btn-secondary" data-sync-action="close">Закрыть</button>
          <button type="button" class="editor-btn editor-btn-secondary" data-sync-action="check" disabled>
            <i class="fa-solid fa-shield-halved"></i><span>Проверить структуру</span>
          </button>
          <button type="button" class="editor-btn editor-btn-primary" data-sync-action="apply" disabled>
            <i class="fa-solid fa-play"></i><span>Применить</span>
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
              <span class="editor-sync-status-ok"><i class="fa-solid fa-circle-check"></i> Настроено</span>
            </div>
          `;
          btnCheck.disabled = false;
        } else {
          targetInfo = null;
          $('target-info').innerHTML = `
            <div class="editor-sync-target-card editor-sync-target-empty">
              <div class="flex items-center gap-2">
                <i class="fa-solid fa-database text-slate-500"></i>
                <div>
                  <div class="font-semibold">Целевая БД не настроена</div>
                  <div class="text-xs editor-muted-text">Перейдите в Настройки → раздел «Синхронизация» для настройки.</div>
                </div>
              </div>
              <span class="editor-sync-status-err"><i class="fa-solid fa-circle-xmark"></i> Не настроено</span>
            </div>
          `;
          btnCheck.disabled = true;
          btnApply.disabled = true;
        }
      } catch (err) {
        $('target-info').innerHTML = `<div class="text-red-400 text-sm">Ошибка загрузки: ${escapeHtml(err.message)}</div>`;
      }
    };

    // Render operations
    const renderOperations = () => {
      const ops = getUnappliedOperations();
      const opsContainer = $('operations');

      if (!ops.length) {
        opsContainer.innerHTML = `
          <div class="editor-sync-ops-empty">
            <i class="fa-solid fa-circle-check text-emerald-400 text-2xl"></i>
            <div class="font-semibold">Нет неприменённых изменений</div>
            <div class="editor-muted-text text-sm">Все изменения уже синхронизированы или история пуста.</div>
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
            Неприменённые изменения: <span class="text-accent">${ops.length}</span>
          </div>
          <button type="button" class="editor-btn editor-btn-secondary text-xs" data-sync-action="clear-history" style="padding:4px 10px">
            <i class="fa-solid fa-broom"></i><span>Очистить историю</span>
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
              <span class="editor-muted-text">${entries.length} оп.</span>
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
              <button type="button" class="editor-icon-btn editor-icon-danger editor-sync-op-remove" data-sync-remove="${entry.id}" title="Убрать из очереди">
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
      opsContainer.querySelector('[data-sync-action="clear-history"]')?.addEventListener('click', () => {
        if (!confirm('Очистить всю историю изменений? Это действие нельзя отменить.')) return;
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
          <span>Сравнение структур БД…</span>
        </div>
      `;

      try {
        const res = await compareSchema({});
        if (!res.ok) throw new Error(res.error || 'Ошибка сравнения');

        const c = res.comparison;
        let html = '';

        if (c.compatible) {
          schemaOk = true;
          html = `
            <div class="editor-sync-schema-ok">
              <i class="fa-solid fa-circle-check text-emerald-400"></i>
              <div>
                <div class="font-semibold text-emerald-300">Структуры совместимы</div>
                <div class="text-xs editor-muted-text">
                  Таблиц OK: ${c.tables_ok.length}
                  ${c.tables_missing.length ? `, отсутствуют: ${c.tables_missing.length}` : ''}
                  ${c.issues.filter(i => i.level === 'warning').length ? `, предупреждений: ${c.issues.filter(i => i.level === 'warning').length}` : ''}
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
                <div class="font-semibold text-red-300">Структуры несовместимы</div>
                <div class="text-xs editor-muted-text">Применение изменений невозможно без обновления структуры целевой БД.</div>
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
              <div class="font-semibold text-red-300">Ошибка проверки</div>
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
        toast('Сначала проверьте совместимость структур.', 'error');
        return;
      }

      const confirm = confirmInput?.value?.trim() || '';
      if (confirm.toUpperCase() !== 'ПРИМЕНИТЬ' && confirm.toUpperCase() !== 'APPLY') {
        toast('Введите "Применить" для подтверждения.', 'error');
        confirmInput?.focus();
        return;
      }

      const entries = getUnappliedOperations();
      if (!entries.length) {
        toast('Нет неприменённых операций.', 'warning');
        return;
      }

      const operations = buildOperationsFromHistory(entries);

      btnApply.disabled = true;
      btnApply.innerHTML = `<i class="fa-solid fa-spinner fa-spin"></i><span>Применение…</span>`;

      try {
        const res = await applyOperations({
          operations,
          confirmPhrase: confirm,
        });

        if (!res.ok) {
          // Show validation errors if any
          if (res.validation_errors) {
            let msg = 'Ошибки валидации:\n';
            for (const ve of (res.validation_errors || [])) {
              msg += `  #${ve.index}: ${ve.error}\n`;
            }
            toast(msg, 'error');
          } else {
            toast(res.error || 'Ошибка применения', 'error');
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

        toast(`Успешно применено: ${res.success} операций в «${res.target?.name || 'Target'}»`, 'success');

        // Update UI
        renderOperations();
        updateAllBadges();
        $('apply-form').style.display = 'none';
        $('schema-result').innerHTML = `
          <div class="editor-sync-schema-ok">
            <i class="fa-solid fa-circle-check text-emerald-400"></i>
            <div>
              <div class="font-semibold text-emerald-300">Изменения применены</div>
              <div class="text-xs editor-muted-text">
                Успешно: ${res.success} | Цель: ${escapeHtml(res.target?.database || '')}
              </div>
            </div>
          </div>
        `;

      } catch (err) {
        toast(err.message || 'Ошибка применения', 'error');
      } finally {
        btnApply.disabled = false;
        btnApply.innerHTML = `<i class="fa-solid fa-play"></i><span>Применить в ${escapeHtml(targetInfo?.name || 'Target')}</span>`;
      }
    });

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
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.Sync = Sync;
})();
