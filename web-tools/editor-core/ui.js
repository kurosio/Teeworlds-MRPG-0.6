(() => {
  const createElementFromHTML = (html) => {
    const template = document.createElement('template');
    template.innerHTML = html.trim();
    return template.content.firstChild;
  };

  const mountToastContainer = ({ id = 'toast-container', className = 'editor-toast-container', parent = document.body } = {}) => {
    let container = document.getElementById(id);
    if (!container) {
      container = document.createElement('div');
      container.id = id;
      container.className = className;
      parent.appendChild(container);
    }
    return container;
  };

  const mountModal = ({ id, html, parent = document.body }) => {
    let modal = document.getElementById(id);
    if (!modal) {
      modal = createElementFromHTML(html);
      parent.appendChild(modal);
    }
    return modal;
  };

  const mountListContainer = ({ id, parent, className = '', emptyMessage }) => {
    let container = document.getElementById(id);
    if (!container) {
      container = document.createElement('div');
      container.id = id;
      if (className) {
        container.className = className;
      }
      parent.appendChild(container);
    }
    if (emptyMessage) {
      container.dataset.emptyMessage = emptyMessage;
    }
    return container;
  };

  const renderEmptyList = (container, message) => {
    if (!container) return;
    container.innerHTML = `<div class="editor-empty-state">${message}</div>`;
  };

  const applyEmptyStateClass = (container, hasItems, emptyClass) => {
    if (!container || !emptyClass) return;
    container.classList.toggle(emptyClass, !hasItems);
  };

  const mountEventEditorUI = () => {
    mountModal({
      id: 'add-step-modal',
      html: `
        <div id="add-step-modal" class="fixed inset-0 z-50 items-center justify-center hidden editor-modal-backdrop">
          <div class="editor-modal-content p-6 w-full max-w-md">
            <h3 class="text-xl font-bold mb-4">Выберите тип элемента</h3>
            <select id="action-select" class="w-full p-2 rounded-md form-input mb-4"></select>
            <div class="flex justify-end space-x-2">
              <button data-action="cancel-add" class="editor-btn editor-btn-secondary">Отмена</button>
              <button data-action="confirm-add" class="editor-btn editor-btn-primary">Добавить</button>
            </div>
          </div>
        </div>
      `
    });

    mountModal({
      id: 'export-modal',
      html: `
        <div id="export-modal" class="fixed inset-0 z-50 items-center justify-center hidden editor-modal-backdrop">
          <div class="editor-modal-content p-6 w-full max-w-2xl flex flex-col" style="height: 80vh;">
            <h3 class="text-xl font-bold mb-4">Экспортированный JSON</h3>
            <textarea id="json-output" readonly class="w-full flex-grow p-3 rounded-md form-input font-mono text-sm resize-none"></textarea>
            <div class="flex justify-end space-x-2 mt-4">
              <button id="copy-json-btn" class="editor-btn editor-btn-primary"><i class="fas fa-copy mr-2"></i>Копировать</button>
              <button data-action="close-modal" class="editor-btn editor-btn-secondary">Закрыть</button>
            </div>
          </div>
        </div>
      `
    });

    mountModal({
      id: 'confirm-modal',
      html: `
        <div id="confirm-modal" class="fixed inset-0 z-50 items-center justify-center hidden editor-modal-backdrop">
          <div class="editor-modal-content p-6 w-full max-w-lg">
            <h3 id="confirm-title" class="text-xl font-bold mb-4"></h3>
            <p id="confirm-message" class="mb-6"></p>
            <div id="confirm-buttons" class="flex justify-end space-x-3"></div>
          </div>
        </div>
      `
    });

    mountModal({
      id: 'undo-toast',
      html: `
        <div id="undo-toast" class="fixed bottom-5 right-5 z-50 p-4 rounded-lg shadow-lg flex items-center gap-4 editor-modal-content">
          <span id="undo-message"></span>
          <button id="undo-btn" class="editor-btn editor-btn-secondary">Отменить</button>
        </div>
      `
    });
  };

  const mountScenarioEditorUI = () => {
    mountToastContainer({ id: 'toast-container' });

    mountModal({
      id: 'modal-backdrop',
      html: `
        <div id="modal-backdrop" class="hidden fixed inset-0 editor-modal-backdrop z-[200] flex justify-center items-center p-4">
          <div id="modal-content" class="editor-modal-content p-6 rounded-lg shadow-xl w-full max-w-xl flex flex-col max-h-[90vh]">
            <h3 id="modal-title" class="text-xl font-bold mb-4">Редактирование</h3>
            <div id="modal-form-container" class="overflow-y-auto pr-4 -mr-4"></div>
            <div class="modal-buttons text-right mt-6 pt-4 border-t border-slate-200/30">
              <button id="modal-cancel" class="editor-btn editor-btn-secondary">Отмена</button>
              <button id="modal-save" class="editor-btn editor-btn-primary ml-2">Сохранить</button>
            </div>
          </div>
        </div>
      `
    });

    mountModal({
      id: 'delete-group-modal-backdrop',
      html: `
        <div id="delete-group-modal-backdrop" class="hidden fixed inset-0 editor-modal-backdrop z-[300] flex justify-center items-center p-4">
          <div class="editor-modal-content p-6 rounded-lg shadow-xl w-full max-w-md">
            <h3 class="text-xl font-bold mb-4">Удалить группу</h3>
            <p class="editor-muted-text mb-6">Как вы хотите удалить эту группу?</p>
            <div class="flex justify-end gap-3">
              <button id="delete-group-cancel-btn" class="editor-btn editor-btn-secondary">Отмена</button>
              <button id="delete-group-only-btn" class="editor-btn editor-btn-secondary" style="border-color: rgba(245,158,11,.35); color: rgb(253,230,138);">Только группу</button>
              <button id="delete-group-and-steps-btn" class="editor-btn editor-btn-danger">Группу и шаги</button>
            </div>
          </div>
        </div>
      `
    });
  };

  /**
   * Notifications (toasts)
   * Unified helper so editors can call EditorCore.UI.toast(...)
   */
  const toast = (message, type = 'success', opts = {}) => {
    if (window.EditorCore?.utils?.showToast) {
      return window.EditorCore.utils.showToast(message, type, {
        // Theme-friendly defaults
        baseClass: opts.baseClass || 'editor-modal-content border border-light px-4 py-3 rounded-xl shadow-lg transition-all duration-300 transform translate-x-full',
        successClass: opts.successClass || 'border-emerald-400/30',
        warningClass: opts.warningClass || 'border-amber-400/30',
        errorClass: opts.errorClass || 'border-red-400/30',
        ttl: typeof opts.ttl === 'number' ? opts.ttl : 2800,
        containerId: opts.containerId || 'toast-container'
      });
    }
  };

  /**
   * Tabs (editor-agnostic)
   * @param {HTMLElement} root
   * @param {{tabs:Array<{id:string,title:string,icon?:string}>, active?:string, onChange?:(id:string)=>void}} cfg
   */
  const mountTabs = (root, cfg) => {
    if (!root) return null;
    const tabs = Array.isArray(cfg?.tabs) ? cfg.tabs : [];
    const state = { active: cfg?.active || (tabs[0]?.id || '') };

    const render = () => {
      root.innerHTML = '';
      const wrap = document.createElement('div');
      wrap.className = 'flex flex-wrap gap-2';
      for (const t of tabs) {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = `editor-btn editor-btn-secondary ${t.id === state.active ? 'active' : ''}`;
        btn.dataset.tabId = t.id;
        btn.innerHTML = `${t.icon ? `<i class="fa-solid ${t.icon}"></i>` : ''}<span>${t.title}</span>`;
        btn.addEventListener('click', () => {
          state.active = t.id;
          render();
          if (typeof cfg?.onChange === 'function') cfg.onChange(t.id);
        });
        wrap.appendChild(btn);
      }
      root.appendChild(wrap);
    };

    render();
    return { getActive: () => state.active, setActive: (id) => { state.active = id; render(); } };
  };

  /**
   * Table (simple, editor-agnostic)
   * @param {HTMLElement} root
   * @param {{columns:Array<{key:string,title:string,mono?:boolean}>, rows:Array<Record<string, any>>}} cfg
   */
  const renderTable = (root, cfg) => {
    if (!root) return;
    const cols = Array.isArray(cfg?.columns) ? cfg.columns : [];
    const rows = Array.isArray(cfg?.rows) ? cfg.rows : [];
    if (!rows.length) {
      root.innerHTML = `<div class="p-3 editor-muted-text">Нет данных</div>`;
      return;
    }
    const thead = cols.map(c => `<th class="text-left px-3 py-2 text-slate-200">${c.title}</th>`).join('');
    const tbody = rows.map(r => {
      const tds = cols.map(c => {
        const v = r[c.key];
        const cls = c.mono ? 'font-mono text-slate-200' : 'text-slate-100';
        return `<td class="px-3 py-2 ${cls}">${String(v ?? '')}</td>`;
      }).join('');
      return `<tr class="border-b border-light">${tds}</tr>`;
    }).join('');
    root.innerHTML = `
      <table class="w-full text-sm">
        <thead><tr class="bg-light border-b border-light">${thead}</tr></thead>
        <tbody>${tbody}</tbody>
      </table>`;
  };

  /**
   * Date/Time picker in a modal (reusable).
   * Uses the shared modal styling from editor-theme.css.
   * @param {{title?:string, valueSec?:number, onApply?:(sec:number)=>void, withTime?:boolean}} cfg
   */
  const openDateTimeModal = (cfg = {}) => {
    const id = 'editor-datetime-modal';
    mountModal({
      id,
      html: `
        <div id="${id}" class="fixed inset-0 z-[500] items-center justify-center hidden editor-modal-backdrop">
          <div class="editor-modal-content p-6 w-full max-w-md">
            <h3 class="text-xl font-bold mb-4" id="${id}-title"></h3>
            <div class="space-y-2">
              <label class="editor-label">Дата и время</label>
              <input id="${id}-input" type="datetime-local" class="w-full form-input" />
              <div class="editor-muted-text text-sm">Сохранится как Unix-время (секунды).</div>
            </div>
            <div class="flex justify-end gap-2 mt-6">
              <button id="${id}-cancel" class="editor-btn editor-btn-secondary">Отмена</button>
              <button id="${id}-apply" class="editor-btn editor-btn-primary">Применить</button>
            </div>
          </div>
        </div>
      `
    });

    const modal = document.getElementById(id);
    const title = document.getElementById(`${id}-title`);
    const input = document.getElementById(`${id}-input`);
    const cancel = document.getElementById(`${id}-cancel`);
    const apply = document.getElementById(`${id}-apply`);

    const toLocalInput = (sec) => {
      if (!sec) return '';
      const d = new Date(sec * 1000);
      const pad = (n) => String(n).padStart(2, '0');
      const yyyy = d.getFullYear();
      const mm = pad(d.getMonth() + 1);
      const dd = pad(d.getDate());
      const hh = pad(d.getHours());
      const mi = pad(d.getMinutes());
      return `${yyyy}-${mm}-${dd}T${hh}:${mi}`;
    };

    title.textContent = cfg.title || 'Выбор даты';
    input.value = toLocalInput(Number(cfg.valueSec || 0));

    const close = () => {
      modal.classList.add('hidden');
      modal.classList.remove('flex');
    };

    cancel.onclick = close;
    modal.addEventListener('click', (e) => {
      if (e.target === modal) close();
    });

    apply.onclick = () => {
      const dt = input.value ? new Date(input.value) : null;
      const sec = dt ? Math.floor(dt.getTime() / 1000) : 0;
      if (typeof cfg.onApply === 'function') cfg.onApply(Number.isFinite(sec) ? sec : 0);
      close();
    };

    modal.classList.remove('hidden');
    modal.classList.add('flex');
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.UI = {
    mountToastContainer,
    mountModal,
    mountListContainer,
    renderEmptyList,
    applyEmptyStateClass,
    mountEventEditorUI,
    mountScenarioEditorUI,
    toast,
    mountTabs,
    renderTable,
    openDateTimeModal
  };
})();
