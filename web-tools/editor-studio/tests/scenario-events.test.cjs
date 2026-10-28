const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

// События сценария (`on_end`, `on_recieve_objectives`, …) сервер запускает как
// отдельные, независимые сценарии. Значит связь между шагами разных событий
// никогда не сработает — и рисовать её нельзя (иначе граф показывает переход,
// которого не существует). Тесты проверяют именно это: линии не строятся через
// границу события, а попытка создать такую связь отклоняется.

const ROOT = path.join(__dirname, '..');
const read = (name) => fs.readFileSync(path.join(ROOT, name), 'utf8');

/** Методы графа, отвечающие за топологию, без DOM и без камеры. */
const graphTopology = () => {
  const js = read('editor-core/blueprint-view.js');
  const start = js.indexOf('    // ---- event scopes');
  const end = js.indexOf('    // ---- auto layout');
  assert.ok(start > 0 && end > start, 'не найдены helpers событий в blueprint-view.js');
  const methods = js.slice(start, end);
  const context = vm.createContext({ Map, Set });
  return { instance: vm.runInNewContext(`new (class Graph { ${methods} })()`, context), js };
};

/**
 * Мини-граф: два события (on_end / on_recieve_objectives) и базовый блок ''.
 * `links` — [stepId, targetId] пары вместо настоящих компонентов.
 */
const buildGraph = (links = []) => {
  const { instance, js } = graphTopology();
  const scopeByStep = new Map([
    ['a1', 'on_end'],
    ['a2', 'on_end'],
    ['b1', 'on_recieve_objectives'],
    ['b2', 'on_recieve_objectives'],
    ['u1', ''],
  ]);
  const steps = Array.from(scopeByStep.keys()).map((id) => ({
    id,
    components: [{ component_id: `comp_${id}`, next_step_id: links.find(([from]) => from === id)?.[1] || '' }],
  }));
  const linkByStep = new Map(links);

  instance.editor = {
    scenarioData: { steps },
    getComponentLinks: (comp) => {
      const from = steps.find((s) => s.components[0] === comp)?.id;
      const to = linkByStep.get(from);
      return to ? [{ field: 'next_step_id', label: '', targetId: to }] : [];
    },
    findStep: (id) => steps.find((s) => s.id === id) || null,
    getStepEventKey: (id) => scopeByStep.get(id) || '',
    isKnownScenarioEvent: (key) => typeof key === 'string' && key !== '' && key !== 'general',
    getGroupEventLabel: (key) => key,
    getEventStyle: (key) => ({
      key: key || '', isEvent: !!key, color: '#64748b',
      icon: 'fa-solid fa-layer-group', label: key || 'No event',
    }),
  };
  instance.stepGroup = new Map();
  instance.viewState = { groups: [], ungroupedSteps: [] };
  instance.flatOrder = ['a1', 'a2', 'b1', 'b2', 'u1'];
  return { instance, js };
};

test('fall-through arrows stop at the end of an event block', () => {
  // a1→a2 inside on_end, b1→b2 inside objectives: both are fine.
  // a2→b1 and b2→u1 cross events: they must not exist.
  const { instance } = buildGraph();
  const edges = Array.from(instance.getImplicitEdges()).map(({ from, to }) => `${from}->${to}`);
  assert.deepEqual(edges, ['a1->a2', 'b1->b2']);

  // …and the same holds for "has incoming": b1 is a root of its own event,
  // even though a2 sits right before it in list order.
  assert.equal(instance.hasIncoming('b1'), false);
  assert.equal(instance.hasIncoming('a2'), true);
});

test('every event starts a new flow: no node is chained to another event', () => {
  const { instance } = buildGraph();
  assert.equal(instance.stepsShareEvent('a2', 'b1'), false);
  assert.equal(instance.stepsShareEvent('a1', 'a2'), true);
  // An ungrouped step lives in the base block, so it is separated as well.
  assert.equal(instance.stepsShareEvent('b2', 'u1'), false);
});

test('a stored link into another event is flagged instead of being drawn', () => {
  const { instance, js } = buildGraph([['a1', 'b2']]);
  const edges = Array.from(instance.getEdges());
  assert.equal(edges.length, 1);
  assert.equal(edges[0].crossEvent, true);
  // The graph must skip those edges when it draws the wires.
  const draw = js.slice(js.indexOf('    drawWires() {'), js.indexOf('    bezier(a, b) {'));
  assert.match(draw, /if \(crossEvent\) return;/);
  // Implicit (dashed) arrows carry the colour of their event.
  assert.match(draw, /this\.eventStyle\(eventKey\)/);
});

