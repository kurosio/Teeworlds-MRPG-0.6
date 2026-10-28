const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

// Настоящий рендер графа в jsdom: проверяем, что провода между событиями
// (on_end / on_recieve_objectives) действительно не появляются на канвасе,
// а сами события видны в легенде и на рамках групп.
//
// jsdom необязателен: без него тест пропускается, как и остальные DOM-тесты.

let JSDOM = null;
try { ({ JSDOM } = require('jsdom')); } catch { /* ignore */ }

const SKIP = JSDOM ? false : 'jsdom не установлен (npm install --no-save jsdom)';

const ROOT = path.join(__dirname, '..');
const read = (name) => fs.readFileSync(path.join(ROOT, name), 'utf8');

const CANVAS_HTML = `<!DOCTYPE html><html><body>
  <div id="bp-canvas">
    <div class="bp-toolbar"></div>
    <div id="bp-event-legend" class="bp-event-legend"></div>
    <canvas id="bp-minimap" width="220" height="150"></canvas>
    <div id="bp-world"><svg id="bp-wires"></svg></div>
    <div class="bp-legend"></div>
  </div>
  <div id="bp-zoom-label">100%</div>
</body></html>`;

/** Сценарий: два события + базовый блок. step_b (objectives) идёт в списке
 *  сразу после step_a (on_end) — раньше это рисовало «ложную» пунктирную связь. */
const STEPS = [
  { id: 'step_a', components: [{ component_id: 'c_a', type: 'message' }] },
  { id: 'step_b', components: [{ component_id: 'c_b', type: 'message' }] },
  { id: 'step_c', components: [{ component_id: 'c_c', type: 'message' }] },
  { id: 'step_d', components: [{ component_id: 'c_d', type: 'message' }] },
];

const EVENTS = {
  general: 'Standard',
  on_recieve_objectives: 'On objectives received',
  on_end: 'On step end',
};

function buildEditor(extraLinks = {}) {
  const scope = new Map();
  const groups = [
    { id: 'g_end', name: 'v1', event_key: 'on_end', steps: ['step_a'] },
    { id: 'g_obj', name: 'v2', event_key: 'on_recieve_objectives', steps: ['step_b'] },
    { id: 'g_base', name: 'v3', steps: ['step_c'] },
    { id: 'g_end2', name: 'v4', event_key: 'on_end', steps: ['step_d'] },
  ];
  groups.forEach((g) => (g.steps || []).forEach((id) => scope.set(id, g.event_key || '')));

  const steps = STEPS.map((s) => {
    const comp = { ...s.components[0], next_step_id: extraLinks[s.id] || '' };
    return { id: s.id, components: [comp] };
  });

  const editor = {
    scenarioData: {
      steps,
      editor_data: { groups, ungrouped_steps: [] },
    },
    scenarioEvents: EVENTS,
    dirty: { markDirty() {} },
    componentTypes: {},
    allComponentTypes: {},
    getComponentLinkFields: (comp) => ['next_step_id'],
    getComponentLinks: (comp) => (comp.next_step_id
      ? [{ field: 'next_step_id', label: '', targetId: comp.next_step_id }]
      : []),
    getComponentSummary: () => '',
    findStep(id) { return steps.find((s) => s.id === id) || null; },
    findComponent(stepId, componentId) {
      return (this.findStep(stepId)?.components || []).find((c) => c.component_id === componentId) || null;
    },
    getCurrentViewState() {
      return {
        groups: groups.map((g) => ({ ...g, collapsed: false })),
        ungroupedSteps: [],
      };
    },
    isKnownScenarioEvent(key) {
      return typeof key === 'string' && key !== 'general' && Object.prototype.hasOwnProperty.call(EVENTS, key);
    },
    getGroupEventLabel(key) { return this.isKnownScenarioEvent(key) ? EVENTS[key] : 'No event'; },
    getStepEventKey(id) { return scope.get(id) || ''; },
    getEventStyles() {
      return {
        on_recieve_objectives: { color: '#60a5fa', icon: 'fa-solid fa-inbox' },
        on_end: { color: '#f59e0b', icon: 'fa-solid fa-flag-checkered' },
      };
    },
    getEventStyle(key) {
      const isEvent = this.isKnownScenarioEvent(key);
      const meta = (isEvent && this.getEventStyles()[key]) || {};
      return {
        key: isEvent ? key : '',
        isEvent,
        color: meta.color || '#64748b',
        icon: meta.icon || 'fa-solid fa-layer-group',
        label: isEvent ? this.getGroupEventLabel(key) : 'No event',
      };
    },
    renderAll() {},
    switchViewMode() {},
    addNewStepAt() { return null; },
    addNewGroup() {},
    moveStepToGroup() {},
    openDeleteGroupModal() {},
    renameGroupSilent() {},
    setGroupEventKey() {},
    generateUUID: () => 'uuid',
  };
  return editor;
}

function renderGraph(extraLinks) {
  const dom = new JSDOM(CANVAS_HTML, {
    pretendToBeVisual: true,
    runScripts: 'outside-only',
    url: 'https://editor.test/scenario-editor.html'
  });
  const { window } = dom;
  window.requestAnimationFrame = (cb) => { cb(); return 0; };
  window.localStorage.clear();
  // blueprint-view.js — обычный IIFE, который вешает класс на window.
  window.eval(read('editor-core/blueprint-view.js'));
  const editor = buildEditor(extraLinks);
  const canvas = window.document.getElementById('bp-canvas');
  const view = new window.EditorCore.BlueprintView(editor, canvas);
  view.show();
  return { window, view, editor };
}

