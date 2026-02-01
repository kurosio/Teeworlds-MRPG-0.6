(() => {
  // ------------------------------------------------------------
  // dependent select utility
  //
  // provides:
  //  - rule-based options for <select>
  //  - optional hide by key
  //  - default enforcement / normalization
  //  - helper to keep a sidebar filter <select> in sync
  //
  // designed to be editor-agnostic and easy to extend.
  // ------------------------------------------------------------

  const core = (window.EditorCore = window.EditorCore || {});

  const asString = (v) => String(v ?? '').trim();
  const uniq = (arr) => {
    const out = [];
    const seen = new Set();
    for (const x of Array.isArray(arr) ? arr : []) {
      const s = asString(x);
      if (!s || seen.has(s)) continue;
      seen.add(s);
      out.push(s);
    }
    return out;
  };

  const getValue = (data, path) => {
    if (!path) return undefined;
    const fr = core.FieldRenderer;
    if (!fr?.getValueAtPath) return data?.[path];
    return fr.getValueAtPath(data, path);
  };

  const setValue = (data, path, value) => {
    if (!path) return;
    const fr = core.FieldRenderer;
    if (!fr?.setValueAtPath) {
      data[path] = value;
      return;
    }
    fr.setValueAtPath(data, path, value);
  };

  const normalizeRuleEntry = (entry) => {
    // supported formats:
    //  - ['A','B']
    //  - { options: [...], hide: true/false, defaultValue: '...' }
    //  - function(key, data) => any of the above
    if (typeof entry === 'function') return entry;
    if (Array.isArray(entry)) return { options: entry };
    if (entry && typeof entry === 'object') return entry;
    return { options: [] };
  };

  const create = (cfg = {}) => {
    const dependsOnPath = asString(cfg.dependsOnPath || cfg.dependsOn || '');
    const valuePath = asString(cfg.valuePath || cfg.value || '');
    const defaultValue = asString(cfg.defaultValue ?? 'Default');

    const hideWhen = uniq(cfg.hideWhen || cfg.hidden || cfg.hide || []);
    const hideSet = new Set(hideWhen.map(asString));

    const map = cfg.mapping || cfg.map || cfg.rules || {};
    const fallback = cfg.fallbackOptions || cfg.fallback || cfg.allOptions || [];
    const allowAllWhenMissing = cfg.allowAllWhenMissing !== false;

    const resolveEntry = (key, data) => {
      const k = asString(key);
      let entry = map?.[k];
      if (entry === undefined && map?.['*'] !== undefined) entry = map['*'];
      if (entry === undefined && map?.['__default__'] !== undefined) entry = map['__default__'];

      const normalized = normalizeRuleEntry(entry);
      if (typeof normalized === 'function') {
        try {
          return normalizeRuleEntry(normalized(k, data));
        } catch {
          return { options: [] };
        }
      }
      return normalized;
    };

    const isHiddenForKey = (key, data) => {
      const k = asString(key);
      if (hideSet.has(k)) return true;
      const entry = resolveEntry(k, data);
      return !!entry?.hide;
    };

    const allowedForKey = (key, data) => {
      const k = asString(key);
      const entry = resolveEntry(k, data);
      const list = uniq(entry?.options || []);
      if (list.length) return list;
      return allowAllWhenMissing ? uniq(fallback) : [];
    };

    const normalizeValueForKey = (key, currentValue, data) => {
      const k = asString(key);
      const cur = asString(currentValue);
      if (isHiddenForKey(k, data)) return defaultValue;

      const allowed = allowedForKey(k, data);
      if (!allowed.length) return cur || defaultValue;

      if (cur && allowed.includes(cur)) return cur;
      if (allowed.includes(defaultValue)) return defaultValue;
      return allowed[0];
    };

    const normalizeModel = (model) => {
      if (!model || typeof model !== 'object') return model;
      if (!valuePath || !dependsOnPath) return model;
      const key = getValue(model, dependsOnPath);
      const cur = getValue(model, valuePath);
      const next = normalizeValueForKey(key, cur, model);
      setValue(model, valuePath, next);
      return model;
    };

    const applyToPayload = (payload, model) => {
      if (!payload || typeof payload !== 'object') return payload;
      if (!valuePath || !dependsOnPath) return payload;
      const src = model && typeof model === 'object' ? model : payload;
      const key = getValue(src, dependsOnPath);
      const cur = getValue(payload, valuePath);
      const next = normalizeValueForKey(key, cur, src);
      setValue(payload, valuePath, next);
      return payload;
    };

    const fieldUI = (opts = {}) => {
      // opts:
      //  - fallbackOptions: override fallback list
      //  - hideWhen: override hidden keys
      const fallbackOverride = opts?.fallbackOptions || fallback;
      const hideOverride = opts?.hideWhen ? new Set(uniq(opts.hideWhen).map(asString)) : null;
      return {
        // hide whole field when rule says so
        showWhen: (data) => {
          const key = getValue(data, dependsOnPath);
          const k = asString(key);
          if (hideOverride?.has(k)) return false;
          return !isHiddenForKey(k, data);
        },
        // dynamic options based on dependsOn
        options: (data) => {
          const key = getValue(data, dependsOnPath);
          const k = asString(key);
          const entry = resolveEntry(k, data);
          const list = uniq(entry?.options || []);
          if (list.length) return list;
          return allowAllWhenMissing ? uniq(fallbackOverride) : [];
        }
      };
    };

    const syncFilterSelect = (selectEl, params = {}) => {
      // params:
      //  - keyValue: current dependsOn value
      //  - allOptions: fallback options
      //  - anyValue: value for "all" option (default '')
      //  - anyLabel: label for "all" option
      //  - hideMode: 'display' | 'disabled' (default 'display')
      if (!selectEl) return;

      const keyValue = params.keyValue;
      const k = asString(keyValue);
      const hideMode = params.hideMode || 'display';

      const hidden = isHiddenForKey(k, null);
      if (hidden) {
        selectEl.value = asString(params.anyValue ?? '');
        selectEl.disabled = true;
        if (hideMode === 'display') selectEl.style.display = 'none';
        return;
      }

      selectEl.disabled = false;
      selectEl.style.display = '';

      const allowed = allowedForKey(k, null);
      const list = allowed.length ? allowed : uniq(params.allOptions || fallback);
      const prev = asString(selectEl.value);

      const anyValue = asString(params.anyValue ?? '');
      const anyLabel = asString(params.anyLabel ?? 'Все');

      const esc = core.utils?.escapeAttr
        ? (x) => core.utils.escapeAttr(x)
        : (x) => String(x).replace(/"/g, '&quot;');

      selectEl.innerHTML = [
        `<option value="${esc(anyValue)}">${anyLabel}</option>`,
        ...list.map((v) => `<option value="${esc(v)}">${v}</option>`)
      ].join('');

      // keep previous if still valid
      if (prev && list.includes(prev)) selectEl.value = prev;
      else selectEl.value = anyValue;
    };

    return {
      dependsOnPath,
      valuePath,
      defaultValue,
      hiddenKeys: hideWhen,
      isHidden: (key, data) => isHiddenForKey(key, data),
      allowed: (key, data) => allowedForKey(key, data),
      normalizeValue: (key, value, data) => normalizeValueForKey(key, value, data),
      normalizeModel,
      applyToPayload,
      fieldUI,
      syncFilterSelect
    };
  };

  core.DependentSelect = { create };
})();
