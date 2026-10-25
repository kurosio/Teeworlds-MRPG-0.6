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
    title: 'File editors',
    items: [
      { key: 'scenario', title: 'Scenarios', icon: 'fa-scroll', src: 'scenario-editor.html', adminOnly: true, hint: 'Scenario steps and components' },
    ]
  },
  {
    id: 'db_editors',
    title: 'Database editors',
    items: [
      { key: 'vouchers', title: 'Vouchers', icon: 'fa-ticket', src: 'vouchers-editor.html', adminOnly: true, hint: 'Gift codes and rewards' },
      { key: 'crafts', title: 'Crafts', icon: 'fa-hammer', src: 'crafts-editor.html', hint: 'Recipes and ingredients' },
      { key: 'warehouses', title: 'Warehouses and shops', icon: 'fa-warehouse', src: 'warehouse-editor.html', hint: 'Warehouse stock by world' },
      { key: 'items', title: 'Items', icon: 'fa-cube', src: 'items-editor.html', hint: 'Item catalogue' },
      { key: 'bots_info', title: 'Bots', icon: 'fa-robot', src: 'bots-info-editor.html', hint: 'Mob profiles' },
      { key: 'bots_mobs', title: 'Mobs', icon: 'fa-skull', src: 'mobs-editor.html', hint: 'Spawns, levels and drop' },
      { key: 'bots_npc', title: 'NPC', icon: 'fa-user-ninja', src: 'bots-npc-editor.html', hint: 'NPCs and their dialogues' },
      { key: 'quests', title: 'Quests', icon: 'fa-scroll', src: 'quests-editor.html', hint: 'Quests, steps and objectives' },
      { key: 'dungeons', title: 'Dungeons', icon: 'fa-dungeon', src: 'dungeons-editor.html', hint: 'Entrance, scenario and timer' },
      { key: 'worlds', title: 'Worlds', icon: 'fa-globe', src: 'worlds-editor.html', hint: 'Worlds and their flags' },
      { key: 'nodes_collecting', title: 'Collecting nodes', icon: 'fa-seedling', src: 'nodes-collecting-editor.html', hint: 'Ore, plants and fish by world and switch' },
      { key: 'aethers', title: 'Aethers (teleports)', icon: 'fa-wand-magic-sparkles', src: 'aethers-editor.html', hint: 'Teleport points' },
      { key: 'accounts', title: 'Player accounts', icon: 'fa-users-gear', src: 'accounts-editor.html', adminOnly: true, hint: 'Logins, passwords, timeouts' },
    ]
  },
  {
    id: 'system',
    title: 'System',
    items: [
      { key: 'requests', title: 'Guest requests', guestTitle: 'My requests', icon: 'fa-code-pull-request', src: 'change-requests.html', hint: 'Reviewing guest edits' },
      { key: 'guests', title: 'Guest users', icon: 'fa-user-shield', src: 'guests-editor.html', adminOnly: true, hint: 'Guest access by tab' },
      { key: 'settings', title: 'Settings', icon: 'fa-gear', src: 'settings.html', adminOnly: true, hint: 'Database connection' },
      { key: 'database', title: 'Database', icon: 'fa-database', src: 'database.html', adminOnly: true, hint: 'Dump and restore' },
    ]
  }
];
