/* Панель карты: связанные точки, черновик тайлов и слои оформления.
 *
 * Что здесь есть:
 *   • «Существующие точки» — координаты уже стоящих на карте тайлов из
 *     highlightTiles (например ENTITY_SPAWN_MOBS) как позиция записи;
 *   • «Новая точка на карте» — постановка тайла на пустую клетку: черновик
 *     живёт в памяти и уходит на сервер только по «Применить»;
 *   • «Слои оформления» — скрытие фоновых/передних слоёв в этом окне.
 *
 * Чего здесь больше нет: замер (picker-measure.js) и включение сетки/привязки
 * (шапка панели в picker.js). Каждый режим сам отвечает за свои обработчики,
 * поэтому клик по карте имеет ровно один смысл в зависимости от переключателя.
 */
(() => {
  'use strict';

  window.MapPickerWorkbench = {
    create(api) {
      const { tw, state, screenToWorld, worldToScreen, screenSize, redraw, options = {} } = api;
      const draw = window.MapPickerOverlays;
      const text = api.text;
      const get = (id) => document.getElementById(`pk-${id}`);
      // Тот же узел, с которым работают picker.js и picker-measure.js: события
      // карты сравниваются по идентичности элемента.
      const canvas = document.getElementById('cnvs');
      const el = Object.fromEntries([
        'collision', 'collision-label', 'spawn', 'collision-status',
        'layer-list', 'layers-reset', 'sec-layers',
        'sec-points', 'linked-tiles-wrap', 'linked-tiles', 'linked-tiles-label',
        'linked-tiles-use', 'linked-tiles-status',
        'sec-add', 'tile-edit-mode-wrap', 'tile-edit-mode', 'tile-edit-mode-label',
        'tile-edit-type-wrap', 'tile-edit-type-label', 'tile-edit-type',
        'tile-edit-clear', 'tile-edit-status',
      ].map((id) => [id, get(id)]));

      // Секции «Существующие точки» / «Новая точка» появляются только когда
      // редактор объявил linkedTiles + highlightTiles: иначе это пустые блоки.
      const linkedTilesEnabled = options.linkedTiles === true && options.highlightLayer !== 'switch'
        && Array.isArray(options.highlightTiles) && options.highlightTiles.length > 0;
      for (const id of ['sec-points', 'sec-add']) el[id].hidden = !linkedTilesEnabled;
      // Отдельные элементы внутри секций тоже скрываем: в разметке они без
      // атрибута hidden, чтобы секция целиком управлялась одним флагом.
      for (const id of ['tile-edit-mode-wrap', 'tile-edit-type-wrap', 'tile-edit-clear', 'tile-edit-status']) {
        el[id].hidden = !linkedTilesEnabled;
      }

      el['linked-tiles-label'].textContent = text.linkedLabel;
      el['linked-tiles-use'].textContent = text.linkedUse;
      el['tile-edit-mode-label'].textContent = text.editMode;
      el['tile-edit-type-label'].textContent = text.editType;
      el['tile-edit-clear'].textContent = text.editClear;
      el['collision-label'].textContent = text.collision;
      el.spawn.textContent = text.spawn;
      el['layers-reset'].textContent = text.layers;

      el.collision.checked = options.tiles === true;

      let layers = [];
      let linkedPoints = [];
      const pendingTileEdits = new Map();
      let tilePlacementMode = false;
      let tilePlacementPress = null;
      let tilePlacementTouch = null;
      let tilePlacementView = null;

      const setEditStatus = (message) => {
        el['tile-edit-status'].textContent = message;
        el['tile-edit-status'].hidden = !linkedTilesEnabled;
        api.onBadge?.(pendingTileEdits.size);
      };

      const refreshLinkedPoints = () => {
        if (!linkedTilesEnabled) return;
        linkedPoints = draw.findHighlightedTiles(tw.map?.gameplay, options.highlightTiles, 10000);
        el['linked-tiles'].replaceChildren();
        for (const [index, point] of linkedPoints.entries()) {
          const option = document.createElement('option');
          option.value = String(index);
          option.textContent = `${text.linkedPoint} ${index + 1} · X ${point.x}, Y ${point.y} · ID ${point.tile}`;
          el['linked-tiles'].appendChild(option);
        }
        // По умолчанию подсвечиваем точку, ближайшую к текущей позиции записи:
        // чаще всего правят именно её.
        if (linkedPoints.length && state.hasPoint) {
          let nearest = 0;
          let nearestDistance = Infinity;
          linkedPoints.forEach((point, index) => {
            const distance = Math.hypot(point.x - state.point.x, point.y - state.point.y);
            if (distance < nearestDistance) { nearest = index; nearestDistance = distance; }
          });
          el['linked-tiles'].value = String(nearest);
        } else if (linkedPoints.length) el['linked-tiles'].value = '0';
        el['linked-tiles-use'].disabled = linkedPoints.length === 0;
        el['linked-tiles-status'].textContent = linkedPoints.length === 0 ? text.linkedEmpty
          : (linkedPoints.length === 10000 ? text.linkedLimit.replace('{0}', '10000')
            : text.linkedCount.replace('{0}', String(linkedPoints.length)));
        api.onBadge?.(pendingTileEdits.size);
      };

      const setTilePlacementMode = (enabled) => {
        tilePlacementPress = tilePlacementTouch = null;
        const nextMode = !!enabled && !el['tile-edit-mode'].disabled;
        // Режим сам показывает игровой слой и сетку, а при выходе возвращает
        // прежние переключатели пользователя.
        if (nextMode && !tilePlacementMode) {
          tilePlacementView = { collision: el.collision.checked, grid: get('grid').checked };
        } else if (!nextMode && tilePlacementMode && tilePlacementView) {
          el.collision.checked = tilePlacementView.collision;
          get('grid').checked = tilePlacementView.grid;
          tilePlacementView = null;
        }
        tilePlacementMode = nextMode;
        el['tile-edit-mode'].checked = tilePlacementMode;
        if (tilePlacementMode) {
          el.collision.checked = true;
          get('grid').checked = true;
        }
        el['tile-edit-mode'].setAttribute('aria-pressed', String(tilePlacementMode));
        canvas.classList.toggle('is-placing-tile', tilePlacementMode);
        api.onModeChange?.(tilePlacementMode ? 'tile' : 'point');
        setEditStatus(tilePlacementMode ? text.editActive
          : (pendingTileEdits.size ? text.editSaved.replace('{0}', String(pendingTileEdits.size)) : text.editEmpty));
        redraw();
      };

      const stageTileAt = (worldX, worldY) => {
        const game = tw.map?.gameplay;
        const tile = Number(el['tile-edit-type'].value);
        if (!game || !Number.isInteger(tile) || tile < 4 || tile > 255) {
          setEditStatus(text.editUnsupported);
          return false;
        }
        const tx = Math.floor(worldX / 32);
        const ty = Math.floor(worldY / 32);
        if (tx < 0 || ty < 0 || tx >= game.width || ty >= game.height) {
          setEditStatus(text.editBlocked);
          return false;
        }
        const index = ty * game.width + tx;
        const current = game.indices[index];
        const staged = pendingTileEdits.get(index);
        if (staged) {
          staged.tile = tile;
          game.indices[index] = tile;
        } else if (current === 0) {
          if (pendingTileEdits.size >= 1000) {
            setEditStatus(text.editUnsupported);
            return false;
          }
          pendingTileEdits.set(index, { x: tx, y: ty, tile, oldTile: 0 });
          game.indices[index] = tile;
        } else if (current !== tile) {
          setEditStatus(text.editBlocked);
          return false;
        }
        el['tile-edit-clear'].disabled = pendingTileEdits.size === 0;
        // Список исходных точек не пересканируем: на мультимиллионном слое это
        // умножение работы на каждую из 1000 правок.
        setEditStatus(pendingTileEdits.size
          ? text.editSaved.replace('{0}', String(pendingTileEdits.size))
          : text.editEmpty);
        api.setPoint(tx * 32 + 16, ty * 32 + 16, { snap: false });
        redraw();
        return true;
      };

      const placeAtScreen = (x, y) => {
        const world = screenToWorld(x, y);
        return stageTileAt(world.x, world.y);
      };

      el.collision.addEventListener('change', redraw);

      el['tile-edit-mode'].addEventListener('change', () => setTilePlacementMode(el['tile-edit-mode'].checked));

      el['tile-edit-clear'].addEventListener('click', () => {
        const game = tw.map?.gameplay;
        if (game) for (const [index, change] of pendingTileEdits) game.indices[index] = change.oldTile;
        pendingTileEdits.clear();
        el['tile-edit-clear'].disabled = true;
        setTilePlacementMode(false);
        redraw();
      });

      el.spawn.addEventListener('click', () => {
        const spawn = tw.map?.spawnPoint;
        if (spawn) api.setCamera(spawn.x, spawn.y, Math.max(api.fitZoom(), .8));
      });

      el['linked-tiles-use'].addEventListener('click', () => {
        const index = Number(el['linked-tiles'].value);
        const point = Number.isInteger(index) ? linkedPoints[index] : null;
        if (!point) return;
        api.setPoint(point.x, point.y, { snap: false });
        api.setCamera(point.x, point.y, Math.max(api.fitZoom(), .8));
      });

      el['layers-reset'].addEventListener('click', () => {
        for (const { layer, input } of layers) { layer.editorHidden = false; input.checked = true; }
        redraw();
      });

      // Клик по карте в режиме тайла перехватываем до рендерера и picker.js:
      // иначе точка сдвинулась бы дважды — от тайла и от обычного клика.
      window.addEventListener('mousedown', (event) => {
        if (!tilePlacementMode || !state.ready || event.target !== canvas || event.button !== 0) return;
        event.preventDefault();
        event.stopImmediatePropagation();
        canvas.focus({ preventScroll: true });
        tilePlacementPress = { x: event.clientX, y: event.clientY };
      }, true);

      window.addEventListener('mouseup', (event) => {
        if (!tilePlacementPress) return;
        const start = tilePlacementPress;
        tilePlacementPress = null;
        event.preventDefault();
        event.stopImmediatePropagation();
        if (Math.hypot(start.x - event.clientX, start.y - event.clientY) < 6 && event.target === canvas) {
          placeAtScreen(event.clientX, event.clientY);
        }
      }, true);

      window.addEventListener('touchstart', (event) => {
        if (!tilePlacementMode || event.target !== canvas) return;
        if (event.touches.length !== 1) { tilePlacementTouch = null; return; }
        const point = event.touches[0];
        tilePlacementTouch = { x: point.clientX, y: point.clientY, moved: false };
        event.preventDefault();
        event.stopImmediatePropagation();
      }, { capture: true, passive: false });

      window.addEventListener('touchmove', (event) => {
        if (!tilePlacementTouch) return;
        if (event.touches.length !== 1) { tilePlacementTouch = null; return; }
        const point = event.touches[0];
        if (Math.hypot(point.clientX - tilePlacementTouch.x, point.clientY - tilePlacementTouch.y) > 10) {
          tilePlacementTouch.moved = true;
        }
        event.preventDefault();
        event.stopImmediatePropagation();
      }, { capture: true, passive: false });

      window.addEventListener('touchend', (event) => {
        if (!tilePlacementTouch) return;
        const start = tilePlacementTouch;
        tilePlacementTouch = null;
        event.preventDefault();
        event.stopImmediatePropagation();
        const point = event.changedTouches[0];
        if (!start.moved && point) placeAtScreen(point.clientX, point.clientY);
      }, { capture: true, passive: false });

      window.addEventListener('blur', () => { tilePlacementPress = tilePlacementTouch = null; });
      window.addEventListener('touchcancel', () => { tilePlacementPress = tilePlacementTouch = null; });

      return {
        get placementMode() { return tilePlacementMode; },
        getPendingTileChanges() {
          return [...pendingTileEdits.values()].map(({ x, y, tile }) => ({ x, y, tile }));
        },
        setEditStatus,
        /** Выход из режима тайла по Esc и переключению инструмента. */
        cancelPlacement() { if (tilePlacementMode) setTilePlacementMode(false); },
        /** Вход в режим тайла из переключателя «Клик по карте». */
        startPlacement() {
          if (el['tile-edit-mode'].disabled) return false;
          if (!tilePlacementMode) setTilePlacementMode(true);
          return true;
        },
        ready() {
          el.spawn.disabled = !tw.map?.spawnPoint;
          el.collision.disabled = !tw.map?.gameplay;
          if (linkedTilesEnabled) {
            refreshLinkedPoints();
            const editableIds = [...new Set(options.highlightTiles
              .filter((id) => Number.isInteger(id) && id >= 4 && id <= 255))];
            el['tile-edit-type'].replaceChildren();
            for (const id of editableIds) {
              const option = document.createElement('option');
              option.value = String(id);
              option.textContent = id === 209 ? 'ID 209 · ENTITY_SPAWN_MOBS' : `ID ${id}`;
              el['tile-edit-type'].appendChild(option);
            }
            if (editableIds.length) el['tile-edit-type'].value = String(editableIds[0]);
            el['tile-edit-mode'].disabled = editableIds.length === 0 || !tw.map?.gameplay || linkedPoints.length === 0;
            el['tile-edit-type'].disabled = editableIds.length === 0 || linkedPoints.length === 0 || !tw.map?.gameplay;
            el['tile-edit-clear'].disabled = true;
            try { window.MapPickerMapEdit.locateGameLayer(tw, tw.map?.dataFile); }
            catch { el['tile-edit-mode'].disabled = true; }
            setEditStatus(linkedPoints.length === 0 && editableIds.length > 0 && tw.map?.gameplay
              ? text.editNoSources
              : (el['tile-edit-mode'].disabled ? text.editUnsupported : text.editEmpty));
          }
          el['layer-list'].replaceChildren();
          layers = [];
          for (const [g, group] of (tw.map?.groups || []).entries()) {
            for (const [l, layer] of (group.layers || []).entries()) {
              const label = document.createElement('label');
              const input = document.createElement('input');
              input.type = 'checkbox';
              input.checked = layer.editorHidden !== true;
              const caption = document.createElement('span');
              caption.textContent = `${text.group} ${g + 1} / ${l + 1}: `
                + `${layer.type === tw.LAYERTYPE_TILES ? text.tileLayer : text.quadLayer}`
                + `${layer.width ? ` (${layer.width}×${layer.height})` : ''}`;
              input.addEventListener('change', () => { layer.editorHidden = !input.checked; redraw(); });
              label.append(input, caption);
              el['layer-list'].appendChild(label);
              layers.push({ layer, input });
            }
          }
        },
        draw(ctx) {
          const game = tw.map?.gameplay;
          const screen = screenSize();
          const bounds = draw.visibleTiles(game, screenToWorld(0, 0), screenToWorld(screen.w, screen.h));
          let status = '';
          if (el.collision.checked) {
            status = !game ? text.noGame : !bounds ? text.zoom : text.legend;
            if (Array.isArray(options.safe) && options.safe.length) {
              status += `${status ? ' · ' : ''}${text.blocked}`;
            }
            draw.drawCollision(ctx, game, bounds, worldToScreen, options.safe);
          }
          if (options.highlightLayer === 'switch') {
            const count = draw.drawSwitchTiles(ctx, tw.map?.switchLayer, worldToScreen,
              options.highlightTiles, options.switchShowID);
            const nodeStatus = `${text.linkedNodes}: ${count}`
              + `${tw.map?.switchLayer ? '' : ` (${text.noSwitchLayer})`}`;
            status += `${status ? ' · ' : ''}${nodeStatus}`;
            if (options.preview) get('note').textContent = nodeStatus;
          } else if (options.highlightTiles?.length) {
            draw.drawHighlightedTiles(ctx, game, bounds, worldToScreen, options.highlightTiles);
          }
          if (state.hasPoint && game) {
            const index = draw.tileAt(game, state.point);
            const kind = index === null ? text.outside
              : index === 0 ? text.empty
                : `${({ 1: text.solid, 2: text.death, 3: text.nohook })[index] || text.tileLabel} (ID ${index})`;
            status += `${status ? ' · ' : ''}${text.point}: ${kind}`;
          }
          const cursor = api.getCursor();
          if (el.collision.checked && cursor && game) {
            const index = draw.tileAt(game, screenToWorld(cursor.x, cursor.y));
            status += ` · ${text.cursor}: ${index === null ? text.outside : `ID ${index}`}`;
          }
          const selectedTile = state.hasPoint ? draw.tileAt(game, state.point) : null;
          const blocked = draw.blockedTile(game, state.hasPoint ? state.point : null, options.safe);
          el['collision-status'].classList.toggle('is-warning', blocked !== null || [1, 2, 3].includes(selectedTile));
          if (el['collision-status'].textContent !== status) el['collision-status'].textContent = status;
        },
      };
    },
  };
})();
