<?php $embedded = isset($_GET['embedded']); ?>
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Редактор событий</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <style>
        :root {
            --bg-dark: #0f172a;
            --bg-medium: #111827;
            --bg-light: #1e293b;
            --bg-strong: #0b1220;
            --primary: #2563eb;
            --secondary: #38bdf8;
            --accent: #38bdf8;
            --danger: #ef4444;
            --warning: #f59e0b;
            --text-light: #e2e8f0;
            --text-dark: #94a3b8;
            --border: #334155;
        }
        body { font-family: ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; background-color: var(--bg-dark); color: var(--text-light); overflow: hidden; }
        #app { background: var(--bg-strong); }
        aside { background-color: var(--bg-medium); border-right-color: var(--bg-light); }
        .sidebar-item { border-left: 3px solid transparent; transition: background 0.2s ease, color 0.2s ease, border-color 0.2s ease; }
        .sidebar-item:hover { background-color: rgba(56, 189, 248, 0.12); border-left-color: var(--accent); }
        .sidebar-item.selected { background: rgba(56, 189, 248, 0.18); border-left-color: var(--accent); color: var(--text-light); }
        .sidebar-item.selected .text-gray-400 { color: var(--text-light); }
        .group-header { background-color: var(--bg-dark); border-top: 1px solid var(--bg-light); border-bottom: 1px solid var(--bg-light); margin-top: 8px; margin-bottom: 4px; }
        .form-input { background-color: var(--bg-strong); border-color: var(--bg-light); color: var(--text-light); border-radius: 8px; transition: border-color 0.2s ease, background 0.2s ease; }
        .form-input:focus { background-color: var(--bg-medium); border-color: var(--secondary); outline: none; box-shadow: 0 0 0 2px rgba(56, 189, 248, 0.2); }
        .btn { border-radius: 8px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; border: 1px solid var(--border); background: #1f2937; color: var(--text-light); transition: background 0.2s ease, transform 0.2s ease; }
        .btn:hover { transform: translateY(-1px); background: #334155; }
        .btn:active { transform: translateY(0px) scale(0.99); }
        #add-step-btn { background: linear-gradient(90deg, var(--primary), var(--secondary)); border: none; color: white;}
        #import-json-btn { background: #1f2937; }
        #view-json-btn { background: #1f2937; }
        #delete-all-btn { background: #1f2937; }
        .modal-backdrop { background-color: rgba(15, 23, 42, 0.8); }
        .modal-content { background: var(--bg-medium); border: 1px solid var(--bg-light); box-shadow: 0 20px 60px rgba(15, 23, 42, 0.45); border-radius: 12px; }
        #undo-toast { transition: transform 0.5s ease, opacity 0.5s ease; transform: translateY(200%); opacity: 0; }
        #undo-toast.show { transform: translateY(0); opacity: 1; }
        ::-webkit-scrollbar { width: 8px; }
        ::-webkit-scrollbar-track { background: var(--bg-dark); }
        ::-webkit-scrollbar-thumb { background: #334155; border-radius: 4px; }
        .drag-handle { cursor: move; }
        .dragging { opacity: 0.6; background: rgba(56, 189, 248, 0.2); }
        .drag-over { border-top: 2px dashed var(--accent); }
        body.embedded { overflow: auto; }
        body.embedded #app { min-height: 100vh; height: auto; background: var(--bg-strong); }
        body.embedded aside { width: 320px; max-width: 320px; }
        body.embedded #editor-panel { background: var(--bg-dark); border-radius: 16px; margin: 16px; }
        body.embedded #step-list { padding-bottom: 16px; }
    </style>
</head>
<body class="antialiased<?php echo $embedded ? ' embedded' : ''; ?>">
    <div id="app" class="flex h-screen overflow-hidden">
        <aside class="w-1/3 max-w-sm flex flex-col border-r">
            <div class="p-4 border-b border-gray-700 text-center">
                <h1 class="text-2xl font-bold text-white tracking-wider" style="text-shadow: 0 0 5px var(--accent);">Редактор</h1>
                <div id="scenario-type-container" class="mt-2"></div>
            </div>
            <div id="step-list" class="flex-grow overflow-y-auto p-2"></div>
            <div id="controls" class="p-4 border-t border-gray-700 space-y-3">
                 <button id="add-step-btn" class="w-full py-3 px-4 btn flex items-center justify-center"><i class="fas fa-plus mr-2"></i> Добавить шаг</button>
                <div class="grid grid-cols-2 gap-3">
                    <button id="import-json-btn" class="w-full py-3 px-4 btn flex items-center justify-center"><i class="fas fa-upload mr-2"></i> Импорт</button>
                    <button id="view-json-btn" class="w-full py-3 px-4 btn flex items-center justify-center"><i class="fas fa-eye mr-2"></i> Просмотр JSON</button>
                </div>
                <button id="delete-all-btn" class="w-full py-3 px-4 btn flex items-center justify-center"><i class="fas fa-bomb mr-2"></i> Удалить все</button>
                 <input type="file" id="import-file-input" class="hidden" accept=".json">
            </div>
        </aside>

        <main id="editor-panel" class="w-2/3 flex-grow p-6 overflow-y-auto">
             <div class="flex flex-col items-center justify-center h-full text-center text-gray-500 opacity-50">
                <i class="fas fa-cat fa-5x mb-4 animate-bounce"></i>
                <h2 class="text-3xl font-semibold">Выберите шаг для редактирования</h2>
                <p>...или создайте новый, чтобы начать приключение!</p>
            </div>
        </main>
    </div>

    <!-- Modals & Toasts -->
    <div id="add-step-modal" class="fixed inset-0 z-50 items-center justify-center hidden modal-backdrop">
        <div class="modal-content p-6 w-full max-w-md">
            <h3 class="text-xl font-bold mb-4">Выберите тип элемента</h3>
            <select id="action-select" class="w-full p-2 rounded-md form-input mb-4"></select>
            <div class="flex justify-end space-x-2">
                <button data-action="cancel-add" class="bg-gray-600 hover:bg-gray-700 text-white font-bold py-2 px-4 btn">Отмена</button>
                <button data-action="confirm-add" style="background: linear-gradient(45deg, var(--primary), var(--secondary)); color: white;" class="text-white font-bold py-2 px-4 btn">Добавить</button>
            </div>
        </div>
    </div>
    
    <div id="export-modal" class="fixed inset-0 z-50 items-center justify-center hidden modal-backdrop">
        <div class="modal-content p-6 w-full max-w-2xl flex flex-col" style="height: 80vh;">
            <h3 class="text-xl font-bold mb-4">Экспортированный JSON</h3>
            <textarea id="json-output" readonly class="w-full flex-grow p-3 rounded-md form-input font-mono text-sm resize-none"></textarea>
             <div class="flex justify-end space-x-2 mt-4">
                <button id="copy-json-btn" style="background: linear-gradient(45deg, #22c55e, #4ade80); color: white;" class="py-2 px-4 btn"><i class="fas fa-copy mr-2"></i>Копировать</button>
                <button data-action="close-modal" class="bg-gray-600 hover:bg-gray-700 text-white font-bold py-2 px-4 btn">Закрыть</button>
            </div>
        </div>
    </div>

    <div id="confirm-modal" class="fixed inset-0 z-50 items-center justify-center hidden modal-backdrop">
        <div class="modal-content p-6 w-full max-w-lg">
            <h3 id="confirm-title" class="text-xl font-bold mb-4"></h3>
            <p id="confirm-message" class="mb-6"></p>
            <div id="confirm-buttons" class="flex justify-end space-x-3"></div>
        </div>
    </div>
    
    <div id="undo-toast" class="fixed bottom-5 right-5 z-50 p-4 rounded-lg shadow-lg flex items-center gap-4" style="background-color: var(--bg-medium); border: 1px solid var(--primary);">
        <span id="undo-message"></span>
        <button id="undo-btn" class="font-bold uppercase" style="color: var(--accent);">Отменить</button>
    </div>

    <script>
    const App = (() => {
        const DOM = {
            stepList: document.getElementById('step-list'),
            editorPanel: document.getElementById('editor-panel'),
            actionSelect: document.getElementById('action-select'),
            addStepModal: document.getElementById('add-step-modal'),
            exportModal: document.getElementById('export-modal'),
            jsonOutput: document.getElementById('json-output'),
            importFileInput: document.getElementById('import-file-input'),
            undoToast: document.getElementById('undo-toast'),
            undoMessage: document.getElementById('undo-message'),
            scenarioTypeContainer: document.getElementById('scenario-type-container'),
            confirmModal: document.getElementById('confirm-modal'),
            confirmTitle: document.getElementById('confirm-title'),
            confirmMessage: document.getElementById('confirm-message'),
            confirmButtons: document.getElementById('confirm-buttons'),
        };

        const STATE = {
            scenarioSteps: [],
            selectedStepIndex: -1,
            draggedIndex: null,
            undoState: {},
            undoTimeout: null,
            selectedScenarioEvent: 'general',
        };
        
        const ACTION_DEFS = {"group_header":{"name":"Заголовок группы","icon":"fa-layer-group","params":{"name":"text"}},"message":{"name":"Сообщение","icon":"fa-comment","params":{"delay":"number","chat":"text","broadcast":"text","full":"text"}},"movement_task":{"name":"Задача на движение","icon":"fa-person-running","params":{"delay":"number","position":"vec2","target_lock_text":"text","target_look":"boolean","chat":"text","broadcast":"text","full":"text"}},"check_has_item":{"name":"Проверить предмет","icon":"fa-box","params":{"item_id":"number","required":"number","remove":"boolean","show_progress":"boolean"}},"reset_quest":{"name":"Сбросить квест","icon":"fa-undo","params":{"quest_id":"number"}},"accept_quest":{"name":"Принять квест","icon":"fa-check-double","params":{"quest_id":"number"}},"new_door":{"name":"Создать дверь","icon":"fa-door-closed","params":{"key":"door_key","follow":"boolean","position":"vec2"}},"remove_door":{"name":"Удалить дверь","icon":"fa-door-open","params":{"key":"door_key","follow":"boolean"}},"pick_item_task":{"name":"Задача на подбор","icon":"fa-hand-sparkles","params":{"position":"vec2","item":"item","chat":"text","broadcast":"text","full":"text"}},"emote":{"name":"Эмоция","icon":"fa-smile","params":{"emote_type":"number","emoticon_type":"number"}},"teleport":{"name":"Телепорт","icon":"fa-plane-departure","params":{"position":"vec2","world_id":"number"}},"use_chat_task":{"name":"Задача на чат","icon":"fa-keyboard","params":{"chat":"text"}},"fix_cam":{"name":"Фиксировать камеру","icon":"fa-camera","params":{"delay":"number","position":"vec2"}},"freeze_movements":{"name":"Заморозить движение","icon":"fa-snowflake","params":{"state":"boolean"}},"check_quest_accepted":{"name":"Проверка: квест принят","icon":"fa-question-circle","params":{"quest_id":"number"}},"check_quest_finished":{"name":"Проверка: квест завершен","icon":"fa-flag-checkered","params":{"quest_id":"number"}},"check_quest_step_finished":{"name":"Проверка: шаг квеста завершен","icon":"fa-list-check","params":{"quest_id":"number","step":"number"}},"shootmarkers":{"name":"Стрельба по маркерам","icon":"fa-crosshairs","params":{"markers":"markers"}}};
        const SCENARIO_EVENTS = {"general":"Стандартный","on_recieve_objectives":"При получении задач","on_complete_objectives":"При завершении задач","on_end":"При окончании шага","on_equip":"При экипировке предмета","on_got":"При получении предмета","on_lost":"При потере предмета","on_unequip":"При снятии предмета"};
        const INPUT_CLASS = "w-full p-2 rounded-md form-input";

        const render = () => {
            renderScenarioTypeSelector();
            renderStepList();
            renderEditorPanel();
        };

        const renderScenarioTypeSelector = () => {
            const optionsHtml = Object.entries(SCENARIO_EVENTS).map(([key, name]) => 
                `<div class="scenario-event-option p-2 rounded-md hover:bg-light cursor-pointer flex items-center" data-event-key="${key}"><span>${name}</span></div>`
            ).join('');
            DOM.scenarioTypeContainer.innerHTML = `
                <div class="relative scenario-type-dropdown">
                    <button class="custom-select-button flex items-center justify-center w-full text-lg font-semibold text-white cursor-pointer" style="color: var(--accent);">
                        <i class="fas fa-tag mr-2"></i>
                        <span>${SCENARIO_EVENTS[STATE.selectedScenarioEvent]}</span>
                        <i class="fas fa-chevron-down ml-2 text-sm opacity-70"></i>
                    </button>
                    <div class="custom-select-options hidden absolute top-full left-0 mt-2 w-full max-h-60 overflow-y-auto rounded-lg z-20 p-2" style="background-color: var(--bg-medium); border: 1px solid var(--bg-light);">${optionsHtml}</div>
                </div>`;
        };

        const renderStepList = () => {
            if (STATE.scenarioSteps.length === 0) {
                DOM.stepList.innerHTML = `<div class="p-4 text-center text-gray-400 opacity-50">Нет шагов... пока!</div>`;
                return;
            }
            let groupSubIndex = 1;
            DOM.stepList.innerHTML = STATE.scenarioSteps.map((step, index) => {
                const def = ACTION_DEFS[step.action] || { name: step.action, icon: 'fa-question' };
                if (step.action === 'group_header') {
                    groupSubIndex = 1;
                    const isCollapsed = !!step.collapsed;
                    return `
                        <div class="group-header sidebar-item flex items-center justify-between p-3 rounded-lg ${index === STATE.selectedStepIndex ? 'selected' : ''}" data-index="${index}" draggable="true">
                            <div class="flex items-center min-w-0">
                                <i class="fas fa-grip-vertical drag-handle text-gray-400 mr-3"></i>
                                <button class="group-toggle-btn text-accent mr-2" data-index="${index}"><i class="fas ${isCollapsed ? 'fa-chevron-right' : 'fa-chevron-down'}"></i></button>
                                <span class="font-bold truncate">${step.name || 'Новая группа'}</span>
                            </div>
                            <button class="delete-step-btn text-gray-400 hover:text-secondary" data-index="${index}"><i class="fas fa-trash-alt"></i></button>
                        </div>`;
                } else {
                    let parentCollapsed = false;
                    let inGroup = false;
                    for (let i = index - 1; i >= 0; i--) {
                        if (STATE.scenarioSteps[i].action === 'group_header') {
                            inGroup = true;
                            parentCollapsed = !!STATE.scenarioSteps[i].collapsed;
                            break;
                        }
                    }
                    return `
                        <div class="sidebar-item flex items-center justify-between p-3 rounded-lg mb-2 ${inGroup ? 'ml-6' : ''} ${parentCollapsed ? 'hidden' : ''} ${index === STATE.selectedStepIndex ? 'selected' : 'hover:bg-opacity-5'}" data-index="${index}" draggable="true">
                            <div class="flex items-center min-w-0">
                                <i class="fas fa-grip-vertical drag-handle text-gray-400 mr-3"></i>
                                <i class="fas ${def.icon} w-6 text-center text-gray-400"></i>
                                <span class="ml-3 font-medium truncate">${groupSubIndex++}. ${def.name}</span>
                            </div>
                            <button class="delete-step-btn text-gray-400 hover:text-secondary" data-index="${index}"><i class="fas fa-trash-alt"></i></button>
                        </div>`;
                }
            }).join('');
        };

        const renderEditorPanel = () => {
            if (STATE.selectedStepIndex === -1) {
                DOM.editorPanel.innerHTML = `<div class="flex flex-col items-center justify-center h-full text-center text-gray-500 opacity-50"><i class="fas fa-cat fa-5x mb-4 animate-bounce"></i><h2 class="text-3xl font-semibold">Выберите шаг для редактирования</h2><p>...или создайте новый, чтобы начать приключение!</p></div>`;
                return;
            }
            const step = STATE.scenarioSteps[STATE.selectedStepIndex];
            const def = ACTION_DEFS[step.action];
            const actionOptions = Object.entries(ACTION_DEFS).sort(([,a],[,b]) => a.name.localeCompare(b.name)).map(([key, value]) => `
                <div class="custom-select-option p-2 rounded-md hover:bg-light cursor-pointer flex items-center" data-action-key="${key}"><i class="fas ${value.icon} w-6 text-center mr-2 text-accent"></i><span>${value.name}</span></div>`
            ).join('');
            const fieldsHtml = Object.entries(def.params).sort(([keyA], [keyB]) => keyA.localeCompare(keyB)).map(([key, type]) => createFormField(key, type, step)).join('');

            DOM.editorPanel.innerHTML = `
                <div class="space-y-6">
                    <div class="flex items-center justify-between">
                        <div class="relative action-type-dropdown">
                            <button class="custom-select-button flex items-center text-3xl font-bold text-white cursor-pointer" style="text-shadow: 0 0 3px var(--accent);">
                                <i class="fas ${def.icon} mr-3"></i><span>${def.name}</span><i class="fas fa-chevron-down ml-3 text-xl opacity-70"></i>
                            </button>
                            <div class="custom-select-options hidden absolute top-full left-0 mt-2 w-72 max-h-80 overflow-y-auto rounded-lg z-10 p-2" style="background-color: var(--bg-medium); border: 1px solid var(--bg-light);">${actionOptions}</div>
                        </div>
                        <span class="text-lg opacity-50">#${STATE.selectedStepIndex + 1}</span>
                    </div>
                    ${fieldsHtml}
                </div>`;
        };

        const createFormField = (key, type, data) => {
            const title = key.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase());
            let field = '';
            const value = data[key];
            switch (type) {
                case 'text': field = `<textarea data-key="${key}" class="${INPUT_CLASS} min-h-[40px] resize-y">${value || ''}</textarea>`; break;
                case 'number': field = `<input type="number" data-key="${key}" value="${value !== undefined ? value : 0}" class="${INPUT_CLASS}">`; break;
                case 'boolean': field = `<label class="flex items-center space-x-3 cursor-pointer"><input type="checkbox" data-key="${key}" ${value ? 'checked' : ''} class="h-5 w-5 rounded-md text-primary focus:ring-primary bg-transparent border-light"><span class="font-medium">${title}</span></label>`; break;
                case 'vec2': field = `<div class="grid grid-cols-2 gap-2"><div><label class="block text-xs font-medium text-gray-400 mb-1">X</label><input type="number" step="0.1" data-key="${key}.x" value="${value?.x || 0}" class="${INPUT_CLASS}"></div><div><label class="block text-xs font-medium text-gray-400 mb-1">Y</label><input type="number" step="0.1" data-key="${key}.y" value="${value?.y || 0}" class="${INPUT_CLASS}"></div></div>`; break;
                case 'item': field = `<div class="grid grid-cols-2 gap-2"><div><label class="block text-xs font-medium text-gray-400 mb-1">ID</label><input type="number" data-key="${key}.id" value="${value?.id || 0}" class="${INPUT_CLASS}"></div><div><label class="block text-xs font-medium text-gray-400 mb-1">Value</label><input type="number" data-key="${key}.value" value="${value?.value || 0}" class="${INPUT_CLASS}"></div></div>`; break;
                case 'door_key': {
                    const allKeys = Array.from(new Set(STATE.scenarioSteps.filter(s => s.action === 'new_door' && s.key).map(s => s.key)));
                    const datalistId = `door-keys-list-${STATE.selectedStepIndex}`;
                    const optionsHtml = allKeys.map(k => `<option value="${k}"></option>`).join('');
                    field = `<input type="text" list="${datalistId}" data-key="${key}" value="${value || ''}" class="${INPUT_CLASS}" placeholder="Введите или выберите ключ..."><datalist id="${datalistId}">${optionsHtml}</datalist>`; break;
                }
                case 'markers': {
                    const markers = value || [];
                    const headerHtml = `<div class="grid grid-cols-3 gap-x-2 items-center px-2 pb-1 text-xs font-bold text-gray-400 uppercase border-b border-gray-600 mb-2"><span>Position X</span><span>Position Y</span><span>Health</span></div>`;
                    const markersHtml = markers.map((marker, index) => `<div class="grid grid-cols-3 gap-x-2 items-center"><input type="number" step="0.1" value="${marker.position.x}" class="${INPUT_CLASS}" data-marker-index="${index}" data-marker-prop="position.x"><input type="number" step="0.1" value="${marker.position.y}" class="${INPUT_CLASS}" data-marker-index="${index}" data-marker-prop="position.y"><div class="flex items-center gap-2"><input type="number" value="${marker.health}" class="${INPUT_CLASS}" data-marker-index="${index}" data-marker-prop="health"><button class="remove-marker-btn text-gray-400 hover:text-secondary" data-marker-index="${index}"><i class="fas fa-times"></i></button></div></div>`).join('');
                    field = `<div class="bg-black/20 p-3 rounded-md">${markers.length > 0 ? headerHtml : ''}<div data-key="${key}" class="space-y-2">${markersHtml}</div></div><button class="add-marker-btn mt-2 bg-blue-500 hover:bg-blue-600 text-white text-sm py-1 px-3 rounded-md btn"><i class="fas fa-plus mr-1"></i>Добавить маркер</button>`; break;
                }
            }
            const containerClass = "p-4 rounded-lg";
            return type === 'boolean' ? `<div class="${containerClass} hover:bg-black/10">${field}</div>` : `<div class="${containerClass} bg-black/10"><label class="block text-sm font-medium text-gray-300 mb-2">${title}</label>${field}</div>`;
        };
        
        const showUndoToast = (message) => {
            clearTimeout(STATE.undoTimeout);
            DOM.undoMessage.textContent = message;
            DOM.undoToast.classList.add('show');
            STATE.undoTimeout = setTimeout(() => {
                DOM.undoToast.classList.remove('show');
                STATE.undoState = {};
            }, 5000);
        };

        const showConfirm = (title, message, buttons) => {
            DOM.confirmTitle.textContent = title;
            DOM.confirmMessage.textContent = message;
            DOM.confirmButtons.innerHTML = '';
            buttons.forEach(btnInfo => {
                const button = Object.assign(document.createElement('button'), {
                    textContent: btnInfo.text,
                    className: btnInfo.class,
                    onclick: () => { btnInfo.onClick(); DOM.confirmModal.style.display = 'none'; }
                });
                DOM.confirmButtons.appendChild(button);
            });
            const cancelButton = Object.assign(document.createElement('button'), {
                textContent: 'Отмена',
                className: 'bg-gray-600 hover:bg-gray-700 text-white font-bold py-2 px-4 btn',
                onclick: () => DOM.confirmModal.style.display = 'none'
            });
            DOM.confirmButtons.appendChild(cancelButton);
            DOM.confirmModal.style.display = 'flex';
        };

        const updateStepData = (input) => {
            if (STATE.selectedStepIndex === -1) return;
            const currentStep = STATE.scenarioSteps[STATE.selectedStepIndex];
            const keyPath = input.dataset.key;
            if (!keyPath) {
                const key = 'markers';
                const index = parseInt(input.dataset.markerIndex);
                if (!currentStep[key]?.[index]) return;
                const propPath = input.dataset.markerProp;
                const value = input.type === 'number' ? parseFloat(input.value) : input.value;
                let obj = currentStep[key][index];
                const keys = propPath.split('.');
                keys.slice(0, -1).forEach(k => obj = obj[k] || (obj[k] = {}));
                obj[keys[keys.length - 1]] = value;
            } else {
                const value = input.type === 'checkbox' ? input.checked : (input.type === 'number' ? parseFloat(input.value) || 0 : input.value);
                const keys = keyPath.split('.');
                let current = currentStep;
                keys.slice(0, -1).forEach(key => current = current[key] || (current[key] = {}));
                current[keys[keys.length - 1]] = value;
            }
        };

        const bindEvents = () => {
            document.addEventListener('click', e => {
                const button = e.target.closest('button');
                const action = button?.dataset.action || button?.id;
                const dropdowns = document.querySelectorAll('.action-type-dropdown, .scenario-type-dropdown');
                dropdowns.forEach(container => {
                     if (!container.contains(e.target)) {
                        container.querySelector('.custom-select-options')?.classList.add('hidden');
                    }
                });

                switch (action) {
                    case 'add-step-btn': DOM.addStepModal.style.display = 'flex'; break;
                    case 'cancel-add': DOM.addStepModal.style.display = 'none'; break;
                    case 'close-modal': DOM.exportModal.style.display = 'none'; DOM.confirmModal.style.display = 'none'; break;
                    case 'confirm-add': {
                        const newStep = { action: DOM.actionSelect.value };
                        if (newStep.action === 'group_header') newStep.name = "Новая группа";
                        const insertionIndex = STATE.selectedStepIndex === -1 ? STATE.scenarioSteps.length : STATE.selectedStepIndex + 1;
                        STATE.scenarioSteps.splice(insertionIndex, 0, newStep);
                        STATE.selectedStepIndex = insertionIndex;
                        render();
                        DOM.addStepModal.style.display = 'none';
                        break;
                    }
                    case 'view-json-btn': {
                        const cleanedSteps = STATE.scenarioSteps.map(({ collapsed, ...step }) => step.action === 'group_header' ? {action: step.action, name: step.name} : step);
                        const exportData = { steps: cleanedSteps };
                        const finalJson = STATE.selectedScenarioEvent === 'general' ? exportData : { [STATE.selectedScenarioEvent]: exportData };
                        DOM.jsonOutput.value = JSON.stringify(finalJson, null, 2);
                        DOM.exportModal.style.display = 'flex';
                        break;
                    }
                    case 'copy-json-btn':
                        DOM.jsonOutput.select();
                        document.execCommand('copy');
                        button.innerHTML = '<i class="fas fa-check mr-2"></i>Скопировано!';
                        setTimeout(() => button.innerHTML = '<i class="fas fa-copy mr-2"></i>Копировать', 2000);
                        break;
                    case 'import-json-btn': DOM.importFileInput.click(); break;
                    case 'delete-all-btn':
                        if (STATE.scenarioSteps.length > 0) {
                            STATE.undoState = { type: 'all', steps: [...STATE.scenarioSteps], event: STATE.selectedScenarioEvent };
                            STATE.scenarioSteps = [];
                            STATE.selectedStepIndex = -1;
                            render();
                            showUndoToast('Все шаги удалены.');
                        }
                        break;
                    case 'undo-btn':
                        if (STATE.undoState.type === 'single' || STATE.undoState.type === 'group') {
                            STATE.scenarioSteps.splice(STATE.undoState.index, 0, ...(STATE.undoState.steps || [STATE.undoState.step]));
                            STATE.selectedStepIndex = STATE.undoState.index;
                        } else if (STATE.undoState.type === 'all') {
                            STATE.scenarioSteps = STATE.undoState.steps;
                            STATE.selectedScenarioEvent = STATE.undoState.event;
                        }
                        STATE.undoState = {};
                        DOM.undoToast.classList.remove('show');
                        clearTimeout(STATE.undoTimeout);
                        render();
                        break;
                }
            });
            
            DOM.stepList.addEventListener('click', e => {
                const target = e.target.closest('.sidebar-item, .delete-step-btn, .group-toggle-btn');
                if (!target) return;
                const index = parseInt(target.dataset.index);

                if (target.matches('.group-toggle-btn, .group-toggle-btn *')) {
                    const groupStep = STATE.scenarioSteps[index];
                    if (groupStep.action === 'group_header') {
                        groupStep.collapsed = !groupStep.collapsed;
                        renderStepList();
                    }
                } else if (target.matches('.delete-step-btn, .delete-step-btn *')) {
                    const step = STATE.scenarioSteps[index];
                    if (step.action === 'group_header') {
                        showConfirm(`Удалить группу "${step.name || 'Без имени'}"?`, 'Выберите способ удаления:', [
                            { text: 'Группу и шаги', class: 'bg-red-600 hover:bg-red-700 text-white font-bold py-2 px-4 btn', onClick: () => {
                                let count = 1;
                                for (let i = index + 1; i < STATE.scenarioSteps.length; i++) {
                                    if (STATE.scenarioSteps[i].action === 'group_header') break;
                                    count++;
                                }
                                const deletedGroup = STATE.scenarioSteps.splice(index, count);
                                STATE.undoState = { type: 'group', steps: deletedGroup, index: index };
                                if (STATE.selectedStepIndex >= index) STATE.selectedStepIndex = (index > 0) ? index - 1 : -1;
                                render();
                                showUndoToast('Группа с шагами удалена.');
                            }},
                            { text: 'Только заголовок', class: 'bg-yellow-500 hover:bg-yellow-600 text-black font-bold py-2 px-4 btn', onClick: () => {
                                STATE.undoState = { type: 'single', step: STATE.scenarioSteps[index], index };
                                STATE.scenarioSteps.splice(index, 1);
                                if (STATE.selectedStepIndex === index) STATE.selectedStepIndex = -1;
                                else if (STATE.selectedStepIndex > index) STATE.selectedStepIndex--;
                                render();
                                showUndoToast('Заголовок группы удален.');
                            }}
                        ]);
                    } else {
                        STATE.undoState = { type: 'single', step: STATE.scenarioSteps[index], index };
                        STATE.scenarioSteps.splice(index, 1);
                        if (STATE.selectedStepIndex === index) STATE.selectedStepIndex = -1;
                        else if (STATE.selectedStepIndex > index) STATE.selectedStepIndex--;
                        render();
                        showUndoToast('Шаг удален.');
                    }
                } else if (target.matches('.sidebar-item, .sidebar-item *')) {
                    STATE.selectedStepIndex = index;
                    render();
                }
            });

            DOM.editorPanel.addEventListener('input', e => {
                 if (STATE.selectedStepIndex !== -1 && e.target.dataset.key) {
                    updateStepData(e.target);
                    const step = STATE.scenarioSteps[STATE.selectedStepIndex];
                    if (step && step.action === 'group_header' && e.target.dataset.key === 'name') {
                        renderStepList();
                    }
                }
            });

            DOM.editorPanel.addEventListener('click', e => {
                const selectButton = e.target.closest('.custom-select-button');
                if (selectButton) {
                    selectButton.nextElementSibling.classList.toggle('hidden');
                    return;
                }
                const selectOption = e.target.closest('.custom-select-option');
                if (selectOption) {
                    const newAction = selectOption.dataset.actionKey;
                    const oldStep = STATE.scenarioSteps[STATE.selectedStepIndex];
                    const newStep = { action: newAction };
                    // Preserve common properties like 'name' if switching to/from group_header
                    if(newAction === 'group_header' && !newStep.hasOwnProperty('name')) {
                        newStep.name = "Новая группа";
                    } else if (oldStep.action === 'group_header') {
                        // If switching away from a group header, don't carry over the name property
                    }
                    STATE.scenarioSteps[STATE.selectedStepIndex] = newStep;

                    render();
                    return;
                }
                const markerButton = e.target.closest('.add-marker-btn, .remove-marker-btn');
                if (markerButton) {
                    const key = 'markers';
                    const step = STATE.scenarioSteps[STATE.selectedStepIndex];
                    if (!step[key]) step[key] = [];
                    if (markerButton.matches('.add-marker-btn')) {
                        step[key].push({ position: { x: 0, y: 0 }, health: 1 });
                    } else {
                        step[key].splice(parseInt(markerButton.dataset.markerIndex), 1);
                    }
                    renderEditorPanel();
                }
            });
            
            DOM.scenarioTypeContainer.addEventListener('click', e => {
                const target = e.target.closest('.custom-select-button, .scenario-event-option');
                if(!target) return;
                if(target.matches('.custom-select-button, .custom-select-button *')) {
                    target.closest('.scenario-type-dropdown').querySelector('.custom-select-options').classList.toggle('hidden');
                } else if (target.matches('.scenario-event-option')) {
                    STATE.selectedScenarioEvent = target.dataset.eventKey;
                    renderScenarioTypeSelector();
                }
            });

            DOM.importFileInput.addEventListener('change', event => {
                const file = event.target.files[0];
                if (!file) return;
                const reader = new FileReader();
                reader.onload = (e) => {
                    try {
                        const data = JSON.parse(e.target.result);
                        const keys = Object.keys(data);
                        if (keys.includes('steps')) {
                            STATE.scenarioSteps = data.steps;
                            STATE.selectedScenarioEvent = 'general';
                        } else {
                            const eventKey = keys.find(k => k !== "steps" && data[k] && Array.isArray(data[k].steps));
                            if (eventKey) {
                                STATE.scenarioSteps = data[eventKey].steps;
                                STATE.selectedScenarioEvent = eventKey;
                            } else throw new Error('Неверный формат файла.');
                        }
                        STATE.selectedStepIndex = -1;
                        render();
                    } catch (error) { alert(`Ошибка при чтении файла: ${error.message}`); }
                };
                reader.readAsText(file);
                event.target.value = '';
            });

            DOM.stepList.addEventListener('dragstart', e => {
                const target = e.target.closest('.sidebar-item');
                if (target) {
                    STATE.draggedIndex = parseInt(target.dataset.index);
                    setTimeout(() => target.classList.add('dragging'), 0);
                }
            });
            DOM.stepList.addEventListener('dragover', e => e.preventDefault());
            DOM.stepList.addEventListener('dragenter', e => e.target.closest('.sidebar-item')?.classList.add('drag-over'));
            DOM.stepList.addEventListener('dragleave', e => e.target.closest('.sidebar-item')?.classList.remove('drag-over'));
            DOM.stepList.addEventListener('dragend', () => document.querySelectorAll('.dragging').forEach(el => el.classList.remove('dragging')));
            DOM.stepList.addEventListener('drop', e => {
                e.preventDefault();
                document.querySelectorAll('.drag-over').forEach(el => el.classList.remove('drag-over'));
                const dropTarget = e.target.closest('.sidebar-item');
                if (!dropTarget) return;
                const dropIndex = parseInt(dropTarget.dataset.index);
                if (STATE.draggedIndex !== null && STATE.draggedIndex !== dropIndex) {
                    const item = STATE.scenarioSteps.splice(STATE.draggedIndex, 1)[0];
                    STATE.scenarioSteps.splice(dropIndex, 0, item);
                    if (STATE.selectedStepIndex === STATE.draggedIndex) STATE.selectedStepIndex = dropIndex;
                    else if (STATE.draggedIndex < STATE.selectedStepIndex && dropIndex >= STATE.selectedStepIndex) STATE.selectedStepIndex--;
                    else if (STATE.draggedIndex > STATE.selectedStepIndex && dropIndex <= STATE.selectedStepIndex) STATE.selectedStepIndex++;
                    render();
                }
                STATE.draggedIndex = null;
            });
        };

        const init = () => {
            DOM.actionSelect.innerHTML = Object.entries(ACTION_DEFS).sort(([,a],[,b]) => a.name.localeCompare(b.name))
                .map(([key, value]) => `<option value="${key}">${value.name}</option>`).join('');
            bindEvents();
            render();
        };

        return { init };
    })();

    const EventEditor = (() => {
        let mounted = false;
        return {
            mount(container, options = {}) {
                if (mounted) {
                    return;
                }
                if (options.embedded) {
                    document.body.classList.add('embedded');
                } else {
                    document.body.classList.remove('embedded');
                }
                App.init();
                mounted = true;
            },
            unmount() {
                mounted = false;
            },
        };
    })();

    window.EventEditor = EventEditor;

    document.addEventListener('DOMContentLoaded', () => {
        EventEditor.mount(document.body, { embedded: <?php echo $embedded ? 'true' : 'false'; ?> });
    });
    </script>
</body>
</html>
