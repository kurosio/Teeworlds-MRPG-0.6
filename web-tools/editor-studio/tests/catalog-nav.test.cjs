/**
 * tests/catalog-nav.test.cjs — навигация «путь + плоский список» в Крафтах
 * и стабильное дерево в Квестах.
 *
 * Крафты: вместо селектов-фильтров и сворачиваемого трёхэтажного дерева —
 * хлебные крошки категории, чипсы следующего уровня и плоский список.
 * Чистая логика пути живёт в editor-core/utils.js (craftNav*) и проверяется
 * здесь напрямую.
 *
 * Квесты: выбранный квест больше не «прыгает» в начало дерева со сбросом
 * скролла; поиск находит квест по его шагам (EditorCore.utils.matchQuestTree);
 * в тулбаре — контекстная полоса (квест, соседние шаги, цепочка NextQuestID),
 * общая планка записей там отключена (data-record-nav-off).
 */
const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');
const readRoot = (name) => fs.readFileSync(path.join(root, name), 'utf8');


// Результаты из VM-контекста нельзя сравнивать deepStrictEqual (чужие
// прототипы) — сравниваем по JSON.
const eq = (actual, expected, message) => assert.equal(JSON.stringify(actual), JSON.stringify(expected), message);

function utilsHarness() {
  const window = { addEventListener() {}, dispatchEvent() {}, setTimeout, clearTimeout };
  const context = vm.createContext({ window, console, setTimeout, clearTimeout, crypto: require('node:crypto') });
  vm.runInContext(source('utils.js'), context);
  return context.window.EditorCore.utils;
}

// ── Крафты: сегменты пути ───────────────────────────────────────────────────

test('craftNavSegments maps every GroupName shape to a segment chain', () => {
  const { craftNavSegments } = utilsHarness();
  const flat = (segs) => segs.map((s) => `${s.kind}:${s.name}`);

  eq(flat(craftNavSegments('|Armor|Tank:Helmets')), ['main:Armor', 'group:Tank', 'sub:Helmets']);
  eq(flat(craftNavSegments('Tank:Helmets')), ['group:Tank', 'sub:Helmets']);
  eq(flat(craftNavSegments('Tank')), ['group:Tank']);
  eq(flat(craftNavSegments('|Armor|')), ['main:Armor', 'group:']);
  eq(flat(craftNavSegments('')), ['group:']);
});

test('craftNavInPath is a kind-aware prefix match', () => {
  const { craftNavSegments, craftNavInPath } = utilsHarness();
  const segs = craftNavSegments('|Armor|Tank:Helmets');

  assert.equal(craftNavInPath(segs, []), true);
  assert.equal(craftNavInPath(segs, [{ kind: 'main', name: 'Armor' }]), true);
  assert.equal(craftNavInPath(segs, segs), true);
  // Группа с тем же именем не равна главной группе — вид сегмента важен.
  assert.equal(craftNavInPath(segs, [{ kind: 'group', name: 'Armor' }]), false);
  assert.equal(craftNavInPath(segs, [{ kind: 'main', name: 'Armor' }, { kind: 'group', name: 'Mage' }]), false);
  // Путь длиннее цепочки рецепта не может совпасть.
  assert.equal(craftNavInPath(craftNavSegments('Tank'), segs), false);
});

test('craftNavChildren counts recipes per next segment, empty bucket first', () => {
  const { craftNavChildren } = utilsHarness();
  const rows = [
    { GroupName: '|Armor|Tank:Helmets' },
    { GroupName: '|Armor|Tank:Shields' },
    { GroupName: '|Armor|Mage' },
    { GroupName: 'Loose:Bits' },
    { GroupName: '' },
  ];
  const pick = (r) => r.GroupName;

  // Корень: «без группы» первым, дальше по алфавиту; смешаны main и group.
  eq(
    craftNavChildren(rows, [], pick).map((c) => `${c.kind}:${c.name}=${c.count}`),
    ['group:=1', 'main:Armor=3', 'group:Loose=1'],
  );
  // Внутри главной группы — её группы; чужие ветки не учитываются.
  eq(
    craftNavChildren(rows, [{ kind: 'main', name: 'Armor' }], pick).map((c) => `${c.kind}:${c.name}=${c.count}`),
    ['group:Mage=1', 'group:Tank=2'],
  );
  // Листовой путь: детей нет.
  eq(
    craftNavChildren(rows, [
      { kind: 'main', name: 'Armor' }, { kind: 'group', name: 'Tank' }, { kind: 'sub', name: 'Helmets' },
    ], pick),
    [],
  );
});

