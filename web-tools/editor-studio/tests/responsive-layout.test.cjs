const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

// Адаптивная вёрстка без браузера: проверяем именно те инварианты CSS,
// из-за которых мобильный Scenarios «разъезжался» — десктопная панель
// оболочки поверх мобильной, grid-ячейки в шапке редактора и липкая
// шапка сценариев на пол-экрана.

const root = path.join(__dirname, '..');
const read = (name) => fs.readFileSync(path.join(root, name), 'utf8');

test('studio shell shows exactly one header bar per viewport width', () => {
  const css = read('editor-core/studio-shell.css');
  // Базовое состояние — мобильная панель; десктопная выключена, иначе её
  // display:flex (файл подключён после tailwind.css) перебивает hidden.
  assert.match(css, /\.studio-workbar \{[^}]*display: none;/);
  assert.match(css, /@media \(min-width: 768px\) \{ \.studio-workbar \{ display: flex; \} \}/);
  assert.match(css, /@media \(min-width: 768px\) \{ \.studio-mobile-bar \{ display: none; \} \}/);
  // На телефоне вкладки уже 220px и не съедают почти весь экран.
  assert.match(css, /@media \(max-width: 767px\) \{[^}]*\.studio-tab \{ max-width: 156px;/);
});

test('page actions wrap as flex on phones instead of the 140px grid', () => {
  const css = read('editor-core/editor-template.css');
  const gridRule = css.indexOf('grid-template-columns:repeat(auto-fit, minmax(140px, 1fr))');
  const flexRule = css.lastIndexOf('.editor-page-actions{\n    display:flex;');
  assert.ok(gridRule > 0, 'не найдено старое grid-правило действий страницы');
  assert.ok(flexRule > gridRule, 'flex-переопределение должно идти после grid-правила');
  assert.match(css, /\.editor-page-actions \.editor-btn\{\s+flex:1 1 calc\(50% - 4px\);/);
  assert.match(css, /\.editor-page-actions :is\(input, select, textarea\)\{\s+flex:1 1 100%;/);
});

test('scenario header stacks compactly and does not stick on phones', () => {
  const html = read('scenario-editor.html');
  assert.doesNotMatch(html, /controls editor-page-actions flex flex-nowrap/);
  assert.match(html, /controls editor-page-actions flex flex-wrap/);
  assert.match(html, /@media \(max-width: 767px\) \{[\s\S]*?#scenario-top-chrome\{ position: static; \}/);
  // Действия делят строку поровну (min-width мал, иначе три кнопки не
  // помещаются в 390px и уезжают за край).
  assert.match(html, /#scenario-header \.controls > \.editor-btn\{\s+flex: 1 1 60px; min-width: 60px;/);
  assert.match(html, /#scenario-header \.editor-page-title\{ flex: 1 1 auto; min-width: 0; \}/);
  assert.match(html, /\.main-container > \*\{ min-width: 0; \}/);
});

test('phone header keeps only the essentials, the rest lives behind "⋯"', () => {
  const html = read('scenario-editor.html');
  // Кнопка «⋯» существует и скрыта на десктопе.
  assert.match(html, /id="scenario-more-btn"/);
  assert.match(html, /#scenario-more-btn\{ display: none; \}/);
  // Второстепенные действия прячутся, пока шапка не раскрыта.
  assert.match(html, /#scenario-header \.controls > \.scenario-extra\{ display: none !important; \}/);
  assert.match(html, /#scenario-header\.is-expanded \.controls > \.scenario-extra/);
  // Разметка помечает второстепенные действия и «главные» оставляет видимыми.
  assert.match(html, /markHeaderExtras\(\)/);
  assert.match(html, /el\.classList\.contains\('bp-viewmode'\)/);
});

test('scenario overview collapses to a single strip on phones', () => {
  const html = read('scenario-editor.html');
  assert.match(html, /@media \(max-width: 767px\) \{[\s\S]*?\.scenario-overview\{[\s\S]*?flex-wrap: nowrap;/);
  assert.match(html, /\.scenario-overview-card\.is-wide \.label\{ display: none; \}/);
});

test('structure tree is collapsible on phones', () => {
  const html = read('scenario-editor.html');
  assert.match(html, /id="navigator-toggle"/);
  assert.match(html, /#navigator\.is-collapsed \.navigator-header \.editor-search/);
  assert.match(html, /this\.setNavigatorCollapsed\(true\)/);
});

test('step card and component rows use the phone width without clipping', () => {
  const html = read('scenario-editor.html');
  // Действия компонента переезжают на свою строку, текст не рвётся по слогам.
  assert.match(html, /\.component-item\{\s+flex-wrap: wrap;/);
  assert.match(html, /\.component-item \.component-info\{ flex: 1 1 100%;/);
  // Кнопка «Добавить шаг» больше не прилипает к низу экрана.
  assert.match(html, /#add-step-button\{ position: static;/);
});

// Табличные списки (дроп мобов, ингредиенты, награды) на десктопе читаются
// через шапку колонок, но на телефоне шапка скрыта — раньше поля оставались
// без подписей и два числовых поля было не различить.
test('table-mode lists keep per-field captions that appear on phones', () => {
  const css = read('editor-core/editor-template.css');
  // Подписи вложенных полей существуют в разметке всегда, но скрыты, пока
  // колонки называет шапка таблицы.
  assert.match(css, /\.editor-list-table \.editor-nested-label\{display:none;\}/);
  const anchor = css.indexOf('.editor-list-table');
  const mobileStart = css.indexOf('@media (max-width:700px){', anchor);
  assert.ok(mobileStart > 0);
  // Вырезаем весь медиаблок с учётом вложенных фигурных скобок.
  const open = css.indexOf('{', mobileStart);
  let depth = 0;
  let end = open;
  for (; end < css.length; end += 1) {
    if (css[end] === '{') depth += 1;
    if (css[end] === '}') depth -= 1;
    if (depth === 0) break;
  }
  const mobile = css.slice(mobileStart, end);
  assert.match(mobile, /\.editor-list-table-header\{display:none;\}/);
  assert.match(mobile, /\.editor-list-table \.editor-nested-label\{display:block;\}/);
  assert.match(mobile, /\.editor-list-table-cell\{grid-column:1 \/ -1;\}/);
  // Номер слота не пропадает: он переезжает в шапку карточки.
  assert.match(mobile, /\.editor-list-table-idx\{[\s\S]*?display:block;/);
});

test('table-mode list rows render nested labels for every cell', () => {
  const vm = require('vm');
  const context = { window: { EditorCore: {
    utils: {
      parsePath: (p) => String(p ?? '').split('.'),
      getAtPath: (data, p) => String(p ?? '').split('.').reduce((c, k) => (c == null ? undefined : c[k]), data),
      setAtPath: () => {},
      cloneData: (v) => JSON.parse(JSON.stringify(v)),
      escapeAttr: (v) => String(v).replace(/"/g, '&quot;'),
      escapeHtml: (v) => String(v),
    },
    DB: { resolveSource: () => '' },
  } } };
  vm.runInNewContext(read('editor-core/field-renderer.js'), context);
  const html = context.window.EditorCore.FieldRenderer.renderField({
    path: 'drops',
    field: {
      type: 'list',
      label: 'Drop table',
      ui: { listMode: 'table', tableHeader: ['Item', 'Quantity', 'Chance (%)'] },
      itemFields: {
        item_id: { type: 'db_select', label: 'Item', ui: { dbKey: 'item' } },
        count: { type: 'number', label: 'Quantity' },
        chance: { type: 'number', label: 'Chance (%)' },
      },
    },
    data: { drops: [{ item_id: 50, count: 1, chance: 5 }] },
    options: { classes: {
      input: 'editor-input form-input', label: 'editor-label',
      nestedLabel: 'editor-nested-label', fieldWrapper: 'editor-field',
      listWrapper: 'editor-list', listAdd: 'editor-btn', listRemove: 'editor-icon-btn',
    } },
  });
  for (const label of ['Item', 'Quantity', 'Chance (%)']) {
    assert.ok(html.includes(`editor-nested-label">${label}<`), `nested caption "${label}" must exist in markup`);
  }
  assert.match(html, /editor-list-table-idx[^>]*>#1</);
});

test('voucher reward rows have real labels revealed below the desktop header breakpoint', () => {
  const html = read('vouchers-editor.html');
  assert.ok(html.includes("label: 'Item'"));
  assert.ok(html.includes("label: 'Quantity'"));
  assert.doesNotMatch(html, /hideLabel: true/);
  assert.match(html, /#itemsRoot \.editor-nested-label\{display:none;\}/);
  assert.match(html, /@media \(max-width:767px\)\{\s*#itemsRoot \.editor-nested-label\{display:block;\}\s*\}/);
});

test('warehouse trade rows carry per-cell captions for the stacked phone layout', () => {
  const html = read('warehouse-editor.html');
  for (const label of ['Item / collection', 'Qty', 'Price 1', 'Price 2', 'Collection group', 'Collection subgroup', 'Raw database line']) {
    assert.ok(html.includes(`data-label="${label}"`), `caption "${label}"`);
  }
  assert.match(html, /\.trade-cell::before\{[\s\S]*?content:attr\(data-label\);/);
  assert.match(html, /\.trade-cell\.is-static\{display:none;\}/);
  for (const label of ['Main group (optional)', 'Group', 'Subgroup']) {
    assert.ok(html.includes(`<div class="trade-group-label">${label}</div>`));
  }
});
