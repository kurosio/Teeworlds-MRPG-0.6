// Shared PHP/JavaScript fixtures: IDs deliberately also occur as quantities/text.
const I = 2000000001, W = 2000000002, B = 2000000003, Q = 2000000004;
const idMap = { [I]: 81, [W]: 82, [B]: 83, [Q]: 84 };
const scenario = {
  editor_data: { groups: [{ id: String(I), name: String(I) }], empty: {} },
  steps: [{ id: String(I), components: [
    { type: 'check_has_item', item_id: I, required: I },
    { type: 'pick_item_task', item: { id: I, value: I }, position: { x: I, y: W } },
    { type: 'accept_quest', quest_id: Q },
    { type: 'teleport', world_id: W },
    { type: 'defeat_mobs', mobs: [{ bot_id: B, count: B, drops: [{ item_id: I, count: I }] }] },
    { type: 'message', chat: `plain ${I}, <item_${I}> <bot_${B}> <world_${W}> <quest_${Q}>` },
  ] }],
};
const mappedScenario = JSON.parse(JSON.stringify(scenario));
const cs = mappedScenario.steps[0].components;
cs[0].item_id = 81; cs[1].item.id = 81; cs[2].quest_id = 84; cs[3].world_id = 82;
cs[4].mobs[0].bot_id = 83; cs[4].mobs[0].drops[0].item_id = 81;
cs[5].chat = `plain ${I}, <item_81> <bot_83> <world_82> <quest_84>`;
const fixtures = [
  { resource: 'crafts', data: { ItemID: I, ItemValue: I, RequiredItems: `[${I}/${I}],[5/1]`, GroupName: String(I) },
    expected: { ItemID: 81, ItemValue: I, RequiredItems: `[81/${I}],[5/1]`, GroupName: String(I) } },
  { resource: 'crafts', data: { RequiredItems: JSON.stringify([[I, I], { id: I, value: I }, { ItemID: I, amount: I }, { key: String(I), qty: I }]) },
    expected: { RequiredItems: JSON.stringify([[81, I], { id: 81, value: I }, { ItemID: 81, amount: I }, { key: '81', qty: I }]) } },
  { resource: 'items', data: { Name: String(I), Description: `ID ${I} <item_${I}>`, InitialPrice: I, Data: { random_box: [{ item_id: I, value: I, chance: 10 }], box: [{ item_id: String(I), value: I }], use_scenario: { world_id: W, duration: W, rewards: [{ item_id: I, value: I }] }, bonus: { amount: I }, extension: { item_id: I } } },
    expected: { Name: String(I), Description: `ID ${I} <item_${I}>`, InitialPrice: I, Data: { random_box: [{ item_id: 81, value: I, chance: 10 }], box: [{ item_id: '81', value: I }], use_scenario: { world_id: 82, duration: W, rewards: [{ item_id: 81, value: I }] }, bonus: { amount: I }, extension: { item_id: I } } } },
  { resource: 'warehouses', data: { Currency: I, Trades: `*Group ${I}\n[${I}/${I}(${I}/${I})]\n[COLLECTION(${I}:${I})]\nraw ${I}` },
    expected: { Currency: 81, Trades: `*Group ${I}\n[81/${I}(${I}/${I})]\n[COLLECTION(${I}:${I})]\nraw ${I}` } },
  { resource: 'nodes_collecting', data: { Properties: `[${I}/12.5],[${I}/2/5]`, WorldID: W, SwitchID: 200 },
    expected: { Properties: '[81/12.5],[81/2/5]', WorldID: 82, SwitchID: 200 } },
  { resource: 'bots_info', data: { SlotHammer: I, EquippedModules: `${I}, ${I},17,0`, JsonTeeInfo: JSON.stringify({ body_color: I }) },
    expected: { SlotHammer: 81, EquippedModules: '81, 81,17,0', JsonTeeInfo: JSON.stringify({ body_color: I }) } },
  { resource: 'bots_mobs', data: { BotID: B, WorldID: W, it_drop_0: I, it_drop_count: String(I), PositionX: W },
    expected: { BotID: 83, WorldID: 82, it_drop_0: 81, it_drop_count: String(I), PositionX: W } },
  { resource: 'bots_npc', data: { BotID: B, GiveQuestID: Q, DialogData: [{ left_speaker_id: B, right_speaker_id: -1, text: `<item_${I}> <bot_${B}> plain ${I}`, action: false }] },
    expected: { BotID: 83, GiveQuestID: 84, DialogData: [{ left_speaker_id: 83, right_speaker_id: -1, text: `<item_81> <bot_83> plain ${I}`, action: false }] } },
  { resource: 'quest_bots', data: { TasksData: { required_items: [{ id: I, value: I }], reward_items: [{ id: I, value: I }], defeat_bots: [{ id: B, value: B }], move_to: [{ world_id: W, x: W, pick_up_item: { id: I, value: I }, required_item: { id: I, value: I }, defeat_bot: { id: B, world_id: W, attribute_power: B }, required_item_id: I, pick_up_item_id: I, defeat_bot_id: B, defeat_bot_world_id: W }], text: String(I) } },
    expected: { TasksData: { required_items: [{ id: 81, value: I }], reward_items: [{ id: 81, value: I }], defeat_bots: [{ id: 83, value: B }], move_to: [{ world_id: 82, x: W, pick_up_item: { id: 81, value: I }, required_item: { id: 81, value: I }, defeat_bot: { id: 83, world_id: 82, attribute_power: B }, required_item_id: 81, pick_up_item_id: 81, defeat_bot_id: 83, defeat_bot_world_id: 82 }], text: String(I) } } },
  ...[['items','ScenarioData'],['quest_bots','ScenarioData'],['dungeons','Scenario']].map(([resource, column]) => ({
    resource, data: { [column]: JSON.stringify(scenario) }, expected: { [column]: JSON.stringify(mappedScenario) },
  })),
  { resource: 'worlds', data: { RespawnWorldID: W, JailWorldID: W, RequiredQuestID: Q, RequiredLevel: W },
    expected: { RespawnWorldID: 82, JailWorldID: 82, RequiredQuestID: Q, RequiredLevel: W } },
  { resource: 'quest_board_links', data: { QuestID: Q, DailyBoardID: 7 }, expected: { QuestID: 84, DailyBoardID: 7 } },
  { resource: 'quests', data: { NextQuestID: Q, RewardGold: Q }, expected: { NextQuestID: 84, RewardGold: Q } },
  { resource: 'items', data: { Data: '{ "bonus": { "amount": 2000000001 }, "empty": {} }' },
    expected: { Data: '{ "bonus": { "amount": 2000000001 }, "empty": {} }' } },
];
module.exports = { I, W, B, Q, idMap, fixtures };
