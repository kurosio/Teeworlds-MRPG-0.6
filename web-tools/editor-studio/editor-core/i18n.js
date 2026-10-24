/**
 * editor-core/i18n.js — переключение языка интерфейса (RU / EN).
 *
 * Исходный язык интерфейса — русский: он лежит прямо в разметке и в коде
 * страниц. Английский каталог (`i18n-en.js`) сопоставляет русским строкам
 * английские. Такой подход выбран осознанно:
 *
 *  • не нужно размечать тысячи мест ключами — переводится любой узел DOM,
 *    включая тот, что страница отрисовала уже после загрузки (наблюдатель
 *    за изменениями переводит и его);
 *  • если перевода нет, интерфейс остаётся рабочим (показывается русская
 *    строка), а не «пустым» местом или ключом вида `nav.items`.
 *
 * Ключи каталога могут содержать подстановки `{0}`, `{1}` — они превращаются
 * в шаблоны и совпадают с уже отрисованным текстом («Найдено: 12»). Это
 * позволяет переводить сообщения со счётчиками, не правя вызывающий код.
 *
 * Использование:
 *   EditorCore.I18n.t('Найдено: {0}', [12])   // → 'Found: 12' при lang = 'en'
 *   EditorCore.I18n.set('en')                 // переключить язык
 *   EditorCore.I18n.onChange((lang) => {})    // реакция на смену языка
 */
