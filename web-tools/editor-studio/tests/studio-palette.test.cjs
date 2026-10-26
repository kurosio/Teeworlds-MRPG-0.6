const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');

function palette() {
  const context = vm.createContext({ window: {}, console });
  vm.runInContext(source('studio-palette.js'), context);
  return context.window.EditorCore.StudioPalette;
}

const ITEMS = [
  { kind: 'editor', key: 'items', title: 'Items', hint: 'Item catalogue', group: 'Database editors', keywords: 'items catalogue weapons armor loot things' },
  { kind: 'editor', key: 'vouchers', title: 'Vouchers', hint: 'Gift codes and rewards', group: 'Database editors', keywords: 'voucher promo code gift reward coupon' },
  { kind: 'editor', key: 'worlds', title: 'Worlds', hint: 'Worlds and their flags', group: 'Database editors', keywords: 'worlds maps zones locations flags' },
  { kind: 'editor', key: 'nodes_collecting', title: 'Collecting nodes', hint: 'Ore, plants and fish by world and switch', group: 'Database editors', keywords: 'nodes collecting ore plants fish gathering' },
  { kind: 'action', action: 'theme', title: 'Toggle color theme', hint: 'Switch between dark and light', keywords: 'dark light mode' },
];

test('palette: title match wins, keyword match is found', () => {
  const P = palette();
  assert.equal(P.filter(ITEMS, 'items')[0].key, 'items');
  assert.deepEqual(P.filter(ITEMS, 'promo').map((i) => i.key), ['vouchers']);
});

test('palette: items with keywords do not match unrelated queries', () => {
  const P = palette();
  // Regression: keyword scoring must keep the "no match" -1 instead of
  // turning it into a small positive score.
  assert.deepEqual(P.filter(ITEMS, 'zzzqq'), []);
  assert.equal(P.scoreItem(ITEMS[0], 'zzzqq'), -1);
});

test('palette: subsequence and multi-term queries', () => {
  const P = palette();
  assert.ok(P.filter(ITEMS, 'itm').some((i) => i.key === 'items'));
  assert.deepEqual(P.filter(ITEMS, 'dark light').map((i) => i.action), ['theme']);
  assert.deepEqual(P.filter(ITEMS, 'dark items'), []);
});

test('palette: title beats hint, order and limit are stable', () => {
  const P = palette();
  assert.deepEqual(P.filter(ITEMS, 'world').map((i) => i.key), ['worlds', 'nodes_collecting']);
  assert.deepEqual(P.filter(ITEMS, '').map((i) => i.key || i.action),
    ['items', 'vouchers', 'worlds', 'nodes_collecting', 'theme']);

  const many = Array.from({ length: 40 }, (_, n) => ({ title: `Editor ${n}` }));
  assert.equal(P.filter(many, 'editor').length, 30);
  assert.equal(P.filter(many, 'editor', 5).length, 5);
  assert.equal(P.filter(many, 'editor 3')[0].title, 'Editor 3');
});

test('palette: scoreText scale', () => {
  const P = palette();
  assert.equal(P.scoreText('Items', 'items'), 100);
  assert.ok(P.scoreText('Items', 'ite') > P.scoreText('Item catalogue', 'ite'));
  assert.equal(P.scoreText('Items', 'xyz'), -1);
  assert.equal(P.scoreItem(ITEMS[0], ''), 0);
});

test('palette prioritize: open tabs go first in tab order', () => {
  const p = palette().prioritize;
  assert.equal(typeof p, 'function');
  const items = [{ key: 'a' }, { key: 'b' }, { key: 'c' }, { key: 'd' }];
  const keysOf = (list) => list.map((i) => i.key).join(',');
  assert.equal(keysOf(p(items, ['c', 'a'])), 'c,a,b,d');
  assert.equal(keysOf(p(items, [])), 'a,b,c,d');
  assert.equal(keysOf(p(items, ['missing'])), 'a,b,c,d');
  assert.equal(p([], ['a']).length, 0);
});
