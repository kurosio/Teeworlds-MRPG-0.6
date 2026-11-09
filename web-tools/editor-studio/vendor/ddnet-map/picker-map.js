/* Геометрия окна карты: экран ↔ мир, размеры вида, рамка карты.
 *
 * Всё, что раньше было размазано по picker.js (расчёт вида, перевод координат,
 * зум «вся карта», границы карты), собрано здесь. Модуль не трогает DOM:
 * размер экрана приходит функцией `screenSize`, состояние камеры живёт в `tw`
 * (cameraPos, cameraZoom, worldView), поэтому результат совпадает с рендером
 * twwebgl пиксель в пиксель.
 */
((root) => {
  'use strict';

  const TILE = 32;

  /** Центр вида в пикселях мира при текущем зуме. */
  const viewSize = (tw, screen) => {
    const zoom = tw.cameraZoom || 1;
    return {
      w: (tw.worldView[0] * (screen.w / screen.h)) / zoom,
      h: tw.worldView[1] / zoom,
    };
  };

  const screenToWorld = (tw, screen, sx, sy) => {
    const view = viewSize(tw, screen);
    return {
      x: tw.cameraPos[0] - view.w / 2 + (sx / screen.w) * view.w,
      y: tw.cameraPos[1] - view.h / 2 + (sy / screen.h) * view.h,
    };
  };

  const worldToScreen = (tw, screen, wx, wy) => {
    const view = viewSize(tw, screen);
    const scale = screen.w / view.w; // CSS-пикселей экрана на пиксель мира
    return {
      x: (wx - (tw.cameraPos[0] - view.w / 2)) * scale,
      y: (wy - (tw.cameraPos[1] - view.h / 2)) * scale,
    };
  };

  const clampZoom = (tw, zoom) => Math.min(tw.ZOOM_MAX, Math.max(tw.ZOOM_MIN, zoom));

  /** Зум, при котором карта целиком помещается на экране. */
  const fitZoom = (tw, screen, extent) => {
    const { w, h } = extent || {};
    if (!w || !h) return 1;
    const aspect = screen.w / screen.h;
    return clampZoom(tw, Math.min(tw.worldView[1] / h, (tw.worldView[0] * aspect) / w) * 0.94);
  };

  /** Приближение «окрестности точки»: около 1400 px мира по высоте. */
  const pointZoom = (tw, screen, extent) => {
    const fit = fitZoom(tw, screen, extent);
    return clampZoom(tw, Math.max(fit, Math.min(1.6, tw.worldView[1] / 1400)));
  };

  /**
   * Границы карты в пикселях мира: сначала игровой слой, затем тайловые и
   * квадовые слои, затем вершины квадов. Без карты остаётся 1024×1024.
   */
  const computeExtent = (tw) => {
    let w = (tw.map?.gameplay?.width || 0) * TILE;
    let h = (tw.map?.gameplay?.height || 0) * TILE;
    if (w && h) return { w, h };
    const groups = (tw.map && tw.map.groups) || [];
    for (const group of groups) {
      for (const layer of group.layers || []) {
        if (layer.type !== tw.LAYERTYPE_TILES && layer.type !== tw.LAYERTYPE_QUADS) continue;
        const size = layer.tileSize || TILE;
        const lw = layer.width || 0, lh = layer.height || 0;
        if (lw && lh) { w = Math.max(w, lw * size); h = Math.max(h, lh * size); }
      }
    }
    if (!w || !h) {
      for (const group of groups) {
        for (const layer of group.layers || []) {
          for (const quad of layer.quads || []) {
            for (const p of quad.points || []) {
              w = Math.max(w, p.x || 0);
              h = Math.max(h, p.y || 0);
            }
          }
        }
      }
    }
    return { w: Math.max(w, 1024), h: Math.max(h, 1024) };
  };

  /** Клетка под экранной точкой (в координатах слоя) или null. */
  const tileAtScreen = (tw, screen, x, y, size) => {
    const world = screenToWorld(tw, screen, x, y);
    const tx = Math.floor(world.x / TILE), ty = Math.floor(world.y / TILE);
    if (!size || tx < 0 || ty < 0 || tx >= size.width || ty >= size.height) return null;
    return { x: tx, y: ty, index: ty * size.width + tx };
  };

  const api = { TILE, viewSize, screenToWorld, worldToScreen, clampZoom, fitZoom, pointZoom, computeExtent, tileAtScreen };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MapPickerMap = api;
})(typeof window !== 'undefined' ? window : globalThis);
