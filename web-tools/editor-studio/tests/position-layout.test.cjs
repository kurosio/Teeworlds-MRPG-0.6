const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

test('position fields occupy a full row by default, including nested positions', () => {
  const context = { window: { EditorCore: {
    utils: {
      getAtPath: () => undefined,
      escapeAttr: String,
      escapeHtml: String,
    },
    PositionPicker: { render: () => '<div>Position</div>' },
  } } };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../editor-core/field-renderer.js'), 'utf8'), context);
  const render = context.window.EditorCore.FieldRenderer.renderField;
  for (const isNested of [false, true]) {
    for (const ui of [{}, { spanAll: true }, { spanAll: false }]) {
      const html = render({ path: 'position', field: { type: 'vec2', ui }, data: {}, isNested });
      assert.equal(html.includes('editor-field--span-all'), ui.spanAll !== false);
    }
  }
  const html = render({ path: 'radius', field: { type: 'number' }, data: {} });
  assert.equal(html.includes('editor-field--span-all'), false);
});

test('scenario modal outer cells give position cards the full row', () => {
  const source = fs.readFileSync(path.join(__dirname, '../scenario-editor.html'), 'utf8');
  // Exercise the actual sizing methods without starting the scenario editor UI.
  const methods = source.slice(source.indexOf('            isWideModalField(field) {'), source.indexOf('            buildModalForm(data, step) {'));
  const sizing = vm.runInNewContext(`new (class {${methods}})()`);
  assert.equal(sizing.modalFieldSize({ type: 'vec2' }), 'wide');
  assert.equal(sizing.modalFieldSize({ type: 'vec2', ui: { highlightTiles: [209], radiusPath: 'radius' } }), 'wide');
  assert.equal(sizing.modalFieldSize({ type: 'vec2', ui: { spanAll: false } }), 'mid');
  assert.equal(sizing.modalFieldSize({ type: 'number' }), 'narrow');
  assert.equal(sizing.modalFieldSize({ type: 'number', ui: { spanAll: true } }), 'wide');
  assert.match(source, /pushField\(html, this\.modalFieldSize\(field\)\)/);
});
