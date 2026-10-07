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
   * - auto-mounts sync button in editor toolbar
   */
  const bootstrapEditor = () => {
    assertReady();

    window.EditorCore.UI.mountScenarioEditorUI();

    // Core UI init (DB-backed selects, validation hints, etc.)
    if (window.EditorCore.UIManager?.init) {
      // fire-and-forget; init is idempotent
      Promise.resolve().then(() => window.EditorCore.UIManager.init(document));
    }

    // Auto-mount sync button in editor toolbar (if not already mounted)
    if (window.EditorCore.Sync?.mountSyncButton) {
      Promise.resolve().then(() => {
        // Skip settings page and database page
        const editorName = document.body?.dataset?.editor || '';
        if (editorName === 'settings' || editorName === 'database') return;

        // Find toolbar actions area
        const actionsArea = document.querySelector('.editor-page-actions');
        if (actionsArea && !actionsArea.querySelector('[data-sync-btn]')) {
          // Extract editor name from page title or data attribute
          const titleEl = document.querySelector('.editor-page-title h1');
          const displayName = titleEl?.textContent?.trim() || editorName || 'Редактор';
          window.EditorCore.Sync.mountSyncButton(actionsArea, { editorName: displayName });
        }
      });
    }

    return {
      fieldRenderOptions: window.EditorCore.defaults.getFieldRenderOptions('scenario'),
    };
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.bootstrapEditor = bootstrapEditor;
})();
