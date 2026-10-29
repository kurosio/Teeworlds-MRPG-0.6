// Editor Studio — единая подсказка «Как это работает» и Ctrl+S для всех редакторов.
//
// Идея: у каждой вкладки одинаковое поведение, поэтому и объяснение короткое:
// что делать по шагам и какие есть горячие клавиши. Подсказка спрятана за
// кнопкой «?» в шапке страницы, чтобы не занимать место у опытных пользователей.
(() => {
  if (window.EditorCore?.Help) return;

  /** Помощь по вкладкам. Ключ — data-editor у <body>. */
  const STEPS = {
    items: [
      'Pick an item in the list on the left — its fields appear on the right.',
      'The item group defines the field set: choose the group first, then fill in the fields.',
      'Group is the item section; it defines the field set and the list filter.',
    ],
    crafts: [
      'A craft is a recipe: the result item plus its RequiredItems list.',
      'The whole group can be renamed with “Rename” — every recipe in the branch updates at once.',
      'Items and worlds are picked by name; their IDs are stored in the database.',
    ],
    warehouses: [
      'A warehouse belongs to a world and a trade type (buy, sell, storage).',
      'Trades is the warehouse stock list; entries can be grouped.',
      'The “Raw value” field shows Trades exactly as they are stored in the database.',
    ],
    quests: [
      'The tree on the left lists quests → mob steps. Select a quest or a step to edit it on the right.',
      'A step has three tabs: General, Dialogue and Objectives.',
      'On the “General” tab, a position is pinned on the map or entered manually; the world is selected in the card.',
      'The map will not apply a point on a solid, death or no-hook tile — move it to a free tile.',
      'Drag steps in the tree, and objectives inside a step by the handle on the left.',
      'On the “Dialogue” tab, the buttons above a line insert a bot, item or world name, and the chips below the field show what is already inserted.',
    ],
    bots_mobs: [
      'A mob spawn belongs to a world and a mob; both are picked by name.',
      'The map shows forbidden tiles, spawns, the wander radius (green) and the active radius (blue); a point on a wall cannot be applied.',
      'The wander radius is the maximum distance from the spawn point — crossing it teleports the mob back.',
      'The active radius is the field of view in which the mob reacts to players.',
      'The level and stats affect how the mob behaves in game.',
      'Drop is up to five slots: item, amount and chance in percent (the “Drop” tab).',
    ],
    bots_info: [
      'Mob profile: name, skin and equipment.',
      'The skin comes from the skin catalogue — a preview appears on the right.',
      'Weapons and armour are picked from the item list; their IDs are stored in the database.',
    ],
    bots_npc: [
      'An NPC is a mob placed in a world with dialogue lines.',
      'The position (X, Y) is set in world coordinates; the map highlights solid, death and spawn tiles.',
      'A quest is given by the NPC if it is set in the “Gives quest” field.',
      'The buttons above a dialogue line insert a bot, item or world name, and the game placeholders — the game substitutes them for the player.',
    ],
    worlds: [
      'A world: type, map file path and game flags.',
      'The world ID is used by every other editor — change it carefully.',
      'The world type (default or dungeon) and flags define the in-map game rules.',
    ],
    aethers: [
      'An aether is a teleport point: world and coordinates.',
      'New points appear in game right after saving.',
      'Players see the point name in the teleport list — keep it clear.',
    ],
    dungeons: [
      'A dungeon: world, entrance point and settings.',
      'Linked records (worlds, mobs) are picked by name.',
      'DoorX and DoorY are the dungeon entrance, TimeLimit is the time limit in seconds.',
    ],
    vouchers: [
      'A voucher is a code with a reward and an expiry: 0 means “no limit”.',
      'The multi flag allows reusing the code.',
      'The reward is a list of items with amounts.',
      'The expiry date is set in ValidUntil; an empty value means the code never expires.',
    ],
    accounts: [
      'Find an account by ID, login or nickname — narrow the list with the search.',
      'This is also where the player password and session timeout are reset.',
      'Changes are applied right after confirmation.',
    ],
    scenario: [
      'A scenario is a tree of steps; select a step on the left to see its components on the right.',
      'Steps and components can be dragged; the order is saved with the scenario.',
      '“Save” writes the whole scenario — review the changes before pressing it.',
    ],
    requests: [
      'On the left is the guest request queue, on the right the details: what changes into what.',
      'Approve and apply — the changes go to the database in one transaction.',
      'Reject — nothing is applied and the guest sees the original data.',
    ],
    guests: [
      'Every guest has a separate login and password issued by the administrator.',
      'Enable only the tabs this guest needs: the rest stay closed for them.',
      'A guest cannot write to the database: their edits arrive here as requests for review.',
    ],
    settings: [
      'The game database connection is set here — every editor needs it.',
      'The “Test” button shows whether the database is reachable with the current settings.',
      'Connection profiles let you switch between databases quickly.',
    ],
    database: [
      'A dump is a database backup; restoring overwrites the data.',
      'Restoring requires typing the database name to confirm.',
      'Download a dump before big edits — it is the fastest way back.',
    ],
  };

  const GENERIC = [
    'Pick a record in the list on the left — its fields appear on the right.',
    'Change the values and press “Save”.',
    '“New …” creates a record, “Delete” removes it for good — confirmation required.',
  ];

  const GUEST_NOTE = 'You are signed in as a guest: “Save” does not touch the database, it adds the edit to a request draft. '
    + 'The administrator reviews and applies it — until then you see your values marked as “changed”. '
    + 'The arrow button next to the “edited” mark restores a single field to the database value.';

  const isGuest = () => {
    try { return window.EditorCore?.Guest?.isGuest?.() === true; } catch { return false; }
  };

  // data-editor ставится на странице, а ключи вкладок берутся из manifest —
  // у двух вкладок имена расходятся, поэтому держим явные псевдонимы.
  const ALIASES = { mobs: 'bots_mobs', warehouse: 'warehouses' };

  const editKey = () => {
    const raw = String(document.body?.dataset?.editor || '').trim();
    return ALIASES[raw] || raw;
  };

  // ── Подсказка ────────────────────────────────────────────────────────────
  let panel = null;

  const buildPanel = () => {
    const key = editKey();
    const steps = STEPS[key] || GENERIC;
    const wrap = document.createElement('div');
    wrap.id = 'editor-help-panel';
    wrap.className = 'fixed inset-0 editor-modal-backdrop z-[950] flex items-center justify-center p-4';
    wrap.setAttribute('role', 'dialog');
    wrap.setAttribute('aria-modal', 'true');
    wrap.setAttribute('aria-label', 'How to use the editor');
    wrap.innerHTML = `
      <div class="editor-modal-content editor-modal-shell w-full max-w-lg">
        <div class="editor-modal-head flex items-start justify-between gap-3">
          <div class="editor-modal-title flex items-center gap-2.5">
            <i class="fa-solid fa-circle-question editor-tone-accent"></i>
            <span>How it works</span>
          </div>
          <button type="button" class="editor-icon-btn" data-help-close title="Close (Esc)"><i class="fa-solid fa-xmark"></i></button>
        </div>
        <div class="editor-modal-body space-y-4">
          <ol class="list-decimal pl-5 space-y-2 text-sm leading-relaxed">
            ${steps.map((s) => `<li>${s}</li>`).join('')}
          </ol>
          <div class="editor-section pt-3">
            <div class="text-xs font-semibold uppercase tracking-wider editor-muted-text mb-2">Keyboard shortcuts</div>
            <div class="flex flex-wrap gap-1.5 text-xs">
              <span class="editor-badge"><b>Ctrl+S</b> — save</span>
              <span class="editor-badge"><b>Ctrl+K</b> — commands and view</span>
              <span class="editor-badge"><b>/</b> — search</span>
              <span class="editor-badge"><b>Alt+N</b> — create</span>
              <span class="editor-badge"><b>↑/↓</b> — through the list</span>
              <span class="editor-badge"><b>Ctrl+Tab</b> — switch editor</span>
              <span class="editor-badge"><b>Alt+←/→</b> — back/forward</span>
              <span class="editor-badge"><b>?</b> — this help</span>
            </div>
          </div>
          ${isGuest() ? `<div class="guest-row-hint"><i class="fa-solid fa-user-secret"></i><div>${GUEST_NOTE}</div></div>` : ''}
        </div>
        <div class="editor-modal-foot flex justify-end">
          <button type="button" class="editor-btn editor-btn-primary" data-help-close>Got it</button>
        </div>
      </div>`;
    return wrap;
  };

  const closeHelp = () => {
    panel?.remove();
    panel = null;
    document.removeEventListener('keydown', onPanelKey, true);
  };

  const onPanelKey = (event) => {
    if (event.key === 'Escape') { event.preventDefault(); closeHelp(); }
  };

  const openHelp = () => {
    if (panel) { closeHelp(); return; }
    panel = buildPanel();
    panel.addEventListener('click', (event) => {
      if (event.target === panel || event.target.closest?.('[data-help-close]')) closeHelp();
    });
    document.body.appendChild(panel);
    document.addEventListener('keydown', onPanelKey, true);
    panel.querySelector('[data-help-close]')?.focus?.();
  };

  // Куда поставить кнопку «?»: сначала в блок действий в шапке, иначе рядом
  // с последней кнопкой строки заголовка, иначе — в угол экрана.
  const findActionsHost = () => {
    const actions = document.querySelector('.editor-page-actions');
    if (actions) return { mode: 'append', el: actions };

    const title = document.querySelector('.editor-page-title h1') || document.querySelector('h1');
    let row = title?.parentElement;
    for (let depth = 0; depth < 2 && row; depth += 1) {
      const buttons = [...row.querySelectorAll('button')].filter((el) => !el.closest('[data-help-button]'));
      if (buttons.length && !row.querySelector('.editor-card table')) {
        return { mode: 'after', el: buttons[buttons.length - 1] };
      }
      row = row.parentElement;
    }
    return null;
  };

  const mountButton = () => {
    // На главной оболочке (data-editor="home") подсказка не нужна.
    if (!editKey() || editKey() === 'home') return;
    if (document.querySelector('[data-help-button]')) return;

    const btn = document.createElement('button');
    btn.type = 'button';
    btn.dataset.helpButton = '1';
    btn.className = 'editor-btn editor-btn-secondary';
    btn.title = 'How to use (?)';
    btn.setAttribute('aria-label', 'How to use the editor');
    btn.innerHTML = '<i class="fa-solid fa-circle-question"></i>';
    btn.addEventListener('click', openHelp);

    const host = findActionsHost();
    if (!host) {
      btn.style.cssText = 'position:fixed;right:14px;bottom:14px;z-index:2147481000;box-shadow:0 8px 24px rgba(0,0,0,.35)';
      document.body.appendChild(btn);
      return;
    }
    if (host.mode === 'append') host.el.appendChild(btn);
    else host.el.insertAdjacentElement('afterend', btn);
  };

  // ── Ctrl+S ───────────────────────────────────────────────────────────────
  // Единая горячая клавиша: нажимает основную кнопку сохранения страницы.
  // Страницы со своим обработчиком помечают <body data-own-save-shortcut="1">.
  const SAVE_SELECTORS = [
    '[data-editor-role="save"]',
    '#btn-save', '#saveBtn', '#save-btn', '#save-button',
    '[data-action="save"]',
  ];

  const bindSaveShortcut = () => {
    if (document.body?.dataset?.ownSaveShortcut === '1') return;
    document.addEventListener('keydown', (event) => {
      if (!(event.ctrlKey || event.metaKey) || event.altKey) return;
      if (String(event.key).toLowerCase() !== 's') return;
      const button = SAVE_SELECTORS
        .map((sel) => document.querySelector(sel))
        .find((el) => el && !el.classList.contains('hidden'));
      if (!button) return;
      // Never let the browser open "save page" instead of saving the record.
      event.preventDefault();
      if (!button.disabled) button.click();
    });
  };

  const init = () => {
    mountButton();
    bindSaveShortcut();
    document.addEventListener('keydown', (event) => {
      if (event.key !== '?' || event.ctrlKey || event.metaKey || event.altKey) return;
      const tag = String(document.activeElement?.tagName || '').toLowerCase();
      if (tag === 'input' || tag === 'textarea' || document.activeElement?.isContentEditable) return;
      event.preventDefault();
      openHelp();
    });
  };

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init, { once: true });
  else init();

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.Help = { open: openHelp, close: closeHelp, steps: (key) => STEPS[key] || GENERIC };
})();
