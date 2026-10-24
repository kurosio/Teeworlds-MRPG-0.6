/* Средство просмотра карты DDNet с выбором точки.
 *
 * Страница открывается в модальном окне редактора (editor-core/point-map.js).
 * Параметры адреса:
 *   map      — адрес .map (api/map-file.php?world=N)
 *   mapres   — база для внешних текстур (api/map-asset.php?name=)
 *   x, y     — текущая точка (пиксели карты)
 *   world    — подпись мира для заголовка
 *   lang     — ru | en
 *   theme    — dark | light
 *   point    — 0, если точки ещё нет (тогда камера ставится в центр карты)
 *
 * Родительскому окну отправляются сообщения:
 *   editor-map:ready        — карта отрисована
 *   editor-map:change {x,y} — точка сдвинута (ещё не применена)
 *   editor-map:apply  {x,y} — пользователь нажал «Применить»
 *   editor-map:close        — пользователь отменил
 *   editor-map:error  {code} — карту не удалось загрузить
 */
(() => {
  'use strict';

  const qs = new URLSearchParams(window.location.search);
  const toNum = (value, fallback = 0) => {
    const n = Number(value);
    return Number.isFinite(n) ? n : fallback;
  };

  const LANG = qs.get('lang') === 'en' ? 'en' : 'ru';
  const STR = {
    ru: {
      loading: 'Загрузка карты…',
      loadingTitle: 'Загружаем карту',
      ready: 'Точка ставится кликом по карте',
      hint: 'Клик по карте — поставить точку · перетаскивание — сдвиг · колесо — масштаб',
      noWorld: 'Мир не выбран',
      noWorldText: 'Сначала выберите мир в поле «Мир» — по нему редактор находит карту.',
      failTitle: 'Карта не загрузилась',
      failText: 'Файл карты не найден на сервере или его не удалось прочитать. Проверьте, что каталог maps/ доступен веб-серверу (см. STRUCTURE.md).',
      failEmpty: 'Не передан адрес карты.',
      failWorld: 'Некорректный номер мира. Выберите мир заново и повторите попытку.',
      timeout: 'Сервер карты не ответил за 20 секунд. Проверьте соединение и повторите попытку.',
      noWebgl: 'Браузер не поддерживает WebGL — карту показать нельзя.',
      failMapsDir: 'На сервере нет каталога с картами мира: положите maps/ рядом с редактором или укажите путь в переменной EDITOR_MAPS_DIR.',
      failMapFile: 'Файл карты этого мира не найден: проверьте поле Path в редакторе миров и наличие файла на сервере.',
      retry: 'Повторить',
      coordsNote: 'Координаты — в пикселях карты, как в базе игры.',
      apply: 'Применить',
      cancel: 'Отмена',
      zoomIn: 'Приблизить',
      zoomOut: 'Отдалить',
      fit: 'Вся карта',
      center: 'К точке',
      mapOf: 'Карта мира',
      noPoint: 'Точка не задана — кликните по карте',
      missingTextures: 'нет текстур',
    },
    en: {
      loading: 'Loading map…',
      loadingTitle: 'Loading the map',
      ready: 'Click the map to place the point',
      hint: 'Click the map to place a point · drag to pan · wheel to zoom',
      noWorld: 'No world selected',
      noWorldText: 'Choose a world in the “World” field first — the editor uses it to find the map.',
      failTitle: 'The map could not be loaded',
      failText: 'The map file is missing on the server or could not be read. Make sure the maps/ directory is available to the web server (see STRUCTURE.md).',
      failEmpty: 'The map address was not provided.',
      failWorld: 'Invalid world ID. Select a world again and retry.',
      timeout: 'The map server did not respond within 20 seconds. Check the connection and retry.',
      noWebgl: 'This browser does not support WebGL, so the map cannot be shown.',
      failMapsDir: 'The server has no world maps directory: put maps/ next to the studio or set EDITOR_MAPS_DIR.',
      failMapFile: 'The map file for this world is missing: check the Path field in the worlds editor and the file on the server.',
      retry: 'Retry',
      coordsNote: 'Coordinates are map pixels, exactly as stored in the game database.',
      apply: 'Apply',
      cancel: 'Cancel',
      zoomIn: 'Zoom in',
      zoomOut: 'Zoom out',
      fit: 'Whole map',
      center: 'To point',
      mapOf: 'World map',
      noPoint: 'No point yet — click the map',
      missingTextures: 'missing textures',
    },
  }[LANG];

  const el = {
    cnvs: document.getElementById('cnvs'),
    ovl: document.getElementById('ovl'),
    title: document.getElementById('pk-title'),
    sub: document.getElementById('pk-sub'),
    outX: document.getElementById('pk-out-x'),
    outY: document.getElementById('pk-out-y'),
    inX: document.getElementById('pk-x'),
    inY: document.getElementById('pk-y'),
    note: document.getElementById('pk-note'),
    hint: document.getElementById('pk-hint'),
    state: document.getElementById('pk-state'),
    stateIcon: document.getElementById('pk-state-icon'),
    stateTitle: document.getElementById('pk-state-title'),
    stateText: document.getElementById('pk-state-text'),
    retry: document.getElementById('pk-retry'),
    apply: document.getElementById('pk-apply'),
    cancel: document.getElementById('pk-cancel'),
    zoomIn: document.getElementById('pk-zoom-in'),
    zoomOut: document.getElementById('pk-zoom-out'),
    fit: document.getElementById('pk-fit'),
    center: document.getElementById('pk-center'),
  };

  const hasPointParam = qs.get('point') !== '0';
  const state = {
    point: { x: Math.round(toNum(qs.get('x'), 0)), y: Math.round(toNum(qs.get('y'), 0)) },
    hasPoint: hasPointParam,
    ready: false,
    extent: { w: 0, h: 0 },
    failed: false,
    dragging: false,
  };

  const mapUrl = qs.get('map') || '';
  let worldName = qs.get('world') || '';

  const THEME = qs.get('theme') === 'light' ? 'light' : 'dark';
  document.documentElement.lang = LANG;
  // Редактор держит тему в data-editor-theme; data-theme оставлен для совместимости.
  document.documentElement.setAttribute('data-editor-theme', THEME);
  document.documentElement.setAttribute('data-theme', THEME);

  // ── тексты ────────────────────────────────────────────────────────────────
  el.title.textContent = worldName ? `${STR.mapOf}: ${worldName}` : STR.mapOf;
  el.sub.textContent = STR.loading;
  el.note.textContent = STR.coordsNote;
  el.hint.textContent = STR.hint;
  el.apply.textContent = STR.apply;
  el.cancel.textContent = STR.cancel;
  el.retry.textContent = STR.retry;
  el.zoomIn.title = `${STR.zoomIn} (+)`;
  el.zoomOut.title = `${STR.zoomOut} (−)`;
  el.fit.textContent = STR.fit;
  el.center.textContent = STR.center;

  const showState = (kind, title, text, withRetry) => {
    el.state.hidden = false;
    el.state.classList.toggle('is-error', kind === 'error');
    el.stateIcon.textContent = kind === 'error' ? '!' : '…';
    el.stateTitle.textContent = title;
    el.stateText.textContent = text || '';
    el.retry.hidden = !withRetry;
  };
  const hideState = () => { el.state.hidden = true; };

  // ── связь с редактором ────────────────────────────────────────────────────
  const post = (type, data) => {
    try {
      if (window.parent && window.parent !== window) {
        window.parent.postMessage(Object.assign({ type }, data || {}), window.location.origin);
      }
    } catch (err) { /* родитель мог закрыться */ }
  };

  if (!mapUrl) {
    state.failed = true;
    showState('error', STR.noWorld, STR.noWorldText, false);
    post('editor-map:error', { code: 'no_world' });
  }

  // ── готовая точка ─────────────────────────────────────────────────────────
  const paintInputs = () => {
    el.outX.textContent = state.hasPoint ? String(Math.round(state.point.x)) : '—';
    el.outY.textContent = state.hasPoint ? String(Math.round(state.point.y)) : '—';
    if (document.activeElement !== el.inX) el.inX.value = state.hasPoint ? String(Math.round(state.point.x)) : '';
    if (document.activeElement !== el.inY) el.inY.value = state.hasPoint ? String(Math.round(state.point.y)) : '';
  };

  const setPoint = (x, y, { silent = false } = {}) => {
    state.point.x = Math.round(toNum(x, state.point.x));
    state.point.y = Math.round(toNum(y, state.point.y));
    state.hasPoint = true;
    paintInputs();
    drawOverlay();
    if (!silent) post('editor-map:change', { x: state.point.x, y: state.point.y });
  };

  // ── геометрия камеры ──────────────────────────────────────────────────────
  // tw.cameraPos — центр вида, tw.worldView — «базовая» высота вида в пикселях
  // мира при зуме 1 (по умолчанию 1000). Эти формулы повторяют рендер
  // библиотеки (tw.Map.Group.initMapScreen), поэтому точка на экране совпадает
  // с картой пиксель в пиксель.
  // Размер окна в CSS-пикселях: координаты мыши и тача приходят именно в них.
  // Канва рендера может быть крупнее (плотность экрана), но карта на экране
  // всё равно занимает CSS-размер — иначе метка уезжает на retina.
  const screenSize = () => {
    const canvas = el.cnvs;
    // clientWidth — размер в CSS-пикселях; width — резерв, если вёрстка ещё
    // не посчитана (или страницу открыли в странном контейнере).
    const w = (canvas && (canvas.clientWidth || canvas.width)) || window.innerWidth;
    const h = (canvas && (canvas.clientHeight || canvas.height)) || window.innerHeight;
    return { w: Math.max(1, w), h: Math.max(1, h) };
  };

  const viewSize = () => {
    const zoom = tw.cameraZoom || 1;
    const screen = screenSize();
    return {
      w: (tw.worldView[0] * (screen.w / screen.h)) / zoom,
      h: tw.worldView[1] / zoom,
    };
  };

  const screenToWorld = (sx, sy) => {
    const view = viewSize();
    const screen = screenSize();
    return {
      x: tw.cameraPos[0] - view.w / 2 + (sx / screen.w) * view.w,
      y: tw.cameraPos[1] - view.h / 2 + (sy / screen.h) * view.h,
    };
  };

  const worldToScreen = (wx, wy) => {
    const view = viewSize();
    const screen = screenSize();
    const scale = screen.w / view.w; // CSS-пикселей экрана на пиксель мира
    return {
      x: (wx - (tw.cameraPos[0] - view.w / 2)) * scale,
      y: (wy - (tw.cameraPos[1] - view.h / 2)) * scale,
    };
  };

  const clampZoom = (zoom) => Math.min(tw.ZOOM_MAX, Math.max(tw.ZOOM_MIN, zoom));

  const fitZoom = () => {
    const { w, h } = state.extent;
    if (!w || !h) return 1;
    const screen = screenSize();
    const aspect = screen.w / screen.h;
    return clampZoom(Math.min(tw.worldView[1] / h, (tw.worldView[0] * aspect) / w) * 0.94);
  };

  const pointZoom = () => {
    const fit = fitZoom();
    // При открытии показываем окрестности точки: примерно 1400 px мира по высоте.
    return clampZoom(Math.max(fit, Math.min(1.6, tw.worldView[1] / 1400)));
  };

  const setCamera = (x, y, zoom) => {
    tw.cameraPos[0] = x;
    tw.cameraPos[1] = y;
    tw.cameraPosTarget[0] = x;
    tw.cameraPosTarget[1] = y;
    tw.cameraZoom = zoom;
    tw.cameraZoomTarget = zoom;
    tw.initialCameraPos[0] = x;
    tw.initialCameraPos[1] = y;
    tw.initialCameraZoom = zoom;
    tw.moveVelocity[0] = 0;
    tw.moveVelocity[1] = 0;
    tw.keyboardHoldMs = 0;
    // Рендер идёт по требованию (tw.autoRender = false): после прыжка камеры
    // нужно явно попросить кадр, иначе карта останется старой.
    tw.requestRender?.(3);
  };

  const centerOnPoint = () => {
    if (!state.hasPoint) return;
    setCamera(state.point.x, state.point.y, pointZoom());
  };

  const fitMap = () => {
    const { w, h } = state.extent;
    if (!w || !h) return;
    setCamera(w / 2, h / 2, fitZoom());
  };

  const centerOfMap = () => {
    const { w, h } = state.extent;
    return { x: w / 2, y: h / 2 };
  };

  // ── рамка карты и точка ───────────────────────────────────────────────────
  const computeExtent = () => {
    let w = 0;
    let h = 0;
    const groups = (tw.map && tw.map.groups) || [];
    for (const group of groups) {
      for (const layer of group.layers || []) {
        if (layer.type === tw.LAYERTYPE_TILES || layer.type === tw.LAYERTYPE_QUADS) {
          const tw32 = layer.tileSize || 32;
          const lw = layer.width || 0;
          const lh = layer.height || 0;
          if (lw && lh) {
            w = Math.max(w, lw * tw32);
            h = Math.max(h, lh * tw32);
          }
        }
      }
    }
    // Слой-квады хранит координаты вершин, а не размер в тайлах: если карта
    // состоит только из квадов, берём границы из них.
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
    state.extent = { w: Math.max(w, 1024), h: Math.max(h, 1024) };
  };

  const overlay = el.ovl.getContext('2d');
  const overlayRatio = Math.min(window.devicePixelRatio || 1, 2);
  const resizeOverlay = () => {
    const screen = screenSize();
    const w = Math.round(screen.w * overlayRatio);
    const h = Math.round(screen.h * overlayRatio);
    if (el.ovl.width !== w) el.ovl.width = w;
    if (el.ovl.height !== h) el.ovl.height = h;
    if (overlay && overlay.setTransform) overlay.setTransform(overlayRatio, 0, 0, overlayRatio, 0, 0);
  };

  // Рисуется из tw.onAfterRender — в том же кадре, что и карта.
  const drawOverlay = () => {
    if (!overlay) return;
    resizeOverlay();
    // Рисуем в CSS-пикселях: масштаб канвы оверлея уже учтён в setTransform.
    const screen = screenSize();
    overlay.clearRect(0, 0, screen.w, screen.h);

    if (state.ready && state.hasPoint) {
      const p = worldToScreen(state.point.x, state.point.y);
      const inView = p.x > -80 && p.y > -80 && p.x < screen.w + 80 && p.y < screen.h + 80;
      if (inView) {
        const accent = getComputedStyle(document.documentElement).getPropertyValue('--pk-primary').trim() || '#6f8cff';
        overlay.save();
        overlay.translate(p.x, p.y);

        // Тёмный ореол — точка видна и на светлом песке, и на тёмных тайлах.
        overlay.strokeStyle = 'rgba(0,0,0,.55)';
        overlay.lineWidth = 5;
        overlay.beginPath();
        overlay.arc(0, 0, 12, 0, Math.PI * 2);
        overlay.moveTo(-18, 0); overlay.lineTo(-5, 0);
        overlay.moveTo(5, 0); overlay.lineTo(18, 0);
        overlay.moveTo(0, -18); overlay.lineTo(0, -5);
        overlay.moveTo(0, 5); overlay.lineTo(0, 18);
        overlay.stroke();

        overlay.strokeStyle = accent;
        overlay.lineWidth = 2;
        overlay.beginPath();
        overlay.arc(0, 0, 12, 0, Math.PI * 2);
        overlay.moveTo(-18, 0); overlay.lineTo(-5, 0);
        overlay.moveTo(5, 0); overlay.lineTo(18, 0);
        overlay.moveTo(0, -18); overlay.lineTo(0, -5);
        overlay.moveTo(0, 5); overlay.lineTo(0, 18);
        overlay.stroke();

        overlay.fillStyle = accent;
        overlay.beginPath();
        overlay.arc(0, 0, 2.5, 0, Math.PI * 2);
        overlay.fill();
        overlay.restore();
      }
    }
  };

  // ── ввод: клик по карте, перетаскивание точки ──────────────────────────────
  const pointer = { down: false, moved: 0, startX: 0, startY: 0, lastX: 0, lastY: 0, dragMarker: false, at: 0 };

  const markerScreen = () => (state.hasPoint ? worldToScreen(state.point.x, state.point.y) : null);

  // Capture-фаза: если пользователь схватил саму точку, гасим событие, чтобы
  // библиотека не начала панорамировать карту под курсором.
  window.addEventListener('mousedown', (evt) => {
    if (!state.ready || evt.button !== 0) return;
    if (evt.target !== el.cnvs) return;
    const marker = markerScreen();
    const near = marker && Math.hypot(evt.clientX - marker.x, evt.clientY - marker.y) <= 16;
    pointer.down = true;
    pointer.moved = 0;
    pointer.startX = pointer.lastX = evt.clientX;
    pointer.startY = pointer.lastY = evt.clientY;
    pointer.at = Date.now();
    pointer.dragMarker = !!near;
    if (near) {
      evt.stopPropagation();
      evt.preventDefault();
    }
  }, true);

  window.addEventListener('mousemove', (evt) => {
    if (!pointer.down) return;
    pointer.moved += Math.abs(evt.clientX - pointer.lastX) + Math.abs(evt.clientY - pointer.lastY);
    pointer.lastX = evt.clientX;
    pointer.lastY = evt.clientY;
    if (pointer.dragMarker) {
      const world = screenToWorld(evt.clientX, evt.clientY);
      setPoint(world.x, world.y);
      evt.preventDefault();
    }
  }, true);

  window.addEventListener('mouseup', (evt) => {
    if (!pointer.down) return;
    const wasMarker = pointer.dragMarker;
    const moved = pointer.moved;
    const slow = Date.now() - pointer.at < 700;
    pointer.down = false;
    pointer.dragMarker = false;
    if (wasMarker) return;
    if (moved <= 6 && slow && evt.target === el.cnvs) {
      const world = screenToWorld(evt.clientX, evt.clientY);
      setPoint(world.x, world.y);
    }
  }, true);

  // Тач: короткий тап ставит точку, движение — панорама (её делает библиотека).
  let touchStart = null;
  window.addEventListener('touchstart', (evt) => {
    if (!state.ready || evt.touches.length !== 1) return;
    const t = evt.touches[0];
    touchStart = { x: t.clientX, y: t.clientY, at: Date.now(), moved: 0 };
  }, { capture: true, passive: true });

  window.addEventListener('touchmove', (evt) => {
    if (!touchStart || evt.touches.length !== 1) return;
    const t = evt.touches[0];
    touchStart.moved += Math.abs(t.clientX - touchStart.x) + Math.abs(t.clientY - touchStart.y);
    touchStart.x = t.clientX;
    touchStart.y = t.clientY;
  }, { capture: true, passive: true });

  window.addEventListener('touchend', (evt) => {
    if (!touchStart) return;
    const t = (evt.changedTouches && evt.changedTouches[0]) || null;
    const info = touchStart;
    touchStart = null;
    if (!t || info.moved > 12 || Date.now() - info.at > 700) return;
    if (evt.target !== el.cnvs) return;
    const world = screenToWorld(t.clientX, t.clientY);
    setPoint(world.x, world.y);
  }, { capture: true, passive: true });

  // ── поля координат ────────────────────────────────────────────────────────
  const readField = (input) => {
    const n = Number(input.value);
    return Number.isFinite(n) ? n : null;
  };
  el.inX.addEventListener('input', () => {
    const v = readField(el.inX);
    if (v === null) return;
    setPoint(v, state.point.y);
  });
  el.inY.addEventListener('input', () => {
    const v = readField(el.inY);
    if (v === null) return;
    setPoint(state.point.x, v);
  });

  // ── кнопки ────────────────────────────────────────────────────────────────
  const zoomBy = (factor) => {
    tw.cameraZoomTarget = clampZoom(tw.cameraZoomTarget * factor);
    tw.requestRender?.(4);
  };
  el.zoomIn.addEventListener('click', () => zoomBy(1.35));
  el.zoomOut.addEventListener('click', () => zoomBy(1 / 1.35));
  el.fit.addEventListener('click', fitMap);
  el.center.addEventListener('click', centerOnPoint);
  el.apply.addEventListener('click', () => {
    if (!state.hasPoint) return;
    post('editor-map:apply', { x: Math.round(state.point.x), y: Math.round(state.point.y) });
  });
  el.cancel.addEventListener('click', () => post('editor-map:close'));
  el.retry.addEventListener('click', () => { window.location.reload(); });

  // ── клавиатура ────────────────────────────────────────────────────────────
  // Внутри полей ввода стрелки/пробел не должны панорамировать карту.
  window.addEventListener('keydown', (evt) => {
    const tag = (evt.target && evt.target.tagName) || '';
    if (tag === 'INPUT' || tag === 'TEXTAREA') {
      if (evt.key === 'Enter') { evt.preventDefault(); el.apply.click(); }
      if (evt.key === 'Escape') { evt.preventDefault(); el.cancel.click(); }
      evt.stopPropagation();
      return;
    }
    const key = String(evt.key || '').toLowerCase();
    if (key === 'escape') { evt.preventDefault(); el.cancel.click(); }
    else if (key === 'enter') { evt.preventDefault(); el.apply.click(); }
    else if (key === 'f') { fitMap(); }
    else if (key === 'c') { centerOnPoint(); }
    else if (key === '+' || key === '=') { zoomBy(1.35); }
    else if (key === '-' || key === '_') { zoomBy(1 / 1.35); }
  }, true);

  // ── запуск ────────────────────────────────────────────────────────────────
  const onMapReady = () => {
    state.ready = true;
    tw.onAfterRender = drawOverlay;
    tw.onMapError = null;
    tw.requestRender?.(4);
    computeExtent();
    reportMissingTextures();
    if (state.hasPoint) centerOnPoint();
    else {
      const c = centerOfMap();
      setCamera(c.x, c.y, fitZoom());
    }
    el.sub.textContent = state.hasPoint ? STR.ready : STR.noPoint;
    hideState();
    paintInputs();
    updateHintTimer();
    drawOverlay();
    post('editor-map:ready', { width: state.extent.w, height: state.extent.h });
  };

  /**
   * Часть текстур mapres может отсутствовать на сервере: такие слои рендер
   * пропускает (раньше они рисовались белыми прямоугольниками). Показываем
   * короткую подсказку, чтобы это не выглядело «сломанной картой».
   */
  const reportMissingTextures = () => {
    const failures = Array.isArray(tw.textureFailures) ? tw.textureFailures : [];
    if (!failures.length || !el.sub) return;
    const names = failures.map((f) => f.name).filter(Boolean);
    const unique = Array.from(new Set(names)).slice(0, 3).join(', ');
    el.sub.textContent = `${state.hasPoint ? STR.ready : STR.noPoint} · ${STR.missingTextures}: ${unique || '?'}`;
    el.sub.title = names.join(', ');
  };

  const updateHintTimer = () => {
    if (!state.hasPoint) return;
    window.setTimeout(() => {
      if (el.hint) el.hint.hidden = true;
    }, 9000);
  };

  // Родитель может прислать точку заново (например, после смены мира).
  window.addEventListener('message', (evt) => {
    if (evt.source !== window.parent || evt.origin !== window.location.origin) return;
    const data = evt.data || {};
    if (data.type === 'editor-map:world-name') {
      worldName = String(data.name || '').trim();
      el.title.textContent = worldName ? `${STR.mapOf}: ${worldName}` : STR.mapOf;
    } else if (data.type === 'editor-map:point') {
      const has = data.x !== null && data.x !== undefined && data.y !== null && data.y !== undefined;
      state.hasPoint = has;
      if (has) setPoint(data.x, data.y, { silent: true });
      paintInputs();
      if (has) centerOnPoint();
    } else if (data.type === 'editor-map:close') {
      post('editor-map:close');
    }
  });

  paintInputs();

  // Коды из X-Map-Error: причина важнее общего «не загрузилось».
  const FAIL_TEXT = {
    maps_dir_missing: STR.failMapsDir,
    map_missing: STR.failMapFile,
    bad_world: STR.failWorld,
    timeout: STR.timeout,
  };

  const fail = (code, text) => {
    state.failed = true;
    showState('error', STR.failTitle, text || FAIL_TEXT[code] || STR.failText, true);
    el.sub.textContent = STR.failTitle;
    post('editor-map:error', { code });
  };

  if (typeof tw === 'undefined' || !tw || typeof tw.init !== 'function') {
    fail('renderer_missing');
    return;
  }

  const webglAvailable = (() => {
    try {
      const probe = document.createElement('canvas');
      return !!(probe.getContext('webgl') || probe.getContext('experimental-webgl'));
    } catch (err) {
      return false;
    }
  })();
  if (!webglAvailable) {
    fail('no_webgl', STR.noWebgl);
    return;
  }

  if (!mapUrl) {
    el.sub.textContent = STR.noWorld;
    return;
  }

  tw.onMapError = (code) => fail(code);
  tw.onMapLoaded = onMapReady;
  showState('loading', STR.loadingTitle, '');
  try {
    // tw.init пишет «Failed to initialise webgl» через alert — в этом окне
    // заменяем его на собственную проверку, поэтому alert не понадобится.
    tw.init({ mapUrl });
  } catch (err) {
    fail('init_failed');
    return;
  }
})();
