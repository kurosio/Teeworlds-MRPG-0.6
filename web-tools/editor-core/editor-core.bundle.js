/* ===== ui.js ===== */
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



/* ===== utils.js ===== */
(() => {
  const ensureUI = () => {
    if (!window.EditorCore?.UI) {
      throw new Error('EditorCore.UI is not available. Load ui.js before utils.js.');
    }
  };

  const uuid = () => ([1e7] + -1e3 + -4e3 + -8e3 + -1e11)
    .replace(/[018]/g, c => (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16));

  const debounce = (fn, delay = 200) => {
    let t;
    return (...args) => {
      clearTimeout(t);
      t = setTimeout(() => fn(...args), delay);
    };
  };

  const rafThrottle = (fn) => {
    let ticking = false;
    let lastArgs = null;
    return (...args) => {
      lastArgs = args;
      if (ticking) return;
      ticking = true;
      requestAnimationFrame(() => {
        ticking = false;
        fn(...(lastArgs || []));
      });
    };
  };

  const copyText = async (text) => {
    if (navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(String(text));
      return true;
    }
    const ta = document.createElement('textarea');
    ta.value = String(text);
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.focus();
    ta.select();
    const ok = document.execCommand('copy');
    ta.remove();
    return ok;
  };

  const downloadText = (filename, text, mime = 'text/plain') => {
    const blob = new Blob([String(text)], { type: mime });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  };

  const downloadJson = (filename, obj) => downloadText(filename, JSON.stringify(obj, null, 2), 'application/json');

  const readJsonFile = (file) => new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => {
      try { resolve(JSON.parse(String(reader.result || 'null'))); }
      catch (e) { reject(e); }
    };
    reader.onerror = () => reject(reader.error || new Error('Failed to read file'));
    reader.readAsText(file);
  });

  const fetchJson = async (url, options = {}) => {
    const res = await fetch(url, {
      headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
      ...options
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const text = await res.text();
    return text ? JSON.parse(text) : null;
  };

  const showToast = (message, type = 'success', opts = {}) => {
    ensureUI();
    const container = window.EditorCore.UI.mountToastContainer({ id: opts.containerId || 'toast-container' });
    const toast = document.createElement('div');
    const base = opts.baseClass || 'toast text-white font-semibold py-3 px-5 rounded-lg shadow-lg transition-all duration-300 transform translate-x-full';
    const cls = type === 'error'
      ? (opts.errorClass || 'bg-red-500')
      : type === 'warning'
        ? (opts.warningClass || 'bg-yellow-500')
        : (opts.successClass || 'bg-green-500');
    toast.className = `${base} ${cls}`;
    toast.textContent = String(message);
    container.appendChild(toast);
    setTimeout(() => toast.classList.remove('translate-x-full'), 10);
    const ttl = typeof opts.ttl === 'number' ? opts.ttl : 3000;
    setTimeout(() => {
      toast.classList.add('translate-x-full');
      toast.addEventListener('transitionend', () => toast.remove(), { once: true });
    }, ttl);
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.utils = {
    uuid,
    debounce,
    rafThrottle,
    copyText,
    downloadText,
    downloadJson,
    readJsonFile,
    fetchJson,
    showToast,
  };
})();



/* ===== registry.js ===== */
(() => {
  const registry = new Map();
  const createField = (type, label, defaultValue, { validate = null, ui = null, ...rest } = {}) => ({
    type,
    label,
    default: defaultValue,
    validate,
    ui,
    ...rest
  });

  // One-liner for DB-backed selects that gracefully fallback to manual ID input when DB is unavailable.
// Usage: createDbSelect('Квест', 0, 'quest')
const createDbSelect = (label, defaultValue, dbKey, { ui = {}, validate = null, ...rest } = {}) =>
  createField('db_select', label, defaultValue, {
    validate,
    ui: { dbKey, ...ui },
    ...rest
  });

  const actionSchemas = {
    group_header: {
      name: 'Заголовок группы',
      icon: 'fa-layer-group',
      fields: {
        name: createField('text', 'Название', 'Новая группа', {
          ui: { placeholder: 'Введите название группы' }
        })
      }
    },
    message: {
      name: 'Сообщение',
      icon: 'fa-comment',
      fields: {
        delay: createField('number', 'Задержка', 0, { ui: { min: 0, max: 600000 } }),
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } }),
        broadcast: createField('text', 'Броадкаст', '', { ui: { placeholder: 'Введите текст для броадкаста', format: 'textarea' } }),
        full: createField('text', 'Полное сообщение', '', { ui: { placeholder: 'Введите полное сообщение', format: 'textarea' } })
      }
    },
    movement_task: {
      name: 'Задача на движение',
      icon: 'fa-person-running',
      fields: {
        delay: createField('number', 'Задержка', 0, { ui: { min: 0, max: 600000 } }),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        target_lock_text: createField('text', 'Текст фиксации цели', '', { ui: { placeholder: 'Например: Следуйте за маркером' } }),
        target_look: createField('boolean', 'Смотреть на цель', false),
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } }),
        broadcast: createField('text', 'Броадкаст', '', { ui: { placeholder: 'Введите текст для броадкаста', format: 'textarea' } }),
        full: createField('text', 'Полное сообщение', '', { ui: { placeholder: 'Введите полное сообщение', format: 'textarea' } })
      }
    },
    check_has_item: {
      name: 'Проверить предмет',
      icon: 'fa-box',
      fields: {
        item_id: createDbSelect('Предмет', 0, 'item', { ui: { placeholder: '— выберите предмет —' } }),
        required: createField('number', 'Количество', 1, { ui: { min: 1, max: 9999 } }),
        remove: createField('boolean', 'Удалить после проверки', false),
        show_progress: createField('boolean', 'Показать прогресс', false)
      }
    },
    reset_quest: {
      name: 'Сбросить квест',
      icon: 'fa-undo',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } })
      }
    },
    accept_quest: {
      name: 'Принять квест',
      icon: 'fa-check-double',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } })
      }
    },
    new_door: {
      name: 'Создать дверь',
      icon: 'fa-door-closed',
      fields: {
        key: createField('door_key', 'Ключ', '', { ui: { placeholder: 'Введите или выберите ключ' } }),
        follow: createField('boolean', 'Следовать за игроком', false),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
    remove_door: {
      name: 'Удалить дверь',
      icon: 'fa-door-open',
      fields: {
        key: createField('door_key', 'Ключ', '', { ui: { placeholder: 'Введите или выберите ключ' } }),
        follow: createField('boolean', 'Следовать за игроком', false)
      }
    },
    pick_item_task: {
      name: 'Задача на подбор',
      icon: 'fa-hand-sparkles',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        item: createField('item', 'Предмет', { id: 0, value: 0 }, { ui: { datasource: 'items', placeholder: '— выберите предмет —', min: 0, max: 999999 } }),
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } }),
        broadcast: createField('text', 'Броадкаст', '', { ui: { placeholder: 'Введите текст для броадкаста', format: 'textarea' } }),
        full: createField('text', 'Полное сообщение', '', { ui: { placeholder: 'Введите полное сообщение', format: 'textarea' } })
      }
    },
    emote: {
      name: 'Эмоция',
      icon: 'fa-smile',
      fields: {
        emote_type: createField('number', 'Тип эмоции', 0, { ui: { min: 0, max: 999 } }),
        emoticon_type: createField('number', 'Тип эмоции (иконка)', 0, { ui: { min: 0, max: 999 } })
      }
    },
    teleport: {
      name: 'Телепорт',
      icon: 'fa-plane-departure',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        world_id: createDbSelect('Мир', 0, 'world', { ui: { placeholder: '— выберите мир —' } })
      }
    },
    use_chat_task: {
      name: 'Задача на чат',
      icon: 'fa-keyboard',
      fields: {
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } })
      }
    },
    fix_cam: {
      name: 'Фиксировать камеру',
      icon: 'fa-camera',
      fields: {
        delay: createField('number', 'Задержка', 0, { ui: { min: 0, max: 600000 } }),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
    freeze_movements: {
      name: 'Заморозить движение',
      icon: 'fa-snowflake',
      fields: {
        state: createField('boolean', 'Включить', false)
      }
    },
    check_quest_accepted: {
      name: 'Проверка: квест принят',
      icon: 'fa-question-circle',
      fields: {
        quest_id: createField('db_select', 'Квест', 0, { datasource: 'quests', ui: { placeholder: '— выберите квест —' } })
      }
    },
    check_quest_finished: {
      name: 'Проверка: квест завершен',
      icon: 'fa-flag-checkered',
      fields: {
        quest_id: createField('db_select', 'Квест', 0, { datasource: 'quests', ui: { placeholder: '— выберите квест —' } })
      }
    },
    check_quest_step_finished: {
      name: 'Проверка: шаг квеста завершен',
      icon: 'fa-list-check',
      fields: {
        quest_id: createField('db_select', 'Квест', 0, { datasource: 'quests', ui: { placeholder: '— выберите квест —' } }),
        step: createField('number', 'Номер шага', 0, { ui: { min: 0, max: 9999 } })
      }
    },
    shootmarkers: {
      name: 'Стрельба по маркерам',
      icon: 'fa-crosshairs',
      fields: {
        markers: createField('list', 'Маркеры', [], {
          itemFields: {
            position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
            health: createField('number', 'Здоровье', 1, { ui: { min: 1, max: 9999 } })
          },
          ui: { placeholder: 'Добавьте маркер' }
        })
      }
    }
  };

  const componentSchemas = {
    message: {
      name: 'Сообщение',
      class: 'info',
      icon: 'fa-solid fa-comment-dots',
      desc: 'Показать текст игроку',
      fields: {
        text: createField('text', 'Текст', 'Новое сообщение', { ui: { placeholder: 'Введите текст сообщения', format: 'textarea' } }),
        mode: createField('text', 'Режим', 'full', {
          ui: { type: 'select', options: ['full', 'broadcast', 'chat'] }
        })
      }
    },
    wait: {
      name: 'Ожидание',
      class: 'info',
      icon: 'fa-solid fa-clock',
      desc: 'Пауза в сценарии',
      fields: {
        duration: createField('number', 'Длительность', 5, { ui: { min: 1, max: 9999 } })
      }
    },
    door_control: {
      name: 'Управление дверью',
      class: 'interactive',
      icon: 'fa-solid fa-door-open',
      desc: 'Создать или удалить дверь',
      fields: {
        action: createField('text', 'Действие', 'create', {
          ui: { type: 'select', options: ['create', 'remove'] }
        }),
        position: createField('vec2', 'Позиция', { x: 100, y: 100 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        key: createField('text', 'Ключ', '', { ui: { placeholder: 'Введите ключ двери' } })
      }
    },
    use_chat_code: {
      name: 'Код в чате',
      class: 'interactive',
      icon: 'fa-solid fa-key',
      desc: 'Переход по кодовому слову',
      fields: {
        code: createField('text', 'Код', 'secret', { ui: { placeholder: 'Введите кодовое слово' } }),
        next_step_id: createField('text', 'Следующий шаг', ''),
        hidden: createField('boolean', 'Скрыть', false)
      }
    },
    defeat_mobs: {
      name: 'Убийство мобов',
      class: 'combat',
      icon: 'fa-solid fa-skull-crossbones',
      desc: 'Боевое испытание',
      fields: {
        mode: createField('text', 'Режим', 'annihilation', {
          ui: { type: 'select', options: ['annihilation', 'wave', 'survival'] }
        }),
        radius: createField('number', 'Радиус', 100, { ui: { min: 0, max: 99999 } }),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        kill_target: createField('number', 'Цель убийств', 10, { ui: { min: 1, max: 9999 } }),
        duration: createField('number', 'Длительность', 60, { ui: { min: 1, max: 99999 } }),
        mobs: createField('list', 'Мобы', [{ mob_id: 21, count: 5, level: 1, power: 1, boss: false }], {
          ui: { layout: 'grid', addLabel: 'Добавить моба' },
          itemFields: {
            mob_id: createDbSelect('Моб', 21, 'mob', { ui: { placeholder: '— выберите моба —' } }),
            count: createField('number', 'Количество', 5, { ui: { min: 1, max: 999 } }),
            level: createField('number', 'Уровень', 1, { ui: { min: 1, max: 999 } }),
            power: createField('number', 'Сила', 1, { ui: { min: 1, max: 999 } }),
            boss: createField('boolean', 'Босс', false)
          }
        })
      }
    },
    follow_camera: {
      name: 'Следование камеры',
      class: 'camera',
      icon: 'fa-solid fa-video',
      desc: 'Переместить камеру к точке',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        delay: createField('number', 'Задержка', 300, { ui: { min: 0, max: 600000 } }),
        smooth: createField('boolean', 'Плавно', true)
      }
    },
    condition_movement: {
      name: 'Условие: Движение',
      class: 'interactive',
      icon: 'fa-solid fa-person-running',
      desc: 'Переход при достижении точки',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        entire_group: createField('boolean', 'Вся группа', true)
      }
    },
    teleport: {
      name: 'Телепорт',
      class: 'interactive',
      icon: 'fa-solid fa-bolt',
      desc: 'Мгновенное перемещение',
      fields: {
        position: createField('vec2', 'Позиция', { x: 1497, y: 529 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
    activate_point: {
      name: 'Точка активации',
      class: 'interactive',
      icon: 'fa-solid fa-location-dot',
      desc: 'Активация по времени в области',
      fields: {
        position: createField('vec2', 'Позиция', { x: 400, y: 3000 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        duration: createField('number', 'Длительность', 5, { ui: { min: 1, max: 9999 } }),
        entire_group: createField('boolean', 'Вся группа', true),
        action_text: createField('text', 'Текст действия', 'Перегрузка главного реактора', { ui: { placeholder: 'Описание действия', format: 'textarea' } })
      }
    }
  };

  const register = (id, app) => {
    if (!id) {
      throw new Error('Editor registry requires an id.');
    }
    registry.set(id, app);
    return app;
  };

  const get = (id) => registry.get(id);
  const list = () => Array.from(registry.keys());
  const cloneValue = (value) => {
    if (value && typeof value === 'object') {
      return JSON.parse(JSON.stringify(value));
    }
    return value;
  };
  const applyDefaults = (target, fields = {}) => {
    Object.entries(fields).forEach(([key, field]) => {
      if (target[key] === undefined) {
        target[key] = cloneValue(field.default);
      } else if (field.type === 'vec2' && typeof target[key] === 'object' && target[key] !== null) {
        target[key].x = target[key].x ?? field.default?.x ?? 0;
        target[key].y = target[key].y ?? field.default?.y ?? 0;
      } else if (field.type === 'item' && typeof target[key] === 'object' && target[key] !== null) {
        target[key].id = target[key].id ?? field.default?.id ?? 0;
        target[key].value = target[key].value ?? field.default?.value ?? 0;
      } else if (field.type === 'list' && Array.isArray(target[key]) && field.itemFields) {
        target[key] = target[key].map((item) => applyDefaults(item, field.itemFields));
      }
    });
    return target;
  };
  const buildDefaults = (fields = {}) => applyDefaults({}, fields);

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.registry = { register, get, list };
  window.EditorCore.schemas = {
    actionSchemas,
    componentSchemas,
    applyDefaults,
    buildDefaults
  };
})();



/* ===== core.js ===== */
(() => {
  const createEditorApp = ({ id, init, render, state = {} }) => {
    if (!window.EditorCore?.registry) {
      throw new Error('EditorCore registry is not available.');
    }

    const app = {
      id,
      state,
      async init() {
        if (typeof init === 'function') {
          await init(app);
        }
      },
      render() {
        if (typeof render === 'function') {
          render(app);
        }
      },
      async mount() {
        await app.init();
        app.render();
        window.EditorCore.registry.register(id, app);
        return app;
      }
    };

    return app;
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.createEditorApp = createEditorApp;
})();



/* ===== field-renderer.js ===== */
(() => {
  const parsePath = (path) => path.split('.').map((part) => (/^\d+$/.test(part) ? Number(part) : part));

  const cloneValue = (value) => {
    if (value && typeof value === 'object') {
      return JSON.parse(JSON.stringify(value));
    }
    return value;
  };

  const getValueAtPath = (data, path) => {
    if (!path) return undefined;
    const keys = parsePath(path);
    return keys.reduce((current, key) => (current == null ? undefined : current[key]), data);
  };

  const setValueAtPath = (data, path, value) => {
    if (!path) return;
    const keys = parsePath(path);
    let current = data;
    keys.forEach((key, index) => {
      if (index === keys.length - 1) {
        current[key] = value;
        return;
      }
      const nextKey = keys[index + 1];
      if (current[key] == null) {
        current[key] = typeof nextKey === 'number' ? [] : {};
      } else if (typeof nextKey === 'number' && !Array.isArray(current[key])) {
        current[key] = [];
      } else if (typeof nextKey !== 'number' && typeof current[key] !== 'object') {
        current[key] = {};
      }
      current = current[key];
    });
  };

  const getInputValue = (input) => {
    if (!input) return undefined;
    // Custom widgets can store structured values in hidden inputs.
    // Example: TagSelect stores JSON array in value.
    if (input.dataset?.valueType === 'json_array') {
      const raw = String(input.value || '').trim();
      if (!raw) return [];
      try {
        const arr = JSON.parse(raw);
        if (Array.isArray(arr)) return arr;
      } catch {}
      return raw.split(',').map(s => s.trim()).filter(Boolean);
    }
    // datetime-local parsing to unix seconds (used by some editors)
    if (input.dataset?.valueType === 'unix_seconds') {
      if (!input.value) return 0;
      const dt = new Date(input.value);
      const sec = Math.floor(dt.getTime() / 1000);
      return Number.isFinite(sec) ? sec : 0;
    }
    if (input.multiple) {
      const values = Array.from(input.selectedOptions).map((option) => option.value);
      if (input.dataset?.valueType === 'number') {
        return values.map((v) => (v === '' ? 0 : Number(v))).filter((v) => !Number.isNaN(v));
      }
      return values;
    }
    if (input.type === 'checkbox') {
      return input.checked;
    }
    if (input.type === 'number') {
      return input.value === '' ? 0 : parseFloat(input.value);
    }
    // Select with numeric IDs
    if (input.tagName === 'SELECT' && input.dataset?.valueType === 'number') {
      return input.value === '' ? 0 : Number(input.value);
    }
    return input.value;
  };

  const escapeAttr = (value) => String(value).replace(/"/g, '&quot;');

  const buildDefaultItem = (field) => {
    if (field?.itemDefault !== undefined) {
      return cloneValue(field.itemDefault);
    }
    if (field?.itemFields && window.EditorCore?.schemas?.buildDefaults) {
      return window.EditorCore.schemas.buildDefaults(field.itemFields);
    }
    return {};
  };

  function buildValidationAttr(field) {
    const spec = field?.validate;
    if (!spec) return '';
    if (typeof spec === 'function') return '';
    if (typeof spec === 'string') return `data-validate="${escapeAttr(JSON.stringify({ pattern: spec }))}"`;
    if (typeof spec === 'object') return `data-validate="${escapeAttr(JSON.stringify(spec))}"`;
    return '';
  }

  const buildInputAttributes = ({ path, field, classes, includeName, includeDataKey, includeDataPath, extraAttrs = '' }) => {
    const attrs = [];
    if (includeName) attrs.push(`name="${escapeAttr(path)}"`);
    if (includeDataPath) attrs.push(`data-path="${escapeAttr(path)}"`);
    if (includeDataKey) attrs.push(`data-key="${escapeAttr(path)}"`);
    if (field?.ui?.placeholder) attrs.push(`placeholder="${escapeAttr(field.ui.placeholder)}"`);
    if (field?.ui?.min !== undefined) attrs.push(`min="${escapeAttr(field.ui.min)}"`);
    if (field?.ui?.max !== undefined) attrs.push(`max="${escapeAttr(field.ui.max)}"`);
    if (field?.ui?.step !== undefined) attrs.push(`step="${escapeAttr(field.ui.step)}"`);
    if (classes) attrs.push(`class="${classes}"`);
    const validationAttr = buildValidationAttr(field);
    if (validationAttr) attrs.push(validationAttr);
    if (extraAttrs) attrs.push(extraAttrs);
    return attrs.join(' ');
  };

  const renderSelectOptions = (options, value, multiple = false) => {
    const values = multiple && Array.isArray(value) ? value.map(String) : null;
    return options.map((option) => {
      const optionValue = typeof option === 'object' ? option.value : option;
      const label = typeof option === 'object' ? option.label : option;
      const selected = multiple ? values?.includes(String(optionValue)) : String(optionValue) === String(value);
      return `<option value="${escapeAttr(optionValue)}"${selected ? ' selected' : ''}>${label}</option>`;
    }).join('');
  };

  const renderField = ({ path, field, data, options, isNested = false }) => {
    const value = getValueAtPath(data, path);
    const label = field.label || path.split('.').slice(-1)[0];
    const ui = field.ui || {};
    const showLabel = ui.hideLabel !== true;
    const inputClass = options?.classes?.input || '';
    const baseLabelClass = options?.classes?.label || '';
    const nestedLabelClass = options?.classes?.nestedLabel || baseLabelClass;
    const labelClass = isNested ? nestedLabelClass : baseLabelClass;
    const labelHtml = showLabel ? `<div class="${labelClass}">${label}</div>` : '';
    const fieldWrapperClass = (isNested ? (options?.classes?.nestedFieldWrapper || options?.classes?.fieldWrapper) : options?.classes?.fieldWrapper) || '';
    const checkboxWrapperClass = options?.classes?.checkboxWrapper || '';
    const listItemClass = options?.classes?.listItem || '';
    const listWrapperClass = options?.classes?.listWrapper || '';
    const listAddClass = options?.classes?.listAdd || '';
    const listRemoveClass = options?.classes?.listRemove || '';
    const multiSelectClass = options?.classes?.multiselect || inputClass;
    const textareaClass = options?.classes?.textarea || inputClass;

    const hintHtml = field?.validate
      ? `<div class="editor-validation-hint hidden" data-hint-for="${escapeAttr(path)}"></div>`
      : '';

    const includeName = options?.includeName !== false;
    const includeDataPath = options?.includeDataPath !== false;
    const includeDataKey = options?.includeDataKey === true;

    const renderInput = (type, overrideValue, extraAttrs = '') => {
      const attrs = buildInputAttributes({
        path,
        field,
        classes: inputClass,
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs
      });
      return `<input type="${type}" value="${escapeAttr(overrideValue ?? '')}" ${attrs}>`;
    };

    const renderToggle = () => {
      const attrs = buildInputAttributes({
        path,
        field,
        classes: '',
        includeName,
        includeDataKey,
        includeDataPath
      });
      const checked = !!value;
      // data-checked is used for styling in CSS (no JS required)
      return `
        <label class="editor-toggle" data-checked="${checked ? '1' : '0'}">
          <input type="checkbox" ${checked ? 'checked' : ''} ${attrs}>
          <span class="editor-toggle-track"><span class="editor-toggle-knob"></span></span>
          <span class="editor-label">${label}</span>
        </label>`;
    };

    const renderTextarea = () => {
      const attrs = buildInputAttributes({
        path,
        field,
        classes: textareaClass,
        includeName,
        includeDataKey,
        includeDataPath
      });
      return `<textarea ${attrs}>${value ?? ''}</textarea>`;
    };

    const renderSelect = (multiple = false) => {
      const attrs = buildInputAttributes({
        path,
        field,
        classes: multiple ? multiSelectClass : inputClass,
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: multiple ? 'multiple' : ''
      });
      return `<select ${attrs}>${renderSelectOptions(ui.options || [], value, multiple)}</select>`;
    };

    const resolveDbParams = () => {
      // datasource can be set directly, or derived from semantic dbKey via EditorCore.DBMap.
      let ds = field.datasource || ui.datasource || ui.source;
      const dbKey = ui.dbKey || field.dbKey;
      if (!ds && dbKey && window.EditorCore?.DB) {
        ds = window.EditorCore.DB.resolveSource(dbKey);
      }
      const placeholder = ui.placeholder || '— выберите —';
      const labelMode = (ui.labelMode || (window.EditorCore?.DBMap?.[dbKey]?.labelMode) || 'id_name');
      return { ds: ds || '', dbKey, placeholder, labelMode };
    };

    // Direct DB select (binds to data model).
    // It can be enhanced with search UI via data-db-searchable + data-db-limit.
    const renderDbSelectDirect = (multiple = false) => {
      const { ds, dbKey, placeholder, labelMode } = resolveDbParams();
      if (!ds) {
        // Fallback: show plain input if datasource missing
        return renderInput('number', value ?? 0);
      }

      // Search for ALL db_select types. For multiselect we default to client filter (bulk-load) to keep it simple.
      const serverSearch = (ui.searchServer ?? (dbKey === 'item'));
      const searchable = multiple ? '0' : (serverSearch ? '1' : '0');

      const defaultLimit = (dbKey === 'item') ? 1000 : 300;
      const dbLimit = String(ui.dbLimit || defaultLimit);

      const valueType = ui.valueType || field.valueType || (multiple ? 'string' : 'number');

      const attrs = buildInputAttributes({
        path,
        field,
        classes: multiple ? multiSelectClass : inputClass,
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: `${multiple ? 'multiple' : ''} data-datasource="${escapeAttr(ds)}" data-placeholder="${escapeAttr(placeholder)}" data-label-mode="${escapeAttr(String(labelMode))}" data-db-searchable="${escapeAttr(searchable)}" data-db-limit="${escapeAttr(dbLimit)}" data-value-type="${escapeAttr(String(valueType))}"`
      });

      // Options will be loaded by EditorCore.DB.init(). Keep a minimal placeholder.
      return `<select ${attrs}><option value="">${escapeAttr(placeholder)}</option></select>`;
    };

    // Helper DB select (does NOT bind to data model). It only syncs into the bound input via data-bind-input-path.
    const renderDbSelectHelper = ({ bindInputPath, multiple = false } = {}) => {
      const { ds, dbKey, placeholder, labelMode } = resolveDbParams();
      const classes = multiple ? multiSelectClass : inputClass;
      if (!ds) return '';

      // Search is available for all DB selects.
      // Two modes:
      // - server search (data-db-searchable=1) => query DB by 'search' and render first N rows
      // - client filter (data-db-searchable=0) => bulk-load options, search filters locally
      const serverSearch = (ui.searchServer ?? (dbKey === 'item'));
      const searchable = serverSearch ? '1' : '0';

      // Default: show more than 213 items for large sources (like items)
      const defaultLimit = (dbKey === 'item') ? 1000 : 300;
      const dbLimit = String(ui.dbLimit || defaultLimit);

      const valueType = ui.valueType || field.valueType || 'string';

      const extraAttrs = `${multiple ? 'multiple' : ''} data-datasource="${escapeAttr(ds)}" data-bind-input-path="${escapeAttr(bindInputPath || '')}" data-placeholder="${escapeAttr(placeholder)}" data-label-mode="${escapeAttr(String(labelMode))}" data-db-searchable="${escapeAttr(searchable)}" data-db-limit="${escapeAttr(dbLimit)}" data-value-type="${escapeAttr(String(valueType))}"`;
      return `<select class="${classes} editor-dbselect-select" ${extraAttrs}><option value="">${escapeAttr(placeholder)}</option></select>`;
    };

    
    const renderDbSelectAdaptive = (multiple = false) => {
      const { ds } = resolveDbParams();
      const showSearch = (ui.showSearch ?? true) && !!ds;
      const searchPh = ui.searchPlaceholder || 'Поиск…';
      const searchHtml = showSearch
        ? `<input type="search" class="${inputClass} editor-dbselect-search" placeholder="${escapeAttr(searchPh)}" />`
        : '';

      // Stability feature:
      // For single-value db_select we always render a manual ID input bound to the data model.
      // The DB dropdown becomes a helper that syncs into the input. If DB is unavailable,
      // the user can still type the ID.
      const useHelperMode = !multiple && (ui.fallbackInput ?? true) && !!ds;

      const manualInputHtml = useHelperMode
        ? (() => {
            const attrs = buildInputAttributes({
              path,
              field,
              classes: `${inputClass} editor-dbselect-input`,
              includeName,
              includeDataKey,
              includeDataPath,
              extraAttrs: 'inputmode="numeric"'
            });
            const cur = (value ?? 0);
            return `<input type="number" ${attrs} value="${escapeAttr(String(cur))}" />`;
          })()
        : '';

      const selectHtml = useHelperMode
        ? renderDbSelectHelper({ bindInputPath: path, multiple: false })
        : renderDbSelectDirect(multiple);

      const controlsHtml = ds
        ? `<div class="editor-dbselect-controls">
             <button type="button" class="editor-btn editor-dbselect-more hidden">Ещё</button>
             <div class="editor-dbselect-meta"></div>
           </div>`
        : '';

      // UI: search (left) + select (right) in one row.
      // Optimistic default state="connected" when datasource exists to avoid initial double-render.
      const state = ds ? 'connected' : '';
      const rowHtml = `<div class="editor-dbselect-row">${searchHtml}${selectHtml}</div>`;
      return `<div class="editor-dbselect" ${state ? `data-db-state="${state}"` : ''}>${rowHtml}${manualInputHtml}${controlsHtml}</div>`;
    };
const renderCheckbox = () => {
      const attrs = buildInputAttributes({
        path,
        field,
        classes: options?.classes?.checkbox || '',
        includeName,
        includeDataKey,
        includeDataPath
      });
      return `<label class="${checkboxWrapperClass}"><input type="checkbox" ${value ? 'checked' : ''} ${attrs}><span>${label}</span></label>`;
    };

    const renderComposite = (fields, wrapperClass = '') => {
      const content = Object.entries(fields).map(([key, subField]) => {
        const subPath = `${path}.${key}`;
        return renderField({ path: subPath, field: subField, data, options, isNested: true });
      }).join('');
      return `<div class="${wrapperClass}">${content}</div>`;
    };

    if (field.type === 'boolean') {
      return `<div class="${fieldWrapperClass}">${renderCheckbox()}${hintHtml}</div>`;
    }

    if (field.type === 'toggle' || ui.type === 'toggle') {
      return `<div class="${fieldWrapperClass}">${renderToggle()}${hintHtml}</div>`;
    }

    if (field.type === 'vec2') {
      const vecFields = {
        x: { type: 'number', label: 'X', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } },
        y: { type: 'number', label: 'Y', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } }
      };
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}<div class="grid grid-cols-1 sm:grid-cols-2 gap-2">${renderComposite(vecFields)}</div>${hintHtml}</div>`;
    }

    if (field.type === 'item') {
      const itemDs = ui.datasource || field.datasource || null;
      const itemFields = {
        id: { type: 'db_select', label: 'Предмет', datasource: itemDs || undefined, ui: { dbKey: ui.dbKey || 'item', placeholder: ui.placeholder || '— выберите предмет —', inputLabel: 'ID', selectLabel: 'Предмет (БД)' } },
        value: { type: 'number', label: 'Value', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } }
      };
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}<div class="grid grid-cols-1 sm:grid-cols-2 gap-2">${renderComposite(itemFields)}</div>${hintHtml}</div>`;
    }

    if (field.type === 'list') {
      const items = Array.isArray(value) ? value : [];
      const itemFields = field.itemFields || {};
      const hideLabel = !!(field?.ui?.hideLabel);
      const layout = (field?.ui?.layout || 'stack').toLowerCase();
      const layoutClass = layout === 'grid' ? 'grid grid-cols-1 md:grid-cols-2 gap-3' : 'space-y-3';
      const listItems = items.map((item, index) => {
        const itemPath = `${path}.${index}`;
        const itemContent = Object.entries(itemFields).map(([key, subField]) => {
          return renderField({ path: `${itemPath}.${key}`, field: subField, data, options, isNested: true });
        }).join('');
        return `
          <div class="${listItemClass}" data-list-item="${escapeAttr(itemPath)}">
            <div class="flex items-center justify-between gap-3 mb-2">
              <div class="editor-muted-text text-xs">#${index + 1}</div>
              <button type="button" class="${listRemoveClass}" title="Удалить" data-list-action="remove" data-list-path="${escapeAttr(path)}" data-list-index="${index}">
                <i class="fa-solid fa-trash"></i>
              </button>
            </div>
            <div class="${layoutClass}">
              ${itemContent}
            </div>
          </div>`;
      }).join('');
      const defaultItem = buildDefaultItem(field);
      const defaultPayload = encodeURIComponent(JSON.stringify(defaultItem));
      return `
        <div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">
          ${hideLabel ? '' : labelHtml}
          <div class="${listWrapperClass}" data-list-container="${escapeAttr(path)}">${listItems}</div>
          <button type="button" class="${listAddClass}" data-list-action="add" data-list-path="${escapeAttr(path)}" data-list-default="${defaultPayload}">${field?.ui?.addLabel || options?.listAddLabel || 'Добавить'}</button>
          ${hintHtml}
        </div>`;
    }

    if (field.type === 'db_select' || ui.type === 'db_select') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderDbSelectAdaptive()}${hintHtml}</div>`;
    }

    if (field.type === 'db_multiselect' || ui.type === 'db_multiselect') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderDbSelectAdaptive(true)}${hintHtml}</div>`;
    }

    if (ui.type === 'select') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderSelect(false)}${hintHtml}</div>`;
    }

    if (ui.type === 'multiselect') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderSelect(true)}${hintHtml}</div>`;
    }

    if (ui.type === 'tags') {
      // TagSelect: visual chips + search, value stored in a hidden input as JSON array.
      const opts = Array.isArray(ui.options) ? ui.options : [];
      const ph = ui.searchPlaceholder || ui.placeholder || 'Добавить…';
      const allowCreate = ui.allowCreate ? '1' : '0';

      const hiddenAttrs = buildInputAttributes({
        path,
        field,
        classes: '',
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: 'type="hidden" data-value-type="json_array"'
      });

      const initial = JSON.stringify(normalizeMultiValue(value));
      return `
        <div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">
          ${labelHtml}
          <div class="editor-tags" data-tags-options="${escapeAttr(JSON.stringify(opts))}" data-tags-placeholder="${escapeAttr(ph)}" data-tags-allow-create="${allowCreate}">
            <input ${hiddenAttrs} value="${escapeAttr(initial)}">
            <div class="editor-tags-selected"></div>
            <div class="editor-tags-searchrow">
              <input type="search" class="${inputClass} editor-tags-search" placeholder="${escapeAttr(ph)}" />
            </div>
            <div class="editor-tags-options"></div>
          </div>
          ${hintHtml}
        </div>`;
    }

    if (ui.format === 'date' || field.type === 'date') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('date', value)}${hintHtml}</div>`;
    }

    if (ui.format === 'time' || field.type === 'time') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('time', value)}${hintHtml}</div>`;
    }

    if (ui.format === 'datetime' || ui.format === 'datetime-local' || field.type === 'datetime') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('datetime-local', value)}${hintHtml}</div>`;
    }

    if (ui.format === 'password' || field.type === 'password') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('password', value ?? '')}${hintHtml}</div>`;
    }

    if (ui.format === 'textarea' || ui.type === 'textarea') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderTextarea()}${hintHtml}</div>`;
    }

    if (field.type === 'number') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('number', value ?? 0)}${hintHtml}</div>`;
    }

    return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('text', value ?? '')}${hintHtml}</div>`;
  };

  const renderFields = ({ fields, data, options, sort = false }) => {
    const entries = Object.entries(fields || {});
    if (sort) entries.sort(([a], [b]) => a.localeCompare(b));
    return entries.map(([key, field]) => renderField({ path: key, field, data, options })).join('');
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.FieldRenderer = {
    parsePath,
    getValueAtPath,
    setValueAtPath,
    getInputValue,
    renderField,
    renderFields
  };
})();



/* ===== db-map.js ===== */
(() => {
  // Central DB map used by all editors.
  // Define semantic keys once and reuse everywhere.
  //
  // Each entry:
  // - source: whitelisted source name for api/db.php
  // - labelMode: how options are displayed
  //     - 'id_name' => "ID: Name"
  //     - 'name'    => "Name"

  const DB_MAP = {
    quest: { source: 'quests', labelMode: 'id_name' },
    item:  { source: 'items',  labelMode: 'id_name' },
    mob:   { source: 'bots',   labelMode: 'id_name' },
    world: { source: 'worlds', labelMode: 'id_name' },
    skill: { source: 'skills', labelMode: 'id_name' },
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DBMap = DB_MAP;
})();



/* ===== db.js ===== */
(() => {
  const DEFAULT_ENDPOINT = 'api/db.php';

  const safeJson = async (res) => {
    const text = await res.text();
    try {
      return JSON.parse(text);
    } catch {
      return { ok: false, error: 'Invalid JSON response', raw: text };
    }
  };

  const fetchJson = async (url, { method = 'GET', body = null } = {}) => {
    const res = await fetch(url, {
      method,
      headers: body ? { 'Content-Type': 'application/json' } : undefined,
      body: body ? JSON.stringify(body) : undefined,
      credentials: 'same-origin'
    });
    const json = await safeJson(res);
    if (!res.ok) {
      return { ok: false, error: json?.error || `HTTP ${res.status}`, details: json };
    }
    return json;
  };

  const cache = new Map();
  const CACHE_TTL_MS = 5 * 60 * 1000;

  const cacheGet = (key) => {
    const e = cache.get(key);
    if (!e) return null;
    if (Date.now() - e.ts > CACHE_TTL_MS) {
      cache.delete(key);
      return null;
    }
    return e.value;
  };
  const cacheSet = (key, value) => cache.set(key, { ts: Date.now(), value });

  const getSelectedValue = (el, { bindInputPath, root }) => {
    if (bindInputPath) {
      const input = root.querySelector(`[data-path="${CSS.escape(bindInputPath)}"]`);
      const v = input ? String(input.value || '') : '';
      return v === '0' ? '' : v;
    }
    // direct binding select
    const v = String(el.value || '');
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
    el.appendChild(new Option(label, val, false, false));
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

      for (const it of (items || [])) {
        const val = String(it.value);
        const baseLabel = String(it.label || '');
        const label = labelMode === 'name' ? baseLabel : `${val}: ${baseLabel}`;

        if (Array.from(el.options).some(o => o.value === val)) continue;
        const opt = new Option(label, val);
        if (!multiple && selectedVal && val === String(selectedVal)) opt.selected = true;
        el.appendChild(opt);
      }
    };

    const syncSelectToInput = () => {
      if (!bindInputPath) return;
      const input = root.querySelector(`[data-path="${CSS.escape(bindInputPath)}"]`);
      if (!input) return;
      input.value = (el.value === '' ? '0' : String(el.value));
      input.dispatchEvent(new Event('input', { bubbles: true }));
      input.dispatchEvent(new Event('change', { bubbles: true }));
    };

    const syncInputToSelect = () => {
      if (!bindInputPath) return;
      const input = root.querySelector(`[data-path="${CSS.escape(bindInputPath)}"]`);
      if (!input) return;
      const v = String(input.value || '0');
      const has = Array.from(el.options).some(o => o.value === v);
      if (has) el.value = v;
      else if (v === '0') el.value = '';
    };

    if (bindInputPath && el.dataset.dbBindInit !== '1') {
      el.dataset.dbBindInit = '1';
      el.addEventListener('change', syncSelectToInput);

      const input = root.querySelector(`[data-path="${CSS.escape(bindInputPath)}"]`);
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
      const res = await db.list(source, { search: state.q, limit: pageSize, offset: state.loaded, withTotal: true });
      if (!res?.ok) {
        el.innerHTML = '';
        el.appendChild(new Option('Ошибка загрузки', '', true, true));
        el.disabled = true;
        if (moreBtn) moreBtn.classList.add('hidden');
        if (metaEl) metaEl.textContent = '';
        return;
      }

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
      const wrapper = el.closest('.editor-dbselect');
      if (wrapper) wrapper.setAttribute('data-db-state', 'connected');

      const multiple = !!el.multiple;
      const placeholder = el.dataset.placeholder || '— выберите —';
      const labelMode = (el.dataset.labelMode || 'id_name').toLowerCase();
      const bindInputPath = el.dataset.bindInputPath || '';

      const prev = multiple
        ? Array.from(el.selectedOptions).map(o => String(o.value))
        : (bindInputPath
          ? getSelectedValue(el, { bindInputPath, root })
          : String(el.value || ''));

      el.innerHTML = '';
      if (!multiple) el.appendChild(new Option(placeholder, '', false, !prev));

      for (const it of (items || [])) {
        const val = String(it.value);
        const baseLabel = String(it.label || '');
        const label = labelMode === 'name' ? baseLabel : `${val}: ${baseLabel}`;
        const opt = new Option(label, val);
        if (multiple) opt.selected = Array.isArray(prev) && prev.includes(val);
        else opt.selected = prev && String(prev) === val;
        el.appendChild(opt);
      }

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
          const input = root.querySelector(`[data-path="${CSS.escape(bindInputPath)}"]`);
          if (!input) return;
          input.value = (el.value === '' ? '0' : String(el.value));
          input.dispatchEvent(new Event('input', { bubbles: true }));
          input.dispatchEvent(new Event('change', { bubbles: true }));
        };
        el.addEventListener('change', syncToInput);

        const input = root.querySelector(`[data-path="${CSS.escape(bindInputPath)}"]`);
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

    async testConnection() {
      return fetchJson(`${this.endpoint}?action=test`, { method: 'POST', body: {} });
    },

    async getOne(source, id) {
      const key = `one:${source}:${id}`;
      const cached = cacheGet(key);
      if (cached) return cached;
      const qs = new URLSearchParams({ action: 'get_one', source, id: String(id) }).toString();
      const res = await fetchJson(`${this.endpoint}?${qs}`);
      if (res?.ok) cacheSet(key, res);
      return res;
    },

    async list(source, { search = '', limit = 1000, offset = 0, withTotal = false } = {}) {
      const key = `list:${source}:${search}:${limit}:${offset}:${withTotal ? 1 : 0}`;
      const cached = cacheGet(key);
      if (cached) return cached;

      const params = { action: 'list', source, search, limit: String(limit), offset: String(offset) };
      if (withTotal) params.with_total = '1';
      const qs = new URLSearchParams(params).toString();

      const res = await fetchJson(`${this.endpoint}?${qs}`);
      if (res?.ok) cacheSet(key, res);
      return res;
    },

    async init(root = document) {
      const selects = Array.from(root.querySelectorAll('select[data-datasource]'));
      if (!selects.length) return;

      const bySource = new Map();
      for (const el of selects) {
        const source = el.dataset.datasource;
        if (!source) continue;
        if (!bySource.has(source)) bySource.set(source, []);
        bySource.get(source).push(el);
      }

      for (const [source, els] of bySource.entries()) {
        // show loading
        for (const el of els) {
          const current = el.value || '';
          if (!el.dataset.dbLoaded) {
            el.innerHTML = '';
            el.appendChild(new Option('Загрузка…', current, true, true));
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
            const res = await this.list(source, { limit: 5000, offset: 0 });
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
          const res = await this.list(source, { limit: PAGE, offset });
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
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DB = DB;
})();



/* ===== db-crud.js ===== */
(() => {
  // Generic CRUD client for DB-backed editors.
  // Requires api/db-crud.php on server.
  const API = 'api/db-crud.php';

  const jsonFetch = async (url, options = {}) => {
    const res = await fetch(url, {
      credentials: 'same-origin',
      headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
      ...options
    });
    const text = await res.text();
    let payload = null;
    try { payload = text ? JSON.parse(text) : null; } catch { payload = { ok: false, error: 'Invalid JSON response', raw: text }; }
    if (!res.ok) throw new Error(payload?.error || `HTTP ${res.status}`);
    if (payload && payload.ok === false) throw new Error(payload.error || 'Request failed');
    return payload;
  };

  const qs = (obj) => Object.entries(obj)
    .filter(([, v]) => v !== undefined && v !== null)
    .map(([k, v]) => `${encodeURIComponent(k)}=${encodeURIComponent(String(v))}`)
    .join('&');

  const DBCrud = {
    async list(resource, { search = '', limit = 100, offset = 0 } = {}) {
      return jsonFetch(`${API}?${qs({ action: 'list', resource, search, limit, offset })}`);
    },
    async get(resource, id) {
      return jsonFetch(`${API}?${qs({ action: 'get', resource, id })}`);
    },
    async create(resource, data) {
      return jsonFetch(`${API}?${qs({ action: 'create', resource })}`, { method: 'POST', body: JSON.stringify({ data }) });
    },
    async update(resource, id, data) {
      return jsonFetch(`${API}?${qs({ action: 'update', resource, id })}`, { method: 'POST', body: JSON.stringify({ data }) });
    },
    async remove(resource, id) {
      return jsonFetch(`${API}?${qs({ action: 'delete', resource, id })}`, { method: 'POST', body: JSON.stringify({}) });
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DBCrud = DBCrud;
})();



/* ===== ui-manager.js ===== */
(() => {
  const parseValidateSpec = (raw) => {
    if (!raw) return null;
    try {
      return JSON.parse(raw);
    } catch {
      return null;
    }
  };

  const setHint = (hintEl, message) => {
    if (!hintEl) return;
    if (!message) {
      hintEl.textContent = '';
      hintEl.classList.add('hidden');
      return;
    }
    hintEl.textContent = message;
    hintEl.classList.remove('hidden');
  };

  const validateValue = ({ value, spec, inputType }) => {
    if (!spec) return { ok: true, message: '' };
    const message = spec.message || 'Некорректное значение';

    // Required
    if (spec.required) {
      const empty = value == null || value === '' || (Array.isArray(value) && value.length === 0);
      if (empty) return { ok: false, message: spec.requiredMessage || 'Обязательное поле' };
    }

    // Number bounds
    if (inputType === 'number' || typeof value === 'number') {
      const num = typeof value === 'number' ? value : (value === '' ? NaN : Number(value));
      if (Number.isFinite(num)) {
        if (spec.min !== undefined && num < spec.min) return { ok: false, message: spec.minMessage || `Минимум: ${spec.min}` };
        if (spec.max !== undefined && num > spec.max) return { ok: false, message: spec.maxMessage || `Максимум: ${spec.max}` };
      }
    }

    // Pattern
    if (spec.pattern) {
      try {
        const re = new RegExp(spec.pattern);
        if (!re.test(String(value ?? ''))) return { ok: false, message };
      } catch {
        // ignore broken regex
      }
    }

    return { ok: true, message: '' };
  };

  const UIManager = {
    async init(root = document) {
      // 0) TagSelect widgets (chips + search) - no DB required
      // They store the actual value in a hidden input with data-value-type="json_array".
      root.querySelectorAll('.editor-tags').forEach((widget) => {
        if (widget.dataset.tagsInited === '1') return;
        widget.dataset.tagsInited = '1';

        const hidden = widget.querySelector('input[type="hidden"][data-path]');
        const selectedWrap = widget.querySelector('.editor-tags-selected');
        const optionsWrap = widget.querySelector('.editor-tags-options');
        const search = widget.querySelector('.editor-tags-search');

        const safeJson = (raw, fallback) => {
          if (!raw) return fallback;
          try { return JSON.parse(raw); } catch { return fallback; }
        };

        const normalizeOptions = (raw) => {
          const arr = Array.isArray(raw) ? raw : [];
          return arr.map((o) => {
            if (o && typeof o === 'object') {
              return { value: String(o.value ?? o.label ?? ''), label: String(o.label ?? o.value ?? '') };
            }
            return { value: String(o), label: String(o) };
          }).filter(o => o.value);
        };

        const allOptions = normalizeOptions(safeJson(widget.dataset.tagsOptions, []));
        const allowCreate = widget.dataset.tagsAllowCreate === '1';

        const readValue = () => {
          const v = window.EditorCore?.FieldRenderer?.getInputValue ? window.EditorCore.FieldRenderer.getInputValue(hidden) : [];
          return Array.isArray(v) ? v.map(x => String(x)).filter(Boolean) : [];
        };

        const writeValue = (arr) => {
          if (!hidden) return;
          const uniq = Array.from(new Set((Array.isArray(arr) ? arr : []).map(x => String(x)).filter(Boolean)));
          hidden.value = JSON.stringify(uniq);
          hidden.dispatchEvent(new Event('input', { bubbles: true }));
          hidden.dispatchEvent(new Event('change', { bubbles: true }));
        };

        const escapeHtml = (s) => String(s)
          .replace(/&/g, '&amp;')
          .replace(/</g, '&lt;')
          .replace(/>/g, '&gt;')
          .replace(/"/g, '&quot;')
          .replace(/'/g, '&#39;');

        const render = () => {
          if (!hidden || !selectedWrap || !optionsWrap) return;
          const q = String(search?.value || '').trim().toLowerCase();
          const selected = readValue();
          const selectedSet = new Set(selected.map(x => x.toLowerCase()));

          if (!selected.length) {
            selectedWrap.innerHTML = `<div class="editor-tags-empty">Ничего не выбрано</div>`;
          } else {
            selectedWrap.innerHTML = selected.map((v) => {
              const opt = allOptions.find(o => o.value.toLowerCase() === v.toLowerCase());
              const label = opt ? opt.label : v;
              return `
                <button type="button" class="editor-tag" data-tag-remove="${escapeHtml(v)}" title="Убрать">
                  <span class="editor-tag-label">${escapeHtml(label)}</span>
                  <span class="editor-tag-x">×</span>
                </button>`;
            }).join('');
          }

          const filtered = allOptions.filter((o) => {
            if (selectedSet.has(o.value.toLowerCase())) return false;
            if (!q) return true;
            return o.label.toLowerCase().includes(q) || o.value.toLowerCase().includes(q);
          });

          let extraCreate = '';
          if (allowCreate && q) {
            const exists = allOptions.some(o => o.value.toLowerCase() === q || o.label.toLowerCase() === q);
            if (!exists) {
              extraCreate = `
                <button type="button" class="editor-tag-option" data-tag-add="${escapeHtml(search.value.trim())}">
                  + Добавить: <span class="editor-tag-option-strong">${escapeHtml(search.value.trim())}</span>
                </button>`;
            }
          }

          optionsWrap.innerHTML = `
            <div class="editor-tags-options-inner">
              ${extraCreate}
              ${filtered.map((o) => {
                return `
                  <button type="button" class="editor-tag-option" data-tag-add="${escapeHtml(o.value)}">
                    ${escapeHtml(o.label)}
                  </button>`;
              }).join('')}
              ${(!extraCreate && !filtered.length) ? `<div class="editor-tags-empty">Нет вариантов</div>` : ''}
            </div>`;

          selectedWrap.querySelectorAll('[data-tag-remove]').forEach((btn) => {
            btn.addEventListener('click', () => {
              const v = btn.getAttribute('data-tag-remove');
              writeValue(readValue().filter(x => x.toLowerCase() !== String(v).toLowerCase()));
              render();
            });
          });
          optionsWrap.querySelectorAll('[data-tag-add]').forEach((btn) => {
            btn.addEventListener('click', () => {
              const v = String(btn.getAttribute('data-tag-add') || '').trim();
              if (!v) return;
              writeValue(readValue().concat([v]));
              if (search) search.value = '';
              render();
            });
          });
        };

        if (search) {
          search.addEventListener('input', render);
          search.addEventListener('change', render);
        }

        render();
      });

      // 1) Dynamic DB selects
      if (window.EditorCore?.DB?.init) {
        await window.EditorCore.DB.init(root);
      }

      // 1.1) Adaptive DB selects: toggle helper dropdown vs manual input
      root.querySelectorAll('.editor-dbselect').forEach((wrapper) => {
        const sel = wrapper.querySelector('select[data-datasource]');
        if (!sel) {
          wrapper.dataset.dbState = 'disconnected';
          return;
        }
        const failed = sel.dataset.dbFailed === '1';
        if (failed) {
          wrapper.dataset.dbState = 'disconnected';
          return;
        }

        // Searchable selects load incrementally; treat them as "connected" once DB.init has run.
        if (sel.dataset.dbSearchable === '1') {
          wrapper.dataset.dbState = 'connected';
          return;
        }

        // Bulk selects set dbLoaded. As an extra guard, consider it loaded if it has options.
        const loaded = sel.dataset.dbLoaded === '1' || (sel.options && sel.options.length > (sel.multiple ? 0 : 1));
        wrapper.dataset.dbState = loaded ? 'connected' : 'disconnected';
      });

      // 2) Toggle state sync (for CSS styling)
      root.querySelectorAll('.editor-toggle input[type=checkbox]').forEach((cb) => {
        const label = cb.closest('.editor-toggle');
        const sync = () => { if (label) label.dataset.checked = cb.checked ? '1' : '0'; };
        cb.addEventListener('change', sync);
        sync();
      });

      // 2) Validation hints
      const inputs = Array.from(root.querySelectorAll('[data-validate]'));
      for (const el of inputs) {
        const spec = parseValidateSpec(el.getAttribute('data-validate'));
        if (!spec) continue;
        const path = el.getAttribute('data-path') || el.getAttribute('name') || el.getAttribute('data-key');
        const hint = path ? root.querySelector(`[data-hint-for="${CSS.escape(path)}"]`) : null;

        const run = () => {
          const value = window.EditorCore?.FieldRenderer?.getInputValue
            ? window.EditorCore.FieldRenderer.getInputValue(el)
            : (el.type === 'checkbox' ? el.checked : el.value);

          const { ok, message } = validateValue({ value, spec, inputType: el.type });
          el.classList.toggle('editor-invalid', !ok);
          setHint(hint, ok ? '' : message);
        };

        el.addEventListener('input', run);
        el.addEventListener('change', run);
        run();
      }
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.UIManager = UIManager;
})();



/* ===== defaults.js ===== */
(() => {
  // Central place for editor-wide visual / rendering defaults.
  // Editors should read these instead of redefining local constants.

  // Unified classes shared by all editors. Keep Tailwind "form-input" so
  // Tailwind Forms plugin normalizes controls, while visual theme comes from editor-theme.css.
  const INPUT_CLASS = 'editor-input form-input';

  const FIELD_RENDER_OPTIONS = {
    scenario: {
      classes: {
        input: INPUT_CLASS,
        textarea: `${INPUT_CLASS} editor-textarea`,
        multiselect: INPUT_CLASS,
        label: 'editor-label editor-label-block text-sm',
        nestedLabel: 'editor-nested-label',
        fieldWrapper: 'editor-field',
        nestedFieldWrapper: 'editor-field-nested',
        checkboxWrapper: 'flex items-center space-x-3 cursor-pointer',
        listWrapper: 'space-y-2',
        listItem: 'editor-list-item',
        listAdd: 'editor-btn editor-btn-primary text-sm',
        listRemove: 'editor-icon-btn editor-icon-danger',
        checkbox: 'h-4 w-4 rounded border-slate-300/40 bg-slate-900/40 text-blue-500',
      },
      includeName: true,
      includeDataPath: true,
      includeDataKey: false,
    },

    event: {
      classes: {
        input: INPUT_CLASS,
        textarea: `${INPUT_CLASS} editor-textarea`,
        multiselect: INPUT_CLASS,
        label: 'editor-label editor-label-block text-sm',
        nestedLabel: 'editor-nested-label',
        fieldWrapper: 'editor-field',
        nestedFieldWrapper: 'editor-field-nested',
        checkboxWrapper: 'flex items-center space-x-3 cursor-pointer',
        listWrapper: 'space-y-2',
        listItem: 'editor-list-item',
        listAdd: 'editor-btn editor-btn-primary text-sm',
        listRemove: 'editor-icon-btn editor-icon-danger',
        checkbox: '',
      },
      includeName: false,
      includeDataPath: true,
      includeDataKey: true,
      listAddLabel: 'Добавить',
    }
  };

  const getFieldRenderOptions = (mode = 'scenario') => {
    const base = FIELD_RENDER_OPTIONS[mode] || FIELD_RENDER_OPTIONS.scenario;
    // return a shallow clone so editors can mutate safely
    return {
      ...base,
      classes: { ...(base.classes || {}) },
    };
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.defaults = {
    getFieldRenderOptions,
    FIELD_RENDER_OPTIONS,
  };
})();



/* ===== form-runtime.js ===== */
(() => {
  // Minimal schema-driven form runtime.
  // Renders fields via FieldRenderer and provides 2-way binding + list add/remove.
  // This is intended to simplify creation of new editors.

  const parsePath = (path) => String(path).split('.').map((part) => (/^\d+$/.test(part) ? Number(part) : part));

  const getAt = (obj, path) => {
    if (!path) return undefined;
    return parsePath(path).reduce((cur, k) => (cur == null ? undefined : cur[k]), obj);
  };

  const setAt = (obj, path, value) => {
    if (!path) return;
    const keys = parsePath(path);
    let cur = obj;
    for (let i = 0; i < keys.length; i++) {
      const k = keys[i];
      if (i === keys.length - 1) {
        cur[k] = value;
        return;
      }
      const nextKey = keys[i + 1];
      if (cur[k] == null) cur[k] = typeof nextKey === 'number' ? [] : {};
      cur = cur[k];
    }
  };

  const readInputValue = (el) => {
    if (!el) return undefined;
    // Prefer the shared FieldRenderer logic (handles db_select, multiselect, numeric ids, unix_seconds, etc.)
    const fr = window.EditorCore?.FieldRenderer;
    if (fr?.getInputValue) return fr.getInputValue(el);
    // Fallback (should rarely happen)
    if (el.type === 'checkbox') return !!el.checked;
    if (el.type === 'number') return el.value === '' ? 0 : Number(el.value);
    return el.value;
  };

  const FormRuntime = {
    mount(root, { fields, data, fieldOptions, onChange } = {}) {
      const state = { data: data || {} };

      const render = () => {
        if (!window.EditorCore?.FieldRenderer) {
          root.innerHTML = '<div class="editor-muted-text">FieldRenderer не загружен</div>';
          return;
        }
        const html = window.EditorCore.FieldRenderer.renderFields({
          fields,
          data: state.data,
          options: fieldOptions || window.EditorCore?.defaults?.fieldOptions
        });
        root.innerHTML = html;

        // Initialize shared UI behaviors (validation, adaptive db_select, etc.)
        // UIManager also triggers DB.init internally.
        window.EditorCore?.UIManager?.init(root);

        // Input bindings
        root.querySelectorAll('[data-path]').forEach((el) => {
          const handler = () => {
            const path = el.getAttribute('data-path');
            const val = readInputValue(el);
            setAt(state.data, path, val);

            // Sync adaptive db_select helper select -> input is handled by DB.init.
            // But we also keep toggle label state in sync.
            if (el.type === 'checkbox' && el.closest('.editor-toggle')) {
              el.closest('.editor-toggle')?.setAttribute('data-checked', el.checked ? '1' : '0');
            }
            if (typeof onChange === 'function') onChange(state.data, { path, value: val });
          };
          el.addEventListener('input', handler);
          el.addEventListener('change', handler);
        });

        // List actions
        root.querySelectorAll('[data-list-action]').forEach((btn) => {
          btn.addEventListener('click', () => {
            const action = btn.getAttribute('data-list-action');
            const listPath = btn.getAttribute('data-list-path');
            const arr = Array.isArray(getAt(state.data, listPath)) ? getAt(state.data, listPath) : [];
            if (action === 'add') {
              const payload = btn.getAttribute('data-list-default') || '';
              let def = {};
              try { def = JSON.parse(decodeURIComponent(payload)); } catch { def = {}; }
              arr.push(def);
              setAt(state.data, listPath, arr);
              if (typeof onChange === 'function') onChange(state.data, { path: listPath, value: arr });
              render();
            }
            if (action === 'remove') {
              const idx = Number(btn.getAttribute('data-list-index'));
              if (Number.isFinite(idx) && idx >= 0 && idx < arr.length) {
                arr.splice(idx, 1);
                setAt(state.data, listPath, arr);
                if (typeof onChange === 'function') onChange(state.data, { path: listPath, value: arr });
                render();
              }
            }
          });
        });
      };

      const api = {
        render,
        getData: () => state.data,
        setData: (next) => {
          state.data = next || {};
          render();
        }
      };

      render();
      return api;
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.FormRuntime = FormRuntime;
})();



/* ===== bootstrap.js ===== */
(() => {
  const assertReady = () => {
    if (!window.EditorCore) throw new Error('EditorCore is missing.');
    if (!window.EditorCore.UI) throw new Error('EditorCore.UI is missing. Load ui.js first.');
    if (!window.EditorCore.defaults) throw new Error('EditorCore.defaults is missing. Load defaults.js.');
  };

  /**
   * One-liner setup for any editor page.
   * - mounts shared modals / toasts
   * - provides shared defaults (FieldRenderer options)
   */
  const bootstrapEditor = ({ mode = 'scenario' } = {}) => {
    assertReady();

    if (mode === 'event') {
      window.EditorCore.UI.mountEventEditorUI();
    } else {
      window.EditorCore.UI.mountScenarioEditorUI();
    }

    // Core UI init (DB-backed selects, validation hints, etc.)
    if (window.EditorCore.UIManager?.init) {
      // fire-and-forget; init is idempotent
      Promise.resolve().then(() => window.EditorCore.UIManager.init(document));
    }

    return {
      fieldRenderOptions: window.EditorCore.defaults.getFieldRenderOptions(mode),
    };
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.bootstrapEditor = bootstrapEditor;
})();

