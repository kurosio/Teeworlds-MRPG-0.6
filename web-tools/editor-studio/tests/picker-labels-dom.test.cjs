/**
 * Подписи панели карты — то, что видел пользователь: «Кости казино» на месте
 * спавна мобов. Здесь проверяется вся цепочка целиком в настоящем DOM:
 *   highlightTiles редактора → каталог (key) → словарь панели (ru/en).
 *
 * Нужен jsdom (`npm install --no-save jsdom`, он же в devDependencies).
 */
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { JSDOM } = require('jsdom');

const root = path.join(__dirname, '..');
const read = (file) => fs.readFileSync(path.join(root, 'vendor/ddnet-map', file), 'utf8');

/** Контекст окна панели с настоящим DOM и загруженными модулями. */
const loadPanel = (search = 'map=/map&x=16&y=16') => {
  const dom = new JSDOM('<html><body></body></html>', {
    url: `https://editor.test/vendor/ddnet-map/picker.html?${search}`,
  });
  const context = vm.createContext({
    window: dom.window,
    document: dom.window.document,
    console,
    URLSearchParams,
    localStorage: { getItem: () => null, setItem() {} },
    getComputedStyle: () => ({ getPropertyValue: () => '#6f8cff' }),
  });
  for (const file of ['picker-text.js', 'picker-tile-catalog.js']) {
    vm.runInContext(read(file), context);
  }
  const catalog = vm.runInContext('window.MapPickerTilesCatalog', context);
  return { catalog, textFor: (lang) => vm.runInContext(`window.MapPickerText.get('${lang}')`, context) };
};

/** Ровно то, что строит picker-workbench.js для <option>. */
const optionLabel = (catalog, names, layer, id) => {
  const key = catalog.label(layer, id);
  return `${id} · ${names[key] || key}`;
};

test('mob spawn reads as «Спавн мобов · 209», casino tiles keep their own ids', () => {
  const { catalog, textFor } = loadPanel();
  const ru = textFor('ru').tileNames;
  assert.equal(optionLabel(catalog, ru, 'game', 209), '209 · Спавн мобов');
  assert.equal(optionLabel(catalog, ru, 'game', 208), '208 · Спавн игрока');
  assert.equal(optionLabel(catalog, ru, 'game', 211), '211 · Спавн тюрьмы');
  // Настоящие казино-тайлы: раньше они были сдвинуты на 209/210.
  assert.equal(optionLabel(catalog, ru, 'game', 242), '242 · Кости казино');
  assert.equal(optionLabel(catalog, ru, 'game', 243), '243 · Рулетка казино');
  assert.equal(optionLabel(catalog, ru, 'game', 192), '192 · Броня');
  // 210 в mapitems.h нет: он не должен выдавать себя за тайл казино.
  assert.equal(optionLabel(catalog, ru, 'game', 210), '210 · Тайл');
});

test('the English panel tells the same truth as the Russian one', () => {
  const { catalog, textFor } = loadPanel();
  const en = textFor('en').tileNames;
  assert.equal(optionLabel(catalog, en, 'game', 209), '209 · Mob spawn');
  assert.equal(optionLabel(catalog, en, 'game', 242), '242 · Casino dice');
  assert.equal(optionLabel(catalog, en, 'game', 243), '243 · Casino roulette');
  assert.equal(optionLabel(catalog, en, 'switch', 200), '200 · Ore');
});

test('the tile editor offers exactly the declared ids of the declared layer', () => {
  const { catalog } = loadPanel('highlightTiles=209&highlightLayer=game');
  // Тот же фильтр, что в picker-workbench.js: правка только 4..255.
  const declared = [209].filter((id) => catalog.isEditable(id));
  assert.deepEqual(declared, [209]);
  assert.equal(catalog.isEntity(209), true);
  assert.equal(catalog.isSwitchTile(209), false, '209 — игровая сущность, не switch-тип');
  // Стены/смерть/без крюка: читать можно, править нельзя.
  for (const id of [1, 2, 3]) assert.equal(catalog.isEditable(id), false);
});
