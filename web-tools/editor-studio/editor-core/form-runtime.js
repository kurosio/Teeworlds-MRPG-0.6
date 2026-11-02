/** Schema-driven form with an explicit, idempotent lifecycle. */
(() => {
  const { getAtPath, setAtPath } = window.EditorCore.utils;
  const readInputValue = (element) => {
    const renderer = window.EditorCore?.FieldRenderer;
    if (renderer?.getInputValue) return renderer.getInputValue(element);
    if (element.type === 'checkbox') return !!element.checked;
    if (element.type === 'number') return element.value === '' ? 0 : Number(element.value);
    return element.value;
  };
  const listDefault = (button) => {
    try { return JSON.parse(decodeURIComponent(button.getAttribute('data-list-default') || '%7B%7D')); }
    catch { return {}; }
  };

  const FormRuntime = {
    mount(root, { fields = {}, data, fieldOptions, onChange } = {}) {
      if (!root?.addEventListener) throw new Error('FormRuntime: a DOM root is required');
      root.__editorFormRuntime?.destroy();
      const state = { data: data || {} };
      const events = new AbortController();
      let destroyed = false;
      let renderVersion = 0;
      const emit = (path, value) => {
        if (!destroyed) onChange?.(state.data, { path, value });
      };
      const disposeFields = () => {
        if (window.EditorCore?.UIManager?.cancelInit) window.EditorCore.UIManager.cancelInit(root);
        else {
          root.__editorFieldBindings?.();
          window.EditorCore?.DB?.cancelInit?.(root);
        }
      };
      const captureFocus = () => {
        const active = root.ownerDocument.activeElement;
        if (!active || !root.contains(active) || !active.dataset.path) return null;
        return { path: active.dataset.path, start: active.selectionStart, end: active.selectionEnd };
      };
      const render = () => {
        if (destroyed) return;
        const version = ++renderVersion;
        const focus = captureFocus();
        disposeFields();
        const renderer = window.EditorCore?.FieldRenderer;
        if (!renderer) {
          root.innerHTML = '<div class="editor-empty-state">FieldRenderer is not loaded</div>';
          return;
        }
        root.innerHTML = renderer.renderFields({ fields, data: state.data,
          options: fieldOptions || window.EditorCore?.defaults?.fieldOptions });
        // Bind validation immediately; lookup responses may arrive much later.
        window.EditorCore?.UIManager?.bindFields?.(root);
        window.EditorCore?.PositionPicker?.scan?.(root);
        if (focus) {
          const input = Array.from(root.querySelectorAll('[data-path]'))
            .find(element => element.dataset.path === focus.path && !element.disabled && element.type !== 'hidden');
          if (input) {
            input.focus({ preventScroll: true });
            if (typeof focus.start === 'number' && input.setSelectionRange) {
              try { input.setSelectionRange(focus.start, focus.end); } catch { /* not a text input */ }
            }
          }
        }
        Promise.resolve(window.EditorCore?.UIManager?.init?.(root)).then(() => {
          if (!destroyed && version === renderVersion) window.EditorCore?.PositionPicker?.scan?.(root);
        }).catch(error => {
          if (!destroyed && version === renderVersion) {
            window.EditorCore?.Page?.report?.('Form initialization failed', error);
            window.EditorCore?.Page?.toast?.('Some form controls could not load. Refresh the record to retry.', 'error');
          }
        });
      };
      const onFieldEvent = (event) => {
        const element = event.target;
        if (destroyed || !element?.matches?.('[data-path]') || !root.contains(element)) return;
        const path = element.getAttribute('data-path');
        const value = readInputValue(element);
        setAtPath(state.data, path, value);
        if (element.type === 'checkbox') {
          element.closest('.editor-toggle')?.setAttribute('data-checked', element.checked ? '1' : '0');
        }
        emit(path, value);
      };
      const onListClick = (event) => {
        const button = event.target?.closest?.('[data-list-action]');
        if (destroyed || !button || !root.contains(button)) return;
        const action = button.getAttribute('data-list-action');
        const path = button.getAttribute('data-list-path');
        const current = getAtPath(state.data, path);
        const list = Array.isArray(current) ? current : [];
        if (action === 'add') list.push(listDefault(button));
        else if (action === 'remove') {
          const index = Number(button.getAttribute('data-list-index'));
          if (!Number.isInteger(index) || index < 0 || index >= list.length) return;
          list.splice(index, 1);
        } else return;
        setAtPath(state.data, path, list);
        emit(path, list);
        render();
      };
      for (const [type, handler] of [['input', onFieldEvent], ['change', onFieldEvent], ['click', onListClick]]) {
        root.addEventListener(type, handler, { signal: events.signal });
      }
      const api = {
        render,
        getData: () => state.data,
        validate: (options) => window.EditorCore.Validation.validateRoot(root, options),
        setData: (next) => { state.data = next || {}; render(); },
        destroy: () => {
          if (destroyed) return;
          destroyed = true;
          renderVersion += 1;
          events.abort();
          disposeFields();
          if (root.__editorFormRuntime === api) delete root.__editorFormRuntime;
        },
      };
      root.__editorFormRuntime = api;
      render();
      return api;
    },
  };
  window.EditorCore = window.EditorCore || {};
  window.EditorCore.FormRuntime = FormRuntime;
})();
