/* Typed record references. Rules are shared with PHP via reference-schema.json.
 * Only declared IDs are visited: quantities, coordinates and ordinary text are never IDs. */
(() => {
  const tokens = { item: 'items', bot: 'bots_info', world: 'worlds', quest: 'quests' };
  const transform = (resource, data, schema, visit) => {
    const result = { ...data };
    for (const rule of schema?.[resource] || []) {
      if (!Object.prototype.hasOwnProperty.call(result, rule.column)) continue;
      const ref = (value, target = rule.target) => {
        if (!['number', 'string'].includes(typeof value)) return value;
        if (!/^[0-9]+$/.test(String(value)) || !Number.isSafeInteger(Number(value))) return value;
        const mapped = visit(Number(value), target, rule.column);
        return mapped === Number(value) ? value : typeof value === 'string' ? String(mapped) : mapped;
      };
      const path = (node, parts, target) => {
        if (!parts.length) return ref(node, target);
        if (!node || typeof node !== 'object') return node;
        const [key, ...rest] = parts;
        for (const k of key === '*' ? Object.keys(node) : [key]) {
          if (Object.prototype.hasOwnProperty.call(node, k)) node[k] = path(node[k], rest, target);
        }
        return node;
      };
      const walk = (node) => {
        if (Array.isArray(node) || (node && typeof node === 'object')) {
          // Recurse first: each declared leaf is visited exactly once.
          Object.keys(node).forEach(k => { node[k] = walk(node[k]); });
          for (const [p, target] of Object.entries(rule.recursive || {})) node = path(node, p.split('.'), target);
        } else if (rule.tokens && typeof node === 'string') {
          node = node.replace(/<(item|bot|world|quest)_([0-9]+)>/g,
            (all, type, id) => `<${type}_${ref(id, tokens[type])}>`);
        }
        return node;
      };
      const original = result[rule.column];
      if (rule.codec === 'id') result[rule.column] = ref(original);
      else if (rule.codec === 'csv' && typeof original === 'string') {
        result[rule.column] = original.replace(/(^|,)(\s*)([0-9]+)(?=\s*(?:,|$))/g,
          (all, comma, space, id) => comma + space + ref(id));
      } else if ((rule.codec === 'brackets' || rule.codec === 'pairs') && typeof original === 'string' && !/^\s*\[\s*[\[{]/.test(original)) {
        result[rule.column] = original.replace(/(\[\s*)([0-9]+)(?=\s*\/)/g,
          (all, prefix, id) => prefix + ref(id));
      } else if (rule.codec === 'json' || rule.codec === 'pairs') {
        let parsed;
        try { parsed = typeof original === 'string' ? JSON.parse(original) : JSON.parse(JSON.stringify(original)); }
        catch { continue; } // Unknown/malformed content is not rewritten.
        if (!parsed || typeof parsed !== 'object') continue;
        const before = JSON.stringify(parsed);
        if (rule.codec === 'pairs') {
          if (Array.isArray(parsed)) for (const pair of parsed) {
            if (!pair || typeof pair !== 'object') continue;
            const key = Array.isArray(pair) ? 0 : ['id', 'ItemID', 'item_id', 'key'].find(k => Object.prototype.hasOwnProperty.call(pair, k));
            if (key !== undefined && Object.prototype.hasOwnProperty.call(pair, key)) pair[key] = ref(pair[key]);
          }
        } else {
          parsed = walk(parsed);
          for (const [p, target] of Object.entries(rule.paths || {})) parsed = path(parsed, p.split('.'), target);
        }
        result[rule.column] = typeof original === 'string'
          ? (JSON.stringify(parsed) === before ? original : JSON.stringify(parsed)) : parsed;
      }
    }
    return result;
  };
  const api = {
    transform,
    remap: (resource, data, schema, idMap) => transform(resource, data, schema, id =>
      Object.prototype.hasOwnProperty.call(idMap, id) ? Number(idMap[id]) : id),
    references: (resource, data, schema) => {
      const refs = [];
      transform(resource, data, schema, (id, target, column) => { refs.push({ id, target, column }); return id; });
      return refs;
    },
  };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else {
    window.EditorCore = window.EditorCore || {};
    window.EditorCore.ReferenceData = api;
  }
})();
