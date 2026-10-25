const { test } = require('node:test');
const assert = require('node:assert/strict');
const O = require('../vendor/ddnet-map/picker-overlays.js');
const game = {width:2,height:2,indices:Uint8Array.from([1,2,3,0])};
test('gameplay tile lookup handles edges, negatives, empty and missing layers', () => {
  assert.equal(O.tileAt(game,{x:0,y:0}),1);
  assert.equal(O.tileAt(game,{x:32,y:0}),2);
  assert.equal(O.tileAt(game,{x:32,y:32}),0);
  assert.equal(O.tileAt(game,{x:-1,y:0}),null);
  assert.equal(O.tileAt(game,{x:64,y:0}),null);
  assert.equal(O.tileAt(null,{x:0,y:0}),null);
});
test('collision culling clips to the viewport and limits distant zoom work', () => {
  assert.deepEqual(O.visibleTiles(game,{x:31,y:31},{x:33,y:33}),{x0:0,y0:0,x1:2,y1:2});
  assert.equal(O.visibleTiles({width:10000,height:10000},{x:0,y:0},{x:320000,y:320000}),null);
});
test('measurement is in world pixels and independent of camera zoom', () => {
  assert.deepEqual(O.distance({x:0,y:0},{x:96,y:128}),{dx:96,dy:128,length:160});
});
test('collision drawing excludes air and tiles outside visible bounds', () => {
  const fills = [];
  const ctx = {save(){},restore(){},fillRect(...args){fills.push(args);},fillText(){}};
  O.drawCollision(ctx,game,{x0:1,y0:0,x1:2,y1:2},(x,y)=>({x,y}));
  assert.deepEqual(fills,[[32,0,32,32]]);
});
test('blockedTile only rejects explicitly forbidden gameplay tiles', () => {
  assert.equal(O.blockedTile(game,{x:16,y:16},[1,2,3]),1);
  assert.equal(O.blockedTile(game,{x:48,y:16},[1,2,3]),2);
  assert.equal(O.blockedTile(game,{x:16,y:48},[1,2,3]),3);
  assert.equal(O.blockedTile(game,{x:48,y:48},[1,2,3]),null); // пустой тайл
  assert.equal(O.blockedTile(game,{x:-16,y:16},[1,2,3]),null); // вне слоя
  assert.equal(O.blockedTile(null,{x:16,y:16},[1,2,3]),null); // нет игрового слоя
  assert.equal(O.blockedTile(game,{x:16,y:16},[]),null); // флаг выключен
  assert.equal(O.blockedTile(game,{x:16,y:16},[2]),null); // кастомный список
});
test('mob spawn highlighting only marks ENTITY_OFFSET + ENTITY_SPAWN_MOBS', () => {
  const highlightTiles = {width:8,height:1,indices:Uint8Array.from([209,192,209,193,194,208,211,18])};
  const arcs = [];
  const ctx = {save(){},restore(){},beginPath(){},stroke(){},arc(...args){arcs.push(args);}};
  O.drawHighlightedTiles(ctx,highlightTiles,{x0:0,y0:0,x1:8,y1:1},(x,y)=>({x,y}), [209]);
  assert.equal(arcs.length,2);
  assert.deepEqual(arcs[0].slice(0,2),[16,16]);
  assert.deepEqual(arcs[1].slice(0,2),[80,16]);
  assert.equal(O.drawHighlightedTiles(ctx,highlightTiles,null,(x,y)=>({x,y})),undefined);
});
test('tile box highlights the hovered tile and blocked tiles get a danger fill', () => {
  const painted = [];
  const ctx = {save(){},restore(){},fillRect(...a){painted.push(['fill',...a]);},strokeRect(...a){painted.push(['stroke',...a]);}};
  O.drawTileBox(ctx,16,16,(x,y)=>({x,y}),'rgba(242,114,127,.34)','#f2727f');
  assert.deepEqual(painted,[['fill',0,0,32,32],['stroke',1,1,30,30]]);
  const fills = [];
  const ctx2 = {save(){},restore(){},fillRect(...a){fills.push(a);},fillText(){},strokeRect(){}};
  O.drawCollision(ctx2,game,{x0:0,y0:0,x1:1,y1:1},(x,y)=>({x,y}),[1,2,3]);
  assert.equal(fills.length,1);
});

test('scenario mob positions enable mob highlightTiles and bind the radius field', () => {
  const fs = require('node:fs');
  const path = require('node:path');
  const vm = require('node:vm');
  const context = {window: {EditorCore: {}}};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../editor-core/registry.js'), 'utf8'), context);
  const schemas = context.window.EditorCore.schemas.componentSchemas;
  const ui = schemas.defeat_mobs.fields.position.ui;
  assert.deepEqual(Array.from(ui.highlightTiles), [209]);
  assert.equal(ui.radiusPath, 'radius');
  assert.equal(ui.safeTiles, false);
  for (const [name, schema] of Object.entries(schemas)) {
    for (const field of Object.values(schema.fields || {})) {
      if (field.type === 'vec2' && name !== 'defeat_mobs') assert.deepEqual(Array.from(field.ui.highlightTiles), []);
    }
  }
});

test('tile highlights accept arbitrary IDs and an empty list disables drawing', () => {
  const game = { width: 4, height: 1, indices: Uint8Array.from([0, 18, 209, 42]) };
  const arcs = [];
  const ctx = { save(){}, restore(){}, beginPath(){}, stroke(){}, arc(x,y){ arcs.push([x,y]); } };
  const bounds = { x0: 0, y0: 0, x1: 4, y1: 1 };
  const project = (x,y) => ({x,y});
  O.drawHighlightedTiles(ctx, game, bounds, project, []);
  assert.deepEqual(arcs, []);
  O.drawHighlightedTiles(ctx, game, bounds, project, [0, 42]);
  assert.deepEqual(arcs, [[16,16], [112,16]]);
});
