# Editor Studio • Core guide

## Цель
- Единый UI/стили для всех редакторов.
- Один набор компонентов + утилит в `editor-core/`.
- Минимум копипасты при создании нового редактора.

## Что использовать
### 1) Подключение Core
Рекомендуемый вариант (единый набор модулей + порядок в одном месте):

```html
<script src="editor-core/page-loader.js" data-mode="scenario"></script>
```

`data-mode` поддерживается для совместимости; базовый UI инициализируется в сценарном режиме. Лоадер подключает шрифты, tailwind, стили и все модули `editor-core` в правильной очередности, а также сохраняет результат `bootstrapEditor` в `window.EditorCoreBootstrap`.

### 2) Быстрый старт страницы

```js
const { fieldRenderOptions } = window.EditorCoreBootstrap || EditorCore.bootstrapEditor();
```

- `data-mode="scenario"` — основной режим сценарных страниц с модальным редактором.

### 3) Формы из схемы (рекомендуется)
Если редактор — это "форма по схеме", используйте `EditorCore.FormRuntime`:

```js
const form = EditorCore.FormRuntime.mount(root, {
  fields: schemaFields,
  data: model,
  fieldOptions: fieldRenderOptions,
  onChange: (data, { path, value }) => { /* ... */ }
});
```

`FormRuntime` + `FieldRenderer` дают:
- единые поля, валидацию, toggles
- `db_select` и `db_multiselect` (авто-подгрузка из БД)
- списки с add/remove

## Быстрый CRUD-редактор (DB)
Для стандартных таблиц (list + форма + сохранение) используйте `EditorCore.DbEditor`:

```js
const { fieldRenderOptions } = window.EditorCoreBootstrap || EditorCore.bootstrapEditor();

EditorCore.DbEditor.mount(document.body, {
  resource: 'worlds',
  fields: schemaFields,
  fieldOptions: fieldRenderOptions,
  defaults: () => ({ Name: '', Path: '', Type: 'default' }),
  rowToModel: (row) => ({ ...row }),
  modelToPayload: (model) => ({ ...model })
});
```

Такой runtime уже решает:
- загрузку списка, поиск, выбор записи
- сохранение / удаление
- единый адаптивный макет (через `editor-template.css`)

## DB Select (db_select)
- В схеме поля указывайте `ui.dbKey` (предпочтительно) или `ui.datasource`.

Пример:
```js
QuestID: { type: 'db_select', label: 'Квест', ui: { dbKey: 'quest' }, validate: { required: true } }
```

Карта ключей хранится в `editor-core/db-map.js`.

## Tags (теги / chips)

Универсальный виджет для полей типа `SET`/массивов (строки) и для списков ID (из БД).

**Возможности:**
- поиск по вариантам
- добавление/удаление кликом
- клавиатура: ↑/↓ выбор, Enter добавить, Backspace удалить последний
- адаптивная сетка вариантов (на широких экранах несколько колонок)

### 1) Статический список (ручные варианты)

```js
Debuffs: {
  type: 'set',
  label: 'Debuffs',
  ui: EditorCore.presets.tags({
    options: ['Slowdown', 'Poison', 'Fire'],
    placeholder: 'Добавить debuff...'
  })
}
```

### 2) Варианты из БД (храним массив ID)

```js
DropItems: {
  type: 'array',
  label: 'Дроп: предметы',
  ui: EditorCore.presets.tags({
    datasource: 'item',   // DBMap key (или прямой source, например 'items')
    valueType: 'number',  // значения будут числами
    labelMode: 'id_name', // 'name' | 'id_name'
    placeholder: 'Добавить предмет...'
  })
}
```

## PositionPicker (позиция на карте)

Один элемент для всех координат: карточка закреплённой точки, мир, кнопка карты
и кнопка ручного ввода. X/Y хранятся в скрытых инпутах, поэтому `FormRuntime`,
валидация и гостевые правки работают как обычно.

**Через схему (рекомендуется):**

```js
Position: {
  type: 'vec2',
  label: 'Позиция на карте',
  ui: { step: 1, mapWorldPath: 'WorldID' }
}
```

- `mapWorldPath` связывает подложку «Мир» с существующим полем формы: выбор
  мира в карточке или в окне карты сохранится в `WorldID`.
- `ui.axes: { x: 'PosX', y: 'PosY' }` — для двух отдельных колонок БД.
- `ui.map: false` / `ui.world: false` скрывают соответствующие действия.

### Флаги карты

Флаги задаются в `ui` (или в `PositionPicker.render()`) и попадают в окно карты
через `PointMap.open()`. Активные флаги видны в карточке как короткие метки —
сразу понятно, с каким набором слоёв откроется карта.

