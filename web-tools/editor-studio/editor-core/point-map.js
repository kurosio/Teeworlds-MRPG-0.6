/**
 * editor-core/point-map.js — окно карты мира с точкой для PositionPicker.
 *
 * Модуль отвечает только за диалог: iframe с локальным рендером
 * (vendor/ddnet-map), список миров (когда мир не задан записью), запись X/Y
 * в скрытые инпуты и события `editor-map:*` для элемента позиции.
 *
 * Вся «политика» (мир записи, флаги тайлов, параметры слоёв) решается в
 * PositionPicker.openMap() и приходит сюда как `options`. Список миров и его
 * кэш — общий модуль editor-core/map-worlds.js, поэтому БД запрашивается один
 * раз и карточка с окном всегда показывают одни и те же миры.
 *
 * Открытие карты — единая точка `PointMap.open(row, options)`. Кнопку карты
 * обрабатывает сам PositionPicker; отдельного глобального слушателя кликов и
 * capture-фазы для синхронизации мира здесь больше нет.
 */
(() => {
  const MapWorlds = () => window.EditorCore?.MapWorlds;
  // Резерв на случай подключения модуля без map-worlds.js (например, в тесте).
  const parseWorldId = (value) => {
    if (MapWorlds()?.parseWorldId) return MapWorlds().parseWorldId(value);
    if (value === null || value === undefined || String(value).trim() === '') return null;
    const n = Number(value);
    return Number.isSafeInteger(n) && n >= 0 ? n : null;
  };
  const t = (text) => {
    const i18n = window.EditorCore?.I18n;
    return i18n?.t ? i18n.t(text) : text;
  };
  const esc = (value) => (window.EditorCore?.utils?.escapeHtml
    ? window.EditorCore.utils.escapeHtml(value ?? '')
    : String(value ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]));

  let dialog = null;
  let iframe = null;
  let worldBar = null;
  let worldSelect = null;
  let worldList = [];
  let session = null;

  // ── Поля формы ────────────────────────────────────────────────────────────
  // Оси лежат прямо в элементе позиции (data-map-axis). Поиск по data-path —
  // только резерв для кастомных форм без скрытых инпутов.
  const axisInput = (row, axis, path) => {
    const scope = row?.isConnected ? row : session?.row;
    const direct = scope?.querySelector?.(`[data-map-axis="${axis}"]`);
    if (direct) return direct;
    if (!path) return null;
    return Array.from(document.querySelectorAll('[data-path]'))
      .find((node) => node.getAttribute('data-path') === path && !scope?.contains?.(node)) || null;
  };

  // ── Окно ──────────────────────────────────────────────────────────────────
  const postWorldName = (name) => {
    if (!name || !iframe?.contentWindow) return;
    iframe.contentWindow.postMessage({ type: 'editor-map:world-name', name }, window.location.origin);
  };

  const dispatch = (type, detail) => {
    session?.row?.dispatchEvent(new CustomEvent(type, { detail, bubbles: true }));
  };

  const ensureDialog = () => {
    if (dialog) return dialog;
    dialog = document.createElement('div');
    dialog.className = 'editor-map-dialog';
    dialog.setAttribute('role', 'dialog');
    dialog.setAttribute('aria-modal', 'true');
    dialog.setAttribute('aria-label', t('Location'));
    dialog.innerHTML = `
      <div class="editor-map-dialog-frame">
        <div class="editor-map-dialog-world" hidden>
          <label class="editor-map-dialog-world-label" for="editor-map-world-select">${esc(t('World'))}</label>
          <select id="editor-map-world-select" class="editor-map-dialog-world-select"></select>
        </div>
        <iframe class="editor-map-dialog-frame-inner" title="${esc(t('World map'))}"
          referrerpolicy="no-referrer" allow="fullscreen"></iframe>
      </div>`;
    iframe = dialog.querySelector('iframe');
    iframe.addEventListener('load', () => postWorldName(session?.worldName));
    worldBar = dialog.querySelector('.editor-map-dialog-world');
    worldSelect = dialog.querySelector('.editor-map-dialog-world-select');
    worldSelect.addEventListener('change', () => {
      const value = parseWorldId(worldSelect.value);
      if (value === null || !session) return;
      session.worldId = value;
      // Смена мира в окне — пока только предпросмотр. В запись (и в
      // «последний выбранный мир») он попадёт по «Применить», а не при отмене.
      dispatch('editor-map:world', { worldId: value, provisional: true });
      loadMap();
    });
    dialog.addEventListener('click', (event) => {
      if (event.target === dialog) close('backdrop');
    });
    document.body.appendChild(dialog);
    return dialog;
  };

  /** Список миров грузим один раз (кэш в MapWorlds) и используем для подписи. */
  const fillWorldList = async () => {
    if (!worldSelect) return;
    try { worldList = await (MapWorlds()?.list?.() || Promise.resolve([])); } catch { worldList = []; }
    const options = [`<option value="">${esc(t('— select a world —'))}</option>`];
    worldList.forEach((world) => {
      options.push(`<option value="${world.id}">${world.id}: ${esc(world.name)}</option>`);
    });
    if (!worldList.length) options.push(`<option value="" disabled>${esc(t('The world list is unavailable'))}</option>`);
    worldSelect.innerHTML = options.join('');
  };

  const worldNameById = (id) => {
    const worldId = parseWorldId(id);
    if (worldId === null) return '';
    return MapWorlds()?.name?.(worldList, worldId)
      || String(worldList.find((w) => w.id === worldId)?.name || '');
  };

  // ── Адреса и параметры карты ──────────────────────────────────────────────
  // Считаем от страницы редактора (document.baseURI), а не от picker.html:
  // редактор может жить в подкаталоге, и относительный путь давал 404.
  const apiUrl = (query) => new URL(`api/map.php?${query}`, document.baseURI).href;

  const pickerUrl = ({ worldId, worldName, x, y, hasPoint, mapOptions = {} }) => {
    const params = new URLSearchParams();
    if (parseWorldId(worldId) !== null) params.set('map', apiUrl(`action=file&world=${worldId}`));
    params.set('mapres', apiUrl('action=asset&name='));
    params.set('x', String(Math.round(x)));
    params.set('y', String(Math.round(y)));
    params.set('point', hasPoint ? '1' : '0');
    if (worldName) params.set('world', worldName);
    const lang = window.EditorCore?.I18n?.lang === 'en' ? 'en' : 'ru';
    const theme = window.EditorCore?.theme?.get?.() === 'light' ? 'light' : 'dark';
    params.set('lang', lang);
    params.set('theme', theme);
    // Флаги элемента позиции: запрещённые тайлы, слои, радиус, инструменты.
    if (Array.isArray(mapOptions.safe) && mapOptions.safe.length) params.set('safe', mapOptions.safe.join(','));
    if (mapOptions.tiles) params.set('tiles', '1');
    if (Array.isArray(mapOptions.highlightTiles) && mapOptions.highlightTiles.length) params.set('highlightTiles', mapOptions.highlightTiles.join(','));
    if (Number.isFinite(mapOptions.radius) && mapOptions.radius > 0) params.set('radius', String(Math.round(mapOptions.radius)));
    if (mapOptions.snap === true || mapOptions.snap === false) params.set('snap', mapOptions.snap ? '1' : '0');
    if (mapOptions.grid === true || mapOptions.grid === false) params.set('grid', mapOptions.grid ? '1' : '0');
    return new URL(`vendor/ddnet-map/picker.html?${params.toString()}`, document.baseURI).href;
  };

  const loadMap = () => {
    if (!session || !iframe) return;
    const worldId = parseWorldId(session.worldId);
    session.worldName = worldNameById(worldId);
    if (worldBar) worldBar.hidden = !session.allowWorldChange;
    if (worldSelect && session.allowWorldChange) worldSelect.value = worldId === null ? '' : String(worldId);
    iframe.src = pickerUrl({
      worldId,
      worldName: session.worldName,
      x: session.startX,
      y: session.startY,
      hasPoint: session.hasPoint,
      mapOptions: session.mapOptions,
    });
    try { iframe.focus(); } catch { /* не критично */ }
  };

  const onMessage = (event) => {
    if (!session || !iframe || event.source !== iframe.contentWindow || event.origin !== window.location.origin) return;
    const data = event.data || {};
    if (data.type === 'editor-map:apply') {
      if (applyPoint(data.x, data.y)) {
        dispatch('editor-map:applied', {
          x: Number(data.x), y: Number(data.y), worldId: session.worldId,
        });
        close('apply');
      }
    } else if (data.type === 'editor-map:close') {
      close('cancel');
    } else if (data.type === 'editor-map:validity') {
      // Точка не сбрасывается: элемент позиции лишь подсвечивает состояние.
      dispatch('editor-map:validity', { valid: data.valid !== false, tile: data.tile ?? null });
    }
  };

  const onKeydown = (event) => {
    if (!session || event.key !== 'Escape') return;
    event.preventDefault();
    close('escape');
  };

  const lockScroll = (lock) => {
    // Не трогаем document.body.style.overflow: этот приём ломал прокрутку.
    document.documentElement.classList.toggle('editor-map-open', lock);
  };

  const close = (reason) => {
    if (!session) return;
    const { restoreFocus } = session;
    // PositionPicker сбрасывает предварительный выбор мира и подсветку при отмене.
    dispatch('editor-map:closed', { reason });
    session = null;
    closeListeners();
    lockScroll(false);
    dialog?.classList.remove('is-open');
    dialog?.setAttribute('aria-hidden', 'true');
    if (iframe) iframe.src = 'about:blank';
    try { restoreFocus?.focus?.({ preventScroll: true }); } catch { /* поле могли перерисовать */ }
  };

  const open = (row, options = {}) => {
    const xPath = row?.getAttribute?.('data-map-x-path') || '';
    const yPath = row?.getAttribute?.('data-map-y-path') || '';
    const xInput = axisInput(row, 'x', xPath);
    const yInput = axisInput(row, 'y', yPath);
    if (!xInput || !yInput) {
      if (window.EditorCore?.UI?.toast) window.EditorCore.UI.toast(t('Could not find the coordinate fields — reload the page'));
      return;
    }
    if (session) close('replace'); // окно уже открыто — закрываем прежнюю сессию

    let worldId = options.worldId === undefined
      ? parseWorldId(row.getAttribute('data-world-id'))
      : parseWorldId(options.worldId);
    const allowWorldChange = options.allowWorldChange ?? (worldId === null);
    // ID 0 — полноценный мир (Tutorilishe), а не отсутствие мира.
    if (worldId === null) worldId = MapWorlds()?.rememberedWorld?.() ?? null;

    // PositionPicker явно помечает «точки ещё нет», чтобы (0, 0) в скрытых
    // инпутах не превращался в фантомный маркер на карте.
    const hasPoint = row.getAttribute('data-position-pinned') !== '0'
      && (String(xInput.value).trim() !== '' || String(yInput.value).trim() !== '');
    const startX = Number(xInput.value || 0) || 0;
    const startY = Number(yInput.value || 0) || 0;

    ensureDialog();
    dialog.classList.add('is-open');
    dialog.removeAttribute('aria-hidden');
    lockScroll(true);

    session = {
      xPath, yPath, row, worldId, allowWorldChange,
      startX, startY, hasPoint,
      mapOptions: options.mapOptions || {},
      worldName: '',
      restoreFocus: document.activeElement,
    };
    window.addEventListener('message', onMessage);
    window.addEventListener('keydown', onKeydown, true);

    loadMap();
    fillWorldList().then(() => {
      if (!session) return;
      if (session.allowWorldChange) worldSelect.value = parseWorldId(session.worldId) === null ? '' : String(session.worldId);
      const name = worldNameById(session.worldId);
      if (!name) return;
      session.worldName = name;
      postWorldName(name);
    }).catch(() => {});
  };

  const closeListeners = () => {
    window.removeEventListener('message', onMessage);
    window.removeEventListener('keydown', onKeydown, true);
  };

  const writeValue = (axis, path, value) => {
    if (!session) return;
    const input = axisInput(session.row, axis, path);
    if (!input) return;
    const next = String(Math.round(value));
    if (input.value === next) return;
    input.value = next;
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  };

  const applyPoint = (x, y) => {
    if (!session) return false;
    const nx = Number(x);
    const ny = Number(y);
    if (!Number.isFinite(nx) || !Number.isFinite(ny)) return false;
    const { xPath, yPath } = session;
    writeValue('x', xPath, nx);
    writeValue('y', yPath, ny);
    // Некоторые формы перерисовываются на первом же событии: если значение
    // сбросилось — записываем ещё раз, уже в новое поле.
    window.requestAnimationFrame?.(() => {
      if (!session) return;
      [['x', xPath, nx], ['y', yPath, ny]].forEach(([axis, path, value]) => {
        const input = axisInput(session.row, axis, path);
        if (input && Number(input.value) !== Math.round(value)) writeValue(axis, path, value);
      });
    });
    return true;
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.PointMap = { open, close: () => close('api') };
})();
