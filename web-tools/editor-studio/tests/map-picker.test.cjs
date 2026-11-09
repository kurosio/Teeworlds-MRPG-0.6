const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const Model = require('../vendor/ddnet-map/picker-model.js');

test('32px tile centers include negative coordinates and zero', () => {
  for (const [input, expected] of [[0,16], [31,16], [32,48], [-1,-16], [-32,-16], [-33,-48]]) {
    assert.equal(Model.tileCenter(input), expected);
  }
  assert.deepEqual(Model.point(0, 32, true), {x:16,y:48});
  assert.equal(Model.point(NaN, 0), null);
  assert.equal(Model.point(0, Infinity), null);
});
test('grid stays aligned and bounded at distant zoom', () => {
  assert.deepEqual(Model.grid(-33, 65, 1), [-32,0,32,64]);
  const distant = Model.grid(-1e7, 1e7, .0001);
  assert.ok(distant.length < 300);
  assert.ok(distant.every(value => value % 32 === 0));
  assert.deepEqual(Model.grid(0, 100, 0), []);
});
test('history restores no-point state, truncates redo, and bounds memory', () => {
  const history = Model.createHistory({x:0,y:0,hasPoint:false}, 3);
  history.push({x:16,y:16,hasPoint:true});
  assert.equal(history.undo().hasPoint, false);
  assert.equal(history.redo().x, 16);
  history.undo();
  history.push({x:48,y:16,hasPoint:true});
  assert.equal(history.canRedo, false);
  for (let x = 0; x < 20; x++) history.push({x,y:0,hasPoint:true});
  assert.equal(history.undo().x,18);
  assert.equal(history.undo().x,17);
  assert.equal(history.canUndo,false);
});