| Флаг | Что делает | Где используется |
| --- | --- | --- |
| `safeTiles: true` | Запрещает COLLIDE(1)/DEATH(2)/UNHOOK(3): точка на них не применяется, но **не сбрасывается** | мобы, NPC, позиция шага и цели заданий |
| `safeTiles: [2, 3]` или `blockTiles: [2]` | Свой список запрещённых ID тайлов | точечные сценарии |
| `showTiles: true` | Включить игровой слой при открытии, без запрета размещения | аэтры, подземелья, склады, телепорт сценария |
| `highlightTiles: [209]` | Подсветить перечисленные ID тайлов; [] отключает подсветку | позиции мобов, компонент defeat_mobs |
| `radii: [{ path: 'Radius', label: 'Spawn radius', color: '#52df9a', dash: [6, 4] }]` | Массив окружностей: у каждой свой радиус (поле формы `path` или фиксированный `value`), цвет линии, штрихи пунктира (`[]` — сплошная), толщина и подпись | мобы (радиус спавна + активности), сценарий `defeat_mobs` |
| `radius: 320` / `radii: [{ value: 320, ... }]` | Фиксированный радиус без поля формы — для дистанций, зашитых в сервер (склад, аэтр, доска квестов) | склады, аэтры, доски квестов |
| `radiusPath` / `activeRadiusPath` | Устаревшие одиночные радиусы; превращаются в те же окружности, подписи — из словаря окна | обратная совместимость |
| `grid: true` | Включить сетку 32×32 при открытии | мобы, NPC, задания, двери подземелий |
| `snap: true` | Включить привязку к центрам тайлов при открытии | мобы, NPC, задания, двери подземелий |

Панель **«Инструменты размещения»** при открытии карты всегда свёрнута. Без
`showTiles: true` игровой слой выключен, без `grid: true` выключена сетка;
`safeTiles`, `highlightTiles` и `radii` не меняют эти начальные состояния.
Например, `ui: { showTiles: true, grid: true }` включает оба инструмента для
конкретного `vec2`, а пустой `ui: {}` оставляет оба выключенными. Сетка не
восстанавливается из прошлого посещения карты.

Пример для моба:

```js
Position: {
  type: 'vec2', label: 'Позиция (X, Y)',
  ui: {
    axes: { x: 'PositionX', y: 'PositionY' }, mapWorldPath: 'WorldID',
    safeTiles: true, highlightTiles: [209],
    // Радиус спавна — зелёный, радиус активности — синий: цвет, пунктир и
    // подпись задаются здесь, поэтому на карте не бывает «безымянных» кругов.
    radii: [
      { path: 'Radius', label: 'Spawn radius', color: '#52df9a', dash: [6, 4] },
      { path: 'ActiveRadius', label: 'Active radius', color: '#6f8cff', dash: [3, 3], lineWidth: 1.5 }
    ],
    grid: true, snap: true
  }
}
```

Точка на запрещённом тайле остаётся в скрытых X/Y: `editor-map:validity`
лишь подсвечивает карточку (`is-invalid`), а «Применить» в карте блокируется,
пока точку не перенесут. Ручной ввод координат работает всегда — это путь
исправить значение, если карта мира недоступна.

**Программно (кастомные редакторы):**

```js
const picker = EditorCore.PositionPicker.create({
  label: 'Точка цели',
  xPath: 'move_to.0.x',
  yPath: 'move_to.0.y',
  worldPath: 'move_to.0.world_id',
  worldId: 0,
  step: 1,
  compact: true,
  safeTiles: true,
  xInputHtml: '<input type="number" data-position-axis="x" data-map-axis="x" ... hidden>',
  yInputHtml: '<input type="number" data-position-axis="y" data-map-axis="y" ... hidden>',
});
host.appendChild(picker);

EditorCore.PositionPicker.onWorldChange(picker, (worldId) => saveWorld(worldId));
EditorCore.PositionPicker.setWorldId(picker, 3);
EditorCore.PositionPicker.openMap(picker, { worldId: 3, mapOptions: { safe: [1, 2, 3] } });
EditorCore.PositionPicker.setValue(picker, { x: 100, y: 200 });
EditorCore.PositionPicker.getMapOptions(picker); // флаги, с которыми создан элемент
```

Элемент сам подписывается на `editor-map:applied`, `editor-map:world`,
`editor-map:validity` и `editor-map:closed`, поэтому выбранные в карте точка,
мир и состояние валидности возвращаются в форму без дополнительного кода.

Архитектура: `PositionPicker` — единственный UI-элемент и единственная точка
открытия карты (`PointMap.open()`). `PointMap` — только диалог с iframe; он не
ищет поля мира и не грузит БД. Общий кэш списка миров, парсинг ID (включая мир
0) и «последний выбранный мир» живут в `editor-core/map-worlds.js`. Кнопку
карты обрабатывает сам элемент — второго глобального слушателя кликов нет.

## Как добавить новый редактор в Studio
1) Создайте файл `my-editor.html`.
2) Добавьте его в `studio.manifest.js` в нужную группу.
3) Используйте набор `editor-core/*.js` + `FormRuntime`.

---

## Editor Template (layout) — единый адаптивный макет

В `editor-core/editor-template.css` лежит тонкий слой разметки/макета:
- `.editor-page` — общий контейнер страницы
- `.editor-shell` — сетка Sidebar + Main (на мобилке складывается)
- `.editor-section` + заголовок `.editor-section-head` — секции формы
- `.editor-form-grid-2` — быстрый 2-колоночный грид для форм

Чтобы включить цветовые пресеты, просто поставьте атрибут:

```html
<body class="editor-theme" data-editor="crafts">
```

### Списки как таблица (ровные строки)

Для любых `type:'list'` можно включить режим таблицы:

```js
RequiredItemsArr: {
  type: 'list',
  ui: {
    listMode: 'table',
    tableHeader: ['Предмет', 'Кол-во'],
    tableColsMd: 'minmax(0,1fr) 160px'
  },
  itemFields: { ... }
}
```

Это даёт ровные колонки на десктопе и аккуратный стек на телефоне.
