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

function picker(search = '?map=/map&x=0&y=0', map = {groups:[]}) {
  const nodes = new Map();
  const listeners = {};
  const messages = [];
  const canvas = new Proxy({}, { get: () => () => {} });
  const document = {
    activeElement: null,
    getElementById(id) {
      if (!nodes.has(id)) nodes.set(id, {
        value: '', checked: false, disabled: false, style: {setProperty() {}},
        classList: {toggle() {}}, setAttribute() {}, focus() { document.activeElement = this; },
        clientWidth:800,clientHeight:600,width:800,height:600,
        getBoundingClientRect: () => ({bottom:80}),
        children: [], append(...items) { this.children.push(...items); },
        appendChild(item) { this.children.push(item); }, replaceChildren() { this.children = []; },
        getContext: () => canvas,
        handlers: {},
        addEventListener(type, fn) { (this.handlers[type] ||= []).push(fn); },
        emit(type, extra = {}) { for (const fn of this.handlers[type] || []) fn({target:this,...extra}); },
        click() { if (this.disabled) return; this.checked = !this.checked; this.emit('click'); this.emit('change'); },
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
    MapPickerModel:Model, MapPickerOverlays:require('../vendor/ddnet-map/picker-overlays.js'), location:{search,origin:'https://editor.test'},
    parent:{postMessage: message => messages.push(message)},
    innerWidth:800,innerHeight:600,devicePixelRatio:1,setTimeout:()=>0,
    addEventListener(type, fn) { (listeners[type] ||= []).push(fn); },
  };
  const context = vm.createContext({ window,document,tw,URLSearchParams, console,
    localStorage:{ getItem:()=>null,setItem() {} },
    getComputedStyle:()=>({getPropertyValue:()=> '#6f8cff'}),
  });
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
  return { node:id=>document.getElementById(`pk-${id}`), document, messages, tw, emit };
}
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
  p.node('measure').click();
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
