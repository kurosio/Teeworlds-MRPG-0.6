/* Optional read-only placement workbench. Map bytes and DB records are never mutated. */
(() => {
  'use strict';
  window.MapPickerWorkbench = {
    create(api) {
      const { tw, state, screenToWorld, worldToScreen, screenSize, redraw, options = {} } = api;
      const draw = window.MapPickerOverlays;
      const en = api.lang === 'en';
      const text = en ? {
        title:'Placement tools', collision:'Gameplay layer', spawn:'To spawn', measure:'Measure (M)', clear:'Clear measurement',
        from:'From selected point', use:'B → position', radius:'Radius preview, px', layers:'Show all layers', noGame:'No gameplay layer in this map.', zoom:'Zoom in to see gameplay tiles.',
        wanderLabel:'Wander radius', activeRadiusLabel:'Active radius', previewLabel:'Preview',
        legend:'1 solid · 2 death · 3 no-hook · green rings: selected tile IDs · blue: other tile IDs',
        blocked:'Red tiles are forbidden here',
        solid:'Solid', death:'Death', nohook:'No-hook', empty:'Empty', outside:'Outside gameplay layer', point:'Selected point',
        start:'Click two points on the map. The selected content position will not move.',
        tiles:'tiles', tileLayer:'Tiles', quadLayer:'Quads', group:'Group',
      } : {
        title:'Инструменты размещения', collision:'Игровой слой', spawn:'К спавну', measure:'Измерить (M)', clear:'Очистить замер',
        from:'От выбранной точки', use:'B → позиция', radius:'Предпросмотр радиуса, px', layers:'Показать все слои', noGame:'В карте нет игрового слоя.', zoom:'Приблизьте карту для просмотра игровых тайлов.',
        wanderLabel:'Радиус блуждания', activeRadiusLabel:'Радиус активности', previewLabel:'Предпросмотр',
        legend:'1 стена · 2 смерть · 3 без крюка · зелёный: спавны · синий: прочие ID',
        blocked:'Красные тайлы здесь запрещены',
        solid:'Стена', death:'Смерть', nohook:'Без крюка', empty:'Пусто', outside:'Вне игрового слоя', point:'Выбранная точка',
        start:'Кликните две точки на карте. Позиция редактируемого контента не изменится.',
        tiles:'тайлов', tileLayer:'Тайлы', quadLayer:'Квады', group:'Группа',
      };
      const canvas = document.getElementById('cnvs');
      const get = id => document.getElementById(`pk-${id}`);
      const el = Object.fromEntries(['collision','spawn','measure','measure-clear','measure-from-point','measure-use','radius','measure-result','collision-status','layer-list','layers-reset'].map(id => [id, get(id)]));
      get('workbench-title').textContent = text.title;
      get('collision-label').textContent = text.collision;
      get('radius-label').textContent = text.radius;
      for (const [id, key] of [['spawn','spawn'],['measure','measure'],['measure-clear','clear'],['measure-from-point','from'],['measure-use','use'],['layers-reset','layers']]) el[id].textContent = text[key];
      let measuring = false, a = null, b = null, preview = null, waiting = false;
      let press = null;
      const originalHint = get('hint').textContent;
      let layers = [];
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
      const setMeasuring = value => {
        press = null;
        touch = null;
        measuring = value;
        el.measure.setAttribute('aria-pressed', String(value));
        canvas.classList.toggle('is-measuring', value);
        if (value) get('workbench').open = true;
        get('hint').textContent = value ? text.start : originalHint;
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
      el.radius.addEventListener('input', redraw);
      el.spawn.addEventListener('click', () => {
        const spawn = tw.map?.spawnPoint;
        if (spawn) api.setCamera(spawn.x, spawn.y, Math.max(api.fitZoom(), .8));
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
      const cancelPress = () => { press = touch = null; };
      window.addEventListener('blur', cancelPress);
      window.addEventListener('touchcancel', cancelPress);
      window.addEventListener('keydown', event => {
        if (['INPUT','TEXTAREA','SELECT'].includes(event.target?.tagName) || event.target?.isContentEditable || event.ctrlKey || event.metaKey || event.altKey) return;
        if (event.key?.toLowerCase() === 'm') { event.preventDefault(); event.stopImmediatePropagation(); setMeasuring(!measuring); }
      }, true);
      return {
        ready() {
          el.spawn.disabled = !tw.map?.spawnPoint;
          el.collision.disabled = !tw.map?.gameplay;
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
