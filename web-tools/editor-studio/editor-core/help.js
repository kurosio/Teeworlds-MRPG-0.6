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
      'Выберите предмет в списке слева — справа появятся его поля.',
      'Группа предмета меняет набор полей: сначала выберите группу, затем заполняйте.',
      'Группа (Group) — раздел предмета; от неё зависит набор полей и фильтр в списке.',
    ],
    crafts: [
      'Крафт — рецепт: готовый предмет плюс список RequiredItems.',
      'Группу можно переименовать целиком кнопкой «Переименовать» — все рецепты ветки обновятся сразу.',
      'Предметы и миры выбираются по названию, в базу сохраняется их ID.',
    ],
    warehouses: [
      'Склад привязан к миру и типу торговли (покупка, продажа, хранение).',
      'Trades — список товаров склада; позиции можно объединять в группы.',
      'Поле «Исходная строка» показывает Trades точно так, как они лежат в БД.',
    ],
    quests: [
      'Слева дерево: квест → шаги мобов. Выберите квест или шаг — справа откроются настройки.',
      'Шаг содержит три вкладки: Основное, Диалог и Задания.',
      'Шаги можно перетаскивать в дереве, задания внутри шага — за ручку слева.',
    ],
    bots_mobs: [
      'Спавн моба привязан к миру и мобу: мир и моб выбираются по названию.',
      'Уровень и характеристики влияют на поведение моба в игре.',
      'Дроп — до пяти слотов: предмет, количество и шанс в процентах (вкладка «Дроп»).',
    ],
    bots_info: [
      'Профиль моба: имя, скин и экипировка.',
      'Скин подставляется из каталога скинов — превью появится справа.',
      'Оружие и броня выбираются из списка предметов — в базу сохраняются их ID.',
    ],
    bots_npc: [
      'NPC — это моб, стоящий в мире с репликами диалога.',
      'Позиция (X, Y) задаётся в координатах мира.',
      'Квест выдаётся NPC, если выбран в поле «Выдаёт квест».',
    ],
    worlds: [
      'Мир: тип, путь к файлу карты и игровые флаги.',
      'ID мира используется во всех остальных редакторах — меняйте его осторожно.',
      'Тип мира (default или dungeon) и флаги влияют на правила игры внутри карты.',
    ],
    aethers: [
      'Аэтр — точка телепорта: мир и координаты.',
      'Новые точки появляются в игре сразу после сохранения.',
      'Название точки видно игроку в списке телепортов — пишите понятно.',
    ],
    dungeons: [
      'Подземелье: мир, точка входа и настройки.',
      'Связанные записи (миры, мобы) выбираются по названию.',
      'DoorX и DoorY — точка входа в мир подземелья, TimeLimit — лимит времени в секундах.',
    ],
    vouchers: [
      'Ваучер — код с наградой и сроком действия: 0 означает «без ограничения».',
      'Флаг multi разрешает использовать код несколько раз.',
      'Награда — список предметов с количеством.',
      'Дата окончания задаётся в ValidUntil; пустое значение — код действует бессрочно.',
    ],
    accounts: [
      'Найдите аккаунт по ID, логину или нику — список можно ограничить поиском.',
      'Здесь же сброс пароля игрока и таймаута истечения сессии.',
      'Изменения применяются сразу после подтверждения.',
    ],
    scenario: [
      'Сценарий — дерево шагов; выберите шаг слева, справа откроются его компоненты.',
      'Шаги и компоненты можно перетаскивать: порядок сохраняется вместе со сценарием.',
      '«Сохранить» записывает сценарий целиком — проверьте изменения перед нажатием.',
    ],
    requests: [
      'Слева очередь запросов гостей, справа — подробности: что и на что меняется.',
      'Принять и применить — изменения уходят в базу одной транзакцией.',
      'Отклонить — изменения не применяются, гость увидит исходные данные.',
    ],
    guests: [
      'Каждый гость — отдельный логин и пароль, которые выдаёт администратор.',
      'Включайте только те вкладки, которые нужны этому гостю: остальные для него закрыты.',
      'Гость не может писать в базу: его правки приходят сюда как запросы на проверку.',
    ],
    settings: [
      'Здесь задаётся подключение к базе игры — оно нужно всем редакторам.',
      'Кнопка «Проверить» показывает, доступна ли база с текущими настройками.',
      'Профили подключения позволяют быстро переключаться между базами.',
      'Сетевой доступ расширяйте только для доверенных адресов, иначе API увидят посторонние.',
    ],
    database: [
      'Дамп — резервная копия базы; восстановление перезаписывает данные.',
      'Восстановление требует ввести имя базы для подтверждения.',
      'Скачайте дамп перед крупными правками — это самая быстрая точка возврата.',
    ],
  };

  const GENERIC = [
    'Выберите запись в списке слева — справа откроются её поля.',
    'Измените значения и нажмите «Сохранить».',
    '«Новый …» создаёт запись, «Удалить» удаляет её безвозвратно — потребуется подтверждение.',
  ];

  const GUEST_NOTE = 'Вы вошли как гость: «Сохранить» не меняет базу, а добавляет правку в черновик запроса. '
    + 'Администратор проверит его и применит — до этого вы видите свои значения с пометкой «изменено».';

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
    wrap.setAttribute('role', 'dialog');
    wrap.setAttribute('aria-modal', 'true');
    wrap.setAttribute('aria-label', 'Как пользоваться редактором');
    wrap.style.cssText = 'position:fixed;inset:0;z-index:2147482000;display:flex;align-items:center;justify-content:center;'
      + 'padding:20px;background:rgba(2,6,23,.72)';
    wrap.innerHTML = `
      <div style="max-width:520px;width:100%;background:var(--editor-surface,#121821);color:var(--editor-text,#edf3fb);border:1px solid var(--editor-border,#273142);border-radius:18px;padding:22px;box-shadow:0 24px 60px rgba(0,0,0,.45)">
        <div style="display:flex;align-items:flex-start;justify-content:space-between;gap:12px">
          <div style="font-size:17px;font-weight:700"><i class="fa-solid fa-circle-question" style="color:var(--editor-primary,#6366f1)"></i> Как это работает</div>
          <button type="button" class="editor-icon-btn" data-help-close title="Закрыть (Esc)"><i class="fa-solid fa-xmark"></i></button>
        </div>
        <ol style="margin:14px 0 0;padding-left:20px;font-size:13.5px;line-height:1.6;color:var(--editor-text,#edf3fb)">
          ${steps.map((s) => `<li style="margin-bottom:6px">${s}</li>`).join('')}
        </ol>
        <div style="margin-top:14px;padding-top:12px;border-top:1px solid var(--editor-soft-border,rgba(255,255,255,.08));font-size:12.5px;color:var(--editor-text-muted,#9aa8bc)">
          <b>Ctrl+S</b> — сохранить · <b>Esc</b> — закрыть это окно
        </div>
        ${isGuest() ? `<div class="guest-row-hint" style="margin:14px 0 0"><i class="fa-solid fa-user-secret"></i><div>${GUEST_NOTE}</div></div>` : ''}
        <div style="margin-top:16px;text-align:right">
          <button type="button" class="editor-btn editor-btn-primary" data-help-close>Понятно</button>
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
    btn.title = 'Как пользоваться (?)';
    btn.setAttribute('aria-label', 'Как пользоваться редактором');
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
