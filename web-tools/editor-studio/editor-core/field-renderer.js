(() => {
  const { parsePath, getAtPath, setAtPath, cloneData } = window.EditorCore.utils;
  const getValueAtPath = getAtPath;
  const setValueAtPath = setAtPath;
  const cloneValue = cloneData;

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

  const escapeAttr = (...args) => window.EditorCore.utils.escapeAttr(...args);
  const escapeHtml = (...args) => window.EditorCore.utils.escapeHtml(...args);
  const t = (text, vars) => (window.EditorCore?.Page?.t
    ? window.EditorCore.Page.t(text, vars)
    : String(text ?? ''));

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
    if (field?.ui?.relatedListSource) attrs.push(`data-related-list-source="${escapeAttr(field.ui.relatedListSource)}"`);
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

    // field visibility
    // supports:
    //  - ui.showWhen: boolean | function(data, ctx) => boolean
    //  - field.showWhen / field.visible: same as above
    //  - ui.showWhen: { path, equals } | { path, in: [] }
    const isFieldVisible = (() => {
      const spec = ui?.showWhen ?? field?.showWhen ?? field?.visible;
      if (spec === undefined) return true;
      const ctx = { path, field, value };
      try {
        if (typeof spec === 'function') return !!spec(data, ctx);
        if (typeof spec === 'boolean') return spec;
        if (spec && typeof spec === 'object') {
          const p = spec.path;
          if (!p) return true;
          const cur = getValueAtPath(data, p);
          if (Object.prototype.hasOwnProperty.call(spec, 'equals')) {
            return String(cur) === String(spec.equals);
          }
          if (Array.isArray(spec.in)) {
            return spec.in.map(String).includes(String(cur));
          }
        }
      } catch {
        return true;
      }
      return true;
    })();

    if (!isFieldVisible) return '';
    const showLabel = ui.hideLabel !== true;
    const inputClass = options?.classes?.input || '';
    const baseLabelClass = options?.classes?.label || '';
    const nestedLabelClass = options?.classes?.nestedLabel || baseLabelClass;
    const labelClass = isNested ? nestedLabelClass : baseLabelClass;
    const requiredMark = field?.validate?.required
      ? '<span class="editor-required-mark" title="Required field" aria-hidden="true">*</span>'
      : '';
    const labelHtml = showLabel ? `<div class="${labelClass}">${label}${requiredMark}</div>` : '';
    const baseFieldWrapperClass = (isNested ? (options?.classes?.nestedFieldWrapper || options?.classes?.fieldWrapper) : options?.classes?.fieldWrapper) || '';
    // Position cards need their own row, including in nested component/list grids.
    // An explicit spanAll: false can opt back into the surrounding column layout.
    const spanAll = field.type === 'vec2' ? ui.spanAll !== false : !isNested && ui.spanAll;
    const spanClass = spanAll ? ' editor-field--span-all' : '';
    const fieldWrapperClass = `${baseFieldWrapperClass}${spanClass}`.trim();
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
    const helpText = ui.help || ui.description || field?.help || '';
    const helpHtml = helpText ? `<div class="editor-help-text">${escapeHtml(helpText)}</div>` : '';
    const footerHtml = `${helpHtml}${hintHtml}`;

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
      return `<textarea ${attrs}>${escapeHtml(value ?? '')}</textarea>`;
    };

    const renderScenarioField = () => {
      const modeRaw = (() => {
        if (ui.scenarioModePath) {
          return getValueAtPath(data, String(ui.scenarioModePath));
        }
        if (typeof ui.scenarioMode === 'function') {
          return ui.scenarioMode(data, { path, field, value });
        }
        return ui.scenarioMode;
      })();
      const mode = String(modeRaw || 'universal').toLowerCase();
      const modeLabel = mode === 'world' ? 'World' : 'Universal';
      const valueText = value == null ? '' : String(value);
      const hasValue = valueText.trim().length > 0;
      // Мир записи: по нему компонент сценария открывает карту в «Местоположении».
      const worldPath = ui.mapWorldPath == null ? '' : String(ui.mapWorldPath);
      const worldAttr = worldPath ? ` data-map-world-path="${escapeAttr(worldPath)}"` : '';
      const hiddenAttrs = buildInputAttributes({
        path,
        field,
        classes: 'editor-scenario-hidden',
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: 'type="hidden" data-scenario-value="1"'
      });
      return `
        <div class="editor-scenario-field" data-scenario-field="1" data-scenario-mode="${escapeAttr(mode)}" data-scenario-path="${escapeAttr(path)}" data-scenario-label="${escapeAttr(label)}"${worldAttr}>
          <input ${hiddenAttrs} value="${escapeAttr(valueText)}">
          <div class="editor-scenario-toolbar">
            <button type="button" class="editor-btn editor-btn-secondary" data-action="open-scenario-editor">
              <i class="fa-solid fa-scroll"></i>
              <span>Open the scenario editor</span>
            </button>
          </div>
          <div class="editor-muted-text text-xs mt-2">
            Scenario type: <b data-scenario-mode-label>${escapeHtml(modeLabel)}</b> · <span data-scenario-status>${hasValue ? 'JSON set' : 'JSON not set'}</span>
          </div>
        </div>`;
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
      const selectOptions = (() => {
        const spec = ui.options;
        if (typeof spec === 'function') {
          try {
            const out = spec(data, { path, field, value, multiple });
            return Array.isArray(out) ? out : [];
          } catch (err) {
            console.warn('field-renderer: options() failed for', path, err);
            return [];
          }
        }
        return Array.isArray(spec) ? spec : [];
      })();
      return `<select ${attrs}>${renderSelectOptions(selectOptions, value, multiple)}</select>`;
    };

    const resolveDbParams = () => {
      // datasource can be set directly, or derived from semantic dbKey via EditorCore.DBMap.
      let ds = field.datasource || ui.datasource || ui.source;
      const dbKey = ui.dbKey || field.dbKey;
      if (!ds && dbKey && window.EditorCore?.DB) {
        ds = window.EditorCore.DB.resolveSource(dbKey);
      }
      const placeholder = ui.placeholder || '— select —';
      const labelMode = (ui.labelMode || (window.EditorCore?.DBMap?.[dbKey]?.labelMode) || 'id_name');
      return { ds: ds || '', dbKey, placeholder, labelMode };
    };

    const renderTagsWidget = ({
      options = [],
      datasource = '',
      labelMode,
      valueType = 'string',
      placeholder,
      searchPlaceholder,
      allowCreate = false,
    } = {}) => {
      const ph = placeholder || searchPlaceholder || 'Add…';
      const dsResolved = datasource
        ? (window.EditorCore?.DB?.resolveSource ? (window.EditorCore.DB.resolveSource(datasource) || datasource) : datasource)
        : '';
      const dsKey = datasource || '';
      const resolvedLabelMode = String(labelMode || (window.EditorCore?.DBMap?.[dsKey]?.labelMode) || 'id_name').toLowerCase();
      const resolvedValueType = String(valueType || 'string').toLowerCase();
      const hiddenValueType = resolvedValueType === 'number' ? 'json_array_number' : 'json_array';

      const hiddenAttrs = buildInputAttributes({
        path,
        field,
        classes: '',
        includeName,
        includeDataKey,
        includeDataPath,
        extraAttrs: `type="hidden" data-value-type="${escapeAttr(hiddenValueType)}"`
      });

      const initial = JSON.stringify(normalizeMultiValue(value));
      return `
        <div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">
          ${labelHtml}
          <div class="editor-tags"
               data-tags-options="${escapeAttr(JSON.stringify(options))}"
               data-tags-placeholder="${escapeAttr(ph)}"
               data-tags-allow-create="${allowCreate ? '1' : '0'}"
               ${dsResolved ? `data-tags-datasource="${escapeAttr(dsResolved)}"` : ''}
               ${dsResolved ? `data-tags-label-mode="${escapeAttr(resolvedLabelMode)}"` : ''}
               data-tags-value-type="${escapeAttr(resolvedValueType)}">
            <input ${hiddenAttrs} value="${escapeAttr(initial)}">
            <div class="editor-tags-selected"></div>
            <div class="editor-tags-searchrow">
              <input type="search" class="${inputClass} editor-tags-search" placeholder="${escapeAttr(ph)}" />
            </div>
            <div class="editor-tags-options"></div>
          </div>
          ${footerHtml}
        </div>`;
    };

    // New default UI: single searchable input with dropdown list.
    const renderDbSelectCombo = () => {
      const { ds, dbKey, placeholder, labelMode } = resolveDbParams();
      const rawFilter = ui.dbFilter || field.dbFilter;
      const dbFilter = (() => {
        if (!rawFilter || typeof rawFilter !== 'object') return null;
        const key = String(rawFilter.key || rawFilter.column || '').trim();
        if (!key) return null;
        const rawValues = rawFilter.values ?? rawFilter.value ?? rawFilter.val ?? '';
        const values = Array.isArray(rawValues)
          ? rawValues.map(v => String(v ?? '').trim()).filter(Boolean)
          : [String(rawValues ?? '').trim()].filter(Boolean);
        if (!values.length) return null;
        return { key, values };
      })();
      const filterAttrs = dbFilter
        ? (dbFilter.values.length > 1
          ? ` data-db-filter-key="${escapeAttr(dbFilter.key)}" data-db-filter-values="${escapeAttr(JSON.stringify(dbFilter.values))}"`
          : ` data-db-filter-key="${escapeAttr(dbFilter.key)}" data-db-filter-value="${escapeAttr(dbFilter.values[0])}"`
        )
        : '';
      const extraOptionsRaw = ui.extraOptions || field.extraOptions;
      const extraOptions = Array.isArray(extraOptionsRaw) ? extraOptionsRaw : [];
      const extraOptionsAttr = extraOptions.length
        ? ` data-extra-options="${escapeAttr(JSON.stringify(extraOptions))}"`
        : '';
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
      const allowZero = ui.allowZero === true || String(dbKey || '').toLowerCase() === 'world' || String(ds).toLowerCase() === 'worlds';
      const cur = value ?? (allowZero ? '' : 0);

      const searchPh = ui.searchPlaceholder || ui.placeholder || 'Search…';
      const state = ds ? 'connected' : '';
      const hasVal = (v) => v !== undefined && v !== null && String(v) !== '' && (allowZero || String(v) !== '0');
      const curVal = hasVal(value) ? String(value) : '';

      return `
        <div class="editor-dbcombo${wrapClass}" ${wrapStyle} data-db-state="${escapeAttr(state)}"
             data-datasource="${escapeAttr(ds)}"
             data-db-searchable="${escapeAttr(searchable)}"
             data-db-limit="${escapeAttr(dbLimit)}"
             data-placeholder="${escapeAttr(placeholder)}"
             data-label-mode="${escapeAttr(String(labelMode))}"${extraOptionsAttr}${filterAttrs}
             data-bind-input-path="${escapeAttr(path)}"
             data-current-value="${escapeAttr(curVal)}"
             data-allow-zero="${allowZero ? '1' : '0'}">
          <div class="editor-dbcombo-control">
            <input type="search" class="${inputClass} editor-dbcombo-input" placeholder="${escapeAttr(searchPh)}" autocomplete="off" />
            <button type="button" class="editor-dbcombo-clear" title="Clear selection" aria-label="Clear selection"${curVal ? '' : ' hidden'}>
              <i class="fa-solid fa-xmark" aria-hidden="true"></i>
            </button>
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
      // data-field-path нужен везде: по нему работают валидация, пометки
      // «поле изменено» в гостевом режиме и адресация полей в форме.
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${renderCheckbox()}${footerHtml}</div>`;
    }

    if (field.type === 'toggle' || ui.type === 'toggle') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${renderToggle()}${footerHtml}</div>`;
    }

    if (field.type === 'vec2') {
      // Позиция — единый UI-элемент PositionPicker во всех редакторах. X/Y
      // остаются скрытыми полями формы, а пользователь видит карточку точки,
      // мир, кнопку карты и раскрывающийся ручной ввод.
      // Два способа хранения:
      //  • вложенный объект (Position {x, y}) — ui.axes не задан;
      //  • две отдельные колонки БД (PosX/PosY, PositionX/PositionY) — ui.axes.
      const axesIn = ui.axes && typeof ui.axes === 'object' ? ui.axes : null;
      const axisX = axesIn ? String(axesIn.x || '') : `${path}.x`;
      const axisY = axesIn ? String(axesIn.y || '') : `${path}.y`;
      const containerMode = !axesIn;
      const worldPath = ui.mapWorldPath == null ? '' : String(ui.mapWorldPath);
      const worldValue = worldPath ? getValueAtPath(data, worldPath) : '';
      const step = ui.step === undefined ? 1 : ui.step;
      const rawAxis = (axisPath) => {
        const raw = getValueAtPath(data, axisPath);
        const numeric = raw === '' || raw == null ? 0 : Number(raw);
        return Number.isFinite(numeric) ? numeric : 0;
      };
      const xValue = rawAxis(axisX);
      const yValue = rawAxis(axisY);
      const axisInputHtml = (axisPath, name) => {
        const attrs = buildInputAttributes({
          path: axisPath,
          field: { ui: { min: ui.min, max: ui.max, step } },
          classes: `${inputClass} position-picker__axis-input`,
          includeName,
          includeDataKey,
          includeDataPath
        });
        return `<input type="number" ${attrs} data-value-type="number"
          data-map-axis="${name.toLowerCase()}" data-position-axis="${name.toLowerCase()}"
          value="${escapeAttr(String(rawAxis(axisPath)))}" hidden>`;
      };
      const picker = window.EditorCore?.PositionPicker;
      const pickerHtml = picker?.render ? window.EditorCore.PositionPicker.render({
        label,
        xPath: axisX,
        yPath: axisY,
        xInputHtml: axisInputHtml(axisX, 'X'),
        yInputHtml: axisInputHtml(axisY, 'Y'),
        xFieldPath: axisX,
        yFieldPath: axisY,
        worldPath,
        worldId: worldValue,
        fieldPath: path,
        min: ui.min,
        max: ui.max,
        step,
        pinned: xValue !== 0 || yValue !== 0,
        map: ui.map !== false,
        world: ui.world !== false,
        compact: !!ui.compact || isNested,
        tag: ui.mapPickerTag || 'position',
        id: ui.positionPickerId || '',
        // Флаги карты: запрет игровых тайлов, оверлеи и радиус. showTiles/grid —
        // opt-in начального состояния слоя и сетки; без них оба выключены.
        safeTiles: ui.safeTiles,
        blockTiles: ui.blockTiles,
        showTiles: ui.showTiles,
        highlightTiles: ui.highlightTiles ?? [],
        highlightLayer: ui.highlightLayer,
        switchShowID: ui.switchShowID,
        radiusPath: ui.radiusPath,
        activeRadiusPath: ui.activeRadiusPath,
        radius: ui.radius,
        radiusActive: ui.radiusActive,
        // Массив радиусов: цвет, пунктир и подпись задаёт каждый редактор.
        // Значение поля подставляем из данных — так окружность видна даже в
        // скрытой сейчас секции, а PositionPicker уточнит его из DOM при
        // открытии карты (правки в форме учитываются).
        radii: Array.isArray(ui.radii) ? ui.radii.map((entry) => {
          if (!entry || typeof entry !== 'object' || !entry.path || entry.value !== undefined) return entry;
          const raw = getValueAtPath(data, String(entry.path));
          const numeric = raw === '' || raw == null ? NaN : Number(raw);
          return Number.isFinite(numeric) && numeric > 0 ? { ...entry, value: numeric } : entry;
        }) : ui.radii,
        snap: ui.snap,
        grid: ui.grid,
      }) : `<div class="position-picker" data-position-picker="1"
          data-map-x-path="${escapeAttr(axisX)}" data-map-y-path="${escapeAttr(axisY)}"
          data-map-world-path="${escapeAttr(worldPath)}" data-position-pinned="${xValue !== 0 || yValue !== 0 ? '1' : '0'}">
          ${axisInputHtml(axisX, 'X')}${axisInputHtml(axisY, 'Y')}
          <div class="editor-muted-text text-sm">${escapeHtml(t('The world map is unavailable — reload the page'))}</div>
        </div>`;
      const wrapperPath = containerMode ? ` data-field-path="${escapeAttr(path)}"` : '';
      return `<div class="${fieldWrapperClass}"${wrapperPath}>${pickerHtml}${footerHtml}</div>`;
    }

    if (field.type === 'item') {
      const itemDs = ui.datasource || field.datasource || null;
      const itemFields = {
        id: { type: 'db_select', label: 'Item', datasource: itemDs || undefined, ui: { dbKey: ui.dbKey || 'item', placeholder: ui.placeholder || '— select an item —', inputLabel: 'ID', selectLabel: 'Item (DB)' } },
        value: { type: 'number', label: 'Quantity', ui: { min: ui.min, max: ui.max, step: ui.step, placeholder: ui.placeholder } }
      };
      // Responsive: stack on narrow screens
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}<div class="grid grid-cols-1 sm:grid-cols-2 gap-2">${renderComposite(itemFields)}</div>${footerHtml}</div>`;
    }

    if (field.type === 'list') {
      const items = Array.isArray(value) ? value : [];
      const itemFields = field.itemFields || {};
      const hideLabel = !!(field?.ui?.hideLabel);
      const layout = (field?.ui?.layout || 'stack').toLowerCase();
      const isGrid = layout === 'grid';
      const listMode = String(field?.ui?.listMode || '').toLowerCase();
      const isTable = listMode === 'table';
      const layoutClass = isGrid ? 'editor-grid-fields' : 'space-y-3';
      const gridStyleParts = [];
      if (isGrid) {
        if (field?.ui?.gridGap) gridStyleParts.push(`--editor-grid-gap:${String(field.ui.gridGap)}`);
        if (field?.ui?.gridTemplate) gridStyleParts.push(`--editor-grid-template:${String(field.ui.gridTemplate)}`);
        if (field?.ui?.gridTemplateMd) gridStyleParts.push(`--editor-grid-template-md:${String(field.ui.gridTemplateMd)}`);
        if (field?.ui?.gridTemplateLg) gridStyleParts.push(`--editor-grid-template-lg:${String(field.ui.gridTemplateLg)}`);
      }
      const gridStyleAttr = gridStyleParts.length ? ` style="${escapeAttr(gridStyleParts.join(';'))}"` : '';

      // Table mode: list renders as aligned rows with optional header.
      // Works best for simple pairs like [item, count] or [item, value].
      if (isTable) {
        const cols = String(field?.ui?.tableCols || field?.ui?.gridTemplate || 'minmax(0,1fr)');
        const colsMd = String(field?.ui?.tableColsMd || field?.ui?.gridTemplateMd || cols);
        const colsStyle = `--editor-list-table-cols: var(--editor-table-idx) ${cols} var(--editor-table-del); --editor-list-table-cols-md: var(--editor-table-idx) ${colsMd} var(--editor-table-del);`;

        const headerMode = field?.ui?.tableHeader;
        const showHeader = headerMode !== false;
        const headerLabels = Array.isArray(headerMode)
          ? headerMode
          : Object.values(itemFields).map(f => String(f?.label || '').trim()).filter(Boolean);

        const headerHtml = showHeader
          ? `<div class="editor-list-table-header"><div>#</div>${headerLabels.map(l => `<div>${escapeHtml(l)}</div>`).join('')}<div></div></div>`
          : '';

        const emptyMessage = field?.ui?.emptyMessage || 'The list is empty';
        const rowsHtml = items.length ? items.map((item, index) => {
          const itemPath = `${path}.${index}`;
          const cells = Object.keys(itemFields).map((key) => {
            const subField = itemFields[key];
            const subPath = `${itemPath}.${key}`;
            return `<div class="editor-list-table-cell">${renderField({ path: subPath, field: subField, data, options, isNested: true })}</div>`;
          }).join('');
          return `
            <div class="editor-list-table-row" data-list-item="${escapeAttr(itemPath)}">
              <div class="editor-list-table-idx">#${index + 1}</div>
              ${cells}
              <button type="button" class="${listRemoveClass} editor-list-table-del" title="Delete" data-list-action="remove" data-list-path="${escapeAttr(path)}" data-list-index="${index}">
                <i class="fa-solid fa-trash"></i>
              </button>
            </div>`;
        }).join('')
          : `<div class="editor-empty-state">${escapeHtml(emptyMessage)}</div>`;

        const defaultItem = buildDefaultItem(field);
        const defaultPayload = encodeURIComponent(JSON.stringify(defaultItem));
        return `
          <div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">
            ${hideLabel ? '' : labelHtml}
            <div class="${listWrapperClass} editor-list-table" data-list-container="${escapeAttr(path)}" style="${escapeAttr(colsStyle)}">${headerHtml}${rowsHtml}</div>
            <button type="button" class="${listAddClass} editor-list-add" data-list-action="add" data-list-path="${escapeAttr(path)}" data-list-default="${defaultPayload}">${field?.ui?.addLabel || options?.listAddLabel || 'Add'}</button>
            ${footerHtml}
          </div>`;
      }

      // Default list rendering (cards).
      //
      // stack   — поля элемента друг под другом (по умолчанию);
      // grid    — CSS-сетка с шаблоном и colSpan (крупные формы редакторов БД);
      // compact — узкая шапка «#N … корзина» и поля, которые сами укладываются
      //           в строки по типу (число узкое, выбор шире, теги на всю строку);
      // row     — весь элемент в одну строку: #N, поля, корзина.
      const isRowLayout = layout === 'row';
      const isCompactLayout = layout === 'compact';
      const isFlowLayout = isRowLayout || isCompactLayout;

      // Ширина ячейки по типу поля: числа и галочки узкие, выбор средний,
      // теги/списки/многострочный текст занимают всю строку.
      const cellSize = (subField) => {
        const explicit = String(subField?.ui?.cellSize || '').toLowerCase();
        if (['narrow', 'mid', 'wide'].includes(explicit)) return explicit;
        const type = String(subField?.type || '').toLowerCase();
        const uiType = String(subField?.ui?.type || '').toLowerCase();
        const format = String(subField?.ui?.format || '').toLowerCase();
        if (['number', 'boolean', 'toggle', 'date', 'time'].includes(type)) return 'narrow';
        if (['tags', 'multiselect', 'db_multiselect'].includes(uiType)) return 'wide';
        if (['list', 'textarea', 'json'].includes(type) || format === 'textarea') return 'wide';
        return 'mid';
      };

      const renderCell = (subField, subPath) => {
        const inner = renderField({ path: subPath, field: subField, data, options, isNested: true });
        if (!isFlowLayout) return inner;
        return `<div class="editor-item-cell is-${cellSize(subField)}">${inner}</div>`;
      };

      const listItems = items.map((item, index) => {
        const itemPath = `${path}.${index}`;
        const removeBtn = `
              <button type="button" class="${listRemoveClass} editor-list-item-del" title="Delete" data-list-action="remove" data-list-path="${escapeAttr(path)}" data-list-index="${index}">
                <i class="fa-solid fa-trash"></i>
              </button>`;

        if (isRowLayout || isCompactLayout) {
          const cells = Object.entries(itemFields)
            .map(([key, subField]) => renderCell(subField, `${itemPath}.${key}`))
            .join('');
          const head = `<span class="editor-list-item-idx">#${index + 1}</span>`;
          return isRowLayout
            ? `
          <div class="${listItemClass} editor-list-item--row" data-list-item="${escapeAttr(itemPath)}">
            ${head}
            <div class="editor-item-fields">${cells}</div>${removeBtn}
          </div>`
            : `
          <div class="${listItemClass} editor-list-item--compact" data-list-item="${escapeAttr(itemPath)}">
            <div class="editor-list-item-bar">${head}${removeBtn}</div>
            <div class="editor-item-fields">${cells}</div>
          </div>`;
        }

        const itemContent = Object.entries(itemFields).map(([key, subField]) => {
          return renderField({ path: `${itemPath}.${key}`, field: subField, data, options, isNested: true });
        }).join('');
        return `
          <div class="${listItemClass}" data-list-item="${escapeAttr(itemPath)}">
            <div class="flex items-center justify-between gap-3 mb-2">
              <div class="editor-muted-text text-xs">#${index + 1}</div>
              <button type="button" class="${listRemoveClass}" title="Delete" data-list-action="remove" data-list-path="${escapeAttr(path)}" data-list-index="${index}">
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
      const emptyMessage = field?.ui?.emptyMessage || 'The list is empty';
      const listContent = listItems || `<div class="editor-empty-state">${escapeHtml(emptyMessage)}</div>`;
      const defaultItem = buildDefaultItem(field);
      const defaultPayload = encodeURIComponent(JSON.stringify(defaultItem));
      return `
        <div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">
          ${hideLabel ? '' : labelHtml}
          <div class="${listWrapperClass}" data-list-container="${escapeAttr(path)}">${listContent}</div>
          <button type="button" class="${listAddClass} editor-list-add" data-list-action="add" data-list-path="${escapeAttr(path)}" data-list-default="${defaultPayload}">${field?.ui?.addLabel || options?.listAddLabel || 'Add'}</button>
          ${footerHtml}
        </div>`;
    }

    // New default db_select: single searchable combo input.
    if (field.type === 'db_select' || ui.type === 'db_select' || field.type === 'db_select_search' || ui.type === 'db_select_search') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderDbSelectCombo()}${footerHtml}</div>`;
    }

    if (field.type === 'db_multiselect' || ui.type === 'db_multiselect') {
      const options = Array.isArray(ui.options) ? ui.options : [];
      const dsRaw = String(ui.datasource || ui.dbKey || field.datasource || '').trim();
      return renderTagsWidget({
        options,
        datasource: dsRaw,
        labelMode: ui.labelMode,
        valueType: ui.valueType || 'number',
        placeholder: ui.placeholder,
        searchPlaceholder: ui.searchPlaceholder,
        allowCreate: ui.allowCreate,
      });
    }

    if (ui.type === 'select') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderSelect(false)}${footerHtml}</div>`;
    }

    if (ui.type === 'multiselect') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderSelect(true)}${footerHtml}</div>`;
    }

    if (ui.type === 'tags') {
      const opts = Array.isArray(ui.options) ? ui.options : [];
      const dsRaw = String(ui.datasource || ui.dbKey || '').trim();
      return renderTagsWidget({
        options: opts,
        datasource: dsRaw,
        labelMode: ui.labelMode,
        valueType: ui.valueType || 'string',
        placeholder: ui.placeholder,
        searchPlaceholder: ui.searchPlaceholder,
        allowCreate: ui.allowCreate,
      });
    }

    if (ui.format === 'date' || field.type === 'date') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('date', value)}${footerHtml}</div>`;
    }

    if (ui.format === 'time' || field.type === 'time') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('time', value)}${footerHtml}</div>`;
    }

    if (ui.format === 'datetime' || ui.format === 'datetime-local' || field.type === 'datetime') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('datetime-local', value)}${footerHtml}</div>`;
    }

    if (ui.format === 'password' || field.type === 'password') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('password', value ?? '')}${footerHtml}</div>`;
    }

    if (ui.format === 'textarea' || ui.type === 'textarea') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderTextarea()}${footerHtml}</div>`;
    }

    if (field.type === 'scenario' || ui.type === 'scenario') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderScenarioField()}${footerHtml}</div>`;
    }

    if (field.type === 'number') {
      return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('number', value ?? 0)}${footerHtml}</div>`;
    }

    return `<div class="${fieldWrapperClass}" data-field-path="${escapeAttr(path)}">${labelHtml}${renderInput('text', value ?? '')}${footerHtml}</div>`;
  };

  const renderSectionDivider = (section) => {
    if (!section) return '';
    const spec = typeof section === 'string' ? { title: section } : section;
    const title = String(spec?.title || '').trim();
    if (!title) return '';
    const icon = spec?.icon ? `<i class="fa-solid ${escapeAttr(spec.icon)}" aria-hidden="true"></i>` : '';
    const sub = spec?.subtitle ? `<span class="editor-form-section-sub">${escapeHtml(spec.subtitle)}</span>` : '';
    return `<div class="editor-form-section-divider"><span class="editor-form-section-heading">${icon}<span>${escapeHtml(title)}</span></span>${sub}</div>`;
  };

  const renderFields = ({ fields, data, options, sort = false }) => {
    const entries = Object.entries(fields || {});
    if (sort) entries.sort(([a], [b]) => a.localeCompare(b));
    let pendingSection = null;
    const out = [];
    for (const [key, field] of entries) {
      const section = field?.ui?.section ?? field?.section ?? null;
      if (section) pendingSection = section;
      const html = renderField({ path: key, field, data, options });
      if (!html) continue;
      if (pendingSection) {
        out.push(renderSectionDivider(pendingSection));
        pendingSection = null;
      }
      out.push(html);
    }
    return out.join('');
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
