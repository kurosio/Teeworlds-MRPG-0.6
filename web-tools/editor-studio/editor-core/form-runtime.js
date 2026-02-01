(() => {
  // Minimal schema-driven form runtime.
  // Renders fields via FieldRenderer and provides 2-way binding + list add/remove.
  // This is intended to simplify creation of new editors.

  const parsePath = (path) => String(path).split('.').map((part) => (/^\d+$/.test(part) ? Number(part) : part));

  const getAt = (obj, path) => {
    if (!path) return undefined;
    return parsePath(path).reduce((cur, k) => (cur == null ? undefined : cur[k]), obj);
  };

  const setAt = (obj, path, value) => {
    if (!path) return;
    const keys = parsePath(path);
    let cur = obj;
    for (let i = 0; i < keys.length; i++) {
      const k = keys[i];
      if (i === keys.length - 1) {
        cur[k] = value;
        return;
      }
      const nextKey = keys[i + 1];
      if (cur[k] == null) cur[k] = typeof nextKey === 'number' ? [] : {};
      cur = cur[k];
    }
  };

  const readInputValue = (el) => {
    if (!el) return undefined;
    // Prefer the shared FieldRenderer logic (handles db_select, multiselect, numeric ids, unix_seconds, etc.)
    const fr = window.EditorCore?.FieldRenderer;
    if (fr?.getInputValue) return fr.getInputValue(el);
    // Fallback (should rarely happen)
    if (el.type === 'checkbox') return !!el.checked;
    if (el.type === 'number') return el.value === '' ? 0 : Number(el.value);
    return el.value;
  };

  const FormRuntime = {
    mount(root, { fields, data, fieldOptions, onChange } = {}) {
      const state = { data: data || {} };

      const render = () => {
        if (!window.EditorCore?.FieldRenderer) {
          root.innerHTML = '<div class="editor-muted-text">FieldRenderer не загружен</div>';
          return;
        }
        const html = window.EditorCore.FieldRenderer.renderFields({
          fields,
          data: state.data,
          options: fieldOptions || window.EditorCore?.defaults?.fieldOptions
        });
        root.innerHTML = html;

        // Initialize shared UI behaviors (validation, adaptive db_select, etc.)
        // UIManager also triggers DB.init internally.
        window.EditorCore?.UIManager?.init(root);

        // Input bindings
        root.querySelectorAll('[data-path]').forEach((el) => {
          const handler = () => {
            const path = el.getAttribute('data-path');
            const val = readInputValue(el);
            setAt(state.data, path, val);

            // Sync adaptive db_select helper select -> input is handled by DB.init.
            // But we also keep toggle label state in sync.
            if (el.type === 'checkbox' && el.closest('.editor-toggle')) {
              el.closest('.editor-toggle')?.setAttribute('data-checked', el.checked ? '1' : '0');
            }
            if (typeof onChange === 'function') onChange(state.data, { path, value: val });
          };
          el.addEventListener('input', handler);
          el.addEventListener('change', handler);
        });

        // List actions
        root.querySelectorAll('[data-list-action]').forEach((btn) => {
          btn.addEventListener('click', () => {
            const action = btn.getAttribute('data-list-action');
            const listPath = btn.getAttribute('data-list-path');
            const arr = Array.isArray(getAt(state.data, listPath)) ? getAt(state.data, listPath) : [];
            if (action === 'add') {
              const payload = btn.getAttribute('data-list-default') || '';
              let def = {};
              try { def = JSON.parse(decodeURIComponent(payload)); } catch { def = {}; }
              arr.push(def);
              setAt(state.data, listPath, arr);
              if (typeof onChange === 'function') onChange(state.data, { path: listPath, value: arr });
              render();
            }
            if (action === 'remove') {
              const idx = Number(btn.getAttribute('data-list-index'));
              if (Number.isFinite(idx) && idx >= 0 && idx < arr.length) {
                arr.splice(idx, 1);
                setAt(state.data, listPath, arr);
                if (typeof onChange === 'function') onChange(state.data, { path: listPath, value: arr });
                render();
              }
            }
          });
        });
      };

      const api = {
        render,
        getData: () => state.data,
        setData: (next) => {
          state.data = next || {};
          render();
        }
      };

      render();
      return api;
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.FormRuntime = FormRuntime;
})();
