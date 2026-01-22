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
      const hasVal = (v) => v !== undefined && v !== null && String(v) !== '' && String(v) !== '0';
      let initialOptions = '';
      if (multiple) {
        const arr = Array.isArray(value) ? value : [];
        const uniq = Array.from(new Set(arr.map(x => String(x)).filter(hasVal)));
        // In multiselect we don't show an empty placeholder option (it is confusing).
        initialOptions = uniq.length
          ? uniq.map(id => `<option value="${escapeAttr(id)}" selected>${escapeAttr(id)}: …</option>`).join('')
          : '';
      } else {
        const cur = hasVal(value) ? String(value) : '';
        initialOptions = `<option value="">${escapeAttr(placeholder)}</option>`;
        if (cur) initialOptions += `<option value="${escapeAttr(cur)}" selected>${escapeAttr(cur)}: …</option>`;
      }

      // Keep initial selection visible even before DB options are loaded.
      // DB.init() will replace the "…"-label with a real name.
      return `<select ${attrs} data-current-value="${escapeAttr(String(value ?? ''))}" ${multiple ? `data-current-values="${escapeAttr(JSON.stringify(Array.isArray(value)?value:[]))}"` : ``}>${initialOptions}</select>`;
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

      // IMPORTANT:
      // Do NOT force an initial "loading" state here.
      // If we set data-db-state="loading" immediately, CSS hides the <select>
      // and the user may never see dropdown lists if DB state is not flipped.
      // DB.init() will set data-db-state to "connected" / "disconnected".
      const state = '';
      return `<div class="editor-dbselect" ${state ? `data-db-state="${state}"` : ''}>${searchHtml}${manualInputHtml}${selectHtml}${controlsHtml}</div>`;
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
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}<div class="grid grid-cols-2 gap-2">${renderComposite(vecFields)}</div>${hintHtml}</div>`;
    }

    if (field.type === 'item') {
      const itemDs = ui.datasource || field.datasource || null;
      const itemFields = {
        id: { type: 'db_select', label: 'Предмет', datasource: itemDs || undefined, ui: { dbKey: ui.dbKey || 'item', placeholder: ui.placeholder || '— выберите предмет —', inputLabel: 'ID', selectLabel: 'Предмет (БД)' } },
        value: { type: 'number', label: 'Value', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } }
      };
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}<div class="grid grid-cols-2 gap-2">${renderComposite(itemFields)}</div>${hintHtml}</div>`;
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
