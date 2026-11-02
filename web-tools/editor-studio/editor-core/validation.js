/**
 * Shared schema validation. Pure rules are also available to node:test.
 * DOM binding is delegated: replacing fields does not accumulate listeners.
 */
((root) => {
  'use strict';
  const translate = (text, vars) => root.EditorCore?.Page?.t?.(text, vars) || text;
  const parseSpec = (raw) => {
    if (!raw) return null;
    if (typeof raw === 'object') return raw;
    try { return JSON.parse(raw); } catch { return undefined; }
  };
  const isEmpty = (value) => value == null || value === false
    || (typeof value === 'string' && value.trim() === '')
    || (Array.isArray(value) && value.length === 0);

  const validateValue = ({ value, spec, inputType = '', badInput = false }) => {
    const fail = (message) => ({ ok: false, message: translate(message) });
    if (!spec || typeof spec !== 'object' || Array.isArray(spec)) return fail('Invalid validation rule');
    if (badInput) return fail('Enter a valid number');
    if (spec.required && isEmpty(value)) {
      return { ok: false, message: spec.requiredMessage || translate('Required field') };
    }
    // Zero is a valid value (notably world 0). Only required checks treat empty text as missing.
    if (value == null || value === '') return { ok: true, message: '' };
    if (inputType === 'number' || typeof value === 'number') {
      const number = Number(value);
      if (!Number.isFinite(number)) return fail('Enter a valid number');
      for (const key of ['min', 'max']) {
        if (spec[key] === undefined || spec[key] === '') continue;
        const bound = Number(spec[key]);
        if (!Number.isFinite(bound)) return fail('Invalid validation rule');
        if ((key === 'min' && number < bound) || (key === 'max' && number > bound)) {
          const message = spec[`${key}Message`]
            || translate(key === 'min' ? 'Minimum: {0}' : 'Maximum: {0}', [String(bound)]);
          return { ok: false, message };
        }
      }
      if (spec.integer && !Number.isInteger(number)) return fail('Enter a whole number');
    }
    if (spec.pattern) {
      try {
        if (!new RegExp(spec.pattern).test(String(value))) {
          return { ok: false, message: spec.message || translate('Invalid value') };
        }
      } catch { return fail('Invalid validation rule'); }
    }
    return { ok: true, message: '' };
  };

  const activeControl = (element, root) => {
    if (element.disabled || element.closest('[inert]')) return false;
    // Structured widgets use hidden value inputs. Their visible wrapper determines activity.
    const structured = element.dataset.valueType?.startsWith('json_array') || element.hasAttribute('data-position-axis');
    if (element.type === 'hidden' && !structured) return false;
    for (let node = element; node; node = node.parentElement) {
      if (node.hasAttribute('hidden') || node.classList?.contains('hidden') || node.style?.display === 'none') return false;
      if (node === root) break;
    }
    return true;
  };
  const controls = (root) => Array.from(root.querySelectorAll('[data-validate]'))
    .filter(element => activeControl(element, root));
  const readValue = (element) => root.EditorCore?.FieldRenderer?.getInputValue
    ? root.EditorCore.FieldRenderer.getInputValue(element)
    : element.type === 'checkbox' ? element.checked : element.value;
  let hintSequence = 0;
  const inspect = (element, scope) => {
    const spec = parseSpec(element.getAttribute('data-validate'));
    const result = validateValue({ value: readValue(element), spec,
      inputType: element.type, badInput: element.validity?.badInput === true });
    const path = element.getAttribute('data-path') || element.getAttribute('name') || '';
    const wrapper = element.closest('[data-field-path]');
    const hint = wrapper?.querySelector('[data-hint-for]')
      || (path ? scope.querySelector(`[data-hint-for="${root.EditorCore.utils.cssEscape(path)}"]`) : null);
    element.classList.toggle('editor-invalid', !result.ok);
    element.setAttribute('aria-invalid', String(!result.ok));
    if (spec?.required) element.setAttribute('aria-required', 'true');
    if (hint) {
      if (!hint.id) hint.id = `editor-validation-${++hintSequence}`;
      const ids = new Set((element.getAttribute('aria-describedby') || '').split(/\s+/).filter(Boolean));
      if (result.ok) ids.delete(hint.id); else ids.add(hint.id);
      if (ids.size) element.setAttribute('aria-describedby', [...ids].join(' '));
      else element.removeAttribute('aria-describedby');
      hint.textContent = result.ok ? '' : result.message;
      hint.classList.toggle('hidden', result.ok);
    }
    return { ...result, element, path };
  };
  const validateRoot = (scope, { focus = false } = {}) => {
    const errors = controls(scope).map(element => inspect(element, scope)).filter(result => !result.ok);
    if (focus && errors.length) {
      const target = errors[0].element;
      const visible = target.closest('.editor-dbcombo')?.querySelector('.editor-dbcombo-input')
        || (target.type === 'hidden' ? target.closest('[data-field-path]')?.querySelector('input:not([type="hidden"]), select, button') : target);
      visible?.focus?.();
      visible?.scrollIntoView?.({ block: 'nearest', inline: 'nearest' });
    }
    return { ok: errors.length === 0, errors };
  };
  const bind = (scope) => {
    scope.__editorValidationDispose?.();
    const controller = new AbortController();
    const onInput = (event) => {
      const element = event.target;
      if (element?.matches?.('[data-validate]') && activeControl(element, scope)) inspect(element, scope);
    };
    for (const type of ['input', 'change', 'focusout']) {
      scope.addEventListener(type, onInput, { signal: controller.signal });
    }
    const dispose = () => {
      controller.abort();
      if (scope.__editorValidationDispose === dispose) delete scope.__editorValidationDispose;
    };
    scope.__editorValidationDispose = dispose;
    validateRoot(scope);
    return dispose;
  };
  const api = { parseSpec, validateValue, validateRoot, bind };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else {
    root.EditorCore = root.EditorCore || {};
    root.EditorCore.Validation = api;
  }
})(typeof window !== 'undefined' ? window : globalThis);
