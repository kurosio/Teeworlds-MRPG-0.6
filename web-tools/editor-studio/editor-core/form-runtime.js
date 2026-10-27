(() => {
  // Schema-driven form runtime. Events are delegated once per mounted form:
  // rerendering lists no longer accumulates handlers on every generated input.
  const { getAtPath, setAtPath } = window.EditorCore.utils;

  const readInputValue = (element) => {
    if (!element) return undefined;
    const renderer = window.EditorCore?.FieldRenderer;
    if (renderer?.getInputValue) return renderer.getInputValue(element);
    if (element.type === 'checkbox') return !!element.checked;
    if (element.type === 'number') return element.value === '' ? 0 : Number(element.value);
    return element.value;
  };

  const listDefault = (button) => {
    try {
      return JSON.parse(decodeURIComponent(button.getAttribute('data-list-default') || ''));
    } catch {
      return {};
    }
  };

  const FormRuntime = {
    mount(root, { fields, data, fieldOptions, onChange } = {}) {
      const state = { data: data || {} };

      // A form root can be mounted more than once (DB editors re-mount the form
      // on every record switch). Listeners left by a previous mount would keep
      // writing to a stale state object and, after render() replaces the event
      // target's DOM, would miss events on the new DOM. Remove them first so
      // re-mounting on the same root is always safe.
      const prevHandlers = root.__editorFormHandlers;
      if (prevHandlers) {
        root.removeEventListener('input', prevHandlers.input);
        root.removeEventListener('change', prevHandlers.change);
        root.removeEventListener('click', prevHandlers.click);
      }

      const render = () => {
        const renderer = window.EditorCore?.FieldRenderer;
        if (!renderer) {
          root.innerHTML = '<div class="editor-muted-text">FieldRenderer is not loaded</div>';
          return;
        }
        root.innerHTML = renderer.renderFields({
          fields,
          data: state.data,
          options: fieldOptions || window.EditorCore?.defaults?.fieldOptions,
        });
        // Validation, toggles and DB widgets are initialized only for new nodes.
        window.EditorCore?.UIManager?.init(root);
        // PositionPicker is declarative: refresh its world list and labels too.
        window.EditorCore?.PositionPicker?.scan?.(root);
      };

      const onFieldEvent = (event) => {
        const element = event.target;
        if (!element?.matches?.('[data-path]') || !root.contains(element)) return;
        const path = element.getAttribute('data-path');
        const value = readInputValue(element);
        setAtPath(state.data, path, value);

        if (element.type === 'checkbox') {
          element.closest('.editor-toggle')?.setAttribute('data-checked', element.checked ? '1' : '0');
        }
        if (typeof onChange === 'function') onChange(state.data, { path, value });
      };

      const onListClick = (event) => {
        const button = event.target?.closest?.('[data-list-action]');
        if (!button || !root.contains(button)) return;
        const action = button.getAttribute('data-list-action');
        const path = button.getAttribute('data-list-path');
        const current = getAtPath(state.data, path);
        const list = Array.isArray(current) ? current : [];

        if (action === 'add') {
          list.push(listDefault(button));
        } else if (action === 'remove') {
          const index = Number(button.getAttribute('data-list-index'));
          if (!Number.isFinite(index) || index < 0 || index >= list.length) return;
          list.splice(index, 1);
        } else {
          return;
        }

        setAtPath(state.data, path, list);
        if (typeof onChange === 'function') onChange(state.data, { path, value: list });
        render();
      };

      root.addEventListener('input', onFieldEvent);
      root.addEventListener('change', onFieldEvent);
      root.addEventListener('click', onListClick);
      root.__editorFormHandlers = { input: onFieldEvent, change: onFieldEvent, click: onListClick };

      const api = {
        render,
        getData: () => state.data,
        setData: (next) => {
          state.data = next || {};
          render();
        },
        destroy: () => {
          root.removeEventListener('input', onFieldEvent);
          root.removeEventListener('change', onFieldEvent);
          root.removeEventListener('click', onListClick);
          const handlers = root.__editorFormHandlers;
          if (handlers && handlers.input === onFieldEvent) delete root.__editorFormHandlers;
        },
      };

      render();
      return api;
    },
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.FormRuntime = FormRuntime;
})();
