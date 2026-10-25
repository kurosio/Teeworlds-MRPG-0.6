/**
 * editor-core/i18n.js — переключение языка интерфейса.
 *
 * Источник строк — сам интерфейс: разметка и код страниц написаны по-английски.
 * Другие языки лежат в каталоге `localization/`: файл `lang_<код>.js` вызывает
 * `EditorCoreLocalization.register({ code, name, locale, strings, plurals })` и
 * содержит словарь «английский текст → перевод». Список файлов — манифест
 * `localization/index.js`, язык по умолчанию — тоже там.
 *
 * Почему перевод применяется к готовому DOM, а не к размеченным строкам:
 *
 *  • не нужно размечать тысячи мест ключами — переводится любой узел DOM,
 *    включая тот, что страница отрисовала уже после загрузки (наблюдатель
 *    за изменениями переводит и его);
 *  • если перевода нет, интерфейс остаётся рабочим: показывается английская
 *    строка, а не «пустое» место или ключ вида `nav.items`;
 *  • язык-источник (`en`) вообще ничего не переводит — это и есть исходный
 *    текст, поэтому английский режим работает без единого словаря.
 *
 * Ключи словаря могут содержать подстановки `{0}`, `{1}` — они превращаются
 * в шаблоны и совпадают с уже отрисованным текстом («Found: 12»). Это
 * позволяет переводить сообщения со счётчиками, не правя вызывающий код.
 *
 * Использование:
 *   EditorCore.I18n.t('Found: {0}', [12])     // → 'Найдено: 12' при lang = 'ru'
 *   EditorCore.I18n.set('ru')                 // переключить язык
 *   EditorCore.I18n.available()               // зарегистрированные языки
 *   EditorCore.I18n.onChange((lang) => {})    // реакция на смену языка
 */
