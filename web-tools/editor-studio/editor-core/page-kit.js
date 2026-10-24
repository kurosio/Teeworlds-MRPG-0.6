/**
 * editor-core/page-kit.js — то, что нужно каждой странице редактора.
 *
 * Страницы исторически заводили свои `esc`, `fmtDate`, `toast` и `plural`
 * (по одному набору на файл, с разным поведением). Здесь они собраны в одном
 * месте и учитывают язык интерфейса, поэтому:
 *
 *   const esc = EditorCore.Page.esc;
 *   const fmtDate = (iso) => EditorCore.Page.fmtDate(iso);
 *
 * Зависимости: editor-core/utils.js (экранирование, toasts), editor-core/i18n.js
 * (язык и формы слов). Оба подключаются раньше этого файла.
 */
(() => {
  if (window.EditorCore?.Page) return;
  const namespace = (window.EditorCore = window.EditorCore || {});

  /** Экранирование HTML. Данные из БД и от гостя проходят только через него. */
  const escapeHtml = (value) => namespace.utils.escapeHtml(value ?? '');

  const i18n = () => namespace.I18n || null;

  /** Перевод строки (если модуль языка уже загружен). */
  const t = (text, vars) => (typeof i18n()?.t === 'function' ? i18n().t(text, vars) : String(text ?? ''));

  const locale = () => i18n()?.locale?.() || 'ru-RU';

  /**
   * Дата в языке интерфейса. Невалидное значение превращается в прочерк,
   * а не в «Invalid Date» на экране.
   */
  const fmtDate = (value, options = { dateStyle: 'medium', timeStyle: 'short' }, fallback = '—') => {
    if (value === null || value === undefined || value === '') return fallback;
    const date = value instanceof Date ? value : new Date(value);
    if (Number.isNaN(date.getTime())) return fallback;
    try {
      return date.toLocaleString(locale(), options);
    } catch {
      return date.toLocaleString();
    }
  };

  /** Согласование числительных: RU — 1/2/5, EN — one/other. */
  const plural = (count, one, few, many) => {
    if (typeof i18n()?.plural === 'function') return i18n().plural(count, one, few, many);
    const n = Math.abs(Number(count) || 0);
    if (n % 10 === 1 && n % 100 !== 11) return one;
    return many ?? few ?? one;
  };

  /**
   * Тост (уведомление). Если UI ещё не поднят — пишем в консоль.
   *
   * Уведомление никогда не должно ломать сценарий: раньше исключение из
   * `showToast` (например, `EditorCore.UI` ещё не загружен) обрывало обработчик
   * на полпути — данные уже сохранены, а список на экране оставался прежним.
   */
  const toast = (message, type = 'success') => {
    try {
      const show = namespace.utils?.showToast || namespace.UI?.toast;
      if (typeof show === 'function') return show(message, type);
      (type === 'error' ? console.error : console.log)('[editor-studio]', message);
    } catch (error) {
      console.error('[editor-studio] toast', error);
    }
    return undefined;
  };

  const debounce = (fn, delay = 200) => namespace.utils.debounce(fn, delay);

  const copyText = async (text) => {
    try { return await namespace.utils.copyText(text); }
    catch { return false; }
  };

  // ── Диалоги ───────────────────────────────────────────────────────────────
  // Раньше подтверждения шли через нативный `confirm()`: он блокирует поток,
  // не переводится на английский одинаково во всех браузерах и выглядит
  // чужеродно. Здесь один модальный диалог на всю студию.

  const DIALOG_STYLE_ID = 'editor-core-dialog-style';

  const injectDialogStyles = () => {
    if (document.getElementById(DIALOG_STYLE_ID)) return;
    const style = document.createElement('style');
    style.id = DIALOG_STYLE_ID;
    style.textContent = `
      .ec-dialog-backdrop { position: fixed; inset: 0; z-index: 10000; display: grid;
        place-items: center; padding: 24px; background: rgba(2, 6, 23, .55);
        backdrop-filter: blur(2px); animation: ec-dialog-fade .12s ease-out; }
      .ec-dialog { width: min(440px, 100%); border-radius: 16px; padding: 22px 22px 18px;
        background: var(--editor-surface, #fff); color: var(--editor-text, #0f172a);
        border: 1px solid var(--editor-soft-border, rgba(148,163,184,.35));
        box-shadow: 0 24px 60px rgba(2, 6, 23, .28); font: inherit; }
      .ec-dialog h2 { margin: 0 0 8px; font-size: 15px; font-weight: 700; }
      .ec-dialog p { margin: 0; font-size: 13.5px; line-height: 1.55; white-space: pre-line; }
      .ec-dialog-actions { display: flex; justify-content: flex-end; gap: 8px; margin-top: 18px; }
      .ec-dialog button { padding: 8px 14px; border-radius: 10px; font: inherit; font-size: 13px;
        font-weight: 600; cursor: pointer; border: 1px solid transparent; transition: filter .15s ease; }
      .ec-dialog button:hover { filter: brightness(.96); }
      .ec-dialog button:focus-visible { outline: 2px solid var(--editor-primary, #2563eb); outline-offset: 2px; }
      .ec-dialog .ec-cancel { background: var(--editor-soft-bg, rgba(148,163,184,.16));
        color: inherit; border-color: var(--editor-soft-border, rgba(148,163,184,.35)); }
      .ec-dialog .ec-ok { background: var(--editor-primary, #2563eb); color: #fff; }
      .ec-dialog .ec-ok.is-danger { background: #dc2626; }
      @keyframes ec-dialog-fade { from { opacity: 0; } to { opacity: 1; } }
      @media (prefers-reduced-motion: reduce) { .ec-dialog-backdrop { animation: none; } }
    `;
    (document.head || document.documentElement).appendChild(style);
  };

  /**
   * Модальное подтверждение. Возвращает `Promise<boolean>`.
   *
   *   if (!await EditorCore.Page.confirm('Удалить запись?')) return;
   *   await EditorCore.Page.confirm(text, { danger: true, confirmText: 'Удалить' });
   *
   * Enter — подтверждение, Escape или клик мимо окна — отказ.
   */
  const confirmDialog = (message, options = {}) => new Promise((resolve) => {
    injectDialogStyles();
    const {
      title = '',
      confirmText = '',
      cancelText = '',
      danger = false,
      hideCancel = false,
    } = options;

    const backdrop = document.createElement('div');
    backdrop.className = 'ec-dialog-backdrop';
    const box = document.createElement('div');
    box.className = 'ec-dialog';
    box.setAttribute('role', 'alertdialog');
    box.setAttribute('aria-modal', 'true');

    if (title) {
      const heading = document.createElement('h2');
      heading.textContent = title;
      box.appendChild(heading);
    }
    const text = document.createElement('p');
    // Переводим сразу: диалог живёт вне страницы и не должен ждать наблюдателя.
    text.textContent = t(message);
    box.appendChild(text);

    const actions = document.createElement('div');
    actions.className = 'ec-dialog-actions';
    const cancel = document.createElement('button');
    cancel.type = 'button';
    cancel.className = 'ec-cancel';
    cancel.textContent = cancelText || t('Отмена');
    const ok = document.createElement('button');
    ok.type = 'button';
    ok.className = danger ? 'ec-ok is-danger' : 'ec-ok';
    ok.textContent = confirmText || t(danger ? 'Удалить' : 'Продолжить');
    if (hideCancel) actions.append(ok);
    else actions.append(cancel, ok);
    box.appendChild(actions);
    backdrop.appendChild(box);

    const previouslyFocused = document.activeElement;
    let finished = false;
    const finish = (result) => {
      if (finished) return;
      finished = true;
      document.removeEventListener('keydown', onKey, true);
      backdrop.remove();
      if (previouslyFocused?.focus) {
        try { previouslyFocused.focus(); } catch { /* элемент уже удалён */ }
      }
      resolve(result);
    };

    const order = hideCancel ? [ok] : [cancel, ok];

    function onKey(event) {
      // Escape = отказ; в информационном окне это и есть его единственная кнопка.
      if (event.key === 'Escape') { event.preventDefault(); finish(hideCancel); return; }
      if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        finish(hideCancel || document.activeElement === ok);
        return;
      }
      if (event.key !== 'Tab') return;
      // Фокус остаётся внутри диалога.
      const index = order.indexOf(document.activeElement);
      event.preventDefault();
      order[(index + (event.shiftKey ? order.length - 1 : 1)) % order.length].focus();
    }

    cancel.addEventListener('click', () => finish(false));
    ok.addEventListener('click', () => finish(true));
    backdrop.addEventListener('mousedown', (event) => {
      if (event.target === backdrop) finish(false);
    });
    document.addEventListener('keydown', onKey, true);
    document.body.appendChild(backdrop);
    // Фокус на «Отмене»: случайный Enter не удалит данные.
    (hideCancel ? ok : cancel).focus();
  });

  /** Информационное окно с одной кнопкой. */
  const alertDialog = (message, options = {}) => confirmDialog(message, {
    ...options,
    hideCancel: true,
    confirmText: options.confirmText || t('Понятно'),
  });

  /**
   * Предупреждение при уходе со страницы с несохранёнными правками.
   *
   *   EditorCore.Page.guardUnsaved(() => dirty.isDirty());
   *
   * Браузер показывает своё окно (текст задать нельзя), поэтому важно только
   * не потерять данные молча.
   */
  const guardUnsaved = (isDirty) => {
    const handler = (event) => {
      let dirty = false;
      try { dirty = !!isDirty(); } catch { dirty = false; }
      if (!dirty) return;
      event.preventDefault();
      event.returnValue = '';
      return '';
    };
    window.addEventListener('beforeunload', handler);
    return () => window.removeEventListener('beforeunload', handler);
  };

  /**
   * Единая точка журналирования неожиданных ошибок. Пустой `catch` скрывает
   * сбой; здесь он попадает в консоль с понятным контекстом.
   */
  const report = (scope, error) => {
    console.error(`[editor-studio] ${scope}`, error);
  };

  /**
   * Запрос к `api/*.php?action=…` — тонкая обёртка над `EditorCore.Http.json`.
   * Возвращает `{ ok, status, error, ...поля ответа }`.
   */
  const api = (endpoint, action, options = {}) => {
    const url = action == null ? endpoint : `${endpoint}?action=${encodeURIComponent(action)}`;
    const http = namespace.Http;
    if (!http?.json) throw new Error('EditorCore.Http is not available. Load http.js before page-kit.js.');
    return http.json(url, options);
  };

  /**
   * Выполняет действие, пока кнопка заблокирована: защита от двойной отправки
   * и от «залипших» кнопок, которые остаются активными во время запроса.
   */
  const withBusy = async (element, task) => {
    if (element && element.disabled) return undefined;
    const restore = element ? { had: element.getAttribute('aria-busy') } : null;
    if (element) {
      element.disabled = true;
      element.setAttribute('aria-busy', 'true');
    }
    try {
      return await task();
    } finally {
      if (element) {
        element.disabled = false;
        if (restore?.had == null) element.removeAttribute('aria-busy');
        else element.setAttribute('aria-busy', restore.had);
      }
    }
  };

  namespace.Page = {
    esc: escapeHtml,
    escapeHtml,
    t,
    locale,
    fmtDate,
    plural,
    toast,
    debounce,
    copyText,
    api,
    withBusy,
    confirm: confirmDialog,
    alert: alertDialog,
    guardUnsaved,
    report,
  };
})();