function picker(search = '?map=/map&x=0&y=0', map = {groups:[]}, savedOptions = {}) {
  const nodes = new Map();
  const listeners = {};
  const messages = [];
  // Оверлей рисует через ctx: записываем вызовы, чтобы проверять цвета,
  // пунктир и подписи радиусов без настоящего canvas.
  const drawing = [];
  const canvas = new Proxy({}, {
    get: (target, key) => (...args) => { drawing.push([String(key), ...args]); },
    set: () => true,
  });
  const document = {
    activeElement: null,
    getElementById(id) {
      if (!nodes.has(id)) nodes.set(id, {
        value: '', checked: false, disabled: false, open: false, style: {setProperty() {}},
        classList: {toggle() {}}, attributes: {},
        setAttribute(name, value) { this.attributes[name] = String(value); },
        getAttribute(name) { return this.attributes[name]; },
        removeAttribute(name) { delete this.attributes[name]; },
        focus() { document.activeElement = this; },
        clientWidth:800,clientHeight:600,width:800,height:600,
        getBoundingClientRect: () => ({bottom:80}),
        children: [], append(...items) { this.children.push(...items); },
        appendChild(item) { this.children.push(item); }, replaceChildren() { this.children = []; },
        getContext: () => canvas,
        handlers: {},
        addEventListener(type, fn) { (this.handlers[type] ||= []).push(fn); },
        emit(type, extra = {}) { for (const fn of this.handlers[type] || []) fn({target:this,...extra}); },
        click() {
          if (this.disabled) return;
          // Переключатели режима — кнопки: у них только click, без change.
          if (this.isSegmented) { this.emit('click'); return; }
          this.checked = !this.checked; this.emit('click'); this.emit('change');
        },
      });
      return nodes.get(id);
    },
    querySelector: selector => document.getElementById(selector),
    createElement: tag => document.getElementById(`generated-${nodes.size}-${tag}`),
  };
  document.documentElement = document.getElementById('root');
  const tw = {
    cameraPos:[0,0],cameraPosTarget:[0,0],initialCameraPos:[0,0],moveVelocity:[0,0],
    worldView:[1000,1000],cameraZoom:1,cameraZoomTarget:1,ZOOM_MIN:.005,ZOOM_MAX:12,
    map, LAYERTYPE_TILES:2, keyState:{},requestRender() {},init() {},
  };
  const window = {
    MapPickerModel:Model, MapPickerOverlays:require('../vendor/ddnet-map/picker-overlays.js'),
    MapPickerTiles:require('../vendor/ddnet-map/picker-tiles.js'),
    MapPickerTilesCatalog:require('../vendor/ddnet-map/picker-tile-catalog.js'),
    MapPickerMap:require('../vendor/ddnet-map/picker-map.js'),
    // Слои правки: как в picker.html, но без датафайла — размеры берём из
    // разобранной карты (map.gameplay / map.switchLayer).
    MapPickerMapEdit:{
      locateLayers:(tw)=>({
        game: tw.map?.gameplay ? {tileLayer:{width:tw.map.gameplay.width,height:tw.map.gameplay.height,version:3,data:0}} : null,
        switch: tw.map?.switchLayer ? {tileLayer:{width:tw.map.switchLayer.width,height:tw.map.switchLayer.height,version:3,data:1}} : null,
      }),
      locateGameLayer:(tw)=>({}),
      patch:()=>new Uint8Array(), reload:()=>null,
    },
    location:{search,origin:'https://editor.test'},
    parent:{postMessage: message => messages.push(message)},
    innerWidth:800,innerHeight:600,devicePixelRatio:1,setTimeout:()=>0,
    addEventListener(type, fn) { (listeners[type] ||= []).push(fn); },
  };
  const context = vm.createContext({ window,document,tw,URLSearchParams, console,
    localStorage:{ getItem:()=>JSON.stringify(savedOptions),setItem() {} },
    getComputedStyle:()=>({getPropertyValue:()=> '#6f8cff'}),
  });
  // Порядок как в picker.html: словарь панели → замер → workbench → picker.
  vm.runInContext(fs.readFileSync(path.join(__dirname,'../vendor/ddnet-map/picker-text.js'),'utf8'),context);
  vm.runInContext(fs.readFileSync(path.join(__dirname,'../vendor/ddnet-map/picker-measure.js'),'utf8'),context);
  vm.runInContext(fs.readFileSync(path.join(__dirname,'../vendor/ddnet-map/picker-workbench.js'),'utf8'),context);
  vm.runInContext(fs.readFileSync(path.join(__dirname,'../vendor/ddnet-map/picker.js'),'utf8'),context);
  tw.onMapLoaded();
  const emit = (type, event) => {
    let stopped = false;
    for (const fn of listeners[type] || []) {
      fn({preventDefault(){},stopPropagation(){},stopImmediatePropagation(){ stopped = true; },...event});
      if (stopped) break;
    }
  };
  return { node:id=>document.getElementById(`pk-${id}`), document, messages, tw, emit, drawing, window };
}
/** Экранная точка для мировых координат при текущей камере окна карты. */
const screenPoint = (p, worldX, worldY) => {
  const screenW = 800, screenH = 600;
  const viewH = p.tw.worldView[1] / p.tw.cameraZoom;
  const viewW = p.tw.worldView[0] * (screenW / screenH) / p.tw.cameraZoom;
  return {
    clientX: (worldX - (p.tw.cameraPos[0] - viewW / 2)) * screenW / viewW,
    clientY: (worldY - (p.tw.cameraPos[1] - viewH / 2)) * screenH / viewH,
  };
};
const clickWorld = (p, worldX, worldY) => {
  const canvas = p.document.getElementById('cnvs');
  const spot = screenPoint(p, worldX, worldY);
  p.emit('mousedown', { target: canvas, button: 0, ...spot });
  p.emit('mouseup', { target: canvas, button: 0, ...spot });
};

