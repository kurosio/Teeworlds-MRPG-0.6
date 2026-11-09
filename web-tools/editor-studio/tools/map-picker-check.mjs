#!/usr/bin/env node
/** Focused regression tests for the map viewer's clipping, culling, parallax, textures, and quad parsing. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const repoRoot = path.resolve(root, '../..');
const code = fs.readFileSync(path.join(root, 'vendor/ddnet-map/twwebgl.js'), 'utf8');
const dataFileCode = fs.readFileSync(path.join(root, 'vendor/ddnet-map/twdatafile.js'), 'utf8');
const dataViewExtensions = fs.readFileSync(path.join(root, 'vendor/ddnet-map/dataviewext.js'), 'utf8');
const mapEditCode = fs.readFileSync(path.join(root, 'vendor/ddnet-map/picker-map-edit.js'), 'utf8');
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
  location: { href: 'http://localhost/picker.html', origin: 'http://localhost' },
};
const Zlib = {
  Inflate: function (compressed) {
    this.decompress = () => new Uint8Array(zlib.inflateSync(Buffer.from(compressed)));
  },
  Deflate: { compress: (plain) => new Uint8Array(zlib.deflateSync(Buffer.from(plain))) },
};
const context = vm.createContext({
  window,
  location: window.location,
  document: {},
  console,
  URL,
  Zlib,
  setTimeout,
  clearTimeout,
});
vm.runInContext(dataViewExtensions, context, { filename: 'dataviewext.js' });
vm.runInContext(dataFileCode, context, { filename: 'twdatafile.js' });
vm.runInContext(code, context, { filename: 'twwebgl.js' });
vm.runInContext(mapEditCode, context, { filename: 'picker-map-edit.js' });
const { tw, TwDataFile } = context;
const MapPickerMapEdit = window.MapPickerMapEdit;
assert.equal(MapPickerMapEdit.sha256(new TextEncoder().encode('abc')), 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');

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
// Editor visibility suppresses both buffer updates and draw submissions.
rendered.length = 0;
visible.editorHidden = true;
renderGroup.render();
assert.deepEqual(rendered, []);
let ticks = 0;
visible.tick = () => ticks++;
distant.tick = () => ticks++;
renderGroup.tick();
assert.equal(ticks, 1);
visible.editorHidden = false;
renderGroup.tick();
assert.equal(ticks, 3);

const gameplay = Uint8Array.from([1, 0, 0, 0, 2, 0, 0, 0, 192, 0, 0, 0]);
assert.deepEqual(Array.from(tw.parseGameplayIndices(gameplay.buffer, 3)), [1, 2, 192]);
assert.throws(() => tw.parseGameplayIndices(gameplay.buffer, 4), error => error.name === 'RangeError');
assert.throws(() => tw.parseGameplayIndices(gameplay.buffer, -1), error => error.name === 'RangeError');

// DDNet v4 tile layers compress runs via m_Skip (byte [tile*4 + 2]).
const rleGameplay = Uint8Array.from([0, 0, 2, 0, 192, 0, 0, 0, 1, 0, 1, 0]);
assert.deepEqual(Array.from(tw.parseGameplayIndices(rleGameplay.buffer, 6, 4)), [0, 0, 0, 192, 1, 1]);
assert.throws(() => tw.parseGameplayIndices(rleGameplay.buffer, 7, 4), error => error.name === 'RangeError');
assert.equal(tw.parseLayerTiles(rleGameplay.buffer, 6, 4).filter(t => t.index !== 0).length, 3);

// Empty quad layers in DDNet maps often carry a 1-quad (152-byte) placeholder buffer.
assert.equal(tw.parseLayerQuads(new ArrayBuffer(152), 0).length, 0);
assert.throws(() => tw.parseLayerQuads(null, 0), error => error.name === 'RangeError');

// Parallax screen sizing:
// - para = 100 matches interfaceRect() at every zoom so clicks/markers/scissors align;
// - para = 0 stays locked to CalcScreenParams (<= 1500 x 1050) even when zoom < 1;
// - paraZoom from CMapItemGroupEx overrides max(paraX, paraY).
tw.mapScreen = [0, 0, 0, 0];
tw.worldView = [1000, 1000];
tw.aspect = 16 / 9;
tw.cameraPos = [14000, 1024];
for (const zoom of [1, 1000 / 1400, 0.125, 4]) {
  tw.cameraZoom = zoom;
  const gameGrp = new tw.Map.Group(null, { parallaxX: 100, parallaxY: 100, offsX: 0, offsY: 0, version: 3 });
  gameGrp.initMapScreen();
  const iface = tw.interfaceRect();
  assert.ok(Math.abs((tw.mapScreen[2] - tw.mapScreen[0]) - iface.w) < 1e-6);
  assert.ok(Math.abs((tw.mapScreen[3] - tw.mapScreen[1]) - iface.h) < 1e-6);

  const skyGrp = new tw.Map.Group(null, { parallaxX: 0, parallaxY: 0, offsX: 0, offsY: 0, version: 3 });
  skyGrp.initMapScreen();
  const ddBase = tw.calcScreenParams(tw.aspect);
  assert.ok(Math.abs((tw.mapScreen[2] - tw.mapScreen[0]) - ddBase.w) < 1e-6);
  assert.ok(Math.abs((tw.mapScreen[3] - tw.mapScreen[1]) - ddBase.h) < 1e-6);
  assert.ok(ddBase.w <= 1500 && ddBase.h <= 1050);
}
const exGrp = new tw.Map.Group(null, { parallaxX: 50, parallaxY: 50, parallaxZoom: 0, offsX: 0, offsY: 0, version: 3 });
tw.cameraZoom = 0.25;
exGrp.initMapScreen();
assert.ok(Math.abs((tw.mapScreen[2] - tw.mapScreen[0]) - tw.calcScreenParams(tw.aspect).w) < 1e-6);

// Envelope evaluation preserves caller defaults on unused channels and supports DDNet curves.
const envMap = {
  envelopes: [
    { version: 2, channels: 3, start: 0, num: 2 },
    { version: 2, channels: 4, start: 2, num: 0 },
  ],
  envelopePoints: [
    { time: 0, curve: tw.CURVETYPE_SMOOTH, values: [0, 0, 0, -842150451] },
    { time: 1000, curve: tw.CURVETYPE_LINEAR, values: [10240, 0, 184320, -842150451] },
  ],
};
const posOut = [0, 0, 0, 0];
tw.evalEnvelope(envMap, 0, 500, posOut);
assert.ok(Math.abs(posOut[0] - 5) < 1e-6);
assert.ok(Math.abs(posOut[2] - 90) < 1e-6);
assert.equal(posOut[3], 0);
const emptyEnvOut = [1, 1, 1, 1];
tw.evalEnvelope(envMap, 1, 0, emptyEnvOut);
assert.deepEqual(emptyEnvOut, [0, 0, 0, 0]);

// Full integration check against real maps (when present in checkout):
// 1. TwDataFile.getItem strips the 8-byte item header;
// 2. Textureless quad layers (image === -1, e.g. sky & streetlamp light cones in maps/main.map)
//    never inherit textures from earlier layers;
// 3. NPOT background textures always configure gl.REPEAT + mipmaps (both WebGL2 and WebGL1);
// 4. Rhythm maps with numQuads === 0 placeholder buffers parse without RangeError.
function createMockRendererGl(isWebGL2) {
  let texSeq = 0;
  const mock = {
    ARRAY_BUFFER: 1, ELEMENT_ARRAY_BUFFER: 2, STATIC_DRAW: 3, FLOAT: 4, UNSIGNED_SHORT: 5,
    TRIANGLES: 6, LINES: 7, TEXTURE_2D: 8, RGBA: 9, UNSIGNED_BYTE: 10,
    TEXTURE_MAG_FILTER: 11, TEXTURE_MIN_FILTER: 12, TEXTURE_WRAP_S: 13, TEXTURE_WRAP_T: 14,
    LINEAR: 15, LINEAR_MIPMAP_NEAREST: 16, REPEAT: 17, CLAMP_TO_EDGE: 18,
    SCISSOR_TEST: 29, MAX_TEXTURE_SIZE: 30,
    textures: [],
    enable() {}, disable() {}, scissor() {},
    createBuffer: () => ({}), bindBuffer() {}, bufferData() {},
    createTexture() { const t = { id: ++texSeq, params: {}, mipmap: false }; mock.textures.push(t); return t; },
    bindTexture(target, tex) { mock._bound = tex; },
    texImage2D(target, level, internalFmt, w, h) {
      if (mock._bound && typeof w === 'number') { mock._bound.upW = w; mock._bound.upH = h; }
    },
    texParameteri(target, pname, param) { if (mock._bound) mock._bound.params[pname] = param; },
    generateMipmap() { if (mock._bound) mock._bound.mipmap = true; },
    getParameter(p) { return p === 30 ? 4096 : 0; },
    uniform4fv() {}, uniform1i() {}, uniformMatrix4fv() {},
    enableVertexAttribArray() {}, disableVertexAttribArray() {}, vertexAttribPointer() {},
    drawArrays() {}, drawElements() {},
  };
  if (isWebGL2) mock.texStorage2D = () => {};
  return mock;
}

context.Image = function MockImage() {
  Object.defineProperty(this, 'src', {
    set(v) { this._src = v; },
  });
};
context.document = {
  createElement() {
    return {
      width: 1,
      height: 1,
      getContext: () => ({ drawImage() {} }),
    };
  },
};

const mainMapPath = path.join(repoRoot, 'maps/main.map');
if (fs.existsSync(mainMapPath)) {
  const rawMain = fs.readFileSync(mainMapPath);
  const mapAb = vm.runInContext(`new Uint8Array(${rawMain.length})`, context);
  mapAb.set(rawMain);

  const df = new TwDataFile('main', mapAb.buffer);
  assert.equal(df.parse(), true);
  const envPtsItem = df.findItem(tw.MAPITEMTYPE_ENVPOINTS, 0);
  assert.equal(envPtsItem.data.byteLength, envPtsItem.size);
  assert.equal(df.findItem(0x9999, 0), undefined);

  for (const isGl2 of [true, false]) {
    const mockGl = createMockRendererGl(isGl2);
    tw.gl = mockGl;
    tw.isWebGL2 = isGl2;
    tw.stdShader = {
      vPosAttr: 0, vVertexColor: 1, vTexCoordAttr: 2,
      pMatUni: {}, mvMatUni: {}, uSampler: {}, uColorMask: {}, uTexCoord: {}, uVertexColor: {},
    };
    tw.requestRender = () => {};
    const parsedMap = new tw.Map(mapAb.buffer);
    if (isGl2) {
      tw.map = parsedMap;
      const sourceLayer = MapPickerMapEdit.locateGameLayer(tw, parsedMap.dataFile).tileLayer;
      const sourceTileBytes = new Uint8Array(parsedMap.dataFile.getData(sourceLayer.data));
      const sourceRecords = MapPickerMapEdit.readTileRecords(sourceTileBytes, sourceLayer.width * sourceLayer.height, sourceLayer.version);
      const sourceTiles = tw.parseGameplayIndices(parsedMap.dataFile.getData(sourceLayer.data), sourceLayer.width * sourceLayer.height, sourceLayer.version);
      const emptyCell = sourceTiles.findIndex((tile) => tile === 0);
      assert.ok(emptyCell >= 0, 'main.map should contain an empty game tile for the writer round-trip');
      const x = emptyCell % sourceLayer.width, y = Math.floor(emptyCell / sourceLayer.width);
      const patchedBytes = MapPickerMapEdit.patch(tw, [{ x, y, tile: 209 }]);
      const patchedDataFile = new TwDataFile('patched-main', patchedBytes.buffer);
      assert.equal(patchedDataFile.parse(), true, 'serialized datafile must parse');
      const patchedLayer = MapPickerMapEdit.locateGameLayer(tw, patchedDataFile).tileLayer;
      const patchedTiles = tw.parseGameplayIndices(patchedDataFile.getData(patchedLayer.data), patchedLayer.width * patchedLayer.height, patchedLayer.version);
      assert.equal(patchedTiles[emptyCell], 209, 'game tile writer must preserve the changed entity ID');
      assert.equal(patchedTiles.length, sourceTiles.length, 'game tile count must survive serialization');
      const patchedRecords = MapPickerMapEdit.readTileRecords(
        new Uint8Array(patchedDataFile.getData(patchedLayer.data)), patchedLayer.width * patchedLayer.height, patchedLayer.version,
      );
      assert.equal(patchedRecords.length, sourceRecords.length, 'tile attributes must retain their full record count');
      for (let i = 0; i < sourceRecords.length; i++) {
        assert.equal(patchedRecords[i * 3 + 1], sourceRecords[i * 3 + 1], `tile flags must remain unchanged at ${i}`);
        assert.equal(patchedRecords[i * 3 + 2], sourceRecords[i * 3 + 2], `reserved tile byte must remain unchanged at ${i}`);
        if (i !== emptyCell) assert.equal(patchedRecords[i * 3], sourceRecords[i * 3], `unselected tile ID must remain unchanged at ${i}`);
      }
      assert.equal(patchedDataFile.numRawData, parsedMap.dataFile.numRawData, 'writer must not add or drop map resources');
      for (let i = 0; i < parsedMap.dataFile.numRawData; i++) {
        if (i !== sourceLayer.data) {
          assert.deepEqual(
            new Uint8Array(patchedDataFile.getData(i)), new Uint8Array(parsedMap.dataFile.getData(i)),
            `non-game map resource ${i} must be byte-for-byte preserved after decompression`,
          );
        }
      }
    }
    for (const tex of parsedMap.textures) {
      if (tex.image && tex.image.onload) {
        tex.image.naturalWidth = tex.width;
        tex.image.naturalHeight = tex.height;
        tex.image.onload();
      }
      assert.equal(tex.glTex.params[mockGl.TEXTURE_WRAP_S], mockGl.REPEAT, `WRAP_S must be REPEAT for ${tex.name}`);
      assert.equal(tex.glTex.params[mockGl.TEXTURE_WRAP_T], mockGl.REPEAT, `WRAP_T must be REPEAT for ${tex.name}`);
      assert.equal(tex.glTex.mipmap, true, `Mipmap must be generated for ${tex.name}`);
      if (!isGl2 && (!tw.isPowerOfTwo(tex.width) || !tw.isPowerOfTwo(tex.height)) && !tex.image) {
        assert.equal(tw.isPowerOfTwo(tex.glTex.upW), true);
        assert.equal(tw.isPowerOfTwo(tex.glTex.upH), true);
      }
    }
    // Group 0 (sky gradient) and the 173-quad "Lights" layer are untextured (image == -1).
    assert.equal(parsedMap.groups[0].layers[0].texture, undefined);
    assert.equal(parsedMap.groups[0].layers[1].texture, undefined);
    const allLayers = parsedMap.groups.flatMap(g => g.layers);
    const lightLayer = allLayers.find(l => l.type === tw.LAYERTYPE_QUADS && l.quads.length === 173);
    assert.ok(lightLayer, 'Expected 173-quad Lights layer in maps/main.map');
    assert.equal(lightLayer.texture, undefined);
  }
}

const emptyQuadMapPath = path.join(repoRoot, 'maps/rhythm/GHOSTEMANE - Mercury.map');
if (fs.existsSync(emptyQuadMapPath)) {
  const rawRhythm = fs.readFileSync(emptyQuadMapPath);
  const rhythmAb = vm.runInContext(`new Uint8Array(${rawRhythm.length})`, context);
  rhythmAb.set(rawRhythm);
  const mockGl = createMockRendererGl(true);
  tw.gl = mockGl;
  tw.isWebGL2 = true;
  const rhythmMap = new tw.Map(rhythmAb.buffer);
  assert.ok(rhythmMap.groups.length > 0);
}


// ── Правка карты: игровой и switch-слой ─────────────────────────────────────
// Черновик правок — чистая модель: добавление, перенос, удаление и откат.
const Tiles = (await import('node:module')).createRequire(import.meta.url)('../vendor/ddnet-map/picker-tiles.js');
{
  const cells = { game: [209, 0, 0, 18], switched: [] };
  const draft = Tiles.createDraft({
    sizes: { game: { width: 4, height: 1 } },
    read: (layer, index) => (layer === 'game' ? cells.game[index] : null),
  });
  assert.equal(draft.set('game', 1, 0, 209), true, 'place a point into an empty cell');
  assert.deepEqual(draft.changes().game, [{ x: 1, y: 0, tile: 209, from: 0 }]);
  assert.equal(draft.move('game', { x: 0, y: 0 }, { x: 2, y: 0 }), true, 'move a point into an empty cell');
  assert.deepEqual(
    draft.changes().game.map((c) => [c.x, c.tile]).sort((a, b) => a[0] - b[0]),
    [[0, 0], [1, 209], [2, 209]],
  );
  assert.equal(draft.move('game', { x: 2, y: 0 }, { x: 3, y: 0 }), false, 'occupied target is rejected');
  // Перенос пишет и исходное значение: сервер сверяет его со своим allowlist.
  const movedCell = draft.changes().game.find((change) => change.x === 0);
  assert.deepEqual([movedCell.tile, movedCell.from], [0, 209]);
  assert.equal(draft.set('game', 3, 0, 0), true, 'delete a point');
  assert.equal(draft.list('game').length, 4);
  // Возврат клетки к исходному значению убирает её из плана.
  draft.restore('game', 1, 0);
  assert.equal(draft.changes().game.some((change) => change.x === 1 && change.tile === 209), false);
  draft.reset();
  assert.equal(draft.size, 0, 'reset drops every edit');
  assert.equal(Tiles.normalizeGame(300), 0, 'out-of-range tile ids are ignored');
  assert.deepEqual(Tiles.normalizeSwitch({ number: 5, type: 200 }), { number: 5, type: 200 });
  assert.equal(Tiles.normalizeSwitch({ number: 5, type: 0 }), null, 'type 0 means an empty switch cell');
}

// Байтовый писатель: switch-слой правится отдельно от игрового, а правки
// соседних ресурсов не появляются.
const switchMapPath = path.join(repoRoot, 'maps/main.map');
if (fs.existsSync(switchMapPath)) {
  const rawSwitchMap = fs.readFileSync(switchMapPath);
  const switchAb = vm.runInContext(`new Uint8Array(${rawSwitchMap.length})`, context);
  switchAb.set(rawSwitchMap);
  const switchFile = new TwDataFile('main-switch', switchAb.buffer);
  assert.equal(switchFile.parse(), true);
  const found = MapPickerMapEdit.locateLayers(tw, switchFile);
  assert.ok(found.game, 'main.map must expose a gameplay layer');
  assert.ok(found.switch, 'main.map must expose a switch layer');
  const switchSize = found.switch.tileLayer;
  const cells = switchSize.width * switchSize.height;
  const source = new Uint8Array(switchFile.getData(found.switch.rawIndex));
  const records = MapPickerMapEdit.readSwitchRecords(source, cells);
  assert.equal(records.length, cells * 4, 'switch records are four bytes per cell');
  // Ищем занятую руду и свободную клетку, затем переносим первую во вторую.
  const gameTiles = tw.parseGameplayIndices(switchFile.getData(found.game.rawIndex),
    found.game.tileLayer.width * found.game.tileLayer.height, found.game.tileLayer.version);
  let ore = -1, empty = -1;
  for (let i = 0; i < cells; i++) {
    if (ore < 0 && records[i * 4 + 1] === 200 && gameTiles[i] === 0) ore = i;
    if (empty < 0 && records[i * 4 + 1] === 0 && gameTiles[i] === 0) empty = i;
  }
  assert.ok(ore >= 0 && empty >= 0, 'main.map needs an ore cell and an empty cell');
  const out = MapPickerMapEdit.patch(tw, {
    switch: [
      { x: ore % switchSize.width, y: Math.floor(ore / switchSize.width), number: 0, type: 0 },
      { x: empty % switchSize.width, y: Math.floor(empty / switchSize.width), number: 77, type: 200 },
    ],
  }, switchFile);
  const patched = new TwDataFile('main-switch-out', out.buffer.slice(out.byteOffset, out.byteOffset + out.byteLength));
  assert.equal(patched.parse(), true, 'patched map must parse');
  const patchedLayers = MapPickerMapEdit.locateLayers(tw, patched);
  const patchedSwitch = new Uint8Array(patched.getData(patchedLayers.switch.rawIndex));
  assert.equal(patchedSwitch[ore * 4 + 1], 0, 'the source ore cell is cleared');
  assert.equal(patchedSwitch[empty * 4], 77, 'the target cell keeps the switch number');
  assert.equal(patchedSwitch[empty * 4 + 1], 200, 'the target cell keeps the ore type');
  // Игровой блок не менялся, остальные ресурсы — байт в байт.
  for (let i = 0; i < switchFile.numRawData; i++) {
    const before = Buffer.from(new Uint8Array(switchFile.getData(i)));
    const after = Buffer.from(new Uint8Array(patched.getData(i)));
    if (i === patchedLayers.switch.rawIndex) continue;
    assert.ok(before.equals(after), `raw block ${i} must stay untouched by a switch edit`);
  }
  // Коллизии игрового слоя правками не трогаются.
  assert.throws(() => MapPickerMapEdit.patch(tw, { game: [{ x: 0, y: 0, tile: 1 }] }, switchFile),
    /collision layer/, 'collision tiles are read-only');
}

console.log('Map picker clipping/culling, visibility, parallax, textures and quad parsing: ALL PASSED');
