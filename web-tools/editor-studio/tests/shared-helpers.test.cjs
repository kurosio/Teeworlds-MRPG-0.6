const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { JSDOM } = require('jsdom');

const root = path.join(__dirname, '..');
const source = name => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');
/** Значения приходят из другого realm — сравниваем простые данные. */
const plain = value => JSON.parse(JSON.stringify(value));

function dom(html = '') {
  return new JSDOM(`<body>${html}</body>`, {
    url: 'https://studio.test/items-editor.html', runScripts: 'outside-only', pretendToBeVisual: true,
  });
}

/** utils.js требует только window; Http подменяется, чтобы видеть ответы. */
function utilsHarness(t) {
  const d = dom();
  t.after(() => d.window.close());
  const w = d.window;
  const sent = [];
  w.EditorCore = {
    Http: {
      isSameOriginMessage: (event, from) => event.source === from,
      postMessage: (target, payload) => sent.push({ target, payload }),
    },
  };
  w.eval(source('utils.js'));
  return { w, sent, ask: data => w.dispatchEvent(askEvent(w, data)) };
}

function askEvent(w, data) {
  const event = new w.MessageEvent('message', { data });
  Object.defineProperty(event, 'source', { value: w.parent });
  return event;
}

test('shell dirty status: отвечает только на запрос оболочки и только пока подписан', t => {
  const { w, sent, ask } = utilsHarness(t);
  let dirty = false;
  const off = w.EditorCore.Dirty.bindShellStatus(() => dirty);

  ask({ type: 'editor-shell:request-dirty-status', requestId: 1 });
  assert.equal(sent.length, 1);
  assert.equal(sent[0].target, w.parent);
  assert.deepEqual(plain(sent[0].payload), { type: 'editor-shell:dirty-status', requestId: 1, dirty: false });

  dirty = true;
  ask({ type: 'editor-shell:request-dirty-status', requestId: 2 });
  assert.equal(plain(sent[1].payload).dirty, true);

  // Другие сообщения оболочки — не наш запрос.
  ask({ type: 'editor-shell:focus-editor', requestId: 3 });
  ask({ type: 'что-то ещё' });
  assert.equal(sent.length, 2);

  off();
  ask({ type: 'editor-shell:request-dirty-status', requestId: 4 });
  assert.equal(sent.length, 2, 'после отписки ответов больше нет');
});

test('shell dirty status: сбой чтения состояния не ломает вкладку', t => {
  const { w, sent, ask } = utilsHarness(t);
  w.EditorCore.Dirty.bindShellStatus(() => { throw new Error('boom'); });
  ask({ type: 'editor-shell:request-dirty-status', requestId: 7 });
  assert.equal(sent.length, 1);
  assert.deepEqual(plain(sent[0].payload), { type: 'editor-shell:dirty-status', requestId: 7, dirty: false });
});

test('craft group rename: общий план правок для гостя и запроса', t => {
  const { w } = utilsHarness(t);
  const { planCraftGroupRename, parseCraftGroup, joinCraftGroup } = w.EditorCore.utils;
  const rows = [
    { ID: 1, GroupName: 'a' },
    { ID: 2, GroupName: 'a:b' },
    { ID: 3, GroupName: '|M|a:sub' },
    { ID: 4, GroupName: '|M|other' },
  ];
  const plan = (target, name, merge) => plain(planCraftGroupRename(rows, target, name, merge).updates)
    .map(update => [update.id, update.groupName]);

  assert.deepEqual(plain(parseCraftGroup('|M|a:b')), { main: 'M', group: 'a', sub: 'b' });
  assert.equal(joinCraftGroup('M', 'a', 'b'), '|M|a:b');
  assert.equal(joinCraftGroup('', 'a', ''), 'a');

  assert.deepEqual(plan({ level: 'main', mainGroup: 'M' }, 'N'), [[3, '|N|a:sub'], [4, '|N|other']]);
  assert.deepEqual(plan({ level: 'group', mainGroup: 'M', group: 'a' }, 'A2'), [[3, '|M|A2:sub']]);
  assert.deepEqual(plan({ level: 'sub', mainGroup: 'M', group: 'a', subGroup: 'sub' }, 'sub2'), [[3, '|M|a:sub2']]);
  assert.deepEqual(plan({ level: 'group', mainGroup: 'M', group: 'a' }, 'a'), [], 'то же имя — менять нечего');

  assert.throws(() => planCraftGroupRename(rows, { level: 'nope' }, 'x'), /Invalid craft group target/);
  assert.throws(() => planCraftGroupRename(rows, { level: 'main', mainGroup: 'M' }, 'a|b'),
    /Invalid new group name/);
  // Имя уже занято соседней группой того же уровня.
  const siblings = [{ ID: 3, GroupName: '|M|a:sub' }, { ID: 5, GroupName: '|M|b' }];
  const mergeTarget = { level: 'group', mainGroup: 'M', group: 'a' };
  assert.throws(() => planCraftGroupRename(siblings, mergeTarget, 'b'), /already taken/);
  assert.equal(
    plain(planCraftGroupRename(siblings, mergeTarget, 'b', true).updates).length,
    1,
    'mergeExisting разрешает совпадение имён',
  );
});

test('confirmDiscard спрашивает только при наличии правок', async t => {
  const d = dom('<div id="root"></div>');
  t.after(() => d.window.close());
  const w = d.window;
  w.eval(source('utils.js'));
  w.eval(source('page-kit.js'));

  assert.equal(await w.EditorCore.Page.confirmDiscard(() => false), true);
  assert.equal(await w.EditorCore.Page.confirmDiscard(() => { throw new Error('boom'); }), true);
  assert.equal(w.document.querySelector('.ec-dialog'), null, 'без правок диалога нет');

  const pending = w.EditorCore.Page.confirmDiscard(() => true);
  const dialog = w.document.querySelector('.ec-dialog');
  assert.ok(dialog, 'с правками диалог показывается');
  assert.match(dialog.textContent, /unsaved changes/i);
  dialog.querySelector('.ec-ok').click();
  assert.equal(await pending, true);
});
