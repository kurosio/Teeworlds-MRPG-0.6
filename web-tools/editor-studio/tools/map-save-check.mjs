#!/usr/bin/env node
/** Проверка сохранения карты (api/map-edit.php + vendor/ddnet-map/picker-map-edit.js).
 *
 * Что проверяется БЕЗ PHP и без записи на диск:
 *
 *   1. sha256 в picker-map-edit.js совпадает с node:crypto (клиент шлёт
 *      контрольную сумму исходной карты в X-Map-Original-Sha256);
 *   2. собирается план правок ровно так, как его строит picker.js по черновику
 *      ({ game: [{ x, y, tile, from }], switch: [{ x, y, number, type, from }] });
 *   3. patch() → разобранный заново датафайл: структура (заголовок, таблицы
 *      типов/элементов/ресурсов) байт в байт, у изменённых блоков сходятся
 *      развёрнутые размеры, остальные ресурсы — байт в байт после распаковки;
 *   4. игровой слой: меняется только байт индекса тайла — флаги и резерв
 *      остаются, коллизии (1–3) не трогаются; switch-слой: флаги и delay
 *      остаются у каждой клетки;
 *   5. цепочка «сохранить → перечитать → сохранить снова» (reload) — второй
 *      цикл считается уже от записанной карты;
 *   6. сжатые блоки, которые пишет Zlib.Deflate (zlib-обёртка), свободно
 *      разворачиваются обратно — именно это делает сервер через gzuncompress.
 *
 * Это зеркалит проверки api/map-edit.php (map_edit_validate_datafile и
 * map_edit_assert_tile_only_change): если файл проходит здесь, сервер его
 * примет; если нет — сохранение упадёт по 400 ещё до записи на диск.
 *
 * Использование:
 *   node tools/map-save-check.mjs [путь/к/карте.map]   (по умолчанию maps/main.map)
 */
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const repoRoot = path.resolve(root, '../..');
const mapPath = path.resolve(process.argv[2] || path.join(repoRoot, 'maps/main.map'));
if (!fs.existsSync(mapPath)) {
  console.log(`skip: map not found at ${mapPath}`);
  process.exit(0);
}

const read = (name) => fs.readFileSync(path.join(root, 'vendor/ddnet-map', name), 'utf8');
// Тот же мост поверх node:zlib, что и в map-picker-check.mjs: отдаём
// браузерному коду zlib-совместимые Inflate/Deflate.
const Zlib = {
  Inflate: function (compressed) {
    this.decompress = () => new Uint8Array(zlib.inflateSync(Buffer.from(compressed)));
  },
  Deflate: { compress: (plain) => new Uint8Array(zlib.deflateSync(Buffer.from(plain))) },
};
const window = {
  twConfig: {},
  location: { href: 'http://localhost/picker.html', origin: 'http://localhost' },
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
for (const name of ['dataviewext.js', 'twdatafile.js', 'twwebgl.js', 'picker-map-edit.js']) {
  vm.runInContext(read(name), context, { filename: name });
}
const { tw, TwDataFile } = context;
const MapPickerMapEdit = window.MapPickerMapEdit;

const raw = fs.readFileSync(mapPath);
const source = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength);
console.log(`map: ${path.relative(repoRoot, mapPath)} (${(raw.length / 1024 / 1024).toFixed(2)} MiB)`);

const checkHeader = (bytes) => {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const length = bytes.byteLength;
  const u32 = (offset) => view.getUint32(offset, true);
  assert.equal(Buffer.from(bytes.subarray(0, 4)).toString('latin1'), 'DATA', 'signature');
  const version = u32(4);
  const size = u32(8);
  const swapLen = u32(12);
  const numTypes = u32(16), numItems = u32(20), numData = u32(24);
  const itemSize = u32(28), dataSize = u32(32);
  assert.equal(version, 4, 'only datafile v4 can be saved');
  assert.equal(size, length - 16, 'header size must equal file length - 16');
  assert(numTypes <= 65536 && numItems <= 2000000 && numData <= 100000, 'table counts are sane');
  assert(itemSize <= length && dataSize <= length, 'table sizes are sane');
  const typesStart = 36;
  const itemOffsetsStart = typesStart + numTypes * 12;
  const dataOffsetsStart = itemOffsetsStart + numItems * 4;
  const dataSizesStart = dataOffsetsStart + numData * 4;
  const itemStart = dataSizesStart + numData * 4;
  const dataStart = itemStart + itemSize;
  assert.equal(swapLen, dataStart - 16, 'swapLen must match the data section');
  assert.equal(dataStart + dataSize, length, 'data section must end at EOF');
  return { numTypes, numItems, numData, itemSize, itemOffsetsStart, dataOffsetsStart, dataSizesStart, itemStart, dataStart, dataSize, length };
};

