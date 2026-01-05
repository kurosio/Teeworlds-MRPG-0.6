const normalizeOption = (option) => {
    if (typeof option === 'string' || typeof option === 'number') {
        return { value: option, label: option };
    }
    return option;
};

const getValueByPath = (data, path) => {
    if (!path) return undefined;
    return path.split('.').reduce((acc, key) => (acc && acc[key] !== undefined ? acc[key] : undefined), data);
};

const setValueByPath = (data, path, value) => {
    if (!path) return;
    const keys = path.split('.');
    let current = data;
    keys.slice(0, -1).forEach((key) => {
        if (typeof current[key] !== 'object' || current[key] === null) {
            current[key] = {};
        }
        current = current[key];
    });
    current[keys[keys.length - 1]] = value;
};

const unsetValueByPath = (data, path) => {
    if (!path) return;
    const keys = path.split('.');
    let current = data;
    keys.slice(0, -1).forEach((key) => {
        if (!current || typeof current !== 'object') return;
        current = current[key];
    });
    if (current && typeof current === 'object') {
        delete current[keys[keys.length - 1]];
    }
};

const resolveOptions = (field, context) => {
    const options = typeof field.options === 'function' ? field.options(context) : field.options || [];
    return options.map(normalizeOption);
};

const resolveDefaultListItem = (field) => {
    if (typeof field.newItem === 'function') {
        return field.newItem();
    }
    const item = {};
    (field.itemFields || []).forEach((subField) => {
        if (subField.type === 'boolean') {
            setValueByPath(item, subField.key, false);
        } else if (subField.type === 'number') {
            setValueByPath(item, subField.key, subField.default ?? 0);
        } else {
            setValueByPath(item, subField.key, subField.default ?? '');
        }
    });
    return item;
};

const parseValue = (field, rawValue) => {
    if (field.type === 'number') {
        if (rawValue === '' || rawValue === null || rawValue === undefined) {
            return field.allowEmpty ? undefined : 0;
        }
        const value = parseFloat(rawValue);
        return Number.isNaN(value) ? 0 : value;
    }
    if (field.type === 'boolean') {
        return Boolean(rawValue);
    }
    return rawValue;
};

const evaluateShowWhen = (showWhen, values) => {
    if (!showWhen) return true;
    const conditions = Array.isArray(showWhen) ? showWhen : [showWhen];
    return conditions.every((condition) => {
        const currentValue = values[condition.key];
        if ('equals' in condition) {
            return currentValue === condition.equals;
        }
        if ('not' in condition) {
            return currentValue !== condition.not;
        }
        if ('in' in condition) {
            return (condition.in || []).includes(currentValue);
        }
        return true;
    });
};

const buildFieldWrapper = (field, innerHtml, options, isBoolean = false) => {
    if (field.type === 'divider') {
        return `<hr class="${options.dividerClass || 'my-5 border-slate-200'}">`;
    }
    const wrapperClass = field.wrapperClass || options.wrapperClass || 'p-4 rounded-lg bg-black/10';
    if (isBoolean) {
        return `<div class="${wrapperClass} hover:bg-black/10" data-field-key="${field.key}">${innerHtml}</div>`;
    }
    return `<div class="${wrapperClass}" data-field-key="${field.key}">
        ${field.label ? `<label class="${field.labelClass || options.labelClass || 'block text-sm font-medium text-slate-700 mb-2'}">${field.label}</label>` : ''}
        ${innerHtml}
    </div>`;
};