test('a link inside one event is accepted', () => {
  const { instance } = buildGraph([['a1', 'a2']]);
  const edges = Array.from(instance.getEdges());
  assert.equal(edges.length, 1);
  assert.equal(edges[0].crossEvent, false);
});

test('creating a cross-event link is refused with an explanation', () => {
  const js = read('editor-core/blueprint-view.js');
  const applyStart = js.indexOf('    applyLink(stepId, componentId, field, targetId) {');
  const applyBody = js.slice(applyStart, applyStart + 900);
  assert.match(applyBody, /if \(!this\.stepsShareEvent\(stepId, targetId\)\)/);
  assert.match(applyBody, /this\.explainCrossEventLink\(stepId, targetId\)/);
  // …and the refusal is phrased in terms of events, not of a random error.
  assert.match(js, /explainCrossEventLink\(fromStepId, toStepId\)/);
  assert.match(js, /different events/);
  // While a wire is dragged, nodes of other events are dimmed / marked blocked.
  assert.match(js, /is-event-mismatch/);
  assert.match(js, /is-drop-blocked/);
});

test('the step list also draws no arrow between different events', () => {
  const html = read('scenario-editor.html');
  const draw = html.slice(html.indexOf('            drawConnections() {'), html.indexOf('            detectNavigatorStacked() {'));
  assert.match(draw, /this\.getStepEventKey\(step\.id\)/);
  assert.match(draw, /this\.getStepEventKey\(link\.targetId\) === fromEvent/);
  // Every event has a colour/icon shared by both views.
  assert.match(html, /getEventStyle\(eventKey\) \{/);
  assert.match(html, /getStepEventKey\(stepId\) \{/);
  // Палитра событий — один общий модуль (его же использует превью графа).
  assert.match(html, /editor-core\/scenario-events\.js/);
  const palette = read('editor-core/scenario-events.js');
  Object.keys({
    on_recieve_objectives: 1, on_complete_objectives: 1, on_end: 1,
    on_got: 1, on_lost: 1, on_equip: 1, on_unequip: 1, on_use: 1,
  }).forEach((key) => assert.match(palette, new RegExp(`${key}: \\{ color:`), `${key} без цвета`));
});

test('the graph explains its events and can focus one of them', () => {
  const js = read('editor-core/blueprint-view.js');
  assert.match(js, /renderEventLegend\(\)/);
  assert.match(js, /focusEvent\(eventKey\)/);
  assert.match(js, /bp-event-chip/);
  const html = read('scenario-editor.html');
  assert.match(html, /id="bp-event-legend"/);
  const css = read('editor-core/blueprint-view.css');
  assert.match(css, /\.bp-event-legend \{/);
  // On a phone the chips stay reachable (they scroll inside the canvas).
  assert.match(css, /@media \(max-width: 767px\) \{[\s\S]{0,400}?\.bp-event-legend \{/);
});

test('groups show which event runs them', () => {
  const js = read('editor-core/blueprint-view.js');
  // Рамка группы получает цвет события, а иконка в шапке — иконку события.
  assert.match(js, /frame\.style\.setProperty\('--bp-event-color', eventStyle\.color\)/);
  assert.match(js, /bp-group-flag \$\{eventStyle\.icon\}/);
  const html = read('scenario-editor.html');
  assert.match(html, /step-group-event-chip/);
  assert.match(html, /nav-event-badge/);
});

test('the graph preview page stays in sync with the editor palette', () => {
  const preview = read('tools/scenario-graph-preview.html');
  // Витрина грузит тот же движок и ту же палитру событий, что и редактор.
  assert.match(preview, /editor-core\/scenario-events\.js/);
  assert.match(preview, /editor-core\/blueprint-view\.js/);
  assert.match(preview, /new window\.EditorCore\.BlueprintView/);
  assert.match(preview, /on_recieve_objectives/);
  assert.match(preview, /on_end/);
  // Палитра — единственный источник цветов: в редакторе её больше нет.
  const html = read('scenario-editor.html');
  assert.match(html, /window\.EditorCore\?\.ScenarioEvents\?\.COLORS/);
});
