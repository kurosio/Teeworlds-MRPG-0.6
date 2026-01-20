(() => {
  // Central place for editor-wide visual / rendering defaults.
  // Editors should read these instead of redefining local constants.

  const INPUT_CLASS_SCENARIO = 'w-full rounded-md border-slate-300/40 bg-slate-900/40 text-slate-100 shadow-sm focus:border-blue-400 focus:ring-blue-400';
  const INPUT_CLASS_EVENT = 'w-full p-2 rounded-md form-input';

  const FIELD_RENDER_OPTIONS = {
    scenario: {
      classes: {
        input: INPUT_CLASS_SCENARIO,
        textarea: `${INPUT_CLASS_SCENARIO} min-h-[80px] resize-y font-mono text-sm`,
        multiselect: INPUT_CLASS_SCENARIO,
        label: 'block text-sm font-medium text-slate-200 mb-1',
        nestedLabel: 'block text-xs font-medium text-slate-300 mb-1',
        fieldWrapper: 'p-3 rounded-lg bg-black/10',
        checkboxWrapper: 'flex items-center space-x-3 cursor-pointer',
        listWrapper: 'space-y-2',
        listItem: 'bg-black/20 p-3 rounded-md space-y-2',
        listAdd: 'bg-green-500 text-white font-semibold py-2 px-3 rounded-md hover:bg-green-600 transition-colors',
        listRemove: 'bg-red-500 text-white font-semibold py-2 px-3 rounded-md hover:bg-red-600 transition-colors',
        checkbox: 'h-4 w-4 rounded border-slate-300/40 bg-slate-900/40 text-blue-500',
      },
      includeName: true,
      includeDataPath: true,
      includeDataKey: false,
    },

    event: {
      classes: {
        input: INPUT_CLASS_EVENT,
        textarea: `${INPUT_CLASS_EVENT} min-h-[40px] resize-y`,
        multiselect: INPUT_CLASS_EVENT,
        label: 'block text-sm font-medium text-gray-300 mb-2',
        nestedLabel: 'block text-xs font-medium text-gray-400 mb-1',
        fieldWrapper: 'p-4 rounded-lg bg-black/10',
        checkboxWrapper: 'flex items-center space-x-3 cursor-pointer',
        listWrapper: 'space-y-2',
        listItem: 'bg-black/20 p-3 rounded-md space-y-2',
        listAdd: 'add-list-btn mt-2 bg-blue-500 hover:bg-blue-600 text-white text-sm py-1 px-3 rounded-md btn',
        listRemove: 'remove-list-btn text-gray-400 hover:text-secondary text-sm',
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
