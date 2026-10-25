const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const source = (name) => fs.readFileSync(path.join(__dirname, '..', 'editor-core', name), 'utf8');

// Model the quest tree's full DOM replacement on selection. The observer and
// animation frames are driven explicitly so navigation races are reproducible.
function harness({ resource = 'quests', edit = false } = {}) {
  const questId = 2000000001;
  const stepId = 2000000002;
  const targetId = resource === 'quests' ? questId : stepId;
  const frames = [];
  let observer = () => {};
  let rows = [];
  let selected = null;
  let collapsed = true;
  let clicks = 0;
  let scrolls = 0;
  let toggles = 0;
  const classes = (names = []) => {
    const values = new Set(names);
    return {
      contains: (name) => values.has(name),
      add: (name) => values.add(name),
      toggle: (name, on) => on ? values.add(name) : values.delete(name),
    };
  };
  const node = () => ({
    classList: classes(), setAttribute() {}, addEventListener() {},
  });
  const render = () => {
    rows = [['quest', questId], ...(!collapsed ? [['step', stepId], ['step', stepId + 1]] : [])]
      .map(([type, id]) => ({
        dataset: { id: String(id), nodeType: type },
        classList: classes([
          ...(selected === id ? ['tree-selected'] : []),
          ...(type === 'quest' && collapsed ? ['is-collapsed'] : []),
        ]),
        scrollIntoView() { scrolls++; },
        click() { clicks++; selected = id; render(); },
        querySelector: () => ({ click() { toggles++; collapsed = false; render(); } }),
      }));
    observer();
  };
  const document = {
    readyState: 'complete',
    body: { classList: classes(), appendChild() {} },
    createElement: node,
    addEventListener() {},
    querySelectorAll(selector) {
      if (selector === '[data-node-type="quest"][data-id]') return rows.filter(row => row.dataset.nodeType === 'quest');
      if (selector === '[data-node-type="step"][data-id]') return rows.filter(row => row.dataset.nodeType === 'step');
      return [];
    },
  };
  const window = {
    location: { search: `?request=CR-1&resource=${resource}&record=${targetId}${edit ? '&edit=1' : ''}` },
    dispatchEvent() {},
    requestAnimationFrame(fn) { frames.push(fn); return frames.length; },
    EditorCore: {
      Guest: { ready: async () => 'admin' },
      ChangeMarkers: {
        operationId: (op) => Number(op.previewId ?? (op.type === 'create' ? op.tempId : op.id)),
        setRequest() {}, refresh() {},
      },
      Http: { json: async () => ({ ok: true, request: {
        id: 'CR-1', version: 1, status: 'pending', operations: [
          { type: 'create', resource: 'quests', tempId: questId, data: { Name: 'New quest' } },
          { type: 'create', resource: 'quest_bots', tempId: stepId, data: { QuestID: questId, Step: 1 } },
          { type: 'create', resource: 'quest_bots', tempId: stepId + 1, data: { QuestID: questId, Step: 2 } },
        ],
      } }) },
    },
  };
  const context = vm.createContext({
    window, document, URLSearchParams, console,
    CustomEvent: class {},
    MutationObserver: class { constructor(callback) { observer = callback; } observe() {} },
  });
  vm.runInContext(source('utils.js'), context);
  vm.runInContext(source('request-context.js'), context);
  return {
    api: window.EditorCore.RequestContext,
    questId, stepId, render,
    select(id) { selected = id; collapsed = false; render(); },
    get selected() { return selected; },
    get clicks() { return clicks; },
    get scrolls() { return scrolls; },
    get toggles() { return toggles; },
    async flush() {
      for (let i = 0; i < 10; i++) {
        frames.splice(0).forEach(fn => fn());
        await Promise.resolve();
      }
      assert.equal(frames.length, 0, 'navigation should settle after rendering');
    },
  };
}

for (const edit of [false, true]) {
  for (const resource of ['quests', 'quest_bots']) {
    test(`${edit ? 'editable' : 'read-only'} request lets users select another quest step (target: ${resource})`, async () => {
      const h = harness({ resource, edit });
      await h.api.ready();
      // Request arrives before the asynchronously loaded tree.
      await h.flush();
      assert.equal(h.clicks, 0);
      h.render();
      await h.flush();
      assert.equal(h.selected, resource === 'quests' ? h.questId : h.stepId);
      assert.equal(h.clicks, 1);
      assert.equal(h.toggles, resource === 'quests' ? 0 : 1);
      const scrolls = h.scrolls;

      h.select(h.stepId + 1);
      await h.flush();
      assert.equal(h.selected, h.stepId + 1, 'DOM replacement must not restore the URL target');
      assert.equal(h.clicks, 1);
      assert.equal(h.scrolls, scrolls, 'do not scroll back to the URL target');

      h.render();
      await h.flush();
      assert.equal(h.selected, h.stepId + 1);

      // Explicit navigation is still allowed after the initial jump.
      h.api.navigate();
      await h.flush();
      assert.equal(h.selected, resource === 'quests' ? h.questId : h.stepId);
      assert.equal(h.clicks, 2);
    });
  }
}

test('an already selected request target also completes automatic navigation', async () => {
  const h = harness({ resource: 'quest_bots' });
  await h.api.ready();
  h.select(h.stepId);
  await h.flush();
  assert.equal(h.clicks, 0);
  h.select(h.stepId + 1);
  await h.flush();
  assert.equal(h.selected, h.stepId + 1);
  assert.equal(h.clicks, 0);
});
