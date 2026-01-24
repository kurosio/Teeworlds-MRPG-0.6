# Editor Studio — структура и ядро

Этот пакет содержит **единое ядро UI + DB** и несколько редакторов, подключаемых как независимые страницы.

## 1) Корень проекта

- **index.html** — *Editor Studio*: оболочка (адаптивная) переключает iframe на нужный редактор.
- **studio.manifest.js** — список редакторов Studio, сгруппированных по категориям (файлы / БД / система).
- **scenario-editor.html** — редактор сценариев.
- **event-editor.php** — редактор событий.
- **settings.html** — вкладка настроек (подключение к БД + просмотр источников списков).
- **vouchers-editor.html** — редактор ваучеров (CRUD напрямую в БД, использует ядро).
- **worlds-editor.html** — редактор миров (tw_worlds).
- **api.php** — файловый API для сценариев (список/загрузка/сохранение JSON в `scenarios/`).
- **api/db.php** — безопасный DB API (конфиг + справочники для DB Select).
- **api/db-crud.php** — универсальный CRUD API для DB-редакторов (ресурсы описываются в карте на сервере).

## 2) editor-core/ — единый набор UI-компонентов и утилит

### Инициализация
- **bootstrap.js** — единая точка входа для редакторов: подключает тему, toasts, запускает UI Manager.
- **ui-manager.js** — *Core UI Manager*: 
  - автозагрузка `DB Select` (`EditorCore.DB.init()`),
  - validation hints (`data-validate`),
  - синхронизация Toggle (`data-checked`).

### Компоненты и рендеринг полей
- **field-renderer.js** — рендерит поля по JSON-схеме:
  - Input (text/number/password)
  - Textarea
  - Select / Multiselect
  - Date / Time / Datetime
  - Checkbox / Toggle
  - Modal (через `ui.js`)
  - DB Select / DB Multiselect (`type: db_select`, `db_multiselect`)

### Схемы и конфигурация
- **registry.js** — библиотека схем (JSON-конфиги полей/компонентов/действий). Здесь заменены ID-поля на DB Select (quests/items/worlds/bots).
- **defaults.js** — дефолтные стили/параметры для форм и модалок.

### UI и утилиты
- **ui.js** — модальные окна, toasts, базовые UI-хелперы + Tabs/Table (не завязаны на конкретный редактор).
- **utils.js** — общие функции (toasts контейнер, helper’ы).
- **core.js** — базовая инициализация EditorCore.

### DB слой
- **db.js** — клиент к `api/db.php` + кеширование + автозаполнение `<select data-datasource="...">`.
- **db-crud.js** — клиент к `api/db-crud.php` (list/get/create/update/delete) для DB-редакторов.

### Упрощение создания редакторов
- **form-runtime.js** — минимальный schema-runtime: рендерит поля + биндинг + list add/remove (ускоряет создание новых редакторов).
- **db-crud.js** — клиент к `api/db-crud.php` (list/get/create/update/delete).

### Runtime для быстрых редакторов
- **form-runtime.js** — минимальный schema-runtime: 2-way binding, list add/remove, переиспользуемый в новых редакторах.
- **db-editor-runtime.js** — компактный runtime для CRUD-редакторов БД (список + форма + сохранение).

### Тема
- **editor-theme.css** — единые CSS variables + строгий UI.
- **tailwind-theme.js** — общий tailwind config (используется CDN Tailwind на страницах).

## 3) api/db.php — поддерживаемые справочники (whitelist)

- quests → `tw_quests_list` (ID/Name)
- items → `tw_items_list` (ID/Name)
- worlds → `tw_worlds` (ID/Name)
- skills → `tw_skills_list` (ID/Name)
- bots → `tw_bots_info` (ID/Name)

Конфиг хранится на сервере: **data/db-config.json** (пароль не возвращается в ответах).

## 4) Как добавить новый редактор

1) Создайте страницу: `my-editor.html` (или `.php`).
2) Подключите ядро (минимум):

```html
<link rel="stylesheet" href="editor-core/editor-theme.css" />
<script src="editor-core/core.js"></script>
<script src="editor-core/field-renderer.js"></script>
<script src="editor-core/db.js"></script>
<script src="editor-core/ui.js"></script>
<script src="editor-core/ui-manager.js"></script>
<script src="editor-core/defaults.js"></script>
<script src="editor-core/utils.js"></script>
<script src="editor-core/bootstrap.js"></script>
<script>
  EditorCore.bootstrapEditor({ mode: 'scenario' });
</script>
```

3) Добавьте редактор в `studio.manifest.js` (в нужную группу):

```js
{ key: 'my', title: 'Мой редактор', icon: 'fa-wand-magic', src: 'my-editor.html' },
```

## 5) DB Select одним объявлением

Пример поля, которое хранит **ID**, но показывает **названия из БД**:

```js
quest_id: createField('db_select', 'Квест', 0, { datasource: 'quests' })
```

Для списка ID:

```js
reward_items: createField('db_multiselect', 'Награды', [], { datasource: 'items' })
```
