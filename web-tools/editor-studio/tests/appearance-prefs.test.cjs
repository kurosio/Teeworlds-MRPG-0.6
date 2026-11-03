'use strict';

/**
 * Настройки внешнего вида: каталог акцентов, видимые образцы и компактные
 * подписи в списке складов. Тест ловит главную регрессию — образцы без
 * заливки (цвет был виден только после выбора) и «большие пузырьки».
 */

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const ROOT = path.join(__dirname, '..');
const read = (file) => fs.readFileSync(path.join(ROOT, file), 'utf8');

/** Загружает tailwind-theme.js в минимальном окружении браузера. */
const loadPrefs = () => {
  const storage = new Map();
  const localStorage = {
    getItem: (key) => (storage.has(key) ? storage.get(key) : null),
    setItem: (key, value) => storage.set(key, String(value)),
  };
  const root = { setAttribute() {}, hasAttribute() { return false; } };
  const body = { setAttribute() {} };
  const doc = {
    documentElement: root,
    body,
    addEventListener() {},
    querySelectorAll() { return []; },
  };
  const win = {
    localStorage,
    matchMedia: () => ({ matches: false, addEventListener() {} }),
    addEventListener() {},
    dispatchEvent() {},
  };
  const context = vm.createContext({
    window: win,
    document: doc,
    localStorage,
    CustomEvent: class CustomEvent {},
    console,
  });
  vm.runInContext(read('editor-core/tailwind-theme.js'), context);
  return { prefs: win.EditorCore.prefs, storage };
};

test('каталог акцентов: один список для предпочтения и для окна настроек', () => {
  const { prefs, storage } = loadPrefs();
  const ids = ['indigo', 'violet', 'fuchsia', 'rose', 'orange', 'amber', 'emerald', 'teal', 'sky', 'slate'];
  // Массив пришёл из vm-контекста: сравниваем содержимое, а не прототип.
  assert.equal(prefs.accents.join(','), ids.join(','), 'сохранённые значения старых акцентов должны оставаться валидными');
  assert.equal(prefs.accentOptions.length, ids.length);
  for (const option of prefs.accentOptions) {
    assert.ok(option.id && option.label && /^#[0-9a-f]{6}$/i.test(option.swatch), `нет цвета у ${option.id}`);
  }
  assert.equal(prefs.set({ accent: 'emerald' }).accent, 'emerald');
  assert.equal(storage.get('editor-accent'), 'emerald');
  assert.equal(prefs.get().accent, 'emerald', 'новый акцент сохраняется в localStorage');
  assert.equal(prefs.set({ accent: 'not-a-color' }).accent, 'emerald', 'неизвестный акцент не ломает тему');
});

test('все производные цвета редактора считаются от выбранного акцента', () => {
  const css = read('editor-core/editor-theme.css');
  assert.match(css, /--editor-tag-bg:color-mix\(in srgb, var\(--editor-primary\)/, 'теги не следуют за акцентом');
  assert.match(css, /--editor-bg-gradient-1:color-mix\(in srgb, var\(--editor-primary\)/, 'градиент не следует за акцентом');
  assert.match(css, /--studio-accent-dark:#38bdf8/, 'нет пресета sky');
  assert.match(css, /--studio-accent-light:#0369a1/, 'нет светлого значения sky');
  assert.match(css, /:not\(\[data-editor-accent="indigo"\]\)/, 'нет общего правила для нестандартных акцентов');
});

test('образцы акцентов видны до выбора и не перерисовываются галочкой', () => {
  const css = read('editor-core/studio-shell.css');
  assert.ok(!css.includes('[data-accent="indigo"]'), 'остался старый селектор, из-за которого цвета были невидны');
  assert.match(css, /\.studio-swatch-dot\s*\{[\s\S]*?var\(--swatch-color/, 'точка образца не берёт цвет из каталога');
  assert.match(css, /\.studio-swatch\.active \.studio-swatch-check/, 'галочка должна управляться классом active');

  const html = read('index.html');
  assert.ok(html.includes('id="studioAccentSwatches"'), 'нет контейнера для образцов');
  assert.ok(html.includes('renderAccentSwatches'), 'образцы не строятся из каталога');
  assert.ok(!html.includes('data-pref-accent="indigo"'), 'статичные кнопки всё ещё дублируют каталог');
  assert.ok(!html.includes('b.innerHTML = active'), 'внутреннее содержимое образца больше не затирается');
});

test('строка склада: компактные чипы вместо крупных плашек', () => {
  const html = read('warehouse-editor.html');
  assert.ok(html.includes('class="warehouse-row-meta"'), 'нет компактной строки метаданных');
  assert.ok(html.includes('class="warehouse-chip"'), 'чипы не используются');
  assert.ok(!html.includes('<span class="editor-badge">positions:'), 'осталась большая плашка позиций');
  assert.match(html, /\.warehouse-row-meta \.warehouse-chip\{[\s\S]*?font-size:10\.5px/, 'чип не уменьшен');
});

test('новые подписи окна настроек переведены на русский', () => {
  const ru = read('localization/lang_ru.js');
  const keys = [
    'Emerald', 'Fuchsia', 'Orange', 'Rose', 'Sky', 'Slate',
    '{0} accent', 'Live preview', 'Primary button', 'Selected row',
    'Accent is used for buttons, links and selections.',
  ];
  for (const key of keys) {
    assert.ok(ru.includes(`"${key}":`), `нет перевода «${key}»`);
  }
});
