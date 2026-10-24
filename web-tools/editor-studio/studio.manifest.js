// Editor Studio manifest.
// To add a new editor: add one item into the appropriate group.
// This keeps Studio shell clean and editor list centralized.
//
// Guest access is NOT defined here: the administrator grants editor tabs to
// each guest account individually (see "Гостевые пользователи" →
// api/guests.php, catalogue editor_guest_page_catalog() in api/db-core.php).
// The `key` of an item must match the catalogue key (items, crafts, quests…).
// `adminOnly: true` — never shown to guests.
// `guestTitle` — optional title shown to a guest instead of `title`.
// `hint` — short plain-language description shown under the title in the sidebar.
// This manifest is only UX: the server enforces the same tab list independently
// (api/db-crud.php, api/change-requests.php).

window.STUDIO_MANIFEST = [
  {
    id: 'file_editors',
    title: 'Редакторы файлов',
    items: [
      { key: 'scenario', title: 'Сценарии', icon: 'fa-scroll', src: 'scenario-editor.html', adminOnly: true, hint: 'Шаги и компоненты сценария' },
    ]
  },
  {
    id: 'db_editors',
    title: 'Редакторы БД',
    items: [
      { key: 'vouchers', title: 'Ваучеры', icon: 'fa-ticket', src: 'vouchers-editor.html', adminOnly: true, hint: 'Подарочные коды и награды' },
      { key: 'crafts', title: 'Крафты', icon: 'fa-hammer', src: 'crafts-editor.html', hint: 'Рецепты и ингредиенты' },
      { key: 'warehouses', title: 'Склады и магазины', icon: 'fa-warehouse', src: 'warehouse-editor.html', hint: 'Товары складов по мирам' },
      { key: 'items', title: 'Предметы', icon: 'fa-cube', src: 'items-editor.html', hint: 'Каталог предметов' },
      { key: 'bots_info', title: 'Боты', icon: 'fa-robot', src: 'bots-info-editor.html', hint: 'Профили мобов' },
      { key: 'bots_mobs', title: 'Мобы', icon: 'fa-skull', src: 'mobs-editor.html', hint: 'Спавны, уровни и дроп' },
      { key: 'bots_npc', title: 'NPC', icon: 'fa-user-ninja', src: 'bots-npc-editor.html', hint: 'NPC и их диалоги' },
      { key: 'quests', title: 'Квесты', icon: 'fa-scroll', src: 'quests-editor.html', hint: 'Квесты, шаги и задания' },
      { key: 'dungeons', title: 'Подземелья', icon: 'fa-dungeon', src: 'dungeons-editor.html', hint: 'Вход, сценарий и таймер' },
      { key: 'worlds', title: 'Миры', icon: 'fa-globe', src: 'worlds-editor.html', hint: 'Миры и их флаги' },
      { key: 'aethers', title: 'Аэтры (телепорты)', icon: 'fa-wand-magic-sparkles', src: 'aethers-editor.html', hint: 'Точки телепортации' },
      { key: 'accounts', title: 'Аккаунты игроков', icon: 'fa-users-gear', src: 'accounts-editor.html', adminOnly: true, hint: 'Логины, пароли, таймауты' },
    ]
  },
  {
    id: 'system',
    title: 'Система',
    items: [
      { key: 'requests', title: 'Запросы гостей', guestTitle: 'Мои запросы', icon: 'fa-code-pull-request', src: 'change-requests.html', hint: 'Проверка правок гостей' },
      { key: 'guests', title: 'Гостевые пользователи', icon: 'fa-user-shield', src: 'guests-editor.html', adminOnly: true, hint: 'Доступы гостей по вкладкам' },
      { key: 'settings', title: 'Настройки', icon: 'fa-gear', src: 'settings.html', adminOnly: true, hint: 'Подключение к базе' },
      { key: 'database', title: 'База данных', icon: 'fa-database', src: 'database.html', adminOnly: true, hint: 'Дамп и восстановление' },
    ]
  }
];
