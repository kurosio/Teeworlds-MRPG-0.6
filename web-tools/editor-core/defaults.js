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

  // UI presets to keep schemas small and consistent across editors.
  // Usage in editor schemas:
  //   ui: EditorCore.presets.tags({ options:[...], placeholder:'...', allowCreate:false })
  //   ui: EditorCore.presets.tags({ datasource:'item', valueType:'number', labelMode:'id_name' })
  window.EditorCore.presets = window.EditorCore.presets || {};
  window.EditorCore.presets.tags = (cfg = {}) => {
    const c = cfg || {};
    const options = Array.isArray(c.options)
      ? c.options
      : (typeof c.options === 'string'
          ? c.options.split(',').map(s => s.trim()).filter(Boolean)
          : []);
    return {
      type: 'tags',
      options,
      datasource: c.datasource || c.dbKey || '',
      labelMode: c.labelMode || undefined,
      valueType: c.valueType || 'string',
      allowCreate: !!c.allowCreate,
      placeholder: c.placeholder || c.searchPlaceholder || 'Добавить…',
      searchPlaceholder: c.searchPlaceholder || undefined,
    };
  };
})();
