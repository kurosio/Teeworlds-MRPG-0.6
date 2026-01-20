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
    if (input.multiple) {
      return Array.from(input.selectedOptions).map((option) => option.value);
    }
    if (input.type === 'checkbox') {
      return input.checked;
    }
    if (input.type === 'number') {
      return input.value === '' ? 0 : parseFloat(input.value);
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
    const inputClass = options?.classes?.input || '';
    const baseLabelClass = options?.classes?.label || '';
    const nestedLabelClass = options?.classes?.nestedLabel || baseLabelClass;
    const labelClass = isNested ? nestedLabelClass : baseLabelClass;
    const fieldWrapperClass = options?.classes?.fieldWrapper || '';
    const checkboxWrapperClass = options?.classes?.checkboxWrapper || '';
    const listItemClass = options?.classes?.listItem || '';
    const listWrapperClass = options?.classes?.listWrapper || '';
    const listAddClass = options?.classes?.listAdd || '';
    const listRemoveClass = options?.classes?.listRemove || '';
    const multiSelectClass = options?.classes?.multiselect || inputClass;
    const textareaClass = options?.classes?.textarea || inputClass;

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
      return `<div class="${fieldWrapperClass}">${renderCheckbox()}</div>`;
    }

    if (field.type === 'vec2') {
      const vecFields = {
        x: { type: 'number', label: 'X', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } },
        y: { type: 'number', label: 'Y', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } }
      };
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}"><div class="${labelClass}">${label}</div><div class="grid grid-cols-2 gap-2">${renderComposite(vecFields)}</div></div>`;
    }

    if (field.type === 'item') {
      const itemFields = {
        id: { type: 'number', label: 'ID', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } },
        value: { type: 'number', label: 'Value', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } }
      };
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}"><div class="${labelClass}">${label}</div><div class="grid grid-cols-2 gap-2">${renderComposite(itemFields)}</div></div>`;
    }

    if (field.type === 'list') {
      const items = Array.isArray(value) ? value : [];
      const itemFields = field.itemFields || {};
      const listItems = items.map((item, index) => {
        const itemPath = `${path}.${index}`;
        const itemContent = Object.entries(itemFields).map(([key, subField]) => {
          return renderField({ path: `${itemPath}.${key}`, field: subField, data, options, isNested: true });
        }).join('');
        return `
          <div class="${listItemClass}" data-list-item="${escapeAttr(itemPath)}">
            ${itemContent}
            <button type="button" class="${listRemoveClass}" data-list-action="remove" data-list-path="${escapeAttr(path)}" data-list-index="${index}">Удалить</button>
          </div>`;
      }).join('');
      const defaultItem = buildDefaultItem(field);
      const defaultPayload = encodeURIComponent(JSON.stringify(defaultItem));
      return `
        <div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">
          <div class="${labelClass}">${label}</div>
          <div class="${listWrapperClass}" data-list-container="${escapeAttr(path)}">${listItems}</div>
          <button type="button" class="${listAddClass}" data-list-action="add" data-list-path="${escapeAttr(path)}" data-list-default="${defaultPayload}">${options?.listAddLabel || 'Добавить'}</button>
        </div>`;
    }

    if (ui.type === 'select') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}"><div class="${labelClass}">${label}</div>${renderSelect(false)}</div>`;
    }

    if (ui.type === 'multiselect') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}"><div class="${labelClass}">${label}</div>${renderSelect(true)}</div>`;
    }

    if (ui.format === 'date' || field.type === 'date') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}"><div class="${labelClass}">${label}</div>${renderInput('date', value)}</div>`;
    }

    if (ui.format === 'textarea' || ui.type === 'textarea') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}"><div class="${labelClass}">${label}</div>${renderTextarea()}</div>`;
    }

    if (field.type === 'number') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}"><div class="${labelClass}">${label}</div>${renderInput('number', value ?? 0)}</div>`;
    }

    return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}"><div class="${labelClass}">${label}</div>${renderInput('text', value ?? '')}</div>`;
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
