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
  assert.match(fs.readFileSync(path.join(repo, 'src/game/server/core/components/bots/bot_data.cpp'), 'utf8'), /hasSet\("ambient_chat"\)/);
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

test('scenario branch components, component defaults, completion_logic, vouchers, and NPC roles match server C++', () => {
  const context = { window: {} };
  vm.runInNewContext(read('editor-core/registry.js'), context);
  const { componentSchemas } = context.window.EditorCore.schemas;

  for (const type of ['quest_condition', 'condition_movement']) {
    const schema = componentSchemas[type];
    assert.ok(schema, `missing schema ${type}`);
    assert.deepEqual(plain(schema.linkTargets), ['pass_step_id', 'fail_step_id'], `${type} must expose pass/fail linkTargets`);
    for (const field of ['timeout', 'pass_step_id', 'fail_step_id']) {
      assert.ok(schema.fields[field], `${type} missing field ${field}`);
    }
  }

  assert.equal(componentSchemas.use_chat_code.fields.hidden.default, true);
  assert.equal(componentSchemas.set_group_spawn.fields.enable_spawn.default, true);
  assert.equal(componentSchemas.condition_movement.fields.entire_group.default, false);
  assert.equal(componentSchemas.dungeon_activate_point.fields.entire_group.default, false);
  assert.equal(componentSchemas.dungeon_activate_point.fields.duration.ui.min, 0);

  const scenarioFilesPhp = read('api/scenario-files.php');
  const aiSchemaPhp = read('api/ai-schema.php');
  const scenarioBaseCpp = fs.readFileSync(path.join(repo, 'src/game/server/core/scenarios/base/scenario_base.cpp'), 'utf8');
  for (const logic of ['all_of', 'any_of', 'sequential']) {
    assert.ok(scenarioBaseCpp.includes(`"${logic}"`), `C++ missing logic ${logic}`);
    assert.ok(scenarioFilesPhp.includes(`'${logic}'`), `scenario-files.php missing logic ${logic}`);
    assert.ok(aiSchemaPhp.includes(`'${logic}'`), `ai-schema.php missing logic ${logic}`);
  }
  assert.doesNotMatch(scenarioFilesPhp, /'parallel'/);
  assert.doesNotMatch(aiSchemaPhp, /'parallel'/);

  const vouchersHtml = read('vouchers-editor.html');
  const voucherFns = vouchersHtml.slice(
    vouchersHtml.indexOf('    const emptyVoucher = () => ({'),
    vouchersHtml.indexOf('    const formatUntil = (sec) => {'),
  );
  const { fromRow, toPayload } = vm.runInNewContext(`${voucherFns}\n({ fromRow, toPayload })`);
  const parsedVoucher = fromRow({ ID: 1, Code: 'GIFT', Multiple: 1, ValidUntil: 1800000000, Data: { exp: 250, money: 500, items: [{ id: 1, value: 3 }] } });
  assert.deepEqual(plain(parsedVoucher), { ID: 1, Code: 'GIFT', Multiple: 1, ValidUntil: 1800000000, exp: 250, money: 500, items: [{ id: 1, value: 3 }] });
  assert.deepEqual(plain(toPayload(parsedVoucher)), { Code: 'GIFT', Multiple: 1, ValidUntil: 1800000000, Data: { items: [{ id: 1, value: 3 }], exp: 250, money: 500 } });
  assert.deepEqual(plain(toPayload({ Code: 'ONLY_ITEMS', Multiple: 0, ValidUntil: 0, exp: 0, money: 0, items: [] })), { Code: 'ONLY_ITEMS', Multiple: 0, ValidUntil: 0, Data: { items: [] } });
  const refSchema = JSON.parse(read('editor-core/reference-schema.json'));
  assert.deepEqual(refSchema.vouchers, [{ column: 'Data', codec: 'json', paths: { 'items.*.id': 'items' } }]);
  assert.match(aiSchemaPhp, /'ValidUntil'\s*=>\s*\['type'\s*=>\s*'int'/);
  assert.match(aiSchemaPhp, /'Function'\s*=>\s*\['type'\s*=>\s*'int',\s*'nullable'\s*=>\s*false,\s*'min'\s*=>\s*-1,\s*'max'\s*=>\s*2/);

  const botsNpcHtml = read('bots-npc-editor.html');
  const npcOpts = literal(botsNpcHtml, 'FUNCTION_OPTIONS');
  assert.deepEqual(npcOpts.map(o => o.value), [-1, 0, 1, 2]);
  assert.match(read('editor-core/field-renderer.js'), /ui\.valueType\s*\?\s*`data-value-type=/);
});
