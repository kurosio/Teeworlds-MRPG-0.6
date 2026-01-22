# Editor Studio • Core guide

## Цель
- Единый UI/стили для всех редакторов.
- Один набор компонентов + утилит в `editor-core/`.
- Минимум копипасты при создании нового редактора.

## Что использовать
### 1) Подключение Core одним скриптом
Рекомендуемый вариант (вместо множества `<script ...>`):

```html
<link rel="stylesheet" href="editor-core/editor-theme.css" />
<script src="editor-core/tailwind-theme.js"></script>
<script src="https://cdn.tailwindcss.com?plugins=forms"></script>
<script src="editor-core/editor-core.bundle.js"></script>
```

> Если нужен `registry.js` (только для схем), подключайте его отдельно перед bundle.

### 2) Быстрый старт страницы

```js
const { fieldRenderOptions } = EditorCore.bootstrapEditor({ mode: 'event' });
```

- `mode: 'scenario'` — для сценарных страниц с модальным редактором.
- `mode: 'event'` — для редакторов шагов/действий (похоже на `event-editor.php`).

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

## DB Select (db_select)
- В схеме поля указывайте `ui.dbKey` (предпочтительно) или `ui.datasource`.

Пример:
```js
QuestID: { type: 'db_select', label: 'Квест', ui: { dbKey: 'quest' }, validate: { required: true } }
```

Карта ключей хранится в `editor-core/db-map.js`.

## Как добавить новый редактор в Studio
1) Создайте файл `my-editor.html`.
2) Добавьте его в `studio.manifest.js` в нужную группу.
3) Используйте `editor-core.editor-core.bundle.js` + `FormRuntime`.

