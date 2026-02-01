(() => {
  const assertReady = () => {
    if (!window.EditorCore) throw new Error('EditorCore is missing.');
    if (!window.EditorCore.UI) throw new Error('EditorCore.UI is missing. Load ui.js first.');
    if (!window.EditorCore.defaults) throw new Error('EditorCore.defaults is missing. Load defaults.js.');
  };

  /**
   * One-liner setup for any editor page.
   * - mounts shared modals / toasts
   * - provides shared defaults (FieldRenderer options)
   */
  const bootstrapEditor = ({ mode = 'scenario' } = {}) => {
    assertReady();

    if (mode === 'event') {
      window.EditorCore.UI.mountEventEditorUI();
    } else {
      window.EditorCore.UI.mountScenarioEditorUI();
    }

    // Core UI init (DB-backed selects, validation hints, etc.)
    if (window.EditorCore.UIManager?.init) {
      // fire-and-forget; init is idempotent
      Promise.resolve().then(() => window.EditorCore.UIManager.init(document));
    }

    return {
      fieldRenderOptions: window.EditorCore.defaults.getFieldRenderOptions(mode),
    };
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.bootstrapEditor = bootstrapEditor;
})();