const renderListItem = (field, item, index, options) => {
    const inputsHtml = (field.itemFields || []).map((subField) => {
        const value = getValueByPath(item, subField.key);
        const inputClass = subField.inputClass || options.inputClass || 'w-full rounded-md border-slate-300 shadow-sm focus:border-blue-500 focus:ring-blue-500';
        const labelClass = options.subLabelClass || 'block text-xs font-medium text-gray-400 mb-1';
        if (subField.type === 'boolean') {
            return `
                <label class="${options.checkboxLabelClass || 'flex items-center space-x-2'}">
                    <input type="checkbox" data-list-key="${field.key}" data-list-index="${index}" data-list-prop="${subField.key}" ${value ? 'checked' : ''} class="${options.checkboxClass || 'rounded border-slate-300 text-blue-600 shadow-sm focus:ring-blue-500'}">
                    <span class="${options.checkboxTextClass || 'text-sm'}">${subField.label || subField.key}</span>
                </label>`;
        }
        return `
            <div>
                ${subField.label ? `<label class="${labelClass}">${subField.label}</label>` : ''}
                <input type="${subField.type === 'number' ? 'number' : 'text'}" ${subField.step ? `step="${subField.step}"` : ''} value="${value ?? ''}" data-list-key="${field.key}" data-list-index="${index}" data-list-prop="${subField.key}" class="${inputClass}">
            </div>`;
    }).join('');

    return `
        <div class="${field.itemWrapperClass || options.listItemWrapperClass || 'grid grid-cols-2 gap-4 relative p-3 rounded-md border border-slate-200 bg-slate-50'}" data-list-item="${index}">
            <button type="button" class="${options.listRemoveClass || 'absolute top-2 right-2 text-slate-400 hover:text-red-500'}" data-list-remove="${field.key}" data-list-index="${index}">
                <i class="${options.listRemoveIcon || 'fa-solid fa-times'}"></i>
            </button>
            ${inputsHtml}
        </div>`;
};

