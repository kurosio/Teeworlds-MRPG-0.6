const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.join(__dirname, '..');
const read = p => fs.readFileSync(path.join(root, p), 'utf8');

test('conditional decision defaults to the compatible waiting mode and exposes both links', () => {
  const context = {window: {}};
  vm.runInNewContext(read('editor-core/registry.js'), context);
  const schemas = context.window.EditorCore.schemas;
  const schema = schemas.componentSchemas.dynamic_condition;
  const defaults = schemas.buildDefaults(schema.fields);
  assert.equal(defaults.check_once, false);
  assert.equal(defaults.timeout, 0);
  assert.deepEqual(Array.from(schema.linkTargets), ['pass_step_id', 'fail_step_id']);
});

test('picker groups are searchable and preserve input focus when clearing search', () => {
  const html = read('scenario-editor.html');
  assert.match(html, /Logic & conditions/);
  assert.match(html, /Messages & presentation/);
  assert.match(html, /_renderAddComponentList\(e.target.value, true\)/);
  assert.match(html, /escape\(info.name\)/);
  assert.match(html, /escapeHtml\(s.id\)/);
  assert.match(html, /return knownFields;/);
});

test('one-shot branch survives serialization and server resets transient jumps on re-entry', () => {
  const html = read('scenario-editor.html');
  assert.doesNotMatch(html, /delete c\.fail_step_id/);
  const base = read('../../src/game/server/core/scenarios/base/component.h');
  assert.match(base, /m_NextStepId = m_ConfiguredNextStepId/);
  const server = read('../../src/game/server/core/scenarios/impl/components/default.h');
  assert.match(server, /m_CheckOnce = j.value\("check_once", false\)/);
  assert.match(server, /if\(m_CheckOnce\)[\s\S]*?FinishPassed\(\);[\s\S]*?FinishTimedOut\(\);/);
  assert.match(server, /int64_t\(m_TimeoutSeconds\) \* this->Server\(\)->TickSpeed\(\)/);
});
