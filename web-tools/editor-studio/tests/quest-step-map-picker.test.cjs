const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const ROOT = path.resolve(__dirname, '..');
const read = (file) => fs.readFileSync(path.join(ROOT, file), 'utf8');

const escapeHtml = (value) => String(value ?? '')
  .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
  .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
const tr = (text, vars) => String(text ?? '').replace(/\{(\d+)\}/g, (m, i) => (Array.isArray(vars) ? vars[Number(i)] : vars) ?? m);

/** Минимальная реализация общего модуля миров для изолированных тестов. */
const mapWorlds = (dbRows = []) => {
  const parseWorldId = (value) => {
    if (value === null || value === undefined || String(value).trim() === '') return null;
    const n = Number(value);
    return Number.isSafeInteger(n) && n >= 0 ? n : null;
  };
  let memory = null;
  return {
    STORAGE_KEY: 'editor-map-world',
    parseWorldId,
    rememberedWorld: () => memory,
    rememberWorld: (value) => { memory = parseWorldId(value); },
    list: async () => dbRows.map((row) => ({ id: parseWorldId(row.ID ?? row.id), name: String(row.Name ?? row.name ?? '') }))
      .filter((row) => row.id !== null),
    name: (worlds, id) => String((worlds || []).find((w) => w.id === parseWorldId(id))?.name || ''),
  };
};

/** Загрузить модуль в минимальном VM и получить чистый API без DOM-тестов. */
const loadModule = () => {
  const listeners = [];
  const document = {
    readyState: 'complete',
    documentElement: {},
    addEventListener: (...args) => listeners.push(args),
    querySelectorAll: () => [],
    createElement: () => ({ innerHTML: '', content: { firstElementChild: null } }),
  };
  const context = {
    console,
    Event: class {}, CustomEvent: class {}, Element: class {},
    MutationObserver: class { observe() {} },
    setTimeout,
    localStorage: { getItem: () => null, setItem: () => {} },
    document,
  };
  context.window = context;
  context.window.EditorCore = {
    utils: { escapeHtml },
    Page: { t: tr },
    UI: { toast: () => {} },
    DBCrud: { list: async () => ({ rows: [] }) },
    MapWorlds: mapWorlds(),
    localStorage: { getItem: () => null, setItem: () => {} },
    setTimeout: (fn) => setTimeout(fn, 0),
    addEventListener: () => {},
  };
  context.window.EditorCore.I18n = { onChange: () => {} };
  vm.createContext(context);
  vm.runInContext(read('editor-core/map-worlds.js'), context, { filename: 'map-worlds.js' });
  vm.runInContext(read('editor-core/position-picker.js'), context, { filename: 'position-picker.js' });
  return context.window.EditorCore.PositionPicker;
};

