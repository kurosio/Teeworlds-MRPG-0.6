/* Safe, focused writer for one DDNet game-layer tilemap in a v4 datafile. */
((root) => {
  'use strict';
  const writeU32 = (view, offset, value) => view.setUint32(offset, value >>> 0, true);
  const readTileRecords = (bytes, count, version) => {
    if (bytes.byteLength % 4 !== 0 || !Number.isSafeInteger(count) || count < 0 || count > 4_000_000) {
      throw new Error('Tile layer data is misaligned or too large.');
    }
    const records = new Uint8Array(count * 3);
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
  const encodeTileRecords = (records, version) => {
    const count = records.length / 3;
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
  const locateGameLayer = (tw, dataFile) => {
    const layers = dataFile?.getType?.(tw.MAPITEMTYPE_LAYER);
    if (!layers) throw new Error('This map has no layer table.');
    for (let i = 0; i < layers.num; i++) {
      const item = dataFile.getItem(layers.start + i);
      if (!item || item.data?.byteLength < 60) continue;
      const layer = tw.parseMapLayer(item.data);
      if (layer.type !== tw.LAYERTYPE_TILES) continue;
      const tileLayer = tw.parseMapLayerTiles(item.data);
      if (tileLayer.image === -1 && (tileLayer.flags & tw.TILESLAYERFLAG_GAME)) {
        return { item, tileLayer };
      }
    }
    throw new Error('This map has no editable game tile layer.');
  };
  const serializeDataFile = (dataFile, replaceIndex, newRawData) => {
    if (!dataFile || dataFile.version !== 4) throw new Error('Only DDNet datafile v4 maps can be edited.');
    if (!dataFile.fileData || dataFile.fileData.byteLength > 48 * 1024 * 1024) {
      throw new Error('This map exceeds the supported 48 MiB save limit.');
    }
    const original = new Uint8Array(dataFile.fileData);
    const typeCount = dataFile.numItemTypes, itemCount = dataFile.numItems, dataCount = dataFile.numRawData;
    const typeStart = 36;
    const itemOffsetsStart = typeStart + typeCount * 12;
    const dataOffsetsStart = itemOffsetsStart + itemCount * 4;
    const dataSizesStart = dataOffsetsStart + dataCount * 4;
    const itemStart = dataSizesStart + dataCount * 4;
    const dataStart = itemStart + dataFile.itemSize;
    if (dataStart > original.length || replaceIndex < 0 || replaceIndex >= dataCount) throw new Error('Map datafile indexes are invalid.');

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
      const blob = i === replaceIndex
        ? Zlib.Deflate.compress(newRawData)
        : original.slice(dataStart + info.offset, dataStart + info.offset + info.compSize);
      offsets.push(packedDataSize);
      sizes.push(i === replaceIndex ? newRawData.byteLength : info.size);
      rawBlobs.push(blob);
      packedDataSize += blob.byteLength;
    }

    const typeBytes = original.slice(typeStart, itemOffsetsStart);
    const itemOffsetBytes = original.slice(itemOffsetsStart, dataOffsetsStart);
    const itemBytes = original.slice(itemStart, dataStart);
    const swapSize = 36 + typeBytes.length + itemOffsetBytes.length + dataCount * 8 + itemBytes.length;
    const fileSize = swapSize + packedDataSize;
    if (!Number.isSafeInteger(fileSize) || fileSize > 48 * 1024 * 1024 || fileSize > 0xffffffff
      || newRawData.byteLength > 0xffffffff || packedDataSize > 0xffffffff) {
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
  const patch = (tw, changes) => {
    const dataFile = tw?.map?.dataFile;
    if (!dataFile?.fileData || dataFile.fileData.byteLength > 48 * 1024 * 1024) {
      throw new Error('This map exceeds the supported 48 MiB save limit.');
    }
    const { tileLayer } = locateGameLayer(tw, dataFile);
    const count = tileLayer.width * tileLayer.height;
    if (!Number.isSafeInteger(count) || tileLayer.width <= 0 || tileLayer.height <= 0 || count > 4_000_000) {
      throw new Error('Game tile layer dimensions are invalid or too large.');
    }
    const records = readTileRecords(new Uint8Array(dataFile.getData(tileLayer.data)), count, tileLayer.version);
    for (const change of changes) {
      const x = Number(change.x), y = Number(change.y), tile = Number(change.tile);
      if (!Number.isInteger(x) || !Number.isInteger(y) || x < 0 || y < 0 || x >= tileLayer.width || y >= tileLayer.height
        || !Number.isInteger(tile) || tile < 4 || tile > 255) throw new Error('A requested tile edit is invalid.');
      const index = y * tileLayer.width + x;
      if (records[index * 3] !== 0 && records[index * 3] !== tile) throw new Error(`Tile ${x}, ${y} is already occupied.`);
      records[index * 3] = tile;
    }
    return serializeDataFile(dataFile, tileLayer.data, encodeTileRecords(records, tileLayer.version));
  };
  root.MapPickerMapEdit = { sha256, locateGameLayer, readTileRecords, encodeTileRecords, serializeDataFile, patch };
})(typeof window !== 'undefined' ? window : globalThis);
