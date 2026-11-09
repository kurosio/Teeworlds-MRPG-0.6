/* Тексты панели «Инструменты карты» (RU/EN) в одном месте.
 *
 * Раньше словарь был размазан по picker.js и picker-workbench.js: одинаковые
 * подписи («Игровой слой», «Тайл») дублировались, а подсказки режимов
 * расходились. Теперь режим клика, секции панели и сообщения о тайлах берутся
 * отсюда, поэтому переименование раздела — это правка одной строки.
 *
 * Ключи намеренно плоские: `MapPickerText.get('ru').addTitle`.
 */
(() => {
  'use strict';

  const DICT = {
    ru: {
      // ── панель и режим клика ──────────────────────────────────────────────
      dockTitle: 'Инструменты карты',
      dockBadgeTiles: 'тайлов: {0}',
      dockBadgePoints: 'точек: {0}',
      modeLabel: 'Клик по карте',
      modes: { point: 'ставит точку', tile: 'ставит тайл', measure: 'измеряет' },
      modeHints: {
        point: 'Клик — точка · перетаскивание — маркер или карта · Alt — без привязки · Shift+стрелки — на тайл · Alt+стрелки — на 1 px · G — сетка · S — привязка',
        tile: 'Клик — поставить или выбрать точку · перетаскивание — перенести · Delete — удалить · 1/2 — слой · R — сбросить · Esc — выбор позиции.',
        measure: 'Клик — точка A, второй клик — точка B. Позиция записи не меняется. M или Esc — закончить замер.',
      },

      // ── секция «Видимость карты» ──────────────────────────────────────────
      viewTitle: 'Видимость карты',
      viewHint: 'Что показать поверх карты. На выбор позиции не влияет, кроме запрещённых тайлов.',
      collision: 'Игровой слой',
      spawn: 'К спавну',
      noGame: 'В карте нет игрового слоя.',
      zoom: 'Приблизьте карту для просмотра игровых тайлов.',
      legend: '1 стена · 2 смерть · 3 без крюка · зелёный: спавны · синий: прочие ID',
      blocked: 'Красные тайлы здесь запрещены',
      solid: 'Стена',
      death: 'Смерть',
      nohook: 'Без крюка',
      empty: 'Пусто',
      outside: 'Вне игрового слоя',
      point: 'Выбранная точка',
      cursor: 'Курсор',
      tileLabel: 'Тайл',
      linkedNodes: 'Связанные узлы',
      noSwitchLayer: 'нет switch-слоя',

      // ── секция «Существующие точки» ───────────────────────────────────────
      pointsTitle: 'Существующие точки',
      pointsHint: 'Точки, которые уже стоят на карте. Выберите одну — её координаты станут позицией записи.',
      linkedLabel: 'Найденные тайлы',
      linkedUse: 'Использовать точку',
      linkedEmpty: 'На карте нет тайлов из highlightTiles — выбирать нечего.',
      linkedCount: 'Найдено точек: {0} · выберите точку и задайте её позицией записи.',
      linkedLimit: 'Показаны первые {0} точек.',
      linkedPoint: 'Точка',

      // ── секция «Новая точка на карте» ─────────────────────────────────────
      addTitle: 'Новая точка на карте',
      addHint: 'Тайл ставится на свободную клетку, а точка — в её центр. Карта сохранится по «Применить» вместе с позицией.',
      editUnsupported: 'Установка требует игрового слоя, входа администратора и локальной папки карт с правом записи.',

      // ── секция «Замер расстояния» ─────────────────────────────────────────
      measureTitle: 'Замер расстояния',
      measureHint: 'Две точки на карте: длина, тайлы и ΔX/ΔY. Позиция записи не меняется.',
      measure: 'Измерить (M)',
      clear: 'Очистить замер',
      from: 'От выбранной точки',
      use: 'B → позиция',
      start: 'Кликните две точки на карте. Позиция редактируемого контента не изменится.',
      tiles: 'тайлов',

      // ── секция «Радиус» ───────────────────────────────────────────────────
      radiusTitle: 'Предпросмотр радиуса',
      radiusHint: 'Дополнительная окружность вокруг точки: подберите радиус, не меняя поле записи.',
      radius: 'Радиус, px',
      previewLabel: 'Предпросмотр',

      // ── секция «Слои оформления» ──────────────────────────────────────────
      layersTitle: 'Слои оформления',
      layersHint: 'Скройте мешающий фон или передний план. Действует только в этом окне.',
      layers: 'Показать все слои',

      // ── правка точек и switch-тайлов ────────────────────────────────────────
      editTitle: 'Правка точек и switch-тайлов',
      editLayerLabel: 'Слой правки',
      editLayerGame: 'Игровой слой',
      editLayerSwitch: 'Switch-слой',
      editTileLabel: 'Тайл',
      editTypeHint: 'ID 192–211 — сущности игрового слоя; 1–3 (стена/смерть/без крюка) не редактируются.',
      switchNumberLabel: 'Номер switch',
      switchTypeLabel: 'Тип switch-тайла',
      switchHint: 'Типы 199/200/201 · номер 1–255 связывает тайл с узлом сбора в базе.',
      editSelected: 'Выбрана клетка X {0}, Y {1} · ID {2}',
      editSelectedSwitch: 'Выбрана клетка X {0}, Y {1} · switch #{3} · тип {2}',
      editPending: 'Черновик: +{0} · ↦{1} · −{2}. «Применить» сохранит карту.',
      editPendingEmpty: 'Правок нет.',
      editOccupied: 'Клетка занята — она выбрана. Перетащите её или нажмите Delete, чтобы удалить.',
      editCollision: 'Тайлы столкновений (1–3) не редактируются.',
      editNotEntity: 'Эта клетка — не точка, а часть карты (вода, зона, стена). Двигать и удалять её нельзя.',
      editDrift: 'Черновик разошёлся с картой (клеток: {0}). Перезагрузите окно, прежде чем сохранять.',
      editNoLayer: 'В этой карте нет switch-слоя — правка доступна только в игровом слое.',
      editNoGame: 'В этой карте нет игрового слоя.',
      editCount: 'Точек на карте: {0}',
      editTileListTitle: 'Точки на карте',
      editTileListHint: 'Все точки выбранного слоя. Выберите строку, чтобы перейти к ней и выделить клетку.',
      editCapReached: 'За одно сохранение можно изменить не более 1000 клеток.',
      editDelete: 'Удалить',
      editRestore: 'Вернуть',
      editReset: 'Сбросить правки',
      editError: 'Не удалось сохранить карту: правки остались в черновике.',
      tileNames: {
        spawn: 'Спавн игрока', spawnMobs: 'Спавн мобов', spawnPrison: 'Спавн тюрьмы',
        armor: 'Броня', health: 'Здоровье', shotgun: 'Дробовик', grenade: 'Граната', laser: 'Лазер',
        npcWall: 'Стена NPC', mobWall: 'Стена мобов', rhythm: 'Ритм-поле',
        casinoDice: 'Кости казино', casinoRoulette: 'Рулетка казино', unknown: 'Тайл',
        plant: 'Растения', ore: 'Руда', fish: 'Рыба',
        swZone: 'Switch-зона', swText: 'Switch-текст', swHouseZone: 'Зона дома', swActionZone: 'Зона действия',
      },
      tileLayer: 'Тайлы',
      quadLayer: 'Квады',
      group: 'Группа',
    },
    en: {
      dockTitle: 'Map tools',
      dockBadgeTiles: 'tiles: {0}',
      dockBadgePoints: 'points: {0}',
      modeLabel: 'Map click',
      modes: { point: 'places the point', tile: 'places a tile', measure: 'measures' },
      modeHints: {
        point: 'Click — point · drag — marker or map · Alt — bypass snap · Shift+arrows — one tile · Alt+arrows — 1 px · G — grid · S — snap',
        tile: 'Click to place or pick a point · drag to move · Delete to remove · 1/2 — layer · R — discard · Esc — position picking.',
        measure: 'Click point A, then point B. The record position does not change. M or Esc ends measuring.',
      },

      viewTitle: 'Map visibility',
      viewHint: 'What to draw on top of the map. It does not change picking, except for forbidden tiles.',
      collision: 'Gameplay layer',
      spawn: 'To spawn',
      noGame: 'No gameplay layer in this map.',
      zoom: 'Zoom in to see gameplay tiles.',
      legend: '1 solid · 2 death · 3 no-hook · green rings: selected tile IDs · blue: other tile IDs',
      blocked: 'Red tiles are forbidden here',
      solid: 'Solid',
      death: 'Death',
      nohook: 'No-hook',
      empty: 'Empty',
      outside: 'Outside gameplay layer',
      point: 'Selected point',
      cursor: 'Cursor',
      tileLabel: 'Tile',
      linkedNodes: 'Linked nodes',
      noSwitchLayer: 'no switch layer',

      pointsTitle: 'Existing points',
      pointsHint: 'Points that already exist on the map. Pick one — its coordinates become this record’s position.',
      linkedLabel: 'Matching tiles',
      linkedUse: 'Use this point',
      linkedEmpty: 'This map has no tiles from highlightTiles, so there is nothing to pick.',
      linkedCount: '{0} matching points · choose one and use it as this record’s position.',
      linkedLimit: 'Showing the first {0} matches.',
      linkedPoint: 'Point',

      addTitle: 'New point on the map',
      addHint: 'A tile goes into an empty cell, and the point lands in its center. Apply saves the map together with the position.',
      editUnsupported: 'Tile placement needs an editable game layer and an administrator with a writable local map folder.',

      measureTitle: 'Measure distance',
      measureHint: 'Two map points: length, tiles and ΔX/ΔY. The record position does not change.',
      measure: 'Measure (M)',
      clear: 'Clear measurement',
      from: 'From selected point',
      use: 'B → position',
      start: 'Click two points on the map. The selected content position will not move.',
      tiles: 'tiles',

      radiusTitle: 'Radius preview',
      radiusHint: 'An extra circle around the point: tune a radius without changing the record field.',
      radius: 'Radius, px',
      previewLabel: 'Preview',

      layersTitle: 'Decoration layers',
      layersHint: 'Hide a distracting background or foreground. Applies to this window only.',
      layers: 'Show all layers',

      editTitle: 'Points and switch tiles',
      editLayerLabel: 'Edit layer',
      editLayerGame: 'Gameplay layer',
      editLayerSwitch: 'Switch layer',
      editTileLabel: 'Tile',
      editTypeHint: 'IDs 192–211 are gameplay entities; 1–3 (solid/death/no-hook) are read-only.',
      switchNumberLabel: 'Switch number',
      switchTypeLabel: 'Switch tile type',
      switchHint: 'Types 199/200/201 · the 1–255 number links the tile to a collecting node in the database.',
      editSelected: 'Selected cell X {0}, Y {1} · ID {2}',
      editSelectedSwitch: 'Selected cell X {0}, Y {1} · switch #{3} · type {2}',
      editPending: 'Draft: +{0} · ↦{1} · −{2}. Apply saves the map.',
      editPendingEmpty: 'No edits yet.',
      editOccupied: 'That cell is occupied — it is now selected. Drag it or press Delete to remove it.',
      editCollision: 'Collision tiles (1–3) cannot be edited.',
      editNotEntity: 'This cell is not a point but part of the map (water, zone, wall). It cannot be moved or deleted.',
      editDrift: 'The draft no longer matches the map ({0} cells). Reload the window before saving.',
      editNoLayer: 'This map has no switch layer — only the gameplay layer can be edited.',
      editNoGame: 'This map has no gameplay layer.',
      editCount: 'Points on the map: {0}',
      editTileListTitle: 'Points on the map',
      editTileListHint: 'Every point of the selected layer. Pick a row to jump to it and select the cell.',
      editCapReached: 'A single save can change at most 1000 cells.',
      editDelete: 'Delete',
      editRestore: 'Revert cell',
      editReset: 'Discard edits',
      editError: 'The map could not be saved: the edits are still in the draft.',
      tileNames: {
        spawn: 'Player spawn', spawnMobs: 'Mob spawn', spawnPrison: 'Prison spawn',
        armor: 'Armor', health: 'Health', shotgun: 'Shotgun', grenade: 'Grenade', laser: 'Laser',
        npcWall: 'NPC wall', mobWall: 'Mob wall', rhythm: 'Rhythm field',
        casinoDice: 'Casino dice', casinoRoulette: 'Casino roulette', unknown: 'Tile',
        plant: 'Plants', ore: 'Ore', fish: 'Fish',
        swZone: 'Switch zone', swText: 'Switch text', swHouseZone: 'House zone', swActionZone: 'Action zone',
      },
      tileLayer: 'Tiles',
      quadLayer: 'Quads',
      group: 'Group',
    },
  };

  const api = { get: (lang) => DICT[lang === 'en' ? 'en' : 'ru'] };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else window.MapPickerText = api;
})();
