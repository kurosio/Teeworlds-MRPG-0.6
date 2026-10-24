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