test('PositionPicker is wired into the editor core and every quest position', () => {
  const loader = read('editor-core/page-loader.js');
  const field = read('editor-core/field-renderer.js');
  const quests = read('quests-editor.html');
  const pointMap = read('editor-core/point-map.js');
  const positionPicker = read('editor-core/position-picker.js');
  const en = read('editor-core/i18n-en.js');

  assert.match(loader, /editor-core\/position-picker\.js/);
  assert.match(loader, /editor-core\/position-picker\.css/);
  assert.match(loader, /editor-core\/map-worlds\.js/);
  assert.match(field, /PositionPicker\.render/);
  assert.match(field, /safeTiles: ui\.safeTiles/);
  assert.match(field, /showTiles: ui\.showTiles/);
  assert.match(field, /grid: ui\.grid/);
  assert.match(read('editor-core/form-runtime.js'), /PositionPicker\?\.scan/);
  assert.match(read('scenario-editor.html'), /PositionPicker\?\.scan/);
  assert.match(read('editor-core/registry.js'), /mapWorldPath: 'world_id'/);
  assert.match(pointMap, /editor-map:applied/);
  assert.match(pointMap, /editor-map:world/);
  assert.match(pointMap, /editor-map:closed/);
  assert.match(pointMap, /editor-map:validity/);
  assert.match(pointMap, /params\.set\('safe'/);
  assert.doesNotMatch(pointMap, /data-map-pick/);
  assert.match(positionPicker, /PointMap\.open/);
  assert.match(positionPicker, /data-map-options/);
  assert.match(read('vendor/ddnet-map/picker.js'), /editor-map:validity/);
  assert.match(read('vendor/ddnet-map/picker-overlays.js'), /blockedTile/);
  assert.match(read('vendor/ddnet-map/picker.html'), /pk-validity/);
  assert.match(quests, /PositionPicker\.render/);
  assert.match(quests, /PositionPicker\.openMap/);
  assert.match(quests, /data-step-action="pin"/);
  assert.match(quests, /data-map-picker="quest-step-position"/);
  assert.match(quests, /data-position-picker-host="quest-tasks"/);
  assert.match(quests, /mapPickerTag: 'quest-step-position'/);
  assert.match(quests, /safeTiles: true/);
  assert.doesNotMatch(quests, /class="editor-vec2"/);
  assert.match(en, /"Ввести вручную":/);
  assert.match(en, /"Мир сохраняется в записи":/);
});

test('all position editors declare the map flags they need', () => {
  for (const [page, token] of [
    ['mobs-editor.html', 'safeTiles: false'],
    ['mobs-editor.html', 'highlightTiles: [209]'],
    ['mobs-editor.html', "radiusPath: 'Radius'"],
    ['bots-npc-editor.html', 'safeTiles: true'],
    ['quests-editor.html', 'safeTiles: true'],
    ['aethers-editor.html', 'showTiles: true'],
    ['dungeons-editor.html', 'showTiles: true'],
    ['warehouse-editor.html', 'showTiles: true'],
  ]) {
    assert.ok(read(page).includes(token), `${page} should declare ${token}`);
  }
});

test('PositionPicker renders one card with map flags and manual entry', () => {
  const api = loadModule();
  assert.equal(api.parseWorldId(0), 0);
  assert.equal(api.parseWorldId(''), null);
  assert.equal(api.parseWorldId('-1'), null);
  assert.deepEqual(Array.from(api.DEFAULT_BLOCKED_TILES), [1, 2, 3]);

  const html = api.render({
    label: 'Позиция на карте',
    xPath: 'Position.x',
    yPath: 'Position.y',
    xInputHtml: '<input type="number" data-position-axis="x" data-map-axis="x" data-path="Position.x" value="10" hidden>',
    yInputHtml: '<input type="number" data-position-axis="y" data-map-axis="y" data-path="Position.y" value="20" hidden>',
    xFieldPath: 'Position.x',
    yFieldPath: 'Position.y',
    worldPath: 'WorldID',
    worldId: 1,
    pinned: true,
    step: 1,
    safeTiles: true,
    highlightTiles: [209],
    radiusPath: 'Radius',
    grid: true,
    snap: true,
  });
  assert.match(html, /data-position-picker="1"/);
  assert.match(html, /data-map-picker="position"/);
  assert.match(html, /data-map-x-path="Position\.x"/);
  assert.match(html, /data-map-world-path="WorldID"/);
  assert.match(html, /data-world-id="1"/);
  assert.match(html, /data-map-pick="1"/);
  assert.match(html, /data-position-action="manual"/);
  assert.match(html, /data-position-world-select/);
  assert.match(html, /data-position-manual-axis="x"/);
  assert.match(html, /data-position-manual-axis="y"/);
  assert.match(html, /data-field-path="Position\.x"/);
  assert.match(html, /class="position-picker is-pinned"/);
  // Флаги карты сериализуются в объявление элемента.
  assert.match(html, /data-map-options="[^"]*&quot;safe&quot;:\[1,2,3\]/);
  assert.match(html, /&quot;highlightTiles&quot;:\[209\]/);
  assert.match(html, /&quot;radiusPath&quot;:&quot;Radius&quot;/);
  for (const flag of ['safe', 'highlightTiles', 'radius', 'grid', 'snap']) {
    assert.match(html, new RegExp(`data-position-flag="${flag}"`));
  }
  // Эти оверлеи больше не включают полный игровой слой неявно.
  assert.match(html, /&quot;tiles&quot;:false/);
});

