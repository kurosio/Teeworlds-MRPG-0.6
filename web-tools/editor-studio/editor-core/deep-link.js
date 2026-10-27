/**
 * editor-core/deep-link.js — переход к конкретной записи в редакторе.
 *
 * Ссылка вида `items-editor.html?resource=items&record=42&field=Comment`
 * открывает вкладку сразу на нужной записи и подсвечивает поле. Такие ссылки
 * строит страница «Запросы» (гость открывает свою запись, администратор —
 * запись из запроса гостя).
 *
 * Модуль намеренно не знает про конкретный редактор: он ждёт, пока список
 * отрисует строку с `data-id`, и «нажимает» её — тот же путь, что и у
 * пользователя. Так работают и простые CRUD-редакторы, и дерево квестов.
 * Снимок запроса (`?request=…`) управляет выбором записи сам, поэтому там
 * модуль отключается; `request-context.js` подсвечивает поле по своим правилам.
 */
(() => {
  const namespace = (window.EditorCore = window.EditorCore || {});
  if (namespace.DeepLink) return;

  // Те же строки, что помечает change-markers.js: единый вид списков.
  const ROW_SELECTOR = '.editor-row[data-id], [data-node-type="quest"][data-id], [data-node-type="step"][data-id]';
  const ACTIVE_CLASSES = ['editor-row-active', 'ring-2', 'tree-selected', 'active', 'is-active', 'selected'];
  const POLL_MS = 200;
  const POLL_LIMIT_MS = 30000;
  const FLASH_MS = 2600;

  const query = new URLSearchParams(window.location?.search || '');
  const requestId = String(query.get('request') || '').trim();
  const parsedRecord = Number(query.get('record'));
  const resource = String(query.get('resource') || '');
  const active = !requestId && query.has('record') && String(query.get('record')).trim() !== ''
    && Number.isSafeInteger(parsedRecord) && (parsedRecord > 0 || (parsedRecord === 0 && resource === 'worlds'));
  const record = active ? parsedRecord : 0;
  // Имя поля из URL сравнивается со `data-field-path` как строка (в селектор
  // оно не попадает), длина ограничена — этого достаточно для безопасности.
  const field = requestId ? '' : String(query.get('field') || '').trim().slice(0, 120);

  let done = false;
  let timer = 0;
  let flashTimer = 0;
  let deadline = 0;

  const rowFor = (id) => {
    for (const node of document.querySelectorAll(ROW_SELECTOR)) {
      const nodeType = node.dataset?.nodeType;
      if (nodeType && resource === 'quests' && nodeType !== 'quest') continue;
      if (nodeType && resource === 'quest_bots' && nodeType !== 'step') continue;
      if (Number(node.dataset?.id) === id) return node;
    }
    return null;
  };

  /** Найти узел поля по имени (точное совпадение, затем по префиксу пути). */
  const fieldNode = () => {
    if (!field) return null;
    const nodes = Array.from(document.querySelectorAll('[data-field-path]'));
    return nodes.find((node) => node.dataset.fieldPath === field)
      || nodes.find((node) => String(node.dataset.fieldPath || '').split('.')[0] === field)
      || nodes.find((node) => String(node.dataset.fieldPath || '').startsWith(`${field}.`))
      || null;
  };

  /** Подсветить поле, к которому вела ссылка (идемпотентно). */
  const revealField = () => {
    const node = fieldNode();
    if (!node) return false;
    try { node.scrollIntoView({ block: 'center', behavior: 'smooth' }); } catch { node.scrollIntoView?.(); }
    node.classList.add('field-link-target');
    window.clearTimeout(flashTimer);
    flashTimer = window.setTimeout(() => node.classList.remove('field-link-target'), FLASH_MS);
    return true;
  };

  const stop = () => {
    done = true;
    window.clearInterval(timer);
    timer = 0;
  };

  const attempt = () => {
    if (done) return;
    if (Date.now() > deadline) { stop(); return; }
    // Форма и список перерисовываются после выбора записи, поэтому поле
    // ищем с небольшой задержкой, но не блокируя остальную страницу.
    const row = rowFor(record);
    if (!row) return;
    stop();
    try { row.scrollIntoView({ block: 'nearest' }); } catch { /* старые браузеры */ }
    // Запись уже выбрана (например, редактор открыл её сам) — не перещёлкиваем.
    const selected = ACTIVE_CLASSES.some((name) => row.classList.contains(name))
      || row.getAttribute('aria-selected') === 'true';
    if (!selected) row.click();
    window.setTimeout(revealField, 300);
  };

  const start = () => {
    if (!active) return;
    deadline = Date.now() + POLL_LIMIT_MS;
    attempt();
    if (!done) timer = window.setInterval(attempt, POLL_MS);
  };

  window.EditorCore.DeepLink = {
    active,
    resource,
    record,
    field,
    revealField,
    /** Запись выбрана из кода (CRUD-редактор прочитал её по ID) — прекращаем опрос. */
    markHandled: stop,
    // Custom editors can resolve a URL target even outside the first page.
    async resolveRow(targetResource, rows) {
      if (!active || (resource && resource !== targetResource)) return null;
      stop();
      let row = rows.find((item) => Number(item.ID) === record);
      if (!row) {
        const response = await namespace.DBCrud.get(targetResource, record);
        row = response?.row;
        if (row) rows.unshift(row);
      }
      if (!row) namespace.UI?.toast?.(namespace.I18n?.t?.('The linked record was not found.') || 'The linked record was not found.', 'error');
      return row || null;
    },
  };

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start, { once: true });
  else start();
})();
