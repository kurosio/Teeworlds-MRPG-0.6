/**
 * tests/picker-tile-catalog.test.cjs — каталог тайлов окна карты против
 * src/game/mapitems.h.
 *
 * Ошибка, которую ловит этот тест: 209 (ENTITY_SPAWN_MOBS, спавн мобов) был
 * подписан как «Кости казино». Редактор мобов подсвечивает 209, поэтому панель
 * показывала чужой тайл, а настоящие казино-тайлы (242/243) в списке не было.
 * Тест читает enum прямо из C++ — так подпись не сможет снова разъехаться с
 * сервером, даже если порядок ENTITY_* поменяют.
 */
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const catalog = require('../vendor/ddnet-map/picker-tile-catalog.js');

const repoRoot = path.resolve(__dirname, '../../..');
const mapitems = fs.readFileSync(path.join(repoRoot, 'src/game/mapitems.h'), 'utf8');

/** Арифметика из заголовка (`255 - 16 * 4`): только цифры и знаки операций. */
function numeric(expression) {
  const source = String(expression).trim();
  assert.match(source, /^[\d\s+*()/-]+$/, `неожидаемое выражение в mapitems.h: ${source}`);
  // eslint-disable-next-line no-new-func
  const value = Function(`"use strict"; return (${source});`)();
  assert.ok(Number.isFinite(value), `не удалось посчитать ${source}`);
  return value;
}

/**
 * Значения enum из mapitems.h: `NAME = N` (в том числе арифметика вида
 * `255 - 16 * 4`) или `NAME,` — автоинкремент предыдущего значения. Если
 * значение задано ссылкой на другой элемент (`ROTATION_90 = TILEFLAG_ROTATE`),
 * цепочка автоинкремента обрывается: такие имена получают null, а важные для
 * каталога записи заданы числами.
 */
function parseEnum(source, blockPattern) {
  const block = source.match(blockPattern);
  assert.ok(block, `в mapitems.h не найден блок ${blockPattern}`);
  const values = new Map();
  let next = 0;
  for (const rawLine of block[0].split('\n')) {
    const line = rawLine.replace(/\/\/.*$/, '').trim();
    const match = line.match(/^([A-Z][A-Z0-9_]*)\s*(?:=\s*([^,]+))?,?$/);
    if (!match) continue;
    const [, name, expression] = match;
    if (expression === undefined) {
      values.set(name, next);
      if (next !== null) next += 1;
      continue;
    }
    if (/^[\d\s+*()/-]+$/.test(expression)) {
      next = numeric(expression);
      values.set(name, next);
      next += 1;
    } else {
      next = null; // ссылка на другое имя: дальнейший автоинкремент неизвестен
      values.set(name, null);
    }
  }
  return values;
}

// Весь большой enum mapitems.h: слои, сущности, тайлы, флаги, ENTITY_OFFSET.
const entities = parseEnum(mapitems, /\/\/ layer types[\s\S]*?ENTITY_OFFSET\s*=\s*255 - 16 \* 4,/);
const offset = entities.get('ENTITY_OFFSET');
assert.ok(Number.isInteger(offset) && offset > 0, 'ENTITY_OFFSET не найден в mapitems.h');
assert.equal(offset, 191, 'ENTITY_OFFSET изменился — пересчитайте индексы тайлов');

const tileId = (name) => offset + entities.get(name);

test('every gameplay entity from mapitems.h has its real ID in the catalog', () => {
  const expected = {
    ENTITY_ARMOR_1: 'armor',
    ENTITY_HEALTH_1: 'health',
    ENTITY_PICKUP_SHOTGUN: 'shotgun',
    ENTITY_PICKUP_GRENADE: 'grenade',
    ENTITY_PICKUP_LASER: 'laser',
    ENTITY_NPC_WALL: 'npcWall',
    ENTITY_MOB_WALL: 'mobWall',
    ENTITY_PLANT: 'plant',
    ENTITY_ORE: 'ore',
    ENTITY_FISH: 'fish',
    ENTITY_SPAWN: 'spawn',
    ENTITY_SPAWN_MOBS: 'spawnMobs',
    ENTITY_SPAWN_PRISON: 'spawnPrison',
    ENTITY_RHYTHM_FIELD: 'rhythm',
    ENTITY_CASINO_DICE: 'casinoDice',
    ENTITY_CASINO_ROULETTE: 'casinoRoulette',
  };
  for (const [name, key] of Object.entries(expected)) {
    const id = tileId(name);
    assert.ok(Number.isInteger(id), `${name} не найден в mapitems.h`);
    assert.equal(catalog.label('game', id), key, `${name} должен быть ${key} на индексе ${id}`);
  }
});

test('mob spawns are 209 and casino tiles are 242/243, not 209/210', () => {
  // Регрессия: эти строки и были ошибкой в редакторе мобов.
  assert.equal(tileId('ENTITY_SPAWN_MOBS'), 209);
  assert.equal(catalog.label('game', 209), 'spawnMobs');
  assert.equal(tileId('ENTITY_CASINO_DICE'), 242);
  assert.equal(tileId('ENTITY_CASINO_ROULETTE'), 243);
  assert.equal(catalog.label('game', 242), 'casinoDice');
  assert.equal(catalog.label('game', 243), 'casinoRoulette');
  assert.equal(catalog.entry('game', 210), null, '210 не существует в mapitems.h');
  assert.equal(catalog.label('game', 210), 'unknown');
});

