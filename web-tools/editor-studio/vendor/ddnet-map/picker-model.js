/* Pure geometry and bounded point history; shared by the picker and Node tests. */
((root) => {
  'use strict';
  const TILE = 32;
  const tileIndex = value => Math.floor(value / TILE);
  const tileCenter = value => tileIndex(value) * TILE + TILE / 2;
  const point = (x, y, snap = false) => {
    if (!Number.isFinite(x) || !Number.isFinite(y)) return null;
    return { x: snap ? tileCenter(x) : Math.round(x), y: snap ? tileCenter(y) : Math.round(y) };
  };
  // Only visible grid lines are generated. At distant zoom levels use major
  // lines instead of drawing thousands of indistinguishable 32px cells.
  const grid = (min, max, pixelsPerWorldUnit) => {
    if (![min, max, pixelsPerWorldUnit].every(Number.isFinite) || pixelsPerWorldUnit <= 0 || max < min) return [];
    let step = TILE;
    while (step * pixelsPerWorldUnit < 8) step *= 2;
    const result = [];
    for (let n = Math.ceil(min / step) * step; n <= max && result.length < 2048; n += step) result.push(n);
    return result;
  };
  const createHistory = (initial, limit = 100) => {
    let entries = [{ ...initial }];
    let index = 0;
    const same = (a, b) => a.x === b.x && a.y === b.y && a.hasPoint === b.hasPoint;
    return {
      push(value) {
        if (same(entries[index], value)) return;
        entries = entries.slice(0, index + 1);
        entries.push({ ...value });
        if (entries.length > limit) entries.shift();
        index = entries.length - 1;
      },
      undo() { if (index > 0) index--; return { ...entries[index] }; },
      redo() { if (index < entries.length - 1) index++; return { ...entries[index] }; },
      get canUndo() { return index > 0; },
      get canRedo() { return index < entries.length - 1; },
    };
  };
  const api = { TILE, tileIndex, tileCenter, point, grid, createHistory };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MapPickerModel = api;
})(typeof window !== 'undefined' ? window : globalThis);
