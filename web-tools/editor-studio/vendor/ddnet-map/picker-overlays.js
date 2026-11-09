/* Read-only editing aids. No DOM or WebGL dependencies; coordinates are world pixels. */
((root) => {
  'use strict';
  const tileAt = (game, point) => {
    if (!game || !point || !Number.isFinite(point.x) || !Number.isFinite(point.y)) return null;
    const x = Math.floor(point.x / 32), y = Math.floor(point.y / 32);
    if (x < 0 || y < 0 || x >= game.width || y >= game.height) return null;
    return game.indices[y * game.width + x];
  };
  /** Позиции в мировых пикселях для связанных игровых тайлов (например mob spawns). */
  const findHighlightedTiles = (game, tileIds, limit = 5000) => {
    if (!game || !Array.isArray(tileIds) || !tileIds.length || !Number.isInteger(game.width)
      || !Number.isInteger(game.height) || !game.indices) return [];
    const ids = new Set(tileIds);
    const points = [];
    const size = Math.min(game.width * game.height, game.indices.length);
    for (let i = 0; i < size && points.length < limit; i++) {
      if (!ids.has(game.indices[i])) continue;
      const x = i % game.width, y = Math.floor(i / game.width);
      // index — номер клетки в слое: по нему черновик правок и панель
      // выделяют ровно ту точку, которую выбрал пользователь.
      points.push({ x: x * 32 + 16, y: y * 32 + 16, tile: game.indices[i], index: i });
    }
    return points;
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
  /**
   * Игровой слой поверх карты.
   *   blocked   — ID, на которые нельзя ставить точку (красные);
   *   highlight — ID, которые подсветил редактор (зелёные: спавны мобов и
   *               прочие объявленные сущности) — это то, что обещает легенда
   *               «зелёный: спавны · синий: прочие ID».
   * Раньше `highlight` не передавался вовсе, и спавн мобов 209 выглядел
   * обычным синим «прочим ID» — рядом с подписью «Кости казино».
   */
  const drawCollision = (ctx, game, bounds, project, blocked, highlight) => {
    if (!bounds || !game) return;
    ctx.save();
    ctx.globalAlpha = .65;
    const origin = project(0, 0), end = project(32, 32);
    const size = { x: end.x - origin.x, y: end.y - origin.y };
    const forbidden = Array.isArray(blocked) ? blocked : [];
    const selected = Array.isArray(highlight) ? highlight : [];
    for (let y = bounds.y0; y < bounds.y1; y++) for (let x = bounds.x0; x < bounds.x1; x++) {
      const index = game.indices[y * game.width + x];
      if (!index) continue;
      const p = project(x * 32, y * 32);
      const isBlocked = forbidden.includes(index);
      // Запрещённые тайлы выделены отдельно: их видно даже без подписей ID.
      ctx.fillStyle = isBlocked ? '#f2727f'
        : selected.includes(index) ? '#52df9a' : (colors[index] || '#5ac8fa');
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
  // highlightTiles identifies entity tile types; switchShowID narrows by switch
  // number and labels every matching location. No gameplay layer is required.
  const drawSwitchTiles = (ctx, layer, project, tileIds = [], switchShowID = null) => {
    if (!layer || !tileIds.length) return 0;
    ctx.save();
    ctx.strokeStyle = '#52df9a'; ctx.fillStyle = '#ffffff'; ctx.lineWidth = 2;
    ctx.font = 'bold 12px monospace';
    let count = 0;
    for (const tile of layer.tiles) {
      if (!tileIds.includes(tile.type) || (switchShowID !== null && tile.number !== switchShowID)) continue;
      count++;
      const p = project(tile.x, tile.y), edge = project(tile.x + 12, tile.y);
      ctx.beginPath(); ctx.arc(p.x, p.y, Math.max(4, Math.abs(edge.x - p.x)), 0, Math.PI * 2); ctx.stroke();
      // A dark backing keeps IDs legible over both bright and dark maps.
      const label = `#${tile.number}`;
      ctx.fillStyle = '#101722'; ctx.fillRect(p.x + 5, p.y - 18, label.length * 8 + 4, 16);
      ctx.fillStyle = '#ffffff'; ctx.fillText(label, p.x + 7, p.y - 6);
    }
    ctx.restore();
    return count;
  };
  /**
   * Кольца всех точек игрового слоя в видимой области. `isPoint(index)`
   * решает, что считать точкой (обычно индекс >= 192), поэтому каталог ID
   * остаётся в вызывающем коде.
   */
  const drawGamePoints = (ctx, game, bounds, project, isPoint, style = {}) => {
    if (!bounds || !game || typeof isPoint !== 'function') return 0;
    const origin = project(0, 0), end = project(32, 32);
    const size = Math.max(1, Math.min(Math.abs(end.x - origin.x), Math.abs(end.y - origin.y)));
    const color = style.color || '#52df9a';
    let count = 0;
    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    for (let y = bounds.y0; y < bounds.y1; y++) {
      for (let x = bounds.x0; x < bounds.x1; x++) {
        const index = game.indices[y * game.width + x];
        if (!isPoint(index)) continue;
        count++;
        const p = project(x * 32 + 16, y * 32 + 16);
        ctx.beginPath();
        ctx.arc(p.x, p.y, Math.max(3, size / 3), 0, Math.PI * 2);
        ctx.stroke();
        if (style.labels && size >= 18) {
          const text = String(index);
          ctx.font = '10px monospace';
          ctx.textAlign = 'center';
          ctx.fillStyle = 'rgba(0,0,0,.55)';
          ctx.fillText(text, p.x, p.y - Math.max(6, size / 3) - 3);
        }
      }
    }
    ctx.restore();
    return count;
  };

  /**
   * Точки switch-слоя: `cells` — Map(index → { number, type }) с координатами
   * слоя, `size` — { width, height }. Показываются только занятые клетки.
   */
  const drawSwitchPoints = (ctx, cells, size, project, style = {}) => {
    if (!cells || !size) return 0;
    const origin = project(0, 0), end = project(32, 32);
    const radius = Math.max(3, Math.min(Math.abs(end.x - origin.x), Math.abs(end.y - origin.y)) / 3);
    let count = 0;
    ctx.save();
    ctx.strokeStyle = style.color || '#52df9a';
    ctx.lineWidth = 2;
    for (const [index, value] of cells) {
      if (!value || !value.type) continue;
      const x = index % size.width, y = Math.floor(index / size.width);
      const p = project(x * 32 + 16, y * 32 + 16);
      ctx.beginPath();
      ctx.arc(p.x, p.y, radius, 0, Math.PI * 2);
      ctx.stroke();
      count++;
      if (style.labels && radius >= 6) {
        const label = `#${value.number}`;
        ctx.fillStyle = '#101722';
        ctx.fillRect(p.x + 4, p.y - 16, label.length * 7 + 4, 14);
        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 10px monospace';
        ctx.fillText(label, p.x + 6, p.y - 5);
      }
    }
    ctx.restore();
    return count;
  };

  /**
   * Черновые правки: зелёная заливка — добавлено, красный крест — удалено,
   * жёлтая рамка — клетка, которую перенесли.
   */
  const drawEditMarks = (ctx, marks, project) => {
    if (!Array.isArray(marks) || !marks.length) return;
    ctx.save();
    for (const mark of marks) {
      if (!mark || !Number.isFinite(mark.x) || !Number.isFinite(mark.y)) continue;
      const p = project(mark.x * 32, mark.y * 32), q = project(mark.x * 32 + 32, mark.y * 32 + 32);
      const w = Math.max(2, q.x - p.x), h = Math.max(2, q.y - p.y);
      if (mark.kind === 'delete') {
        ctx.strokeStyle = '#ff536b';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(p.x + 4, p.y + 4); ctx.lineTo(p.x + w - 4, p.y + h - 4);
        ctx.moveTo(p.x + w - 4, p.y + 4); ctx.lineTo(p.x + 4, p.y + h - 4);
        ctx.stroke();
      } else if (mark.kind === 'move') {
        ctx.strokeStyle = '#ffda70';
        ctx.lineWidth = 1.5;
        ctx.strokeRect(p.x + 1, p.y + 1, w - 2, h - 2);
      } else {
        ctx.fillStyle = 'rgba(82,223,154,.3)';
        ctx.fillRect(p.x, p.y, w, h);
        ctx.strokeStyle = '#52df9a';
        ctx.lineWidth = 1.5;
        ctx.strokeRect(p.x + 1, p.y + 1, w - 2, h - 2);
      }
    }
    ctx.restore();
  };

  /** Выделенная клетка: заметная рамка с подписью. */
  const drawSelection = (ctx, cell, project, caption, color = '#ffda70') => {
    if (!cell || !Number.isFinite(cell.x) || !Number.isFinite(cell.y)) return;
    const p = project(cell.x * 32, cell.y * 32), q = project(cell.x * 32 + 32, cell.y * 32 + 32);
    const w = Math.max(2, q.x - p.x), h = Math.max(2, q.y - p.y);
    ctx.save();
    ctx.strokeStyle = 'rgba(0,0,0,.6)';
    ctx.lineWidth = 5;
    ctx.strokeRect(p.x + 1, p.y + 1, w - 2, h - 2);
    ctx.strokeStyle = color;
    ctx.lineWidth = 2.5;
    ctx.strokeRect(p.x + 1, p.y + 1, w - 2, h - 2);
    if (caption) {
      const width = String(caption).length * 6.3 + 10;
      ctx.fillStyle = 'rgba(0,0,0,.78)';
      ctx.fillRect(p.x, p.y - 18, width, 16);
      ctx.fillStyle = color;
      ctx.font = '600 11px Inter, system-ui, sans-serif';
      ctx.fillText(String(caption), p.x + 5, p.y - 6);
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
  /** Полупрозрачная заливка из hex-цвета: у каждой окружности свой оттенок. */
  const fillFromColor = (color) => {
    const match = /^#([0-9a-f]{6})$/i.exec(color || '');
    if (!match) return 'rgba(111,140,255,.07)';
    const value = parseInt(match[1], 16);
    return `rgba(${(value >> 16) & 255},${(value >> 8) & 255},${value & 255},.09)`;
  };
  // Радиус из объявления UI: цвет, пунктир и подпись приходят готовыми.
  // `fill: false` — только контур, `dash: []` — сплошная линия.
  const drawRadius = (ctx, point, radius, project, style = {}) => {
    if (!point || !Number.isFinite(radius) || radius <= 0) return;
    const center = project(point.x, point.y), edge = project(point.x + radius, point.y);
    const color = style.color || '#52df9a';
    const fill = style.fill === false ? null
      : (typeof style.fill === 'string' && style.fill ? style.fill : fillFromColor(color));
    const dash = Array.isArray(style.dash) ? style.dash : [6, 4];
    const lineWidth = Number.isFinite(style.lineWidth) && style.lineWidth > 0 ? style.lineWidth : 2;
    const screenRadius = Math.abs(edge.x - center.x);
    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = lineWidth;
    if (dash.length && ctx.setLineDash) ctx.setLineDash(dash);
    ctx.beginPath(); ctx.arc(center.x, center.y, screenRadius, 0, Math.PI * 2);
    if (fill) { ctx.fillStyle = fill; ctx.fill(); }
    ctx.stroke();
    ctx.restore();

    if (style.label) {
      const ex = center.x + screenRadius;
      const ey = center.y;
      const text = `${style.label}: ${Math.round(radius)} px`;
      ctx.save();
      ctx.translate(ex, ey);
      ctx.textAlign = 'left'; ctx.textBaseline = 'middle';
      ctx.font = '11px sans-serif';
      // measureText есть не в каждом окружении (например, в тестовом canvas).
      const metric = typeof ctx.measureText === 'function' ? ctx.measureText(text) : null;
      const width = metric && Number.isFinite(metric.width) ? metric.width : text.length * 6.3;
      ctx.fillStyle = 'rgba(0,0,0,.7)';
      ctx.fillRect(4, -10, width + 8, 18);
      ctx.fillStyle = color;
      ctx.fillText(text, 8, 0);
      ctx.restore();
    }
  };
  const drawMeasure = (ctx, a, b, project) => {
    if (!a || !b) return;
    const start = project(a.x, a.y), end = project(b.x, b.y);
    ctx.save(); ctx.strokeStyle = '#ffda70'; ctx.fillStyle = '#ffda70'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(start.x, start.y); ctx.lineTo(end.x, end.y); ctx.stroke();
    for (const p of [start, end]) { ctx.beginPath(); ctx.arc(p.x, p.y, 4, 0, Math.PI * 2); ctx.fill(); }
    ctx.restore();
  };
  const api = { drawSwitchTiles, drawGamePoints, drawSwitchPoints, drawEditMarks, drawSelection, tileAt, findHighlightedTiles, blockedTile, distance, visibleTiles, drawCollision, drawHighlightedTiles, drawTileBox, drawRadius, drawMeasure };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MapPickerOverlays = api;
})(typeof window !== 'undefined' ? window : globalThis);