test('vec2 gameplay and grid defaults are explicit and independent of other overlays', () => {
  const api = loadModule();
  const rootWith = (options) => ({
    getAttribute: (name) => name === 'data-map-options' ? JSON.stringify(options) : null,
  });

  const plain = api.resolveMapOptions(rootWith({}));
  assert.equal(plain.tiles, false);
  assert.equal(plain.grid, null); // picker трактует отсутствие настройки как выключено

  const overlaysOnly = api.resolveMapOptions(rootWith({ safeTiles: true, highlightTiles: [209], radius: 96 }));
  assert.equal(overlaysOnly.tiles, false);
  assert.equal(overlaysOnly.grid, null);
  assert.deepEqual(Array.from(overlaysOnly.highlightTiles), [209]);
  assert.equal(overlaysOnly.radius, 96);

  const explicit = api.resolveMapOptions(rootWith({ showTiles: true, grid: true }));
  assert.equal(explicit.tiles, true);
  assert.equal(explicit.grid, true);
});

let JSDOM = null;
try { ({ JSDOM } = require('jsdom')); } catch { JSDOM = null; }
const domSkip = JSDOM ? false : 'jsdom не установлен (npm install --no-save jsdom)';

test('PositionPicker saves manual coordinates, world and map result', { skip: domSkip }, async () => {
  const dom = new JSDOM('<!doctype html><html><body>'
    + '<input id="world" data-path="WorldID" value="1">'
    + '<input id="radius" data-path="Radius" value="256"></body></html>', {
    runScripts: 'outside-only', pretendToBeVisual: true, url: 'https://editor.test/editor.html',
  });
  const { window } = dom;
  let opened = null;
  let openedOptions = null;
  window.EditorCore = {
    utils: { escapeHtml },
    Page: { t: tr },
    UI: { toast: () => {} },
    DBCrud: { list: async () => ({ rows: [{ ID: 0, Name: 'Zero' }, { ID: 1, Name: 'One' }, { ID: 3, Name: 'Three' }] }) },
    MapWorlds: mapWorlds([{ ID: 0, Name: 'Zero' }, { ID: 1, Name: 'One' }, { ID: 3, Name: 'Three' }]),
    PointMap: { open: (root, options) => { opened = root; openedOptions = options; } },
    I18n: { onChange: () => {} },
  };
  window.eval(read('editor-core/map-worlds.js'));
  window.eval(read('editor-core/position-picker.js'));
  const api = window.EditorCore.PositionPicker;

  const picker = api.create({
    label: 'Позиция на карте',
    xPath: 'Position.x',
    yPath: 'Position.y',
    xInputHtml: '<input type="number" data-position-axis="x" data-map-axis="x" data-path="Position.x" data-value-type="number" value="10" hidden>',
    yInputHtml: '<input type="number" data-position-axis="y" data-map-axis="y" data-path="Position.y" data-value-type="number" value="20" hidden>',
    xFieldPath: 'Position.x',
    yFieldPath: 'Position.y',
    worldPath: 'WorldID',
    worldId: 1,
    pinned: true,
    step: 1,
    safeTiles: true,
    highlightTiles: [209],
    radiusPath: 'Radius',
    grid: true,
    snap: true,
  });
  window.document.body.appendChild(picker);
  api.scan(window.document);
  await new Promise((resolve) => setTimeout(resolve, 20));

  const worldSelect = picker.querySelector('[data-position-world-select]');
  assert.equal(worldSelect.value, '1');
  assert.match(picker.querySelector('[data-position-state]').textContent, /X 10/);

  // Ручной ввод не меняет скрытые поля до «Применить».
  picker.querySelector('[data-position-action="manual"]').click();
  const manualPanel = picker.querySelector('[data-position-manual]');
  assert.equal(manualPanel.hidden, false);
  manualPanel.querySelector('[data-position-manual-axis="x"]').value = '123';
  manualPanel.querySelector('[data-position-manual-axis="y"]').value = '-45';
  assert.equal(picker.querySelector('[data-position-axis="x"]').value, '10');
  picker.querySelector('[data-position-action="manual-apply"]').click();
  assert.equal(picker.querySelector('[data-position-axis="x"]').value, '123');
  assert.equal(picker.querySelector('[data-position-axis="y"]').value, '-45');
  assert.equal(manualPanel.hidden, true);
  assert.equal(picker.dataset.positionPinned, '1');

  // Выбор мира в подложке сохраняется в связанное поле формы.
  worldSelect.value = '3';
  worldSelect.dispatchEvent(new window.Event('change', { bubbles: true }));
  assert.equal(window.document.getElementById('world').value, '3');
  assert.equal(picker.getAttribute('data-world-id'), '3');

  // Кнопка карты — единственный путь открытия: флаги и мир уходят в PointMap.
  picker.querySelector('[data-position-action="map"]').click();
  assert.equal(opened, picker);
  assert.equal(openedOptions.worldId, 3);
  assert.equal(openedOptions.allowWorldChange, false);
  assert.deepEqual(Array.from(openedOptions.mapOptions.safe), [1, 2, 3]);
  assert.deepEqual(Array.from(openedOptions.mapOptions.highlightTiles), [209]);
  assert.equal(openedOptions.mapOptions.tiles, false);
  assert.equal(openedOptions.mapOptions.radius, 256);
  assert.equal(openedOptions.mapOptions.grid, true);
  assert.equal(openedOptions.mapOptions.snap, true);
  assert.deepEqual(Array.from(api.getMapOptions(picker).safe), [1, 2, 3]);

  api.openMap(picker, { worldId: 3 });
  assert.equal(opened, picker);
  picker.dispatchEvent(new window.CustomEvent('editor-map:applied', {
    detail: { x: 7, y: 8, worldId: 3 }, bubbles: true,
  }));
  assert.equal(picker.dataset.positionPinned, '1');
  assert.equal(api.getWorldId(picker), 3);

  // Предварительный выбор мира в карте не сохраняется при отмене.
  picker.dispatchEvent(new window.CustomEvent('editor-map:world', {
    detail: { worldId: 0, provisional: true }, bubbles: true,
  }));
  assert.equal(picker.getAttribute('data-world-preview'), '0');
  assert.equal(window.document.getElementById('world').value, '3');
  picker.dispatchEvent(new window.CustomEvent('editor-map:closed', {
    detail: { reason: 'cancel' }, bubbles: true,
  }));
  assert.equal(picker.hasAttribute('data-world-preview'), false);
  assert.equal(api.getWorldId(picker), 3);
  dom.window.close();
});

