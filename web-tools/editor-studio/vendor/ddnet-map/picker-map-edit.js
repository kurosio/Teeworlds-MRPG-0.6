/* Байтовый писатель карты DDNet v4: игровой слой и switch-слой.
 *
 * Карта правится не «целиком», а двумя независимыми таблицами клеток:
 *
 *   game   — CTile (index, flags, reserved) в raw-блоке игрового слоя.
 *            В v4 индексы сжаты повторами (m_Skip), поэтому слой сначала
 *            разворачивается в 3 байта на клетку, а потом сжимается обратно.
 *   switch — CSwitchTileExtra (number, type, flags, delay), ровно 4 байта на
 *            клетку без RLE. Живёт в отдельном raw-блоке (m_Switch), который
 *            находит locateLayers: у слоя стоит флаг 16 и m_Switch >= 0.
 *            Типы 199/200/201 — узлы сбора (растения/руда/рыба), number —
 *            номер switch.
 *
 * Всё остальное в датафайле остаётся байт в байт: serializeDataFile меняет
 * только указанные raw-блоки, а таблицы типов, элементов и смещений
 * пересобираются из исходника.
 */
((root) => {
  'use strict';

  const MAX_BYTES = 48 * 1024 * 1024;
  const MAX_CELLS = 4_000_000;
  const TILE_STRIDE = 3;   // index, flags, reserved
  const SWITCH_STRIDE = 4; // number, type, flags, delay
  const COLLISION_MAX = 3; // 1..3 — стена/смерть/без крюка: их не правим никогда

  const writeU32 = (view, offset, value) => view.setUint32(offset, value >>> 0, true);

  const assertCount = (count) => {
    if (!Number.isSafeInteger(count) || count < 0 || count > MAX_CELLS) {
      throw new Error('Tile layer data is misaligned or too large.');
    }
  };

  /** CTile (index, flags, m_Skip, reserved) → 3 байта на клетку. */
  const readTileRecords = (bytes, count, version) => {
    assertCount(count);
    if (!bytes || bytes.byteLength % 4 !== 0) {
      throw new Error('Tile layer data is misaligned or too large.');
    }
    const records = new Uint8Array(count * TILE_STRIDE);
    if (version >= 4) {
      let dst = 0;
      for (let src = 0; src < bytes.length && dst < count; src += 4) {
        const repetitions = Math.min(bytes[src + 2] + 1, count - dst);
        for (let n = 0; n < repetitions; n++, dst++) {
          records[dst * 3] = bytes[src]; records[dst * 3 + 1] = bytes[src + 1]; records[dst * 3 + 2] = bytes[src + 3];
        }
      }
      if (dst !== count) throw new Error('Game tile layer is truncated or malformed.');
    } else {
      if (bytes.length < count * 4) throw new Error('Game tile layer is truncated.');
      for (let i = 0; i < count; i++) {
        records[i * 3] = bytes[i * 4]; records[i * 3 + 1] = bytes[i * 4 + 1]; records[i * 3 + 2] = bytes[i * 4 + 3];
      }
    }
    return records;
  };

  /** Обратное сжатие повторами: одинаковые соседние клетки кодируются m_Skip. */
  const encodeTileRecords = (records, version) => {
    const count = records.length / TILE_STRIDE;
    if (!Number.isInteger(count)) throw new Error('Tile records are malformed.');
    if (version < 4) {
      const bytes = new Uint8Array(count * 4);
      for (let i = 0; i < count; i++) {
        bytes[i * 4] = records[i * 3]; bytes[i * 4 + 1] = records[i * 3 + 1];
        bytes[i * 4 + 2] = 0; bytes[i * 4 + 3] = records[i * 3 + 2];
      }
      return bytes;
    }
    const encoded = new Uint8Array(count * 4);
    let cursor = 0;
    for (let i = 0; i < count;) {
      let run = 1;
      while (run < 256 && i + run < count
        && records[(i + run) * 3] === records[i * 3]
        && records[(i + run) * 3 + 1] === records[i * 3 + 1]
        && records[(i + run) * 3 + 2] === records[i * 3 + 2]) run++;
      encoded[cursor++] = records[i * 3];
      encoded[cursor++] = records[i * 3 + 1];
      encoded[cursor++] = run - 1;
      encoded[cursor++] = records[i * 3 + 2];
      i += run;
    }
    return encoded.subarray(0, cursor);
  };

  /**
   * CSwitchTileExtra → 4 байта на клетку. Блок может быть длиннее (паддинг):
   * хвост сохраняется как есть, иначе повторное сохранение меняло бы размер
   * ресурса без причины.
   */
  const readSwitchRecords = (bytes, count) => {
    assertCount(count);
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes || []);
    if (view.byteLength < count * SWITCH_STRIDE) throw new Error('Switch tile layer is truncated.');
    return view.slice();
  };

  const encodeSwitchRecords = (records) => {
    if (!(records instanceof Uint8Array) || records.length % SWITCH_STRIDE !== 0) {
      throw new Error('Switch tile records are malformed.');
    }
    return records.slice();
  };

  const sha256 = (bytes) => {
    const constants = [
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
    ];
    const state = [0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
    const input = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    const paddedLength = Math.ceil((input.length + 9) / 64) * 64;
    const padded = new Uint8Array(paddedLength);
    padded.set(input); padded[input.length] = 0x80;
    new DataView(padded.buffer).setUint32(paddedLength - 4, input.length * 8, false);
    const words = new Uint32Array(64), rotate = (x, n) => (x >>> n) | (x << (32 - n));
    for (let offset = 0; offset < paddedLength; offset += 64) {
      const view = new DataView(padded.buffer, offset, 64);
      for (let i = 0; i < 16; i++) words[i] = view.getUint32(i * 4, false);
      for (let i = 16; i < 64; i++) {
        const a = words[i - 15], b = words[i - 2];
        const s0 = rotate(a, 7) ^ rotate(a, 18) ^ (a >>> 3);
        const s1 = rotate(b, 17) ^ rotate(b, 19) ^ (b >>> 10);
        words[i] = (words[i - 16] + s0 + words[i - 7] + s1) >>> 0;
      }
      let [a,b,c,d,e,f,g,h] = state;
      for (let i = 0; i < 64; i++) {
        const sum1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
        const choice = (e & f) ^ (~e & g);
        const t1 = (h + sum1 + choice + constants[i] + words[i]) >>> 0;
        const sum0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
        const majority = (a & b) ^ (a & c) ^ (b & c);
        const t2 = (sum0 + majority) >>> 0;
        h = g; g = f; f = e; e = (d + t1) >>> 0; d = c; c = b; b = a; a = (t1 + t2) >>> 0;
      }
      state[0]=(state[0]+a)>>>0; state[1]=(state[1]+b)>>>0; state[2]=(state[2]+c)>>>0; state[3]=(state[3]+d)>>>0;
      state[4]=(state[4]+e)>>>0; state[5]=(state[5]+f)>>>0; state[6]=(state[6]+g)>>>0; state[7]=(state[7]+h)>>>0;
    }
    return state.map((value) => value.toString(16).padStart(8, '0')).join('');
  };

  /** Смещение m_Switch в CMapItemLayerTilemap (DDNet: 84 для актуальных карт). */
  const switchOffset = (version) => (version <= 2 ? 72 : 84);

  /**
   * Находит raw-блоки игрового и switch-слоя. Возвращает `{ game, switch }`,
   * где каждая запись — `{ item, tileLayer, rawIndex }` или null. rawIndex —
   * номер блока в таблице данных (game — m_Data, switch — m_Switch).
   */
  const locateLayers = (tw, dataFile) => {
    const layers = dataFile?.getType?.(tw.MAPITEMTYPE_LAYER);
    if (!layers) throw new Error('This map has no layer table.');
    const gameFlag = tw.TILESLAYERFLAG_GAME ?? 1;
    const switchFlag = tw.TILESLAYERFLAG_SWITCH ?? 16;
    const found = { game: null, switch: null };
    for (let i = 0; i < layers.num; i++) {
      const item = dataFile.getItem(layers.start + i);
      if (!item?.data || item.data.byteLength < 60) continue;
      if (tw.parseMapLayer(item.data).type !== tw.LAYERTYPE_TILES) continue;
      const tileLayer = tw.parseMapLayerTiles(item.data);
      if (!Number.isInteger(tileLayer.width) || !Number.isInteger(tileLayer.height)
        || tileLayer.width <= 0 || tileLayer.height <= 0) continue;
      if (!found.game && tileLayer.image === -1 && (tileLayer.flags & gameFlag)
        && Number.isInteger(tileLayer.data) && tileLayer.data >= 0) {
        found.game = { item, tileLayer, rawIndex: tileLayer.data };
        continue;
      }
      if (!found.switch && (tileLayer.flags & switchFlag)
        && Number.isInteger(tileLayer.switchData) && tileLayer.switchData >= 0) {
        found.switch = { item, tileLayer, rawIndex: tileLayer.switchData };
      }
      if (found.game && found.switch) break;
    }
    return found;
  };

  const locateLayer = (tw, dataFile, kind) => {
    const layers = locateLayers(tw, dataFile);
    const layer = layers[kind === 'switch' ? 'switch' : 'game'];
    if (!layer) {
      throw new Error(kind === 'switch'
        ? 'This map has no switch tile layer.'
        : 'This map has no editable game tile layer.');
    }
    return layer;
  };

  /**
   * Пересобирает датафайл, заменяя указанные raw-блоки (`Map<index, bytes>`).
   * Все прочие блоки копируются сжатыми байт в байт, поэтому их контрольные
   * суммы на сервере не меняются.
   */
  const serializeDataFile = (dataFile, replacements) => {
    if (!dataFile || dataFile.version !== 4) throw new Error('Only DDNet datafile v4 maps can be edited.');
    if (!dataFile.fileData || dataFile.fileData.byteLength > MAX_BYTES) {
      throw new Error('This map exceeds the supported 48 MiB save limit.');
    }
    const replacementsMap = replacements instanceof Map
      ? replacements
      : new Map(Object.entries(replacements || {}).map(([key, value]) => [Number(key), value]));
    const original = new Uint8Array(dataFile.fileData);
    const typeCount = dataFile.numItemTypes, itemCount = dataFile.numItems, dataCount = dataFile.numRawData;
    const typeStart = 36;
    const itemOffsetsStart = typeStart + typeCount * 12;
    const dataOffsetsStart = itemOffsetsStart + itemCount * 4;
    const dataSizesStart = dataOffsetsStart + dataCount * 4;
    const itemStart = dataSizesStart + dataCount * 4;
    const dataStart = itemStart + dataFile.itemSize;
    if (dataStart > original.length) throw new Error('Map datafile indexes are invalid.');
    for (const index of replacementsMap.keys()) {
      if (!Number.isInteger(index) || index < 0 || index >= dataCount) throw new Error('Map datafile indexes are invalid.');
    }

    const rawBlobs = [];
    const offsets = [];
    const sizes = [];
    let packedDataSize = 0;
    for (let i = 0; i < dataCount; i++) {
      const info = dataFile.dataInfos[i];
      if (!info || !Number.isSafeInteger(info.offset) || !Number.isSafeInteger(info.compSize)
        || info.offset < 0 || info.compSize < 0 || dataStart + info.offset + info.compSize > original.length) {
        throw new Error(`Map raw-data block ${i} has invalid bounds.`);
      }
      const blob = replacementsMap.has(i)
        ? Zlib.Deflate.compress(replacementsMap.get(i))
        : original.slice(dataStart + info.offset, dataStart + info.offset + info.compSize);
      offsets.push(packedDataSize);
      sizes.push(replacementsMap.has(i) ? replacementsMap.get(i).byteLength : info.size);
      rawBlobs.push(blob);
      packedDataSize += blob.byteLength;
    }

    const typeBytes = original.slice(typeStart, itemOffsetsStart);
    const itemOffsetBytes = original.slice(itemOffsetsStart, dataOffsetsStart);
    const itemBytes = original.slice(itemStart, dataStart);
    const swapSize = 36 + typeBytes.length + itemOffsetBytes.length + dataCount * 8 + itemBytes.length;
    const fileSize = swapSize + packedDataSize;
    if (!Number.isSafeInteger(fileSize) || fileSize > MAX_BYTES || fileSize > 0xffffffff || packedDataSize > 0xffffffff) {
      throw new Error('The edited map exceeds the supported 48 MiB datafile limit.');
    }
    const header = new Uint8Array(36);
    header.set([68, 65, 84, 65]); // DATA
    const headerView = new DataView(header.buffer);
    [4, fileSize - 16, swapSize - 16, typeCount, itemCount, dataCount, dataFile.itemSize, packedDataSize]
      .forEach((value, index) => writeU32(headerView, 4 + index * 4, value));

    const output = new Uint8Array(fileSize);
    let cursor = 0;
    const append = (bytes) => { output.set(bytes, cursor); cursor += bytes.length; };
    append(header); append(typeBytes); append(itemOffsetBytes);
    const offsetTable = new Uint8Array(dataCount * 4), sizeTable = new Uint8Array(dataCount * 4);
    const offsetView = new DataView(offsetTable.buffer), sizeView = new DataView(sizeTable.buffer);
    offsets.forEach((value, i) => writeU32(offsetView, i * 4, value));
    sizes.forEach((value, i) => writeU32(sizeView, i * 4, value));
    append(offsetTable); append(sizeTable); append(itemBytes);
    rawBlobs.forEach(append);
    return output;
  };

  const cellIndex = (change, tileLayer, kind) => {
    const x = Number(change.x), y = Number(change.y);
    if (!Number.isInteger(x) || !Number.isInteger(y) || x < 0 || y < 0
      || x >= tileLayer.width || y >= tileLayer.height) {
      throw new Error(`A requested ${kind} tile edit is invalid.`);
    }
    return y * tileLayer.width + x;
  };

  const byteValue = (value, fallback = 0) => {
    const number = Number(value ?? fallback);
    if (!Number.isInteger(number) || number < 0 || number > 255) {
      throw new Error('A requested tile edit is invalid.');
    }
    return number;
  };

  /**
   * Правит клетки и возвращает готовый файл карты.
   *
   * `plan` — `{ game: [{ x, y, tile }], switch: [{ x, y, number, type, flags, delay }] }`;
   * `tile`/`type` равны 0 — удалить клетку. Для совместимости принимается и
   * плоский массив правок игрового слоя.
   */
  const patch = (tw, plan, dataFile = tw?.map?.dataFile) => {
    if (!dataFile?.fileData) throw new Error('This map cannot be edited in the picker.');
    if (dataFile.fileData.byteLength > MAX_BYTES) throw new Error('This map exceeds the supported 48 MiB save limit.');
    const edits = Array.isArray(plan) ? { game: plan } : (plan || {});
    const layers = locateLayers(tw, dataFile);
    const replacements = new Map();

    if (Array.isArray(edits.game) && edits.game.length) {
      if (!layers.game) throw new Error('This map has no editable game tile layer.');
      const { tileLayer } = layers.game;
      const count = tileLayer.width * tileLayer.height;
      if (count > MAX_CELLS) throw new Error('Game tile layer dimensions are invalid or too large.');
      const records = readTileRecords(new Uint8Array(dataFile.getData(layers.game.rawIndex)), count, tileLayer.version);
      for (const change of edits.game) {
        const index = cellIndex(change, tileLayer, 'game');
        const tile = byteValue(change.tile);
        if (tile > 0 && tile <= COLLISION_MAX) throw new Error(`Tile ${change.x}, ${change.y} is part of the collision layer.`);
        const current = records[index * TILE_STRIDE];
        if (current > 0 && current <= COLLISION_MAX && tile !== current) {
          throw new Error(`Tile ${change.x}, ${change.y} is part of the collision layer.`);
        }
        records[index * TILE_STRIDE] = tile;
      }
      replacements.set(layers.game.rawIndex, encodeTileRecords(records, tileLayer.version));
    }

    if (Array.isArray(edits.switch) && edits.switch.length) {
      if (!layers.switch) throw new Error('This map has no switch tile layer.');
      const { tileLayer } = layers.switch;
      const count = tileLayer.width * tileLayer.height;
      if (count > MAX_CELLS) throw new Error('Switch tile layer dimensions are invalid or too large.');
      const records = readSwitchRecords(new Uint8Array(dataFile.getData(layers.switch.rawIndex)), count);
      for (const change of edits.switch) {
        const index = cellIndex(change, tileLayer, 'switch');
        const cursor = index * SWITCH_STRIDE;
        records[cursor] = byteValue(change.number);
        records[cursor + 1] = byteValue(change.type);
        // Флаги и задержку меняют только явно: иначе редактор затирал бы
        // поведение switch-тайла, которое он не показывает.
        if (change.flags !== undefined) records[cursor + 2] = byteValue(change.flags);
        if (change.delay !== undefined) records[cursor + 3] = byteValue(change.delay);
      }
      replacements.set(layers.switch.rawIndex, encodeSwitchRecords(records));
    }

    if (!replacements.size) throw new Error('No tile edits were requested.');
    return serializeDataFile(dataFile, replacements);
  };

  /**
   * После успешного сохранения заменяет разобранный датафайл в `tw.map`:
   * следующая правка должна считаться от уже записанной карты, иначе она
   * отменила бы предыдущие изменения switch-слоя.
   */
  const reload = (tw, bytes) => {
    if (typeof TwDataFile !== 'function' || !tw?.map) return null;
    const buffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    const file = new TwDataFile(tw.map.dataFile?.mapName || 'map', buffer);
    if (!file.parse()) throw new Error('The saved map could not be re-read.');
    tw.map.dataFile = file;
    return file;
  };

  root.MapPickerMapEdit = {
    MAX_BYTES, MAX_CELLS, TILE_STRIDE, SWITCH_STRIDE, switchOffset,
    sha256, readTileRecords, encodeTileRecords, readSwitchRecords, encodeSwitchRecords,
    locateLayer, locateLayers, locateGameLayer: (tw, dataFile) => locateLayer(tw, dataFile, 'game'),
    serializeDataFile, patch, reload,
  };
})(typeof window !== 'undefined' ? window : globalThis);
