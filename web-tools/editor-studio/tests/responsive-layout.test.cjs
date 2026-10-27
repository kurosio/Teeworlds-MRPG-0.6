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
  assert.match(html, /#scenario-header \.controls > \.editor-btn\{\s+flex: 1 1 calc\(50% - 4px\);/);
  assert.match(html, /#scenario-header \.editor-page-title\{ flex: 1 1 auto; min-width: 0; \}/);
  assert.match(html, /\.main-container > \*\{ min-width: 0; \}/);
});
