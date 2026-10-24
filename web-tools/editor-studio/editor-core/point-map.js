/**
 * editor-core/point-map.js — окно «Местоположение»: карта мира с точкой.
 *
 * Кнопка появляется рядом с любой позицией (см. FieldRenderer, тип `vec2`):
 *   [X] [Y] [📍 Местоположение]
 *
 * Что делает окно:
 *   • открывает карту мира, выбранного в записи (по `world_id` → tw_worlds.Path);
 *   • показывает текущую точку, позволяет поставить новую кликом, перетащить
 *     маркер, двигать карту мышью и приближать колесом;
 *   • по кнопке «Применить» записывает X и Y в поля формы.
 *
 * Рендер карты — локальный (vendor/ddnet-map): .map и текстуры отдаёт наш же
 * сервер (api/map.php), интернет не нужен.
 */
(() => {
  const MIN_WORLD_FIELDS = ['WorldID', 'world_id', 'WorldId', 'worldId', 'Map', 'map'];
  const parseWorldId = (value) => {
    if (value === null || value === undefined || String(value).trim() === '') return null;
    const n = Number(value);
    return Number.isSafeInteger(n) && n >= 0 ? n : null;
  };

  // Мир, выбранный вручную: в редакторе сценариев у компонента (например,
  // «Убийство мобов») вообще нет поля мира — без этого карта не открывалась,
  // вместо неё был только тост «Сначала выберите мир».
  const WORLD_STORAGE_KEY = 'editor-map-world';

  /**
   * Мир записи, внутри которой открыт компонент: атрибут `data-map-world-id`
   * на общем контейнере (сценарий предмета/подземелья/квеста знает свой мир).
   * Это надёжнее «последнего выбранного вручную» мира из localStorage.
   */
  const contextWorldId = (row) => {
    const host = row?.closest?.('[data-map-world-id]');
    return parseWorldId(host?.getAttribute('data-map-world-id'));
  };

  let dialog = null;
  let iframe = null;
  let worldBar = null;
  let worldSelect = null;
  let session = null;

  const t = (text, params) => {
    const i18n = window.EditorCore?.I18n;
    return i18n?.t ? i18n.t(text, params) : text;
  };

  const toast = (message, type = 'error') => {
    const ui = window.EditorCore?.UI;
    if (ui?.toast) ui.toast(message, type);
    else console.warn('[editor-studio][point-map]', message);
  };

  const cssEscape = (value) => {
    if (window.CSS?.escape) return window.CSS.escape(String(value));
    return String(value).replace(/["\\\]]/g, '\\$&');
  };

  // ── Поиск полей в форме ────────────────────────────────────────────────────

  // Для вложенных форм важно брать поле из той же записи: поиск первым
  // совпадением в общем родителе мог выбрать мир соседнего шага/модалки.
  const domDistance = (a, b) => {
    const ancestors = new Map();
    let node = a;
    let distance = 0;
    while (node) {
      ancestors.set(node, distance++);
      node = node.parentElement;
    }
    node = b;
    distance = 0;
    while (node) {
      if (ancestors.has(node)) return ancestors.get(node) + distance;
      distance++;
      node = node.parentElement;
    }
    return Number.POSITIVE_INFINITY;
  };

  const nearestPathInput = (row, path) => {
    if (!row || !path) return null;
    const candidates = Array.from(document.querySelectorAll(`[data-path="${cssEscape(path)}"]`))
      .filter((node) => node !== row && !row.contains(node));
    candidates.sort((a, b) => domDistance(row, a) - domDistance(row, b));
    return candidates[0] || null;
  };

  /** Ближайший ввод мира: сначала путь, заданный схемой, затем стандартные. */
  const findWorldInput = (row) => {
    const hint = row.getAttribute('data-map-world-path') || '';
    const names = hint ? [hint, ...MIN_WORLD_FIELDS] : MIN_WORLD_FIELDS;
    for (const name of names) {
      const hit = nearestPathInput(row, name);
      if (hit) return hit;
    }
    return null;
  };

  const findInput = (row, path) => nearestPathInput(row, path);

  /**
   * Поле оси прямо в строке позиции. Так работают и поля формы (`data-path`),
   * и поля задач квеста (`data-task-field`): обе разновидности помечены
   * data-map-axis, поэтому искать по путям не обязательно.
   */
  const axisInput = (row, axis, path) => {
    const row2 = row.isConnected ? row : session?.row;
    const direct = row2?.querySelector(`[data-map-axis="${axis}"]`);
    if (direct) return direct;
    return findInput(row2 || row, path);
  };

  const worldLabel = (input) => {
    if (!input) return '';
    const combo = input.closest('.editor-dbcombo');
    const shown = combo?.querySelector('.editor-dbcombo-input')?.value || '';
    return String(shown || '').trim();
  };

  // ── Окно ──────────────────────────────────────────────────────────────────

  const ensureDialog = () => {
    if (dialog) return dialog;
    dialog = document.createElement('div');
    dialog.className = 'editor-map-dialog';
    dialog.setAttribute('role', 'dialog');
    dialog.setAttribute('aria-modal', 'true');
    dialog.setAttribute('aria-label', t('Местоположение'));
    dialog.innerHTML = `
      <div class="editor-map-dialog-frame">
        <div class="editor-map-dialog-world" hidden>
          <label class="editor-map-dialog-world-label" for="editor-map-world-select">${t('Мир')}</label>
          <select id="editor-map-world-select" class="editor-map-dialog-world-select"></select>
        </div>
        <iframe class="editor-map-dialog-frame-inner" title="${t('Карта мира')}"
          referrerpolicy="no-referrer" allow="fullscreen"></iframe>
      </div>`;
    iframe = dialog.querySelector('iframe');
    iframe.addEventListener('load', () => {
      if (!session || !session.worldName || !iframe.contentWindow) return;
      iframe.contentWindow.postMessage({ type: 'editor-map:world-name', name: session.worldName }, window.location.origin);
    });
    worldBar = dialog.querySelector('.editor-map-dialog-world');
    worldSelect = dialog.querySelector('.editor-map-dialog-world-select');
    worldSelect.addEventListener('change', () => {
      const value = parseWorldId(worldSelect.value);
      if (value === null) return;
      rememberWorld(value);
      if (session) session.worldId = value;
      loadMap();
    });
    dialog.addEventListener('click', (event) => {
      // Клик по затемнению вокруг окна закрывает его, как обычный попап.
      if (event.target === dialog) close('backdrop');
    });
    document.body.appendChild(dialog);
    return dialog;
  };

  const rememberWorld = (id) => {
    if (parseWorldId(id) === null) return;
    try { window.localStorage.setItem(WORLD_STORAGE_KEY, String(id)); } catch { /* приватный режим */ }
  };

  const rememberedWorld = () => {
    try { return parseWorldId(window.localStorage.getItem(WORLD_STORAGE_KEY)); } catch { return null; }
  };

  const worldLabelById = (id) => {
    const option = worldSelect?.querySelector(`option[value="${cssEscape(id)}"]`);
    return option ? String(option.textContent || '').trim() : '';
  };

  /**
   * Список миров для ручного выбора. Гостю worlds может быть недоступен —
   * тогда остаётся число: мир всё равно нужен, но окно не падает.
   */
  const fillWorldList = async (selectedId) => {
    if (!worldSelect) return;
    if (worldSelect.dataset.loaded === '1') {
      worldSelect.value = selectedId === null || selectedId === undefined ? '' : String(selectedId);
      return;
    }
    let rows = [];
    try {
      const res = await window.EditorCore?.DBCrud?.list?.('worlds', { limit: 1000, offset: 0 });
      rows = Array.isArray(res?.rows) ? res.rows : [];
    } catch { rows = []; }

    const options = ['<option value="">' + t('— выберите мир —') + '</option>'];
    rows.forEach((row) => {
      const id = parseWorldId(row.ID ?? row.id);
      if (id === null) return;
      const name = String(row.Name ?? row.name ?? '');
      options.push(`<option value="${id}">${id}: ${escapeHtml(name)}</option>`);
    });
    if (rows.length === 0) {
      options.push('<option value="" disabled>' + t('Список миров недоступен') + '</option>');
    }
    worldSelect.innerHTML = options.join('');
    worldSelect.dataset.loaded = '1';
    worldSelect.value = selectedId === null || selectedId === undefined ? '' : String(selectedId);
  };

  const escapeHtml = (value) => window.EditorCore.utils.escapeHtml(value);

  const onMessage = (event) => {
    if (!session || !iframe || event.source !== iframe.contentWindow || event.origin !== window.location.origin) return;
    const data = event.data || {};
    if (data.type === 'editor-map:apply') {
      applyPoint(data.x, data.y);
      close('apply');
    } else if (data.type === 'editor-map:close') {
      close('cancel');
    }
  };

  const onKeydown = (event) => {
    if (!session) return;
    if (event.key === 'Escape') {
      event.preventDefault();
      close('escape');
    }
  };

  const lockScroll = (lock) => {
    // Не трогаем document.body.style.overflow: этот приём уже ломал прокрутку
    // страницы (окно входа). Достаточно position:fixed у самого затемнения.
    document.documentElement.classList.toggle('editor-map-open', lock);
  };

  const close = (reason) => {
    if (!session) return;
    const { restoreFocus } = session;
    session = null;
    window.removeEventListener('message', onMessage);
    window.removeEventListener('keydown', onKeydown, true);
    lockScroll(false);
    dialog?.classList.remove('is-open');
    dialog?.setAttribute('aria-hidden', 'true');
    if (iframe) iframe.src = 'about:blank';
    try { restoreFocus?.focus?.({ preventScroll: true }); } catch { /* поле могли перерисовать */ }
    if (reason === 'cancel' || reason === 'escape') {
      // Пользователь передумал: значения формы не трогаем.
    }
  };

  /**
   * Адрес API карт. Считается от самой страницы редактора (document.baseURI),
   * а не от пути до picker.html: редактор может жить в подкаталоге
   * (/editor/quests-editor.html), и относительный путь из vendor/ddnet-map
   * вёл в /editor/vendor/api/map.php — отсюда был 404.
   */
  const apiUrl = (query) => new URL(`api/map.php?${query}`, document.baseURI).href;

  const pickerUrl = ({ worldId, worldName, x, y, hasPoint }) => {
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
    return new URL(`vendor/ddnet-map/picker.html?${params.toString()}`, document.baseURI).href;
  };

  /** Загрузить карту в окно. Мир берём из сессии (поле формы или ручной выбор). */
  const loadMap = () => {
    if (!session || !iframe) return;
    const worldInput = session.worldInput;
    const fromField = parseWorldId(worldInput?.value);
    if (fromField !== null) session.worldId = fromField;

    const fixedWorldId = parseWorldId(session.fixedWorldId);
    const worldId = parseWorldId(session.worldId);
    const worldName = fromField !== null ? (worldLabel(worldInput) || worldLabelById(fromField)) : worldLabelById(worldId);

    // ID 0 — полноценный мир (Tutorilishe), а не признак отсутствующего мира.
    if (worldBar) worldBar.hidden = fixedWorldId !== null;
    if (worldSelect && fromField === null) worldSelect.value = worldId !== null ? String(worldId) : '';

    // Подпись мира берётся из списка миров: если он ещё грузится, обновим
    // заголовок карты один раз, когда список придёт.
    session.needsWorldName = worldId !== null && !worldName;
    session.worldName = worldName;

    iframe.src = pickerUrl({
      worldId,
      worldName,
      x: session.startX,
      y: session.startY,
      hasPoint: session.hasPoint,
    });
    try { iframe.focus(); } catch { /* не критично */ }
  };

  const open = (row) => {
    const xPath = row.getAttribute('data-map-x-path') || '';
    const yPath = row.getAttribute('data-map-y-path') || '';
    const xInput = axisInput(row, 'x', xPath);
    const yInput = axisInput(row, 'y', yPath);
    if (!xInput || !yInput) {
      toast(t('Не удалось найти поля координат — обновите страницу'));
      return;
    }

    // Мир ищем в форме; если поля нет (компонент сценария) — берём мир
    // записи, в которой открыт компонент; и только затем последний выбранный
    // вручную. Выбор в окне остаётся, когда мир неизвестен совсем.
    const worldInput = findWorldInput(row);
    const fieldWorldId = parseWorldId(worldInput?.value);
    const contextId = contextWorldId(row);
    const fixedWorldId = fieldWorldId !== null ? fieldWorldId : contextId;
    const fallbackWorldId = fixedWorldId !== null ? fixedWorldId : rememberedWorld();

    const hasPoint = String(xInput.value).trim() !== '' || String(yInput.value).trim() !== '';
    const startX = Number(xInput.value || 0) || 0;
    const startY = Number(yInput.value || 0) || 0;

    ensureDialog();
    dialog.classList.add('is-open');
    dialog.removeAttribute('aria-hidden');
    lockScroll(true);

    session = {
      xPath,
      yPath,
      row,
      worldInput,
      worldId: fallbackWorldId,
      fixedWorldId,
      startX,
      startY,
      // (0, 0) — допустимая координата; её нельзя путать с отсутствием точки.
      hasPoint,
      restoreFocus: document.activeElement,
    };

    // Родитель ищет поля заново на каждое применение: форма могла перерисоваться.
    window.removeEventListener('message', onMessage);
    window.removeEventListener('keydown', onKeydown, true);
    window.addEventListener('message', onMessage);
    window.addEventListener('keydown', onKeydown, true);

    // Список нужен и для подписи мира записи, и для ручного выбора.
    loadMap();
    fillWorldList(fallbackWorldId).then(() => {
      if (!session?.needsWorldName) return;
      const name = worldLabelById(session.worldId);
      if (!name) return;
      session.worldName = name;
      session.needsWorldName = false;
      if (iframe?.contentWindow) {
        iframe.contentWindow.postMessage({ type: 'editor-map:world-name', name }, window.location.origin);
      }
    }).catch(() => {});
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
    if (!session) return;
    const nx = Number(x);
    const ny = Number(y);
    if (!Number.isFinite(nx) || !Number.isFinite(ny)) return;
    const { xPath, yPath } = session;
    writeValue('x', xPath, nx);
    writeValue('y', yPath, ny);

    // Некоторые формы перерисовываются на первом же событии: если значение
    // сбросилось — записываем ещё раз, уже в новое поле.
    window.requestAnimationFrame(() => {
      if (!session) return;
      [[ 'x', xPath, nx ], [ 'y', yPath, ny ]].forEach(([axis, path, value]) => {
        const input = axisInput(session.row, axis, path);
        if (input && Number(input.value) !== Math.round(value)) writeValue(axis, path, value);
      });
    });
  };

  // Одна точка входа: кнопка «Местоположение» у любой позиции.
  document.addEventListener('click', (event) => {
    const button = event.target instanceof Element ? event.target.closest('[data-map-pick]') : null;
    if (!button) return;
    const row = button.closest('.editor-vec2');
    if (!row) return;
    event.preventDefault();
    open(row);
  });

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.PointMap = { open, close: () => close('api') };
})();