test('craftNavPathFields and craftNavPrunePath keep navigation honest', () => {
  const { craftNavPathFields, craftNavPrunePath } = utilsHarness();

  eq(
    craftNavPathFields([{ kind: 'main', name: 'Armor' }, { kind: 'group', name: 'Tank' }]),
    { main: 'Armor', group: 'Tank', sub: '' },
  );
  eq(craftNavPathFields([]), { main: '', group: '', sub: '' });

  const rows = [{ GroupName: '|Armor|Tank:Helmets' }];
  const dead = [{ kind: 'main', name: 'Gone' }, { kind: 'group', name: 'Tank' }];
  const pruned = craftNavPrunePath(rows, dead, (r) => r.GroupName);
  eq(pruned, [], 'полностью мёртвый путь обрезается в корень');
  assert.equal(dead.length, 2, 'исходный путь не мутируется');

  const halfDead = [{ kind: 'main', name: 'Armor' }, { kind: 'group', name: 'Mage' }];
  eq(
    craftNavPrunePath(rows, halfDead, (r) => r.GroupName),
    [{ kind: 'main', name: 'Armor' }],
    'живой префикс сохраняется',
  );
});

// ── Квесты: стабильный порядок и поиск по шагам ─────────────────────────────

const QUESTS = [
  { ID: 3, Name: 'Third', Flags: 'Type side' },
  { ID: 1, Name: 'First steps', Flags: 'Type main' },
  { ID: 2, Name: 'Second', Flags: 'Type main' },
];
const STEPS = [
  { ID: 12, QuestID: 1, Step: 2, BotID: 7 },
  { ID: 11, QuestID: 1, Step: 1, BotID: 5 },
  { ID: 21, QuestID: 2, Step: 1, BotID: 9 },
];
const BOTS = { 5: 'Gatekeeper', 7: 'Blacksmith', 9: 'Hermit' };
const treeArgs = (utils, extra = {}) => ({
  quests: QUESTS,
  steps: STEPS,
  tokens: [],
  questText: (q) => `${q.Name} ${q.ID}`,
  stepText: (s) => `${BOTS[s.BotID] || ''} ${s.ID}`,
  ...extra,
});

test('matchQuestTree keeps ID order and sorts steps by position', () => {
  const utils = utilsHarness();
  const entries = utils.matchQuestTree(treeArgs(utils));
  eq(entries.map((e) => e.quest.ID), [1, 2, 3], 'порядок квестов — по ID, не по входу');
  eq(entries[0].steps.map((s) => s.ID), [11, 12], 'шаги — по позиции');
  assert.equal(entries[0].totalSteps, 2);
});

test('matchQuestTree finds a quest through its steps (mob search)', () => {
  const utils = utilsHarness();
  const byMob = utils.matchQuestTree(treeArgs(utils, { tokens: ['blacksmith'] }));
  eq(byMob.map((e) => e.quest.ID), [1], 'квест найден по мобу шага, хотя его имя не совпало');
  eq(byMob[0].steps.map((s) => s.ID), [12], 'показан только совпавший шаг');
  assert.equal(byMob[0].questMatched, false);

  const byQuest = utils.matchQuestTree(treeArgs(utils, { tokens: ['first'] }));
  eq(byQuest.map((e) => e.quest.ID), [1]);
  eq(byQuest[0].steps.map((s) => s.ID), [11, 12], 'совпал квест — видны все его шаги');

  const none = utils.matchQuestTree(treeArgs(utils, { tokens: ['zzz'] }));
  eq(none, []);

  const twoTokens = utils.matchQuestTree(treeArgs(utils, { tokens: ['first', 'blacksmith'] }));
  eq(twoTokens, [], 'токены объединяются через И внутри одной сущности');
});

