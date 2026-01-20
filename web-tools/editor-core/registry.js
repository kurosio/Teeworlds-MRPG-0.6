(() => {
  const registry = new Map();
  const createField = (type, label, defaultValue, { validate = null, ui = null } = {}) => ({
    type,
    label,
    default: defaultValue,
    validate,
    ui
  });

  const actionSchemas = {
    group_header: {
      name: 'Заголовок группы',
      icon: 'fa-layer-group',
      fields: {
        name: createField('text', 'Название', 'Новая группа')
      }
    },
    message: {
      name: 'Сообщение',
      icon: 'fa-comment',
      fields: {
        delay: createField('number', 'Задержка', 0),
        chat: createField('text', 'Чат', ''),
        broadcast: createField('text', 'Броадкаст', ''),
        full: createField('text', 'Полное сообщение', '')
      }
    },
    movement_task: {
      name: 'Задача на движение',
      icon: 'fa-person-running',
      fields: {
        delay: createField('number', 'Задержка', 0),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }),
        target_lock_text: createField('text', 'Текст фиксации цели', ''),
        target_look: createField('boolean', 'Смотреть на цель', false),
        chat: createField('text', 'Чат', ''),
        broadcast: createField('text', 'Броадкаст', ''),
        full: createField('text', 'Полное сообщение', '')
      }
    },
    check_has_item: {
      name: 'Проверить предмет',
      icon: 'fa-box',
      fields: {
        item_id: createField('number', 'ID предмета', 0),
        required: createField('number', 'Количество', 1),
        remove: createField('boolean', 'Удалить после проверки', false),
        show_progress: createField('boolean', 'Показать прогресс', false)
      }
    },
    reset_quest: {
      name: 'Сбросить квест',
      icon: 'fa-undo',
      fields: {
        quest_id: createField('number', 'ID квеста', 0)
      }
    },
    accept_quest: {
      name: 'Принять квест',
      icon: 'fa-check-double',
      fields: {
        quest_id: createField('number', 'ID квеста', 0)
      }
    },
    new_door: {
      name: 'Создать дверь',
      icon: 'fa-door-closed',
      fields: {
        key: createField('door_key', 'Ключ', ''),
        follow: createField('boolean', 'Следовать за игроком', false),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 })
      }
    },
    remove_door: {
      name: 'Удалить дверь',
      icon: 'fa-door-open',
      fields: {
        key: createField('door_key', 'Ключ', ''),
        follow: createField('boolean', 'Следовать за игроком', false)
      }
    },
    pick_item_task: {
      name: 'Задача на подбор',
      icon: 'fa-hand-sparkles',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }),
        item: createField('item', 'Предмет', { id: 0, value: 0 }),
        chat: createField('text', 'Чат', ''),
        broadcast: createField('text', 'Броадкаст', ''),
        full: createField('text', 'Полное сообщение', '')
      }
    },
    emote: {
      name: 'Эмоция',
      icon: 'fa-smile',
      fields: {
        emote_type: createField('number', 'Тип эмоции', 0),
        emoticon_type: createField('number', 'Тип эмоции (иконка)', 0)
      }
    },
    teleport: {
      name: 'Телепорт',
      icon: 'fa-plane-departure',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }),
        world_id: createField('number', 'ID мира', 0)
      }
    },
    use_chat_task: {
      name: 'Задача на чат',
      icon: 'fa-keyboard',
      fields: {
        chat: createField('text', 'Чат', '')
      }
    },
    fix_cam: {
      name: 'Фиксировать камеру',
      icon: 'fa-camera',
      fields: {
        delay: createField('number', 'Задержка', 0),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 })
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
        quest_id: createField('number', 'ID квеста', 0)
      }
    },
    check_quest_finished: {
      name: 'Проверка: квест завершен',
      icon: 'fa-flag-checkered',
      fields: {
        quest_id: createField('number', 'ID квеста', 0)
      }
    },
    check_quest_step_finished: {
      name: 'Проверка: шаг квеста завершен',
      icon: 'fa-list-check',
      fields: {
        quest_id: createField('number', 'ID квеста', 0),
        step: createField('number', 'Номер шага', 0)
      }
    },
    shootmarkers: {
      name: 'Стрельба по маркерам',
      icon: 'fa-crosshairs',
      fields: {
        markers: createField('markers', 'Маркеры', [])
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
        text: createField('text', 'Текст', 'Новое сообщение'),
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
        duration: createField('number', 'Длительность', 5)
      }
    },
    door_control: {
      name: 'Управление дверью',
      class: 'interactive',
      icon: 'fa-solid fa-door-open',
      desc: 'Создать или удалить дверь',
      fields: {
        action: createField('text', 'Действие', 'create', {
          ui: { type: 'select', options: ['create', 'remove'] }
        }),
        position: createField('vec2', 'Позиция', { x: 100, y: 100 }),
        key: createField('text', 'Ключ', '')
      }
    },
    use_chat_code: {
      name: 'Код в чате',
      class: 'interactive',
      icon: 'fa-solid fa-key',
      desc: 'Переход по кодовому слову',
      fields: {
        code: createField('text', 'Код', 'secret'),
        next_step_id: createField('text', 'Следующий шаг', ''),
        hidden: createField('boolean', 'Скрыть', false)
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
        radius: createField('number', 'Радиус', 100),
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }),
        mobs: createField('list', 'Мобы', [{ mob_id: 21, count: 5, level: 1, power: 1, boss: false }])
      }
    },
    follow_camera: {
      name: 'Следование камеры',
      class: 'camera',
      icon: 'fa-solid fa-video',
      desc: 'Переместить камеру к точке',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }),
        delay: createField('number', 'Задержка', 300),
        smooth: createField('boolean', 'Плавно', true)
      }
    },
    condition_movement: {
      name: 'Условие: Движение',
      class: 'interactive',
      icon: 'fa-solid fa-person-running',
      desc: 'Переход при достижении точки',
      fields: {
        position: createField('vec2', 'Позиция', { x: 0, y: 0 }),
        entire_group: createField('boolean', 'Вся группа', true)
      }
    },
    teleport: {
      name: 'Телепорт',
      class: 'interactive',
      icon: 'fa-solid fa-bolt',
      desc: 'Мгновенное перемещение',
      fields: {
        position: createField('vec2', 'Позиция', { x: 1497, y: 529 })
      }
    },
    activate_point: {
      name: 'Точка активации',
      class: 'interactive',
      icon: 'fa-solid fa-location-dot',
      desc: 'Активация по времени в области',
      fields: {
        position: createField('vec2', 'Позиция', { x: 400, y: 3000 }),
        duration: createField('number', 'Длительность', 5),
        entire_group: createField('boolean', 'Вся группа', true),
        action_text: createField('text', 'Текст действия', 'Перегрузка главного реактора')
      }
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
    applyDefaults,
    buildDefaults
  };
})();
