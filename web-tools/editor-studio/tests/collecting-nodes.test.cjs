const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const zlib = require('node:zlib');
const codec = require('../editor-core/collecting-properties.js');
const overlays = require('../vendor/ddnet-map/picker-overlays.js');
const root = path.join(__dirname, '..');
const read = name => fs.readFileSync(path.join(root, name), 'utf8');

test('collecting properties preserve legacy amounts and small chances without rounding', () => {
  const old = '[173/80.0],[50/20.0],[172/0.0001],[171/0.0005]';
  const items = codec.parse(old);
  assert.equal(items[0].amount, 0);
  assert.equal(items[2].chance, 0.0001);
  assert.equal(codec.validate(items), '');
  assert.deepEqual(codec.parse(codec.serialize(items)), items);
  const explicit = codec.parse('[173/3/80],[50/1/20]');
  assert.equal(explicit[0].amount, 3);
  assert.equal(codec.serialize(explicit), '[173/3/80],[50/1/20]');
  for (const malformed of ['[1/2/3/4]', '[1/NaN]', '[1/20]junk', 'junk', '[1/20],']) assert.throws(() => codec.parse(malformed));
});

test('collecting validation rejects empty, duplicate and invalid drop values', () => {
  assert.ok(codec.validate([]));
  assert.ok(codec.validate(codec.parse('[1/10],[1/20]')));
  for (const item of [{item_id:0,amount:1,chance:1}, {item_id:1,amount:-1,chance:1},
    {item_id:1,amount:1.5,chance:1}, {item_id:1,amount:1,chance:0},
    {item_id:1,amount:1,chance:101}, {item_id:1,amount:1,chance:NaN}]) assert.ok(codec.validate([item]));
});

test('collecting editor boots on shared UI, filters world zero and previews unsaved switch values', () => {
  const nodes = new Map();
  const node = id => {
    if (!nodes.has(id)) nodes.set(id, {value:'', classList:{toggle(){}}, addEventListener(type, fn){ this[type] = fn; }});
    return nodes.get(id);
  };
  let cfg, model;
  const previews = [];
  const core = { CollectingProperties:codec, Page:{guardUnsaved(){}}, UI:{toast(){}},
    PointMap:{preview: options => previews.push(options)},
    DbEditor:{ mount: (_, options) => { cfg = options; return {getModel:()=>model,refresh(){}}; } } };
  const window = {EditorCoreBootstrap:{fieldRenderOptions:{}}};
  const document = {body:{},getElementById:node};
  const html = read('nodes-collecting-editor.html');
  const script = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].at(-1)[1];
  vm.runInNewContext(script, {window, document, EditorCore:core});
  assert.equal(cfg.resource, 'nodes_collecting');
  for (const [Type, icon] of Object.entries({ fish:'fa-fish', ore:'fa-gem', plant:'fa-seedling' })) {
    assert.equal(cfg.listItem({ Type, Name:'Resource' }).icon, icon);
  }
  assert.ok(!html.includes('previewWorld'));
  assert.ok(html.includes('Preview linked nodes on map'));
  assert.ok(!html.includes('One database node links all switch tiles'));
  assert.ok(!html.includes('One item is selected per harvest.'));
  const row = {ID:1, Name:'Stone / Coal', WorldID:0, Type:'ore', SwitchID:1, Level:1, Health:3, Properties:'[173/80],[50/20]'};
  model = cfg.rowToModel(row);
  assert.equal(cfg.validateModel(model), '');
  assert.equal(cfg.modelToPayload(model).Properties, row.Properties);
  node('filterWorld').value = '0';
  assert.equal(cfg.transformList([row,{...row,WorldID:1}]).length, 1);
  model.SwitchID = 255;
  node('previewNode').click();
  assert.equal(previews[0].worldId, 0);
  assert.equal(previews[0].switchShowID, 255);
  assert.deepEqual(Array.from(previews[0].highlightTiles), [200]);
  cfg.dirty.markDirty(); assert.equal(cfg.dirty.isDirty(), true);
  cfg.dirty.setClean(); assert.equal(cfg.dirty.isDirty(), false);
});

