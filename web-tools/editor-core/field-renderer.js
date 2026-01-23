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
      } catch { /* ignore */ }
      // Fallback for legacy formats (CSV)
      return raw.split(',').map(s => s.trim()).filter(Boolean);
    }

    // Same as json_array, but coerces values to numbers.
    // Useful for tags sourced from DB where stored values are numeric IDs.
    if (input.dataset?.valueType === 'json_array_number') {
      const raw = String(input.value || '').trim();
      if (!raw) return [];
      try {
        const arr = JSON.parse(raw);
        if (Array.isArray(arr)) {
          return arr
            .map((v) => (v === '' || v == null ? NaN : Number(v)))
            .filter((n) => Number.isFinite(n));
        }
      } catch { /* ignore */ }
      // Fallback for legacy formats (CSV)
      return raw
        .split(',')
        .map((s) => Number(s.trim()))
        .filter((n) => Number.isFinite(n));
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

  const normalizeCssSize = (v) => {
    if (v == null) return '';
    const s = String(v).trim();
    if (!s) return '';
    // Allow numbers as px for convenience
    if (/^\d+(?:\.\d+)?$/.test(s)) return `${s}px`;
    return s;
  };

  // Generic per-control sizing API.
  // Can be used by input/select/textarea/db_select widgets.
  // Supported ui props:
  //  - controlWidth / controlMinWidth / controlMaxWidth
  //  - controlFlex / controlGrow / controlShrink / controlBasis
  //  - controlStyle (raw CSS string)
  const buildControlStyleAttr = (ui = {}) => {
    const parts = [];
    const w = normalizeCssSize(ui.controlWidth);
    const minW = normalizeCssSize(ui.controlMinWidth);
    const maxW = normalizeCssSize(ui.controlMaxWidth);
    const basis = normalizeCssSize(ui.controlBasis);

    if (w) parts.push(`width:${w}`);
    if (minW) parts.push(`min-width:${minW}`);
    if (maxW) parts.push(`max-width:${maxW}`);

    if (ui.controlFlex) {
      parts.push(`flex:${String(ui.controlFlex)}`);
    } else {
      if (ui.controlGrow !== undefined) parts.push(`flex-grow:${Number(ui.controlGrow)}`);
      if (ui.controlShrink !== undefined) parts.push(`flex-shrink:${Number(ui.controlShrink)}`);
      if (basis) parts.push(`flex-basis:${basis}`);
    }

    if (ui.controlStyle) parts.push(String(ui.controlStyle));
    const style = parts.filter(Boolean).join(';');
    return style ? `style="${escapeAttr(style)}"` : '';
  };

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

  // For multiselect fields we accept both array values and common DB formats:
  // - comma-separated string (MySQL SET)
  // - JSON array string
  const normalizeMultiValue = (v) => {
    if (Array.isArray(v)) {
      return v.map(x => String(x).trim()).filter(Boolean);
    }
    if (v == null) return [];
    const s = String(v).trim();
    if (!s) return [];
    // JSON array fallback
    if (s[0] === '[') {
      try {
        const arr = JSON.parse(s);
        if (Array.isArray(arr)) return arr.map(x => String(x).trim()).filter(Boolean);
      } catch { /* ignore */ }
    }
    return s.split(',').map(x => String(x).trim()).filter(Boolean);
  };

  const renderSelectOptions = (options, value, multiple = false) => {
    const values = multiple ? normalizeMultiValue(value).map(String) : null;
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
      const controlClass = ui.controlClass ? ` ${String(ui.controlClass)}` : '';
      const styleAttr = buildControlStyleAttr(ui);
      const attrs = buildInputAttributes({
        path,
        field,
        classes: `${inputClass}${controlClass}`,
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: `${styleAttr} ${extraAttrs}`.trim()
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
      const controlClass = ui.controlClass ? ` ${String(ui.controlClass)}` : '';
      const styleAttr = buildControlStyleAttr(ui);
      const attrs = buildInputAttributes({
        path,
        field,
        classes: `${textareaClass}${controlClass}`,
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: styleAttr
      });
      return `<textarea ${attrs}>${value ?? ''}</textarea>`;
    };

    const renderSelect = (multiple = false) => {
      const controlClass = ui.controlClass ? ` ${String(ui.controlClass)}` : '';
      const styleAttr = buildControlStyleAttr(ui);
      const attrs = buildInputAttributes({
        path,
        field,
        classes: `${multiple ? multiSelectClass : inputClass}${controlClass}`,
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: `${styleAttr} ${multiple ? 'multiple' : ''}`.trim()
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

    
    // Legacy UI: Search input (left) + <select> (right).
    // Kept as a separate UI component: db_select_search.
    const renderDbSelectSearch = (multiple = false) => {
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
      // UI: place Search (left) + Select (right) in one row.
      // We set an optimistic default state="connected" when datasource exists, to avoid
      // an initial "double" render (manual input + select) before DB.init() flips state.
      const state = ds ? 'connected' : '';
      const wrapClass = ui.controlWrapClass ? ` ${String(ui.controlWrapClass)}` : '';
      const wrapStyle = buildControlStyleAttr(ui);
      const rowHtml = `<div class="editor-dbselect-row">${searchHtml}${selectHtml}</div>`;
      return `<div class="editor-dbselect${wrapClass}" ${wrapStyle} ${state ? `data-db-state="${state}"` : ''}>${rowHtml}${manualInputHtml}${controlsHtml}</div>`;
    };

    // New default UI: single searchable input with dropdown list.
    // Visually it's one field (input). Old two-field layout remains available as db_select_search.
    const renderDbSelectCombo = () => {
      const { ds, dbKey, placeholder, labelMode } = resolveDbParams();
      if (!ds) return renderInput('number', value ?? 0);

      const wrapClass = ui.controlWrapClass ? ` ${String(ui.controlWrapClass)}` : '';
      const wrapStyle = buildControlStyleAttr(ui);

      // Similar defaults to legacy select.
      const serverSearch = (ui.searchServer ?? (dbKey === 'item'));
      const searchable = serverSearch ? '1' : '0';
      const defaultLimit = (dbKey === 'item') ? 1000 : 300;
      const dbLimit = String(ui.dbLimit || defaultLimit);

      // Stability feature (same as before): keep a bound numeric input as fallback.
      // When DB is connected, the input is hidden and the combo drives it.
      const manualInputAttrs = buildInputAttributes({
        path,
        field,
        classes: `${inputClass} editor-dbselect-input editor-dbcombo-bound`,
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: 'inputmode="numeric"'
      });
      const cur = (value ?? 0);

      const searchPh = ui.searchPlaceholder || ui.placeholder || 'Поиск…';
      const state = ds ? 'connected' : '';
      const hasVal = (v) => v !== undefined && v !== null && String(v) !== '' && String(v) !== '0';
      const curVal = hasVal(value) ? String(value) : '';

      return `
        <div class="editor-dbcombo${wrapClass}" ${wrapStyle} data-db-state="${escapeAttr(state)}"
             data-datasource="${escapeAttr(ds)}"
             data-db-searchable="${escapeAttr(searchable)}"
             data-db-limit="${escapeAttr(dbLimit)}"
             data-placeholder="${escapeAttr(placeholder)}"
             data-label-mode="${escapeAttr(String(labelMode))}"
             data-bind-input-path="${escapeAttr(path)}"
             data-current-value="${escapeAttr(curVal)}">
          <div class="editor-dbcombo-control">
            <input type="search" class="${inputClass} editor-dbcombo-input" placeholder="${escapeAttr(searchPh)}" autocomplete="off" />
          </div>
          <div class="editor-dbcombo-dropdown" role="listbox" aria-label="${escapeAttr(label)}"></div>
          <input type="number" ${manualInputAttrs} value="${escapeAttr(String(cur))}" />
        </div>`;
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
      // Responsive: stack on narrow screens
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}<div class="grid grid-cols-1 sm:grid-cols-2 gap-2">${renderComposite(vecFields)}</div>${hintHtml}</div>`;
    }

    if (field.type === 'item') {
      const itemDs = ui.datasource || field.datasource || null;
      const itemFields = {
        id: { type: 'db_select', label: 'Предмет', datasource: itemDs || undefined, ui: { dbKey: ui.dbKey || 'item', placeholder: ui.placeholder || '— выберите предмет —', inputLabel: 'ID', selectLabel: 'Предмет (БД)' } },
        value: { type: 'number', label: 'Value', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } }
      };
      // Responsive: stack on narrow screens
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}<div class="grid grid-cols-1 sm:grid-cols-2 gap-2">${renderComposite(itemFields)}</div>${hintHtml}</div>`;
    }

    if (field.type === 'list') {
      const items = Array.isArray(value) ? value : [];
      const itemFields = field.itemFields || {};
      const hideLabel = !!(field?.ui?.hideLabel);
      const layout = (field?.ui?.layout || 'stack').toLowerCase();
      const isGrid = layout === 'grid';
      const layoutClass = isGrid ? 'editor-grid-fields' : 'space-y-3';
      const gridStyleParts = [];
      if (isGrid) {
        if (field?.ui?.gridGap) gridStyleParts.push(`--editor-grid-gap:${String(field.ui.gridGap)}`);
        if (field?.ui?.gridTemplate) gridStyleParts.push(`--editor-grid-template:${String(field.ui.gridTemplate)}`);
        if (field?.ui?.gridTemplateMd) gridStyleParts.push(`--editor-grid-template-md:${String(field.ui.gridTemplateMd)}`);
        if (field?.ui?.gridTemplateLg) gridStyleParts.push(`--editor-grid-template-lg:${String(field.ui.gridTemplateLg)}`);
      }
      const gridStyleAttr = gridStyleParts.length ? ` style="${escapeAttr(gridStyleParts.join(';'))}"` : '';
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
            <div class="${layoutClass}"${gridStyleAttr}>
              ${isGrid
                ? Object.entries(itemFields).map(([key, subField]) => {
                    const subPath = `${itemPath}.${key}`;
                    const span = Number(subField?.ui?.colSpan || 1);
                    const spanMd = Number(subField?.ui?.colSpanMd || span);
                    const spanLg = Number(subField?.ui?.colSpanLg || spanMd);
                    let cls = '';
                    if (span > 1) cls += ` col-span-${span}`;
                    if (spanMd > 1) cls += ` md:col-span-${spanMd}`;
                    if (spanLg > 1) cls += ` lg:col-span-${spanLg}`;
                    const inner = renderField({ path: subPath, field: subField, data, options, isNested: true });
                    return cls.trim() ? `<div class="${cls.trim()}">${inner}</div>` : inner;
                  }).join('')
                : itemContent}
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

    // New default db_select: single searchable combo input.
    if (field.type === 'db_select' || ui.type === 'db_select') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderDbSelectCombo()}${hintHtml}</div>`;
    }

    // Legacy two-field variant (search + select)
    if (field.type === 'db_select_search' || ui.type === 'db_select_search') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderDbSelectSearch()}${hintHtml}</div>`;
    }

    if (field.type === 'db_multiselect' || ui.type === 'db_multiselect') {
      // Multiselect keeps the legacy layout for now.
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderDbSelectSearch(true)}${hintHtml}</div>`;
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

      // Optional DB source for options.
      // You can pass:
      // - ui.datasource: DBMap key (e.g. 'item', 'mob') OR direct source name (e.g. 'items')
      // - ui.labelMode: 'id_name' | 'name' (defaults from DBMap if available)
      // - ui.valueType: 'string' | 'number' (controls how values are parsed)
      const dsRaw = String(ui.datasource || ui.dbKey || '').trim();
      const dsKey = dsRaw;
      const dsResolved = dsRaw && window.EditorCore?.DB?.resolveSource ? (window.EditorCore.DB.resolveSource(dsRaw) || dsRaw) : dsRaw;
      const labelMode = String(ui.labelMode || (window.EditorCore?.DBMap?.[dsKey]?.labelMode) || 'id_name').toLowerCase();
      const valueType = String(ui.valueType || 'string').toLowerCase();
      const hiddenValueType = valueType === 'number' ? 'json_array_number' : 'json_array';

      const hiddenAttrs = buildInputAttributes({
        path,
        field,
        classes: '',
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: `type="hidden" data-value-type="${escapeAttr(hiddenValueType)}"`
      });

      // Keep current value visible for first render.
      const initial = JSON.stringify(normalizeMultiValue(value));
      return `
        <div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">
          ${labelHtml}
          <div class="editor-tags"
               data-tags-options="${escapeAttr(JSON.stringify(opts))}"
               data-tags-placeholder="${escapeAttr(ph)}"
               data-tags-allow-create="${allowCreate}"
               ${dsResolved ? `data-tags-datasource="${escapeAttr(dsResolved)}"` : ''}
               ${dsResolved ? `data-tags-label-mode="${escapeAttr(labelMode)}"` : ''}
               data-tags-value-type="${escapeAttr(valueType)}">
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
