(() => {
  const registry = new Map();
  const createField = (type, label, defaultValue, { validate = null, ui = null, ...rest } = {}) => ({
    type,
    label,
    default: defaultValue,
    validate,
    ui: type === 'vec2' ? { highlightTiles: [], ...ui } : ui,
    ...rest
  });

// One-liner for DB-backed selects.
// New default UI (db_select): single searchable input with dropdown.
// Usage: createDbSelect('Квест', 0, 'quest')
const createDbSelect = (label, defaultValue, dbKey, { ui = {}, validate = null, ...rest } = {}) =>
  createField('db_select', label, defaultValue, {
    validate,
    ui: { dbKey, ...ui },
    ...rest
  });

  const actionSchemas = {
    group_header: {
      name: 'Group title',
      icon: 'fa-layer-group',
      fields: {
        name: createField('text', 'Name', 'New group', {
          ui: { placeholder: 'Enter the group name' }
        })
      }
    },
    message: {
      name: 'Message',
      icon: 'fa-comment',
      fields: {
        delay: createField('number', 'Delay', 0, { ui: { min: 0, max: 600000 } }),
        chat: createField('text', 'Chat', '', { ui: { placeholder: 'Enter chat text', format: 'textarea' } }),
        broadcast: createField('text', 'Broadcast to everyone', '', { ui: { placeholder: 'Enter the announcement text', format: 'textarea' } }),
        full: createField('text', 'Full message', '', { ui: { placeholder: 'Enter the full message', format: 'textarea' } })
      }
    },
    movement_task: {
      name: 'Movement objective',
      icon: 'fa-person-running',
      fields: {
        delay: createField('number', 'Delay', 0, { ui: { min: 0, max: 600000 } }),
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        target_lock_text: createField('text', 'Target lock text', '', { ui: { placeholder: 'e.g. Follow the marker' } }),
        target_look: createField('boolean', 'Look at the target', false),
        chat: createField('text', 'Chat', '', { ui: { placeholder: 'Enter chat text', format: 'textarea' } }),
        broadcast: createField('text', 'Broadcast to everyone', '', { ui: { placeholder: 'Enter the announcement text', format: 'textarea' } }),
        full: createField('text', 'Full message', '', { ui: { placeholder: 'Enter the full message', format: 'textarea' } })
      }
    },
    check_has_item: {
      name: 'Check the item',
      icon: 'fa-box',
      fields: {
        item_id: createDbSelect('Item', 0, 'item', { ui: { placeholder: '— select an item —' } }),
        required: createField('number', 'Quantity', 1, { ui: { min: 1, max: 9999 } }),
        remove: createField('boolean', 'Remove after check', false),
        show_progress: createField('boolean', 'Show progress', false)
      }
    },
    reset_quest: {
      name: 'Reset quest',
      icon: 'fa-undo',
      fields: {
        quest_id: createDbSelect('Quest', 0, 'quest', { ui: { placeholder: '— select a quest —' } })
      }
    },
    accept_quest: {
      name: 'Accept quest',
      icon: 'fa-check-double',
      fields: {
        quest_id: createDbSelect('Quest', 0, 'quest', { ui: { placeholder: '— select a quest —' } })
      }
    },
    new_door: {
      name: 'Create door',
      icon: 'fa-door-closed',
      fields: {
        key: createField('door_key', 'Key', '', { ui: { placeholder: 'Enter or pick a key' } }),
        follow: createField('boolean', 'Follow the player', false),
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
    remove_door: {
      name: 'Remove door',
      icon: 'fa-door-open',
      fields: {
        key: createField('door_key', 'Key', '', { ui: { placeholder: 'Enter or pick a key' } }),
        follow: createField('boolean', 'Follow the player', false)
      }
    },
    pick_item_task: {
      name: 'Collect objective',
      icon: 'fa-hand-sparkles',
      fields: {
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        item: createField('item', 'Item', { id: 0, value: 0 }, { ui: { datasource: 'items', placeholder: '— select an item —', min: 0, max: 999999 } }),
        chat: createField('text', 'Chat', '', { ui: { placeholder: 'Enter chat text', format: 'textarea' } }),
        broadcast: createField('text', 'Broadcast to everyone', '', { ui: { placeholder: 'Enter the announcement text', format: 'textarea' } }),
        full: createField('text', 'Full message', '', { ui: { placeholder: 'Enter the full message', format: 'textarea' } })
      }
    },
    emote: {
      name: 'Emotion',
      icon: 'fa-smile',
      fields: {
        emote_type: createField('number', 'Emote type', 0, { ui: { min: 0, max: 999 } }),
        emoticon_type: createField('number', 'Emote type (icon)', 0, { ui: { min: 0, max: 999 } })
      }
    },
    teleport: {
      name: 'Teleport',
      icon: 'fa-plane-departure',
      fields: {
        // Телепорт сценария: карта открывается в мире записи и показывает
        // игровой слой; запрет тайлов здесь не нужен (точка может быть любой).
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1, mapWorldPath: 'world_id', showTiles: true } }),
        world_id: createDbSelect('World', 0, 'world', { ui: { placeholder: '— select a world —' } })
      }
    },
    fix_cam: {
      name: 'Lock the camera',
      icon: 'fa-camera',
      fields: {
        delay: createField('number', 'Delay', 0, { ui: { min: 0, max: 600000 } }),
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
    freeze_movements: {
      name: 'Freeze movement',
      icon: 'fa-snowflake',
      fields: {
        state: createField('boolean', 'Enable', false)
      }
    },
    check_quest_accepted: {
      name: 'Check: quest accepted',
      icon: 'fa-question-circle',
      fields: {
        quest_id: createField('db_select', 'Quest', 0, { datasource: 'quests', ui: { placeholder: '— select a quest —' } })
      }
    },
    check_quest_finished: {
      name: 'Check: quest completed',
      icon: 'fa-flag-checkered',
      fields: {
        quest_id: createField('db_select', 'Quest', 0, { datasource: 'quests', ui: { placeholder: '— select a quest —' } })
      }
    },
    check_quest_step_finished: {
      name: 'Check: quest step completed',
      icon: 'fa-list-check',
      fields: {
        quest_id: createField('db_select', 'Quest', 0, { datasource: 'quests', ui: { placeholder: '— select a quest —' } }),
        step: createField('number', 'Step number', 0, { ui: { min: 0, max: 9999 } }),
        entire_group: createField('boolean', 'Require for the whole group', false)
      }
    },
    shootmarkers: {
      name: 'Shooting at markers',
      icon: 'fa-crosshairs',
      fields: {
        markers: createField('list', 'Markers', [], {
          itemFields: {
            position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
            health: createField('number', 'Health', 1, { ui: { min: 1, max: 9999 } })
          },
          ui: { layout: 'row', placeholder: 'Add a marker' }
        })
      }
    }
  };

  const componentSchemas = {
    message: {
      name: 'Message',
      class: 'info',
      icon: 'fa-solid fa-comment-dots',
      desc: 'Show text to the player',
      fields: {
        execution_time: createField('number', 'Run time (ms)', 0, { ui: { min: 0, max: 600000 } }),
        text: createField('text', 'Text', 'New message', { ui: { placeholder: 'Enter the message text', format: 'textarea' } }),
        mode: createField('text', 'Mode', 'full', {
          ui: { type: 'select', options: ['full', 'broadcast', 'chat'] }
        })
      }
    },
    wait: {
      name: 'Wait',
      class: 'info',
      icon: 'fa-solid fa-clock',
      desc: 'Scenario pause',
      fields: {
        duration: createField('number', 'Duration', 5, { ui: { min: 1, max: 9999 } })
      }
    },
    branch_random: {
      name: 'Random branch',
      class: 'logic',
      icon: 'fa-solid fa-shuffle',
      desc: 'Branch by probability',
      fields: {
        chance_percent: createField('number', 'Chance true (%)', 50, { ui: { min: 0, max: 100 } }),
        true_step_id: createField('text', 'TRUE branch', '', { ui: { placeholder: 'Step ID on success' } }),
        false_step_id: createField('text', 'FALSE branch', '', { ui: { placeholder: 'Step ID on failure' } })
      },
      linkTargets: ['true_step_id', 'false_step_id']
    },
    dungeon_door_control: {
      name: 'Dungeon: door control',
      class: 'interactive',
      icon: 'fa-solid fa-door-open',
      desc: 'Door control for a dungeon scenario',
      fields: {
        action: createField('text', 'Action', 'create', {
          ui: { type: 'select', options: ['create', 'remove'] }
        }),
        position: createField('vec2', 'Position', { x: 100, y: 100 }, { 
          ui: { min: -99999, max: 99999, step: 0.1, showWhen: { path: 'action', equals: 'create' } } 
        }),
        key: createField('text', 'Key', '', { ui: { placeholder: 'Enter the door key' } })
      }
    },
    use_chat_code: {
      name: 'Chat code',
      class: 'interactive',
      icon: 'fa-solid fa-key',
      desc: 'Chat text condition',
      fields: {
        code: createField('text', 'Code', 'secret', { ui: { placeholder: 'Enter the code word' } }),
        next_step_id: createField('text', 'Next step', ''),
        hidden: createField('boolean', 'Hide', false)
      }
    },
    restore_resources: {
      name: 'Restore resources',
      class: 'logic',
      icon: 'fa-solid fa-heart-circle-plus',
      desc: 'Restores HP/MP/Ammo for the scenario players',
      fields: {
        restore_hp: createField('boolean', 'Restore HP', true),
        restore_mp: createField('boolean', 'Restore MP', true),
        restore_ammo: createField('boolean', 'Restore Ammo', true)
      }
    },
    world_complete: {
      name: 'World: finish scenario',
      class: 'interactive',
      icon: 'fa-solid fa-flag-checkered',
      desc: 'Force-finish a world scenario',
      fields: {
        successful: createField('boolean', 'Success', true)
      }
    },
    set_group_flags: {
      name: 'Set group flags',
      class: 'logic',
      icon: 'fa-solid fa-flag',
      desc: 'Set base flags for group scenarios only',
      fields: {
        disable_group_damage: createField('boolean', 'No friendly fire', false),
        disable_group_collision: createField('boolean', 'No collision between allies', false),
        disable_group_hooking: createField('boolean', 'No hook on allies', false),
        disable_group_pvp: createField('boolean', 'No PvP between allies', false)
      }
    },
	set_group_lives: {
      name: 'Group lives control',
      class: 'logic',
      icon: 'fa-solid fa-earth-europe',
      desc: 'Set / add / remove group lives in a scenario',
      fields: {
        action: createField('text', 'Operation', 'set', {
          ui: {
            type: 'select',
            options: ['set', 'add', 'sub']
          }
        }),
        value: createField('number', 'Lives amount', 1, { ui: { min: 0, max: 9999 } })
      }
    },	
	set_group_spawn: {
      name: 'Group respawn point',
      class: 'logic',
      icon: 'fa-solid fa-flag',
      desc: 'Configure the group respawn point',
      fields: {
        enable_spawn: createField('boolean', 'Enable respawn at the given position', false),
        position: createField('vec2', 'Position', { x: 100, y: 100 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
    check_has_item: {
      name: 'Check the item',
      class: 'condition',
      icon: 'fa-solid fa-box-open',
      desc: 'Check for an item in the inventory',
      fields: {
        item_id: createDbSelect('Item', 0, 'item', { ui: { placeholder: '— select an item —' } }),
        required: createField('number', 'Quantity', 1, { ui: { min: 1, max: 9999 } }),
        remove: createField('boolean', 'Remove after check', false),
        show_progress: createField('boolean', 'Show progress', false)
      }
    },
    reset_quest: {
      name: 'Reset quest',
      class: 'quest',
      icon: 'fa-solid fa-rotate-left',
      desc: 'Reset quest state',
      fields: {
        quest_id: createDbSelect('Quest', 0, 'quest', { ui: { placeholder: '— select a quest —' } })
      }
    },
    accept_quest: {
      name: 'Accept quest',
      class: 'quest',
      icon: 'fa-solid fa-clipboard-check',
      desc: 'Give the player a quest',
      fields: {
        quest_id: createDbSelect('Quest', 0, 'quest', { ui: { placeholder: '— select a quest —' } })
      }
    },
    check_quest_accepted: {
      name: 'Quest accepted',
      class: 'condition',
      icon: 'fa-solid fa-circle-check',
      desc: 'Check quest acceptance',
      fields: {
        quest_id: createDbSelect('Quest', 0, 'quest', { ui: { placeholder: '— select a quest —' } })
      }
    },
    check_quest_finished: {
      name: 'Quest completed',
      class: 'condition',
      icon: 'fa-solid fa-flag-checkered',
      desc: 'Check quest completion',
      fields: {
        quest_id: createDbSelect('Quest', 0, 'quest', { ui: { placeholder: '— select a quest —' } })
      }
    },
    check_quest_step_finished: {
      name: 'Quest step completed',
      class: 'condition',
      icon: 'fa-solid fa-list-check',
      desc: 'Check quest step progress',
      fields: {
        quest_id: createDbSelect('Quest', 0, 'quest', { ui: { placeholder: '— select a quest —' } }),
        step: createField('number', 'Step number', 0, { ui: { min: 0, max: 9999 } }),
        entire_group: createField('boolean', 'Require for the whole group', false)
      }
    },
    pick_item_task: {
      name: 'Collect-item objective',
      class: 'interactive',
      icon: 'fa-solid fa-hand',
      desc: 'Create a collect-item objective',
      fields: {
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        item: createField('item', 'Item', { id: 0, value: 0 }, { ui: { datasource: 'items', placeholder: '— select an item —', min: 0, max: 999999 } }),
        chat: createField('text', 'Chat', '', { ui: { placeholder: 'Enter chat text', format: 'textarea' } }),
        broadcast: createField('text', 'Broadcast to everyone', '', { ui: { placeholder: 'Enter the announcement text', format: 'textarea' } })
      }
    },
    shootmarkers: {
      name: 'Shooting markers',
      class: 'combat',
      icon: 'fa-solid fa-bullseye',
      desc: 'Destroy markers by shooting',
      fields: {
        markers: createField('list', 'Markers', [], {
          itemFields: {
            position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
            health: createField('number', 'Health', 1, { ui: { min: 1, max: 9999 } })
          },
          ui: { layout: 'row', placeholder: 'Add a marker' }
        })
      }
    },

    universal_door_control: {
      name: 'Universal: door control',
      class: 'interactive',
      icon: 'fa-solid fa-door-open',
      desc: 'Personal door control in a universal scenario',
      fields: {
        action: createField('text', 'Action', 'create', {
          ui: { type: 'select', options: ['create', 'remove'] }
        }),
        position: createField('vec2', 'Position', { x: 100, y: 100 }, { 
          ui: { min: -99999, max: 99999, step: 0.1, showWhen: { path: 'action', equals: 'create' } } 
        }),
        key: createField('text', 'Key', '', { ui: { placeholder: 'Enter the door key' } })
      }
    },
    universal_condition_item: {
      name: 'Universal: item check',
      class: 'condition',
      icon: 'fa-solid fa-box-open',
      desc: 'Item presence check',
      fields: {
        item_id: createDbSelect('Item', 0, 'item', { ui: { placeholder: '— select an item —' } }),
        required: createField('number', 'Quantity', 1, { ui: { min: 1, max: 9999 } }),
        remove: createField('boolean', 'Remove after check', false),
        show_progress: createField('boolean', 'Show progress', false)
      }
    },
    quest_action: {
      name: 'Quest action',
      class: 'quest',
      icon: 'fa-solid fa-clipboard-check',
      desc: 'Accept or reset a quest for scenario players',
      fields: {
        quest_id: createDbSelect('Quest', 0, 'quest', { ui: { placeholder: '— select a quest —' } }),
        action: createField('text', 'Action', 'accept', {
          ui: { type: 'select', options: ['accept', 'reset'] }
        })
      }
    },
    quest_condition: {
      name: 'Quest condition',
      class: 'condition',
      icon: 'fa-solid fa-list-check',
      desc: 'Check quest state for scenario players',
      fields: {
        quest_id: createDbSelect('Quest', 0, 'quest', { ui: { placeholder: '— select a quest —' } }),
        condition: createField('text', 'Condition', 'accepted', {
          ui: { type: 'select', options: ['accepted', 'finished', 'step_finished'] }
        }),
        step: createField('number', 'Step number', 0, { 
          ui: { min: 0, max: 9999, showWhen: { path: 'condition', equals: 'step_finished' } } 
        }),
        entire_group: createField('boolean', 'Require for the whole group', false)
      }
    },
    universal_pick_item_task: {
      name: 'Universal: collect objective',
      class: 'interactive',
      icon: 'fa-solid fa-hand',
      desc: 'Collect-item objective',
      fields: {
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        item: createField('item', 'Item', { id: 0, value: 0 }, { ui: { datasource: 'items', placeholder: '— select an item —', min: 0, max: 999999 } }),
        chat: createField('text', 'Chat', '', { ui: { placeholder: 'Enter chat text', format: 'textarea' } }),
        broadcast: createField('text', 'Broadcast to everyone', '', { ui: { placeholder: 'Enter the announcement text', format: 'textarea' } })
      }
    },
    universal_shootmarkers: {
      name: 'Universal: shooting markers',
      class: 'combat',
      icon: 'fa-solid fa-bullseye',
      desc: 'Destroy markers by shooting',
      fields: {
        markers: createField('list', 'Markers', [], {
          itemFields: {
            position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
            health: createField('number', 'Health', 1, { ui: { min: 1, max: 9999 } })
          },
          ui: { layout: 'row', placeholder: 'Add a marker' }
        })
      }
    },
    defeat_mobs: {
      name: 'Kill mobs',
      class: 'combat',
      icon: 'fa-solid fa-skull-crossbones',
      desc: 'Combat trial',
      fields: {
        mode: createField('text', 'Mode', 'annihilation', {
          ui: { type: 'select', options: ['annihilation', 'wave', 'survival'] }
        }),
        radius: createField('number', 'Radius', 800, { ui: { min: 0, max: 99999 }, help: 'Spawn area radius around the position' }),
        activeradius: createField('number', 'Active radius (0 — default)', 0, { ui: { min: 0, max: 999999 }, help: 'Field of view radius in which mobs become active' }),
        wanderradius: createField('number', 'Wander radius (0 — off)', 0, { ui: { min: 0, max: 999999 }, help: 'Max distance from the spawn point; mobs are teleported back when crossed' }),
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1, safeTiles: false, highlightTiles: [209], radiusPath: 'wanderradius', activeRadiusPath: 'activeradius' } }),
        kill_target: createField('number', 'Kill goal', 10, { 
          ui: { min: 1, max: 9999, showWhen: { path: 'mode', equals: 'wave' } } 
        }),
        duration: createField('number', 'Duration', 60, { 
          ui: { min: 1, max: 99999, showWhen: { path: 'mode', equals: 'survival' } } 
        }),
        mobs: createField('list', 'Mobs', [{ bot_id: 0, level: 1, power: 1, count: 5, boss: false, Behavior: [], Debuffs: [], drops: [] }], {
          // Поля сами укладываются в строки: число узкое, выбор моба шире,
          // теги и дропы — на всю строку. Форма моба вдвое короче.
          ui: { layout: 'compact', addLabel: 'Add a mob' },
          itemFields: {
            bot_id: createDbSelect('Mob', 0, 'mob', { ui: { placeholder: '— select a mob —' } }),
            level: createField('number', 'Level', 1, { ui: { min: 1, max: 999 } }),
            power: createField('number', 'Power', 1, { ui: { min: 1, max: 99999 } }),
            count: createField('number', 'Quantity', 5, { ui: { min: 1, max: 999 } }),
            boss: createField('boolean', 'Boss', false),
            Behavior: {
              label: 'Behavior',
              ui: { type: 'tags', options: ['sleepy', 'slower', 'poisonous', 'neutral', 'skills_base', 'skills_tank', 'skills_dps', 'skills_healer', 'ambient_chat'], placeholder: 'Add behaviour…' }
            },
            Debuffs: {
              label: 'Debuffs',
              // Дебаффов мало — помещаются в половину строки.
              ui: { type: 'tags', options: ['Slowdown', 'Poison', 'Fire'], placeholder: 'Add a debuff…', cellSize: 'mid' }
            },
            drops: createField('list', 'Drop slots (max 5)', [], {
              // Слот дропа — одна строка: предмет, количество, шанс.
              ui: { layout: 'row', addLabel: 'Add a slot' },
              itemDefault: { item_id: 0, count: 1, chance: 0 },
              itemFields: {
                item_id: createDbSelect('Item', 0, 'item', { ui: { placeholder: '— item —', searchServer: true, dbLimit: 1000 } }),
                count: createField('number', 'Quantity', 1, { ui: { min: 1, max: 9999 } }),
                chance: createField('number', 'Chance (%)', 0, { ui: { min: 0, max: 100, step: 0.1 } }),
              }
            })
          }
        })
      }
    },
    follow_camera: {
      name: 'Camera follow',
      class: 'camera',
      icon: 'fa-solid fa-video',
      desc: 'Move the camera to a point',
      fields: {
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        execution_time: createField('number', 'Run time (ms)', 300, { ui: { min: 0, max: 600000 } }),
        smooth: createField('boolean', 'Smooth', true)
      }
    },

    dynamic_condition: {
      name: 'Universal condition',
      class: 'condition',
      icon: 'fa-solid fa-sliders',
      desc: 'Dynamic condition: players, items, quests, world. Works in single/group/world scenarios',
      fields: {
        condition_type: createField('text', 'Which condition', 'players_count', {
          ui: {
            type: 'select',
            options: [
              { value: 'players_count', label: 'Scenario participants' },
              { value: 'alive_players_count', label: 'Living participants' },
              { value: 'item_count', label: 'Item count' },
              { value: 'quest_state', label: 'Quest state' }
            ],
            help: 'After choosing a condition the form rebuilds and shows only the relevant fields.'
          }
        }),
        compare: createField('text', 'Comparison', '>=', {
          ui: {
            type: 'select',
            options: ['>=', '==', '!=', '>', '<', '<='],
            showWhen: { path: 'condition_type', in: ['players_count', 'alive_players_count', 'item_count'] }
          }
        }),
        value: createField('number', 'Value', 1, {
          ui: {
            min: -999999,
            max: 999999,
            showWhen: { path: 'condition_type', in: ['players_count', 'alive_players_count', 'item_count'] }
          }
        }),
        item_id: createDbSelect('Item', 0, 'item', {
          ui: {
            placeholder: '— select an item —',
            searchServer: true,
            dbLimit: 1000,
            showWhen: { path: 'condition_type', equals: 'item_count' }
          }
        }),
        item_scope: createField('text', 'How to count items', 'any_player', {
          ui: {
            type: 'select',
            options: [
              { value: 'any_player', label: 'Enough for any player' },
              { value: 'all_players', label: 'Required for every player' },
              { value: 'total', label: 'Sum over all participants' }
            ],
            showWhen: { path: 'condition_type', equals: 'item_count' }
          }
        }),
        quest_id: createDbSelect('Quest', 0, 'quest', {
          ui: {
            placeholder: '— select a quest —',
            showWhen: { path: 'condition_type', equals: 'quest_state' }
          }
        }),
        quest_state: createField('text', 'Quest state', 'accepted', {
          ui: {
            type: 'select',
            options: [
              { value: 'accepted', label: 'Quest accepted' },
              { value: 'finished', label: 'Quest completed' },
              { value: 'step_finished', label: 'Quest step completed' }
            ],
            showWhen: { path: 'condition_type', equals: 'quest_state' }
          }
        }),
        step: createField('number', 'Quest step number', 0, {
          ui: { min: 0, max: 9999, showWhen: (data) => data.condition_type === 'quest_state' && data.quest_state === 'step_finished' }
        }),
        entire_group: createField('boolean', 'Require for the whole group', false, {
          ui: { showWhen: { path: 'condition_type', equals: 'quest_state' } }
        }),
        invert: createField('boolean', 'Invert the result', false),
        check_once: createField('boolean', 'Decide immediately', false, { ui: { help: 'Check once and take the failure path immediately if false; otherwise wait until success or timeout.' } }),
        timeout: createField('number', 'Timeout (seconds; 0 = wait)', 0, { ui: { min: 0, max: 999999, help: 'Ignored when Decide immediately is enabled.' } }),
        pass_step_id: createField('text', 'Step on success', ''),
        fail_step_id: createField('text', 'Step on failure or timeout', ''),
        show_progress: createField('boolean', 'Show a reminder to players', true)
      },
      linkTargets: ['pass_step_id', 'fail_step_id']
    },
    condition_movement: {
      name: 'Condition: Movement',
      class: 'interactive',
      icon: 'fa-solid fa-person-running',
      desc: 'Transition on reaching the point',
      fields: {
        position: createField('vec2', 'Position', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        entire_group: createField('boolean', 'Whole group', true)
      }
    },
    teleport: {
      name: 'Teleport',
      class: 'interactive',
      icon: 'fa-solid fa-bolt',
      desc: 'Instant movement',
      fields: {
        position: createField('vec2', 'Position', { x: 1497, y: 529 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
	moving_disable: {
      name: 'Disable movement',
      class: 'interactive',
      icon: 'fa-solid fa-person-walking-dashed-line-arrow-right',
      desc: 'Enable or disable movement',
      fields: {
        state: createField('boolean', 'Movement disabled', true)
      }
    },
    emote: {
      name: 'Emotion',
      class: 'interactive',
      icon: 'fa-solid fa-face-smile',
      desc: 'Show an emote and icon',
      fields: {
        execution_time: createField('number', 'Run time (ms)', 0, { ui: { min: 0, max: 600000 } }),
        emote_type: createField('number', 'Emote type', 0, { ui: { min: 0, max: 6 } }),
        emoticon_type: createField('number', 'Icon type', -1, { ui: { min: -1, max: 999 } })
      }
    },
    dungeon_activate_point: {
      name: 'Dungeon: activation point',
      class: 'interactive',
      icon: 'fa-solid fa-location-dot',
      desc: 'Activation point of a dungeon scenario',
      fields: {
        position: createField('vec2', 'Position', { x: 400, y: 3000 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        duration: createField('number', 'Duration', 5, { ui: { min: 1, max: 9999 } }),
        entire_group: createField('boolean', 'Whole group', true),
        action_text: createField('text', 'Action text', 'Main reactor overload', { ui: { placeholder: 'Action description', format: 'textarea' } })
      }
    },
    dungeon_complete: {
      name: 'Dungeon: finish the dungeon',
      class: 'interactive',
      icon: 'fa-solid fa-flag-checkered',
      desc: 'Force-finish a dungeon scenario',
      fields: {}
    }
  };



  const defaultScenarioComponentTypes = [
    'branch_random',
    'use_chat_code',
    'message',
    'wait',
    'follow_camera',
    'dynamic_condition',
    'condition_movement',
    'teleport',
    'moving_disable',
    'emote',
    'quest_action',
    'quest_condition',
    'defeat_mobs',
    'restore_resources'
  ];

  const scenarioModeSchemas = {
    default: {
      label: 'Default (components/default.h)',
      description: 'Base components from default.h',
      componentTypes: [...defaultScenarioComponentTypes]
    },
    dungeon: {
      label: 'Dungeon (scenario_dungeon.h)',
      description: 'Components of a group dungeon scenario',
      componentTypes: [
        ...defaultScenarioComponentTypes,
        'set_group_flags',
        'set_group_lives',
        'set_group_spawn',
        'dungeon_door_control',
        'dungeon_activate_point',
        'dungeon_complete'
      ]
    },
    universal: {
      label: 'Universal (scenario_universal.h)',
      description: 'Components and legacy adapters of a universal scenario',
      componentTypes: [
        ...defaultScenarioComponentTypes,
        'universal_door_control',
        'universal_condition_item',
        'universal_pick_item_task',
        'universal_shootmarkers'
      ]
    },
    world: {
      label: 'World (scenario_world.h)',
      description: 'A world scenario for every player in the current world',
      componentTypes: [
        ...defaultScenarioComponentTypes,
        'set_group_flags',
        'set_group_lives',
        'set_group_spawn',
        'world_complete'
      ]
    }
  };

  const register = (id, app) => {
    if (!id) {
      throw new Error('Editor registry requires an id.');
    }
    registry.set(id, app);
    return app;
  };

  const get = (id) => registry.get(id);
  const list = () => Array.from(registry.keys());
  const cloneValue = (value) => {
    if (value && typeof value === 'object') {
      return JSON.parse(JSON.stringify(value));
    }
    return value;
  };
  const applyDefaults = (target, fields = {}) => {
    Object.entries(fields).forEach(([key, field]) => {
      if (target[key] === undefined) {
        target[key] = cloneValue(field.default);
      } else if (field.type === 'vec2' && typeof target[key] === 'object' && target[key] !== null) {
        target[key].x = target[key].x ?? field.default?.x ?? 0;
        target[key].y = target[key].y ?? field.default?.y ?? 0;
      } else if (field.type === 'item' && typeof target[key] === 'object' && target[key] !== null) {
        target[key].id = target[key].id ?? field.default?.id ?? 0;
        target[key].value = target[key].value ?? field.default?.value ?? 0;
      } else if (field.type === 'list' && Array.isArray(target[key]) && field.itemFields) {
        target[key] = target[key].map((item) => applyDefaults(item, field.itemFields));
      }
    });
    return target;
  };
  const buildDefaults = (fields = {}) => applyDefaults({}, fields);

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.registry = { register, get, list };
  window.EditorCore.schemas = {
    actionSchemas,
    componentSchemas,
    scenarioModeSchemas,
    applyDefaults,
    buildDefaults
  };
})();
