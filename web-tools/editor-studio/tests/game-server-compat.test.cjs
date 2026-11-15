const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const studio = path.join(__dirname, '..');
const repo = path.resolve(studio, '../..');
const read = file => fs.readFileSync(path.join(studio, file), 'utf8');
const sql = fs.readFileSync(path.join(repo, 'mrpg_shared.sql'), 'utf8');
const plain = value => JSON.parse(JSON.stringify(value));
const sorted = values => Array.from(new Set(values)).sort();

function sqlValues(table, column) {
  const tableBody = sql.match(new RegExp('CREATE TABLE(?: IF NOT EXISTS)? `'+ table +'` \\(([\\s\\S]*?)\\) ENGINE='));
  assert.ok(tableBody, `missing table ${table}`);
  const declaration = tableBody[1].match(new RegExp('`' + column + "` (?:enum|set)\\(((?:'(?:[^']|'')*',?)+)\\)"));
  assert.ok(declaration, `missing enum/set ${table}.${column}`);
  return [...declaration[1].matchAll(/'((?:[^']|'')*)'/g)].map(m => m[1].replace(/''/g, "'"));
}

function literal(source, name) {
  const match = source.match(new RegExp(`const ${name} = (\\[[\\s\\S]*?\\]);`));
  assert.ok(match, `missing ${name}`);
  return plain(vm.runInNewContext(match[1]));
}

test('editor world, mob, and item options match the current game schema', () => {
  const worlds = read('worlds-editor.html');
  const mobs = read('mobs-editor.html');
  const items = read('items-editor.html');
  assert.deepEqual(sorted(literal(worlds, 'TYPES')), sorted(sqlValues('tw_worlds', 'Type')));
  assert.deepEqual(sorted(literal(worlds, 'FLAGS')), sorted(sqlValues('tw_worlds', 'Flags')));
  assert.deepEqual(sorted(literal(mobs, 'BEHAVIORS')), sorted(sqlValues('tw_bots_mobs', 'Behavior')));
  assert.deepEqual(sorted(literal(mobs, 'DEBUFFS')), sorted(sqlValues('tw_bots_mobs', 'Debuffs')));
  assert.deepEqual(sorted(literal(items, 'GROUPS')), sorted(sqlValues('tw_items_list', 'Group')));
  const rules = items.match(/const TYPE_RULES = (\(\(\) => \{[\s\S]*?\}\)\(\));/);
  assert.ok(rules);
  const types = Object.values(plain(vm.runInNewContext(rules[1]))).flat();
  assert.deepEqual(sorted(types), sorted(sqlValues('tw_items_list', 'Type')));
});

test('quest flags round-trip the exact values consumed by SQL and the game server', () => {
  const html = read('quests-editor.html');
  const definition = html.match(/const QuestFlags = Object\.freeze\(\{[\s\S]*?\}\);/)[0];
  const parse = html.match(/const parseFlags = \(raw\) => \{[\s\S]*?\n    \};/)[0];
  const result = vm.runInNewContext(`${definition}\nconst QUEST_FLAG_OPTIONS = Object.values(QuestFlags);\n${parse}\n({ QuestFlags, parseFlags })`);
  assert.deepEqual(sorted(Object.values(result.QuestFlags)), sorted(sqlValues('tw_quests_list', 'Flags')));
  const flags = 'Type repeatable,No activity point';
  assert.equal(Array.from(result.parseFlags(flags)).join(','), flags);
  const server = fs.readFileSync(path.join(repo, 'src/game/server/core/components/quests/quest_data.h'), 'utf8');
  assert.match(server, /hasSet\("No activity point"\)/);
});

test('game schema includes server-supported enum values', () => {
  for (const [table, column, value] of [
    ['tw_worlds', 'Type', 'social'], ['tw_bots_mobs', 'Behavior', 'ambient_chat'], ['tw_items_list', 'Type', 'Equip potion Miner'],
  ]) assert.equal(sqlValues(table, column).at(-1), value, 'new enum/set values are appended');
  assert.match(fs.readFileSync(path.join(repo, 'src/engine/shared/world_detail.h'), 'utf8'), /Type == "social"/);
  assert.match(fs.readFileSync(path.join(repo, 'src/game/server/core/components/Bots/BotData.cpp'), 'utf8'), /hasSet\("ambient_chat"\)/);
  assert.match(fs.readFileSync(path.join(repo, 'src/game/server/core/mmo_context.h'), 'utf8'), /hasSet\("Equip potion Miner"\)/);
});

test('editor can represent every registered server scenario component', () => {
  const context = { window: {} };
  vm.runInNewContext(read('editor-core/registry.js'), context);
  const { componentSchemas, scenarioModeSchemas } = context.window.EditorCore.schemas;
  const sources = [
    'src/game/server/core/scenarios/impl/components/default.h',
    'src/game/server/core/scenarios/impl/scenario_world.cpp',
    'src/game/server/core/scenarios/impl/scenario_dungeon.cpp',
    'src/game/server/core/scenarios/impl/scenario_universal.cpp',
  ];
  const names = sources.flatMap(file => [...fs.readFileSync(path.join(repo, file), 'utf8').matchAll(/DECLARE_COMPONENT_NAME\("([^"]+)"\)/g)].map(m => m[1]));
  const offered = new Set(Object.values(scenarioModeSchemas).flatMap(mode => Array.from(mode.componentTypes)));
  for (const name of names) {
    assert.ok(componentSchemas[name], `missing component schema ${name}`);
    assert.ok(offered.has(name), `component ${name} is not offered in any mode`);
  }
  assert.ok(scenarioModeSchemas.world.componentTypes.includes('world_group_lives'));
});

test('scenario timing uses server ticks and saves execution_time for every component', () => {
  const html = read('scenario-editor.html');
  const registry = read('editor-core/registry.js');
  const component = fs.readFileSync(path.join(repo, 'src/game/server/core/scenarios/base/component.h'), 'utf8');
  const scenario = fs.readFileSync(path.join(repo, 'src/game/server/core/scenarios/base/scenario_base.cpp'), 'utf8');
  assert.match(component, /m_ExecutionTimeTick--/);
  assert.match(scenario, /Server\(\)->Tick\(\) - m_LastStepTimeTick\) >= currentStep.m_DelayTick/);
  assert.doesNotMatch(html + registry, /(?:Delay|Run time) \(ms\)/);
  assert.match(html, /Delay \(ticks\)/);
  assert.match(registry, /Run time \(ticks\)/);
  const method = html.slice(html.indexOf('            buildScenarioContent() {'), html.indexOf('            // Build the standalone'));
  assert.ok(method.includes('buildScenarioContent()'));
  const instance = vm.runInNewContext(`new (class { ${method} })()`);
  instance.scenarioData = { steps: [{ id: 'a', delay: 100, components: [{ type: 'teleport', execution_time: 75, position: { x: 1, y: 2 } }] }], editor_data: {} };
  instance.currentMode = 'universal';
  instance.normalizeGroup = value => value;
  instance.isKnownScenarioEvent = () => false;
  instance.getComponentLinkFields = () => [];
  instance.buildBlueprintBlock = () => null;
  const result = plain(instance.buildScenarioContent());
  assert.equal(result.steps[0].delay, 100);
  assert.equal(result.steps[0].components[0].execution_time, 75);
});