test('a cancelled map world change rolls back even without a bound world field', { skip: domSkip }, async () => {
  const dom = new JSDOM('<!doctype html><html><body></body></html>', {
    runScripts: 'outside-only', pretendToBeVisual: true, url: 'https://editor.test/editor.html',
  });
  const { window } = dom;
  const mw = mapWorlds();
  window.EditorCore = {
    utils: { escapeHtml }, Page: { t: tr }, UI: { toast: () => {} },
    MapWorlds: mw, PointMap: { open: () => {} }, I18n: { onChange: () => {} },
  };
  window.eval(read('editor-core/map-worlds.js'));
  window.eval(read('editor-core/position-picker.js'));
  const api = window.EditorCore.PositionPicker;
  const picker = api.create({
    xPath: 'x', yPath: 'y', worldId: 2,
    xInputHtml: '<input type="number" data-position-axis="x" data-map-axis="x" value="1" hidden>',
    yInputHtml: '<input type="number" data-position-axis="y" data-map-axis="y" value="1" hidden>',
  });
  window.document.body.appendChild(picker);
  await new Promise((resolve) => setTimeout(resolve, 10));
  assert.equal(api.getWorldId(picker), 2);

  picker.dispatchEvent(new window.CustomEvent('editor-map:world', {
    detail: { worldId: 9, provisional: true }, bubbles: true,
  }));
  assert.equal(picker.getAttribute('data-world-preview'), '9');
  assert.equal(api.getWorldId(picker), 2); // зафиксированный мир не тронут
  assert.equal(mw.rememberedWorld(), null); // и «последний мир» не испорчен

  picker.dispatchEvent(new window.CustomEvent('editor-map:closed', {
    detail: { reason: 'escape' }, bubbles: true,
  }));
  assert.equal(picker.hasAttribute('data-world-preview'), false);
  assert.equal(api.getWorldId(picker), 2);
  dom.window.close();
});

