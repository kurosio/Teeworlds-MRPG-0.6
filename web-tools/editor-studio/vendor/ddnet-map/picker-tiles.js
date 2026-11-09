/* Черновик правок тайлов: чистая модель без DOM и без карты.
 *
 * Окно карты редактирует две независимые сетки клеток:
 *
 *   game   — игровой слой: значение клетки — индекс тайла (0 — пусто);
 *   switch — switch-слой: значение клетки — `{ number, type, flags, delay }`
 *            (type 0 или отсутствие значения — пусто). Растения/руда/рыба
 *            живут именно здесь: type 199/200/201, number — номер switch.
 *
 * Черновик хранит только реально изменённые клетки, поэтому сравнение
 * «было/стало» не требует копий многомиллионных слоёв: `before` запоминается
 * при первом касании клетки, а `after` — текущее значение. Отмена клетки
 * возвращает её в исходное состояние и выбрасывает запись, так что «применить»
 * уходит на сервер ровно тем набором правок, который видит пользователь.
 *
 * Модуль ничего не знает про рендер и DOM: размеры слоёв и функции чтения
 * приходят снаружи, поэтому его можно проверять в Node.
 */
((root) => {
  'use strict';

  const LAYERS = ['game', 'switch'];
  const MAX_EDITS = 1000;
  const isIndex = (value) => Number.isInteger(value) && value >= 0;

  /** Одно и то же значение клетки? Пустые значения приводятся к null. */
  const same = (a, b) => {
    const left = a ?? null, right = b ?? null;
    if (left === null || right === null) return left === right;
    if (typeof left === 'number' || typeof right === 'number') return left === right;
    return left.number === right.number && left.type === right.type;
  };

  /*
   * Switch-клетка — это только номер и тип. Флаги и задержку редактор не
   * меняет: они остаются как в карте (см. MapPickerMapEdit.patch), иначе
   * сохранение меняло бы поведение чужих switch-тайлов.
   */
  const normalizeSwitch = (value) => {
    if (!value || !Number(value.type)) return null;
    const number = Number(value.number), type = Number(value.type);
    if (!Number.isInteger(number) || number < 0 || number > 255) return null;
    if (!Number.isInteger(type) || type < 1 || type > 255) return null;
    return { number, type };
  };

  const normalizeGame = (value) => {
    const tile = Number(value ?? 0);
    return Number.isInteger(tile) && tile > 0 && tile <= 255 ? tile : 0;
  };

  const normalize = (layer, value) => (layer === 'switch' ? normalizeSwitch(value) : normalizeGame(value));

  /**
   * `sizes`  — `{ game: { width, height }, switch: { width, height } }`;
   * `read`   — `(layer, index) => значение клетки в исходной карте`;
   * `onCell` — `(layer, index, value) => void`, применяет значение к живому
   *            просмотру карты (черновик сам ничего не мутирует).
   */
  const createDraft = ({ sizes = {}, read = () => null, onCell = () => {}, limit = MAX_EDITS } = {}) => {
    const cells = { game: new Map(), switch: new Map() }; // index → { before, after }
    const capacity = Number.isInteger(limit) && limit > 0 ? limit : MAX_EDITS;

    const zone = (layer) => {
      const size = sizes[layer];
      if (!size || !Number.isInteger(size.width) || !Number.isInteger(size.height)
        || size.width < 1 || size.height < 1) return null;
      return size;
    };

    const indexOf = (layer, x, y) => {
      const size = zone(layer);
      const cx = Number(x), cy = Number(y);
      if (!size || !Number.isInteger(cx) || !Number.isInteger(cy) || cx < 0 || cy < 0
        || cx >= size.width || cy >= size.height) return -1;
      return cy * size.width + cx;
    };

    const coordsOf = (layer, index) => {
      const size = zone(layer);
      if (!size || !isIndex(index) || index >= size.width * size.height) return null;
      return { x: index % size.width, y: Math.floor(index / size.width) };
    };

    const effective = (layer, index) => {
      const cell = cells[layer].get(index);
      return cell ? cell.after : normalize(layer, read(layer, index));
    };

    const paint = (layer, index, value) => {
      const cell = cells[layer].get(index);
      const original = cell ? cell.before : normalize(layer, read(layer, index));
      if (same(original, value)) {
        // Клетка вернулась к исходному значению — правки больше нет.
        if (cell) { cells[layer].delete(index); onCell(layer, index, original); }
        return true;
      }
      if (!cell && draft.size >= capacity) return false;
      cells[layer].set(index, { before: original, after: value });
      onCell(layer, index, value);
      return true;
    };

    const draft = {
      get size() { return cells.game.size + cells.switch.size; },
      get limit() { return capacity; },
      canTouch() { return draft.size < capacity; },
      zone,
      indexOf,
      coordsOf,
      get(layer, index) { return LAYERS.includes(layer) ? effective(layer, index) : null; },
      /** Существует ли клетка в слое (вне сетки — false). */
      inside(layer, x, y) { return indexOf(layer, x, y) >= 0; },
      /** Поставить значение клетки; `null`/0 — очистить. */
      set(layer, x, y, value) {
        if (!LAYERS.includes(layer)) return false;
        const index = indexOf(layer, x, y);
        if (index < 0) return false;
        return paint(layer, index, normalize(layer, value));
      },
      /** Перенести значение клетки; цель должна быть пустой. */
      move(layer, from, to) {
        if (!LAYERS.includes(layer)) return false;
        const source = indexOf(layer, from?.x, from?.y);
        const target = indexOf(layer, to?.x, to?.y);
        if (source < 0 || target < 0 || source === target) return false;
        const value = effective(layer, source);
        if (!value || value.type === 0) return false;
        const occupied = effective(layer, target);
        if (occupied && occupied.type !== 0) return false;
        if (layer === 'game' && (value <= 3)) return false;
        if (cells[layer].size + 2 > capacity) return false;
        paint(layer, source, normalize(layer, null));
        return paint(layer, target, value);
      },
      /** Вернуть одну клетку к исходному значению. */
      restore(layer, x, y) {
        if (!LAYERS.includes(layer)) return false;
        const index = indexOf(layer, x, y);
        if (index < 0) return false;
        return paint(layer, index, normalize(layer, read(layer, index)));
      },
      /** Отбросить все правки, вернув живой просмотр к исходной карте. */
      reset() {
        for (const layer of LAYERS) {
          for (const [index, cell] of cells[layer]) onCell(layer, index, cell.before);
          cells[layer].clear();
        }
      },
      /** Список изменённых клеток: готовый план для MapPickerMapEdit.patch. */
      changes() {
        const result = { game: [], switch: [] };
        for (const layer of LAYERS) {
          for (const [index, cell] of cells[layer]) {
            const spot = coordsOf(layer, index);
            if (!spot) continue;
            // `from` — исходное значение клетки: сервер сверяет и его, чтобы
            // удалять можно было только те сущности, которые редактор знает.
            if (layer === 'switch') {
              const value = cell.after || { number: 0, type: 0 };
              result.switch.push({
                x: spot.x, y: spot.y, number: value.number, type: value.type,
                from: cell.before ? { number: cell.before.number, type: cell.before.type } : { number: 0, type: 0 },
              });
            } else {
              result.game.push({ x: spot.x, y: spot.y, tile: cell.after || 0, from: cell.before || 0 });
            }
          }
        }
        return result;
      },
      /** Изменённые клетки одного слоя (для подсветки и списков). */
      list(layer) {
        if (!LAYERS.includes(layer)) return [];
        return [...cells[layer].entries()]
          .map(([index, cell]) => ({ index, ...coordsOf(layer, index), before: cell.before, after: cell.after }))
          .filter((entry) => entry.x !== undefined);
      },
    };
    return draft;
  };

  const api = { LAYERS, MAX_EDITS, createDraft, normalizeGame, normalizeSwitch, same };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MapPickerTiles = api;
})(typeof window !== 'undefined' ? window : globalThis);