// Распаковывает все raw-блоки и сверяет их с ожиданием: тот же алгоритм, что
// у сервера (gzuncompress + сравнение длин из таблицы размеров).
const inflateBlocks = (bytes, hc) => {
  const blocks = [];
  let offset = -1;
  let total = 0;
  for (let i = 0; i < hc.numData; i++) {
    const start = view(bytes).getUint32(hc.dataOffsetsStart + i * 4, true);
    const plainSize = view(bytes).getUint32(hc.dataSizesStart + i * 4, true);
    const next = i + 1 < hc.numData ? view(bytes).getUint32(hc.dataOffsetsStart + (i + 1) * 4, true) : hc.dataSize;
    assert(start >= 0 && start >= offset && next >= start && next <= hc.dataSize, `raw block ${i} offsets`);
    const compressed = bytes.subarray(hc.dataStart + start, hc.dataStart + next);
    const plain = zlib.inflateSync(Buffer.from(compressed)); // бросит при повреждении
    assert.equal(plain.length, plainSize, `raw block ${i}: inflated length must match the size table`);
    blocks.push({ compressed, plain: new Uint8Array(plain) });
    total += plain.length;
    offset = start;
  }
  // Тот же потолок, что MAP_EDIT_MAX_UNCOMPRESSED в api/map-edit.php: 512 MiB.
  assert(total <= 512 * 1024 * 1024, 'uncompressed data stays within the server limit');
  return blocks;
};
const sameBytes = (a, b) => Buffer.from(a).equals(Buffer.from(b));
const view = (bytes) => new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);

const gameRecords = (bytes) => {
  const file = new TwDataFile('check', bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength));
  assert.equal(file.parse(), true);
  const layers = MapPickerMapEdit.locateLayers(tw, file);
  assert(layers.game, 'patched map must keep a gameplay layer');
  const { tileLayer } = layers.game;
  const count = tileLayer.width * tileLayer.height;
  const records = MapPickerMapEdit.readTileRecords(new Uint8Array(file.getData(layers.game.rawIndex)), count, tileLayer.version);
  return { file, layers, tileLayer, records, count };
};
const switchRecords = (bytes) => {
  const file = new TwDataFile('check', bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength));
  assert.equal(file.parse(), true);
  const layers = MapPickerMapEdit.locateLayers(tw, file);
  assert(layers.switch, 'patched map must keep a switch layer');
  const { tileLayer } = layers.switch;
  const count = tileLayer.width * tileLayer.height;
  const records = MapPickerMapEdit.readSwitchRecords(new Uint8Array(file.getData(layers.switch.rawIndex)), count);
  return { file, layers, tileLayer, records, count };
};

// ── 1. Контрольная сумма клиента совпадает с серверной ─────────────────────
assert.equal(MapPickerMapEdit.sha256(new Uint8Array(source)),
  crypto.createHash('sha256').update(raw).digest('hex'),
  'client sha256 must match the digest a server computes from the same file');
console.log('1. sha256 клиента == sha256 node/crypto (X-Map-Original-Sha256)');

