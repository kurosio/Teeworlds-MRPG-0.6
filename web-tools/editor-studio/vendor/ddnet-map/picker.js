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
 *   safe     — список ID игровых тайлов, на которые нельзя ставить точку
 *              (по умолчанию флаг редактора safeTiles даёт 1,2,3);
 *   tiles    — 1: показать игровой слой; spawns — 1: подсветить спавны;
 *   radius   — предпросмотр радиуса; grid/snap — 1|0: инструменты по умолчанию
 *
 * Родительскому окну отправляются сообщения:
 *   editor-map:ready            — карта отрисована
 *   editor-map:change {x,y}     — точка сдвинута (ещё не применена)
 *   editor-map:validity {valid} — точка на запрещённом тайле (не сбрасывается)
 *   editor-map:apply  {x,y}     — пользователь нажал «Применить»
 *   editor-map:close            — пользователь отменил
 *   editor-map:error  {code}    — карту не удалось загрузить
 */
(() => {
  'use strict';

  const Model = window.MapPickerModel;
  const qs = new URLSearchParams(window.location.search);
  const toNum = (value, fallback = 0) => {
    const n = Number(value);
    return Number.isFinite(n) ? n : fallback;
  };

  const LANG = qs.get('lang') === 'en' ? 'en' : 'ru';
  const STR = {
    ru: {
      grid: 'Сетка 32×32', snap: 'Центры тайлов', align: 'В центр тайла',
      undo: 'Отменить (Ctrl+Z)', redo: 'Повторить (Ctrl+Shift+Z)', reset: 'Исходная точка',
      copy: 'Копировать X, Y', copied: 'Скопировано', copyFailed: 'Не удалось скопировать', tile: 'Тайл',
      tools: 'Инструменты карты',
      loading: 'Загрузка карты…',
      loadingTitle: 'Загружаем карту',
      ready: 'Точка ставится кликом по карте',
      hint: 'Клик — точка · перетаскивание — карта/маркер · Alt — без привязки · Shift+стрелки — точка на 1 тайл · Alt+стрелки — на 1 px · G — сетка · S — привязка',
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
      blockedTile: 'Нельзя ставить точку на этом игровом тайле',
      blockedHint: 'COLLIDE / DEATH / UNHOOK запрещены — выберите свободный тайл',
      blockedKinds: { 1: 'стена', 2: 'смерть', 3: 'без крюка' },
      tileShort: 'Тайл',
    },
    en: {
      grid: '32×32 grid', snap: 'Tile centers', align: 'Center in tile',
      undo: 'Undo (Ctrl+Z)', redo: 'Redo (Ctrl+Shift+Z)', reset: 'Original point',
      copy: 'Copy X, Y', copied: 'Copied', copyFailed: 'Copy failed', tile: 'Tile',
      tools: 'Map tools',
      loading: 'Loading map…',
      loadingTitle: 'Loading the map',
      ready: 'Click the map to place the point',
      hint: 'Click — point · drag — pan/marker · Alt — bypass snap · Shift+arrows — move point 1 tile · Alt+arrows — 1 px · G — grid · S — snap',
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
      blockedTile: 'A point cannot be placed on this gameplay tile',
      blockedHint: 'COLLIDE / DEATH / UNHOOK are forbidden — pick a free tile',
      blockedKinds: { 1: 'solid', 2: 'death', 3: 'no-hook' },
      tileShort: 'Tile',
    },
  }[LANG];

  const el = {
    grid: document.getElementById('pk-grid'),
    snap: document.getElementById('pk-snap'),
    align: document.getElementById('pk-align'),
    undo: document.getElementById('pk-undo'),
    redo: document.getElementById('pk-redo'),
    reset: document.getElementById('pk-reset'),
    copy: document.getElementById('pk-copy'),
    tile: document.getElementById('pk-tile'),
    cnvs: document.getElementById('cnvs'),
    ovl: document.getElementById('ovl'),
    title: document.getElementById('pk-title'),
    sub: document.getElementById('pk-sub'),
    outX: document.getElementById('pk-out-x'),
    outY: document.getElementById('pk-out-y'),
    inX: document.getElementById('pk-x'),
    inY: document.getElementById('pk-y'),
    note: document.getElementById('pk-note'),
    validity: document.getElementById('pk-validity'),
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

  // Флаги приходят из объявления UI-элемента позиции (PositionPicker).
  const safeIds = (qs.get('safe') || '').split(',')
    .map((value) => Number(value.trim()))
    .filter((value, index, all) => Number.isInteger(value) && value >= 0 && all.indexOf(value) === index);
  const OPTIONS = {
    safe: safeIds,
    tiles: qs.get('tiles') === '1',
    spawns: qs.get('spawns') === '1',
    radius: Math.max(0, toNum(qs.get('radius'), 0)),
    snap: qs.has('snap') ? qs.get('snap') === '1' : null,
    grid: qs.has('grid') ? qs.get('grid') === '1' : null,
  };
  const safeMode = OPTIONS.safe.length > 0;
  const Overlays = window.MapPickerOverlays;
  const gameplay = () => (typeof tw === 'undefined' ? null : tw.map?.gameplay || null);

  const hasPointParam = qs.get('point') !== '0';
  const state = {
    point: { x: Math.round(toNum(qs.get('x'), 0)), y: Math.round(toNum(qs.get('y'), 0)) },
    hasPoint: hasPointParam,
    ready: false,
    extent: { w: 0, h: 0 },
    invalid: false,
    invalidTile: null,
  };

  const snapshot = () => ({ ...state.point, hasPoint: state.hasPoint });
  const original = snapshot();
  const history = Model.createHistory(original);
  let workbench = null;
  let cursor = null;
  let gesture = false;
  const preferenceKey = 'editor-map-picker-options-v1';
  try {
    const saved = JSON.parse(localStorage.getItem(preferenceKey) || '{}');
    el.grid.checked = saved.grid === true;
    el.snap.checked = saved.snap === true;
  } catch { /* Storage is optional. */ }
  // Явные флаги редактора важнее сохранённых настроек браузера.
  if (OPTIONS.grid !== null) el.grid.checked = OPTIONS.grid;
  if (OPTIONS.snap !== null) el.snap.checked = OPTIONS.snap;

  const mapUrl = qs.get('map') || '';
  let worldName = qs.get('world') || '';

  const THEME = qs.get('theme') === 'light' ? 'light' : 'dark';
  const cssColor = (name, fallback) => {
    try { return getComputedStyle(document.documentElement).getPropertyValue(name).trim() || fallback; } catch { return fallback; }
  };
  const DANGER = cssColor('--pk-danger', '#f2727f');
  document.documentElement.lang = LANG;
  // Редактор держит тему в data-editor-theme; data-theme оставлен для совместимости.
  document.documentElement.setAttribute('data-editor-theme', THEME);
  document.documentElement.setAttribute('data-theme', THEME);

  // ── тексты ────────────────────────────────────────────────────────────────
  el.title.textContent = worldName ? `${STR.mapOf}: ${worldName}` : STR.mapOf;
  el.sub.textContent = STR.loading;
  el.note.textContent = STR.coordsNote;
  el.hint.textContent = safeMode ? `${STR.hint} · ${STR.blockedHint}` : STR.hint;
  el.apply.textContent = STR.apply;
  el.cancel.textContent = STR.cancel;
  el.retry.textContent = STR.retry;
  el.zoomIn.title = `${STR.zoomIn} (+)`;
  el.zoomOut.title = `${STR.zoomOut} (−)`;
  el.fit.textContent = STR.fit;
  el.center.textContent = STR.center;
  el.cnvs.setAttribute('aria-label', STR.mapOf);
  document.querySelector('.pk-options').setAttribute('aria-label', STR.tools);
  for (const key of ['grid', 'snap']) document.getElementById(`pk-${key}-label`).textContent = STR[key];
  for (const key of ['align', 'reset', 'copy']) el[key].textContent = STR[key];
  for (const key of ['undo', 'redo']) {
    el[key].title = STR[key];
    el[key].setAttribute('aria-label', STR[key]);
  }
  el.snap.title = LANG === 'ru' ? '16 + 32 × N; Alt — временно без привязки' : '16 + 32 × N; Alt bypasses snapping';
  el.grid.title = LANG === 'ru' ? 'При отдалении остаются только крупные линии' : 'Only major lines remain when zoomed out';

  // Toolbars wrap on narrow screens and when the world name is long.
  const layoutTools = () => {
    const bar = document.querySelector('.pk-bar').getBoundingClientRect();
    const options = document.querySelector('.pk-options');
    document.documentElement.style.setProperty('--pk-toolbar-top', `${bar.bottom + 8}px`);
    document.documentElement.style.setProperty('--pk-hint-top', `${options.getBoundingClientRect().bottom + 8}px`);
  };
  if (typeof ResizeObserver === 'function') {
    const observer = new ResizeObserver(layoutTools);
    observer.observe(document.querySelector('.pk-bar'));
    observer.observe(document.querySelector('.pk-options'));
  }
  window.addEventListener('resize', layoutTools);
  layoutTools();

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
    showState('error', STR.noWorld, STR.noWorldText, false);
    post('editor-map:error', { code: 'no_world' });
  }

  // ── валидность точки ──────────────────────────────────────────────────────
  // Точка на запрещённом тайле остаётся на месте: координаты и X/Y не
  // сбрасываются, но «Применить» заблокирован, а маркер подсвечен красным.
  let lastValidity = { valid: true, tile: null };
  const updateValidity = () => {
    const tile = safeMode && state.hasPoint
      ? Overlays.blockedTile(gameplay(), state.point, OPTIONS.safe)
      : null;
    const valid = tile === null;
    state.invalid = !valid;
    state.invalidTile = tile;
    if (el.validity) {
      el.validity.hidden = valid;
      const kind = STR.blockedKinds[tile] || `${STR.tileShort} ID ${tile}`;
      el.validity.textContent = valid ? '' : `${STR.blockedTile}: ${kind}`;
    }
    if (el.apply) el.apply.title = valid ? '' : STR.blockedTile;
    if (lastValidity.valid !== valid || lastValidity.tile !== tile) {
      lastValidity = { valid, tile };
      post('editor-map:validity', { valid, tile });
    }
  };

  // ── готовая точка ─────────────────────────────────────────────────────────
  const paintInputs = () => {
    updateValidity();
    el.apply.disabled = !state.hasPoint || state.invalid || [el.inX, el.inY].some(input =>
      document.activeElement === input && (!input.value.trim() || !Number.isFinite(Number(input.value))));
    el.center.disabled = el.align.disabled = el.copy.disabled = !state.hasPoint;
    el.undo.disabled = !history.canUndo;
    el.redo.disabled = !history.canRedo;
    const tile = state.hasPoint ? `${Model.tileIndex(state.point.x)}, ${Model.tileIndex(state.point.y)}` : '—';
    el.tile.textContent = `${STR.tile}: ${tile}`;
    el.outX.textContent = state.hasPoint ? String(Math.round(state.point.x)) : '—';
    el.outY.textContent = state.hasPoint ? String(Math.round(state.point.y)) : '—';
    if (document.activeElement !== el.inX) el.inX.value = state.hasPoint ? String(Math.round(state.point.x)) : '';
    if (document.activeElement !== el.inY) el.inY.value = state.hasPoint ? String(Math.round(state.point.y)) : '';
  };

  const setPoint = (x, y, { silent = false, snap = false, record = true } = {}) => {
    const next = Model.point(Number(x), Number(y), snap);
    if (!next) return;
    if (!Number.isSafeInteger(next.x) || !Number.isSafeInteger(next.y)) return;
    state.point = next;
    state.hasPoint = true;
    if (record && !gesture) history.push(snapshot());
    paintInputs();
    drawOverlay();
    if (!silent) post('editor-map:change', { ...state.point });
  };
  const restore = (value) => {
    state.point = { x: value.x, y: value.y };
    state.hasPoint = value.hasPoint;
    paintInputs();
    drawOverlay();
    post('editor-map:change', state.hasPoint ? { ...state.point } : { x: null, y: null });
  };
  const placePoint = (x, y, event) => setPoint(x, y, { snap: el.snap.checked && !event?.altKey });
  const commitGesture = () => {
    if (!gesture) return;
    gesture = false;
    history.push(snapshot());
    paintInputs();
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
    let w = tw.map?.gameplay?.width * 32 || 0;
    let h = tw.map?.gameplay?.height * 32 || 0;
    if (w && h) { state.extent = { w, h }; return; }
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

    if (state.ready && el.grid.checked) {
      const start = screenToWorld(0, 0);
      const end = screenToWorld(screen.w, screen.h);
      const scale = screen.w / viewSize().w;
      overlay.save();
      overlay.beginPath();
      for (const x of Model.grid(start.x, end.x, scale)) {
        const p = worldToScreen(x, 0);
        overlay.moveTo(Math.round(p.x) + .5, 0); overlay.lineTo(Math.round(p.x) + .5, screen.h);
      }
      for (const y of Model.grid(start.y, end.y, scale)) {
        const p = worldToScreen(0, y);
        overlay.moveTo(0, Math.round(p.y) + .5); overlay.lineTo(screen.w, Math.round(p.y) + .5);
      }
      overlay.strokeStyle = THEME === 'light' ? 'rgba(0,0,0,.3)' : 'rgba(255,255,255,.3)';
      overlay.lineWidth = 1;
      overlay.stroke();
      overlay.restore();
    }
    if (state.ready && cursor && (el.grid.checked || safeMode)) {
      const world = screenToWorld(cursor.x, cursor.y);
      const blocked = safeMode ? Overlays.blockedTile(gameplay(), world, OPTIONS.safe) : null;
      // Курсор заранее показывает, куда точку ставить нельзя.
      Overlays.drawTileBox(overlay, world.x, world.y, worldToScreen,
        blocked !== null ? 'rgba(242,114,127,.34)' : 'rgba(111,140,255,.22)',
        blocked !== null ? DANGER : null);
    }

    if (state.ready) workbench?.draw(overlay);

    if (state.ready && state.hasPoint) {
      const p = worldToScreen(state.point.x, state.point.y);
      const inView = p.x > -80 && p.y > -80 && p.x < screen.w + 80 && p.y < screen.h + 80;
      if (inView) drawPointMarker(p, state.invalid);
    }
  };

  /** Крупный маркер: свечение, пунктирное кольцо, крест и подпись X/Y. */
  const drawPointMarker = (p, invalid) => {
    const accent = cssColor('--pk-primary', '#6f8cff');
    const color = invalid ? DANGER : accent;
    if (invalid && gameplay()) {
      Overlays.drawTileBox(overlay, state.point.x, state.point.y, worldToScreen,
        'rgba(242,114,127,.22)', DANGER);
    }
    overlay.save();
    overlay.translate(p.x, p.y);

    // Мягкое свечение — точку видно на любом фоне карты.
    overlay.fillStyle = invalid ? 'rgba(242,114,127,.18)' : 'rgba(111,140,255,.15)';
    overlay.beginPath(); overlay.arc(0, 0, 27, 0, Math.PI * 2); overlay.fill();

    const traceMarker = () => {
      overlay.beginPath();
      overlay.arc(0, 0, 12, 0, Math.PI * 2);
      overlay.moveTo(-24, 0); overlay.lineTo(-5, 0);
      overlay.moveTo(5, 0); overlay.lineTo(24, 0);
      overlay.moveTo(0, -24); overlay.lineTo(0, -5);
      overlay.moveTo(0, 5); overlay.lineTo(0, 24);
    };
    overlay.strokeStyle = 'rgba(0,0,0,.6)';
    overlay.lineWidth = 6;
    traceMarker(); overlay.stroke();
    overlay.strokeStyle = color;
    overlay.lineWidth = 2.5;
    traceMarker(); overlay.stroke();

    overlay.setLineDash([5, 4]);
    overlay.strokeStyle = color;
    overlay.lineWidth = 1.5;
    overlay.beginPath(); overlay.arc(0, 0, 20, 0, Math.PI * 2); overlay.stroke();
    overlay.setLineDash([]);

    overlay.fillStyle = color;
    overlay.beginPath(); overlay.arc(0, 0, 3, 0, Math.PI * 2); overlay.fill();

    if (invalid) {
      // Крест поверх центра — «точка не валидна», но сама точка не сброшена.
      const cross = () => {
        overlay.beginPath();
        overlay.moveTo(-7, -7); overlay.lineTo(7, 7);
        overlay.moveTo(7, -7); overlay.lineTo(-7, 7);
      };
      overlay.strokeStyle = 'rgba(0,0,0,.65)'; overlay.lineWidth = 5; cross(); overlay.stroke();
      overlay.strokeStyle = DANGER; overlay.lineWidth = 2.5; cross(); overlay.stroke();
    }

    // Подпись с координатами: без measureText (ширина оценивается по символам).
    const label = `${invalid ? '⚠ ' : ''}X ${Math.round(state.point.x)} · Y ${Math.round(state.point.y)}`;
    const width = label.length * 6.3 + 14;
    overlay.fillStyle = 'rgba(0,0,0,.74)';
    overlay.fillRect(-width / 2, -48, width, 19);
    overlay.fillStyle = color;
    overlay.font = '600 11px Inter, system-ui, sans-serif';
    overlay.textAlign = 'center';
    overlay.fillText(label, 0, -34);
    overlay.restore();
  };

  workbench = window.MapPickerWorkbench.create({
    tw, state, lang: LANG, screenToWorld, worldToScreen, screenSize,
    getCursor: () => cursor,
    setCamera, fitZoom, setPoint,
    options: OPTIONS,
    redraw: () => tw.requestRender?.(2),
  });

  // ── ввод: клик по карте, перетаскивание точки ──────────────────────────────
  const pointer = { down: false, moved: 0, startX: 0, startY: 0, lastX: 0, lastY: 0, dragMarker: false, at: 0 };

  const markerScreen = () => (state.hasPoint ? worldToScreen(state.point.x, state.point.y) : null);

  // Capture-фаза: если пользователь схватил саму точку, гасим событие, чтобы
  // библиотека не начала панорамировать карту под курсором.
  window.addEventListener('mousedown', (evt) => {
    if (!state.ready || evt.button !== 0) return;
    if (evt.target !== el.cnvs) return;
    el.cnvs.focus({ preventScroll: true });
    const marker = markerScreen();
    const near = marker && Math.hypot(evt.clientX - marker.x, evt.clientY - marker.y) <= 16;
    pointer.down = true;
    pointer.moved = 0;
    pointer.startX = pointer.lastX = evt.clientX;
    pointer.startY = pointer.lastY = evt.clientY;
    pointer.at = Date.now();
    pointer.dragMarker = !!near;
    if (near) {
      gesture = true;
      evt.stopPropagation();
      evt.preventDefault();
    }
  }, true);

  window.addEventListener('mousemove', (evt) => {
    if (state.ready && evt.target === el.cnvs) {
      cursor = { x: evt.clientX, y: evt.clientY };
      const world = screenToWorld(cursor.x, cursor.y);
      el.tile.textContent = `${STR.tile}: ${Model.tileIndex(world.x)}, ${Model.tileIndex(world.y)} · X ${Math.round(world.x)} / Y ${Math.round(world.y)}`;
      tw.requestRender?.(1);
    } else if (cursor) {
      cursor = null;
      tw.requestRender?.(1);
    }
    if (!pointer.down) return;
    pointer.moved += Math.abs(evt.clientX - pointer.lastX) + Math.abs(evt.clientY - pointer.lastY);
    pointer.lastX = evt.clientX;
    pointer.lastY = evt.clientY;
    if (pointer.dragMarker) {
      const world = screenToWorld(evt.clientX, evt.clientY);
      placePoint(world.x, world.y, evt);
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
    if (wasMarker) { commitGesture(); return; }
    if (moved <= 6 && slow && evt.target === el.cnvs) {
      const world = screenToWorld(evt.clientX, evt.clientY);
      placePoint(world.x, world.y, evt);
    }
  }, true);

  // Тач: короткий тап ставит точку, движение — панорама (её делает библиотека).
  let touchStart = null;
  window.addEventListener('touchstart', (evt) => {
    if (!state.ready || evt.target !== el.cnvs || evt.touches.length !== 1) { touchStart = null; return; }
    el.cnvs.focus({ preventScroll: true });
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
    placePoint(world.x, world.y, evt);
  }, { capture: true, passive: true });

  // ── поля координат ────────────────────────────────────────────────────────
  const readField = (input) => {
    if (!input.value.trim()) return null;
    const n = Number(input.value);
    return Number.isFinite(n) ? n : null;
  };
  el.inX.addEventListener('input', () => {
    const v = readField(el.inX);
    if (v === null) { paintInputs(); return; }
    setPoint(v, state.point.y);
  });
  el.inY.addEventListener('input', () => {
    const v = readField(el.inY);
    if (v === null) { paintInputs(); return; }
    setPoint(state.point.x, v);
  });

  // Coalesce edits in one coordinate field into one undo step.
  for (const input of [el.inX, el.inY]) {
    input.addEventListener('focus', () => { gesture = true; });
    input.addEventListener('blur', commitGesture);
  }
  const cancelPointer = () => {
    pointer.down = pointer.dragMarker = false;
    if (typeof tw !== 'undefined') {
      tw.mousePressed = false;
      tw.keyState = {};
    }
    touchStart = null;
    cursor = null;
    commitGesture();
    if (typeof tw !== 'undefined') tw.requestRender?.(1);
  };
  window.addEventListener('blur', cancelPointer);
  window.addEventListener('touchcancel', cancelPointer);

  for (const key of ['grid', 'snap']) el[key].addEventListener('change', () => {
    try { localStorage.setItem(preferenceKey, JSON.stringify({ grid: el.grid.checked, snap: el.snap.checked })); } catch { /* optional */ }
    drawOverlay();
  });
  el.align.addEventListener('click', () => {
    if (state.hasPoint) setPoint(state.point.x, state.point.y, { snap: true });
  });
  el.undo.addEventListener('click', () => restore(history.undo()));
  el.redo.addEventListener('click', () => restore(history.redo()));
  el.reset.addEventListener('click', () => { history.push(original); restore(original); });
  el.copy.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(`${state.point.x}, ${state.point.y}`);
      el.copy.textContent = STR.copied;
    } catch { el.copy.textContent = STR.copyFailed; }
    window.setTimeout(() => { el.copy.textContent = STR.copy; }, 1800);
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
    if (!state.hasPoint || el.apply.disabled) return;
    commitGesture();
    post('editor-map:apply', { x: Math.round(state.point.x), y: Math.round(state.point.y) });
  });
  el.cancel.addEventListener('click', () => post('editor-map:close'));
  el.retry.addEventListener('click', () => { window.location.reload(); });

  // ── клавиатура ────────────────────────────────────────────────────────────
  // Внутри полей ввода стрелки/пробел не должны панорамировать карту.
  window.addEventListener('keydown', (evt) => {
    const tag = (evt.target && evt.target.tagName) || '';
    if (tag === 'INPUT' || tag === 'TEXTAREA') {
      if (evt.key === 'Enter' && (evt.target === el.inX || evt.target === el.inY)) { evt.preventDefault(); el.apply.click(); }
      if (evt.key === 'Escape') { evt.preventDefault(); el.cancel.click(); }
      evt.stopImmediatePropagation();
      return;
    }
    if (tag === 'SELECT' || evt.target?.isContentEditable) return;
    if (tag === 'BUTTON' && (evt.key === 'Enter' || evt.key === ' ')) return;
    const key = String(evt.key || '').toLowerCase();
    const command = evt.ctrlKey || evt.metaKey;
    if (command && (key === 'z' || key === 'y')) {
      evt.preventDefault(); evt.stopImmediatePropagation();
      commitGesture();
      restore(key === 'y' || evt.shiftKey ? history.redo() : history.undo());
      return;
    }
    const direction = { arrowleft: [-1, 0], arrowright: [1, 0], arrowup: [0, -1], arrowdown: [0, 1] }[key];
    if (direction && (evt.shiftKey || evt.altKey) && !command) {
      evt.preventDefault(); evt.stopImmediatePropagation();
      if (tw.keyState) tw.keyState[key] = false;
      if (!state.ready || !state.hasPoint) return;
      const step = evt.altKey ? 1 : Model.TILE;
      const base = el.snap.checked && !evt.altKey
        ? Model.point(state.point.x, state.point.y, true) : state.point;
      setPoint(base.x + direction[0] * step, base.y + direction[1] * step);
      return;
    }
    if (command) return;
    if (key === 'g' || key === 's') {
      evt.preventDefault(); evt.stopImmediatePropagation();
      el[key === 'g' ? 'grid' : 'snap'].click();
      return;
    }
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
    workbench?.ready();
    if (state.hasPoint) centerOnPoint();
    else {
      const c = centerOfMap();
      setCamera(c.x, c.y, fitZoom());
    }
    el.sub.textContent = state.hasPoint ? STR.ready : STR.noPoint;
    reportMissingTextures();
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
