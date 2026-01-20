(() => {
  const createEditorApp = ({ id, init, render, state = {} }) => {
    if (!window.EditorCore?.registry) {
      throw new Error('EditorCore registry is not available.');
    }

    const app = {
      id,
      state,
      async init() {
        if (typeof init === 'function') {
          await init(app);
        }
      },
      render() {
        if (typeof render === 'function') {
          render(app);
        }
      },
      async mount() {
        await app.init();
        app.render();
        window.EditorCore.registry.register(id, app);
        return app;
      }
    };

    return app;
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.createEditorApp = createEditorApp;
})();
