(() => {
  const registry = new Map();
  const createField = (type, label, defaultValue, { validate = null, ui = null, ...rest } = {}) => ({
    type,
    label,
    default: defaultValue,
    validate,
    ui,
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
      name: 'Заголовок группы',
      icon: 'fa-layer-group',
      fields: {
        name: createField('text', 'Название', 'Новая группа', {
          ui: { placeholder: 'Введите название группы' }
        })
      }
    },
    message: {
      name: 'Сообщение',
      icon: 'fa-comment',
      fields: {
        delay: createField('number', 'Задержка', 0, { ui: { min: 0, max: 600000 } }),
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } }),
        broadcast: createField('text', 'Броадкаст', '', { ui: { placeholder: 'Введите текст для броадкаста', format: 'textarea' } }),
        full: createField('text', 'Полное сообщение', '', { ui: { placeholder: 'Введите полное сообщение', format: 'textarea' } })
      }
    },
    movement_task: {
      name: 'Задача на движение',
      icon: 'fa-person-running',
      fields: {
        delay: createField('number', 'Задержка', 0, { ui: { min: 0, max: 600000 } }),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        target_lock_text: createField('text', 'Текст фиксации цели', '', { ui: { placeholder: 'Например: Следуйте за маркером' } }),
        target_look: createField('boolean', 'Смотреть на цель', false),
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } }),
        broadcast: createField('text', 'Броадкаст', '', { ui: { placeholder: 'Введите текст для броадкаста', format: 'textarea' } }),
        full: createField('text', 'Полное сообщение', '', { ui: { placeholder: 'Введите полное сообщение', format: 'textarea' } })
      }
    },
    check_has_item: {
      name: 'Проверить предмет',
      icon: 'fa-box',
      fields: {
        item_id: createDbSelect('Предмет', 0, 'item', { ui: { placeholder: '— выберите предмет —' } }),
        required: createField('number', 'Количество', 1, { ui: { min: 1, max: 9999 } }),
        remove: createField('boolean', 'Удалить после проверки', false),
        show_progress: createField('boolean', 'Показать прогресс', false)
      }
    },
    reset_quest: {
      name: 'Сбросить квест',
      icon: 'fa-undo',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } })
      }
    },
    accept_quest: {
      name: 'Принять квест',
      icon: 'fa-check-double',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } })
      }
    },
    new_door: {
      name: 'Создать дверь',
      icon: 'fa-door-closed',
      fields: {
        key: createField('door_key', 'Ключ', '', { ui: { placeholder: 'Введите или выберите ключ' } }),
        follow: createField('boolean', 'Следовать за игроком', false),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
    remove_door: {
      name: 'Удалить дверь',
      icon: 'fa-door-open',
      fields: {
        key: createField('door_key', 'Ключ', '', { ui: { placeholder: 'Введите или выберите ключ' } }),
        follow: createField('boolean', 'Следовать за игроком', false)
      }
    },
    pick_item_task: {
      name: 'Задача на подбор',
      icon: 'fa-hand-sparkles',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        item: createField('item', 'Предмет', { id: 0, value: 0 }, { ui: { datasource: 'items', placeholder: '— выберите предмет —', min: 0, max: 999999 } }),
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } }),
        broadcast: createField('text', 'Броадкаст', '', { ui: { placeholder: 'Введите текст для броадкаста', format: 'textarea' } }),
        full: createField('text', 'Полное сообщение', '', { ui: { placeholder: 'Введите полное сообщение', format: 'textarea' } })
      }
    },
    emote: {
      name: 'Эмоция',
      icon: 'fa-smile',
      fields: {
        emote_type: createField('number', 'Тип эмоции', 0, { ui: { min: 0, max: 999 } }),
        emoticon_type: createField('number', 'Тип эмоции (иконка)', 0, { ui: { min: 0, max: 999 } })
      }
    },
    teleport: {
      name: 'Телепорт',
      icon: 'fa-plane-departure',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        world_id: createDbSelect('Мир', 0, 'world', { ui: { placeholder: '— выберите мир —' } })
      }
    },
    use_chat_task: {
      name: 'Задача на чат',
      icon: 'fa-keyboard',
      fields: {
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } })
      }
    },
    fix_cam: {
      name: 'Фиксировать камеру',
      icon: 'fa-camera',
      fields: {
        delay: createField('number', 'Задержка', 0, { ui: { min: 0, max: 600000 } }),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
    freeze_movements: {
      name: 'Заморозить движение',
      icon: 'fa-snowflake',
      fields: {
        state: createField('boolean', 'Включить', false)
      }
    },
    check_quest_accepted: {
      name: 'Проверка: квест принят',
      icon: 'fa-question-circle',
      fields: {
        quest_id: createField('db_select', 'Квест', 0, { datasource: 'quests', ui: { placeholder: '— выберите квест —' } })
      }
    },
    check_quest_finished: {
      name: 'Проверка: квест завершен',
      icon: 'fa-flag-checkered',
      fields: {
        quest_id: createField('db_select', 'Квест', 0, { datasource: 'quests', ui: { placeholder: '— выберите квест —' } })
      }
    },
    check_quest_step_finished: {
      name: 'Проверка: шаг квеста завершен',
      icon: 'fa-list-check',
      fields: {
        quest_id: createField('db_select', 'Квест', 0, { datasource: 'quests', ui: { placeholder: '— выберите квест —' } }),
        step: createField('number', 'Номер шага', 0, { ui: { min: 0, max: 9999 } }),
        entire_group: createField('boolean', 'Требовать для всей группы', false)
      }
    },
    shootmarkers: {
      name: 'Стрельба по маркерам',
      icon: 'fa-crosshairs',
      fields: {
        markers: createField('list', 'Маркеры', [], {
          itemFields: {
            position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
            health: createField('number', 'Здоровье', 1, { ui: { min: 1, max: 9999 } })
          },
          ui: { placeholder: 'Добавьте маркер' }
        })
      }
    }
  };

  const componentSchemas = {
    message: {
      name: 'Сообщение',
      class: 'info',
      icon: 'fa-solid fa-comment-dots',
      desc: 'Показать текст игроку',
      fields: {
        execution_time: createField('number', 'Время выполнения (мс)', 0, { ui: { min: 0, max: 600000 } }),
        text: createField('text', 'Текст', 'Новое сообщение', { ui: { placeholder: 'Введите текст сообщения', format: 'textarea' } }),
        mode: createField('text', 'Режим', 'full', {
          ui: { type: 'select', options: ['full', 'broadcast', 'chat'] }
        })
      }
    },
    wait: {
      name: 'Ожидание',
      class: 'info',
      icon: 'fa-solid fa-clock',
      desc: 'Пауза в сценарии',
      fields: {
        duration: createField('number', 'Длительность', 5, { ui: { min: 1, max: 9999 } })
      }
    },
    dungeon_door_control: {
      name: 'Dungeon: управление дверью',
      class: 'interactive',
      icon: 'fa-solid fa-door-open',
      desc: 'Управление дверью dungeon-сценария',
      fields: {
        action: createField('text', 'Действие', 'create', {
          ui: { type: 'select', options: ['create', 'remove'] }
        }),
        position: createField('vec2', 'Позиция', { x: 100, y: 100 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        key: createField('text', 'Ключ', '', { ui: { placeholder: 'Введите ключ двери' } })
      }
    },
    dungeon_use_chat_code: {
      name: 'Dungeon: код в чате',
      class: 'interactive',
      icon: 'fa-solid fa-key',
      desc: 'Условие чата dungeon-сценария',
      fields: {
        code: createField('text', 'Код', 'secret', { ui: { placeholder: 'Введите кодовое слово' } }),
        next_step_id: createField('text', 'Следующий шаг', ''),
        hidden: createField('boolean', 'Скрыть', false)
      }
    },
    check_has_item: {
      name: 'Проверить предмет',
      class: 'condition',
      icon: 'fa-solid fa-box-open',
      desc: 'Проверка наличия предмета в инвентаре',
      fields: {
        item_id: createDbSelect('Предмет', 0, 'item', { ui: { placeholder: '— выберите предмет —' } }),
        required: createField('number', 'Количество', 1, { ui: { min: 1, max: 9999 } }),
        remove: createField('boolean', 'Удалить после проверки', false),
        show_progress: createField('boolean', 'Показать прогресс', false)
      }
    },
    reset_quest: {
      name: 'Сбросить квест',
      class: 'quest',
      icon: 'fa-solid fa-rotate-left',
      desc: 'Сброс состояния квеста',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } })
      }
    },
    accept_quest: {
      name: 'Принять квест',
      class: 'quest',
      icon: 'fa-solid fa-clipboard-check',
      desc: 'Выдать игроку квест',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } })
      }
    },
    check_quest_accepted: {
      name: 'Квест принят',
      class: 'condition',
      icon: 'fa-solid fa-circle-check',
      desc: 'Проверка принятия квеста',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } })
      }
    },
    check_quest_finished: {
      name: 'Квест завершён',
      class: 'condition',
      icon: 'fa-solid fa-flag-checkered',
      desc: 'Проверка завершения квеста',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } })
      }
    },
    check_quest_step_finished: {
      name: 'Шаг квеста завершён',
      class: 'condition',
      icon: 'fa-solid fa-list-check',
      desc: 'Проверка прогресса шага квеста',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } }),
        step: createField('number', 'Номер шага', 0, { ui: { min: 0, max: 9999 } }),
        entire_group: createField('boolean', 'Требовать для всей группы', false)
      }
    },
    pick_item_task: {
      name: 'Задача подобрать предмет',
      class: 'interactive',
      icon: 'fa-solid fa-hand',
      desc: 'Создать задачу на подбор предмета',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        item: createField('item', 'Предмет', { id: 0, value: 0 }, { ui: { datasource: 'items', placeholder: '— выберите предмет —', min: 0, max: 999999 } }),
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } }),
        broadcast: createField('text', 'Броадкаст', '', { ui: { placeholder: 'Введите текст для броадкаста', format: 'textarea' } })
      }
    },
    shootmarkers: {
      name: 'Маркеры стрельбы',
      class: 'combat',
      icon: 'fa-solid fa-bullseye',
      desc: 'Уничтожение маркеров выстрелами',
      fields: {
        markers: createField('list', 'Маркеры', [], {
          itemFields: {
            position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
            health: createField('number', 'Здоровье', 1, { ui: { min: 1, max: 9999 } })
          },
          ui: { placeholder: 'Добавьте маркер' }
        })
      }
    },

    universal_door_control: {
      name: 'Universal: управление дверью',
      class: 'interactive',
      icon: 'fa-solid fa-door-open',
      desc: 'Управление персональной дверью в universal сценарии',
      fields: {
        action: createField('text', 'Действие', 'create', {
          ui: { type: 'select', options: ['create', 'remove'] }
        }),
        position: createField('vec2', 'Позиция', { x: 100, y: 100 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        key: createField('text', 'Ключ', '', { ui: { placeholder: 'Введите ключ двери' } })
      }
    },
    universal_use_chat: {
      name: 'Universal: код в чате',
      class: 'interactive',
      icon: 'fa-solid fa-key',
      desc: 'Ожидание команды в чате',
      fields: {
        chat: createField('text', 'Текст в чате', '/rules', { ui: { placeholder: 'Введите ожидаемую команду' } })
      }
    },
    universal_condition_item: {
      name: 'Universal: проверка предмета',
      class: 'condition',
      icon: 'fa-solid fa-box-open',
      desc: 'Проверка наличия предмета',
      fields: {
        item_id: createDbSelect('Предмет', 0, 'item', { ui: { placeholder: '— выберите предмет —' } }),
        required: createField('number', 'Количество', 1, { ui: { min: 1, max: 9999 } }),
        remove: createField('boolean', 'Удалить после проверки', false),
        show_progress: createField('boolean', 'Показать прогресс', false)
      }
    },
    quest_action: {
      name: 'Действие квеста',
      class: 'quest',
      icon: 'fa-solid fa-clipboard-check',
      desc: 'Принятие или сброс квеста для участников сценария',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } }),
        action: createField('text', 'Действие', 'accept', {
          ui: { type: 'select', options: ['accept', 'reset'] }
        })
      }
    },
    quest_condition: {
      name: 'Условие квеста',
      class: 'condition',
      icon: 'fa-solid fa-list-check',
      desc: 'Проверка состояния квеста для участников сценария',
      fields: {
        quest_id: createDbSelect('Квест', 0, 'quest', { ui: { placeholder: '— выберите квест —' } }),
        condition: createField('text', 'Условие', 'accepted', {
          ui: { type: 'select', options: ['accepted', 'finished', 'step_finished'] }
        }),
        step: createField('number', 'Номер шага', 0, { ui: { min: 0, max: 9999 } }),
        entire_group: createField('boolean', 'Требовать для всей группы', false)
      }
    },
    universal_teleport: {
      name: 'Universal: телепорт',
      class: 'interactive',
      icon: 'fa-solid fa-bolt',
      desc: 'Телепорт/смена мира',
      fields: {
        position: createField('vec2', 'Позиция', { x: 1497, y: 529 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        world_id: createField('number', 'ID мира', -1, { ui: { min: -1, max: 999999 } })
      }
    },
    universal_pick_item_task: {
      name: 'Universal: задача подбора',
      class: 'interactive',
      icon: 'fa-solid fa-hand',
      desc: 'Задача подобрать предмет',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        item: createField('item', 'Предмет', { id: 0, value: 0 }, { ui: { datasource: 'items', placeholder: '— выберите предмет —', min: 0, max: 999999 } }),
        chat: createField('text', 'Чат', '', { ui: { placeholder: 'Введите текст для чата', format: 'textarea' } }),
        broadcast: createField('text', 'Броадкаст', '', { ui: { placeholder: 'Введите текст для броадкаста', format: 'textarea' } })
      }
    },
    universal_shootmarkers: {
      name: 'Universal: маркеры стрельбы',
      class: 'combat',
      icon: 'fa-solid fa-bullseye',
      desc: 'Уничтожение маркеров выстрелами',
      fields: {
        markers: createField('list', 'Маркеры', [], {
          itemFields: {
            position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
            health: createField('number', 'Здоровье', 1, { ui: { min: 1, max: 9999 } })
          },
          ui: { placeholder: 'Добавьте маркер' }
        })
      }
    },
    defeat_mobs: {
      name: 'Убийство мобов',
      class: 'combat',
      icon: 'fa-solid fa-skull-crossbones',
      desc: 'Боевое испытание',
      fields: {
        mode: createField('text', 'Режим', 'annihilation', {
          ui: { type: 'select', options: ['annihilation', 'wave', 'survival'] }
        }),
        radius: createField('number', 'Радиус', 100, { ui: { min: 0, max: 99999 } }),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        kill_target: createField('number', 'Цель убийств', 10, { ui: { min: 1, max: 9999 } }),
        duration: createField('number', 'Длительность', 60, { ui: { min: 1, max: 99999 } }),
        mobs: createField('list', 'Мобы', [{ bot_id: 0, level: 1, power: 1, count: 5, boss: false, Behavior: [], Debuffs: [], drops: [] }], {
          ui: { layout: 'stack', addLabel: 'Добавить моба' },
          itemFields: {
            bot_id: createDbSelect('BotID', 0, 'mob', { ui: { placeholder: '— выберите моба —' } }),
            level: createField('number', 'Уровень', 1, { ui: { min: 1, max: 999 } }),
            power: createField('number', 'Сила', 1, { ui: { min: 1, max: 999 } }),
            count: createField('number', 'Количество', 5, { ui: { min: 1, max: 999 } }),
            boss: createField('boolean', 'Босс', false),
            Behavior: {
              label: 'Behavior',
              ui: { type: 'tags', options: ['sleepy', 'slower', 'poisonous', 'neutral', 'skills_base', 'skills_tank', 'skills_dps', 'skills_healer'], placeholder: 'Добавить behavior…' }
            },
            Debuffs: {
              label: 'Debuffs',
              ui: { type: 'tags', options: ['Slowdown', 'Poison', 'Fire'], placeholder: 'Добавить debuff…' }
            },
            drops: createField('list', 'Слоты дропа (макс 5)', [], {
              ui: { addLabel: 'Добавить слот' },
              itemDefault: { item_id: 0, count: 1, chance: 0 },
              itemFields: {
                item_id: createDbSelect('Предмет', 0, 'item', { ui: { placeholder: '— предмет —', searchServer: true, dbLimit: 1000 } }),
                count: createField('number', 'Кол-во', 1, { ui: { min: 1, max: 9999 } }),
                chance: createField('number', 'Шанс (%)', 0, { ui: { min: 0, max: 100, step: 0.1 } }),
              }
            })
          }
        })
      }
    },
    follow_camera: {
      name: 'Следование камеры',
      class: 'camera',
      icon: 'fa-solid fa-video',
      desc: 'Переместить камеру к точке',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        execution_time: createField('number', 'Время выполнения (мс)', 300, { ui: { min: 0, max: 600000 } }),
        smooth: createField('boolean', 'Плавно', true)
      }
    },
    condition_movement: {
      name: 'Условие: Движение',
      class: 'interactive',
      icon: 'fa-solid fa-person-running',
      desc: 'Переход при достижении точки',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        entire_group: createField('boolean', 'Вся группа', true)
      }
    },
    teleport: {
      name: 'Телепорт',
      class: 'interactive',
      icon: 'fa-solid fa-bolt',
      desc: 'Мгновенное перемещение',
      fields: {
        position: createField('vec2', 'Позиция', { x: 1497, y: 529 }, { ui: { min: -99999, max: 99999, step: 0.1 } })
      }
    },
	moving_disable: {
      name: 'Отключение движения',
      class: 'interactive',
      icon: 'fa-solid fa-person-walking-dashed-line-arrow-right',
      desc: 'Включить или выключить движение',
      fields: {
        state: createField('boolean', 'Отключить', true)
      }
    },
    emote: {
      name: 'Эмоция',
      class: 'interactive',
      icon: 'fa-solid fa-face-smile',
      desc: 'Показать эмоцию и иконку',
      fields: {
        execution_time: createField('number', 'Время выполнения (мс)', 0, { ui: { min: 0, max: 600000 } }),
        emote_type: createField('number', 'Тип эмоции', 0, { ui: { min: 0, max: 6 } }),
        emoticon_type: createField('number', 'Тип иконки', -1, { ui: { min: -1, max: 999 } })
      }
    },
    dungeon_activate_point: {
      name: 'Dungeon: точка активации',
      class: 'interactive',
      icon: 'fa-solid fa-location-dot',
      desc: 'Точка активации dungeon-сценария',
      fields: {
        position: createField('vec2', 'Позиция', { x: 400, y: 3000 }, { ui: { min: -99999, max: 99999, step: 0.1 } }),
        duration: createField('number', 'Длительность', 5, { ui: { min: 1, max: 9999 } }),
        entire_group: createField('boolean', 'Вся группа', true),
        action_text: createField('text', 'Текст действия', 'Перегрузка главного реактора', { ui: { placeholder: 'Описание действия', format: 'textarea' } })
      }
    },
    dungeon_complete: {
      name: 'Dungeon: завершить темницу',
      class: 'interactive',
      icon: 'fa-solid fa-flag-checkered',
      desc: 'Принудительно завершить dungeon-сценарий',
      fields: {}
    }
  };



  const defaultScenarioComponentTypes = [
    'message',
    'wait',
    'follow_camera',
    'condition_movement',
    'teleport',
    'moving_disable',
    'emote',
    'quest_action',
    'quest_condition',
    'defeat_mobs'
  ];

  const scenarioModeSchemas = {
    default: {
      label: 'Default (components/default.h)',
      description: 'Базовые компоненты из default.h',
      componentTypes: [...defaultScenarioComponentTypes]
    },
    dungeon: {
      label: 'Dungeon (scenario_dungeon.h)',
      description: 'Компоненты группового dungeon-сценария',
      componentTypes: [
        ...defaultScenarioComponentTypes,
        'dungeon_door_control',
        'dungeon_use_chat_code',
        'dungeon_activate_point',
        'dungeon_complete'
      ]
    },
    universal: {
      label: 'Universal (scenario_universal.h)',
      description: 'Компоненты и legacy-адаптеры universal-сценария',
      componentTypes: [
        ...defaultScenarioComponentTypes,
        'universal_door_control',
        'universal_use_chat',
        'universal_condition_item',
        'universal_teleport',
        'universal_pick_item_task',
        'universal_shootmarkers'
      ]
    },
    world: {
      label: 'World (scenario_world.h)',
      description: 'Мировой сценарий для всех игроков в текущем мире',
      componentTypes: [...defaultScenarioComponentTypes]
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
