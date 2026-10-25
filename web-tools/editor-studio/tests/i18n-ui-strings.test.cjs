'use strict';

/**
 * Языковая система: английский источник, переводы в `localization/`.
 *
 * Интерфейс написан по-английски — это язык исходников и язык по умолчанию.
 * Русский (и любой другой) язык живёт отдельным файлом `localization/lang_*.js`
 * со словарём «английский текст → перевод». Тест проверяет четыре вещи, без
 * которых система разваливается незаметно:
 *
 *  1. каждый объявленный в манифесте язык действительно зарегистрировался, а
 *     каждый файл `lang_*.js` объявлен в манифесте (иначе язык «невидим»);
 *  2. словарь русского языка не содержит русских ключей, пустых переводов и
 *     «переводов», совпадающих с ключом;
 *  3. число подстановок `{0}`, `{1}` в переводе совпадает с оригиналом —
 *     иначе в интерфейсе появится чужое число;
 *  4. в разметке и коде не осталось русского текста: источник должен быть
 *     английским (единственное исключение — значение из базы).
 */

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const ROOT = path.join(__dirname, '..');
const CORE_DIR = path.join(ROOT, 'editor-core');
const LOCALIZATION_DIR = path.join(ROOT, 'localization');

const CYRILLIC = /[А-Яа-яЁё]/;

/**
 * Значения из базы: остаются английскими в обоих режимах и не переводятся
 * (перечисления предметов, флаги квестов, эффекты мобов). Единственная русская
 * строка — значение поля, которое приходит из данных карты.
 */
const DATA_STRINGS = new Set(['Без точки активности']);

const read = (file) => fs.readFileSync(file, 'utf8');
const normalize = (text) => String(text).replace(/\s+/g, ' ').trim();
const sortCodes = (codes) => [...codes].sort();

/** Загружает реестр и все языки так же, как это делает браузер. */
const loadRegistry = () => {
  const registered = [];
  const context = vm.createContext({
    window: { EditorCoreLocalization: {} },
    console,
  });
  context.window.EditorCoreLocalization.register = (language) => {
    registered.push(language);
  };
  const file = path.join(LOCALIZATION_DIR, 'index.js');
  // index.js сам создаёт registry; перехватываем register после его загрузки.
  vm.runInContext(read(file), context);
  const registry = context.window.EditorCoreLocalization;
  const originalRegister = registry.register;
  registry.register = (language) => {
    registered.push(language);
    return originalRegister(language);
  };
  for (const languageFile of registry.files || []) {
    const target = path.join(LOCALIZATION_DIR, languageFile);
    assert.ok(fs.existsSync(target), `манифест ссылается на отсутствующий файл ${languageFile}`);
    vm.runInContext(read(target), context);
  }
  return { registry, registered };
};

const { registry, registered } = loadRegistry();
const languageFiles = fs.readdirSync(LOCALIZATION_DIR).filter((name) => /^lang_[a-z-]+\.js$/.test(name));