test('matchQuestTree honours the questAllowed (type) filter', () => {
  const utils = utilsHarness();
  const entries = utils.matchQuestTree(treeArgs(utils, {
    questAllowed: (q) => String(q.Flags).includes('main'),
  }));
  eq(entries.map((e) => e.quest.ID), [1, 2]);
});

// ── Статика страниц ─────────────────────────────────────────────────────────

test('crafts editor: breadcrumb catalog replaced selects and the collapsible tree', () => {
  const html = readRoot('crafts-editor.html');
  assert.ok(!html.includes('id="filter-group"') && !html.includes('id="filter-sub"'), 'остались селекты-фильтры');
  assert.ok(!html.includes('btn-collapse-all') && !html.includes('collapsedGroups'), 'осталась механика сворачивания');
  assert.ok(html.includes('id="crumb-bar"') && html.includes('id="cat-chips"'), 'нет хлебных крошек/чипсов');
  assert.ok(html.includes('craftNavChildren') && html.includes('craftNavInPath'), 'список не использует логику пути');
  assert.ok(html.includes('ensureRowVisible'), 'выбранный рецепт не гарантированно виден');
  assert.ok(html.includes("mrpgEditor:craftsNav:v1"), 'место в каталоге не запоминается');
  assert.ok(html.includes('DeepLink?.markHandled'), 'прямая ссылка не открывает рецепт');
  // Переименование категории доступно из крошек, а не карандашами по всему дереву.
  assert.ok(html.includes('data-nav-rename'), 'нет переименования текущей категории');
  assert.ok(!html.includes('crafts-group-rename'), 'остались карандаши на каждом заголовке');
  // Sticky-заголовок живёт внутри своей секции и имеет непрозрачный фон:
  // иначе все заголовки прилипают в одну точку и просвечивают друг сквозь друга.
  assert.ok(html.includes('<section class="crafts-section">'), 'разделы списка не обёрнуты для sticky-заголовков');
  assert.ok(/\.crafts-section-header\{[^}]*background:var\(--editor-surface-2\)/.test(html), 'sticky-заголовок не на непрозрачной поверхности');
  // Ингредиенты, мир и цена — одна компактная строка, а не крупные editor-badge.
  assert.ok(html.includes('class="crafts-row-meta"'), 'метаданные рецепта не компактны');
  assert.ok(!html.includes('<span class="editor-badge">ingr:'), 'осталась крупная плашка ингредиентов');
});

test('quests editor: stable tree, context strip, remembered place', () => {
  const html = readRoot('quests-editor.html');
  assert.ok(!html.includes('lastQuestScrollId'), 'остался «прыжок» выбранного квеста в начало');
  assert.ok(!/DOM\.tree\.scrollTop = 0/.test(html), 'остался сброс скролла дерева при выборе');
  assert.ok(html.includes('matchQuestTree'), 'дерево не использует поиск по шагам');
  assert.ok(html.includes('id="questNavStrip"') && html.includes('renderNavStrip'), 'нет контекстной полосы навигации');
  assert.ok(html.includes('nextQuestInChain') && html.includes('prevQuestInChain'), 'нет ходьбы по цепочке');
  assert.ok(html.includes("mrpgEditor:questsNav:v1") && html.includes('restoreQuestNav'), 'место в дереве не запоминается');
  assert.ok(html.includes('data-record-nav-off'), 'дерево не отказалось от общей планки записей');
  // Контракты дерева сохранены для deep-link/request-context/drag-and-drop.
  assert.ok(html.includes('data-node-type="step"') && html.includes('data-quest-toggle'), 'сломан контракт узлов дерева');
  assert.ok(html.includes('data-step-level-id') && html.includes('data-step-action="pin"'), 'сломаны быстрые действия шага');
});

