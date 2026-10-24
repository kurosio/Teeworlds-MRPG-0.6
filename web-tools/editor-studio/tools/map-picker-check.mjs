#!/usr/bin/env node
/** Focused regression tests for the map viewer's clipping and culling. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const code = fs.readFileSync(path.join(root, 'vendor/ddnet-map/twwebgl.js'), 'utf8');
const dataViewExtensions = fs.readFileSync(path.join(root, 'vendor/ddnet-map/dataviewext.js'), 'utf8');
const calls = [];
const gl = {
  SCISSOR_TEST: 0x0c11,
  disable: (x) => calls.push(['disable', x]),
  enable: (x) => calls.push(['enable', x]),
  scissor: (...args) => calls.push(['scissor', ...args]),
};
const window = {
  twConfig: {},
  requestAnimationFrame: () => 0,
  innerWidth: 800,
  innerHeight: 600,
};
const context = vm.createContext({ window, document: {}, console, URL, setTimeout, clearTimeout });
vm.runInContext(dataViewExtensions, context, { filename: 'dataviewext.js' });
vm.runInContext(code, context, { filename: 'twwebgl.js' });
const { tw } = context;

// DDNet's CQuad wire format is exactly 152 bytes; malformed/legacy 104-byte
// blocks must fail explicitly rather than shift every parsed field.
const quadBytes = new ArrayBuffer(152);
const quadView = new DataView(quadBytes);
for (let i = 0; i < 38; i++) quadView.setInt32(i * 4, i, true);
assert.equal(tw.parseLayerQuads(quadBytes, 1).length, 1);
assert.throws(() => tw.parseLayerQuads(new ArrayBuffer(104), 1), (error) => error.name === 'RangeError');
const legacyGroupInfo = tw.parseMapGroup(new ArrayBuffer(28));
assert.equal(legacyGroupInfo.useClipping, 0);
const truncatedGroupInfoBytes = new ArrayBuffer(28);
new DataView(truncatedGroupInfoBytes).setInt32(0, 3, true);
assert.throws(() => tw.parseMapGroup(truncatedGroupInfoBytes), (error) => error.name === 'RangeError');
const clippedGroupInfoBytes = new ArrayBuffer(48);
new DataView(clippedGroupInfoBytes).setInt32(0, 3, true);
new DataView(clippedGroupInfoBytes).setInt32(28, 1, true);
new DataView(clippedGroupInfoBytes).setInt32(32, 10, true);
new DataView(clippedGroupInfoBytes).setInt32(36, 20, true);
new DataView(clippedGroupInfoBytes).setInt32(40, 300, true);
new DataView(clippedGroupInfoBytes).setInt32(44, 400, true);
const clippedInfo = tw.parseMapGroup(clippedGroupInfoBytes);
assert.equal(clippedInfo.useClipping, 1);
assert.deepEqual([clippedInfo.clipX, clippedInfo.clipY, clippedInfo.clipW, clippedInfo.clipH], [10, 20, 300, 400]);

tw.gl = gl;
tw.canvas = { width: 800, height: 600 };
tw.worldView = [1000, 1000];
tw.aspect = 4 / 3;
tw.cameraZoom = 1;
tw.cameraPos = [500, 400];

const group = (overrides = {}) => ({
  useClipping: true,
  version: 3,
  gameLayer: false,
  clipX: 0,
  clipY: 0,
  clipW: 500,
  clipH: 500,
  ...overrides,
});

// Clip is projected from interface/world coordinates into framebuffer pixels.
calls.length = 0;
assert.equal(tw.Map.Group.prototype.applyClip.call(group()), true);
assert.deepEqual(calls.at(-1), ['scissor', 99, 240, 302, 300]);

// A clipping region completely outside the viewport suppresses the entire group.
calls.length = 0;
assert.equal(tw.Map.Group.prototype.applyClip.call(group({ clipX: 5000 })), false);
assert.equal(calls.some(([name]) => name === 'enable'), false);

// Invalid empty clipping bounds are empty, never interpreted as unclipped.
calls.length = 0;
assert.equal(tw.Map.Group.prototype.applyClip.call(group({ clipW: 0 })), false);

// Legacy groups and game-layer groups follow the engine's clipping exceptions.
calls.length = 0;
assert.equal(tw.Map.Group.prototype.applyClip.call(group({ version: 1 })), true);
assert.equal(calls.some(([name]) => name === 'scissor'), false);
calls.length = 0;
assert.equal(tw.Map.Group.prototype.applyClip.call(group({ gameLayer: true })), true);
assert.equal(calls.some(([name]) => name === 'scissor'), false);

// Large quad layers outside the current group view are culled before drawing.
calls.length = 0;
tw.initPrjMat = () => {};
const rendered = [];
const visible = { bounds: { x0: 0, y0: 0, x1: 100, y1: 100 }, render: () => rendered.push('visible') };
const distant = { bounds: { x0: 10000, y0: 10000, x1: 10100, y1: 10100 }, render: () => rendered.push('distant') };
const renderGroup = Object.create(tw.Map.Group.prototype);
Object.assign(renderGroup, group({ useClipping: false }), {
  layers: [visible, distant],
  initMapScreen() { tw.mapScreen = [-100, -100, 1100, 900]; },
});
renderGroup.render();
assert.deepEqual(rendered, ['visible']);

// Clipped groups wholly outside the camera never submit their quads.
rendered.length = 0;
const clippedGroup = Object.create(tw.Map.Group.prototype);
Object.assign(clippedGroup, group({ clipX: 5000 }), {
  layers: [visible],
  initMapScreen() { tw.mapScreen = [-100, -100, 1100, 900]; },
});
clippedGroup.render();
assert.deepEqual(rendered, []);
console.log('Map picker clipping/culling: ALL PASSED');
