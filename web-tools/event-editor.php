<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Редактор событий</title>
    <!-- Tailwind (CDN). Load shared config BEFORE tailwindcss. -->
    <script src="editor-core/tailwind-theme.js"></script>
    <script src="https://cdn.tailwindcss.com?plugins=forms"></script>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <link rel="stylesheet" href="editor-core/editor-theme.css">
    <style>
        body{background:var(--editor-bg);color:var(--editor-text);overflow:hidden;}
        #app{background:var(--editor-bg);}
        aside{background:rgba(15,23,42,.78);border-right:1px solid var(--editor-border);backdrop-filter:blur(6px);}
        .sidebar-item{border-left:3px solid transparent;transition:background .12s ease,border-color .12s ease;}
        .sidebar-item:hover{background:rgba(255,255,255,.04);}
        .sidebar-item.selected{background:rgba(99,102,241,.16);border-left-color:var(--editor-primary);color:var(--editor-text);} 
        .sidebar-item.selected .text-gray-400{color:var(--editor-text-muted);} 
        .group-header{background:rgba(255,255,255,.03);border-top:1px solid var(--editor-border);border-bottom:1px solid var(--editor-border);margin-top:10px;margin-bottom:6px;}
        #undo-toast{transition:transform .25s ease,opacity .25s ease;transform:translateY(200%);opacity:0;}
        #undo-toast.show{transform:translateY(0);opacity:1;}
        .drag-handle{cursor:move;}
        .dragging{opacity:.6;}
        .drag-over{border-top:2px dashed var(--editor-accent);}
    </style>