(() => {
  // Повторное подключение файла (например, при ручной вставке тега) не должно
  // создавать второй наблюдатель за DOM и второй набор обратных замен.
  if (window.__editorCoreI18n) return;
  window.__editorCoreI18n = true;

  const LANG_KEY = 'editorStudio.lang';
  // Реестр языков лежит в `localization/index.js`; он подключается раньше.
  const registry = () => window.EditorCoreLocalization || null;
  const sourceLanguage = () => registry()?.sourceLanguage || 'en';
  const available = () => registry()?.available?.() || ['en'];
  const languageOf = (code) => registry()?.language?.(code) || null;
  const langName = (code) => languageOf(code)?.name || String(code).toUpperCase();
  const langShort = (code) => languageOf(code)?.short || String(code).toUpperCase();
  const LATIN = /[A-Za-z]/;
  const TEXT_ATTRS = ['title', 'placeholder', 'aria-label', 'alt', 'data-tooltip'];
  const SKIP_TAGS = new Set(['SCRIPT', 'STYLE', 'CODE', 'PRE', 'KBD', 'SAMP', 'TEXTAREA']);

  const namespace = (window.EditorCore = window.EditorCore || {});

  /**
   * Язык по умолчанию — английский (язык исходников). Сохранённый выбор
   * уважаем, но только если такой язык всё ещё зарегистрирован: файл языка
   * могли убрать из `localization/`.
   */
  const readLang = () => {
    try {
      const stored = window.localStorage?.getItem(LANG_KEY);
      if (stored && available().includes(stored)) return stored;
    } catch { /* приватный режим — работаем без сохранения */ }
    const fallback = registry()?.defaultLanguage || sourceLanguage();
    return available().includes(fallback) ? fallback : sourceLanguage();
  };

  let lang = readLang();
  const listeners = new Set();
  /** Обратные замены: чтобы вернуть русский текст без перезагрузки страницы. */
  let applying = false;
  let observer = null;
  const patternCache = new Map();

  const dictFor = (code) => registry()?.strings?.(code) || null;
  const normalize = (text) => String(text).replace(/\s+/g, ' ').trim();
  const escapeRe = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

  // Служебные слова: шаблон, в котором нет ничего, кроме предлога, совпадёт с
  // любой фразой («{0} of {1}» ломает «Remove the request from the history») —
  // такие ключи не используются как шаблоны.
  const WEAK_LITERALS = new Set([
    'of', 'in', 'on', 'at', 'to', 'into', 'from', 'by', 'for', 'with', 'without',
    'and', 'or', 'but', 'the', 'a', 'an', 'is', 'are', 'was', 'were', 'be', 'not',
    'no', 'yes', 'as', 'if', 'so', 'then', 'than', 'that', 'this', 'it',
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
      bucket.push({ re: new RegExp(source, 'u'), segments, key, weight: literalWeight(key) });
    }
  };

  // Разделители, которыми страницы склеивают подписи из готовых кусков:
  // «Мир #3 · покупка», «Гость — онлайн». Такой узел целиком в каталоге не
  // встречается, поэтому его переводим по частям.
  const COMPOSITE_SEPARATORS = [' · ', ' | ', ' — ', ' – '];
  // Символы, которые подстановка «заглатывать» не должна: иначе короткий ключ
  // «Мир #{0}» совпал бы с целой строкой «Мир #3 · покупка» и съел её хвост.
  const GUARD_CHARS = ['·', '|', '—', '–'];

  /** Перевод подставленного значения: «Подключение 2» внутри сообщения. */
  const translateValue = (value, depth) => {
    if (typeof value !== 'string' || !LATIN.test(value)) return value;
    const core = normalize(value);
    if (!core) return value;
    const lead = value.match(/^\s*/)[0];
    const tail = value.match(/\s*$/)[0];
    const next = translateCore(core, depth + 1);
    return next === core ? value : lead + next + tail;
  };

  /**
   * Подходит ли совпадение шаблона: подстановка не должна съедать соседнюю
   * часть составной подписи. Проверяем именно разделитель с пробелами
   * (« · »), иначе шаблон «Мир: {0}» отбрасывался бы на строке «Мир: —»,
   * где подстановка — просто прочерк вместо пустого значения.
   */
  const capturesOk = (match, key) => {
    for (let index = 1; index < match.length; index += 1) {
      const capture = match[index] || '';
      for (const char of GUARD_CHARS) {
        if (key.includes(char)) continue;
        // escapeRe: «|» иначе стал бы альтернацией, и подстановка «Подключение 2»
        // отбрасывалась бы как разделитель.
        if (new RegExp(`\\s${escapeRe(char)}\\s`).test(capture)) return false;
      }
    }
    return true;
  };

  /**
   * Перевод произвольной строки: точное совпадение, затем шаблоны с
   * подстановками, затем — составные подписи по частям.
   *
   * `depth` ограничивает дробление: «{0} · {1}» не должно бесконечно
   * раскладываться на части, если ни одна из них не переводится.
   */
  const translateCore = (core, depth = 0) => {
    if (!core) return core;
    const dict = dictFor(lang);
    if (!dict) return core;
    const exact = dict[core];
    if (typeof exact === 'string') return exact;
    if (patternCache.size === 0) buildPatterns();
    const head = core.split(' ')[0];
    const candidates = [patternCache.get(head), patternCache.get('*')];
    // Первый подошедший шаблон — не всегда самый точный: общий «{0} · {1}»
    // перехватил бы «ID 5 · Мир: Лес», оставив половину строки русской.
    // Поэтому выбираем шаблон, который оставляет подстановкам меньше всего
    // текста, то есть покрывает больше собственных слов ключа.
    let best = null;
    for (const bucket of candidates) {
      if (!bucket) continue;
      for (const { re, segments, key } of bucket) {
        const match = core.match(re);
        if (!match) continue;
        if (!capturesOk(match, key)) continue;
        let capturedLength = 0;
        for (let index = 1; index < match.length; index += 1) capturedLength += (match[index] || '').length;
        if (!best || capturedLength < best.capturedLength) best = { match, segments, capturedLength };
      }
    }
    if (best) {
      return best.segments.map((segment) => {
        if (typeof segment === 'string') return segment;
        // Подставленное значение тоже может быть текстом интерфейса
        // (например, вложенное сообщение сервера или имя профиля) —
        // переводим и его, сохраняя пробелы вокруг.
        return translateValue(best.match[segment + 1] || '', depth);
      }).join('');
    }
    if (depth < 2) {
      for (const separator of COMPOSITE_SEPARATORS) {
        if (!core.includes(separator)) continue;
        let changed = false;
        const parts = core.split(separator).map((part) => {
          const trimmed = part.trim();
          if (!trimmed) return part;
          const next = translateCore(trimmed, depth + 1);
          if (next !== trimmed) changed = true;
          const lead = part.match(/^\s*/)[0];
          const tail = part.match(/\s*$/)[0];
          return lead + next + tail;
        });
        if (changed) return parts.join(separator);
      }
    }
    return core;
  };

  /** Перевод строки целиком (с сохранением исходных пробелов по краям). */
  const translate = (text) => {
    const raw = String(text ?? '');
    if (!raw) return raw;
    const dict = dictFor(lang);
    if (!dict) return raw;
    const core = normalize(raw);
    if (!core) return raw;
    const next = translateCore(core);
    return next === core ? raw : next;
  };

  /**
   * Запоминание исходного текста — по узлу и атрибуту, а не списком.
   *
   * Раньше это был массив с обрезкой «самое старое выкидываем», и на больших
   * страницах (тысячи переведённых узлов) из него выпадали первые записи:
   * заголовок вкладки, шапка справки, палитра команд. После возврата на
   * русский они оставались английскими — «переведено не всё». Ключ узла
   * (Map) решает это: повторный перевод того же узла обновляет значение,
   * а не добавляет запись, и ничего не теряется.
   */
  const store = new Map(); // node -> { text?: original, attrs: Map<attr, original> }
  let titleOriginal = null;

  const slotFor = (node) => {
    let slot = store.get(node);
    if (!slot) { slot = { attrs: new Map() }; store.set(node, slot); }
    return slot;
  };

  const rememberText = (node, original) => { slotFor(node).text = original; };
  const rememberAttr = (node, attr, original) => { slotFor(node).attrs.set(attr, original); };
  const rememberTitle = (original) => { titleOriginal = original; };

  const translateTextNode = (node) => {
    const raw = node.nodeValue;
    // Источник интерфейса — английский: узлы без латиницы переводить нечего
    // (числа, прочерки, иконки), а это самый частый случай на странице.
    if (!raw || !LATIN.test(raw)) return;
    const match = raw.match(/^(\s*)([\s\S]*?)(\s*)$/);
    if (!match) return;
    const [, lead, core, tail] = match;
    if (!core) return;
    const next = translate(core);
    if (next === core) return;
    rememberText(node, raw);
    node.nodeValue = lead + next + tail;
  };

  const translateAttr = (el, attr) => {
    if (!el || !el.getAttribute) return;
    const value = el.getAttribute(attr);
    if (!value || !LATIN.test(value)) return;
    const next = translate(value);
    if (next === value) return;
    rememberAttr(el, attr, value);
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
    if (titleOriginal !== null) {
      document.title = titleOriginal;
      titleOriginal = null;
    }
    for (const [node, slot] of store) {
      // Узлы, которые страница уже выбросила из документа, не трогаем —
      // иначе «русским» станет текст, которого никто не видит.
      if (!node || node.isConnected === false) continue;
      if (typeof slot.text === 'string' && node.nodeType === 3) node.nodeValue = slot.text;
      for (const [attr, original] of slot.attrs) {
        if (node.getAttribute?.(attr) !== null) node.setAttribute(attr, original);
      }
    }
    store.clear();
  };

  /** Переводит поддерево (или весь документ). Русский режим — без изменений. */
  const apply = (root = document) => {
    if (!root || !dictFor(lang)) return;
    applying = true;
    try {
      if (root === document) {
        if (document.title) {
          const next = translate(document.title);
          if (next !== document.title) {
            rememberTitle(document.title);
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
    box.setAttribute('aria-label', t('Interface language'));
    for (const code of available()) {
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.dataset.lang = code;
      btn.textContent = langShort(code);
      btn.title = langName(code);
      btn.setAttribute('aria-label', langName(code));
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
      // Источник — английский: переводить нужно только когда у языка есть словарь.
      if (applying || !dictFor(lang)) return;
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
   * Согласование числительных.
   *
   * Источник — английские формы из вызова: `plural(n, 'line', 'lines')`.
   * Русские формы (1 строка / 2 строки / 5 строк) лежат в `localization/lang_ru.js`
   * в таблице `plurals` под ключом английского единственного числа, а правило
   * выбора формы задаёт `pluralIndex` этого языка. Язык без таблицы получает
   * обычное английское правило: one / other.
   */
  const plural = (n, one, many) => {
    const count = Math.abs(Number(n) || 0);
    const language = languageOf(lang);
    const forms = language?.plurals?.[one];
    if (Array.isArray(forms) && forms.length) {
      const index = typeof language.pluralIndex === 'function' ? language.pluralIndex(count) : (count === 1 ? 0 : 1);
      return forms[Math.min(Math.max(index, 0), forms.length - 1)];
    }
    return count === 1 ? one : many;
  };

  const set = (code, { force = false } = {}) => {
    const next = available().includes(code) ? code : sourceLanguage();
    const changed = next !== lang;
    if (!changed && !force) return lang;
    // Перед сменой языка возвращаем исходный (английский) текст: иначе
    // следующий словарь получил бы уже переведённые строки.
    applying = true;
    try { restore(); } finally { applying = false; }
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
    // Подставленные значения тоже переводим: `t('New profile “{0}”', [name])`,
    // где name собрано кодом страницы, иначе в русском интерфейсе осталось бы
    // английское имя внутри русской фразы.
    const fill = (template) => String(template).replace(/\{(\d+)\}/g, (whole, index) => {
      const value = list[Number(index)];
      if (value === undefined || value === null) return whole;
      if (typeof value !== 'string') return String(value);
      return translateValue(value, 1);
    });
    if (!dictFor(lang)) return fill(raw);
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
        if (dictFor(lang)) {
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
    apply(document);
    observe();
    // Язык сменили в другом документе (шапка студии, соседняя вкладка).
    window.addEventListener('storage', (event) => {
      if (event.key !== LANG_KEY || !event.newValue || event.newValue === lang) return;
      if (available().includes(event.newValue)) set(event.newValue);
    });
  };

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
  } else {
    init();
  }

  namespace.I18n = {
    get langs() { return available(); },
    get names() {
      const names = {};
      for (const code of available()) names[code] = langName(code);
      return names;
    },
    available,
    sourceLanguage: sourceLanguage(),
    get lang() { return lang; },
    set,
    toggle: () => {
      const codes = available();
      const index = codes.indexOf(lang);
      return set(codes[(index + 1) % codes.length] || sourceLanguage());
    },
    t,
    tr: translate,
    locale,
    plural,
    apply,
    mountSwitcher,
    onChange: (fn) => { listeners.add(fn); return () => listeners.delete(fn); },
  };
})();