// ── 2. Исходная карта разбирается, слои находятся ──────────────────────────
const file = new TwDataFile('source', source);
assert.equal(file.parse(), true);
const layers = MapPickerMapEdit.locateLayers(tw, file);
assert(layers.game, 'a gameplay layer is required');
const gameLayer = layers.game.tileLayer;
const switchLayer = layers.switch?.tileLayer || null;
const gameCount = gameLayer.width * gameLayer.height;
const gameIndices = tw.parseGameplayIndices(file.getData(layers.game.rawIndex), gameCount, gameLayer.version);
const sourceHc = checkHeader(new Uint8Array(source));
const sourceBlocks = inflateBlocks(new Uint8Array(source), sourceHc);
console.log(`2. карта разобрана: gameplay ${gameLayer.width}x${gameLayer.height}, switch ${switchLayer ? `${switchLayer.width}x${switchLayer.height}` : 'нет'}, блоков ${sourceHc.numData}`);

// ── 3. Клетки для проверки: пустая игровая + пустая switch ─────────────────
let emptyGame = -1;
for (let i = 0; i < gameCount; i++) { if (gameIndices[i] === 0) { emptyGame = i; break; } }
assert(emptyGame >= 0, 'map needs at least one empty gameplay cell to test an add');
const gx = emptyGame % gameLayer.width, gy = Math.floor(emptyGame / gameLayer.width);
let emptySwitch = -1;
if (switchLayer) {
  const switchCells = new Uint8Array(file.getData(layers.switch.rawIndex));
  const switchCount = switchLayer.width * switchLayer.height;
  for (let i = 0; i < switchCount; i++) {
    if (switchCells[i * 4] === 0 && switchCells[i * 4 + 1] === 0) { emptySwitch = i; break; }
  }
}
const sx = emptySwitch >= 0 ? emptySwitch % switchLayer.width : 0;
const sy = emptySwitch >= 0 ? Math.floor(emptySwitch / switchLayer.width) : 0;

