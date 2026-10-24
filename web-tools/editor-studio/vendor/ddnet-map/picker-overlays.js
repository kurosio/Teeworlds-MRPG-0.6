/* Read-only editing aids. No DOM or WebGL dependencies; coordinates are world pixels. */
((root) => {
  'use strict';
  const tileAt = (game, point) => {
    if (!game || !point || !Number.isFinite(point.x) || !Number.isFinite(point.y)) return null;
    const x = Math.floor(point.x / 32), y = Math.floor(point.y / 32);
    if (x < 0 || y < 0 || x >= game.width || y >= game.height) return null;
    return game.indices[y * game.width + x];
  };
  const distance = (a, b) => ({ dx: b.x - a.x, dy: b.y - a.y, length: Math.hypot(b.x - a.x, b.y - a.y) });
  const visibleTiles = (game, min, max, limit = 60000) => {
    if (!game) return null;
    const x0 = Math.max(0, Math.floor(min.x / 32)), y0 = Math.max(0, Math.floor(min.y / 32));
    const x1 = Math.min(game.width, Math.ceil(max.x / 32)), y1 = Math.min(game.height, Math.ceil(max.y / 32));
    if (Math.max(0, x1 - x0) * Math.max(0, y1 - y0) > limit) return null;
    return { x0, y0, x1, y1 };
  };
  const colors = { 1: '#ffb454', 2: '#ff536b', 3: '#b78bfa', 192: '#52df9a', 193: '#52df9a', 194: '#52df9a' };
  const drawCollision = (ctx, game, bounds, project) => {
    if (!bounds || !game) return;
    ctx.save();
    ctx.globalAlpha = .65;
    const origin = project(0, 0), end = project(32, 32);
    const size = { x: end.x - origin.x, y: end.y - origin.y };
    for (let y = bounds.y0; y < bounds.y1; y++) for (let x = bounds.x0; x < bounds.x1; x++) {
      const index = game.indices[y * game.width + x];
      if (!index) continue;
      const p = project(x * 32, y * 32);
      ctx.fillStyle = colors[index] || '#5ac8fa';
      ctx.fillRect(p.x, p.y, Math.max(1, size.x), Math.max(1, size.y));
      if (size.x >= 22) {
        ctx.fillStyle = '#101722'; ctx.font = '11px monospace';
        ctx.fillText(String(index), p.x + 3, p.y + 13);
      }
    }
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
  const api = { tileAt, distance, visibleTiles, drawCollision, drawRadius, drawMeasure };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MapPickerOverlays = api;
})(typeof window !== 'undefined' ? window : globalThis);
