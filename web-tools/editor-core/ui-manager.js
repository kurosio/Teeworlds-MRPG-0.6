(() => {
  const parseValidateSpec = (raw) => {
    if (!raw) return null;
    try {
      return JSON.parse(raw);
    } catch {
      return null;
    }
  };

  const setHint = (hintEl, message) => {
    if (!hintEl) return;
    if (!message) {
      hintEl.textContent = '';
      hintEl.classList.add('hidden');
      return;
    }
    hintEl.textContent = message;
    hintEl.classList.remove('hidden');
  };

  const validateValue = ({ value, spec, inputType }) => {
    if (!spec) return { ok: true, message: '' };
    const message = spec.message || 'Некорректное значение';

    // Required
    if (spec.required) {
      const empty = value == null || value === '' || (Array.isArray(value) && value.length === 0);
      if (empty) return { ok: false, message: spec.requiredMessage || 'Обязательное поле' };
    }

    // Number bounds
    if (inputType === 'number' || typeof value === 'number') {
      const num = typeof value === 'number' ? value : (value === '' ? NaN : Number(value));
      if (Number.isFinite(num)) {
        if (spec.min !== undefined && num < spec.min) return { ok: false, message: spec.minMessage || `Минимум: ${spec.min}` };
        if (spec.max !== undefined && num > spec.max) return { ok: false, message: spec.maxMessage || `Максимум: ${spec.max}` };
      }
    }

    // Pattern
    if (spec.pattern) {
      try {
        const re = new RegExp(spec.pattern);
        if (!re.test(String(value ?? ''))) return { ok: false, message };
      } catch {
        // ignore broken regex
      }
    }

    return { ok: true, message: '' };
  };

  const UIManager = {
    async init(root = document) {
      // 1) Dynamic DB selects
      if (window.EditorCore?.DB?.init) {
        await window.EditorCore.DB.init(root);
      }

      // 1.1) Adaptive DB selects: toggle helper dropdown vs manual input
      root.querySelectorAll('.editor-dbselect').forEach((wrapper) => {
        const sel = wrapper.querySelector('select[data-datasource]');
        if (!sel) {
          wrapper.dataset.dbState = 'disconnected';
          return;
        }
        const failed = sel.dataset.dbFailed === '1';
        if (failed) {
          wrapper.dataset.dbState = 'disconnected';
          return;
        }

        // Searchable selects load incrementally; treat them as "connected" once DB.init has run.
        if (sel.dataset.dbSearchable === '1') {
          wrapper.dataset.dbState = 'connected';
          return;
        }

        // Bulk selects set dbLoaded. As an extra guard, consider it loaded if it has options.
        const loaded = sel.dataset.dbLoaded === '1' || (sel.options && sel.options.length > (sel.multiple ? 0 : 1));
        wrapper.dataset.dbState = loaded ? 'connected' : 'disconnected';
      });

      // 2) Toggle state sync (for CSS styling)
      root.querySelectorAll('.editor-toggle input[type=checkbox]').forEach((cb) => {
        const label = cb.closest('.editor-toggle');
        const sync = () => { if (label) label.dataset.checked = cb.checked ? '1' : '0'; };
        cb.addEventListener('change', sync);
        sync();
      });

      // 2) Validation hints
      const inputs = Array.from(root.querySelectorAll('[data-validate]'));
      for (const el of inputs) {
        const spec = parseValidateSpec(el.getAttribute('data-validate'));
        if (!spec) continue;
        const path = el.getAttribute('data-path') || el.getAttribute('name') || el.getAttribute('data-key');
        const hint = path ? root.querySelector(`[data-hint-for="${CSS.escape(path)}"]`) : null;

        const run = () => {
          const value = window.EditorCore?.FieldRenderer?.getInputValue
            ? window.EditorCore.FieldRenderer.getInputValue(el)
            : (el.type === 'checkbox' ? el.checked : el.value);

          const { ok, message } = validateValue({ value, spec, inputType: el.type });
          el.classList.toggle('editor-invalid', !ok);
          setHint(hint, ok ? '' : message);
        };

        el.addEventListener('input', run);
        el.addEventListener('change', run);
        run();
      }
    }
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.UIManager = UIManager;
})();