test('invalid tile is highlighted but never resets the coordinates', { skip: domSkip }, async () => {
  const dom = new JSDOM('<!doctype html><html><body></body></html>', {
    runScripts: 'outside-only', pretendToBeVisual: true, url: 'https://editor.test/editor.html',
  });
  const { window } = dom;
  window.EditorCore = {
    utils: { escapeHtml }, Page: { t: tr }, UI: { toast: () => {} },
    MapWorlds: mapWorlds(), PointMap: { open: () => {} }, I18n: { onChange: () => {} },
  };
  window.eval(read('editor-core/map-worlds.js'));
  window.eval(read('editor-core/position-picker.js'));
  const api = window.EditorCore.PositionPicker;
  const picker = api.create({
    xPath: 'x', yPath: 'y', safeTiles: true,
    xInputHtml: '<input type="number" data-position-axis="x" data-map-axis="x" value="128" hidden>',
    yInputHtml: '<input type="number" data-position-axis="y" data-map-axis="y" value="64" hidden>',
  });
  window.document.body.appendChild(picker);

  picker.dispatchEvent(new window.CustomEvent('editor-map:validity', {
    detail: { valid: false, tile: 1 }, bubbles: true,
  }));
  assert.equal(picker.classList.contains('is-invalid'), true);
  assert.equal(picker.dataset.positionValid, '0');
  assert.equal(picker.querySelector('[data-position-warning]').hidden, false);
  assert.equal(picker.querySelector('[data-position-warning]').getAttribute('data-position-tile'), '1');
  // Главное требование: точка остаётся на месте.
  assert.equal(picker.querySelector('[data-position-axis="x"]').value, '128');
  assert.equal(picker.querySelector('[data-position-axis="y"]').value, '64');

  picker.dispatchEvent(new window.CustomEvent('editor-map:validity', {
    detail: { valid: true, tile: null }, bubbles: true,
  }));
  assert.equal(picker.classList.contains('is-invalid'), false);
  assert.equal(picker.querySelector('[data-position-warning]').hidden, true);
  assert.equal(picker.querySelector('[data-position-axis="x"]').value, '128');
  dom.window.close();
});

test('highlightTiles normalizes a tile ID array without implicit spawn IDs', () => {
  const api = loadModule();
  const rootWith = options => ({ getAttribute: name => name === 'data-map-options' ? JSON.stringify(options) : null });
  assert.deepEqual(Array.from(api.resolveMapOptions(rootWith({})).highlightTiles), []);
  assert.deepEqual(Array.from(api.resolveMapOptions(rootWith({ highlightTiles: true })).highlightTiles), []);
  assert.deepEqual(Array.from(api.resolveMapOptions(rootWith({ highlightTiles: [42, 0, 42, 255, -1, 256, 1.5, null, '18'] })).highlightTiles), [42, 0, 255]);
});
