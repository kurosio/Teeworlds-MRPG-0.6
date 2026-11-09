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
        tile: 'Кликните по пустой клетке игрового слоя — тайл и точка встанут в её центр. Esc — обычный выбор позиции.',
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
      editMode: 'Ставить тайл кликом по карте',
      editType: 'Тайл для установки',
      editClear: 'Отменить добавленные тайлы',
      editNoSources: 'Нет существующих тайлов из highlightTiles — установка отключена. Позицию по-прежнему можно выбрать обычным кликом по карте.',
      editEmpty: 'Тайлы не добавлены. Включите режим и нажмите на пустую клетку. Сохранение требует прав администратора и локальную папку карт с правом записи.',
      editActive: 'Режим установки включён: кликните по пустой клетке игрового слоя. «Применить» сохранит карту и позицию; отмена отбросит изменения.',
      editSaved: 'Добавлено тайлов: {0}. «Применить» сохранит карту вместе с позицией; отмена отбросит изменения.',
      editBlocked: 'Клетка занята другим тайлом. Выберите пустую клетку.',
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
        tile: 'Click an empty gameplay tile — the tile and the point land in its center. Esc returns to ordinary position picking.',
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
      editMode: 'Place a tile by clicking the map',
      editType: 'Tile to place',
      editClear: 'Discard staged tiles',
      editNoSources: 'No matching highlighted tiles exist on this map. Tile placement is disabled; you can still click to choose a position.',
      editEmpty: 'No tiles staged. Enable add mode and click an empty tile. Saving requires admin access and a writable local map folder.',
      editActive: 'Add mode is on: click an empty game tile. Apply saves the map; Cancel discards staged edits.',
      editSaved: '{0} tile(s) staged. Apply saves them with the position; Cancel discards them.',
      editBlocked: 'That map tile is occupied. Choose an empty tile.',
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
      tileLayer: 'Tiles',
      quadLayer: 'Quads',
      group: 'Group',
    },
  };

  const api = { get: (lang) => DICT[lang === 'en' ? 'en' : 'ru'] };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else window.MapPickerText = api;
})();
