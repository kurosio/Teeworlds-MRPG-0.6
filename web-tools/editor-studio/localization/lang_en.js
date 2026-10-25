/**
 * localization/lang_en.js — английский язык (источник интерфейса).
 *
 * Словаря здесь нет и быть не может: разметка страниц и строки в коде уже
 * написаны по-английски, поэтому в режиме `en` движок ничего не переводит.
 * Файл нужен ради остальных полей: подпись в переключателе, локаль для дат,
 * формы числительных и место в порядке языков.
 */
window.EditorCoreLocalization.register({
  code: 'en',
  name: 'English',
  short: 'EN',
  locale: 'en-US',
  order: 0,
  // Английские числительные: 1 line / 2 lines. Формы приходят из вызова
  // `I18n.plural(n, 'line', 'lines')`, поэтому таблица пустая.
  plurals: {},
  pluralIndex(count) {
    return Math.abs(Number(count) || 0) === 1 ? 0 : 1;
  },
  strings: {},
});