</head>
<body class="antialiased editor-theme">
    <div id="app" class="flex flex-col md:flex-row h-screen overflow-hidden">
        <aside class="w-full md:w-1/3 md:max-w-sm flex flex-col border-r">
            <div class="p-4 border-b border-slate-700/40 text-center">
                <h1 class="text-xl font-semibold text-slate-100">Редактор событий</h1>
                <div id="scenario-type-container" class="mt-2"></div>
            </div>
            <div id="step-list-root" class="flex-grow overflow-y-auto p-2"></div>
            <div id="controls" class="p-4 border-t border-slate-700/40 space-y-3">
                 <button id="add-step-btn" class="w-full editor-btn editor-btn-primary"><i class="fas fa-plus mr-2"></i> Добавить шаг</button>
                <div class="grid grid-cols-1 sm:grid-cols-2 gap-3">
                    <button id="import-json-btn" class="w-full editor-btn editor-btn-secondary"><i class="fas fa-upload mr-2"></i> Импорт</button>
                    <button id="view-json-btn" class="w-full editor-btn editor-btn-secondary"><i class="fas fa-eye mr-2"></i> Просмотр JSON</button>
                </div>
                <button id="delete-all-btn" class="w-full editor-btn editor-btn-danger"><i class="fas fa-bomb mr-2"></i> Удалить все</button>
                 <input type="file" id="import-file-input" class="hidden" accept=".json">
            </div>
        </aside>

        <main id="editor-panel" class="w-full md:w-2/3 flex-grow p-4 md:p-6 overflow-y-auto">
             <div class="flex flex-col items-center justify-center h-full text-center text-gray-500 opacity-50">
                <i class="fas fa-cat fa-5x mb-4 animate-bounce"></i>
                <h2 class="text-3xl font-semibold">Выберите шаг для редактирования</h2>
                <p>...или создайте новый, чтобы начать приключение!</p>
            </div>
        </main>
    </div>

    <script src="editor-core/registry.js"></script>
    <script src="editor-core/core.js"></script>
    <script src="editor-core/db-map.js"></script>
    <script src="editor-core/field-renderer.js"></script>
    <script src="editor-core/db.js"></script>
    <script src="editor-core/ui.js"></script>
    <script src="editor-core/ui-manager.js"></script>
    <script src="editor-core/defaults.js"></script>
    <script src="editor-core/utils.js"></script>
    <script src="editor-core/bootstrap.js"></script>
    <script>
    const App = (() => {
        let DOM = {};

        const bindDOM = () => {
            DOM = {
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
        };

        const STATE = {
            scenarioSteps: [],
            selectedStepIndex: -1,
            draggedIndex: null,
            undoState: {},
            undoTimeout: null,
            selectedScenarioEvent: 'general',
        };
        
        const { actionSchemas, applyDefaults } = window.EditorCore.schemas;
        const SCENARIO_EVENTS = {"general":"Стандартный","on_recieve_objectives":"При получении задач","on_complete_objectives":"При завершении задач","on_end":"При окончании шага","on_equip":"При экипировке предмета","on_got":"При получении предмета","on_lost":"При потере предмета","on_unequip":"При снятии предмета"};
        // Shared boot + shared FieldRenderer options.
        const { fieldRenderOptions: FIELD_RENDER_OPTIONS } = EditorCore.bootstrapEditor({ mode: 'event' });

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
                    <button class="custom-select-button flex items-center justify-center w-full text-lg font-semibold text-white cursor-pointer" style="color: var(--editor-accent);">
                        <i class="fas fa-tag mr-2"></i>
                        <span>${SCENARIO_EVENTS[STATE.selectedScenarioEvent]}</span>
                        <i class="fas fa-chevron-down ml-2 text-sm opacity-70"></i>
                    </button>
                    <div class="custom-select-options hidden absolute top-full left-0 mt-2 w-full max-h-60 overflow-y-auto rounded-lg z-20 p-2" style="background-color: var(--editor-surface); border: 1px solid var(--editor-border);">${optionsHtml}</div>
                </div>`;
        };

        const renderStepList = () => {
            if (STATE.scenarioSteps.length === 0) {
                EditorCore.UI.renderEmptyList(DOM.stepList, DOM.stepList.dataset.emptyMessage || 'Нет шагов... пока!');
                return;
            }
            let groupSubIndex = 1;
            DOM.stepList.innerHTML = STATE.scenarioSteps.map((step, index) => {
                const def = actionSchemas[step.action] || { name: step.action, icon: 'fa-question', fields: {} };
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
            const def = actionSchemas[step.action] || { name: step.action, icon: 'fa-question', fields: {} };
            const actionOptions = Object.entries(actionSchemas).sort(([,a],[,b]) => a.name.localeCompare(b.name)).map(([key, value]) => `
                <div class="custom-select-option p-2 rounded-md hover:bg-light cursor-pointer flex items-center" data-action-key="${key}"><i class="fas ${value.icon} w-6 text-center mr-2 text-accent"></i><span>${value.name}</span></div>`
            ).join('');
            const fieldsHtml = window.EditorCore.FieldRenderer.renderFields({
                fields: def.fields,
                data: step,
                options: FIELD_RENDER_OPTIONS,
                sort: true
            });

            DOM.editorPanel.innerHTML = `
                <div class="space-y-6">
                    <div class="flex items-center justify-between">
                        <div class="relative action-type-dropdown">
                            <button class="custom-select-button flex items-center text-3xl font-bold text-white cursor-pointer" style="text-shadow: 0 0 3px var(--editor-accent);">
                                <i class="fas ${def.icon} mr-3"></i><span>${def.name}</span><i class="fas fa-chevron-down ml-3 text-xl opacity-70"></i>
                            </button>
                            <div class="custom-select-options hidden absolute top-full left-0 mt-2 w-72 max-h-80 overflow-y-auto rounded-lg z-10 p-2" style="background-color: var(--editor-surface); border: 1px solid var(--editor-border);">${actionOptions}</div>
                        </div>
                        <span class="text-lg opacity-50">#${STATE.selectedStepIndex + 1}</span>
                    </div>
                    ${fieldsHtml}
                </div>`;

            // Initialize UI components for newly rendered fields (DB selects, validation hints, toggles).
            // This is REQUIRED because the editor panel is re-rendered on every selection.
            if (window.EditorCore?.UIManager) {
                window.EditorCore.UIManager.init(DOM.editorPanel);
            }
        };

        const applyActionDefaults = (step) => {
            const schema = actionSchemas[step.action];
            if (schema?.fields) {
                applyDefaults(step, schema.fields);
            }
            return step;
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
            const keyPath = input.dataset.path || input.dataset.key;
            if (!keyPath) return;
            const value = window.EditorCore.FieldRenderer.getInputValue(input);
            window.EditorCore.FieldRenderer.setValueAtPath(currentStep, keyPath, value);
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
                        const action = DOM.actionSelect.value;
                        const newStep = { action };
                        applyActionDefaults(newStep);
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

            const onEditorFieldChange = (e) => {
                if (STATE.selectedStepIndex !== -1 && (e.target.dataset.key || e.target.dataset.path)) {
                    updateStepData(e.target);
                    const step = STATE.scenarioSteps[STATE.selectedStepIndex];
                    if (step && step.action === 'group_header' && (e.target.dataset.key === 'name' || e.target.dataset.path === 'name')) {
                        renderStepList();
                    }
                }
            };

            // Some controls (select/checkbox) primarily emit `change`, while text inputs emit `input`.
            // Listening to both ensures DB-backed selects correctly update the model and JSON preview.
            DOM.editorPanel.addEventListener('input', onEditorFieldChange);
            DOM.editorPanel.addEventListener('change', onEditorFieldChange);

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
                const listButton = e.target.closest('[data-list-action]');
                if (listButton) {
                    const step = STATE.scenarioSteps[STATE.selectedStepIndex];
                    const listPath = listButton.dataset.listPath;
                    if (!listPath) return;
                    const listValue = window.EditorCore.FieldRenderer.getValueAtPath(step, listPath) || [];
                    if (listButton.dataset.listAction === 'add') {
                        const defaultItemRaw = listButton.dataset.listDefault ? decodeURIComponent(listButton.dataset.listDefault) : null;
                        const defaultItem = defaultItemRaw ? JSON.parse(defaultItemRaw) : {};
                        listValue.push(defaultItem);
                        window.EditorCore.FieldRenderer.setValueAtPath(step, listPath, listValue);
                    } else if (listButton.dataset.listAction === 'remove') {
                        const index = parseInt(listButton.dataset.listIndex);
                        if (!Number.isNaN(index)) {
                            listValue.splice(index, 1);
                            window.EditorCore.FieldRenderer.setValueAtPath(step, listPath, listValue);
                        }
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
                            STATE.scenarioSteps = data.steps.map(step => applyActionDefaults(step));
                            STATE.selectedScenarioEvent = 'general';
                        } else {
                            const eventKey = keys.find(k => k !== "steps" && data[k] && Array.isArray(data[k].steps));
                            if (eventKey) {
                                STATE.scenarioSteps = data[eventKey].steps.map(step => applyActionDefaults(step));
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
            EditorCore.UI.mountListContainer({
                id: 'step-list',
                parent: document.getElementById('step-list-root'),
                className: 'space-y-2',
                emptyMessage: 'Нет шагов... пока!'
            });
            bindDOM();
            DOM.actionSelect.innerHTML = Object.entries(actionSchemas).sort(([,a],[,b]) => a.name.localeCompare(b.name))
                .map(([key, value]) => `<option value="${key}">${value.name}</option>`).join('');
            bindEvents();
            render();
        };

        return EditorCore.createEditorApp({ id: 'event-editor', init, render, state: STATE });
    })();

    document.addEventListener('DOMContentLoaded', () => App.mount());
    </script>
</body>
</html>