test('picker controls: align, undo/redo, reset, grid, tile keyboard step and pixel step', () => {
  const p = picker();
  p.node('align').click();
  assert.equal(p.node('out-x').textContent,'16');
  p.node('undo').click();
  assert.equal(p.node('out-x').textContent,'0');
  p.node('redo').click();
  assert.equal(p.node('out-x').textContent,'16');
  p.node('grid').click(); // exercises overlay geometry
  p.node('snap').click();
  p.emit('keydown',{key:'ArrowRight',shiftKey:true});
  assert.equal(p.node('out-x').textContent,'48');
  p.emit('keydown',{key:'ArrowUp',altKey:true});
  assert.equal(p.node('out-y').textContent,'15');
  p.node('reset').click();
  assert.equal(p.node('out-x').textContent,'0');
  p.node('apply').click();
  assert.equal(p.messages.at(-1).type,'editor-map:apply');
  assert.equal(p.messages.at(-1).x,0);
});
test('snap on canvas click, Alt bypass, and undo no-point selection', () => {
  const p = picker('?map=/map&point=0');
  assert.equal(p.node('apply').disabled,true);
  p.node('snap').click();
  const canvas = p.document.getElementById('cnvs');
  const click = altKey => {
    p.emit('mousedown',{target:canvas,button:0,clientX:100,clientY:100});
    p.emit('mouseup',{target:canvas,button:0,clientX:100,clientY:100,altKey});
  };
  click(false);
  assert.equal(Math.abs(Number(p.node('out-x').textContent) % 32),16);
  p.node('undo').click();
  assert.equal(p.node('apply').disabled,true);
  click(true);
  assert.notEqual(Math.abs(Number(p.node('out-x').textContent) % 32),16);
});
test('dragging the marker is one undo step', () => {
  const p = picker();
  const canvas = p.document.getElementById('cnvs');
  p.emit('mousedown',{target:canvas,button:0,clientX:400,clientY:300});
  for (const clientX of [420,440,460]) p.emit('mousemove',{target:canvas,clientX,clientY:300});
  p.emit('mouseup',{target:canvas,button:0,clientX:460,clientY:300});
  assert.notEqual(p.node('out-x').textContent,'0');
  p.node('undo').click();
  assert.equal(p.node('out-x').textContent,'0');
  assert.equal(p.node('undo').disabled,true);
});
test('unsafe query coordinates are discarded instead of enabling an invalid Apply', () => {
  const p = picker('?map=/map&x=1e100&y=9007199254740992');
  assert.equal(p.node('apply').disabled, true);
  assert.equal(p.node('out-x').textContent, '—');
  assert.equal(p.node('out-y').textContent, '—');
});
test('empty coordinates do not silently become zero or allow Apply', () => {
  const p = picker('?map=/map&x=123&y=42');
  const input = p.node('x');
  input.focus(); input.emit('focus');
  input.value = ''; input.emit('input');
  assert.equal(p.node('out-x').textContent,'123');
  assert.equal(p.node('apply').disabled,true);
  input.value = '456'; input.emit('input');
  assert.equal(p.node('apply').disabled,false);
  p.document.activeElement = null; input.emit('blur');
  p.node('undo').click();
  assert.equal(p.node('out-x').textContent,'123');
});

test('measurement isolates content position; using endpoint is explicit and undoable', () => {
  const p = picker();
  const canvas = p.document.getElementById('cnvs');
  const click = (x, y) => {
    p.emit('mousedown',{target:canvas,button:0,clientX:x,clientY:y});
    p.emit('mouseup',{target:canvas,button:0,clientX:x,clientY:y});
  };
  p.node('mode-measure').click();
  assert.equal(p.node('mode-measure').getAttribute('aria-checked'), 'true');
  click(400,300); click(500,300);
  assert.equal(p.node('out-x').textContent,'0');
  assert.equal(p.node('undo').disabled,true);
  assert.match(p.node('measure-result').textContent,/ΔX/);
  assert.equal(p.node('measure-use').disabled,false);
  p.node('measure-use').click();
  assert.notEqual(p.node('out-x').textContent,'0');
  p.node('undo').click();
  assert.equal(p.node('out-x').textContent,'0');
});
test('layer toggles, spawn navigation, collision inspection and radius rendering', () => {
  const layer = {type:2,width:2,height:2};
  const p = picker('?map=/map&x=16&y=16', {
    groups:[{layers:[layer]}], spawnPoint:{x:48,y:48},
    gameplay:{width:2,height:2,indices:Uint8Array.from([1,2,3,0])},
  });
  const checkbox = p.node('layer-list').children[0].children[0];
  checkbox.checked = false; checkbox.emit('change');
  assert.equal(layer.editorHidden,true);
  p.node('layers-reset').click();
  assert.equal(layer.editorHidden,false);
  p.node('spawn').click();
  assert.deepEqual(p.tw.cameraPos,[48,48]);
  p.node('collision').click();
  p.node('radius').value = '64';
  p.tw.onAfterRender();
  assert.match(p.node('collision-status').textContent,/ID 1/);
  assert.match(p.node('collision-status').textContent,/Стена/);
  // Camera commands never change the actual content coordinates.
  assert.equal(p.node('out-x').textContent,'16');
});