test('shared record nav bar respects data-record-nav-off but keeps keyboard wiring', () => {
  const ui = source('ui-manager.js');
  assert.ok(ui.includes('[data-record-nav-off]'), 'ui-manager не знает про opt-out');
  const mount = ui.slice(ui.indexOf('const mountRecordNav'), ui.indexOf('const bindShortcuts'));
  assert.ok(mount.includes("list.addEventListener('keydown'"), 'стрелки в списке должны остаться');
  assert.ok(mount.indexOf('[data-record-nav-off]') < mount.indexOf('editor-record-nav'), 'opt-out проверяется до создания планки');
});

// ── Поведение в jsdom: дерево квестов без общей планки, но со стрелками ─────

let JSDOM = null;
try { ({ JSDOM } = require('jsdom')); } catch { JSDOM = null; }
const skip = JSDOM ? false : 'jsdom не установлен (npm install --no-save jsdom)';

const tick = async () => {
  await new Promise((resolve) => setTimeout(resolve, 10));
  await new Promise((resolve) => setTimeout(resolve, 10));
};

test('quest tree: no shared bar under data-record-nav-off, arrows still walk nodes', { skip }, async () => {
  const dom = new JSDOM(`<!DOCTYPE html><body data-editor="quests">
    <input id="searchInput" type="search">
    <div id="tree" data-record-nav-off></div>
  </body>`, { url: 'http://studio.local/quests-editor.html', runScripts: 'outside-only' });
  const { window } = dom;
  const { document } = window;
  window.eval(source('utils.js'));

  // Имитация дерева квестов: клик по заголовку/шапке шага выбирает узел и
  // перерисовывает дерево (как renderTree на странице).
  const tree = document.querySelector('#tree');
  const render = (type, id) => {
    const sel = (t, i) => (t === type && i === id ? ' tree-selected' : '');
    tree.innerHTML = `
      <div class="quest-node${sel('quest', 1)}" data-node-type="quest" data-id="1">
        <div class="quest-node-header"><div class="quest-node-title"><span>First</span></div></div>
        <div class="step-list">
          <div class="step-node${sel('step', 11)}" data-node-type="step" data-id="11"><div class="step-node-header">Gatekeeper</div></div>
          <div class="step-node${sel('step', 12)}" data-node-type="step" data-id="12"><div class="step-node-header">Blacksmith</div></div>
        </div>
      </div>
      <div class="quest-node${sel('quest', 2)}" data-node-type="quest" data-id="2">
        <div class="quest-node-header"><div class="quest-node-title"><span>Second</span></div></div>
      </div>`;
  };
  tree.addEventListener('click', (event) => {
    const step = event.target.closest('[data-node-type="step"]');
    const node = step || event.target.closest('[data-node-type="quest"]');
    if (node) render(node.dataset.nodeType, Number(node.dataset.id));
  });
  render('quest', 1);

  window.eval(source('ui-manager.js'));
  await window.EditorCore.UIManager.init(document);
  await tick();

  assert.equal(document.querySelector('[data-editor-record-nav]'), null,
    'общая планка не должна появляться у дерева с собственной навигацией');

  const questNode = document.querySelector('.quest-node[data-id="1"]');
  assert.equal(questNode.getAttribute('tabindex'), '0', 'roving tabindex на активном узле');

  questNode.focus();
  questNode.dispatchEvent(new window.KeyboardEvent('keydown', { key: 'ArrowDown', bubbles: true }));
  await tick();
  const active = document.querySelector('.tree-selected');
  assert.equal(active?.dataset.nodeType, 'step', 'ArrowDown перешёл с квеста на его первый шаг');
  assert.equal(active?.dataset.id, '11');
});
