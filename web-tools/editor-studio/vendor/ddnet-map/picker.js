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
 *   tiles    — 1: включить игровой слой при открытии; highlightTiles — список ID подсвечиваемых тайлов;
 *   radii    — JSON-массив окружностей вокруг точки. Каждый элемент:
 *              { value, label, color, dash, lineWidth, fill } — радиус (px),
 *              подпись, цвет линии, штрихи пунктира, толщина и заливка
 *              (`fill: false` — только контур). Массив задаёт количество и
 *              подписи радиусов, поэтому одно окно показывает столько
 *              окружностей, сколько объявил редактор;
 *   grid/snap — 1|0: начальные инструменты
 *              (без tiles/grid соответствующие переключатели выключены)
 *
 * Радиусы передаются только параметром `radii`: одиночных radius/radiusActive
 * в адресе окна нет, поэтому и «радиус блуждания» больше не подписывается
 * автоматически — подпись приходит из объявления `ui.radii`.
 *
 * Панель карты устроена из трёх уровней:
 *   1. шапка        — точка и камера (X/Y, зум, «Вся карта», «К точке»);
 *   2. панель карты — переключатель «Клик по карте» (точка / тайл / замер) и
 *                     секции: настройки точки, существующие точки, новая точка,
 *                     видимость, замер, радиус, слои оформления;
 *   3. подвал       — X/Y записи, предупреждение о запрещённом тайле и
 *                     «Применить / Отмена».
 * Режимы клика взаимоисключающие: замер — picker-measure.js, тайл —
 * picker-workbench.js, обычная точка — этот файл.
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
  // Тексты панели «Инструменты карты» лежат в picker-text.js: раньше те же
  // подписи дублировались здесь и в picker-workbench.js и расходились.
  const QSText = window.MapPickerText || { get: () => ({}) };
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
      loading: 'Загрузка карты…',
      loadingTitle: 'Загружаем карту',
      ready: 'Точка ставится кликом по карте',
      modeLabel: 'Клик по карте',
      hint: 'Клик — точка · перетаскивание — карта/маркер · Alt — без привязки · Shift+стрелки — точка на 1 тайл · Alt+стрелки — на 1 px · G — сетка · S — привязка',
      noWorld: 'Мир не выбран',
      noWorldText: 'Сначала выберите мир в поле «Мир» — по нему редактор находит карту.',
      failTitle: 'Карта не загрузилась',
      failText: 'Файл карты не найден на сервере или его не удалось прочитать. Проверьте, что каталог maps/ доступен веб-серверу (см. STRUCTURE.md).',
      failEmpty: 'Не передан адрес карты.',
      failWorld: 'Некорректный номер мира. Выберите мир заново и повторите попытку.',
      timeout: 'Карта не загрузилась за 120 секунд. Проверьте соединение и настройки источника карт, затем повторите попытку.',
      failBridge: 'Не удалось получить карту через map-api. Проверьте адрес, токен и доступность моста в настройках редактора.',
      failParse: 'Файл карты повреждён или не поддерживается. Проверьте файл мира на сервере.',
      failAccess: 'Доступ к карте запрещён. Проверьте вход и разрешения гостя.',
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
    },
    en: {
      grid: '32×32 grid', snap: 'Tile centers', align: 'Center in tile',
      undo: 'Undo (Ctrl+Z)', redo: 'Redo (Ctrl+Shift+Z)', reset: 'Original point',
      copy: 'Copy X, Y', copied: 'Copied', copyFailed: 'Copy failed', tile: 'Tile',
      loading: 'Loading map…',
      loadingTitle: 'Loading the map',
      ready: 'Click the map to place the point',
      modeLabel: 'Map click',
      hint: 'Click — point · drag — pan/marker · Alt — bypass snap · Shift+arrows — move point 1 tile · Alt+arrows — 1 px · G — grid · S — snap',
      noWorld: 'No world selected',
      noWorldText: 'Choose a world in the “World” field first — the editor uses it to find the map.',
      failTitle: 'The map could not be loaded',
      failText: 'The map file is missing on the server or could not be read. Make sure the maps/ directory is available to the web server (see STRUCTURE.md).',
      failEmpty: 'The map address was not provided.',
      failWorld: 'Invalid world ID. Select a world again and retry.',
      timeout: 'The map did not load within 120 seconds. Check the connection and map source settings, then retry.',
      failBridge: 'Could not retrieve the map through map-api. Check the bridge address, token and availability in editor settings.',
      failParse: 'The map file is damaged or unsupported. Check the world file on the server.',
      failAccess: 'Access to the map was denied. Check your login and guest permissions.',
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
    },
  }[LANG];

  // Тексты панели (секции, режимы, бейджи) — единый словарь picker-text.js.
  // Вложенные словари приводим к полной форме: если словарь не подключён или
  // в нём нет нового режима, панель не должна падать на чтении `modes.point`.
  const DOCK_RAW = QSText.get(LANG) || {};
  const DOCK = {
    ...DOCK_RAW,
    modes: { point: '', tile: '', measure: '', ...(DOCK_RAW.modes || {}) },
    modeHints: { point: '', tile: '', measure: '', ...(DOCK_RAW.modeHints || {}) },
  };
  STR.tools = DOCK.dockTitle || STR.tools;

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
    dock: document.getElementById('pk-dock'),
    dockToggle: document.getElementById('pk-dock-toggle'),
    dockTitle: document.getElementById('pk-dock-title'),
    dockBadge: document.getElementById('pk-dock-badge'),
    modePoint: document.getElementById('pk-mode-point'),
    modeTile: document.getElementById('pk-mode-tile'),
    modeMeasure: document.getElementById('pk-mode-measure'),
    clickHint: document.getElementById('pk-sec-click-hint'),
    pickSection: document.getElementById('pk-sec-pick'),
    pickTitle: document.getElementById('pk-pick-title'),
    measureSection: document.getElementById('pk-sec-measure'),
    measure: document.getElementById('pk-measure'),
    measureClear: document.getElementById('pk-measure-clear'),
    measureFrom: document.getElementById('pk-measure-from-point'),
    measureUse: document.getElementById('pk-measure-use'),
    measureResult: document.getElementById('pk-measure-result'),
    radius: document.getElementById('pk-radius'),
    radiusLabel: document.getElementById('pk-radius-label'),
    collisionStatus: document.getElementById('pk-collision-status'),
  };

  // Флаги приходят из объявления UI-элемента позиции (PositionPicker).
  const readTileIds = (value) => {
    if (typeof value !== 'string' || value.length > 2048) return [];
    const ids = new Set();
    for (const token of value.split(',').slice(0, 256)) {
      if (!token.trim()) continue; // Number('') is 0, not an explicitly blocked tile.
      const id = Number(token.trim());
      if (Number.isInteger(id) && id >= 0 && id <= 255) ids.add(id);
    }
    return [...ids];
  };
  const safeIds = readTileIds(qs.get('safe') || '');
  // Цвет и штрихи приходят из объявления UI-элемента позиции: принимаем только
  // простые css-цвета, чтобы параметр адреса не превратился в произвольную строку.
  const RADIUS_MAX = 100000;
  const radiusColor = (value) => {
    const text = typeof value === 'string' ? value.trim() : '';
    if (!text || text.length > 64) return '';
    if (/^#[0-9a-f]{3,8}$/i.test(text)) return text;
    if (/^rgba?\(\s*\d{1,3}\s*,\s*\d{1,3}\s*,\s*\d{1,3}\s*(?:,\s*(?:0|1|0?\.\d+)\s*)?\)$/i.test(text)) return text;
    return '';
  };
  // `radii` — массив окружностей; пустой или битый JSON просто не рисует их.
  const readRadii = () => {
    const raw = qs.get('radii');
    if (!raw) return [];
    try {
      const list = JSON.parse(raw);
      if (!Array.isArray(list)) return [];
      return list.slice(0, 8).map((entry) => {
        if (!entry || typeof entry !== 'object') return null;
        const value = Math.round(toNum(entry.value, 0));
        if (!Number.isFinite(value) || value <= 0 || value > RADIUS_MAX) return null;
        return {
          value,
          label: typeof entry.label === 'string' ? entry.label.slice(0, 80) : '',
          color: radiusColor(entry.color) || '',
          dash: Array.isArray(entry.dash)
            ? entry.dash.map(Number).filter((n) => Number.isFinite(n) && n >= 0 && n <= 64).slice(0, 8)
            : null,
          lineWidth: Number.isFinite(Number(entry.lineWidth)) && Number(entry.lineWidth) >= 0.5
            ? Math.min(Number(entry.lineWidth), 8) : null,
          fill: entry.fill === false ? false : (radiusColor(entry.fill) || null),
        };
      }).filter(Boolean);
    } catch { return []; }
  };
  const OPTIONS = {
    preview: qs.get('preview') === '1',
    highlightLayer: qs.get('highlightLayer') === 'switch' ? 'switch' : 'game',
    switchShowID: /^([1-9]\d{0,2})$/.test(qs.get('switchShowID') || '') && Number(qs.get('switchShowID')) <= 255 ? Number(qs.get('switchShowID')) : null,
    safe: safeIds,
    tiles: qs.get('tiles') === '1',
    highlightTiles: readTileIds(qs.get('highlightTiles') || ''),
    linkedTiles: qs.get('linkedTiles') === '1',
    radii: readRadii(),
    snap: qs.has('snap') ? qs.get('snap') === '1' : null,
    grid: qs.has('grid') ? qs.get('grid') === '1' : null,
  };
  if (OPTIONS.preview) {
    STR.noPoint = STR.ready = LANG === 'en' ? 'Resource node preview' : 'Предпросмотр ресурсных узлов';
    STR.hint = LANG === 'en' ? 'Drag to pan · wheel to zoom · green rings show resource nodes with switch numbers'
      : 'Перетаскивание — карта · колесо — масштаб · зелёные кольца — узлы ресурсов с номерами switch';
  }
  const safeMode = OPTIONS.safe.length > 0;
  const Overlays = window.MapPickerOverlays;
  const gameplay = () => (typeof tw === 'undefined' ? null : tw.map?.gameplay || null);

  const parsedCoordinate = (key) => {
    const value = Math.round(toNum(qs.get(key), 0));
    return Number.isSafeInteger(value) && Math.abs(value) <= 1000000000 ? value : null;
  };
  const initialX = parsedCoordinate('x'), initialY = parsedCoordinate('y');
  const hasPointParam = qs.get('point') !== '0' && initialX !== null && initialY !== null;
  const state = {
    point: { x: initialX ?? 0, y: initialY ?? 0 },
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
  // Keep manual snap preference scoped to the position field, not global across
  // unrelated editors/coordinate variants. Only low-risk UI preference is stored.
  const preferenceScope = (qs.get('prefs') || 'direct')
    .replace(/[^a-z0-9_.:-]/gi, '_').slice(0, 180) || 'direct';
  const preferenceKey = `editor-map-picker-options-v2:${preferenceScope}`;
  // Сетка не восстанавливается из прошлого окна: без ui.grid в объявлении
  // vec2 она всегда выключена. Для привязки прежнее поведение сохраняем.
  el.grid.checked = OPTIONS.grid === true;
  try {
    const saved = JSON.parse(localStorage.getItem(preferenceKey) || '{}');
    el.snap.checked = saved.snap === true;
  } catch { /* Storage is optional. */ }
  // Явный флаг редактора важнее сохранённой настройки привязки.
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
  if (OPTIONS.preview) {
    el.apply.hidden = true;
    el.inX.disabled = el.inY.disabled = true;

  }
  el.cancel.textContent = OPTIONS.preview ? (LANG === 'en' ? 'Close preview' : 'Закрыть предпросмотр') : STR.cancel;
  el.retry.textContent = STR.retry;
  el.zoomIn.title = `${STR.zoomIn} (+)`;
  el.zoomOut.title = `${STR.zoomOut} (−)`;
  el.fit.textContent = STR.fit;
  el.center.textContent = STR.center;
  el.cnvs.setAttribute('aria-label', STR.mapOf);
  el.dock.setAttribute('aria-label', DOCK.dockTitle);
  el.dockTitle.textContent = DOCK.dockTitle;
  el.modePoint.textContent = DOCK.modes.point;
  el.modeTile.textContent = DOCK.modes.tile;
  el.modeMeasure.textContent = DOCK.modes.measure;
  el.pickTitle.textContent = LANG === 'en' ? 'Point settings' : 'Настройки точки';
  el.measure.textContent = DOCK.measure;
  el.measureClear.textContent = DOCK.clear;
  el.measureFrom.textContent = DOCK.from;
  el.measureUse.textContent = DOCK.use;
  el.radiusLabel.textContent = DOCK.radius;
  for (const key of ['grid', 'snap']) document.getElementById(`pk-${key}-label`).textContent = STR[key];
  for (const key of ['align', 'reset', 'copy']) el[key].textContent = STR[key];
  for (const key of ['undo', 'redo']) {
    el[key].title = STR[key];
    el[key].setAttribute('aria-label', STR[key]);
  }
  el.snap.title = LANG === 'ru' ? '16 + 32 × N; Alt — временно без привязки' : '16 + 32 × N; Alt bypasses snapping';
  el.grid.title = LANG === 'ru' ? 'При отдалении остаются только крупные линии' : 'Only major lines remain when zoomed out';

  // ── панель карты: режим клика, свёрнутость, раскладка ─────────────────────
  // Режим клика — единственное место, где решается, что делает клик по карте.
  // Переключатель в панели и кнопки секций меняют одно и то же состояние,
  // поэтому «Измерить» и «Ставить тайл» больше не могут быть включены разом.
  const DOCK_STORAGE_KEY = 'editor-map-picker-dock-v3';
  let layoutTools = () => {}; // объявляем до setDockOpen — он её вызывает
  let clickMode = 'point';
  let workbenchApi = null;
  let measureApi = null;

  const setClickHint = () => {
    el.clickHint.textContent = DOCK.modeHints[clickMode] || DOCK.modeHints.point;
    for (const [mode, button] of [['point', el.modePoint], ['tile', el.modeTile], ['measure', el.modeMeasure]]) {
      const active = clickMode === mode;
      button.setAttribute('aria-checked', String(active));
      button.classList.toggle('is-active', active);
    }
  };
  const setDockOpen = (open) => {
    el.dock.classList.toggle('is-collapsed', !open);
    el.dockToggle.setAttribute('aria-expanded', String(open));
    el.dockToggle.title = open
      ? (LANG === 'en' ? 'Collapse the panel' : 'Свернуть панель')
      : (LANG === 'en' ? 'Expand the panel' : 'Развернуть панель');
    try { localStorage.setItem(DOCK_STORAGE_KEY, open ? '1' : '0'); } catch { /* Storage is optional. */ }
    if (typeof requestAnimationFrame === 'function') requestAnimationFrame(() => layoutTools());
    else layoutTools();
  };

  const setClickMode = (mode, { silent = false } = {}) => {
    if (!['point', 'tile', 'measure'].includes(mode)) return;
    if (clickMode === mode && !silent) {
      // Повторное нажатие активного переключателя возвращает обычный клик.
      if (mode !== 'point') { setClickMode('point'); return; }
    }
    clickMode = mode;
    if (mode !== 'tile') workbenchApi?.cancelPlacement?.();
    if (mode !== 'measure' && measureApi?.active) measureApi.setMeasuring(false);
    if (mode === 'measure') {
      setDockOpen(true);
      measureApi?.setMeasuring(true);
    }
    if (mode === 'tile') setDockOpen(true);
    setClickHint();
    el.hint.hidden = false;
    el.hint.textContent = DOCK.modeHints[clickMode];
    el.dock.classList.toggle('is-measuring', mode === 'measure');
    if (typeof tw !== 'undefined') { tw.mousePressed = false; tw.keyState = {}; }
    tw.requestRender?.(1);
  };

  // Панель стоит под шапкой и не заходит на подвал. Подсказка режима — справа
  // от панели, а на узком экране панель уходит вниз и подсказка скрыта CSS.
  layoutTools = () => {
    const bar = document.querySelector('.pk-bar');
    const foot = document.querySelector('.pk-foot');
    if (!bar) return;
    const top = bar.getBoundingClientRect().bottom + 8;
    const bottom = foot ? foot.getBoundingClientRect().top : window.innerHeight - 12;
    const dockWidth = el.dock ? el.dock.getBoundingClientRect().width : 0;
    document.documentElement.style.setProperty('--pk-dock-top', `${top}px`);
    document.documentElement.style.setProperty('--pk-hint-top', `${top}px`);
    document.documentElement.style.setProperty('--pk-hint-left', `${dockWidth + 24}px`);
    document.documentElement.style.setProperty('--pk-dock-max', `${Math.max(180, bottom - top - 12)}px`);
  };
  if (typeof ResizeObserver === 'function') {
    // Панель меняет высоту при раскрытии секций и в узком окне становится
    // нижней: подсказка должна стоять под шапкой, а не под её краем.
    const observer = new ResizeObserver(() => layoutTools());
    observer.observe(el.dock);
    observer.observe(document.querySelector('.pk-bar'));
  }
  window.addEventListener('resize', () => layoutTools());

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
      const kind = STR.blockedKinds[tile] || `${DOCK.tileLabel} ID ${tile}`;
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
    // Рассинхронизированный черновик не отправляем: сервер отверг бы его
    // (409) уже после бэкапа карты, а часть правок могла бы потеряться.
    const drift = workbench?.isConsistent?.() === false;
    el.apply.disabled = OPTIONS.preview || !state.hasPoint || state.invalid || drift || [el.inX, el.inY].some(input =>
      document.activeElement === input && (!input.value.trim() || !Number.isFinite(Number(input.value))));
    if (drift) {
      const count = workbench?.problems?.()?.length || 0;
      const message = (DOCK.editDrift || '{0}')
        .replace('{0}', String(count));
      el.apply.title = message;
      workbench?.setStatus?.('editDrift', count);
    }
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
    if (OPTIONS.preview) return;
    const next = Model.point(Number(x), Number(y), snap);
    if (!next) return;
    if (!Number.isSafeInteger(next.x) || !Number.isSafeInteger(next.y)
      || Math.abs(next.x) > 1000000000 || Math.abs(next.y) > 1000000000) return;
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
  // Формулы экран↔мир и размеры вида живут в picker-map.js: там же зум «вся
  // карта» и границы карты. Здесь остаются только обёртки над `tw` и `state`,
  // чтобы остальной код вызывал screenToWorld(x, y) как раньше.
  // Размер окна в CSS-пикселях: координаты мыши и тача приходят именно в них.
  // Канва рендера может быть крупнее (плотность экрана), но карта на экране
  // всё равно занимает CSS-размер — иначе метка уезжает на retina.
  const MapMath = window.MapPickerMap;
  const screenSize = () => {
    const canvas = el.cnvs;
    // clientWidth — размер в CSS-пикселях; width — резерв, если вёрстка ещё
    // не посчитана (или страницу открыли в странном контейнере).
    const w = (canvas && (canvas.clientWidth || canvas.width)) || window.innerWidth;
    const h = (canvas && (canvas.clientHeight || canvas.height)) || window.innerHeight;
    return { w: Math.max(1, w), h: Math.max(1, h) };
  };

  const viewSize = () => MapMath.viewSize(tw, screenSize());
  const screenToWorld = (sx, sy) => MapMath.screenToWorld(tw, screenSize(), sx, sy);
  const worldToScreen = (wx, wy) => MapMath.worldToScreen(tw, screenSize(), wx, wy);
  const clampZoom = (zoom) => MapMath.clampZoom(tw, zoom);
  const fitZoom = () => MapMath.fitZoom(tw, screenSize(), state.extent);
  const pointZoom = () => MapMath.pointZoom(tw, screenSize(), state.extent);

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
  const computeExtent = () => { state.extent = MapMath.computeExtent(tw); };

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
    if (state.ready) measureApi?.draw(overlay);

    // Радиусы объявлены редактором (ui.radii): у каждой окружности свой цвет,
    // пунктир и подпись. Рисуются здесь, потому что это часть выбора позиции,
    // а не панели инструментов.
    if (state.ready && state.hasPoint) {
      const editorRadii = Array.isArray(OPTIONS.radii) ? OPTIONS.radii : [];
      for (const radius of editorRadii) {
        if (!radius || !Number.isFinite(radius.value) || radius.value <= 0 || radius.value > 100000) continue;
        Overlays.drawRadius(overlay, state.point, radius.value, worldToScreen, radius);
      }
      // Предпросмотр из поля «Радиус, px»: не дублирует объявленный радиус.
      const manual = Number(el.radius.value);
      const declared = editorRadii.map((radius) => Number(radius?.value));
      if (Number.isFinite(manual) && manual > 0 && manual <= 100000 && !declared.includes(manual)) {
        Overlays.drawRadius(overlay, state.point, manual, worldToScreen, {
          color: '#ffda70', dash: [4, 3], lineWidth: 1.5,
          fill: 'rgba(255,218,112,.06)', label: DOCK.previewLabel,
        });
      }
    }

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

  // ── инструменты панели ────────────────────────────────────────────────────
  // Каждый режим живёт в своём модуле и получает только то, что ему нужно:
  // workbench — связанные точки и черновик тайлов, measure — замер. Общий
  // «переключатель клика» следит, чтобы активным был ровно один.
  workbench = window.MapPickerWorkbench.create({
    tw, state, screenToWorld, worldToScreen, screenSize,
    getCursor: () => cursor,
    setCamera, fitZoom, setPoint,
    options: OPTIONS,
    text: DOCK,
    redraw: () => tw.requestRender?.(2),
    onModeChange: (mode) => { if (mode === 'tile' && clickMode !== 'tile') setClickMode('tile'); },
    onBadge: (staged) => {
      el.dockBadge.hidden = !staged;
      if (staged) el.dockBadge.textContent = DOCK.dockBadgeTiles.replace('{0}', String(staged));
    },
    // Черновик изменился: обновляем доступность «Применить» и, если правок
    // больше нет, снимаем подсказку прошлой ошибки сохранения.
    onDraftChange: () => {
      paintInputs();
      if (!workbenchApi?.pendingCount) el.hint.classList.remove('is-error');
    },
    onPointer: (event) => {
      // Двигать выбранную точку можно и мышью, и тачем.
      if (event.type === 'touch') {
        const point = event.touches?.[0];
        return point ? screenToWorld(point.clientX, point.clientY) : null;
      }
      return screenToWorld(event.clientX, event.clientY);
    },
  });
  workbenchApi = workbench;
  measureApi = window.MapPickerMeasure.create({
    canvas: el.cnvs, state, text: DOCK, setPoint, redraw: () => tw.requestRender?.(2),
    point: () => state.point,
    worldToScreen,
    worldPoint: (event) => {
      const p = screenToWorld(event.clientX, event.clientY);
      const snapped = el.snap.checked && !event.altKey;
      return Model.point(p.x, p.y, snapped);
    },
    onResult: (value) => { if (el.measureResult.textContent !== value) el.measureResult.textContent = value; },
    onEndpoint: (ready) => { el.measureUse.disabled = !ready; },
    onCanStart: (disabled) => { el.measureFrom.disabled = disabled; },
    onStateChange: (active) => {
      el.measure.setAttribute('aria-pressed', String(active));
      el.cnvs.classList.toggle('is-measuring', active);
      // Только отражение состояния: режим клика и так задан там, где замер
      // включают, поэтому обратный вызов не должен переключать его снова
      // (иначе setMeasuring ↔ setClickMode уходят в рекурсию).
      if (active) setDockOpen(true);
    },
  });
  el.measure.addEventListener('click', () => setClickMode(clickMode === 'measure' ? 'point' : 'measure'));
  el.measureClear.addEventListener('click', () => measureApi.reset());
  el.measureFrom.addEventListener('click', () => measureApi.startFromPoint());
  el.measureUse.addEventListener('click', () => measureApi.useEndpoint());
  el.modePoint.addEventListener('click', () => setClickMode('point'));
  el.modeTile.addEventListener('click', () => {
    if (clickMode === 'tile') { setClickMode('point'); return; }
    // Режим включает сам workbench: он показывает игровой слой и сетку, знает
    // про слои карты и сообщает о смене режима через onModeChange. Повторно
    // переключать его здесь нельзя — тумблер «нажался» бы дважды.
    if (!workbenchApi?.startPlacement?.()) {
      el.hint.hidden = false;
      el.hint.textContent = DOCK.editNoLayer || DOCK.editUnsupported;
      setDockOpen(true);
      return;
    }
    if (clickMode !== 'tile') setClickMode('tile', { silent: true });
  });
  el.modeMeasure.addEventListener('click', () => setClickMode(clickMode === 'measure' ? 'point' : 'measure'));
  el.dockToggle.addEventListener('click', () => setDockOpen(el.dock.classList.contains('is-collapsed')));
  el.dockBadge.hidden = true;
  // Кнопки-переключатели: у них нет checkbox-состояния, только aria-checked.
  for (const button of [el.modePoint, el.modeTile, el.modeMeasure]) button.isSegmented = true;
  setClickHint();
  // «Радиус, px» стартует со значения первого объявленного радиуса: так поле
  // сразу показывает то, что редактор уже рисует на карте.
  const firstRadius = (Array.isArray(OPTIONS.radii) ? OPTIONS.radii : [])
    .find((radius) => Number.isFinite(radius?.value) && radius.value > 0);
  if (firstRadius) el.radius.value = String(firstRadius.value);
  try {
    // Панель запоминает свёрнутость: при повторном открытии карты она не
    // занимает пол-экрана там, где пользователь оставил её свёрнутой.
    if (localStorage.getItem(DOCK_STORAGE_KEY) === '0') setDockOpen(false);
  } catch { /* Storage is optional. */ }

  // ── ввод: клик по карте, перетаскивание точки ──────────────────────────────
  const pointer = { down: false, moved: 0, startX: 0, startY: 0, lastX: 0, lastY: 0, dragMarker: false, at: 0 };

  const markerScreen = () => (state.hasPoint ? worldToScreen(state.point.x, state.point.y) : null);

  // Capture-фаза: если пользователь схватил саму точку, гасим событие, чтобы
  // библиотека не начала панорамировать карту под курсором.
  // Обычный клик ставит точку только в режиме «точка»: в режимах тайла и
  // замера события уже перехвачены соответствующими модулями.
  window.addEventListener('mousedown', (evt) => {
    if (!state.ready || evt.button !== 0 || clickMode !== 'point') return;
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
    if (clickMode !== 'point') return;
    if (moved <= 6 && slow && evt.target === el.cnvs) {
      const world = screenToWorld(evt.clientX, evt.clientY);
      placePoint(world.x, world.y, evt);
    }
  }, true);

  // Тач: короткий тап ставит точку, движение — панорама (её делает библиотека).
  let touchStart = null;
  window.addEventListener('touchstart', (evt) => {
    if (!state.ready || clickMode !== 'point' || evt.target !== el.cnvs || evt.touches.length !== 1) { touchStart = null; return; }
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
    if (key === 'snap') {
      try { localStorage.setItem(preferenceKey, JSON.stringify({ snap: el.snap.checked })); } catch { /* optional */ }
    }
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
  /**
   * Сохраняет черновик правок одним запросом. Тело — целый датафайл, поэтому
   * сервер сверяет структуру, разрешённые ID и контрольную сумму исходной
   * карты. `X-Map-Allowed-Tiles` — объединение ID обоих слоёв: игровой слой
   * и switch-слой проверяются одним списком.
   */
  /**
   * Значения плана проверяются до отправки: сервер те же правила применит
   * после того, как сделает бэкап карты, — лучше отказать сразу и словами.
   */
  const assertStagedValue = (layer, change) => {
    const fail = (ru, en) => { throw new Error(LANG === 'en' ? en : ru); };
    if (layer === 'game') {
      // 0 — очистка клетки, иначе только сущности игрового слоя (>191).
      for (const [id, what] of [[change.tile, 'after'], [change.from, 'from']]) {
        if (!Number.isInteger(id) || id < 0 || id > 255) {
          fail(`Некорректный ID тайла в правке (${what}).`, `Invalid tile ID in a staged edit (${what}).`);
        }
        if (id > 0 && id <= 3) {
          fail('Тайлы столкновений (1–3) не редактируются.', 'Collision tiles (1–3) cannot be edited.');
        }
      }
      return;
    }
    if (!Number.isInteger(change.type) || change.type < 0 || change.type > 255) {
      fail('Некорректный тип switch-тайла.', 'Invalid switch tile type.');
    }
    if (!Number.isInteger(change.number) || change.number < 0 || change.number > 255) {
      fail('Номер switch должен быть от 1 до 255.', 'The switch number must be between 1 and 255.');
    }
    if (change.type !== 0 && change.number < 1) {
      fail('Номер switch должен быть от 1 до 255.', 'The switch number must be between 1 and 255.');
    }
    const from = change.from || { number: 0, type: 0 };
    if (!Number.isInteger(from.type) || from.type < 0 || from.type > 255
      || !Number.isInteger(from.number) || from.number < 0 || from.number > 255) {
      fail('Некорректное исходное значение switch-тайла.', 'Invalid original switch tile value.');
    }
  };

  const saveMapTileChanges = async (plan) => {
    const changes = plan || { game: [], switch: [] };
    const total = (changes.game?.length || 0) + (changes.switch?.length || 0);
    if (!total) return { tiles: 0, switchTiles: 0 };
    // Перед отправкой проверяем сам план: дубликаты клеток, значения вне
    // диапазона и «не те» слои отсекаются здесь, а не на сервере после
    // записи бэкапа.
    const layerSizes = {
      game: tw.map?.gameplay ? { width: tw.map.gameplay.width, height: tw.map.gameplay.height } : null,
      switch: tw.map?.switchLayer ? { width: tw.map.switchLayer.width, height: tw.map.switchLayer.height } : null,
    };
    for (const [name, list] of Object.entries({ game: changes.game || [], switch: changes.switch || [] })) {
      const size = layerSizes[name];
      if (list.length && !size) throw new Error(LANG === 'en'
        ? `This map has no ${name} layer to edit.` : `В этой карте нет слоя «${name}» для правки.`);
      const seen = new Set();
      for (const change of list) {
        const x = Number(change.x), y = Number(change.y);
        if (!Number.isInteger(x) || !Number.isInteger(y) || !size
          || x < 0 || y < 0 || x >= size.width || y >= size.height) {
          throw new Error(LANG === 'en'
            ? 'A staged edit points outside the map.' : 'Черновик содержит правку вне карты.');
        }
        const key = y * size.width + x;
        if (seen.has(key)) throw new Error(LANG === 'en'
          ? 'The same cell is staged twice.' : 'Одна и та же клетка правится дважды.');
        seen.add(key);
        assertStagedValue(name, change);
      }
    }
    const worldId = Number(qs.get('worldId'));
    if (!Number.isSafeInteger(worldId) || worldId < 0) throw new Error(LANG === 'en'
      ? 'Cannot save map tiles: no world ID was provided.' : 'Не удалось сохранить тайлы: не передан ID мира.');
    const editor = window.MapPickerMapEdit;
    if (!editor || !tw.map?.dataFile?.fileData) throw new Error(LANG === 'en'
      ? 'This map format cannot be edited in the picker.' : 'Этот формат карты нельзя редактировать в map-picker.');
    const original = new Uint8Array(tw.map.dataFile.fileData);
    const originalHash = window.crypto?.subtle
      ? [...new Uint8Array(await crypto.subtle.digest('SHA-256', original))].map((byte) => byte.toString(16).padStart(2, '0')).join('')
      : editor.sha256(original);
    const updated = editor.patch(tw, changes);
    const base = new URL('../../api/', window.location.href);
    const csrfResponse = await fetch(new URL('auth.php?action=csrf', base), { credentials: 'same-origin', cache: 'no-store' });
    const csrfPayload = await csrfResponse.json().catch(() => ({}));
    if (!csrfResponse.ok || csrfPayload.role !== 'admin' || !/^[a-f0-9]{64}$/.test(csrfPayload.csrf || '')) {
      throw new Error(csrfPayload.error || (LANG === 'en'
        ? 'Map tile editing requires an administrator session.' : 'Добавлять тайлы может только администратор.'));
    }
    const endpoint = new URL(`map-edit.php?world=${worldId}`, base);
    // Разрешённые ID: подсвеченные редактором сущности + всё, что реально
    // правит черновик. Сервер отвергнет посторонние изменения карты.
    const allowedTiles = [...new Set([
      ...OPTIONS.highlightTiles,
      // Новое значение и то, что стояло в клетке: сервер не даёт удалять
      // сущности, которых редактор не объявлял.
      ...changes.game.flatMap((change) => [change.tile, change.from]),
      ...changes.switch.flatMap((change) => [change.type, change.from?.type]),
    ].filter((id) => Number.isInteger(id) && id >= 4 && id <= 255))];
    if (!allowedTiles.length) throw new Error(LANG === 'en'
      ? 'No allowed map tiles were listed.' : 'Список разрешённых тайлов пуст.');
    const response = await fetch(endpoint, {
      method: 'POST', credentials: 'same-origin', cache: 'no-store',
      headers: {
        'Content-Type': 'application/octet-stream', 'X-Editor-CSRF': csrfPayload.csrf,
        'X-Map-Original-Sha256': originalHash, 'X-Map-Allowed-Tiles': allowedTiles.join(','),
      },
      body: updated,
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok !== true) throw new Error(payload.error || (LANG === 'en'
      ? `Map save failed (HTTP ${response.status}).` : `Не удалось сохранить карту (HTTP ${response.status}).`));
    // Карта записана: следующая правка должна считаться от нового файла,
    // иначе повторное сохранение отменило бы это (и сервер вернул бы 409).
    try { editor.reload(tw, updated); } catch { /* перезагрузка не критична для ответа */ }
    return { tiles: changes.game.length, switchTiles: changes.switch.length, sha256: payload.sha256 || '' };
  };
  el.apply.addEventListener('click', async () => {
    if (OPTIONS.preview || !state.hasPoint) return;
    // Последняя проверка перед записью: если карта изменилась под черновиком,
    // сохранение перезаписало бы чужие правки. Лучше отказать и объяснить.
    // Проверяем раньше `disabled`, иначе кнопка просто «не нажимается» молча.
    const drift = workbench?.isConsistent?.() === false;
    if (drift) {
      const count = workbench?.problems?.()?.length || 0;
      const message = (DOCK.editDrift || '{0}').replace('{0}', String(count));
      workbench?.setStatus?.('editDrift', count);
      paintInputs(); // кнопка должна стать недоступной, а не «молча не нажиматься»
      el.hint.hidden = false;
      el.hint.textContent = message;
      el.hint.classList.add('is-error');
      return;
    }
    if (el.apply.disabled) return;
    commitGesture();
    const plan = workbench?.getPendingChanges?.() || { game: [], switch: [] };
    if (!plan.game.length && !plan.switch.length) {
      post('editor-map:apply', { x: Math.round(state.point.x), y: Math.round(state.point.y), tilesSaved: 0 });
      return;
    }
    const applyLabel = el.apply.textContent;
    el.apply.disabled = true;
    el.apply.textContent = LANG === 'en' ? 'Saving map…' : 'Сохранение карты…';
    try {
      const saved = await saveMapTileChanges(plan);
      workbench?.markSaved?.();
      post('editor-map:apply', {
        x: Math.round(state.point.x), y: Math.round(state.point.y),
        tilesSaved: saved.tiles, switchTilesSaved: saved.switchTiles,
      });
    } catch (error) {
      // Правки остаются в черновике: пользователь может исправить причину и
      // повторить, ничего не потеряв.
      el.apply.disabled = false;
      el.apply.textContent = applyLabel;
      const message = error?.message || (LANG === 'en' ? 'Map save failed.' : 'Не удалось сохранить карту.');
      workbench?.setStatus?.('editError');
      workbench?.paintStatus?.();
      el.hint.hidden = false;
      el.hint.textContent = message;
      el.hint.classList.add('is-error');
      el.hint.title = message;
      window.setTimeout(() => el.hint.classList.remove('is-error'), 4000);
      // Сводка причины — в статус секции: подсказка внизу может быть скрыта
      // на узком экране, а статус остаётся рядом с самим редактором.
      workbench?.setStatusText?.(message);
    }
  });
  /**
   * Отмена с несохранёнными правками: спрашиваем один раз и коротко.
   * Иначе «Применить» и «Отмена» стояли рядом, и промах по кнопке молча
   * выбрасывал весь черновик карты.
   */
  const cancelMap = () => {
    const staged = workbench?.pendingCount ?? 0;
    if (staged > 0 && !OPTIONS.preview) {
      const message = LANG === 'en'
        ? `Discard ${staged} staged map edit(s)?`
        : `Отбросить несохранённые правки карты (${staged})?`;
      let confirmed = false;
      try { confirmed = window.confirm(message); } catch { confirmed = true; }
      if (!confirmed) return;
    }
    post('editor-map:close');
  };
  el.cancel.addEventListener('click', cancelMap);
  el.retry.addEventListener('click', () => { window.location.reload(); });

  // ── клавиатура ────────────────────────────────────────────────────────────
  // Внутри полей ввода стрелки/пробел не должны панорамировать карту.
  window.addEventListener('keydown', (evt) => {
    const tag = (evt.target && evt.target.tagName) || '';
    if (tag === 'INPUT' || tag === 'TEXTAREA') {
      if (evt.key === 'Enter' && (evt.target === el.inX || evt.target === el.inY)) { evt.preventDefault(); el.apply.click(); }
      if (evt.key === 'Escape') { evt.preventDefault(); cancelMap(); }
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
    // Esc не закрывает окно молча, если включён инструмент: сначала он
    // возвращает обычный выбор позиции, и только следующий Esc отменяет.
    if (key === 'escape' && clickMode !== 'point') {
      evt.preventDefault(); evt.stopImmediatePropagation();
      setClickMode('point');
      return;
    }
    // Delete/Backspace удаляют выбранную точку: в режиме правки это основная
    // операция, и мышь с клавиатурой должны делать одно и то же.
    if ((key === 'delete' || key === 'backspace') && clickMode === 'tile') {
      evt.preventDefault(); evt.stopImmediatePropagation();
      workbenchApi?.deleteSelected?.();
      return;
    }
    // В режиме правки: 1/2 — слой, R — сбросить черновик. Те же действия
    // есть кнопками, поэтому клавиши — ускорение, а не обязательный путь.
    if (clickMode === 'tile' && (key === '1' || key === '2')) {
      evt.preventDefault(); evt.stopImmediatePropagation();
      workbenchApi?.selectLayer?.(key === '1' ? 'game' : 'switch');
      return;
    }
    if (clickMode === 'tile' && key === 'r' && !evt.shiftKey) {
      evt.preventDefault(); evt.stopImmediatePropagation();
      workbenchApi?.resetEdits?.();
      return;
    }
    if (key === 'g' || key === 's') {
      evt.preventDefault(); evt.stopImmediatePropagation();
      el[key === 'g' ? 'grid' : 'snap'].click();
      return;
    }
    // Хоткей замера живёт здесь же: панель одна, и «M» значит одно и то же
    // в любом режиме.
    if (key === 'm') {
      evt.preventDefault(); evt.stopImmediatePropagation();
      setClickMode(clickMode === 'measure' ? 'point' : 'measure');
      return;
    }
    if (key === 'escape') { evt.preventDefault(); cancelMap(); }
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
    if (OPTIONS.preview) {
      // Предпросмотр узлов: точка не выбирается, поэтому инструменты установки
      // и «Измерить» скрыты — остаётся только осмотр карты и её слоёв.
      el.pickSection.hidden = true;
      el.measureSection.hidden = true;
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
      // Подсказку режима не прячем: она объясняет, что делает клик.
      if (el.hint && clickMode === 'point') el.hint.hidden = true;
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
    map_api_unreachable: STR.failBridge,
    parse_failed: STR.failParse,
    guest_forbidden: STR.failAccess,
    http_401: STR.failAccess,
    http_403: STR.failAccess,
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