// План строится ровно так, как его собирает picker.js из черновика.
const plan = {
  game: [{ x: gx, y: gy, tile: 209, from: gameIndices[emptyGame] || 0 }],
  switch: emptySwitch >= 0 ? [{ x: sx, y: sy, number: 77, type: 200, from: { number: 0, type: 0 } }] : [],
};
console.log(`3. план: game (${gx}, ${gy}) → 209${plan.switch.length ? `, switch (${sx}, ${sy}) → #77 тип 200` : ' (в карте нет switch-слоя)'}`);

// ── 4. Первый прогон: патчим, разбираем, сверяем с правилами сервера ───────
const patched = MapPickerMapEdit.patch(tw, plan, file);
const patchedBytes = new Uint8Array(patched);
checkHeader(patchedBytes);
const patchedHc = checkHeader(patchedBytes);
const patchedBlocks = inflateBlocks(patchedBytes, patchedHc);
assert.equal(patchedHc.numData, sourceHc.numData, 'raw block count must not change');
assert.equal(patchedHc.numItems, sourceHc.numItems, 'item count must not change');
assert.equal(patchedHc.numTypes, sourceHc.numTypes, 'type count must not change');
assert.equal(patchedHc.itemSize, sourceHc.itemSize, 'item table size must not change');
// Таблицы — байт в байт (через Buffer: Uint8Array не умеет .equals).
const table = (bytes, from, to) => Buffer.from(bytes.buffer, bytes.byteOffset + from, to - from);
for (const [from, to, what] of [
  [36, patchedHc.itemOffsetsStart, 'item types'],
  [patchedHc.itemOffsetsStart, patchedHc.dataOffsetsStart, 'item offsets'],
  [patchedHc.dataSizesStart + patchedHc.numData * 4, patchedHc.dataStart, 'items'],
]) {
  assert(table(new Uint8Array(source), from, to).equals(table(patchedBytes, from, to)), `${what} must stay byte-identical`);
}
// Все ресурсы, кроме правленных блоков, — байт в байт после распаковки.
const edited = new Set([layers.game.rawIndex, ...(plan.switch.length ? [layers.switch.rawIndex] : [])]);
for (let i = 0; i < sourceBlocks.length; i++) {
  if (edited.has(i)) continue;
  assert(sameBytes(sourceBlocks[i].plain, patchedBlocks[i].plain), `raw block ${i} must stay identical`);
  assert(sameBytes(sourceBlocks[i].compressed, patchedBlocks[i].compressed), `compressed raw block ${i} must stay identical`);
}
// Игровой слой: только байт индекса меняется.
const beforeGame = gameRecords(new Uint8Array(source));
const afterGame = gameRecords(patchedBytes);
assert.equal(afterGame.records[emptyGame * 3], 209, 'the staged entity lands in the cell');
assert.equal(afterGame.count, beforeGame.count, 'gameplay cell count must not change');
for (let i = 0; i < beforeGame.count; i++) {
  assert.equal(afterGame.records[i * 3 + 1], beforeGame.records[i * 3 + 1], `flags must stay at cell ${i}`);
  assert.equal(afterGame.records[i * 3 + 2], beforeGame.records[i * 3 + 2], `reserved byte must stay at cell ${i}`);
  if (i !== emptyGame) assert.equal(afterGame.records[i * 3], beforeGame.records[i * 3], `tile id must stay at cell ${i}`);
}
// Switch-слой: флаги и delay не трогали.
if (plan.switch.length) {
  const beforeSwitch = switchRecords(new Uint8Array(source));
  const afterSwitch = switchRecords(patchedBytes);
  assert.equal(afterSwitch.records[emptySwitch * 4], 77, 'switch number lands');
  assert.equal(afterSwitch.records[emptySwitch * 4 + 1], 200, 'switch type lands');
  assert.equal(afterSwitch.count, beforeSwitch.count, 'switch cell count must not change');
  for (let i = 0; i < beforeSwitch.count; i++) {
    assert.equal(afterSwitch.records[i * 4 + 2], beforeSwitch.records[i * 4 + 2], `switch flags must stay at cell ${i}`);
    assert.equal(afterSwitch.records[i * 4 + 3], beforeSwitch.records[i * 4 + 3], `switch delay must stay at cell ${i}`);
    if (i !== emptySwitch) {
      assert.equal(afterSwitch.records[i * 4], beforeSwitch.records[i * 4], `switch number must stay at cell ${i}`);
      assert.equal(afterSwitch.records[i * 4 + 1], beforeSwitch.records[i * 4 + 1], `switch type must stay at cell ${i}`);
    }
  }
}
console.log('4. патч прошёл сверку: структура и чужие ресурсы байт в байт, флаги целы');

// ── 5. reload-цепочка: сохранили → перечитали → правим дальше ──────────────
// Именно так делает picker.js: после записи окно заменяет разобранный
// датафайл на только что сохранённые байты.
tw.map = { dataFile: file };
const reloaded = MapPickerMapEdit.reload(tw, patchedBytes);
assert(reloaded, 'reload() must return the re-parsed map');
assert(tw.map.dataFile === reloaded, 'reload() must swap the parsed datafile');
const backPlan = {
  game: [{ x: gx, y: gy, tile: 0, from: 209 }],
  switch: emptySwitch >= 0 ? [{ x: sx, y: sy, number: 0, type: 0, from: { number: 77, type: 200 } }] : [],
};
const patchedBack = MapPickerMapEdit.patch(tw, backPlan, reloaded);
const back = gameRecords(new Uint8Array(patchedBack));
assert.equal(back.records[emptyGame * 3], 0, 'the second save starts from the already-saved bytes');
if (emptySwitch >= 0) {
  const backSwitch = switchRecords(new Uint8Array(patchedBack));
  assert.equal(backSwitch.records[emptySwitch * 4 + 1], 0, 'the switch cell is cleared on the second save');
}
console.log('5. reload()-цепочка: вторая правка считается от сохранённой карты');

// ── 6. Запрещённые правки отклоняются до отправки ───────────────────────────
assert.throws(() => MapPickerMapEdit.patch(tw, { game: [{ x: 0, y: 0, tile: 1 }] }, file),
  /collision layer/, 'collision tiles are read-only');
assert.throws(() => MapPickerMapEdit.patch(tw, { game: [{ x: gx, y: gy, tile: 300 }] }, file),
  /not a valid tile index/, 'out-of-range tile ids throw');
console.log('6. невалидные правки отклоняются писателем до отправки на сервер');

console.log('\nMap save round-trip: ALL CHECKS PASSED');