export const formUtils = {
    renderFields(fields, data, context = {}, options = {}) {
        return fields.map((field) => {
            if (field.type === 'divider') {
                return buildFieldWrapper(field, '', options);
            }
            const value = getValueByPath(data, field.key);
            const inputClass = field.inputClass || options.inputClass || 'w-full rounded-md border-slate-300 shadow-sm focus:border-blue-500 focus:ring-blue-500';
            const selectClass = field.selectClass || options.selectClass || inputClass;
            const textareaClass = field.textareaClass || options.textareaClass || inputClass;
            const checkboxClass = field.checkboxClass || options.checkboxClass || 'rounded border-slate-300 text-blue-600 shadow-sm focus:ring-blue-500';

            switch (field.type) {
                case 'text': {
                    const control = field.multiline
                        ? `<textarea name="${field.key}" rows="${field.rows || 4}" class="${textareaClass}">${value ?? ''}</textarea>`
                        : `<input type="text" name="${field.key}" value="${value ?? ''}" class="${inputClass}">`;
                    return buildFieldWrapper(field, control, options);
                }
                case 'number': {
                    const control = `<input type="number" name="${field.key}" value="${value ?? field.default ?? 0}" ${field.step ? `step="${field.step}"` : ''} class="${inputClass}">`;
                    return buildFieldWrapper(field, control, options);
                }
                case 'boolean': {
                    const control = `<label class="${options.checkboxLabelClass || 'flex items-center space-x-3 cursor-pointer'}">
                        <input type="checkbox" name="${field.key}" ${value ? 'checked' : ''} class="${checkboxClass}">
                        <span class="${options.checkboxTextClass || 'font-medium'}">${field.label || field.key}</span>
                    </label>`;
                    return buildFieldWrapper(field, control, options, true);
                }
                case 'select': {
                    const optionsHtml = resolveOptions(field, context).map((option) => (
                        `<option value="${option.value}" ${option.value === value ? 'selected' : ''}>${option.label}</option>`
                    )).join('');
                    const control = `<select name="${field.key}" class="${selectClass}">${optionsHtml}</select>`;
                    return buildFieldWrapper(field, control, options);
                }
                case 'vec2': {
                    const xValue = value?.x ?? 0;
                    const yValue = value?.y ?? 0;
                    const control = `<div class="grid grid-cols-2 gap-2">
                        <div>
                            <label class="${options.subLabelClass || 'block text-xs font-medium text-gray-400 mb-1'}">X</label>
                            <input type="number" name="${field.key}.x" value="${xValue}" ${field.step ? `step="${field.step}"` : ''} class="${inputClass}">
                        </div>
                        <div>
                            <label class="${options.subLabelClass || 'block text-xs font-medium text-gray-400 mb-1'}">Y</label>
                            <input type="number" name="${field.key}.y" value="${yValue}" ${field.step ? `step="${field.step}"` : ''} class="${inputClass}">
                        </div>
                    </div>`;
                    return buildFieldWrapper(field, control, options);
                }
                case 'item': {
                    const idValue = value?.id ?? 0;
                    const valueValue = value?.value ?? 0;
                    const control = `<div class="grid grid-cols-2 gap-2">
                        <div>
                            <label class="${options.subLabelClass || 'block text-xs font-medium text-gray-400 mb-1'}">ID</label>
                            <input type="number" name="${field.key}.id" value="${idValue}" class="${inputClass}">
                        </div>
                        <div>
                            <label class="${options.subLabelClass || 'block text-xs font-medium text-gray-400 mb-1'}">Value</label>
                            <input type="number" name="${field.key}.value" value="${valueValue}" class="${inputClass}">
                        </div>
                    </div>`;
                    return buildFieldWrapper(field, control, options);
                }
                case 'door_key': {
                    const listId = `door-keys-${field.key}-${context.instanceId || 'default'}`;
                    const optionsHtml = (context.doorKeys || []).map((key) => `<option value="${key}"></option>`).join('');
                    const control = `<input type="text" name="${field.key}" list="${listId}" value="${value ?? ''}" class="${inputClass}" placeholder="${field.placeholder || 'Введите ключ...'}">
                        <datalist id="${listId}">${optionsHtml}</datalist>`;
                    return buildFieldWrapper(field, control, options);
                }
                case 'list': {
                    const items = Array.isArray(value) ? value : [];
                    const header = field.showHeader ? `
                        <div class="${options.listHeaderClass || 'grid grid-cols-3 gap-x-2 items-center px-2 pb-1 text-xs font-bold text-gray-400 uppercase border-b border-gray-600 mb-2'}">
                            ${(field.itemFields || []).map((subField) => `<span>${subField.label || subField.key}</span>`).join('')}
                        </div>` : '';
                    const listItemsHtml = items.map((item, index) => renderListItem(field, item, index, options)).join('');
                    const control = `
                        <div class="${field.listContainerClass || options.listContainerClass || 'bg-black/20 p-3 rounded-md'}">
                            ${header}
                            <div data-list-key="${field.key}" class="${field.listItemsClass || options.listItemsClass || 'space-y-2'}">${listItemsHtml}</div>
                        </div>
                        <button type="button" class="${field.addButtonClass || options.listAddClass || 'mt-2 ui-btn ui-btn-primary text-sm py-1 px-3 rounded-md'}" data-list-add="${field.key}">
                            <i class="${field.addButtonIcon || 'fas fa-plus'} mr-1"></i>${field.addLabel || 'Добавить'}
                        </button>`;
                    return buildFieldWrapper(field, control, options);
                }
                default:
                    return buildFieldWrapper(field, `<pre class="text-xs">${JSON.stringify(value ?? {}, null, 2)}</pre>`, options);
            }
        }).join('');
    },

    initForm(container, fields, data, context = {}, options = {}, onChange) {
        if (!container) return;
        const fieldMap = new Map(fields.filter((field) => field.key).map((field) => [field.key, field]));

        const updateVisibility = () => {
            const values = {};
            fields.forEach((field) => {
                if (!field.key || field.type === 'divider') return;
                values[field.key] = this.getFieldValue(container, field);
            });
            fields.forEach((field) => {
                if (!field.showWhen || !field.key) return;
                const wrapper = container.querySelector(`[data-field-key="${field.key}"]`);
                if (!wrapper) return;
                const isVisible = evaluateShowWhen(field.showWhen, values);
                wrapper.classList.toggle('hidden', !isVisible);
            });
        };

        const handleListUpdate = () => {
            this.reindexList(container, options);
            updateVisibility();
            if (onChange) onChange();
        };

        container.addEventListener('click', (event) => {
            const addButton = event.target.closest('[data-list-add]');
            if (addButton) {
                const key = addButton.dataset.listAdd;
                const field = fieldMap.get(key);
                if (field) {
                    const listContainer = container.querySelector(`[data-list-key="${key}"]`);
                    if (listContainer) {
                        const newItem = resolveDefaultListItem(field);
                        const index = listContainer.querySelectorAll('[data-list-item]').length;
                        listContainer.insertAdjacentHTML('beforeend', renderListItem(field, newItem, index, options));
                        handleListUpdate();
                    }
                }
                return;
            }

            const removeButton = event.target.closest('[data-list-remove]');
            if (removeButton) {
                const key = removeButton.dataset.listRemove;
                const index = Number(removeButton.dataset.listIndex);
                const listContainer = container.querySelector(`[data-list-key="${key}"]`);
                const itemEl = listContainer?.querySelector(`[data-list-item="${index}"]`);
                if (itemEl) {
                    itemEl.remove();
                    handleListUpdate();
                }
            }
        });

        const handleInputChange = () => {
            updateVisibility();
            if (onChange) onChange();
        };

        container.addEventListener('input', handleInputChange);
        container.addEventListener('change', handleInputChange);

        updateVisibility();
    },

    reindexList(container) {
        container.querySelectorAll('[data-list-key]').forEach((listContainer) => {
            Array.from(listContainer.querySelectorAll('[data-list-item]')).forEach((item, index) => {
                item.dataset.listItem = String(index);
                item.querySelectorAll('[data-list-index]').forEach((input) => {
                    input.dataset.listIndex = String(index);
                });
                const removeButton = item.querySelector('[data-list-remove]');
                if (removeButton) {
                    removeButton.dataset.listIndex = String(index);
                }
            });
        });
    },

    getFieldValue(container, field) {
        if (field.type === 'list') {
            const listContainer = container.querySelector(`[data-list-key="${field.key}"]`);
            const items = [];
            if (!listContainer) return items;
            listContainer.querySelectorAll('[data-list-item]').forEach((itemEl) => {
                const item = {};
                (field.itemFields || []).forEach((subField) => {
                    const input = itemEl.querySelector(`[data-list-prop="${subField.key}"]`);
                    if (!input) return;
                    const value = input.type === 'checkbox' ? input.checked : input.value;
                    const parsed = parseValue(subField, value);
                    if (parsed !== undefined) {
                        setValueByPath(item, subField.key, parsed);
                    }
                });
                items.push(item);
            });
            return items;
        }
        if (field.type === 'vec2') {
            const xInput = container.querySelector(`[name="${field.key}.x"]`);
            const yInput = container.querySelector(`[name="${field.key}.y"]`);
            return {
                x: parseValue({ type: 'number', allowEmpty: field.allowEmpty }, xInput?.value ?? 0),
                y: parseValue({ type: 'number', allowEmpty: field.allowEmpty }, yInput?.value ?? 0),
            };
        }
        if (field.type === 'item') {
            const idInput = container.querySelector(`[name="${field.key}.id"]`);
            const valueInput = container.querySelector(`[name="${field.key}.value"]`);
            return {
                id: parseValue({ type: 'number', allowEmpty: field.allowEmpty }, idInput?.value ?? 0),
                value: parseValue({ type: 'number', allowEmpty: field.allowEmpty }, valueInput?.value ?? 0),
            };
        }
        const input = container.querySelector(`[name="${field.key}"]`);
        if (!input) return undefined;
        if (field.type === 'boolean') {
            return input.checked;
        }
        return parseValue(field, input.value);
    },

    collectFormData(container, fields, data, context = {}) {
        const nextData = JSON.parse(JSON.stringify(data));
        const errors = [];
        const values = {};
        fields.forEach((field) => {
            if (!field.key || field.type === 'divider') return;
            const value = this.getFieldValue(container, field);
            values[field.key] = value;
        });

        fields.forEach((field) => {
            if (!field.key || field.type === 'divider') return;
            const isVisible = evaluateShowWhen(field.showWhen, values);
            if (!isVisible && field.clearWhenHidden) {
                unsetValueByPath(nextData, field.key);
                return;
            }
            let value = values[field.key];
            if (field.type === 'select' && field.allowEmpty && (value === '' || value === undefined)) {
                if (field.clearIfEmpty) {
                    unsetValueByPath(nextData, field.key);
                    return;
                }
                value = field.emptyValue ?? undefined;
            }
            if (value !== undefined) {
                setValueByPath(nextData, field.key, value);
            }
        });

        fields.forEach((field) => {
            if (!field.validators || !field.key) return;
            const value = values[field.key];
            const isVisible = evaluateShowWhen(field.showWhen, values);
            if (!isVisible) return;
            field.validators.forEach((validator) => {
                if (validator.type === 'required') {
                    const empty = value === undefined || value === null || value === '' || (Array.isArray(value) && value.length === 0);
                    if (empty) {
                        errors.push({ key: field.key, message: validator.message || 'Поле обязательно.' });
                    }
                }
                if (validator.type === 'min' && typeof value === 'number' && value < validator.value) {
                    errors.push({ key: field.key, message: validator.message || `Минимум ${validator.value}.` });
                }
                if (validator.type === 'max' && typeof value === 'number' && value > validator.value) {
                    errors.push({ key: field.key, message: validator.message || `Максимум ${validator.value}.` });
                }
            });
        });

        return { data: nextData, errors };
    },

    applyValidation(container, errors, options = {}) {
        container.querySelectorAll('[data-form-error]').forEach((el) => el.remove());
        container.querySelectorAll('[data-field-key]').forEach((wrapper) => wrapper.classList.remove(options.errorClass || 'ring-1 ring-red-400'));
        errors.forEach((error) => {
            const wrapper = container.querySelector(`[data-field-key="${error.key}"]`);
            if (!wrapper) return;
            wrapper.classList.add(options.errorClass || 'ring-1 ring-red-400');
            const errorEl = document.createElement('div');
            errorEl.textContent = error.message;
            errorEl.dataset.formError = error.key;
            errorEl.className = options.errorTextClass || 'text-xs text-red-500 mt-2';
            wrapper.appendChild(errorEl);
        });
    },
};