test('game catalog lists exactly the entity IDs of mapitems.h (no phantoms)', () => {
  const entityIds = [...entities.entries()]
    .filter(([name, value]) => name.startsWith('ENTITY_') && name !== 'ENTITY_NULL'
      && value > 0 && offset + value <= 255)
    .map(([, value]) => offset + value)
    .sort((a, b) => a - b);
  assert.ok(entityIds.length >= 16, 'в mapitems.h должны быть все сущности игрового слоя');
  assert.deepEqual(catalog.ids('game').slice().sort((a, b) => a - b), entityIds);
});

test('switch catalog matches the switch types read by collision.cpp', () => {
  // Растения/руда/рыба обрабатываются как switch-тайлы (OnEntitySwitch):
  // в CSwitchTileExtra лежит Type = ENTITY_OFFSET + ENTITY_PLANT/ORE/FISH
  // (collision.cpp: `if(Type >= ENTITY_OFFSET) funcInit(Type - ENTITY_OFFSET)`).
  for (const name of ['ENTITY_PLANT', 'ENTITY_ORE', 'ENTITY_FISH']) {
    const type = tileId(name);
    assert.equal(type, offset + entities.get(name));
    assert.ok(catalog.isSwitchTile(type), `${name}: switch-тип ${type} должен быть в каталоге`);
  }
  // Те же ID в игровом слое — отдельные метки узлов сбора, и это не мешает
  // каталогу: слой выбирается явно (`catalog.label('game', 200)` ≠ switch).
  assert.equal(catalog.label('game', 200), 'ore');
  assert.equal(catalog.label('switch', 200), 'ore');
  // Зоны читаются из CSwitchTileExtra switch(type) в InitSwitchExtra.
  for (const name of ['TILE_SW_ZONE', 'TILE_SW_TEXT', 'TILE_SW_HOUSE_ZONE', 'TILE_SW_ACTION_ZONE']) {
    const value = Number(mapitems.match(new RegExp(`${name}\\s*=\\s*(\\d+)`))?.[1]);
    assert.ok(Number.isInteger(value), `${name} не найден в mapitems.h`);
    assert.ok(catalog.isSwitchTile(value), `${name} (${value}) должен быть в switch-каталоге`);
  }
});

test('entity and collision helpers keep the boundary at ENTITY_OFFSET', () => {
  assert.equal(catalog.isEntity(offset), false);
  assert.equal(catalog.isEntity(offset + 1), true);
  assert.equal(catalog.isEntity(255), true);
  assert.equal(catalog.isEntity(256), false);
  assert.deepEqual(catalog.COLLISION_IDS, [1, 2, 3]);
  // ID 1–3 — только чтение: правка точки выше нуля и вне коллизий.
  assert.equal(catalog.isEditable(3), false);
  assert.equal(catalog.isEditable(4), true);
});

// ── подписи ────────────────────────────────────────────────────────────────
// Ключ каталога обязан иметь перевод: иначе вместо «Спавн мобов» в списке
// появится сырой ключ `spawnMobs`, что выглядит как ещё одна ошибка подписи.
test('every catalog key has a Russian and an English label', () => {
  const catalog = require('../vendor/ddnet-map/picker-tile-catalog.js');
  const text = require('../vendor/ddnet-map/picker-text.js');
  for (const lang of ['ru', 'en']) {
    const names = text.get(lang).tileNames || {};
    for (const layer of ['game', 'switch']) {
      for (const id of catalog.ids(layer)) {
        const key = catalog.label(layer, id);
        assert.ok(names[key], `${lang}: у ключа ${key} (${layer} ${id}) нет подписи`);
      }
    }
    // Лишние подписи не вредят, но «unknown» обязан быть: это фолбэк панели.
    assert.ok(names.unknown, `${lang}: нет подписи для unknown`);
  }
});

test('the mob-spawn label never reads as a casino tile', () => {
  const catalog = require('../vendor/ddnet-map/picker-tile-catalog.js');
  const text = require('../vendor/ddnet-map/picker-text.js');
  const ru = text.get('ru').tileNames;
  const en = text.get('en').tileNames;
  assert.equal(ru[catalog.label('game', 209)], 'Спавн мобов');
  assert.equal(en[catalog.label('game', 209)], 'Mob spawn');
  assert.equal(ru[catalog.label('game', 242)], 'Кости казино');
  assert.equal(en[catalog.label('game', 243)], 'Casino roulette');
  // Ни одна подпись спавна не содержит «казино»/«dice».
  for (const id of [208, 209, 211]) {
    const key = catalog.label('game', id);
    assert.doesNotMatch(ru[key], /казино|кости/i, `${id}: ${ru[key]}`);
    assert.doesNotMatch(en[key], /casino|dice|roulette/i, `${id}: ${en[key]}`);
  }
});
