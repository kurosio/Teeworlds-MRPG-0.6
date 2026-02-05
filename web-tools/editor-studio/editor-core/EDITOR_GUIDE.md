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
