/* Каталог тайлов MRPG для редактора карты.
 *
 * Индексы — из src/game/mapitems.h (MRPG) и src/game/collision.cpp:
 *   • игровой слой хранит сущности с индексом >= ENTITY_OFFSET (255 − 16·4 = 191),
 *     поэтому индекс тайла = ENTITY_OFFSET + ENTITY_* (см. таблицу ниже);
 *   • switch-слой хранит CSwitchTileExtra: type тоже сравнивается с ENTITY_OFFSET
 *     (199/200/201 — растения, руда, рыба), number — номер switch (1..255).
 *
 * Каталог нужен, чтобы панель предлагала осмысленный список вместо ручного
 * ввода ID: у каждого тайла есть подпись и группа. Список не ограничивает
 * правки — сервер дополнительно сверяет ID с заголовком X-Map-Allowed-Tiles.
 *
 * ВАЖНО: подписи должны совпадать с mapitems.h, иначе панель врёт — например,
 * 209 (ENTITY_SPAWN_MOBS) подписывался как «Кости казино», и редактор мобов
 * показывал чужой тайл. Кости казино — это 242, рулетка — 243, а 210 не
 * существует вовсе. Соответствие проверяет tests/picker-tile-catalog.test.cjs,
 * который читает enum прямо из src/game/mapitems.h.
 */
((root) => {
  'use strict';

  // Коллизии игрового слоя: их нельзя ставить, удалять или перемещать.
  const COLLISION_IDS = [1, 2, 3];
  // ENTITY_OFFSET из mapitems.h: индекс тайла = 191 + ENTITY_*.
  const ENTITY_OFFSET = 191;

  // Игровой слой: индекс → подпись. Порядок — как в mapitems.h.
  //   192..198 — ENTITY_ARMOR_1..ENTITY_MOB_WALL,
  //   199..201 — ENTITY_PLANT/ORE/FISH (метки узлов сбора в игровом слое),
  //   208/209/211 — спавны игрока/мобов/тюрьмы,
  //   241..243 — ритм-поле и казино.
  const GAME_TILES = [
    { id: 192, key: 'armor', group: 'pickup' },
    { id: 193, key: 'health', group: 'pickup' },
    { id: 194, key: 'shotgun', group: 'pickup' },
    { id: 195, key: 'grenade', group: 'pickup' },
    { id: 196, key: 'laser', group: 'pickup' },
    { id: 197, key: 'npcWall', group: 'wall' },
    { id: 198, key: 'mobWall', group: 'wall' },
    { id: 199, key: 'plant', group: 'resource' },
    { id: 200, key: 'ore', group: 'resource' },
    { id: 201, key: 'fish', group: 'resource' },
    { id: 208, key: 'spawn', group: 'spawn' },
    { id: 209, key: 'spawnMobs', group: 'spawn' },
    { id: 211, key: 'spawnPrison', group: 'spawn' },
    { id: 241, key: 'rhythm', group: 'effect' },
    { id: 242, key: 'casinoDice', group: 'effect' },
    { id: 243, key: 'casinoRoulette', group: 'effect' },
  ];

  // switch-слой: type → подпись. 199/200/201 — эталон «как в руде».
  const SWITCH_TILES = [
    { id: 199, key: 'plant', group: 'resource' },
    { id: 200, key: 'ore', group: 'resource' },
    { id: 201, key: 'fish', group: 'resource' },
    { id: 22, key: 'swZone', group: 'zone' },
    { id: 23, key: 'swText', group: 'zone' },
    { id: 24, key: 'swHouseZone', group: 'zone' },
    { id: 25, key: 'swActionZone', group: 'zone' },
  ];

  // Группы нужны только для порядка в выпадающем списке.
  const GROUPS = ['spawn', 'pickup', 'wall', 'effect', 'resource', 'zone'];

  const byId = (list) => new Map(list.map((entry) => [entry.id, entry]));

  const GAME_BY_ID = byId(GAME_TILES);
  const SWITCH_BY_ID = byId(SWITCH_TILES);

  const entry = (layer, id) => (layer === 'switch' ? SWITCH_BY_ID.get(id) : GAME_BY_ID.get(id)) || null;

  const label = (layer, id) => entry(layer, id)?.key || 'unknown';

  /** Все известные ID слоя (без коллизий) — для allowlist и подсказок. */
  const ids = (layer) => (layer === 'switch' ? SWITCH_TILES : GAME_TILES).map((item) => item.id);

  const isCollision = (id) => COLLISION_IDS.includes(Number(id));

  /** ID из игрового слоя — сущность (а не стена/смерть/пусто). */
  const isEntity = (id) => {
    const value = Number(id);
    return Number.isInteger(value) && value > ENTITY_OFFSET && value <= 255;
  };

  /** Разумный ли ID для правки: 4..255 (1..3 — только чтение). */
  const isEditable = (id) => {
    const value = Number(id);
    return Number.isInteger(value) && value >= 4 && value <= 255;
  };

  /**
   * Может ли ID быть типом switch-тайла. Проверяем именно каталог switch-слоя:
   * игровые 192–198 в switch-слое смысла не имеют, а 199/200/201 там как раз
   * растения/руда/рыба.
   */
  const isSwitchTile = (id) => !!SWITCH_BY_ID.get(Number(id));

  const api = {
    COLLISION_IDS, ENTITY_OFFSET, GAME_TILES, SWITCH_TILES, GROUPS,
    label, ids, entry, isCollision, isEntity, isEditable, isSwitchTile,
  };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MapPickerTilesCatalog = api;
})(typeof window !== 'undefined' ? window : globalThis);
