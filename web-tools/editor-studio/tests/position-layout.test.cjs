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

test('vec2 UI forwards every map-picker option, including linked tile editing', () => {
  let captured;
  const context = { window: { EditorCore: {
    utils: { getAtPath: () => undefined, escapeAttr: String, escapeHtml: String },
    PositionPicker: { render: (options) => { captured = options; return '<div>Position</div>'; } },
  } } };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../editor-core/field-renderer.js'), 'utf8'), context);
  context.window.EditorCore.FieldRenderer.renderField({
    path: 'position', field: { type: 'vec2', label: 'Position', ui: {
      safeTiles: [2], showTiles: true, highlightTiles: [209], linkedTiles: true,
      highlightLayer: 'game', grid: true, snap: false,
      radii: [{ value: 96, label: 'Spawn radius' }],
    } }, data: {},
  });
  assert.equal(captured.safeTiles[0], 2);
  assert.equal(captured.showTiles, true);
  assert.deepEqual(Array.from(captured.highlightTiles), [209]);
  assert.equal(captured.linkedTiles, true);
  assert.equal(captured.highlightLayer, 'game');
  assert.equal(captured.grid, true);
  assert.equal(captured.snap, false);
  assert.equal(captured.radii[0].value, 96);
});
test('scenario modal outer cells give position cards the full row', () => {
  const source = fs.readFileSync(path.join(__dirname, '../scenario-editor.html'), 'utf8');
  // Exercise the actual sizing methods without starting the scenario editor UI.
  const methods = source.slice(source.indexOf('            isWideModalField(field) {'), source.indexOf('            buildModalForm(data, step) {'));
  const sizing = vm.runInNewContext(`new (class {${methods}})()`);
  assert.equal(sizing.modalFieldSize({ type: 'vec2' }), 'wide');
  assert.equal(sizing.modalFieldSize({ type: 'vec2', ui: { highlightTiles: [209], radii: [{ path: 'radius' }] } }), 'wide');
  assert.equal(sizing.modalFieldSize({ type: 'vec2', ui: { spanAll: false } }), 'mid');
  assert.equal(sizing.modalFieldSize({ type: 'number' }), 'narrow');
  assert.equal(sizing.modalFieldSize({ type: 'number', ui: { spanAll: true } }), 'wide');
  assert.match(source, /pushField\(html, this\.modalFieldSize\(field\)\)/);
});
