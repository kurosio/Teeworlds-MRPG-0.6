/* Shared collecting-node codec. Legacy rows retain their random 1–2 quantity. */
((root) => {
  'use strict';
  const parse = (text) => {
    if (!String(text ?? '').trim()) return [];
    return String(text).split(',').map((entry) => {
      const match = entry.trim().match(/^\[(\d+)\/(?:(\d+)\/)?(\d+(?:\.\d+)?(?:e[+-]?\d+)?)\]$/i);
      if (!match) throw new Error(`Invalid collecting property: ${entry}`);
      return { item_id: Number(match[1]), amount: match[2] === undefined ? 0 : Number(match[2]), chance: Number(match[3]) };
    });
  };
  const validate = (items) => {
    if (!Array.isArray(items) || !items.length) return 'Add at least one item.';
    const seen = new Set();
    for (const item of items) {
      const id = Number(item.item_id), amount = Number(item.amount), chance = Number(item.chance);
      if (!Number.isSafeInteger(id) || id < 1 || id > 2147483647) return 'Choose an item in every row.';
      if (seen.has(id)) return 'Duplicate drop items are not allowed.';
      seen.add(id);
      if (!Number.isInteger(amount) || amount < 0 || amount > 1000000) return 'Quantity must be an integer from 1 to 1000000 (0 means random 1–2).';
      if (!Number.isFinite(chance) || chance <= 0 || chance > 100) return 'Chance must be greater than 0 and at most 100.';
    }
    return '';
  };
  const serialize = (items) => items.map((item) => Number(item.amount) === 0
    ? `[${Number(item.item_id)}/${Number(item.chance)}]`
    : `[${Number(item.item_id)}/${Number(item.amount)}/${Number(item.chance)}]`).join(',');
  const api = { parse, validate, serialize, tiles: { plant: 199, ore: 200, fish: 201 } };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else { root.EditorCore = root.EditorCore || {}; root.EditorCore.CollectingProperties = api; }
})(typeof window !== 'undefined' ? window : globalThis);
