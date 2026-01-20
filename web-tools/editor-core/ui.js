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
              <button data-action="cancel-add" class="bg-gray-600 hover:bg-gray-700 text-white font-bold py-2 px-4 btn">Отмена</button>
              <button data-action="confirm-add" style="background: linear-gradient(45deg, var(--editor-primary), var(--editor-secondary)); color: white;" class="text-white font-bold py-2 px-4 btn">Добавить</button>
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
              <button id="copy-json-btn" style="background: linear-gradient(45deg, #22c55e, #4ade80); color: white;" class="py-2 px-4 btn"><i class="fas fa-copy mr-2"></i>Копировать</button>
              <button data-action="close-modal" class="bg-gray-600 hover:bg-gray-700 text-white font-bold py-2 px-4 btn">Закрыть</button>
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
        <div id="undo-toast" class="fixed bottom-5 right-5 z-50 p-4 rounded-lg shadow-lg flex items-center gap-4" style="background-color: var(--editor-surface); border: 1px solid var(--editor-primary);">
          <span id="undo-message"></span>
          <button id="undo-btn" class="font-bold uppercase" style="color: var(--editor-accent);">Отменить</button>
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
              <button id="modal-cancel" class="bg-slate-200 text-slate-700 font-semibold py-2 px-4 rounded-md hover:bg-slate-300 transition-colors">Отмена</button>
              <button id="modal-save" class="bg-green-500 text-white font-semibold py-2 px-4 rounded-md hover:bg-green-600 transition-colors ml-2">Сохранить</button>
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
              <button id="delete-group-cancel-btn" class="bg-slate-200 text-slate-700 font-semibold py-2 px-4 rounded-md hover:bg-slate-300 transition-colors">Отмена</button>
              <button id="delete-group-only-btn" class="bg-yellow-500 text-white font-semibold py-2 px-4 rounded-md hover:bg-yellow-600 transition-colors">Только группу</button>
              <button id="delete-group-and-steps-btn" class="bg-red-600 text-white font-semibold py-2 px-4 rounded-md hover:bg-red-700 transition-colors">Группу и шаги</button>
            </div>
          </div>
        </div>
      `
    });
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.UI = {
    mountToastContainer,
    mountModal,
    mountListContainer,
    renderEmptyList,
    applyEmptyStateClass,
    mountEventEditorUI,
    mountScenarioEditorUI
  };
})();