function renderer() {
  const context = vm.createContext({window:{}, console, Uint8Array, DataView});
  vm.runInContext(read('vendor/ddnet-map/dataviewext.js'), context);
  vm.runInContext(read('vendor/ddnet-map/twwebgl.js'), context);
  return context.tw;
}

test('switch layer parses number/type in both modern and legacy map item layouts', () => {
  const tw = renderer();
  for (const [version, offset] of [[2,72],[3,84]]) {
    const data = new ArrayBuffer(offset + 4), view = new DataView(data);
    view.setInt32(12,version,true); view.setInt32(offset,42,true);
    assert.equal(tw.parseMapLayerTiles(data).switchData,42);
  }
  const tiles = tw.parseSwitchTiles(Uint8Array.from([1,200,0,0, 2,199,0,0, 1,201,0,0, 0,0,0,0]).buffer,2,2);
  assert.equal(tiles.tiles.length,3);
  assert.equal(tiles.tiles[2].y,48);
  assert.equal(tiles.tiles[2].number,1);
  assert.throws(()=>tw.parseSwitchTiles(new ArrayBuffer(1),2,2), /Truncated/);
});

test('highlightTiles with switchShowID selects every matching location but not other types or IDs', () => {
  const arcs = [], labels = [];
  const ctx = {save(){},restore(){},beginPath(){},stroke(){},fillRect(){},arc(x,y){arcs.push([x,y]);},fillText(s){labels.push(s);}};
  const layer = {tiles:[{x:16,y:16,type:200,number:1},{x:48,y:16,type:200,number:2},
    {x:80,y:16,type:199,number:1},{x:112,y:16,type:200,number:1}]};
  assert.equal(overlays.drawSwitchTiles(ctx,layer,(x,y)=>({x,y}),[200],1),2);
  assert.deepEqual(arcs,[[16,16],[112,16]]);
  assert.deepEqual(labels,['#1','#1']);
  assert.equal(overlays.drawSwitchTiles(ctx,layer,(x,y)=>({x,y}),[199,200],null),4);
  assert.equal(overlays.drawSwitchTiles(ctx,null,(x,y)=>({x,y}),[200],1),0);
});

test('real main.map switch data can be decoded and contains collecting entities', () => {
  const tw = renderer();
  const b = fs.readFileSync(path.join(root,'../../maps/main.map'));
  const types=b.readInt32LE(16), items=b.readInt32LE(20), raw=b.readInt32LE(24), itemSize=b.readInt32LE(28);
  const itemOffsets=36+types*12, rawOffsets=itemOffsets+items*4, rawSizes=rawOffsets+raw*4;
  const itemStart=rawSizes+raw*4, dataStart=itemStart+itemSize;
  let found = false;
  for(let i=0;i<items;i++) {
    const offset=itemStart+b.readInt32LE(itemOffsets+i*4);
    if ((b.readUInt32LE(offset)>>>16)!==5) continue;
    const size=b.readInt32LE(offset+4);
    const item=Uint8Array.from(b.subarray(offset+8,offset+8+size)).buffer;
    if (new DataView(item).getInt32(4,true)!==2) continue;
    const info=tw.parseMapLayerTiles(item);
    if (!(info.flags&16)) continue;
    const start=b.readInt32LE(rawOffsets+info.switchData*4);
    const end=info.switchData+1<raw ? b.readInt32LE(rawOffsets+(info.switchData+1)*4) : b.readInt32LE(32);
    const data=Uint8Array.from(zlib.inflateSync(b.subarray(dataStart+start,dataStart+end))).buffer;
    const layer=tw.parseSwitchTiles(data,info.width,info.height);
    assert.ok(layer.tiles.some(t=>[199,200,201].includes(t.type)));
    found=true;
  }
  assert.equal(found,true);
});
