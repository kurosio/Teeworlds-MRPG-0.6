/* Каталог тайлов MRPG для редактора карты.
 *
 * Индексы — из src/game/mapitems.h (MRPG) и src/game/collision.cpp:
 *   • игровой слой хранит сущности с индексом >= ENTITY_OFFSET (255 − 16·4 = 191),
 *     поэтому 192..211 — это «точки» (спавны, пикапы, стены ботов, узлы сбора…);
 *   • switch-слой хранит CSwitchTileExtra: type тоже сравнивается с ENTITY_OFFSET
 *     (199/200/201 — растения, руда, рыба), number — номер switch (1..255).
 *
 * Каталог нужен, чтобы панель предлагала осмысленный список вместо ручного
 * ввода ID: у каждого тайла есть подпись и группа. Список не ограничивает
 * правки — сервер дополнительно сверяет ID с заголовком X-Map-Allowed-Tiles.
 */
((root) => {
  'use strict';

  // Коллизии игрового слоя: их нельзя ставить, удалять или перемещать.
  const COLLISION_IDS = [1, 2, 3];
  const ENTITY_OFFSET = 191;

  // Игровой слой: индекс → подпись. Порядок — как в mapitems.h.
  const GAME_TILES = [
    { id: 192, key: 'spawn', group: 'spawn' },
    { id: 193, key: 'spawnMobs', group: 'spawn' },
    { id: 194, key: 'spawnPrison', group: 'spawn' },
    { id: 195, key: 'armor', group: 'pickup' },
    { id: 196, key: 'health', group: 'pickup' },
    { id: 197, key: 'shotgun', group: 'pickup' },
    { id: 198, key: 'grenade', group: 'pickup' },
    { id: 199, key: 'laser', group: 'pickup' },
    { id: 200, key: 'npcWall', group: 'wall' },
    { id: 201, key: 'mobWall', group: 'wall' },
    { id: 208, key: 'rhythm', group: 'effect' },
    { id: 209, key: 'casinoDice', group: 'effect' },
    { id: 210, key: 'casinoRoulette', group: 'effect' },
    { id: 211, key: 'unknown', group: 'effect' },
  ];

  // Switch-слой: type → подпись. 199/200/201 — эталон «как в руде».
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

  const api = {
    COLLISION_IDS, ENTITY_OFFSET, GAME_TILES, SWITCH_TILES, GROUPS,
    label, ids, entry, isCollision, isEntity, isEditable,
  };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MapPickerTilesCatalog = api;
})(typeof window !== 'undefined' ? window : globalThis);