export const scenarioComponentRegistry = {
    message: {
        name: 'Сообщение',
        class: 'info',
        icon: 'fa-solid fa-comment-dots',
        desc: 'Показать текст игроку',
        default: { type: 'message', text: 'Новое сообщение', mode: 'full' },
        fields: [
            {
                key: 'mode',
                label: 'Тип сообщения',
                type: 'select',
                options: [
                    { value: 'full', label: 'Все (Броадкаст и чат)' },
                    { value: 'broadcast', label: 'Броадкаст' },
                    { value: 'chat', label: 'Чат' },
                ],
            },
            {
                key: 'text',
                label: 'Текст сообщения',
                type: 'text',
                multiline: true,
                rows: 4,
                validators: [{ type: 'required', message: 'Введите текст сообщения.' }],
            },
        ],
    },
    wait: {
        name: 'Ожидание',
        class: 'info',
        icon: 'fa-solid fa-clock',
        desc: 'Пауза в сценарии',
        default: { type: 'wait', duration: 5 },
        fields: [
            { key: 'duration', label: 'Длительность (в секундах)', type: 'number', validators: [{ type: 'min', value: 0 }] },
        ],
    },
    door_control: {
        name: 'Управление дверью',
        class: 'interactive',
        icon: 'fa-solid fa-door-open',
        desc: 'Создать или удалить дверь',
        default: { type: 'door_control', action: 'create', position: { x: 100, y: 100 }, key: '' },
        fields: [
            {
                key: 'action',
                label: 'Действие',
                type: 'select',
                options: [
                    { value: 'create', label: 'Создать' },
                    { value: 'remove', label: 'Удалить' },
                ],
            },
            {
                key: 'key',
                label: 'Ключ/Имя двери (key)',
                type: 'door_key',
                placeholder: 'Введите или выберите ключ...',
                validators: [{ type: 'required', message: 'Укажите ключ двери.' }],
            },
            {
                key: 'position',
                label: 'Позиция двери',
                type: 'vec2',
                showWhen: { key: 'action', not: 'remove' },
                clearWhenHidden: true,
            },
        ],
    },
    use_chat_code: {
        name: 'Код в чате',
        class: 'interactive',
        icon: 'fa-solid fa-key',
        desc: 'Переход по кодовому слову',
        default: { type: 'use_chat_code', code: 'secret', next_step_id: '', hidden: false },
        fields: [
            { key: 'code', label: 'Кодовое слово', type: 'text', validators: [{ type: 'required', message: 'Введите кодовое слово.' }] },
            { key: 'hidden', label: 'Скрыть ключевое слово', type: 'boolean' },
        ],
    },
    defeat_mobs: {
        name: 'Убийство мобов',
        class: 'combat',
        icon: 'fa-solid fa-skull-crossbones',
        desc: 'Боевое испытание',
        default: { type: 'defeat_mobs', mode: 'annihilation', radius: 100, position: { x: 0, y: 0 }, mobs: [{ mob_id: 21, count: 5, level: 1, power: 1, boss: false }] },
        fields: [
            { key: 'radius', label: 'Радиус (radius)', type: 'number', validators: [{ type: 'min', value: 0 }] },
            { key: 'position', label: 'Позиция боя', type: 'vec2' },
            {
                key: 'mode',
                label: 'Режим',
                type: 'select',
                options: [
                    { value: 'annihilation', label: 'Уничтожение' },
                    { value: 'wave', label: 'Волна' },
                    { value: 'survival', label: 'Выживание' },
                ],
            },
            {
                key: 'kill_target',
                label: 'Цель убийств (для "волны")',
                type: 'number',
                showWhen: { key: 'mode', equals: 'wave' },
                clearWhenHidden: true,
                validators: [{ type: 'min', value: 1 }],
            },
            {
                key: 'duration',
                label: 'Длительность (сек, для "выживания")',
                type: 'number',
                showWhen: { key: 'mode', equals: 'survival' },
                clearWhenHidden: true,
                validators: [{ type: 'min', value: 1 }],
            },
            {
                key: 'mobs',
                label: 'Список мобов',
                type: 'list',
                showHeader: true,
                addLabel: 'Добавить моба',
                itemFields: [
                    { key: 'mob_id', label: 'ID Моба', type: 'number', default: 21 },
                    { key: 'count', label: 'Количество', type: 'number', default: 1 },
                    { key: 'level', label: 'Уровень', type: 'number', default: 1 },
                    { key: 'power', label: 'Сила', type: 'number', default: 1 },
                    { key: 'boss', label: 'Босс', type: 'boolean', default: false },
                ],
            },
        ],
    },
    follow_camera: {
        name: 'Следование камеры',
        class: 'camera',
        icon: 'fa-solid fa-video',
        desc: 'Переместить камеру к точке',
        default: { type: 'follow_camera', position: { x: 0, y: 0 }, delay: 300, smooth: true },
        fields: [
            { key: 'position', label: 'Позиция камеры', type: 'vec2' },
            { key: 'smooth', label: 'Плавное перемещение', type: 'boolean' },
        ],
    },
    condition_movement: {
        name: 'Условие: Движение',
        class: 'interactive',
        icon: 'fa-solid fa-person-running',
        desc: 'Переход при достижении точки',
        default: { type: 'condition_movement', position: { x: 0, y: 0 }, entire_group: true },
        fields: [
            { key: 'position', label: 'Позиция точки', type: 'vec2' },
            { key: 'entire_group', label: 'Для всей группы', type: 'boolean' },
        ],
    },
    teleport: {
        name: 'Телепорт',
        class: 'interactive',
        icon: 'fa-solid fa-bolt',
        desc: 'Мгновенное перемещение',
        default: { type: 'teleport', position: { x: 1497, y: 529 } },
        fields: [
            { key: 'position', label: 'Позиция телепорта', type: 'vec2' },
        ],
    },
    activate_point: {
        name: 'Точка активации',
        class: 'interactive',
        icon: 'fa-solid fa-location-dot',
        desc: 'Активация по времени в области',
        default: { type: 'activate_point', position: { x: 400, y: 3000 }, duration: 5, entire_group: true, action_text: 'Перегрузка главного реактора' },
        fields: [
            { key: 'action_text', label: 'Текст действия (action_text)', type: 'text' },
            { key: 'duration', label: 'Длительность активации (сек)', type: 'number', validators: [{ type: 'min', value: 1 }] },
            { key: 'position', label: 'Позиция точки', type: 'vec2' },
            { key: 'entire_group', label: 'Для всей группы', type: 'boolean' },
        ],
    },
};