const stripComments = (code) => code
  .replace(/\/\*[\s\S]*?\*\//g, ' ')
  .replace(/(^|[^:\\])\/\/[^\n]*/g, '$1 ');

test('манифест и файлы языков согласованы', () => {
  assert.deepEqual(
    sortCodes(registry.files || []),
    sortCodes(languageFiles),
    'каждый файл localization/lang_*.js должен быть в манифесте и наоборот',
  );
  const declared = registered.map((language) => language.code);
  for (const file of languageFiles) {
    const code = file.replace(/^lang_/, '').replace(/\.js$/, '');
    assert.ok(declared.includes(code), `${file} не зарегистрировал язык «${code}»`);
  }
});

test('английский — язык источника и язык по умолчанию', () => {
  assert.equal(registry.sourceLanguage, 'en');
  assert.equal(registry.defaultLanguage, 'en');
  assert.ok(registry.available().includes('en'), 'английский должен быть в переключателе');
});

test('русский словарь переводит английский текст, а не наоборот', () => {
  const strings = registry.strings('ru');
  assert.ok(strings && Object.keys(strings).length > 1500, 'русский словарь не загрузился');

  const russianKeys = Object.keys(strings).filter((key) => CYRILLIC.test(key));
  assert.deepEqual(russianKeys, [], 'в словаре не должно быть русских ключей');

  const empty = Object.entries(strings).filter(([, value]) => !normalize(value));
  assert.deepEqual(empty, [], 'пустые переводы');

  const identical = Object.entries(strings)
    .filter(([key, value]) => key === value)
    .map(([key]) => key);
  assert.deepEqual(identical, [], 'перевод, совпадающий с оригиналом, только шумит');
});

test('подстановки в переводе совпадают с оригиналом', () => {
  const placeholders = (text) => (String(text).match(/\{\d+\}/g) || []);
  const broken = [];
  for (const [key, value] of Object.entries(registry.strings('ru'))) {
    const expected = placeholders(key);
    const actual = placeholders(value);
    if (actual.length && expected.join() !== actual.join()) broken.push(`${key} → ${value}`);
  }
  assert.deepEqual(broken, [], `порядок или число подстановок не совпадает:\n  ${broken.join('\n  ')}`);
});

test('в разметке и коде страниц не осталось русского текста', () => {
  const pages = fs.readdirSync(ROOT).filter((name) => name.endsWith('.html'));
  const hits = [];
  const check = (text, where) => {
    const value = normalize(text);
    if (value.length < 2 || DATA_STRINGS.has(value) || !CYRILLIC.test(value)) return;
    hits.push(`«${value}» (${where})`);
  };
  for (const page of pages) {
    const html = read(path.join(ROOT, page));
    const withoutComments = html.replace(/<!--[\s\S]*?-->/g, ' ');
    // Стили и скрипты проверяются отдельно: в css-комментариях русский допустим.
    const markup = withoutComments
      .replace(/<style[\s\S]*?<\/style>/gi, ' ')
      .replace(/<script[\s\S]*?<\/script>/gi, ' ');
    for (const match of markup.matchAll(/>([^<>]+)</g)) check(match[1], page);
    for (const attr of ['title', 'placeholder', 'aria-label', 'alt', 'data-tooltip']) {
      const re = new RegExp(`${attr}="([^"]*)"`, 'gi');
      for (const match of withoutComments.matchAll(re)) check(match[1], page);
    }
    for (const match of withoutComments.matchAll(/<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)<\/script>/gi)) {
      for (const literal of stripComments(match[1]).matchAll(/'([^'\\\n]{2,200})'|"([^"\\\n]{2,200})"|`([^`\\]{2,200})`/g)) {
        check(literal[1] || literal[2] || literal[3], page);
      }
    }
  }
  const coreModules = fs.readdirSync(CORE_DIR).filter((name) => name.endsWith('.js'));
  for (const name of coreModules) {
    const code = stripComments(read(path.join(CORE_DIR, name)));
    for (const literal of code.matchAll(/'([^'\\\n]{2,200})'|"([^"\\\n]{2,200})"/g)) {
      check(literal[1] || literal[2], `editor-core/${name}`);
    }
  }
  assert.deepEqual(hits, [], `русский текст в источнике:\n  ${hits.join('\n  ')}`);
});

test('страницы подключают реестр языков, а не отдельный каталог', () => {
  const core = read(path.join(CORE_DIR, 'page-loader.js'));
  assert.ok(core.includes('localization/index.js'), 'page-loader.js не грузит реестр языков');
  assert.ok(core.includes('LANGUAGE_PLACEHOLDER'), 'page-loader.js не разворачивает манифест языков');
  assert.ok(!core.includes('i18n-en.js'), 'старый каталог i18n-en.js больше не нужен');
  const index = read(path.join(ROOT, 'index.html'));
  assert.ok(index.includes('localization/index.js'), 'index.html не грузит реестр языков');
  assert.ok(index.includes('lang="en"'), 'index.html должен объявлять английский язык страницы');
  assert.ok(!fs.existsSync(path.join(CORE_DIR, 'i18n-en.js')), 'editor-core/i18n-en.js должен быть удалён');
});
