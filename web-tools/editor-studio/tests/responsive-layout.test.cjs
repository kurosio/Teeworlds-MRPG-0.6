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

// ── Окно карты (map-picker) ────────────────────────────────────────────────
// Раньше на телефоне панель «Инструменты карты» была прибита к `bottom: 64px`
// при подвале выше 64px: она накрывала «Применить», а `max-height: 46vh` не
// давал прокрутить секцию правки. Кроме того, подсказка режима стояла под
// шапкой поверх панели, а поля ввода мельче 16px вызывали зум iOS.

test('map picker dock is a bottom sheet bounded by real header/footer sizes', () => {
  const css = read('vendor/ddnet-map/picker.css');
  const mobileStart = css.indexOf('@media (max-width: 640px)');
  assert.ok(mobileStart > 0, 'нет мобильных блоков окна карты');
  // Все мобильные правила идут подряд в конце файла; берём всё от первого.
  const mobile = css.slice(mobileStart);
  // Панель отсчитывается от шапки и ограничена вычисленной высотой.
  assert.match(mobile, /\.pk-dock \{[^}]*top: var\(--pk-dock-top/);
  assert.match(mobile, /\.pk-dock \{[^}]*max-height: var\(--pk-dock-max/);
  // Явных «магических» отступов, которые накрывали подвал, больше нет.
  assert.doesNotMatch(mobile, /\.pk-dock \{[^}]*bottom: 64px/);
  assert.doesNotMatch(mobile, /max-height: min\(46vh/);
  // Прокрутка живёт в теле панели: flex-колонка «шапка + скролл».
  const css2 = read('vendor/ddnet-map/picker.css');
  assert.match(css2, /\.pk-dock-body \{[^}]*overflow: hidden/);
  assert.match(css2, /\.pk-dock-scroll \{[^}]*flex: 1 1 auto[^}]*overflow-y: auto/);
  assert.match(css2, /\.pk-dock-scroll \{[^}]*min-height: 0/);
});

test('map picker header and footer wrap instead of overflowing phones', () => {
  const css = read('vendor/ddnet-map/picker.css');
  const mobile = css.slice(css.indexOf('@media (max-width: 640px)'));
  // Координаты уходят на свою строку, зум остаётся в шапке.
  assert.match(mobile, /\.pk-coords \{[^}]*flex: 1 1 100%/);
  assert.match(mobile, /\.pk-tools \{[^}]*margin-left: auto/);
  // X и Y делят строку, действия — отдельную.
  assert.match(mobile, /\.pk-foot > \.pk-field \{ order: 1; flex: 1 1 calc\(50% - 4px\); \}/);
  assert.match(mobile, /\.pk-foot-actions \{[^}]*width: 100%/);
  assert.match(mobile, /\.pk-foot-actions \.pk-btn \{[^}]*height: 40px/);
  // Вырез экрана и жест-бар iOS не перекрывают панели.
  assert.match(mobile, /env\(safe-area-inset-bottom\)/);
  assert.match(mobile, /env\(safe-area-inset-top\)/);
  // Предупреждение о запрещённом тайле стоит ДО кнопок, а не после них.
  assert.ok(mobile.indexOf('.pk-validity { order: 2') > 0, 'валидность — вторая строка');
  assert.ok(mobile.indexOf('.pk-foot-actions { order: 3') > 0, 'действия — третья строка');
});

test('map picker keeps iOS from zooming into inputs and shrinks the canvas to dvh', () => {
  const css = read('vendor/ddnet-map/picker.css');
  // Ввод >= 16px на сенсоре: иначе Safari зумит страницу и окно «уезжает».
  assert.match(css, /@media \(pointer: coarse\) \{[\s\S]*?\.pk-field input,[\s\S]*?font-size: 16px/);
  // 100dvh вместо 100vh: адресная строка телефона не оставляет полосу.
  assert.match(css, /html, body \{[\s\S]*?height: 100dvh/);
  // Подсказка не висит над панелью на широком, но низком окне.
  assert.doesNotMatch(css, /\.pk-hint \{[^}]*top: 160px/);
});

test('map dialog gives the whole phone screen to the map and wraps the world row', () => {
  const css = read('editor-core/editor-theme.css');
  const start = css.indexOf('@media (max-width:640px){\n  .editor-map-dialog{padding:0;}');
  assert.ok(start > 0, 'нет мобильного блока окна карты');
  const block = css.slice(start, css.indexOf('\n}\n', start) + 3);
  assert.match(block, /\.editor-map-dialog-frame\{[\s\S]*?height:100%/);
  assert.match(block, /\.editor-map-dialog-frame\{[\s\S]*?max-width:none/);
  assert.match(block, /\.editor-map-dialog-world\{[\s\S]*?flex-wrap:wrap/);
  assert.match(block, /\.editor-map-dialog-world-select\{[\s\S]*?font-size:16px/);
});

test('map picker layout is recalculated from header and footer boxes', () => {
  const js = read('vendor/ddnet-map/picker.js');
  // Отступы считаются по фактическим размерам, а не по константам 64/96px.
  assert.match(js, /const footTop = foot \? foot\.getBoundingClientRect\(\)\.top : viewport - 12/);
  assert.match(js, /const bottom = footTop > 24 \? footTop : viewport - 12/);
  // Панель никогда не заходит на подвал, даже в низком окне.
  assert.match(js, /const top = Math\.max\(8, Math\.min\(barBottom, bottom - 116\)\)/);
  // Ширина панели на телефоне — во всю строку, подсказку она не сдвигает.
  assert.match(js, /const dockWidth = narrow \? 0 :/);
  // Смена ориентации и клавиатура iOS тоже пересчитывают раскладку.
  assert.match(js, /addEventListener\('orientationchange', \(\) => layoutTools\(\)\)/);
  assert.match(js, /visualViewport\?\.addEventListener\?\.\('resize'/);
});

// ── геометрия панели карты на реальных размерах устройств ──────────────────
// layoutTools() — единственное место, где считается раскладка окна карты.
// Гоняем её на размерах настоящих телефонов и на низком ландшафтном окне:
// панель обязана оставаться между шапкой и подвалом, а не накрывать кнопки.
test('map picker dock geometry stays between the header and the footer', () => {
  const fs = require('node:fs');
  const path = require('node:path');
  const vm = require('node:vm');
  const source = fs.readFileSync(path.join(__dirname, '../vendor/ddnet-map/picker.js'), 'utf8');
  // Вырезаем только layoutTools: остальной файл требует canvas и карту.
  // Именно присваивание, а не `let layoutTools = () => {};` выше по файлу.
  const start = source.indexOf('\n  layoutTools = () => {');
  const end = source.indexOf('\n  };\n', start);
  assert.ok(start > 0 && end > start, 'layoutTools не найдена');
  const layout = source.slice(start, end);
  // Хвост `\n  };` в срез не входит — тело функции идёт до конца среза.
  const body = layout.slice(layout.indexOf('{') + 1);
  const run = ({ width, height, barBottom, footTop }) => {
    const props = {};
    const context = vm.createContext({
      window: { innerWidth: width, innerHeight: height },
      document: {
        querySelector: (selector) => (selector === '.pk-bar'
          ? { getBoundingClientRect: () => ({ bottom: barBottom }) }
          : selector === '.pk-foot' ? { getBoundingClientRect: () => ({ top: footTop }) } : null),
        documentElement: { style: { setProperty: (name, value) => { props[name] = value; } } },
      },
      el: { dock: { getBoundingClientRect: () => ({ width: 378 }) } },
    });
    vm.runInContext(`(() => {${body}})()`, context);
    return props;
  };
  const px = (value) => Number(String(value).replace('px', ''));

  const phone = run({ width: 390, height: 844, barBottom: 132, footTop: 690 });
  assert.equal(px(phone['--pk-dock-top']), 140, 'панель под шапкой (132 + 8)');
  assert.ok(px(phone['--pk-dock-max']) <= 690 - 140 - 12, 'панель не заходит на подвал');
  assert.ok(px(phone['--pk-dock-max']) >= 96, 'панель не может быть совсем плоской');
  assert.equal(px(phone['--pk-hint-top']), 140);
  assert.equal(phone['--pk-hint-left'], '24px', 'на телефоне подсказки нет — отступ не зависит от ширины панели');

  const small = run({ width: 320, height: 568, barBottom: 146, footTop: 470 });
  assert.ok(px(small['--pk-dock-top']) >= 8);
  assert.ok(px(small['--pk-dock-top']) + px(small['--pk-dock-max']) <= 470 + 12, 'низкий экран: подвал свободен');

  // Ландшафт: между шапкой и подвалом меньше 116px — панель поднимается,
  // но её низ всё равно выше подвала, и высота остаётся разумной.
  const landscape = run({ width: 844, height: 390, barBottom: 300, footTop: 264 });
  assert.ok(px(landscape['--pk-dock-max']) >= 96, 'высота не уходит в ноль');
  assert.ok(px(landscape['--pk-dock-top']) < 264, 'панель начинается выше подвала');

  const desktop = run({ width: 1440, height: 900, barBottom: 76, footTop: 836 });
  assert.equal(px(desktop['--pk-dock-top']), 84);
  assert.equal(desktop['--pk-hint-left'], '402px', 'подсказка справа от панели на широком экране');
});