test('radii array draws every circle with its own caption and dash pattern', () => {
  const radii = [
    { value: 96, label: 'Spawn radius', color: '#52df9a', dash: [6, 4], lineWidth: 2 },
    { value: 48, label: 'Action radius', color: '#ffda70', dash: [], lineWidth: 1.5, fill: false },
  ];
  const p = picker(`?map=/map&x=48&y=48&radii=${encodeURIComponent(JSON.stringify(radii))}`);
  p.tw.onAfterRender();
  const texts = p.drawing.filter((call) => call[0] === 'fillText').map((call) => call[1]);
  assert.ok(texts.some((text) => /^Spawn radius: 96 px$/.test(text)));
  assert.ok(texts.some((text) => /^Action radius: 48 px$/.test(text)));
  // Пунктир приходит из объявления: [6,4] у первой окружности, сплошная у второй.
  assert.ok(p.drawing.some((call) => call[0] === 'setLineDash' && JSON.stringify(call[1]) === '[6,4]'));
  // Предпросмотр стартует со значения первого радиуса.
  assert.equal(p.node('radius').value, '96');
  // Битый JSON в адресе не ломает окно и не рисует лишних окружностей.
  const broken = picker('?map=/map&x=48&y=48&radii=not-json');
  broken.tw.onAfterRender();
  assert.equal(broken.node('radius').value, '');
});

test('legacy radius/radiusActive address params are gone and draw nothing', () => {
  const p = picker('?map=/map&x=48&y=48&radius=96&radiusActive=32');
  p.tw.onAfterRender();
  const texts = p.drawing.filter((call) => call[0] === 'fillText').map((call) => call[1]);
  assert.deepEqual(texts.filter((text) => /px$/.test(text)), []);
  assert.equal(p.node('radius').value, '');
});