test('graph draws no wire between different events', { skip: SKIP }, () => {
  const { window, view } = renderGraph();
  const wires = Array.from(window.document.querySelectorAll('#bp-wires path'));

  // Ни одного провода между step_a (on_end) и step_b (objectives) и т.п.
  const nodePos = (id) => view.bp.positions[id];
  assert.ok(nodePos('step_a') && nodePos('step_b'), 'узлы не разложены');

  const implicit = wires.filter((p) => p.classList.contains('bp-wire-implicit'));
  // step_a — единственный шаг on_end (плюс step_d в другом его группе, но связь
  // между группами одного события допустима: она остаётся). Единственная
  // допустимая пунктирная стрелка здесь — step_d → ничего (последний шаг).
  // step_b — единственный шаг objectives, поэтому пунктирных стрелок нет вовсе.
  assert.equal(implicit.length, 0, `лишние пунктирные связи: ${implicit.length}`);
});

test('dashed arrows inside one event are still drawn', { skip: SKIP }, () => {
  const { window, view, editor } = renderGraph();
  // Добавим второй шаг в то же событие on_end и перерисуем.
  editor.scenarioData.editor_data.groups[0].steps.push('step_d');
  editor.scenarioData.editor_data.groups[3].steps = [];
  view.render();
  const implicit = Array.from(window.document.querySelectorAll('#bp-wires path.bp-wire-implicit'));
  assert.equal(implicit.length, 1, 'внутри одного события стрелка нужна');
  // …и она покрашена в цвет своего события.
  assert.match(implicit[0].style.stroke, /#f59e0b|rgb\(245, 158, 11\)/i);
});

test('a stored link into another event is not drawn but flagged', { skip: SKIP }, () => {
  const { window, view } = renderGraph({ step_a: 'step_b' });
  assert.equal(window.document.querySelectorAll('#bp-wires path.bp-wire:not(.bp-wire-implicit)').length, 0,
    'провод между событиями не должен рисоваться');
  // Источник помечен предупреждением, чтобы мёртвую связь можно было убрать.
  const row = window.document.querySelector('.bp-node[data-step-id="step_a"] .bp-comp.is-cross-event');
  assert.ok(row, 'строка с межсобытийной связью не помечена');
  const pin = row.querySelector('.bp-pin-dot.is-cross-event');
  assert.ok(pin, 'точка связи не помечена');
  assert.match(pin.title, /different event/);
});

test('event legend lists the events and frames one on click', { skip: SKIP }, () => {
  const { window, view } = renderGraph();
  const chips = Array.from(window.document.querySelectorAll('#bp-event-legend .bp-event-chip'));
  assert.equal(chips.length, 3, 'ожидались «No event» + два события');
  const labels = chips.map((c) => c.textContent.replace(/\s+/g, ' ').trim());
  assert.ok(labels.some((l) => l.includes('On step end')));
  assert.ok(labels.some((l) => l.includes('On objectives received')));
  assert.ok(labels.some((l) => l.includes('No event')));

  const endChip = chips.find((c) => c.dataset.eventKey === 'on_end');
  endChip.dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
  assert.equal(endChip.classList.contains('is-active'), false, 'чип перерисовывается после клика');
  assert.equal(view._focusEventKey, 'on_end');
});

test('event groups and nodes carry their event colour', { skip: SKIP }, () => {
  const { window } = renderGraph();
  const endGroup = window.document.querySelector('.bp-group[data-group-id="g_end"]');
  assert.ok(endGroup.classList.contains('is-event'));
  assert.equal(endGroup.dataset.eventKey, 'on_end');
  assert.match(endGroup.style.getPropertyValue('--bp-event-color'), /#f59e0b/i);
  // Иконка события и его название в селекторе окрашены тем же цветом.
  assert.ok(endGroup.querySelector('.bp-group-flag.fa-flag-checkered'));
  assert.equal(endGroup.querySelector('.bp-group-event').value, 'on_end');
  // Узел внутри события: акцент + подпись «начало события» у корневого шага.
  const node = window.document.querySelector('.bp-node[data-step-id="step_a"]');
  assert.ok(node.classList.contains('has-event'));
  assert.ok(node.querySelector('.bp-node-event'), 'нет метки начала события');
});

test('linking into another event is refused with a toast, inside one event it is applied', { skip: SKIP }, () => {
  const { view, editor } = renderGraph();
  const toasts = [];
  editor.showToast = (message, type) => toasts.push({ message, type });

  assert.equal(view.applyLink('step_a', 'c_a', 'next_step_id', 'step_b'), false);
  assert.equal(editor.scenarioData.steps[0].components[0].next_step_id, '', 'мёртвая связь записана');
  assert.equal(toasts.length, 1);
  assert.match(toasts[0].message, /different events/);
  assert.equal(toasts[0].type, 'warning');

  // Внутри одного события (on_end: step_a + step_d) связь создаётся.
  assert.equal(view.applyLink('step_a', 'c_a', 'next_step_id', 'step_d'), true);
  assert.equal(editor.scenarioData.steps[0].components[0].next_step_id, 'step_d');
});