export const eventActionRegistry = {
    group_header: {
        name: 'Заголовок группы',
        icon: 'fa-layer-group',
        fields: [{ key: 'name', label: 'Название группы', type: 'text', validators: [{ type: 'required', message: 'Введите название группы.' }] }],
    },
    message: {
        name: 'Сообщение',
        icon: 'fa-comment',
        fields: [
            { key: 'delay', label: 'Задержка', type: 'number', validators: [{ type: 'min', value: 0 }] },
            { key: 'chat', label: 'Чат', type: 'text', multiline: true },
            { key: 'broadcast', label: 'Броадкаст', type: 'text', multiline: true },
            { key: 'full', label: 'Полный текст', type: 'text', multiline: true },
        ],
    },
    movement_task: {
        name: 'Задача на движение',
        icon: 'fa-person-running',
        fields: [
            { key: 'delay', label: 'Задержка', type: 'number', validators: [{ type: 'min', value: 0 }] },
            { key: 'position', label: 'Позиция', type: 'vec2', step: 0.1 },
            { key: 'target_lock_text', label: 'Текст фиксации', type: 'text' },
            { key: 'target_look', label: 'Смотреть на цель', type: 'boolean' },
            { key: 'chat', label: 'Чат', type: 'text', multiline: true },
            { key: 'broadcast', label: 'Броадкаст', type: 'text', multiline: true },
            { key: 'full', label: 'Полный текст', type: 'text', multiline: true },
        ],
    },
    check_has_item: {
        name: 'Проверить предмет',
        icon: 'fa-box',
        fields: [
            { key: 'item_id', label: 'ID предмета', type: 'number' },
            { key: 'required', label: 'Количество', type: 'number', validators: [{ type: 'min', value: 0 }] },
            { key: 'remove', label: 'Удалить после проверки', type: 'boolean' },
            { key: 'show_progress', label: 'Показывать прогресс', type: 'boolean' },
        ],
    },
    reset_quest: {
        name: 'Сбросить квест',
        icon: 'fa-undo',
        fields: [{ key: 'quest_id', label: 'ID квеста', type: 'number' }],
    },
    accept_quest: {
        name: 'Принять квест',
        icon: 'fa-check-double',
        fields: [{ key: 'quest_id', label: 'ID квеста', type: 'number' }],
    },
    new_door: {
        name: 'Создать дверь',
        icon: 'fa-door-closed',
        fields: [
            { key: 'key', label: 'Ключ двери', type: 'door_key' },
            { key: 'follow', label: 'Следовать за игроком', type: 'boolean' },
            { key: 'position', label: 'Позиция', type: 'vec2', step: 0.1 },
        ],
    },
    remove_door: {
        name: 'Удалить дверь',
        icon: 'fa-door-open',
        fields: [
            { key: 'key', label: 'Ключ двери', type: 'door_key' },
            { key: 'follow', label: 'Следовать за игроком', type: 'boolean' },
        ],
    },
    pick_item_task: {
        name: 'Задача на подбор',
        icon: 'fa-hand-sparkles',
        fields: [
            { key: 'position', label: 'Позиция', type: 'vec2', step: 0.1 },
            { key: 'item', label: 'Предмет', type: 'item' },
            { key: 'chat', label: 'Чат', type: 'text', multiline: true },
            { key: 'broadcast', label: 'Броадкаст', type: 'text', multiline: true },
            { key: 'full', label: 'Полный текст', type: 'text', multiline: true },
        ],
    },
    emote: {
        name: 'Эмоция',
        icon: 'fa-smile',
        fields: [
            { key: 'emote_type', label: 'Тип эмоции', type: 'number' },
            { key: 'emoticon_type', label: 'Тип эмоции (иконка)', type: 'number' },
        ],
    },
    teleport: {
        name: 'Телепорт',
        icon: 'fa-plane-departure',
        fields: [
            { key: 'position', label: 'Позиция', type: 'vec2', step: 0.1 },
            { key: 'world_id', label: 'ID мира', type: 'number' },
        ],
    },
    use_chat_task: {
        name: 'Задача на чат',
        icon: 'fa-keyboard',
        fields: [{ key: 'chat', label: 'Текст', type: 'text', multiline: true }],
    },
    fix_cam: {
        name: 'Фиксировать камеру',
        icon: 'fa-camera',
        fields: [
            { key: 'delay', label: 'Задержка', type: 'number', validators: [{ type: 'min', value: 0 }] },
            { key: 'position', label: 'Позиция', type: 'vec2', step: 0.1 },
        ],
    },
    freeze_movements: {
        name: 'Заморозить движение',
        icon: 'fa-snowflake',
        fields: [{ key: 'state', label: 'Включить заморозку', type: 'boolean' }],
    },
    check_quest_accepted: {
        name: 'Проверка: квест принят',
        icon: 'fa-question-circle',
        fields: [{ key: 'quest_id', label: 'ID квеста', type: 'number' }],
    },
    check_quest_finished: {
        name: 'Проверка: квест завершен',
        icon: 'fa-flag-checkered',
        fields: [{ key: 'quest_id', label: 'ID квеста', type: 'number' }],
    },
    check_quest_step_finished: {
        name: 'Проверка: шаг квеста завершен',
        icon: 'fa-list-check',
        fields: [
            { key: 'quest_id', label: 'ID квеста', type: 'number' },
            { key: 'step', label: 'Шаг', type: 'number' },
        ],
    },
    shootmarkers: {
        name: 'Стрельба по маркерам',
        icon: 'fa-crosshairs',
        fields: [
            {
                key: 'markers',
                label: 'Маркеры',
                type: 'list',
                showHeader: true,
                addLabel: 'Добавить маркер',
                itemFields: [
                    { key: 'position.x', label: 'Position X', type: 'number', step: 0.1 },
                    { key: 'position.y', label: 'Position Y', type: 'number', step: 0.1 },
                    { key: 'health', label: 'Health', type: 'number', default: 1 },
                ],
            },
        ],
    },
};