const safeMap = () => ({
  groups:[{layers:[{type:2,width:2,height:2}]}],
  gameplay:{width:2,height:2,indices:Uint8Array.from([1,2,3,0])},
});
test('safeTiles blocks Apply on solid/death/no-hook tiles without resetting the point', () => {
  const p = picker('?map=/map&x=16&y=16&safe=1,2,3', safeMap());
  assert.equal(p.node('apply').disabled,true);
  assert.equal(p.node('validity').hidden,false);
  assert.match(p.node('validity').textContent,/стена/);
  assert.equal(p.node('out-x').textContent,'16');
  assert.equal(p.node('out-y').textContent,'16');
  assert.equal(p.node('x').value,'16');
  const validity = p.messages.filter(m => m.type === 'editor-map:validity');
  assert.deepEqual({...validity.at(-1)},{type:'editor-map:validity',valid:false,tile:1});
  // Apply click while invalid must not send coordinates to the editor.
  p.node('apply').click();
  assert.equal(p.messages.filter(m => m.type === 'editor-map:apply').length,0);

  // Перенос на свободный тайл снимает блокировку, координаты при этом живые.
  p.node('x').value = '48'; p.node('x').emit('input');
  p.node('y').value = '48'; p.node('y').emit('input');
  assert.equal(p.node('out-x').textContent,'48');
  assert.equal(p.node('apply').disabled,false);
  assert.equal(p.node('validity').hidden,true);
  assert.deepEqual({...p.messages.filter(m => m.type === 'editor-map:validity').at(-1)},
    {type:'editor-map:validity',valid:true,tile:null});
  p.node('apply').click();
  const applied = p.messages.filter(m => m.type === 'editor-map:apply').at(-1);
  assert.equal(applied.x,48); assert.equal(applied.y,48);
});
test('without safeTiles the same solid tile is still applicable', () => {
  const p = picker('?map=/map&x=16&y=16', safeMap());
  assert.equal(p.node('apply').disabled,false);
  assert.equal(p.node('validity').hidden,true);
  p.node('apply').click();
  assert.equal(p.messages.at(-1).type,'editor-map:apply');
});
test('disabled safeTiles allows every gameplay tile, including air', () => {
  for (const safe of ['', '&safe=', '&safe=%20', '&safe=,%20,']) {
    for (const [x, y] of [[16,16], [48,16], [16,48], [48,48]]) {
      const p = picker(`?map=/map&x=${x}&y=${y}${safe}`, safeMap());
      assert.equal(p.node('apply').disabled,false);
      assert.equal(p.node('validity').hidden,true);
      p.node('apply').click();
      assert.equal(p.messages.at(-1).type,'editor-map:apply');
      assert.equal(p.messages.at(-1).x,x);
      assert.equal(p.messages.at(-1).y,y);
    }
  }
});
test('safeTiles ignores empty list entries but still supports explicitly blocking air', () => {
  const p = picker('?map=/map&x=48&y=48&safe=1,,%20,2,3,', safeMap());
  assert.equal(p.node('apply').disabled,false);
  const blockedAir = picker('?map=/map&x=48&y=48&safe=0', safeMap());
  assert.equal(blockedAir.node('apply').disabled,true);
  assert.match(blockedAir.node('validity').textContent,/ID 0/);
});
test('safeTiles can be a custom tile list and fails open without a gameplay layer', () => {
  const onlyDeath = picker('?map=/map&x=16&y=16&safe=2', safeMap());
  assert.equal(onlyDeath.node('apply').disabled,false); // тайл 1 не запрещён
  const noGame = picker('?map=/map&x=16&y=16&safe=1,2,3', {groups:[]});
  assert.equal(noGame.node('apply').disabled,false); // проверять нечего — не блокируем
});
test('picker opens in point mode, gameplay and grid off by default', () => {
  const p = picker('?map=/map&x=48&y=48', safeMap(), {grid:true});
  assert.equal(p.node('collision').checked,false);
  assert.equal(p.node('grid').checked,false); // localStorage не задаёт умолчание для сетки
  assert.equal(p.node('snap').checked,false);
  // Режим клика один и по умолчанию — обычная точка; скрытые секции
  // связанных точек не мешают карте.
  assert.equal(p.node('mode-point').getAttribute('aria-checked'),'true');
  assert.equal(p.node('mode-tile').getAttribute('aria-checked'),'false');
  assert.equal(p.node('sec-points').hidden,true);
  assert.equal(p.node('sec-edit').hidden,true);
});
test('safe, highlightTiles and radius do not implicitly enable gameplay or expand tools', () => {
  const p = picker('?map=/map&x=48&y=48&safe=1,2,3&highlightTiles=209&radii=%5B%7B%22value%22%3A96%7D%5D', safeMap());
  assert.equal(p.node('collision').checked,false);
  assert.equal(p.node('grid').checked,false);
  assert.equal(p.node('radius').value,'96');
  assert.equal(p.node('mode-tile').getAttribute('aria-checked'),'false');
  p.tw.onAfterRender(); // спавны и радиус работают без полного игрового слоя
});
test('explicit map flags enable gameplay and grid without expanding tools', () => {
  const p = picker('?map=/map&x=48&y=48&safe=1,2,3&tiles=1&highlightTiles=209&radii=%5B%7B%22value%22%3A96%7D%5D&grid=1&snap=1', safeMap());
  assert.equal(p.node('collision').checked,true);
  assert.equal(p.node('grid').checked,true);
  assert.equal(p.node('snap').checked,true);
  assert.equal(p.node('radius').value,'96');
  assert.equal(p.node('mode-point').getAttribute('aria-checked'),'true');
  p.tw.onAfterRender(); // спавны/запрещённые тайлы/маркер рисуются без ошибок
  assert.match(p.node('collision-status').textContent,/Красные тайлы здесь запрещены/);
});

