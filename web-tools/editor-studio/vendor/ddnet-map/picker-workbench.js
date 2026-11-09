/* Placement tools: previews are staged in memory and persisted only on explicit Apply. */
(() => {
  'use strict';
  window.MapPickerWorkbench = {
    create(api) {
      const { tw, state, screenToWorld, worldToScreen, screenSize, redraw, options = {} } = api;
      const draw = window.MapPickerOverlays;
      const en = api.lang === 'en';
      const text = en ? {
        title:'Placement tools', collision:'Gameplay layer', linkedLabel:'Matching highlighted tiles', linkedUse:'Use this point', linkedEmpty:'No matching highlighted tiles on this map.', editNoSources:'No matching highlighted tiles exist on this map. Tile placement is disabled; you can still click to choose a position.', linkedCount:'{0} linked points · choose one and use it as this record’s position.', linkedLimit:'Showing the first {0} matches.', linkedPoint:'Point', editMode:'Place highlighted tile (off: click to choose position)', editType:'Tile to add', editClear:'Discard staged tiles', editEmpty:'No tiles staged. Enable add mode and click an empty tile. Saving requires admin access and a writable local map folder.', editActive:'Add mode is on: click an empty game tile. Apply saves the map; Cancel discards staged edits.', editSaved:'{0} tile(s) staged. Apply saves them with the position; Cancel discards them.', editBlocked:'That map tile is occupied. Choose an empty tile.', editUnsupported:'Tile placement needs an editable game layer and an administrator with a writable local map folder.', spawn:'To spawn', measure:'Measure (M)', clear:'Clear measurement',
        from:'From selected point', use:'B → position', radius:'Radius preview, px', layers:'Show all layers', noGame:'No gameplay layer in this map.', zoom:'Zoom in to see gameplay tiles.',
        previewLabel:'Preview',
        legend:'1 solid · 2 death · 3 no-hook · green rings: selected tile IDs · blue: other tile IDs',
        blocked:'Red tiles are forbidden here',
        solid:'Solid', death:'Death', nohook:'No-hook', empty:'Empty', outside:'Outside gameplay layer', point:'Selected point',
        start:'Click two points on the map. The selected content position will not move.',
        tiles:'tiles', tileLayer:'Tiles', quadLayer:'Quads', group:'Group',
      } : {
        title:'Инструменты размещения', collision:'Игровой слой', linkedLabel:'Точки с подсвеченными тайлами', linkedUse:'Использовать точку', linkedEmpty:'На карте не найдены подходящие подсвеченные тайлы.', editNoSources:'Нет существующих тайлов из highlightTiles — установка отключена. Позицию по-прежнему можно выбрать обычным кликом по карте.', linkedCount:'Связанных точек: {0} · выберите точку и задайте её позицией этой записи.', linkedLimit:'Показаны первые {0} точек.', linkedPoint:'Точка', editMode:'Устанавливать подсвеченный тайл (выкл. — выбор позиции по клику)', editType:'Тайл для установки', editClear:'Отменить добавленные тайлы', editEmpty:'Тайлы не добавлены. Включите режим и нажмите на пустую клетку. Сохранение требует прав администратора и локальную папку карт с правом записи.', editActive:'Режим установки включён: кликните по пустой клетке игрового слоя. «Применить» сохранит карту и позицию; отмена отбросит изменения.', editSaved:'Добавлено тайлов: {0}. «Применить» сохранит карту вместе с позицией; отмена отбросит изменения.', editBlocked:'Клетка занята другим тайлом. Выберите пустую клетку.', editUnsupported:'Установка требует игрового слоя, входа администратора и локальной папки карт с правом записи.', spawn:'К спавну', measure:'Измерить (M)', clear:'Очистить замер',
        from:'От выбранной точки', use:'B → позиция', radius:'Предпросмотр радиуса, px', layers:'Показать все слои', noGame:'В карте нет игрового слоя.', zoom:'Приблизьте карту для просмотра игровых тайлов.',
        previewLabel:'Предпросмотр',
        legend:'1 стена · 2 смерть · 3 без крюка · зелёный: спавны · синий: прочие ID',
        blocked:'Красные тайлы здесь запрещены',
        solid:'Стена', death:'Смерть', nohook:'Без крюка', empty:'Пусто', outside:'Вне игрового слоя', point:'Выбранная точка',
        start:'Кликните две точки на карте. Позиция редактируемого контента не изменится.',
        tiles:'тайлов', tileLayer:'Тайлы', quadLayer:'Квады', group:'Группа',
      };
      const canvas = document.getElementById('cnvs');
      const get = id => document.getElementById(`pk-${id}`);
      const el = Object.fromEntries(['collision','spawn','measure','measure-clear','measure-from-point','measure-use','radius','measure-result','collision-status','layer-list','layers-reset','linked-tiles-wrap','linked-tiles','linked-tiles-label','linked-tiles-use','linked-tiles-status','tile-edit-mode-wrap','tile-edit-mode','tile-edit-mode-label','tile-edit-type-wrap','tile-edit-type-label','tile-edit-type','tile-edit-clear','tile-edit-status'].map(id => [id, get(id)]));
      get('workbench-title').textContent = text.title;
      const linkedTilesEnabled = options.linkedTiles === true && options.highlightLayer !== 'switch'
        && Array.isArray(options.highlightTiles) && options.highlightTiles.length > 0;
      for (const id of ['linked-tiles-wrap','linked-tiles','linked-tiles-use','linked-tiles-status',
        'tile-edit-mode-wrap','tile-edit-type-wrap','tile-edit-clear','tile-edit-status']) get(id).hidden = !linkedTilesEnabled;
      el['linked-tiles-label'].textContent = text.linkedLabel;
      el['linked-tiles-use'].textContent = text.linkedUse;
      el['tile-edit-mode-label'].textContent = text.editMode;
      el['tile-edit-type-label'].textContent = text.editType;
      el['tile-edit-clear'].textContent = text.editClear;
      get('collision-label').textContent = text.collision;
      get('radius-label').textContent = text.radius;
      for (const [id, key] of [['spawn','spawn'],['measure','measure'],['measure-clear','clear'],['measure-from-point','from'],['measure-use','use'],['layers-reset','layers']]) el[id].textContent = text[key];
      let measuring = false, a = null, b = null, preview = null, waiting = false;
      let press = null;
      const originalHint = get('hint').textContent;
      let layers = [];
      let linkedPoints = [];
      const pendingTileEdits = new Map();
      let tilePlacementPress = null;
      let tilePlacementTouch = null;
      let tilePlacementMode = false;
      let tilePlacementView = null;
      el['measure-use'].disabled = true;
      el['measure-from-point'].disabled = !state.hasPoint;
      // Панель всегда открывается свёрнутой. Игровой слой включается только
      // явным showTiles в объявлении vec2; safe/highlightTiles/radius работают сами
      // по себе и не меняют начальное состояние панели или этого переключателя.
      get('workbench').open = false;
      el.collision.checked = options.tiles === true;
      // Предпросмотр стартует со значения первого объявленного радиуса.
      const firstRadius = (Array.isArray(options.radii) ? options.radii : [])
        .find((radius) => Number.isFinite(radius?.value) && radius.value > 0);
      if (firstRadius) el.radius.value = String(firstRadius.value);
      const show = (element, value) => { if (element.textContent !== value) element.textContent = value; };
      const updateResult = () => {
        el['measure-use'].disabled = !b;
        const end = b || preview;
        if (!a || !end) { show(el['measure-result'], measuring ? text.start : ''); return; }
        const d = draw.distance(a, end);
        show(el['measure-result'], `A (${a.x}, ${a.y}) → B (${end.x}, ${end.y}) · ${d.length.toFixed(1)} px / ${(d.length / 32).toFixed(2)} ${text.tiles} · ΔX ${d.dx}, ΔY ${d.dy}`);
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
        if (linkedPoints.length && state.hasPoint) {
          let nearest = 0, nearestDistance = Infinity;
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
      };
      const setEditStatus = (message) => {
        el['tile-edit-status'].textContent = message;
        el['tile-edit-status'].hidden = !linkedTilesEnabled;
      };
      const setTilePlacementMode = (enabled) => {
        tilePlacementPress = tilePlacementTouch = null;
        if (enabled && measuring) setMeasuring(false);
        const nextMode = !!enabled && !el['tile-edit-mode'].disabled;
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
        if (tilePlacementMode) get('workbench').open = true;
        if (!measuring) get('hint').textContent = tilePlacementMode ? text.editActive : originalHint;
        setEditStatus(tilePlacementMode ? text.editActive
          : (pendingTileEdits.size ? text.editSaved.replace('{0}', String(pendingTileEdits.size)) : text.editEmpty));
        redraw();
      };
      const stageTileAt = (worldX, worldY) => {
        const game = tw.map?.gameplay;
        const tile = Number(el['tile-edit-type'].value);
        if (!game || !Number.isInteger(tile) || tile < 4 || tile > 255) {
          setEditStatus(text.editUnsupported); return false;
        }
        const tx = Math.floor(worldX / 32), ty = Math.floor(worldY / 32);
        if (tx < 0 || ty < 0 || tx >= game.width || ty >= game.height) {
          setEditStatus(text.editBlocked); return false;
        }
        const index = ty * game.width + tx;
        const current = game.indices[index];
        const staged = pendingTileEdits.get(index);
        if (staged) {
          staged.tile = tile;
          game.indices[index] = tile;
        } else if (current === 0) {
          if (pendingTileEdits.size >= 1000) { setEditStatus(text.editUnsupported); return false; }
          pendingTileEdits.set(index, { x: tx, y: ty, tile, oldTile: 0 });
          game.indices[index] = tile;
        } else if (current !== tile) {
          setEditStatus(text.editBlocked); return false;
        }
        const x = tx * 32 + 16, y = ty * 32 + 16;
        api.setPoint(x, y, { snap: false });
        el['tile-edit-clear'].disabled = pendingTileEdits.size === 0;
        // Keep the original-point list stable; rescanning a multi-million-cell
        // layer after each staged click would multiply work across up to 1000 edits.
        setEditStatus(pendingTileEdits.size
          ? text.editSaved.replace('{0}', String(pendingTileEdits.size))
          : text.editEmpty);
        redraw();
        return true;
      };
      const setMeasuring = value => {
        press = null;
        touch = null;
        measuring = value;
        el.measure.setAttribute('aria-pressed', String(value));
        canvas.classList.toggle('is-measuring', value);
        if (value && tilePlacementMode) setTilePlacementMode(false);
        if (value) get('workbench').open = true;
        get('hint').textContent = value ? text.start : (tilePlacementMode ? text.editActive : originalHint);
        get('hint').hidden = false;
        tw.mousePressed = false;
        tw.keyState = {};
        updateResult(); redraw();
      };
      el.measure.addEventListener('click', () => setMeasuring(!measuring));
      el['measure-clear'].addEventListener('click', () => { a = b = preview = null; waiting = false; updateResult(); redraw(); });
      el['measure-from-point'].addEventListener('click', () => {
        if (!state.hasPoint) return;
        a = { ...state.point }; b = null; preview = null; waiting = true;
        setMeasuring(true);
      });
      el['measure-use'].addEventListener('click', () => {
        if (!b) return;
        api.setPoint(b.x, b.y);
        setMeasuring(false);
      });
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
      el.radius.addEventListener('input', redraw);
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
      const worldPoint = event => {
        const p = screenToWorld(event.clientX, event.clientY);
        const snapped = get('snap').checked && !event.altKey;
        return window.MapPickerModel.point(p.x, p.y, snapped);
      };
      const select = event => {
        const point = worldPoint(event);
        if (!point) return;
        if (waiting) { b = point; waiting = false; }
        else { a = point; b = null; preview = point; waiting = true; }
        updateResult(); redraw();
      };
      // Registered before the picker's own handlers: measuring never changes its point/history.
      window.addEventListener('mousedown', event => {
        if (!measuring || !state.ready || event.target !== canvas || event.button !== 0) return;
        event.preventDefault(); event.stopImmediatePropagation();
        canvas.focus({ preventScroll: true });
        press = { x:event.clientX, y:event.clientY };
      }, true);
      window.addEventListener('mouseup', event => {
        if (!press) return;
        const start = press; press = null;
        event.preventDefault(); event.stopImmediatePropagation();
        if (Math.hypot(start.x - event.clientX, start.y - event.clientY) < 6 && event.target === canvas) select(event);
      }, true);
      window.addEventListener('mousemove', event => {
        if (!measuring || !waiting || !state.ready || event.target !== canvas) return;
        preview = worldPoint(event); updateResult(); redraw();
      }, true);
      // Tap measurement on touch screens; multi-touch is left to the renderer.
      let touch = null;
      window.addEventListener('touchstart', event => {
        if (!measuring || event.target !== canvas) return;
        if (event.touches.length !== 1) { touch = null; return; }
        const p = event.touches[0]; touch = { x:p.clientX, y:p.clientY, moved:false };
        event.stopImmediatePropagation();
      }, { capture:true, passive:true });
      window.addEventListener('touchmove', event => {
        if (!touch) return;
        if (event.touches.length !== 1) { touch = null; return; }
        const p = event.touches[0];
        if (Math.hypot(p.clientX - touch.x, p.clientY - touch.y) > 10) touch.moved = true;
        event.stopImmediatePropagation();
      }, { capture:true, passive:true });
      window.addEventListener('touchend', event => {
        if (!touch) return;
        const start = touch; touch = null;
        event.preventDefault(); event.stopImmediatePropagation();
        const p = event.changedTouches[0];
        if (!start.moved && p) select(p);
      }, { capture:true, passive:false });
      const cancelPress = () => { press = touch = tilePlacementPress = tilePlacementTouch = null; };
      window.addEventListener('blur', cancelPress);
      window.addEventListener('touchcancel', cancelPress);
      const placeAtScreen = (x, y) => {
        const world = screenToWorld(x, y);
        stageTileAt(world.x, world.y);
      };
      // Tile placement captures map clicks before the renderer and point picker.
      window.addEventListener('mousedown', event => {
        if (!tilePlacementMode || measuring || !state.ready || event.target !== canvas || event.button !== 0) return;
        event.preventDefault(); event.stopImmediatePropagation();
        canvas.focus({ preventScroll: true });
        tilePlacementPress = { x:event.clientX, y:event.clientY };
      }, true);
      window.addEventListener('mouseup', event => {
        if (!tilePlacementPress) return;
        const start = tilePlacementPress; tilePlacementPress = null;
        event.preventDefault(); event.stopImmediatePropagation();
        if (Math.hypot(start.x - event.clientX, start.y - event.clientY) < 6 && event.target === canvas) placeAtScreen(event.clientX, event.clientY);
      }, true);
      window.addEventListener('touchstart', event => {
        if (!tilePlacementMode || measuring || event.target !== canvas) return;
        if (event.touches.length !== 1) { tilePlacementTouch = null; return; }
        const point = event.touches[0];
        tilePlacementTouch = { x:point.clientX, y:point.clientY, moved:false };
        event.preventDefault(); event.stopImmediatePropagation();
      }, { capture:true, passive:false });
      window.addEventListener('touchmove', event => {
        if (!tilePlacementTouch) return;
        if (event.touches.length !== 1) { tilePlacementTouch = null; return; }
        const point = event.touches[0];
        if (Math.hypot(point.clientX - tilePlacementTouch.x, point.clientY - tilePlacementTouch.y) > 10) tilePlacementTouch.moved = true;
        event.preventDefault(); event.stopImmediatePropagation();
      }, { capture:true, passive:false });
      window.addEventListener('touchend', event => {
        if (!tilePlacementTouch) return;
        const start = tilePlacementTouch; tilePlacementTouch = null;
        event.preventDefault(); event.stopImmediatePropagation();
        const point = event.changedTouches[0];
        if (!start.moved && point) placeAtScreen(point.clientX, point.clientY);
      }, { capture:true, passive:false });
      window.addEventListener('keydown', event => {
        if (['INPUT','TEXTAREA','SELECT'].includes(event.target?.tagName) || event.target?.isContentEditable || event.ctrlKey || event.metaKey || event.altKey) return;
        if (event.key?.toLowerCase() === 'm') { event.preventDefault(); event.stopImmediatePropagation(); setMeasuring(!measuring); }
      }, true);
      return {
        getPendingTileChanges() {
          return [...pendingTileEdits.values()].map(({ x, y, tile }) => ({ x, y, tile }));
        },
        setEditStatus,
        ready() {
          el.spawn.disabled = !tw.map?.spawnPoint;
          el.collision.disabled = !tw.map?.gameplay;
          if (linkedTilesEnabled) {
            refreshLinkedPoints();
            const editableIds = [...new Set(options.highlightTiles.filter((id) => Number.isInteger(id) && id >= 4 && id <= 255))];
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
          el['layer-list'].replaceChildren(); layers = [];
          for (const [g, group] of (tw.map?.groups || []).entries()) for (const [l, layer] of (group.layers || []).entries()) {
            const label = document.createElement('label');
            const input = document.createElement('input'); input.type = 'checkbox'; input.checked = layer.editorHidden !== true;
            const caption = document.createElement('span');
            caption.textContent = `${text.group} ${g + 1} / ${l + 1}: ${layer.type === tw.LAYERTYPE_TILES ? text.tileLayer : text.quadLayer}${layer.width ? ` (${layer.width}×${layer.height})` : ''}`;
            input.addEventListener('change', () => { layer.editorHidden = !input.checked; redraw(); });
            label.append(input, caption); el['layer-list'].appendChild(label); layers.push({ layer, input });
          }
        },
        draw(ctx) {
          el['measure-from-point'].disabled = !state.hasPoint;
          const game = tw.map?.gameplay;
          const screen = screenSize();
          const bounds = draw.visibleTiles(game, screenToWorld(0,0), screenToWorld(screen.w,screen.h));
          let status = '';
          if (el.collision.checked) {
            status = !game ? text.noGame : !bounds ? text.zoom : text.legend;
            if (Array.isArray(options.safe) && options.safe.length) {
              status += `${status ? ' · ' : ''}${text.blocked}`;
            }
            draw.drawCollision(ctx, game, bounds, worldToScreen, options.safe);
          }
          if (options.highlightLayer === 'switch') {
            const count = draw.drawSwitchTiles(ctx, tw.map?.switchLayer, worldToScreen, options.highlightTiles, options.switchShowID);
            const nodeStatus = `${en ? 'Linked nodes' : 'Связанные узлы'}: ${count}${tw.map?.switchLayer ? '' : (en ? ' (no switch layer)' : ' (нет switch-слоя)')}`;
            status += `${status ? ' · ' : ''}${nodeStatus}`;
            if (options.preview) get('note').textContent = nodeStatus;
          } else if (options.highlightTiles?.length) draw.drawHighlightedTiles(ctx, game, bounds, worldToScreen, options.highlightTiles);
          if (state.hasPoint && game) {
            const index = draw.tileAt(game, state.point);
            const kind = index === null ? text.outside : index === 0 ? text.empty : `${({1:text.solid,2:text.death,3:text.nohook})[index] || (en ? 'Tile' : 'Тайл')} (ID ${index})`;
            status += `${status ? ' · ' : ''}${text.point}: ${kind}`;
          }
          const cursor = api.getCursor();
          if (el.collision.checked && cursor && game) {
            const index = draw.tileAt(game, screenToWorld(cursor.x, cursor.y));
            status += ` · ${en ? 'Cursor' : 'Курсор'}: ${index === null ? text.outside : `ID ${index}`}`;
          }
          const selectedTile = state.hasPoint ? draw.tileAt(game, state.point) : null;
          const blocked = draw.blockedTile(game, state.hasPoint ? state.point : null, options.safe);
          el['collision-status'].classList.toggle('is-warning', blocked !== null || [1,2,3].includes(selectedTile));
          show(el['collision-status'], status);
          // Radii declared by the editor: every entry carries its own color,
          // dash pattern and caption, so one point can show any number of
          // circles (spawn/active/wander/action radius, etc.).
          const editorRadii = Array.isArray(options.radii) ? options.radii : [];
          if (state.hasPoint) {
            for (const radius of editorRadii) {
              if (!radius || !Number.isFinite(radius.value) || radius.value <= 0 || radius.value > 100000) continue;
              draw.drawRadius(ctx, state.point, radius.value, worldToScreen, radius);
            }
          }
          // Ad-hoc preview from the workbench input (hidden when it repeats a
          // declared radius, so the circles do not overlap).
          const manual = Number(el.radius.value);
          const declared = editorRadii.map((radius) => Number(radius?.value));
          if (state.hasPoint && Number.isFinite(manual) && manual > 0 && manual <= 100000
            && !declared.includes(manual)) {
            draw.drawRadius(ctx, state.point, manual, worldToScreen, {
              color: '#ffda70', dash: [4, 3], lineWidth: 1.5, fill: 'rgba(255,218,112,.06)', label: text.previewLabel
            });
          }
          draw.drawMeasure(ctx, a, b || (waiting ? preview : null), worldToScreen);
        },
      };
    },
  };
})();
