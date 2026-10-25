/**
 * localization/index.js — реестр языков интерфейса.
 *
 * Источник строк интерфейса — английский: именно на нём написаны разметка и код
 * страниц. Любой другой язык — это файл `lang_<код>.js` в этой папке, который
 * регистрирует себя через `window.EditorCoreLocalization.register({...})` и
 * содержит словарь «английский текст → перевод».
 *
 * Чтобы добавить язык:
 *   1. положите рядом `lang_de.js` с вызовом `register({ code: 'de', ... })`;
 *   2. добавьте имя файла в массив `files` ниже (это единственное место,
 *      которое браузер не может прочитать сам — список файлов папки).
 * Всё остальное сделает `editor-core/page-loader.js`: он подключит файлы из
 * `files`, а переключатель языка в шапке построится по зарегистрированным языкам.
 *
 * Структура описания языка:
 *   code        — 'ru'
 *   name        — 'Русский'            (подпись в переключателе)
 *   short       — 'RU'                 (короткая подпись на кнопке)
 *   locale      — 'ru-RU'              (для дат и чисел)
 *   strings     — { 'Save': 'Сохранить', ... }
 *   plurals     — { line: ['строка', 'строки', 'строк'], ... }
 *                 ключ — английская форма единственного числа из вызова
 *                 `I18n.plural(n, 'line', 'lines')`
 *   pluralIndex — необязательная функция выбора формы по числу
 */
(() => {
  const registry = (window.EditorCoreLocalization = window.EditorCoreLocalization || {});

  // Язык исходников интерфейса. Его словарь не нужен: строки уже английские.
  registry.sourceLanguage = registry.sourceLanguage || 'en';
  // Язык по умолчанию для нового пользователя.
  registry.defaultLanguage = registry.defaultLanguage || 'en';
  // Порядок кнопок в переключателе: сначала язык по умолчанию.
  registry.order = Array.isArray(registry.order) ? registry.order : ['en', 'ru'];
  registry.languages = registry.languages || {};
  // Манифест папки: какие файлы языков подключать (см. выше).
  registry.files = Array.isArray(registry.files) && registry.files.length
    ? registry.files
    : ['lang_en.js', 'lang_ru.js'];

  /**
   * Регистрация языка. Файл `lang_*.js` вызывает эту функцию на загрузке.
   * Неверное описание не ломает интерфейс: язык просто не появится в списке.
   */
  registry.register = (language) => {
    if (!language || typeof language !== 'object') return null;
    const code = String(language.code || '').trim().toLowerCase();
    if (!/^[a-z]{2}(?:-[a-z]{2})?$/.test(code)) {
      console.warn('[editor-studio][i18n] язык без корректного кода пропущен:', language);
      return null;
    }
    registry.languages[code] = {
      code,
      name: String(language.name || code.toUpperCase()),
      short: String(language.short || code.toUpperCase()),
      locale: String(language.locale || code),
      strings: language.strings && typeof language.strings === 'object' ? language.strings : {},
      plurals: language.plurals && typeof language.plurals === 'object' ? language.plurals : {},
      pluralIndex: typeof language.pluralIndex === 'function' ? language.pluralIndex : null,
      order: Number.isFinite(language.order) ? language.order : null,
    };
    if (!registry.order.includes(code)) registry.order.push(code);
    return code;
  };

  /** Зарегистрированные языки в порядке переключателя. */
  registry.available = () => {
    const codes = Object.keys(registry.languages);
    const rank = (code) => {
      const language = registry.languages[code];
      const declared = language?.order;
      if (Number.isFinite(declared)) return declared;
      const index = registry.order.indexOf(code);
      return index === -1 ? 1000 : index;
    };
    return codes.sort((a, b) => rank(a) - rank(b) || a.localeCompare(b));
  };

  /** Описание языка или `null`. */
  registry.language = (code) => registry.languages[String(code || '').toLowerCase()] || null;

  /** Словарь «английский текст → перевод»; у языка-источника его нет. */
  registry.strings = (code) => {
    const language = registry.language(code);
    if (!language || language.code === registry.sourceLanguage) return null;
    return Object.keys(language.strings).length ? language.strings : null;
  };
})();