test('linked highlighted mob spawn tiles can be assigned as the current record position', () => {
  const map = {groups:[], gameplay:{width:4,height:1,indices:Uint8Array.from([209,0,209,18])}};
  const p = picker('?map=/map&x=16&y=16&linkedTiles=1&highlightTiles=209', map);
  // Секция точек появляется сама (linkedTiles + highlightTiles): «найденные
  // тайлы» — это выбор координат записи, а не правка карты.
  assert.equal(p.node('sec-points').hidden, false);
  assert.equal(p.node('sec-edit').hidden, false);
  assert.equal(p.node('linked-tiles-use').disabled, false);
  assert.match(p.node('linked-tiles-status').textContent, /Найдено точек: 2/);
  assert.equal(p.node('linked-tiles').children.length, 2);
  p.node('linked-tiles').value = '1';
  p.node('linked-tiles-use').click();
  assert.equal(p.node('out-x').textContent, '80'); // second marker center
  assert.equal(p.node('out-y').textContent, '16');
  assert.equal(p.messages.at(-1).type, 'editor-map:change');
  assert.equal(p.tw.cameraPosTarget[0], 80);
});
test('edit mode places a point on an empty cell and selects an occupied one', () => {
  const map = {groups:[], gameplay:{width:4,height:1,indices:Uint8Array.from([209,0,0,18])}};
  const p = picker('?map=/map&x=16&y=16&linkedTiles=1&highlightTiles=209', map);
  p.node('mode-tile').click();
  assert.equal(p.node('mode-tile').getAttribute('aria-checked'), 'true');
  assert.equal(p.node('collision').checked, true);
  clickWorld(p, 48, 16);
  assert.equal(map.gameplay.indices[1], 209);
  assert.equal(p.node('out-x').textContent, '48');
  assert.equal(p.node('out-y').textContent, '16');
  assert.match(p.node('edit-status').textContent, /Черновик: добавлено 1/);
  // Клик по занятой клетке ничего не перезаписывает: она выделяется.
  clickWorld(p, 112, 16);
  assert.equal(map.gameplay.indices[3], 18);
  assert.match(p.node('edit-selected').textContent, /X 3, Y 0 · ID 18/);
  // «Сбросить правки» возвращает карту к исходному состоянию.
  p.node('edit-reset').click();
  assert.equal(map.gameplay.indices[1], 0);
});
test('selected point can be moved and deleted, and edits are bounded', () => {
  const map = {groups:[], gameplay:{width:8,height:1,indices:Uint8Array.from([209,0,0,0,0,0,0,0])}};
  const p = picker('?map=/map&x=16&y=16&linkedTiles=1&highlightTiles=209', map);
  p.node('mode-tile').click();
  // Выбираем существующую точку и тащим её на две клетки вправо.
  clickWorld(p, 16, 16);
  const canvas = p.document.getElementById('cnvs');
  const from = screenPoint(p, 16, 16), to = screenPoint(p, 80, 16);
  p.emit('mousedown', { target: canvas, button: 0, ...from });
  p.emit('mousemove', { target: canvas, ...to });
  p.emit('mouseup', { target: canvas, button: 0, ...to });
  assert.equal(map.gameplay.indices[0], 0, 'source cell is cleared');
  assert.equal(map.gameplay.indices[2], 209, 'target cell receives the point');
  // Delete удаляет выбранную точку.
  p.emit('keydown', { key: 'Delete' });
  assert.equal(map.gameplay.indices[2], 0);
  assert.match(p.node('edit-status').textContent, /Черновик/);
  // Правки можно отбросить целиком.
  p.node('edit-reset').click();
  assert.equal(map.gameplay.indices[0], 209);
});
test('edit mode can be toggled off to resume ordinary position picking', () => {
  const map = {groups:[], gameplay:{width:8,height:1,indices:Uint8Array.from([209,0,0,0,0,0,0,0])}};
  const p = picker('?map=/map&x=16&y=16&linkedTiles=1&highlightTiles=209', map);
  p.node('mode-tile').click();
  clickWorld(p, 48, 16);
  assert.equal(map.gameplay.indices[1], 209);
  // Повторное нажатие того же инструмента возвращает обычный клик.
  p.node('mode-tile').click();
  assert.equal(p.node('mode-point').getAttribute('aria-checked'), 'true');
  assert.equal(p.node('collision').checked, false, 'leaving placement mode restores the prior gameplay-layer toggle');
  assert.equal(p.node('grid').checked, false, 'leaving placement mode restores the prior grid toggle');
  clickWorld(p, 112, 16);
  assert.equal(p.node('out-x').textContent, '112');
  assert.equal(map.gameplay.indices[3], 0, 'ordinary point mode must not stage a map tile');
});
test('switch layer template edits ore tiles with a switch number', () => {
  const map = {groups:[], gameplay:{width:2,height:1,indices:Uint8Array.from([0,0])},
    switchLayer:{width:2,height:1,tiles:[{x:16,y:16,type:200,number:5},{x:48,y:16,type:199,number:1}]}};
  const p = picker('?map=/map&x=16&y=16&linkedTiles=1&highlightTiles=200', map);
  p.node('mode-tile').click();
  // Switch-слой доступен и выбирается переключателем слоя.
  assert.equal(p.node('edit-layer-switch').disabled, false);
  p.node('edit-layer-switch').click();
  assert.equal(p.node('edit-layer-switch').getAttribute('aria-checked'), 'true');
  // Тип и номер приходят из полей панели.
  p.node('switch-type').value = '200';
  p.node('switch-number').value = '42';
  clickWorld(p, 16, 16);
  // Клик по занятой клетке выбирает её, а не перезаписывает номер.
  assert.equal(p.node('edit-selected').textContent.includes('switch #5'), true);
  p.node('switch-number').value = '43';
  p.node('switch-number').emit('change');
  assert.match(p.node('edit-selected').textContent, /switch #43/);
});
test('map save endpoint keeps admin, optimistic and allowlist checks for both layers', () => {
  const endpoint = fs.readFileSync(path.join(__dirname, '../api/map-edit.php'), 'utf8');
  assert.match(endpoint, /bootstrap_editor_api_roles\(\[EDITOR_ROLE_ADMIN\]\)/);
  assert.match(endpoint, /editor_csrf_verify\(\)/);
  assert.match(endpoint, /HTTP_X_MAP_ORIGINAL_SHA256/);
  assert.match(endpoint, /HTTP_X_MAP_ALLOWED_TILES/);
  // Оба слоя проверяются по списку разрешённых ID; удаление и перенос
  // разрешены, но только для сущностей и только в пределах allowlist.
  assert.match(endpoint, /if \(\$newIndex !== 0 && !isset\(\$allowed\[\$newIndex\]\)\)/);
  assert.match(endpoint, /if \(\$newType !== 0 && !isset\(\$allowed\[\$newType\]\)\)/);
  assert.match(endpoint, /Номер switch должен быть от 1 до 255/);
  assert.match(endpoint, /map_edit_expand_switch/);
  assert.match(endpoint, /isset\(\$allowed\[\$oldIndex\]\)/);
  assert.match(endpoint, /map_edit_assert_tile_only_change\(\$oldMapInfo, \$newMapInfo, \$allowedTileIds\)/);
  assert.match(endpoint, /hash_equals\(\$expectedHash, \$latestHash\)/);
  assert.match(endpoint, /@rename\(\$temp, \$file\)/);
  assert.match(endpoint, /map-backups/);
  // Тайлы столкновений удалять/перемещать нельзя.
  assert.match(endpoint, /Тайлы столкновений/);
});
test('collecting preview is read-only and labels switch locations without a gameplay layer', () => {
  const p = picker('?map=/map&point=0&preview=1&highlightLayer=switch&highlightTiles=200&switchShowID=255', {
    groups:[], switchLayer:{width:2,height:1,tiles:[{x:16,y:16,type:200,number:255},{x:48,y:16,type:200,number:1}]},
  });
  assert.equal(p.node('apply').hidden,true);
  assert.equal(p.node('x').disabled,true);
  assert.equal(p.node('y').disabled,true);
  const canvas = p.document.getElementById('cnvs');
  p.emit('mousedown',{target:canvas,button:0,clientX:100,clientY:100});
  p.emit('mouseup',{target:canvas,button:0,clientX:100,clientY:100});
  p.node('x').value='123'; p.node('x').emit('change');
  p.node('apply').click();
  assert.equal(p.messages.some(m=>m.type==='editor-map:apply'),false);
  assert.equal(p.messages.some(m=>m.type==='editor-map:change'),false);
  assert.match(p.node('note').textContent, /1/);
});
