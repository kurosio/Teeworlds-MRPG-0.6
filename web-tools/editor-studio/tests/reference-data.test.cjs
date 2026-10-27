const { test } = require('node:test');
const assert = require('node:assert/strict');
const refs = require('../editor-core/reference-data.js');
const schema = require('../editor-core/reference-schema.json');
const { fixtures, idMap, I } = require('./reference-fixtures.cjs');

for (const [index, fixture] of fixtures.entries()) {
  test(`typed reference remap ${index + 1}: ${fixture.resource}`, () => {
    const before = JSON.stringify(fixture.data);
    const actual = refs.remap(fixture.resource, fixture.data, schema, idMap);
    assert.deepEqual(actual, fixture.expected);
    assert.equal(JSON.stringify(fixture.data), before, 'caller data is not mutated');
    assert.deepEqual(refs.remap(fixture.resource, actual, schema, idMap), actual, 'remap is idempotent');
  });
}

test('reference discovery visits only typed leaves and reports the expected resource', () => {
  const actual = refs.references('items', { Name: String(I), InitialPrice: I, Data: {
    random_box: [{ item_id: I, value: I }], use_scenario: { world_id: 2000000002, duration: I },
  } }, schema);
  assert.deepEqual(actual, [
    { id: I, target: 'items', column: 'Data' },
    { id: 2000000002, target: 'worlds', column: 'Data' },
  ]);
});

test('typed traversal handles empty, malformed and unknown values without guessing', () => {
  for (const Data of [null, '', '{invalid', 'null', '[]', '{}']) {
    assert.deepEqual(refs.remap('items', { Data }, schema, idMap), { Data });
  }
  assert.deepEqual(refs.remap('unknown', { ItemID: I }, schema, idMap), { ItemID: I });
  assert.deepEqual(refs.references('crafts', { ItemID: [I] }, schema), []);
});

test('all schema codecs and target resources are explicitly declared', () => {
  const codecs = new Set(['id','json','csv','pairs','brackets']);
  for (const rules of Object.values(schema)) for (const rule of rules) {
    assert.ok(codecs.has(rule.codec));
    for (const target of [rule.target, ...Object.values(rule.paths || {}), ...Object.values(rule.recursive || {})].filter(Boolean)) {
      assert.ok(schema[target] || target === 'attributes', `unknown target ${target}`);
    }
  }
});
