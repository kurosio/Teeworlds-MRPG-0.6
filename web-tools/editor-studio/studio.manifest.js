// Editor Studio manifest.
// To add a new editor: add one item into the appropriate group.
// This keeps Studio shell clean and editor list centralized.
//
// guest: true — the editor is visible to the guest account. The guest can browse
// data there and its edits become change requests (nothing is written to the DB).
// Items without `guest: true` are hidden from guests and blocked when opened directly.
// This flag is only UX — the server enforces access independently (api/*.php).
// guestTitle — optional title shown to the guest instead of `title`.

window.STUDIO_MANIFEST = [
  {
    id: 'file_editors',
    title: 'Редакторы файлов',
    items: [
      { key: 'scenario', title: 'Сценарии', icon: 'fa-scroll', src: 'scenario-editor.html' },
    ]
  },
  {
    id: 'db_editors',
    title: 'Редакторы БД',
    items: [
      { key: 'vouchers', title: 'Ваучеры', icon: 'fa-ticket', src: 'vouchers-editor.html' },
      { key: 'crafts', title: 'Крафты', icon: 'fa-hammer', src: 'crafts-editor.html', guest: true },
      { key: 'warehouses', title: 'Warehouse', icon: 'fa-warehouse', src: 'warehouse-editor.html', guest: true },
      { key: 'items', title: 'Предметы', icon: 'fa-cube', src: 'items-editor.html', guest: true },
      { key: 'bots_info', title: 'Боты', icon: 'fa-robot', src: 'bots-info-editor.html', guest: true },
      { key: 'bots_mobs', title: 'Мобы', icon: 'fa-skull', src: 'mobs-editor.html', guest: true },
      { key: 'bots_npc', title: 'NPC', icon: 'fa-user-ninja', src: 'bots-npc-editor.html', guest: true },
      { key: 'quests', title: 'Квесты', icon: 'fa-scroll', src: 'quests-editor.html', guest: true },
      { key: 'dungeons', title: 'Подземелья', icon: 'fa-dungeon', src: 'dungeons-editor.html', guest: true },
      { key: 'worlds', title: 'Миры', icon: 'fa-globe', src: 'worlds-editor.html', guest: true },
      { key: 'aethers', title: 'Телепорты (Aether)', icon: 'fa-wand-magic-sparkles', src: 'aethers-editor.html', guest: true },
      { key: 'accounts', title: 'Управление аккаунтами', icon: 'fa-users-gear', src: 'accounts-editor.html' },
    ]
  },
  {
    id: 'system',
    title: 'Система',
    items: [
      { key: 'requests', title: 'Запросы гостей', guestTitle: 'Мои запросы', icon: 'fa-code-pull-request', src: 'change-requests.html', guest: true },
      { key: 'settings', title: 'Настройки', icon: 'fa-gear', src: 'settings.html' },
      { key: 'database', title: 'База данных', icon: 'fa-database', src: 'database.html' },
    ]
  }
];
