(() => {
  const registry = new Map();

  const register = (id, app) => {
    if (!id) {
      throw new Error('Editor registry requires an id.');
    }
    registry.set(id, app);
    return app;
  };

  const get = (id) => registry.get(id);
  const list = () => Array.from(registry.keys());

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.registry = { register, get, list };
})();