(() => {
  // Повторное подключение файла (например, при ручной вставке тега) не должно
  // создавать второй наблюдатель за DOM и второй набор обратных замен.
  if (window.__editorCoreI18n) return;
  window.__editorCoreI18n = true;

  const LANG_KEY = 'editorStudio.lang';
  const SUPPORTED = ['ru', 'en'];
  const NAMES = { ru: 'Русский', en: 'English' };
  const SHORT = { ru: 'RU', en: 'EN' };
  const CYRILLIC = /[А-Яа-яЁё]/;
  const TEXT_ATTRS = ['title', 'placeholder', 'aria-label', 'alt', 'data-tooltip'];
  const SKIP_TAGS = new Set(['SCRIPT', 'STYLE', 'CODE', 'PRE', 'KBD', 'SAMP', 'TEXTAREA']);

  const namespace = (window.EditorCore = window.EditorCore || {});

  const readLang = () => {
    try {
      const stored = window.localStorage?.getItem(LANG_KEY);
      if (SUPPORTED.includes(stored)) return stored;
    } catch { /* приватный режим — работаем без сохранения */ }
    const nav = String(window.navigator?.language || '').toLowerCase();
    return nav.startsWith('en') ? 'en' : 'ru';
  };

  let lang = readLang();
  const listeners = new Set();
  /** Обратные замены: чтобы вернуть русский текст без перезагрузки страницы. */
  const originals = [];
  let applying = false;
  let observer = null;
  const patternCache = new Map();

  const dictFor = (code) => window.EditorCoreDict?.[code] || null;
  const normalize = (text) => String(text).replace(/\s+/g, ' ').trim();
  const escapeRe = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

  // Служебные слова: шаблон, в котором нет ничего, кроме предлога, совпадёт с
  // любой фразой («{0} из {0}» ломает «Удалить запрос из истории») — такие
  // ключи не используются как шаблоны.
  const WEAK_LITERALS = new Set([
    'из', 'в', 'во', 'на', 'с', 'со', 'от', 'до', 'по', 'для', 'к', 'ко', 'о', 'об',
    'при', 'за', 'под', 'над', 'без', 'и', 'или', 'а', 'но', 'же', 'ли', 'не', 'что',
  ]);

  /**
   * «Вес» шаблона: 0 — ключ не годится как шаблон (нет собственного текста
   * или это одно служебное слово), иначе — чем больше, тем он специфичнее.
   */
  const literalWeight = (key) => {
    const literal = key.replace(/\{\d+\}/g, ' ').replace(/\s+/g, ' ').trim();
    if (!literal) return 0;
    const words = literal.split(' ').filter(Boolean);
    const strong = words.filter((word) => /[A-Za-zА-Яа-яЁё0-9]{2}/.test(word) && !WEAK_LITERALS.has(word.toLowerCase()));
    if (!strong.length) return 0;
    return literal.length + words.length * 4;
  };

  /** Шаблоны из ключей с подстановками, разложенные по первому слову строки. */
  const buildPatterns = () => {
    patternCache.clear();
    patternCache.set('*', []);
    const dict = dictFor(lang);
    if (!dict) return;
    for (const [rawKey, rawValue] of Object.entries(dict)) {
      if (!rawKey.includes('{') || typeof rawValue !== 'string') continue;
      const key = normalize(rawKey);
      if (!literalWeight(key)) continue;
      // Подстановки нумеруются по порядку появления: в значении {0} — первая,
      // {1} — вторая и т. д. Так работают и ключи, где один индекс повторяется
      // («Запрос {0}: {0} …»), потому что текст на экране всё равно разный.
      let source = '^';
      let last = 0;
      let occurrences = 0;
      key.replace(/\{\d+\}/g, (match, offset) => {
        source += escapeRe(key.slice(last, offset)).replace(/ /g, '\\s+');
        source += '([\\s\\S]*?)';
        occurrences += 1;
        last = offset + match.length;
        return match;
      });
      source += escapeRe(key.slice(last)).replace(/ /g, '\\s+') + '$';

      // Значение разбивается на куски: строка — как есть, число — номер группы
      // подстановки. Собираем результат вручную, чтобы подставленные значения
      // тоже можно было перевести (например текст ошибки внутри сообщения).
      const segments = [];
      let tail = 0;
      rawValue.replace(/\{(\d+)\}/g, (match, num, offset) => {
        if (offset > tail) segments.push(rawValue.slice(tail, offset));
        const index = Number(num) < occurrences ? Number(num) : 0;
        segments.push(index);
        tail = offset + match.length;
        return match;
      });
      if (tail < rawValue.length) segments.push(rawValue.slice(tail));

      const head = key.startsWith('{') ? '*' : key.split(' ')[0];
      const bucket = patternCache.get(head) || patternCache.set(head, []).get(head);
      bucket.push({ re: new RegExp(source, 'u'), segments, weight: literalWeight(key) });
    }
  };

  /** Перевод произвольной строки: точное совпадение, затем шаблоны. */
  const translate = (text) => {
    const raw = String(text ?? '');
    if (!raw) return raw;
    const dict = dictFor(lang);
    if (!dict) return raw;
    const core = normalize(raw);
    if (!core) return raw;
    const exact = dict[core];
    if (typeof exact === 'string') return exact;
    if (patternCache.size === 0) buildPatterns();
    const head = core.split(' ')[0];
    const candidates = [patternCache.get(head), patternCache.get('*')];
    for (const bucket of candidates) {
      if (!bucket) continue;
      for (const { re, segments } of bucket) {
        const match = core.match(re);
        if (!match) continue;
        return segments.map((segment) => {
          if (typeof segment === 'string') return segment;
          const value = match[segment + 1] || '';
          // Подставленное значение тоже может быть текстом интерфейса
          // (например, вложенное сообщение сервера) — переводим и его, сохраняя
          // пробелы вокруг: «Напишите гостю» → «Write to the guest».
          const trimmed = normalize(value);
          if (!trimmed) return value;
          const nested = dict[trimmed];
          if (typeof nested !== 'string') return value;
          const lead = value.match(/^\s*/)[0];
          const tail = value.match(/\s*$/)[0];
          return lead + nested + tail;
        }).join('');
      }
    }
    return raw;
  };

  const remember = (entry) => {
    originals.push(entry);
    // Не даём списку расти бесконечно на долгоживущей странице.
    if (originals.length > 4000) originals.splice(0, 1000);
  };

  const translateTextNode = (node) => {
    const raw = node.nodeValue;
    if (!raw || !CYRILLIC.test(raw)) return;
    const match = raw.match(/^(\s*)([\s\S]*?)(\s*)$/);
    if (!match) return;
    const [, lead, core, tail] = match;
    if (!core) return;
    const next = translate(core);
    if (next === core) return;
    remember({ kind: 'text', node, original: raw });
    node.nodeValue = lead + next + tail;
  };

  const translateAttr = (el, attr) => {
    if (!el || !el.getAttribute) return;
    const value = el.getAttribute(attr);
    if (!value || !CYRILLIC.test(value)) return;
    const next = translate(value);
    if (next === value) return;
    remember({ kind: 'attr', node: el, attr, original: value });
    el.setAttribute(attr, next);
  };

  /** Узел, внутри которого текст — данные, а не интерфейс. */
  const isDataNode = (el) => !!el && (SKIP_TAGS.has(el.tagName) || el.isContentEditable === true);

  /** Элемент и всё его поддерево исключены из перевода автором страницы. */
  const isOptedOut = (el) => !!el && el.hasAttribute && el.hasAttribute('data-i18n-skip');

  /** Перевод одного узла и его потомков (без узлов, где текст — данные). */
  const translateTree = (node) => {
    if (!node) return;
    if (node.nodeType === 3) {
      const parent = node.parentElement;
      if (isDataNode(parent) || isOptedOut(parent)) return;
      translateTextNode(node);
      return;
    }
    if (node.nodeType !== 1) return;
    if (isOptedOut(node)) return;
    // Подписи и подсказки переводим даже у «данных» (например, placeholder
    // у textarea), а вот содержимое таких элементов не трогаем.
    if (isDataNode(node)) {
      for (const attr of TEXT_ATTRS) translateAttr(node, attr);
      return;
    }
    if (node.tagName === 'TITLE' && node.firstChild) {
      translateTextNode(node.firstChild);
      return;
    }
    for (const attr of TEXT_ATTRS) translateAttr(node, attr);
    if ((node.tagName === 'INPUT' || node.tagName === 'BUTTON')
      && ['submit', 'button', 'reset'].includes(String(node.type).toLowerCase())) {
      translateAttr(node, 'value');
    }
    for (const child of Array.from(node.childNodes)) translateTree(child);
  };

  const restore = () => {
    for (const entry of originals) {
      if (entry.kind === 'title') { document.title = entry.original; continue; }
      if (!entry.node || entry.node.isConnected === false) continue;
      if (entry.kind === 'text') entry.node.nodeValue = entry.original;
      else entry.node.setAttribute(entry.attr, entry.original);
    }
    originals.length = 0;
  };

  /** Переводит поддерево (или весь документ). Русский режим — без изменений. */
  const apply = (root = document) => {
    if (!root || lang === 'ru') return;
    applying = true;
    try {
      if (root === document) {
        if (document.title) {
          const next = translate(document.title);
          if (next !== document.title) {
            remember({ kind: 'title', original: document.title });
            document.title = next;
          }
        }
        translateTree(document.documentElement);
      } else {
        translateTree(root);
      }
    } finally {
      applying = false;
    }
  };

  const syncSwitcher = (box) => {
    box.querySelectorAll('[data-lang]').forEach((btn) => {
      const active = btn.dataset.lang === lang;
      btn.classList.toggle('is-active', active);
      btn.setAttribute('aria-pressed', active ? 'true' : 'false');
    });
  };

  const mountSwitcher = (host) => {
    if (!host || !host.appendChild) return null;
    const existing = host.querySelector('[data-i18n-switcher]');
    if (existing) { syncSwitcher(existing); return existing; }
    const box = document.createElement('div');
    box.className = 'i18n-switcher';
    box.setAttribute('data-i18n-switcher', '1');
    box.setAttribute('role', 'group');
    box.setAttribute('aria-label', 'Язык интерфейса');
    for (const code of SUPPORTED) {
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.dataset.lang = code;
      btn.textContent = SHORT[code];
      btn.title = NAMES[code];
      btn.setAttribute('aria-label', NAMES[code]);
      btn.addEventListener('click', () => set(code));
      box.appendChild(btn);
    }
    host.appendChild(box);
    syncSwitcher(box);
    return box;
  };

  /**
   * Переключатель языка нужен ровно один на экран:
   *  • в студии он живёт в верхней панели (.studio-workbar-actions), а страницы
   *    редакторов открываются в iframe — свой переключатель им не нужен,
   *    язык и так общий (localStorage + событие storage);
   *  • если страницу редактора открыли отдельным файлом, переключатель
   *    появляется в её собственных действиях (.editor-page-actions);
   *  • [data-i18n-switcher-host] — явное место (например, экран входа).
   */
  const embeddedInStudio = () => {
    try { return window.top !== window.self; } catch { return true; }
  };

  const mountSwitchers = (root = document) => {
    // Внутри студии переключатель уже есть в её верхней панели.
    if (embeddedInStudio()) return;
    const hosts = new Set();
    root.querySelectorAll?.('[data-i18n-switcher-host], .studio-workbar-actions').forEach((el) => hosts.add(el));
    // Если на странице есть явные места под переключатель, они и решают:
    // иначе в панель действий рядом с ними попал бы второй такой же.
    if (!hosts.size) {
      // У страницы может быть несколько панелей действий (шапка и липкая панель
      // редактора) — переключатель ставим только в первую, иначе он дублируется.
      const pageActions = root.querySelector?.('.editor-page-actions');
      if (pageActions) hosts.add(pageActions);
    }
    hosts.forEach(mountSwitcher);
    document.querySelectorAll('[data-i18n-switcher]').forEach(syncSwitcher);
  };

  const notify = () => {
    for (const fn of Array.from(listeners)) {
      try { fn(lang); } catch (error) { console.error('[editor-studio][i18n]', error); }
    }
    window.dispatchEvent(new CustomEvent('editor-core:lang-changed', { detail: { lang, locale: locale() } }));
  };

  const observe = () => {
    if (observer || !window.MutationObserver || !document.documentElement) return;
    observer = new MutationObserver((records) => {
      if (applying || lang === 'ru') return;
      applying = true;
      try {
        for (const record of records) {
          if (record.type === 'characterData') translateTextNode(record.target);
          else if (record.type === 'attributes') translateAttr(record.target, record.attributeName);
          else record.addedNodes.forEach((node) => translateTree(node));
        }
      } finally {
        applying = false;
      }
    });
    observer.observe(document.documentElement, {
      childList: true,
      subtree: true,
      characterData: true,
      attributes: true,
      attributeFilter: TEXT_ATTRS,
    });
  };

  const injectStyles = () => {
    if (document.getElementById('i18n-switcher-style')) return;
    const style = document.createElement('style');
    style.id = 'i18n-switcher-style';
    style.textContent = `
      .i18n-switcher { display: inline-flex; align-items: center; gap: 2px; padding: 3px;
        border-radius: 999px; border: 1px solid var(--editor-soft-border, rgba(148,163,184,.35));
        background: var(--editor-soft-bg, rgba(148,163,184,.12)); }
      .i18n-switcher button { min-width: 34px; padding: 3px 8px; border: 0; border-radius: 999px;
        font: 600 11px/1.6 inherit; letter-spacing: .04em; cursor: pointer; color: inherit;
        background: transparent; opacity: .65; transition: background .15s ease, opacity .15s ease; }
      .i18n-switcher button:hover { opacity: 1; }
      .i18n-switcher button.is-active { opacity: 1; color: #fff; background: var(--editor-primary, #2563eb); }
      .i18n-switcher button:focus-visible { outline: 2px solid var(--editor-primary, #2563eb); outline-offset: 1px; }
    `;
    (document.head || document.documentElement).appendChild(style);
  };

  const locale = () => (lang === 'en' ? 'en-US' : 'ru-RU');

  /**
   * Согласование числительных. Русские формы передаются как обычно
   * (1 операция / 2 операции / 5 операций); по-английски берётся единственная
   * или множественная форма, переведённая по каталогу.
   */
  const plural = (n, one, few, many) => {
    const count = Number(n) || 0;
    if (lang === 'en') return translate(count === 1 ? one : (many ?? few ?? one));
    const m10 = count % 10;
    const m100 = count % 100;
    if (m10 === 1 && m100 !== 11) return one;
    if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14)) return few;
    return many;
  };

  const set = (code, { force = false } = {}) => {
    const next = SUPPORTED.includes(code) ? code : 'ru';
    const changed = next !== lang;
    if (!changed && !force) return lang;
    applying = true;
    try { if (next === 'ru') restore(); } finally { applying = false; }
    lang = next;
    try { window.localStorage?.setItem(LANG_KEY, lang); } catch { /* приватный режим */ }
    document.documentElement.setAttribute('lang', lang);
    apply(document);
    document.querySelectorAll('[data-i18n-switcher]').forEach(syncSwitcher);
    if (changed) notify();
    return lang;
  };

  /**
   * Перевод строки с подстановками: `I18n.t('Найдено: {0}', [count])`.
   * В русском режиме возвращает исходную строку с подставленными значениями.
   */
  const t = (text, vars) => {
    const raw = String(text ?? '');
    const list = Array.isArray(vars) ? vars : (vars === undefined || vars === null ? [] : [vars]);
    const fill = (template) => String(template).replace(/\{(\d+)\}/g, (whole, index) => {
      const value = list[Number(index)];
      return value === undefined || value === null ? whole : String(value);
    });
    if (lang === 'ru') return fill(raw);
    const translated = translate(raw);
    return fill(translated === raw ? raw : translated);
  };

  /**
   * Нативные окна (alert/confirm/prompt) не зависят от DOM, поэтому их текст
   * переводим на входе — иначе в английском режиме они остались бы русскими.
   */
  const wrapDialogs = () => {
    if (window.__editorI18nDialogs) return;
    window.__editorI18nDialogs = true;
    for (const name of ['alert', 'confirm', 'prompt']) {
      const original = window[name];
      if (typeof original !== 'function') continue;
      window[name] = function patched(message, ...rest) {
        if (lang === 'en') {
          const translated = translate(message);
          if (translated !== message) return original.call(window, translated, ...rest);
        }
        return original.call(window, message, ...rest);
      };
    }
  };

  const init = () => {
    wrapDialogs();
    injectStyles();
    document.documentElement.setAttribute('lang', lang);
    mountSwitchers(document);
    if (lang !== 'ru') apply(document);
    observe();
    // Язык сменили в другом документе (шапка студии, соседняя вкладка).
    window.addEventListener('storage', (event) => {
      if (event.key !== LANG_KEY || !event.newValue || event.newValue === lang) return;
      if (SUPPORTED.includes(event.newValue)) set(event.newValue);
    });
  };

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
  } else {
    init();
  }

  namespace.I18n = {
    langs: SUPPORTED,
    names: NAMES,
    get lang() { return lang; },
    set,
    toggle: () => set(lang === 'ru' ? 'en' : 'ru'),
    t,
    tr: translate,
    locale,
    plural,
    apply,
    mountSwitcher,
    onChange: (fn) => { listeners.add(fn); return () => listeners.delete(fn); },
  };
})();
