/* Read-only editing aids. No DOM or WebGL dependencies; coordinates are world pixels. */
((root) => {
  'use strict';
  const tileAt = (game, point) => {
    if (!game || !point || !Number.isFinite(point.x) || !Number.isFinite(point.y)) return null;
    const x = Math.floor(point.x / 32), y = Math.floor(point.y / 32);
    if (x < 0 || y < 0 || x >= game.width || y >= game.height) return null;
    return game.indices[y * game.width + x];
  };
  /**
   * Индекс запрещённого тайла под точкой или null. Вне игрового слоя и пустой
   * тайл (0) никогда не блокируют размещение: карта без игрового слоя не
   * должна превращаться в «нельзя применить вообще ничего».
   */
  const blockedTile = (game, point, blocked) => {
    if (!game || !Array.isArray(blocked) || !blocked.length) return null;
    const index = tileAt(game, point);
    return index !== null && blocked.includes(index) ? index : null;
  };
  const distance = (a, b) => ({ dx: b.x - a.x, dy: b.y - a.y, length: Math.hypot(b.x - a.x, b.y - a.y) });
  const visibleTiles = (game, min, max, limit = 60000) => {
    if (!game) return null;
    const x0 = Math.max(0, Math.floor(min.x / 32)), y0 = Math.max(0, Math.floor(min.y / 32));
    const x1 = Math.min(game.width, Math.ceil(max.x / 32)), y1 = Math.min(game.height, Math.ceil(max.y / 32));
    if (Math.max(0, x1 - x0) * Math.max(0, y1 - y0) > limit) return null;
    return { x0, y0, x1, y1 };
  };
  const colors = { 1: '#ffb454', 2: '#ff536b', 3: '#b78bfa' };
  const drawCollision = (ctx, game, bounds, project, blocked) => {
    if (!bounds || !game) return;
    ctx.save();
    ctx.globalAlpha = .65;
    const origin = project(0, 0), end = project(32, 32);
    const size = { x: end.x - origin.x, y: end.y - origin.y };
    const highlight = Array.isArray(blocked) ? blocked : [];
    for (let y = bounds.y0; y < bounds.y1; y++) for (let x = bounds.x0; x < bounds.x1; x++) {
      const index = game.indices[y * game.width + x];
      if (!index) continue;
      const p = project(x * 32, y * 32);
      const isBlocked = highlight.includes(index);
      // Запрещённые тайлы выделены отдельно: их видно даже без подписей ID.
      ctx.fillStyle = isBlocked ? '#f2727f' : (colors[index] || '#5ac8fa');
      ctx.fillRect(p.x, p.y, Math.max(1, size.x), Math.max(1, size.y));
      if (isBlocked && size.x >= 8 && ctx.strokeRect) {
        ctx.strokeStyle = '#ff536b'; ctx.lineWidth = 2;
        ctx.strokeRect(p.x + 1, p.y + 1, Math.max(1, size.x - 2), Math.max(1, size.y - 2));
      }
      if (size.x >= 22) {
        ctx.fillStyle = '#101722'; ctx.font = '11px monospace';
        ctx.fillText(String(index), p.x + 3, p.y + 13);
      }
    }
    ctx.restore();
  };
  /** Выбранные тайлы видны и при отдалении (обычный игровой слой там молчит). */
  const drawHighlightedTiles = (ctx, game, bounds, project, tileIds = []) => {
    if (!bounds || !game || !Array.isArray(tileIds) || !tileIds.length) return;
    const origin = project(0, 0), end = project(32, 32);
    const w = Math.max(1, Math.abs(end.x - origin.x)), h = Math.max(1, Math.abs(end.y - origin.y));
    ctx.save();
    ctx.strokeStyle = '#52df9a';
    ctx.lineWidth = 2;
    for (let y = bounds.y0; y < bounds.y1; y++) for (let x = bounds.x0; x < bounds.x1; x++) {
      if (!tileIds.includes(game.indices[y * game.width + x])) continue;
      const p = project(x * 32 + 16, y * 32 + 16);
      const r = Math.max(3, Math.min(w, h) / 3);
      ctx.beginPath();
      ctx.arc(p.x, p.y, r, 0, Math.PI * 2);
      ctx.stroke();
    }
    ctx.restore();
  };
  /** Рамка одного тайла под курсором/точкой (цвет задаёт вызывающий код). */
  const drawTileBox = (ctx, worldX, worldY, project, fill, stroke) => {
    const tx = Math.floor(worldX / 32), ty = Math.floor(worldY / 32);
    const p = project(tx * 32, ty * 32), q = project(tx * 32 + 32, ty * 32 + 32);
    ctx.save();
    if (fill) { ctx.fillStyle = fill; ctx.fillRect(p.x, p.y, q.x - p.x, q.y - p.y); }
    if (stroke) { ctx.strokeStyle = stroke; ctx.lineWidth = 2; ctx.strokeRect(p.x + 1, p.y + 1, q.x - p.x - 2, q.y - p.y - 2); }
    ctx.restore();
  };
  const drawRadius = (ctx, point, radius, project) => {
    if (!point || !Number.isFinite(radius) || radius <= 0) return;
    const center = project(point.x, point.y), edge = project(point.x + radius, point.y);
    ctx.save();
    ctx.strokeStyle = '#52df9a'; ctx.fillStyle = 'rgba(82,223,154,.09)';
    ctx.lineWidth = 2; ctx.setLineDash([6, 4]);
    ctx.beginPath(); ctx.arc(center.x, center.y, Math.abs(edge.x - center.x), 0, Math.PI * 2);
    ctx.fill(); ctx.stroke(); ctx.restore();
  };
  const drawMeasure = (ctx, a, b, project) => {
    if (!a || !b) return;
    const start = project(a.x, a.y), end = project(b.x, b.y);
    ctx.save(); ctx.strokeStyle = '#ffda70'; ctx.fillStyle = '#ffda70'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(start.x, start.y); ctx.lineTo(end.x, end.y); ctx.stroke();
    for (const p of [start, end]) { ctx.beginPath(); ctx.arc(p.x, p.y, 4, 0, Math.PI * 2); ctx.fill(); }
    ctx.restore();
  };
  const api = { tileAt, blockedTile, distance, visibleTiles, drawCollision, drawHighlightedTiles, drawTileBox, drawRadius, drawMeasure };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MapPickerOverlays = api;
})(typeof window !== 'undefined' ? window : globalThis);
