const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

// Мобильный редактор сценариев без браузера: проверяем инварианты, из-за
// которых им нельзя было пользоваться на телефоне —
//   • граф не принимал касания (всё висело на mouse-событиях);
//   • граф на телефоне имел нулевую высоту и прятался под шапкой;
//   • стрелки в списке рисовались «криво» (петли через весь экран).

const root = path.join(__dirname, '..');
const read = (name) => fs.readFileSync(path.join(root, name), 'utf8');

test('graph interactions go through pointer events, not mouse-only handlers', () => {
  const js = read('editor-core/blueprint-view.js');
  // Ни одного «ручного» mousemove/mouseup вне общего помощника.
  assert.doesNotMatch(js, /document\.addEventListener\('mousemove'/);
  assert.doesNotMatch(js, /document\.addEventListener\('mouseup'/);
  assert.match(js, /startPointerDrag\(startEvent, handlers = \{\}\)/);
  // Все перетаскивания графа используют этот помощник.
  ['startNodeDrag', 'startGroupDrag', 'startGroupResize', 'startCommentDrag',
    'startCommentResize', 'startMultiDrag', 'startConnect', 'startCompDrag', 'startMarquee'
  ].forEach((fn) => {
    const at = js.indexOf(`\n    ${fn}(`);
    assert.ok(at > 0, `${fn} не найдена`);
    const body = js.slice(at);
    assert.ok(/this\.startPointerDrag\(/.test(body.slice(0, 2500)), `${fn} должен использовать startPointerDrag`);
  });
  // Касание по канвасу = панорама графа.
  assert.match(js, /this\.canvas\.addEventListener\('pointerdown'/);
  assert.match(js, /touch-action/);
});

test('pinch-to-zoom works even when a finger starts on a node', () => {
  const js = read('editor-core/blueprint-view.js');
  assert.match(js, /beginPinch\(/);
  assert.match(js, /capture: true/);
  assert.match(js, /this\.canvas\.addEventListener\('pointermove', onPinchMove, \{ capture: true, passive: false \}\)/);
});

test('graph canvas never collapses to zero height on a phone', () => {
  const css = read('editor-core/blueprint-view.css');
  assert.match(css, /height: max\(320px, calc\(100dvh - var\(--scenario-top-h, 126px\)\)\)/);
  // На телефоне граф занимает весь экран, а действия — внизу под большой палец.
  assert.match(css, /body\.bp-mode #bp-canvas \{[^}]*position: fixed;/);
  assert.match(css, /body\.bp-mode #scenario-top-chrome/);
  assert.match(css, /@media \(max-width: 767px\) \{[\s\S]*?\.bp-toolbar \{[\s\S]*?bottom: calc\(8px \+ env\(safe-area-inset-bottom, 0px\)\);/);
  // Возврат к списку доступен из самого графа.
  const html = read('scenario-editor.html');
  assert.match(html, /id="bp-back-list"/);
  assert.match(html, /backList\?\.addEventListener\('click', \(\) => this\.switchViewMode\('list'\)\)/);
});

test('graph keeps readable nodes on touch screens', () => {
  const css = read('editor-core/blueprint-view.css');
  // Тач-цели больше, действия узла появляются у выбранного узла (иначе
  // идентификатор шага сжимался до одной буквы).
  assert.match(css, /@media \(pointer: coarse\) \{/);
  assert.match(css, /\.bp-node\.is-selected \.bp-node-actions \{[^}]*display: flex;/);
  assert.match(css, /\.bp-drag-zone \{ width: 26px; height: 26px;/);
  const js = read('editor-core/blueprint-view.js');
  // Первый показ графа кадрируется, а на телефоне зум не падает до 20 %.
  assert.match(js, /if \(!restored\) requestAnimationFrame\(\(\) => this\.fitToView\(\)\)/);
  assert.match(js, /if \(compact\) scale = Math\.max\(scale, 0\.44\)/);
});

test('list-mode arrows are routed instead of looping across the screen', () => {
  const html = read('scenario-editor.html');
  assert.match(html, /roundedPolyline\(points, radius = 12\)/);
  assert.match(html, /detectNavigatorStacked\(\)/);
  // Стрелка всегда входит в карточку по горизонтали — остриё смотрит в цель.
  assert.match(html, /const endX = toRect\.left - 4;/);
  assert.match(html, /arrow\.setAttribute\('points', `\$\{endX\},\$\{endY\} \$\{endX - 9\},\$\{endY - 4\.5\} \$\{endX - 9\},\$\{endY \+ 4\.5\}`\)/);
  // Скрытые/свёрнутые элементы (нулевой прямоугольник) не дают линий из угла.
  assert.match(html, /fromRect\.width < 2 && fromRect\.height < 2/);
  // В режиме графа отдельный SVG-слой списка не нужен.
  assert.match(html, /if \(!this\.scenarioData\.steps \|\| this\.viewMode === 'graph'\) return;/);
});

test('arrows redraw when the layout changes, not only on scroll', () => {
  const html = read('scenario-editor.html');
  assert.match(html, /new ResizeObserver\(\(\) => this\.scheduleDrawConnections\(\)\)/);
  assert.match(html, /window\.addEventListener\('orientationchange'/);
});
