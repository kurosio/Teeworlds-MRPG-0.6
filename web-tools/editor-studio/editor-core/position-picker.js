/**
 * editor-core/position-picker.js — единый UI-элемент позиции для всех редакторов.
 *
 * Один элемент заменяет старые строки X/Y + «Местоположение»:
 *   • карточка показывает, закреплена ли точка, её мир и координаты;
 *   • «Изменить позицию» открывает локальный map-picker (PointMap);
 *   • «Ввести вручную» раскрывает X/Y только по запросу пользователя;
 *   • «Мир» — подложка/настройка элемента: пишет в поле формы (`worldPath`)
 *     либо в код редактора (`onWorldChange`); без цели сохранения мир нужен
 *     только для карты.
 *
 * Флаги карты задаются при объявлении элемента (схема `ui.*` или `render()`),
 * например `{ safeTiles: true, highlightTiles: [209], radiusPath: 'Radius' }`.
 * Они едут в окно карты через `PointMap.open()` и подсвечивают нужные слои.
 *
 * Скрытые инпуты `data-position-axis` остаются обычными полями формы:
 * FormRuntime, гостевые правки, валидация и сохранение работают без изменений.
 */
(() => {
  const namespace = (window.EditorCore = window.EditorCore || {});
  if (namespace.PositionPicker) return; // повторное подключение не дублирует слушатели
  const esc = (value) => (window.EditorCore?.utils?.escapeHtml
    ? window.EditorCore.utils.escapeHtml(value ?? '')
    : String(value ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]));
  const t = (text, vars) => {
    if (window.EditorCore?.Page?.t) return window.EditorCore.Page.t(text, vars);
    if (window.EditorCore?.I18n?.t) return window.EditorCore.I18n.t(text, vars);
    return String(text ?? '').replace(/\{(\d+)\}/g, (m, i) => (Array.isArray(vars) ? vars[Number(i)] : vars) ?? m);
  };
  const toast = (message, type = 'error') => {
    if (window.EditorCore?.UI?.toast) window.EditorCore.UI.toast(message, type);
    else console.warn('[editor-studio][position-picker]', message);
  };
  const worlds = () => window.EditorCore?.MapWorlds;
  const parseWorldId = (value) => {
    if (worlds()?.parseWorldId) return worlds().parseWorldId(value);
    if (value === null || value === undefined || String(value).trim() === '') return null;
    const n = Number(value);
    return Number.isSafeInteger(n) && n >= 0 ? n : null;
  };
  const worldCallbacks = new WeakMap();
  const changeCallbacks = new WeakMap();

  const formatCoord = (value) => (Number.isFinite(value) ? String(Math.round(value * 1000) / 1000) : '—');
  const attr = (name, value) => (value === null || value === undefined || value === ''
    ? ''
    : ` ${name}="${esc(value)}"`);

  // ── Флаги карты ───────────────────────────────────────────────────────────
  // 1 SOLID (COLLIDE) · 2 DEATH · 3 NOHOOK (UNHOOK) — игровые тайлы, на
  // которые нельзя ставить мобов, NPC и цели заданий.
  const DEFAULT_BLOCKED_TILES = Object.freeze([1, 2, 3]);

  /**
   * Нормализуем объявление флагов в один объект (он же едет в URL карты).
   * Функция идемпотентна: уже нормализованный объект из `data-map-options`
   * (`safe`, `highlightTiles`, …) читается так же, как объявление (`safeTiles`,
   * `highlightTiles`, …).
   */
  const normalizeMapOptions = (o = {}) => {
    const raw = o.safeTiles === undefined
      ? (o.blockTiles === undefined ? o.safe : o.blockTiles)
      : o.safeTiles;
    const safe = (raw === true ? DEFAULT_BLOCKED_TILES.slice()
      : Array.isArray(raw) ? raw.slice() : [])
      .map(Number)
      .filter((n) => Number.isInteger(n) && n >= 0);
    const showTiles = !!o.showTiles;
    const highlightTiles = [...new Set((Array.isArray(o.highlightTiles) ? o.highlightTiles : [])
      .filter((id) => Number.isInteger(id) && id >= 0 && id <= 255))];
    return {
      safe,
      showTiles,
      // Начальная видимость игрового слоя — только явная настройка vec2.
      // safeTiles/highlightTiles добавляют свои оверлеи, но не раскрывают панель
      // и не включают полный игровой слой за пользователя.
      tiles: showTiles,
      highlightTiles,
      highlightLayer: o.highlightLayer === 'switch' ? 'switch' : 'game',
      switchShowID: Number.isInteger(o.switchShowID) && o.switchShowID >= 1 && o.switchShowID <= 255 ? o.switchShowID : null,
      radiusPath: o.radiusPath == null ? '' : String(o.radiusPath),
      radius: o.radius == null ? '' : o.radius,
      snap: typeof o.snap === 'boolean' ? o.snap : null,
      grid: typeof o.grid === 'boolean' ? o.grid : null,
    };
  };

  const FLAG_LABELS = {
    safe: () => t('No collision tiles'),
    highlightTiles: () => t('Tile highlights'),
    tiles: () => t('Gameplay layer'),
    radius: () => t('Radius'),
    snap: () => t('Tile centers'),
    grid: () => t('Grid'),
  };
  const FLAG_TITLES = {
    safe: () => t('A point cannot be placed on a solid, death or no-hook tile'),
    highlightTiles: () => t('Highlight selected tile IDs on the map'),
    tiles: () => t('Show the map gameplay layer'),
    radius: () => t('Show a radius around the point'),
    snap: () => t('Snap the point to tile centers'),
    grid: () => t('Show the tile grid'),
  };
  const flagOn = (o, key) => ({
    safe: o.safe.length > 0,
    highlightTiles: o.highlightTiles.length > 0,
    tiles: o.showTiles,
    radius: !!o.radiusPath || Number(o.radius) > 0,
    snap: o.snap === true,
    grid: o.grid === true,
  })[key];

  const flagsHtml = (o) => Object.keys(FLAG_LABELS)
    .filter((key) => flagOn(o, key))
    .map((key) => `<span class="position-picker__flag position-picker__flag--${key}"`
      + ` data-position-flag="${key}" title="${esc(FLAG_TITLES[key]())}">${esc(FLAG_LABELS[key]())}</span>`)
    .join('');

  /** Разметка элемента. `xInputHtml`/`yInputHtml` — уже готовые скрытые инпуты. */
  const render = (options = {}) => {
    const {
      label = t('Map position'),
      xPath = '', yPath = '',
      xInputHtml = '', yInputHtml = '',
      xFieldPath = '', yFieldPath = '',
      worldPath = '', worldId = '',
      fieldPath = '',
      min = '', max = '', step = 1,
      pinned = false, map = true, world = true, compact = false,
      tag = 'position',
      id = '',
    } = options;
    const mapOptions = normalizeMapOptions(options);
    const worldAttr = attr('data-world-id', worldId);
    const numAttrs = (axis) => `${attr('min', min)}${attr('max', max)}${attr('step', step)}`
      + ` data-position-manual-axis="${axis}"`;
    return `<div class="position-picker${compact ? ' position-picker--compact' : ''}${pinned ? ' is-pinned' : ''}"\n      data-position-picker="1" data-map-picker="${esc(tag)}"${attr('id', id)}${attr('data-position-field-path', fieldPath)}\n      data-position-label="${esc(label)}" data-position-pinned="${pinned ? '1' : '0'}"\n      data-position-min="${esc(min)}" data-position-max="${esc(max)}" data-position-step="${esc(step)}"\n      data-map-x-path="${esc(xPath)}" data-map-y-path="${esc(yPath)}"\n      data-map-world-path="${esc(worldPath)}"${worldAttr}\n      data-map-options="${esc(JSON.stringify(mapOptions))}"\n      role="group" aria-label="${esc(label)}">\n      ${xInputHtml}${yInputHtml}\n      <div class="position-picker__main">\n        <span class="position-picker__icon" aria-hidden="true"><i class="fa-solid fa-map-pin"></i></span>\n        <div class="position-picker__copy">\n          <strong class="position-picker__title" data-position-title>${esc(label)}</strong>\n          <div class="position-picker__state" aria-live="polite">\n            <span data-position-state>${esc(t('No pinned point'))}</span>\n            <span class="position-picker__axis"${attr('data-field-path', xFieldPath)}>X <b data-position-value="x">—</b></span>\n            <span class="position-picker__axis"${attr('data-field-path', yFieldPath)}>Y <b data-position-value="y">—</b></span>\n          </div>\n          <div class="position-picker__flags" data-position-flags>${flagsHtml(mapOptions)}</div>\n          <div class="position-picker__warning" data-position-warning hidden></div>\n        </div>\n        ${world ? `<div class="position-picker__world" data-position-world-wrap>\n          <label class="position-picker__world-label" data-position-world-label>${esc(t('World'))}</label>\n          <select class="position-picker__world-select" data-position-world-select\n                  aria-label="${esc(t('World'))}" title="${esc(t('World'))}">\n            <option value="">${esc(t('— select a world —'))}</option>\n          </select>\n        </div>` : ''}\n        <div class="position-picker__actions">\n          ${map ? `<button type="button" class="editor-btn editor-btn-secondary" data-position-action="map" data-map-pick="1"\n              title="${esc(t('Pin position on the map'))}" aria-label="${esc(t('Pin position on the map'))}">\n            <i class="fa-solid fa-map-location-dot" aria-hidden="true"></i><span data-position-map-label>${esc(t('Pin position'))}</span>\n          </button>` : ''}\n          <button type="button" class="editor-btn editor-btn-secondary" data-position-action="manual"\n                  aria-expanded="false" title="${esc(t('Enter coordinates manually'))}" aria-label="${esc(t('Enter coordinates manually'))}">\n            <i class="fa-solid fa-keyboard" aria-hidden="true"></i><span data-position-manual-label>${esc(t('Enter manually'))}</span>\n          </button>\n        </div>\n      </div>\n      <div class="position-picker__manual" data-position-manual hidden>\n        <label class="position-picker__manual-field">X\n          <input class="editor-input form-input" type="number"${numAttrs('x')} value="" autocomplete="off">\n        </label>\n        <label class="position-picker__manual-field">Y\n          <input class="editor-input form-input" type="number"${numAttrs('y')} value="" autocomplete="off">\n        </label>\n        <div class="position-picker__manual-actions">\n          <button type="button" class="editor-btn editor-btn-primary editor-btn--sm" data-position-action="manual-apply">${esc(t('Apply'))}</button>\n          <button type="button" class="editor-btn editor-btn-secondary editor-btn--sm" data-position-action="manual-cancel">${esc(t('Cancel'))}</button>\n        </div>\n      </div>\n    </div>`;
  };

  /** Флаги, с которыми отрисован элемент. */
  const readMapOptions = (root) => {
    try {
      const raw = JSON.parse(root?.getAttribute?.('data-map-options') || '{}');
      return normalizeMapOptions(raw);
    } catch { return normalizeMapOptions({}); }
  };

  /** Ближайшее поле формы с `data-path` — мир, радиус и т.п. */
  const findFieldInput = (root, path) => {
    if (!path) return null;
    const scope = root.closest('form, [data-position-form], [data-position-picker-host], .editor-modal-content, .editor-field') || document;
    const matches = (node) => node !== root && !root.contains(node) && node.getAttribute('data-path') === path;
    const local = Array.from(scope.querySelectorAll('[data-path]')).find(matches);
    if (local) return local;
    return Array.from(document.querySelectorAll('[data-path]')).find(matches) || null;
  };

  /** Флаги + актуальное значение радиуса из формы → параметры окна карты. */
  const resolveMapOptions = (root, override = {}) => {
    const o = readMapOptions(root);
    let radius = 0;
    let raw = '';
    if (o.radiusPath) raw = findFieldInput(root, o.radiusPath)?.value ?? '';
    if (String(raw).trim() === '') raw = o.radius ?? '';
    const num = Number(raw);
    if (Number.isFinite(num) && num > 0) radius = Math.min(Math.round(num), 100000);
    return {
      safe: o.safe,
      tiles: !!o.tiles,
      highlightTiles: o.highlightTiles,
      highlightLayer: o.highlightLayer,
      switchShowID: o.switchShowID,
      radius,
      snap: o.snap,
      grid: o.grid,
      ...(override.mapOptions || {}),
    };
  };

  const allRoots = (scope = document) => {
    const list = [];
    if (scope?.matches?.('[data-position-picker]')) list.push(scope);
    if (scope?.querySelectorAll) list.push(...scope.querySelectorAll('[data-position-picker]'));
    return list;
  };

  // Два состояния мира: `data-world-id` — зафиксированный (поле формы или
  // callback), `data-world-preview` — временный выбор в открытом окне карты.
  // Разделение нужно, чтобы отмена возвращала исходный мир даже в компонентах
  // сценария, где поля мира в записи нет.
  const worldPathOf = (root) => root.getAttribute('data-map-world-path') || '';
  const boundWorldInput = (root) => findFieldInput(root, worldPathOf(root));
  // Внешний контекст: `data-map-world-id` ставят форма шага квеста или окно
  // сценария. Свой атрибут элементу больше не нужен — мир приходит в PointMap
  // явным параметром, а не чтением DOM.
  const outerWorldId = (root) => parseWorldId(
    root.parentElement?.closest?.('[data-map-world-id]')?.getAttribute('data-map-world-id'));

  const resolveWorldId = (root) => {
    // «Последний выбранный мир» из localStorage здесь не участвует: это
    // подсказка для селектора в окне карты, а не мир записи. Иначе очистка
    // мира в карточке молча возвращала бы старое значение.
    const candidates = [
      parseWorldId(root.getAttribute('data-world-preview')),
      parseWorldId(boundWorldInput(root)?.value),
      parseWorldId(root.getAttribute('data-world-id')),
      outerWorldId(root),
    ];
    return candidates.find((value) => value !== null) ?? null;
  };

  /** Мир записи/контекста без «последнего выбранного вручную» из localStorage. */
  const recordWorldId = (root) => {
    const field = parseWorldId(boundWorldInput(root)?.value);
    return field !== null ? field : outerWorldId(root);
  };

  const syncWorldContext = (root) => {
    const id = resolveWorldId(root);
    // Во время предпросмотра не фиксируем мир: отмена просто уберёт атрибут.
    if (!root.hasAttribute('data-world-preview')) {
      if (id === null) root.removeAttribute('data-world-id');
      else root.setAttribute('data-world-id', String(id));
    }
    const select = root.querySelector('[data-position-world-select]');
    if (select && select.dataset.loaded === '1') select.value = id === null ? '' : String(id);
    const wrap = root.querySelector('[data-position-world-wrap]');
    if (wrap) {
      const canSave = !!boundWorldInput(root) || worldCallbacks.has(root);
      root.dataset.worldSave = canSave ? '1' : '0';
      const title = canSave ? t('The world is saved in the record') : t('The world is used for the map only');
      wrap.title = title;
      if (select) select.title = title;
    }
    return id;
  };

  const fillWorldSelect = (root, options) => {
    const select = root.querySelector('[data-position-world-select]');
    if (!select) return;
    const current = resolveWorldId(root);
    const html = [`<option value="">${esc(t('— select a world —'))}</option>`];
    if (current !== null && !options.some((option) => option.id === current)) {
      html.push(`<option value="${current}">#${current}</option>`);
    }
    options.forEach((option) => {
      html.push(`<option value="${option.id}">${option.id}: ${esc(option.name)}</option>`);
    });
    if (!options.length) html.push(`<option value="" disabled>${esc(t('The world list is unavailable'))}</option>`);
    select.innerHTML = html.join('');
    select.dataset.loaded = '1';
    select.value = current === null ? '' : String(current);
  };

  const loadWorldOptions = () => (worlds()?.list ? worlds().list() : Promise.resolve([]));

  const readValue = (root) => {
    const xInput = root.querySelector('[data-position-axis="x"]');
    const yInput = root.querySelector('[data-position-axis="y"]');
    const rawX = xInput?.value ?? '';
    const rawY = yInput?.value ?? '';
    const x = String(rawX).trim() === '' ? NaN : Number(rawX);
    const y = String(rawY).trim() === '' ? NaN : Number(rawY);
    return { x, y, rawX, rawY, xInput, yInput };
  };

  const isPinned = (root) => {
    if (root.dataset.positionPinned === '1') return true;
    const { x, y } = readValue(root);
    return Number.isFinite(x) && Number.isFinite(y) && (x !== 0 || y !== 0);
  };

  const stateText = (root) => {
    const pinned = isPinned(root);
    const { x, y } = readValue(root);
    if (!pinned) return t('No pinned point');
    const worldId = syncWorldContext(root);
    const world = worldId === null ? '—' : `#${worldId}`;
    return t('Position pinned · world  {0} · X {1} · Y {2}', [world, formatCoord(x), formatCoord(y)]);
  };

  const updateText = (root) => {
    if (!root || root.dataset.positionPicker !== '1') return;
    const pinned = isPinned(root);
    const { x, y } = readValue(root);
    root.classList.toggle('is-pinned', pinned);
    root.dataset.positionPinned = pinned ? '1' : '0';
    syncWorldContext(root);
    const label = t(root.getAttribute('data-position-label') || t('Map position'));
    const titleEl = root.querySelector('[data-position-title]');
    if (titleEl) titleEl.textContent = label;
    root.setAttribute('aria-label', label);
    const stateEl = root.querySelector('[data-position-state]');
    if (stateEl) stateEl.textContent = stateText(root);
    const xEl = root.querySelector('[data-position-value="x"]');
    const yEl = root.querySelector('[data-position-value="y"]');
    if (xEl) xEl.textContent = pinned ? formatCoord(x) : '—';
    if (yEl) yEl.textContent = pinned ? formatCoord(y) : '—';
    const mapBtn = root.querySelector('[data-position-action="map"]');
    if (mapBtn) {
      const mapLabel = pinned ? t('Change position') : t('Pin position');
      const mapTitle = pinned ? t('Change position on the map') : t('Pin position on the map');
      const span = mapBtn.querySelector('[data-position-map-label]');
      if (span) span.textContent = mapLabel;
      mapBtn.title = mapTitle;
      mapBtn.setAttribute('aria-label', mapTitle);
    }
    const manualBtn = root.querySelector('[data-position-action="manual"]');
    if (manualBtn) {
      const span = manualBtn.querySelector('[data-position-manual-label]');
      if (span) span.textContent = t('Enter manually');
      manualBtn.title = t('Enter coordinates manually');
      manualBtn.setAttribute('aria-label', t('Enter coordinates manually'));
    }
    const applyBtn = root.querySelector('[data-position-action="manual-apply"]');
    const cancelBtn = root.querySelector('[data-position-action="manual-cancel"]');
    if (applyBtn) applyBtn.textContent = t('Apply');
    if (cancelBtn) cancelBtn.textContent = t('Cancel');
    const worldLabel = root.querySelector('[data-position-world-label]');
    if (worldLabel) worldLabel.textContent = t('World');
    const worldSelect = root.querySelector('[data-position-world-select]');
    if (worldSelect) worldSelect.setAttribute('aria-label', t('World'));
    root.querySelectorAll('[data-position-flag]').forEach((chip) => {
      const key = chip.getAttribute('data-position-flag');
      if (FLAG_LABELS[key]) chip.textContent = FLAG_LABELS[key]();
      if (FLAG_TITLES[key]) chip.title = FLAG_TITLES[key]();
    });
    const warning = root.querySelector('[data-position-warning]');
    if (warning) warning.textContent = t('Point is on a forbidden tile');
  };

  const init = (root) => {
    if (!root || root.dataset.positionReady === '1') return;
    root.dataset.positionReady = '1';
    updateText(root);
    loadWorldOptions().then((options) => {
      if (!root.isConnected) return;
      fillWorldSelect(root, options);
      updateText(root);
      namespace.RelatedRecords?.init(root);
    }).catch(() => {});
  };

  const scan = (scope = document) => { allRoots(scope).forEach(init); };

  const dispatchInput = (input) => {
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  };

  const validateCoords = (root, x, y) => {
    if (!Number.isFinite(x) || !Number.isFinite(y)) return t('Coordinates must be finite numbers');
    // Пустые min/max — «без ограничений»: Number('') вернул бы 0 и запретил
    // отрицательные координаты сценариев.
    const minRaw = String(root.dataset.positionMin ?? '').trim();
    const maxRaw = String(root.dataset.positionMax ?? '').trim();
    const min = minRaw === '' ? NaN : Number(minRaw);
    const max = maxRaw === '' ? NaN : Number(maxRaw);
    if ((Number.isFinite(min) && (x < min || y < min)) || (Number.isFinite(max) && (x > max || y > max))) {
      return t('Coordinates are outside the allowed range');
    }
    return '';
  };

  const setValue = (root, coords = {}, options = {}) => {
    if (!root) return false;
    const x = Number(coords.x);
    const y = Number(coords.y);
    const error = validateCoords(root, x, y);
    if (error) { toast(error, 'error'); return false; }
    const { xInput, yInput } = readValue(root);
    if (!xInput || !yInput) return false;
    if (xInput.value !== String(x)) { xInput.value = String(x); dispatchInput(xInput); }
    if (yInput.value !== String(y)) { yInput.value = String(y); dispatchInput(yInput); }
    root.dataset.positionPinned = '1';
    updateText(root);
    const detail = { x, y, source: options.source || 'api' };
    root.dispatchEvent(new CustomEvent('position-picker:change', { detail, bubbles: true }));
    const callback = changeCallbacks.get(root);
    if (callback) callback(getValue(root), detail);
    return true;
  };

  const getValue = (root) => {
    const { x, y } = readValue(root);
    return { x, y, worldId: parseWorldId(root.getAttribute('data-world-id')), pinned: isPinned(root) };
  };

  const setWorldId = (root, value, options = {}) => {
    if (!root) return null;
    const id = parseWorldId(value);
    if (options.save === false && options.source === 'map' && id !== null) {
      // Только предпросмотр: зафиксированный мир и localStorage не трогаем.
      root.setAttribute('data-world-preview', String(id));
      updateText(root);
      return id;
    }
    root.removeAttribute('data-world-preview');
    const current = parseWorldId(root.getAttribute('data-world-id'));
    const changed = current !== id;
    if (id === null) {
      root.removeAttribute('data-world-id');
    } else {
      root.setAttribute('data-world-id', String(id));
      if (options.source === 'select' || options.source === 'map') worlds()?.rememberWorld?.(id);
    }
    let saved = false;
    if (options.save !== false && id !== null) {
      const bound = boundWorldInput(root);
      if (bound) {
        if (bound.value !== String(id)) { bound.value = String(id); dispatchInput(bound); }
        saved = true;
      }
      const callback = worldCallbacks.get(root);
      if (callback) { callback(id, { source: options.source || 'api', saved }); saved = true; }
    }
    root.dataset.worldSave = saved ? '1' : '0';
    const select = root.querySelector('[data-position-world-select]');
    if (select && select.dataset.loaded === '1') select.value = id === null ? '' : String(id);
    updateText(root);
    if (changed) {
      root.dispatchEvent(new CustomEvent('position-picker:world', {
        detail: { worldId: id, saved, source: options.source || 'api' }, bubbles: true,
      }));
    }
    return id;
  };

  /** Единая точка открытия карты: кнопка элемента и вызов из кода редактора. */
  const openMap = (root, options = {}) => {
    if (!root) return false;
    syncWorldContext(root);
    const explicit = options.worldId === undefined ? null : parseWorldId(options.worldId);
    const worldId = explicit !== null ? explicit : recordWorldId(root);
    if (typeof options.onWorldChange === 'function') worldCallbacks.set(root, options.onWorldChange);
    if (!window.EditorCore?.PointMap?.open) {
      toast(t('The world map is unavailable — reload the page'), 'error');
      return false;
    }
    window.EditorCore.PointMap.open(root, {
      worldId,
      allowWorldChange: worldId === null,
      mapOptions: resolveMapOptions(root, options),
    });
    return true;
  };

  const panelOpen = (root) => root?.querySelector('[data-position-manual]')?.hidden === false;

  const toggleManual = (root, open) => {
    const panel = root?.querySelector('[data-position-manual]');
    const button = root?.querySelector('[data-position-action="manual"]');
    if (!panel || !button) return;
    panel.hidden = !open;
    button.setAttribute('aria-expanded', open ? 'true' : 'false');
    root.dataset.positionManualOpen = open ? '1' : '0';
    if (open) {
      const { x, y } = readValue(root);
      const xEl = panel.querySelector('[data-position-manual-axis="x"]');
      const yEl = panel.querySelector('[data-position-manual-axis="y"]');
      if (xEl) xEl.value = Number.isFinite(x) ? String(x) : '';
      if (yEl) yEl.value = Number.isFinite(y) ? String(y) : '';
      const defer = window.setTimeout || setTimeout;
      defer(() => xEl?.focus(), 0);
    }
  };

  const applyManual = (root) => {
    const panel = root?.querySelector('[data-position-manual]');
    if (!panel) return;
    const xRaw = panel.querySelector('[data-position-manual-axis="x"]')?.value ?? '';
    const yRaw = panel.querySelector('[data-position-manual-axis="y"]')?.value ?? '';
    const x = String(xRaw).trim() === '' ? NaN : Number(xRaw);
    const y = String(yRaw).trim() === '' ? NaN : Number(yRaw);
    if (!setValue(root, { x, y }, { source: 'manual' })) return;
    toggleManual(root, false);
  };

  const onAction = (event) => {
    const button = event.target instanceof Element ? event.target.closest('[data-position-action]') : null;
    if (!button) return;
    const root = button.closest('[data-position-picker]');
    if (!root) return;
    const action = button.dataset.positionAction;
    if (action === 'map') { event.preventDefault(); openMap(root); return; }
    event.preventDefault();
    if (action === 'manual') toggleManual(root, !panelOpen(root));
    else if (action === 'manual-apply') applyManual(root);
    else if (action === 'manual-cancel') toggleManual(root, false);
  };

  const onFormEvent = (event) => {
    const target = event.target;
    if (!(target instanceof Element)) return;
    if (target.matches('[data-position-axis]')) {
      const root = target.closest('[data-position-picker]');
      if (!root) return;
      const { x, y, xInput, yInput } = readValue(root);
      if (String(xInput.value).trim() !== '' && String(yInput.value).trim() !== ''
        && Number.isFinite(x) && Number.isFinite(y)) root.dataset.positionPinned = '1';
      // Координаты изменились — старый вердикт карты о тайле уже не актуален.
      setValidity(root, true);
      updateText(root);
    }
    const path = target.getAttribute?.('data-path');
    if (path) {
      allRoots(document).forEach((root) => {
        if (root.getAttribute('data-map-world-path') !== path) return;
        if (findFieldInput(root, path) === target) {
          root.setAttribute('data-world-id', String(target.value ?? ''));
          updateText(root);
        }
      });
    }
  };

  const onWorldSelect = (event) => {
    const select = event.target instanceof Element ? event.target.closest('[data-position-world-select]') : null;
    if (!select) return;
    const root = select.closest('[data-position-picker]');
    if (!root) return;
    setWorldId(root, select.value, { source: 'select' });
  };

  const setValidity = (root, valid, tile) => {
    root.classList.toggle('is-invalid', !valid);
    root.dataset.positionValid = valid ? '1' : '0';
    const warning = root.querySelector('[data-position-warning]');
    if (warning) warning.hidden = valid;
    if (!valid && tile !== null && tile !== undefined) warning?.setAttribute('data-position-tile', String(tile));
    else warning?.removeAttribute('data-position-tile');
  };

  const onMapApplied = (event) => {
    const root = event.target instanceof Element ? event.target.closest('[data-position-picker]') : null;
    if (!root) return;
    root.dataset.positionPinned = '1';
    setValidity(root, true);
    const worldId = parseWorldId(event.detail?.worldId);
    // «Применить» — точка фиксации: именно здесь выбранный в карте мир
    // записывается в поле формы / callback редактора.
    if (worldId !== null) setWorldId(root, worldId, { source: 'map', save: true });
    else updateText(root);
  };
  const onMapWorld = (event) => {
    const root = event.target instanceof Element ? event.target.closest('[data-position-picker]') : null;
    if (!root) return;
    // Пока карта открыта, меняем только контекст элемента. Отмена не трогает БД.
    setWorldId(root, event.detail?.worldId, { source: 'map', save: false });
  };
  const onMapClosed = (event) => {
    const root = event.target instanceof Element ? event.target.closest('[data-position-picker]') : null;
    if (!root) return;
    // Вердикт о запрещённом тайле оставляем в карточке: точка не валидна,
    // но не сброшена — пользователь увидит предупреждение и поправит X/Y.
    // Снимется оно при следующем открытии карты или смене координат.
    if (event.detail?.reason === 'apply') { setValidity(root, true); return; }
    root.removeAttribute('data-world-preview');
    updateText(root);
  };
  const onMapValidity = (event) => {
    const root = event.target instanceof Element ? event.target.closest('[data-position-picker]') : null;
    if (!root) return;
    // Точка остаётся на месте: мы только подсвечиваем состояние, не сбрасываем X/Y.
    setValidity(root, event.detail?.valid !== false, event.detail?.tile);
  };

  const onManualKeydown = (event) => {
    if (!(event.target instanceof Element) || !event.target.matches('[data-position-manual-axis]')) return;
    const root = event.target.closest('[data-position-picker]');
    if (!root) return;
    if (event.key === 'Enter') { event.preventDefault(); applyManual(root); }
    else if (event.key === 'Escape') { event.preventDefault(); toggleManual(root, false); }
  };

  document.addEventListener('click', onAction);
  document.addEventListener('input', onFormEvent);
  document.addEventListener('change', (event) => { onFormEvent(event); onWorldSelect(event); });
  document.addEventListener('keydown', onManualKeydown);
  document.addEventListener('editor-map:applied', onMapApplied);
  document.addEventListener('editor-map:world', onMapWorld);
  document.addEventListener('editor-map:closed', onMapClosed);
  document.addEventListener('editor-map:validity', onMapValidity);

  let scanScheduled = false;
  const scheduleScan = () => {
    if (scanScheduled) return;
    scanScheduled = true;
    const defer = window.setTimeout || setTimeout;
    defer(() => { scanScheduled = false; scan(document); }, 0);
  };
  if (window.MutationObserver) {
    const observer = new MutationObserver(scheduleScan);
    observer.observe(document.documentElement || document, { childList: true, subtree: true });
  }
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', scheduleScan, { once: true });
  } else scheduleScan();
  window.EditorCore?.I18n?.onChange?.(() => allRoots(document).forEach(updateText));

  namespace.PositionPicker = {
    render,
    create(options = {}) {
      const template = document.createElement('template');
      template.innerHTML = render(options).trim();
      const element = template.content.firstElementChild;
      if (element) init(element);
      return element;
    },
    scan,
    init,
    update: updateText,
    getValue,
    setValue,
    getWorldId: (root) => parseWorldId(root?.getAttribute?.('data-world-id')),
    setWorldId,
    openMap,
    onWorldChange(root, callback) { if (root) worldCallbacks.set(root, callback); },
    onChange(root, callback) { if (root) changeCallbacks.set(root, callback); },
    getMapOptions: readMapOptions,
    resolveMapOptions,
    DEFAULT_BLOCKED_TILES,
    parseWorldId,
    isPinned,
  };
})();
