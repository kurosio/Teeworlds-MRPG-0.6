// Editor Studio manifest.
// To add a new editor: add one item into the appropriate group.
// This keeps Studio shell clean and editor list centralized.

window.STUDIO_MANIFEST = [
  {
    id: 'file_editors',
    title: 'Редакторы файлов',
    items: [
      { key: 'scenario', title: 'Сценарии', icon: 'fa-scroll', src: 'scenario-editor.html' },
      { key: 'event', title: 'События', icon: 'fa-bolt', src: 'event-editor.php' },
    ]
  },
  {
    id: 'db_editors',
    title: 'Редакторы БД',
    items: [
      { key: 'vouchers', title: 'Ваучеры', icon: 'fa-ticket', src: 'vouchers-editor.html' },
      { key: 'crafts', title: 'Крафты (tw_crafts_list)', icon: 'fa-hammer', src: 'crafts-editor.html' },
      { key: 'items', title: 'Предметы (tw_items_list)', icon: 'fa-cube', src: 'items-editor.html' },
      { key: 'bots_mobs', title: 'Мобы (tw_bots_mobs)', icon: 'fa-skull', src: 'mobs-editor.html' },
      { key: 'worlds', title: 'Миры (tw_worlds)', icon: 'fa-globe', src: 'worlds-editor.html' },
    ]
  },
  {
    id: 'system',
    title: 'Система',
    items: [
      { key: 'settings', title: 'Настройки', icon: 'fa-gear', src: 'settings.html' },
    ]
  }
];
