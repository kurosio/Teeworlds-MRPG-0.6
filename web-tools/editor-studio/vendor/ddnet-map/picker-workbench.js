/* Панель точек: редактирование игрового и switch-слоя карты.
 *
 * Раньше это была «установка тайла»: поставить можно было только в пустую
 * клетку, а удалять, двигать и править switch-слой (руду/растения/рыбу) —
 * нельзя. Теперь режим «Ставит тайл» — это редактор точек:
 *
 *   • видны все точки карты: игровой слой (индекс >= 192) и switch-слой
 *     (занятые клетки с номером и типом);
 *   • клик по занятой клетке её выбирает, по пустой — ставит новый тайл;
 *   • перетаскивание выбранной точки переносит её, Delete — удаляет;
 *   • switch-тайл ставится с номером 1–255 и типом 199/200/201 — так же, как
 *     узлы сбора в редакторе руды;
 *   • все правки живут в черновике (MapPickerTiles) и уходят на сервер одним
 *     пакетом по «Применить»: карта пишется атомарно, а повторная правка после
 *     успешного сохранения считается уже от записанного файла.
 *
 * Модуль знает про DOM панели, мышь/тач и карту, но не про HTTP: сохранение
 * делает picker.js, а он спрашивает у черновика `getPendingChanges()`.
 */
(() => {
  'use strict';

  const TILE = 32;

  window.MapPickerWorkbench = {
    create(api) {
      const { tw, state, screenToWorld, worldToScreen, screenSize, redraw, options = {} } = api;
      const pointer = (event) => (api.onPointer ? api.onPointer(event) : screenToWorld(event.clientX, event.clientY));
      const draw = window.MapPickerOverlays;
      const model = window.MapPickerTiles;
      const catalog = window.MapPickerTilesCatalog;
      const text = api.text;
      const get = (id) => document.getElementById(`pk-${id}`);
      // Тот же узел, с которым работают picker.js и picker-measure.js: события
      // карты сравниваются по идентичности элемента.
      const canvas = document.getElementById('cnvs');

      const NODE_IDS = [
        'collision', 'collision-label', 'spawn', 'collision-status',
        'layer-list', 'layers-reset', 'sec-layers',
        'sec-edit', 'sec-points', 'layer-group',
        'edit-layer-game', 'edit-layer-switch', 'edit-pane-game', 'edit-pane-switch',
        'edit-tile-wrap', 'edit-tile-label', 'edit-tile', 'edit-type-hint',
        'switch-type-wrap', 'switch-type-label', 'switch-type',
        'switch-number-wrap', 'switch-number-label', 'switch-number', 'switch-hint',
        'edit-delete', 'edit-restore', 'edit-reset',
        'edit-selected', 'edit-status', 'sec-edit-title', 'sec-points-title',
        'linked-tiles-wrap', 'linked-tiles', 'linked-tiles-label',
        'linked-tiles-use', 'linked-tiles-status', 'points-hint',
      ];
      const el = Object.fromEntries(NODE_IDS.map((id) => [id, get(id)]));

      // Режим правки доступен там, где редактор передал highlightTiles или
      // linkedTiles: остальным окнам (замер, чистый выбор позиции) он не нужен.
      const highlightIds = [...new Set((Array.isArray(options.highlightTiles) ? options.highlightTiles : [])
        .filter((id) => catalog.isEditable(id)))];
      // Правка — строго по объявленным highlightTiles, и у каждого слоя свой
      // набор. Слой приходит из редактора (`highlightLayer`), и ему верим
      // безоговорочно: 199/200/201 существуют и в игровом слое (метки узлов
      // сбора), и как типы switch-слоя, поэтому догадка «это switch» съедала
      // правку игрового слоя. Без явного слоя делим по каталогу switch.
      const explicitLayer = options.highlightLayer === 'switch' ? 'switch'
        : options.highlightLayer === 'game' ? 'game' : null;
      const editableGameIds = explicitLayer === 'switch' ? []
        : explicitLayer === 'game' ? highlightIds
        : highlightIds.filter((id) => !catalog.isSwitchTile(id));
      const editableSwitchIds = explicitLayer === 'switch' ? highlightIds
        : explicitLayer === 'game' ? []
        : highlightIds.filter((id) => catalog.isSwitchTile(id));
      // Есть что править, только если объявлен хотя бы один ID слоя.
      const editEnabled = editableGameIds.length > 0 || editableSwitchIds.length > 0;
      // Список «Существующие точки» — прежняя функция: выбрать координаты
      // подсвеченного тайла как позицию записи (без правки карты).
      // Список координат — только для игрового слоя: switch-тайлы показываются
      // предпросмотром. Условие по игровым ID, а не по отсутствию 'switch':
      // так список не появится и тогда, когда объявлены только switch-типы.
      const linkedEnabled = options.linkedTiles === true && editableGameIds.length > 0;

      for (const id of ['sec-edit', 'sec-points']) el[id].hidden = true;

      // ── подписи ───────────────────────────────────────────────────────────
      const tileName = (layer, id) => {
        const key = catalog.label(layer, id);
        return text.tileNames?.[key] || key;
      };
      const fill = (template, ...values) => String(template || '')
        .replace(/\{(\d+)\}/g, (match, index) => (values[Number(index)] ?? match));

      el['edit-tile-label'].textContent = text.editTileLabel;
      el['switch-type-label'].textContent = text.switchTypeLabel;
      el['switch-number-label'].textContent = text.switchNumberLabel;
      el['edit-layer-game'].textContent = text.editLayerGame;
      el['edit-layer-switch'].textContent = text.editLayerSwitch;
      el['layer-group'].setAttribute('aria-label', text.editLayerLabel);
      el['edit-type-hint'].textContent = text.editTypeHint;
      el['switch-hint'].textContent = text.switchHint;
      el['linked-tiles-label'].textContent = text.linkedLabel;
      el['linked-tiles-use'].textContent = text.linkedUse;
      el['points-hint'].textContent = text.pointsHint;
      el['collision-label'].textContent = text.collision;
      el.spawn.textContent = text.spawn;
      el['layers-reset'].textContent = text.layers;
      // Подписи кнопок — из общего словаря, как и остальные секции панели.
      el['edit-delete'].textContent = text.editDelete;
      el['edit-restore'].textContent = text.editRestore;
      el['edit-reset'].textContent = text.editReset;
      get('sec-edit-title').textContent = text.editTitle;
      get('sec-points-title').textContent = text.editTileListTitle;

      el.collision.checked = options.tiles === true;

      // ── состояние ─────────────────────────────────────────────────────────
      const sizes = {};
      let draft = null;
      let layer = 'game';
      let editMode = false;
      let selection = null;
      let linkedPoints = [];
      let layers = [];
      let press = null;
      let touch = null;
      let view = null;
      let statusKey = '';
      const statusArgs = [];
      const setStatus = (key, ...args) => {
        statusKey = key;
        statusArgs.length = 0;
        statusArgs.push(...args);
        paintStatus();
      };
      const paintStatus = () => {
        const message = statusKey ? fill(text[statusKey], ...statusArgs) : '';
        if (el['edit-status'].textContent !== message) el['edit-status'].textContent = message;
        el['edit-status'].classList.toggle('is-warning',
          statusKey === 'editCapReached' || statusKey === 'editCollision' || statusKey === 'editDrift');
      };

      // ── чтение слоёв карты ────────────────────────────────────────────────
      // Слой правится, если он есть в разобранной карте: размеры приходят из
      // `ready()` (gameplay/switchLayer), а запись в файл делает picker.js.
      // Слой правится, только если у него есть объявленные highlightTiles:
      // без них остаётся обычный выбор позиции.
      const gameLayer = () => (editableGameIds.length ? sizes.game : null);
      const switchLayer = () => (editableSwitchIds.length ? sizes.switch : null);
      /** Все ID, правка которых разрешена этим объявлением. */
      const isDeclared = (target, id) => (target === 'switch'
        ? editableSwitchIds.includes(Number(id))
        : editableGameIds.includes(Number(id)));
      // Размер слоя нужен и оверлею (показать все точки), но править его можно
      // только при объявленных highlightTiles: `sizeOf` — про правку,
      // `rawSize` — про отрисовку.
      const rawSize = (target) => (target === 'switch' ? sizes.switch : sizes.game) || null;
      const sizeOf = (target) => (target === 'switch'
        ? (editableSwitchIds.length ? sizes.switch : null)
        : (editableGameIds.length ? sizes.game : null));
      /**
       * Значение клетки в живой карте. Внимание: черновик пишет сюда же,
       * поэтому исходное состояние для сравнения берётся из `baseline`
       * (см. `readOriginal`).
       */
      const readCell = (target, index) => {
        if (target === 'switch') {
          const cells = tw.map?.switchCells;
          return cells ? cells.get(index) || null : null;
        }
        const game = tw.map?.gameplay;
        return game ? game.indices[index] || 0 : 0;
      };
      /** Применяет значение черновика к живому просмотру карты. */
      const applyCell = (target, index, value) => {
        if (target === 'switch') {
          const cells = tw.map?.switchCells;
          if (!cells) return;
          if (value && value.type) cells.set(index, { number: value.number, type: value.type });
          else cells.delete(index);
          return;
        }
        const game = tw.map?.gameplay;
        if (game && index >= 0 && index < game.indices.length) game.indices[index] = value || 0;
      };
      const cell = (target, index) => {
        const size = rawSize(target);
        return { target, index, x: index % size.width, y: Math.floor(index / size.width) };
      };

      /**
       * Исходное значение клетки — всегда из первоначальной карты, а не из
       * живого просмотра: черновик сам меняет `tw.map`, и читать оттуда
       * значило бы сравнивать правку саму с собой. `baseline` запоминает
       * исходные значения при первом касании клетки.
       */
      const baseline = new Map(); // `${layer}:${index}` → исходное значение
      const readOriginal = (target, index) => {
        const key = `${target}:${index}`;
        if (baseline.has(key)) return baseline.get(key);
        return readCell(target, index);
      };
      const ensureDraft = () => {
        if (draft) return draft;
        draft = model.createDraft({
          sizes,
          // read() должен быть неизменным: карта в `tw.map` живёт своей
          // жизнью, поэтому читаем из снимка исходного состояния.
          read: readOriginal,
          onCell: applyCell,
          limit: model.MAX_EDITS,
          onChange: () => { paintSelection(); paintDraftStatus(); redraw(); },
        });
        return draft;
      };
      /** Запомнить исходное значение клетки до первой правки. */
      const remember = (target, index) => {
        const key = `${target}:${index}`;
        if (!baseline.has(key)) baseline.set(key, readCell(target, index));
      };
      /**
       * Проблемы черновика. Модель сверяет правки со своим неизменным
       * снимком, но живая карта — отдельная копия: черновик пишет в неё сам,
       * а другие окна/повторная загрузка могут её поменять. Поэтому
       * дополнительно сверяем каждую правку с живым значением: оно должно
       * совпадать с тем, что черновик туда записал.
       */
      const consistencyProblems = () => {
        if (!draft) return [];
        const problems = draft.problems().slice();
        const known = new Set(problems.map((item) => `${item.layer}:${item.index}`));
        const normalize = (target, value) => (target === 'switch'
          ? model.normalizeSwitch(value) : model.normalizeGame(value));
        for (const target of model.LAYERS) {
          for (const entry of draft.list(target)) {
            const key = `${target}:${entry.index}`;
            if (known.has(key)) continue;
            const live = normalize(target, readCell(target, entry.index));
            if (!model.same(live, draft.get(target, entry.index))) {
              problems.push({ layer: target, index: entry.index, reason: 'drift' });
              known.add(key);
            }
          }
        }
        return problems;
      };

      // ── списки тайлов ─────────────────────────────────────────────────────
      /**
       * В списке ровно объявленные ID: правка не должна позволять выбрать
       * тайл, который редактор не подсветил. ID вне каталога (свой тип у
       * редактора) тоже допустим, если он пришёл в highlightTiles.
       */
      const optionsGame = () => editableGameIds.slice().sort((a, b) => a - b);
      const fillGameTiles = () => {
        el['edit-tile'].replaceChildren();
        for (const id of optionsGame()) {
          const option = document.createElement('option');
          option.value = String(id);
          option.textContent = `${id} · ${tileName('game', id)}`;
          el['edit-tile'].appendChild(option);
        }
        const preferred = editableGameIds[0];
        if (preferred !== undefined) el['edit-tile'].value = String(preferred);
        el['edit-tile'].disabled = !gameLayer();
      };
      const fillSwitchTypes = () => {
        el['switch-type'].replaceChildren();
        for (const id of editableSwitchIds.slice().sort((a, b) => a - b)) {
          const option = document.createElement('option');
          option.value = String(id);
          option.textContent = `${id} · ${tileName('switch', id)}`;
          el['switch-type'].appendChild(option);
        }
        if (editableSwitchIds.length) el['switch-type'].value = String(editableSwitchIds[0]);
        el['switch-type'].disabled = !switchLayer();
        el['switch-number'].disabled = !switchLayer();
      };
      /** Допустимый номер switch или null: невалидное значение не подменяем. */
      const switchNumberValue = () => {
        const raw = String(el['switch-number'].value ?? '').trim();
        const number = Number(raw);
        const valid = raw !== '' && Number.isInteger(number) && number >= 1 && number <= 255;
        el['switch-number'].classList.toggle('is-warning', !valid);
        return valid ? number : null;
      };
      /** Номер для уже существующей клетки: при невалидном вводе — её прежний. */
      const switchNumberOr = (fallback) => switchNumberValue() ?? fallback;
      /**
       * Тип switch берётся из списка, но значение DOM может быть подменено:
       * принимаем только объявленный тип, иначе null (как и с номером).
       */
      const switchTypeValue = () => {
        const type = Math.round(Number(el['switch-type'].value));
        return editableSwitchIds.includes(type) ? type : null;
      };
      /**
       * Значение для новой клетки выбранного слоя. `null` — ввод неполный
       * (например, номер switch вне 1–255): ставить такое нельзя.
       */
      const valueFor = () => {
        if (layer !== 'switch') {
          const value = Math.round(Number(el['edit-tile'].value));
          return isDeclared('game', value) ? value : null;
        }
        const number = switchNumberValue();
        const type = switchTypeValue();
        return number === null || type === null ? null : { number, type };
      };

      const paintLayers = () => {
        for (const [target, button] of [['game', el['edit-layer-game']], ['switch', el['edit-layer-switch']]]) {
          const active = layer === target;
          button.setAttribute('aria-checked', String(active));
          button.classList.toggle('is-active', active);
        }
        el['edit-pane-game'].hidden = layer !== 'game';
        el['edit-pane-switch'].hidden = layer !== 'switch';
        el['edit-layer-switch'].disabled = !switchLayer();
        paintSelection();
        paintDraftStatus();
        redraw();
      };

      // ── выделение и статусы ───────────────────────────────────────────────
      const paintSelection = () => {
        const empty = !selection || selection.target !== layer;
        el['edit-delete'].disabled = empty;
        // «Вернуть» доступно только для клетки, у которой есть правка.
        el['edit-restore'].disabled = empty || !draft?.list(layer).some((entry) => entry.index === selection.index);
        if (empty) {
          // Пустое выделение не дублирует подсказку режима: строка скрывается,
          // а состояние объясняет статус секции.
          el['edit-selected'].textContent = '';
          return;
        }
        const spot = cell(layer, selection.index);
        const value = draft ? draft.get(layer, selection.index) : readCell(layer, selection.index);
        // Пустая клетка тоже «выбрана»: клик по ней поставит точку, поэтому
        // подпись показывает координаты и честно пишет «пусто».
        if (layer === 'switch') {
          el['edit-selected'].textContent = value?.type
            ? fill(text.editSelectedSwitch, spot.x, spot.y, value.type, value.number)
            : fill(text.editSelectedSwitch, spot.x, spot.y, text.empty, '—');
        } else {
          el['edit-selected'].textContent = fill(text.editSelected, spot.x, spot.y, value || text.empty);
        }
      };

      const paintDraftStatus = () => {
        const counts = { added: 0, moved: 0, deleted: 0 };
        const list = model.LAYERS.flatMap((target) => draft ? draft.list(target) : []);
        for (const entry of list) {
          const empty = !entry.after || entry.after.type === 0 || entry.after === 0;
          const wasEmpty = !entry.before || entry.before.type === 0 || entry.before === 0;
          if (empty && !wasEmpty) counts.deleted++;
          else if (!empty && wasEmpty) counts.added++;
          else counts.moved++;
        }
        api.onBadge?.(list.length, counts);
        // Рассинхронизация важнее сводки: иначе пользователь видел бы
        // «всё готово» рядом с заблокированным «Применить».
        const problems = consistencyProblems();
        if (problems.length) setStatus('editDrift', problems.length);
        else if (list.length) setStatus('editPending', counts.added, counts.moved, counts.deleted);
        else if (editMode) setStatus('editPendingEmpty');
        else setStatus('');
        // Кнопка «Применить» зависит от состояния черновика (в том числе от
        // рассинхронизации с картой) — сообщаем об этом окну.
        api.onDraftChange?.(list.length);
      };

      // ── выбор клетки и правки ─────────────────────────────────────────────
      const canEditLayer = () => (layer === 'switch' ? !!switchLayer() : !!gameLayer());

      /**
       * Причина, по которой клетку нельзя править, или '' если можно.
       * Разделяем «это вообще не точка» и «эта точка не объявлена»:
       * пользователю важно знать, что он кликнул по чужому ID.
       */
      const blockedReason = (target, value) => {
        const occupied = target === 'switch' ? !!(value && value.type) : Number(value) > 0;
        if (!occupied) return '';
        if (editableValue(target, value)) return '';
        if (target === 'game' && !catalog.isEntity(Number(value))) return 'editNotEntity';
        if (target === 'switch' && !catalog.isSwitchTile(value.type)) return 'editNotEntity';
        return 'editNotDeclared';
      };
      /**
       * Можно ли трогать это значение. Правится ровно то, что редактор объявил
       * в `highlightTiles`: чужие сущности, вода, зоны и стены — часть карты,
       * их нельзя ни красить, ни двигать, ни удалять. У switch-слоя номер
       * switch можно менять любой (1–255), но только у объявленных типов.
       */
      const editableValue = (target, value) => (target === 'switch'
        ? !!(value && value.type) && isDeclared('switch', value.type)
        : isDeclared('game', Number(value) || 0));

      const selectCell = (target, index, { reveal = false } = {}) => {
        if (!model.LAYERS.includes(target)) return false;
        const size = sizeOf(target);
        if (!size || index === null || index < 0 || index >= size.width * size.height) return false;
        const value = draft ? draft.get(target, index) : readCell(target, index);
        // Занятую клетку, которую нельзя править (вода, зона, стена),
        // не выделяем: иначе Delete молча пытался бы её удалить.
        const blocked = blockedReason(target, value);
        if (blocked) {
          setStatus(blocked, target === 'switch' ? value?.type : value);
          return false;
        }
        layer = target;
        selection = { target, index };
        if (reveal) {
          const world = { x: (index % size.width) * TILE + TILE / 2, y: Math.floor(index / size.width) * TILE + TILE / 2 };
          api.setPoint(world.x, world.y, { snap: false });
          api.setCamera(world.x, world.y, Math.max(api.fitZoom(), .8));
        }
        paintLayers();
        return true;
      };

      /** Клик по клетке: выбрать занятую, поставить новую в пустую. */
      const clickCell = (target, x, y) => {
        const size = sizeOf(target);
        if (!size) {
          const declared = target === 'switch' ? editableSwitchIds.length : editableGameIds.length;
          if (!declared) {
            setStatus(target === 'switch' ? 'editNoSwitchDeclared' : 'editNoGameDeclared');
            return false;
          }
          setStatus(target === 'switch' ? 'editNoLayer' : 'editNoGame');
          return false;
        }
        if (x < 0 || y < 0 || x >= size.width || y >= size.height) return false;
        const edit = ensureDraft();
        const current = edit.get(target, y * size.width + x);
        layer = target;
        selection = { target, index: y * size.width + x };
        const occupied = target === 'switch' ? !!(current && current.type) : (current > 0);
        if (!occupied) {
          const value = valueFor();
          // valueFor() возвращает null и для невалидного номера switch, и для
          // ID, которого нет в объявлении: сообщение выбираем по причине.
          if (!value) {
            paintLayers();
            setStatus(target === 'switch' ? 'editBadSwitchNumber' : 'editNotDeclared',
              target === 'switch' ? undefined : el['edit-tile'].value);
            return false;
          }
          remember(target, selection.index);
          if (target === 'game' && catalog.isCollision(value)) { setStatus('editCollision'); paintLayers(); return false; }
          if (!edit.set(target, x, y, value)) { setStatus('editCapReached'); paintLayers(); return false; }
          const world = { x: x * TILE + TILE / 2, y: y * TILE + TILE / 2 };
          api.setPoint(world.x, world.y, { snap: false });
          paintLayers();
          return true;
        }
        const blocked = blockedReason(target, current);
        if (blocked) {
          // Чужая геометрия или необъявленный ID: подсказываем, что тут
          // править нечего, и не превращаем клетку в «выбранную» с Delete.
          selection = null;
          paintLayers();
          setStatus(blocked, target === 'switch' ? current?.type : current);
          return false;
        }
        // Занятая клетка того же значения просто выбирается: так руда с
        // нужным номером не перезаписывается случайным кликом.
        const same = target === 'switch'
          ? current.number === switchNumberOr(current.number) && current.type === (switchTypeValue() ?? current.type)
          : current === Math.round(Number(el['edit-tile'].value));
        setStatus(same ? '' : 'editOccupied');
        paintLayers();
        return false;
      };

      const deleteSelected = () => {
        if (!selection || selection.target !== layer || !canEditLayer()) return false;
        const edit = ensureDraft();
        const current = edit.get(layer, selection.index);
        if (!editableValue(layer, current)) {
          // Пустая клетка или геометрия карты: удалять нечего.
          paintDraftStatus();
          return false;
        }
        const spot = cell(layer, selection.index);
        remember(layer, selection.index);
        if (!edit.set(layer, spot.x, spot.y, layer === 'switch' ? { number: 0, type: 0 } : 0)) {
          setStatus('editCapReached');
          return false;
        }
        paintLayers();
        return true;
      };

      /** Отбросить весь черновик (кнопка «Сбросить правки» и клавиша R). */
      const resetEdits = () => {
        ensureDraft().reset();
        baseline.clear();
        selection = null;
        paintLayers();
      };

      const restoreSelected = () => {
        if (!selection || selection.target !== layer) return false;
        const spot = cell(layer, selection.index);
        const restored = ensureDraft().restore(layer, spot.x, spot.y);
        paintLayers();
        return restored;
      };

      const moveSelected = (worldX, worldY) => {
        if (!selection || selection.target !== layer) return false;
        const size = sizeOf(layer);
        if (!size) return false;
        const tx = Math.floor(worldX / TILE), ty = Math.floor(worldY / TILE);
        const target = ty * size.width + tx;
        if (target === selection.index) return false;
        const edit = ensureDraft();
        const current = edit.get(layer, selection.index);
        if (!editableValue(layer, current)) {
          // Пустая «выбранная» клетка просто переезжает вместе с кликом.
          selection = { target: layer, index: target };
          paintSelection();
          return false;
        }
        remember(layer, selection.index);
        remember(layer, target);
        const moved = edit.move(layer, cell(layer, selection.index), { x: tx, y: ty });
        if (!moved) { setStatus('editOccupied'); return false; }
        selection = { target: layer, index: target };
        paintLayers();
        return true;
      };

      // ── режим ─────────────────────────────────────────────────────────────
      const setEditMode = (enabled) => {
        press = touch = null;
        const next = !!enabled && editEnabled && canEditLayer();
        if (next && !editMode) {
          // Режим сам показывает игровой слой и сетку, а при выходе
          // возвращает прежние переключатели пользователя.
          view = { collision: el.collision.checked, grid: get('grid').checked };
          el.collision.checked = true;
          get('grid').checked = true;
        } else if (!next && editMode && view) {
          el.collision.checked = view.collision;
          get('grid').checked = view.grid;
          view = null;
        }
        editMode = next;
        canvas.classList.toggle('is-editing-tiles', editMode);
        if (!editMode) selection = null;
        api.onModeChange?.(editMode ? 'tile' : 'point');
        setStatus(editMode ? 'editPendingEmpty' : '');
        if (editMode) paintDraftStatus();
        paintSelection();
        redraw();
      };

      // ── обработчики панели ────────────────────────────────────────────────
      el.collision.addEventListener('change', redraw);
      el['edit-layer-game'].addEventListener('click', () => { layer = 'game'; paintLayers(); });
      el['edit-layer-switch'].addEventListener('click', () => { layer = 'switch'; paintLayers(); });
      el['edit-tile'].addEventListener('change', () => {
        // Смена тайла при выбранной занятой клетке — это её перекраска.
        if (selection && selection.target === 'game' && canEditLayer()) {
          const spot = cell('game', selection.index);
          const current = ensureDraft().get('game', selection.index);
          if (current > 0) {
            const value = valueFor();
            if (!value) { paintLayers(); setStatus('editNotDeclared', current); return; }
            remember('game', selection.index);
            ensureDraft().set('game', spot.x, spot.y, value);
          }
        }
        paintLayers();
      });
      for (const input of [el['switch-type'], el['switch-number']]) {
        input.addEventListener('change', () => {
          if (selection && selection.target === 'switch' && switchLayer()) {
            const spot = cell('switch', selection.index);
            const current = ensureDraft().get('switch', selection.index);
            if (current && current.type) {
              // Невалидный номер не подменяем единицей: клетка остаётся с
              // прежним номером, а поле подсвечивается.
              if (switchNumberValue() === null) {
                paintLayers();           // перерисовать поле/выделение
                setStatus('editBadSwitchNumber'); // и уже потом объяснить
                return;
              }
              if (switchTypeValue() === null) {
                paintLayers();
                setStatus('editNotDeclared', current.type);
                return;
              }
              remember('switch', selection.index);
              ensureDraft().set('switch', spot.x, spot.y, { number: switchNumberValue(), type: switchTypeValue() });
            }
          }
          paintLayers();
        });
      }
      el['edit-delete'].addEventListener('click', deleteSelected);
      el['edit-restore'].addEventListener('click', restoreSelected);
      el['edit-reset'].addEventListener('click', resetEdits);
      el['linked-tiles-use'].addEventListener('click', () => {
        const index = Number(el['linked-tiles'].value);
        const point = Number.isInteger(index) ? linkedPoints[index] : null;
        if (!point) return;
        api.setPoint(point.x, point.y, { snap: false });
        api.setCamera(point.x, point.y, Math.max(api.fitZoom(), .8));
        // Точку списка тоже выделяем: следующая правка почти наверняка о ней.
        if (Number.isInteger(point.index)) selectCell('game', point.index);
      });
      el['linked-tiles'].addEventListener('change', () => {
        const index = Number(el['linked-tiles'].value);
        const point = Number.isInteger(index) ? linkedPoints[index] : null;
        if (point && Number.isInteger(point.index)) selectCell('game', point.index);
      });
      el['layers-reset'].addEventListener('click', () => {
        for (const { layer: decoration, input } of layers) { decoration.editorHidden = false; input.checked = true; }
        redraw();
      });
      el.spawn.addEventListener('click', () => {
        const spawn = tw.map?.spawnPoint;
        if (spawn) api.setCamera(spawn.x, spawn.y, Math.max(api.fitZoom(), .8));
      });

      // ── мышь и тач ────────────────────────────────────────────────────────
      // Клики в режиме правки перехватываются в capture-фазе: иначе точка
      // записи сдвигалась бы дважды — от клетки и от обычного клика.
      // Перетаскивание редактируемой (занятой и объявленной) точки переносит
      // её, а нажатие на пустую клетку или чужую сущность отдаёт событие
      // рендереру — зажатой ЛКМ/пальцем камера панорамируется самой картой,
      // как и в обычном режиме. Короткий клик всё равно ставит/выбирает клетку.
      const pixelAt = (event) => pointer(event) || { x: 0, y: 0 };
      const cellUnder = (event) => {
        const world = pixelAt(event);
        const size = sizeOf(layer);
        if (!size) return null;
        const x = Math.floor(world.x / TILE), y = Math.floor(world.y / TILE);
        if (x < 0 || y < 0 || x >= size.width || y >= size.height) return null;
        return { x, y, index: y * size.width + x, world };
      };
      // Тащить можно только занятую объявленную клетку: всё остальное
      // (пустое поле, стены, чужие сущности) — панорама камеры.
      const dragValueAt = (event) => {
        const spot = cellUnder(event);
        if (!spot) return null;
        const value = draft ? draft.get(layer, spot.index) : readCell(layer, spot.index);
        return editableValue(layer, value) ? value : null;
      };

      window.addEventListener('mousedown', (event) => {
        if (!editMode || !state.ready || event.target !== canvas || event.button !== 0) return;
        canvas.focus({ preventScroll: true });
        // Клетку под курсором выделяем сразу: так перетаскивание занятой
        // точки работает без отдельного «сначала клик, потом тащить».
        const spot = cellUnder(event);
        if (spot) selectCell(layer, spot.index);
        if (dragValueAt(event)) {
          event.preventDefault();
          event.stopImmediatePropagation();
          press = { x: event.clientX, y: event.clientY, moved: false, lock: 'tile' };
          return;
        }
        // Нечего тащить — пусть карту двигает рендерер; жест трактуем как панораму.
        press = { x: event.clientX, y: event.clientY, moved: false, lock: 'pan' };
      }, true);

      window.addEventListener('mousemove', (event) => {
        if (!press || !editMode) return;
        if (press.lock === 'pan') return; // камеру двигает рендерер
        if (Math.hypot(press.x - event.clientX, press.y - event.clientY) > 6) {
          if (!press.moved) {
            press.moved = true;
            // Двигать можно только занятую клетку; пустая просто снимает выбор.
            if (!selection) press.moved = false;
          }
          if (press.moved) {
            const world = pixelAt(event);
            moveSelected(world.x, world.y);
            event.preventDefault();
            event.stopImmediatePropagation();
          }
        }
      }, true);

      window.addEventListener('mouseup', (event) => {
        if (!press) return;
        const start = press;
        press = null;
        if (start.lock === 'tile') {
          event.preventDefault();
          event.stopImmediatePropagation();
          if (event.target !== canvas) return;
          if (start.moved) { paintLayers(); return; }
          if (Math.hypot(start.x - event.clientX, start.y - event.clientY) > 6) return;
          const spot = cellUnder(event);
          if (spot) clickCell(layer, spot.x, spot.y);
          return;
        }
        // Панорама: клик (без заметного сдвига) — это обычный выбор/установка.
        if (event.target !== canvas) return;
        if (Math.hypot(start.x - event.clientX, start.y - event.clientY) > 6) return;
        const spot = cellUnder(event);
        if (spot) clickCell(layer, spot.x, spot.y);
      }, true);

      window.addEventListener('touchstart', (event) => {
        if (!editMode || !state.ready || event.target !== canvas) return;
        if (event.touches.length !== 1) { touch = null; return; }
        const point = event.touches[0];
        // Короткий тап ставит/выбирает точку, перенос редактируемой точки —
        // тащит её; всё остальное — жест рендереру (панорама и пинч-зум).
        const draggable = !!dragValueAt({ clientX: point.clientX, clientY: point.clientY });
        touch = { x: point.clientX, y: point.clientY, moved: false, at: Date.now(), lock: draggable ? 'tile' : 'pan' };
        if (draggable) {
          event.preventDefault();
          event.stopImmediatePropagation();
          selectCell(layer, cellUnder({ clientX: point.clientX, clientY: point.clientY }).index);
        }
      }, { capture: true, passive: false });

      window.addEventListener('touchmove', (event) => {
        if (!touch) return;
        if (event.touches.length !== 1) { touch = null; return; }
        const point = event.touches[0];
        if (Math.hypot(point.clientX - touch.x, point.clientY - touch.y) > 10) touch.moved = true;
        if (touch.lock === 'pan') return; // панораму делает рендерер
        if (touch.moved) {
          const world = screenToWorld(point.clientX, point.clientY);
          moveSelected(world.x, world.y);
        }
        event.preventDefault();
        event.stopImmediatePropagation();
      }, { capture: true, passive: false });

      window.addEventListener('touchend', (event) => {
        if (!touch) return;
        const start = touch;
        touch = null;
        // Глушим совместимые mouse-события (iOS Safari): иначе тап в
        // pan-ветке был бы обработан дважды — здесь и в мышином mouseup.
        // Рендереру не мешает: он слушает touch-события напрямую.
        if (event.cancelable) event.preventDefault();
        if (start.lock === 'tile') {
          event.preventDefault();
          event.stopImmediatePropagation();
          if (start.moved) { paintLayers(); return; }
          const point = event.changedTouches[0];
          if (!point) return;
          const spot = cellUnder({ clientX: point.clientX, clientY: point.clientY });
          if (spot) clickCell(layer, spot.x, spot.y);
          return;
        }
        // Панорама: короткий тап — это обычный выбор/установка клетки.
        if (start.moved || Date.now() - start.at > 700) return;
        const point = event.changedTouches && event.changedTouches[0];
        if (!point) return;
        const spot = cellUnder({ clientX: point.clientX, clientY: point.clientY });
        if (spot) clickCell(layer, spot.x, spot.y);
      }, { capture: true, passive: false });

      window.addEventListener('blur', () => { press = touch = null; });
      window.addEventListener('touchcancel', () => { press = touch = null; });

      // ── список связанных точек ────────────────────────────────────────────
      const refreshLinkedPoints = () => {
        if (!linkedEnabled) return;
        // Тот же поиск точек, что рисует подсветку (picker-overlays.js):
        // у каждой строки есть index, поэтому выбор строки выделяет её клетку.
        linkedPoints = draw.findHighlightedTiles(tw.map?.gameplay, highlightIds, 10000);
        el['linked-tiles'].replaceChildren();
        for (const [index, point] of linkedPoints.entries()) {
          const option = document.createElement('option');
          option.value = String(index);
          option.textContent = `${text.linkedPoint} ${index + 1} · X ${point.x}, Y ${point.y} · ID ${point.tile}`;
          el['linked-tiles'].appendChild(option);
        }
        if (linkedPoints.length && state.hasPoint) {
          let nearest = 0, nearestDistance = Infinity;
          linkedPoints.forEach((point, index) => {
            const distance = Math.hypot(point.x - state.point.x, point.y - state.point.y);
            if (distance < nearestDistance) { nearest = index; nearestDistance = distance; }
          });
          el['linked-tiles'].value = String(nearest);
        } else if (linkedPoints.length) el['linked-tiles'].value = '0';
        el['linked-tiles-use'].disabled = linkedPoints.length === 0;
        el['linked-tiles-status'].textContent = linkedPoints.length === 0 ? text.linkedEmpty
          : (linkedPoints.length === 10000 ? fill(text.linkedLimit, '10000')
            : fill(text.linkedCount, String(linkedPoints.length)));
      };

      return {
        get mode() { return editMode ? 'tile' : 'point'; },
        /** Есть ли вообще что править по объявлению highlightTiles. */
        get editEnabled() { return editEnabled; },
        get editMode() { return editMode; },
        get layer() { return layer; },
        get selection() { return selection ? { ...selection, ...cell(selection.target, selection.index) } : null; },
        getPendingChanges() {
          return draft ? draft.changes() : { game: [], switch: [] };
        },
        get pendingCount() { return draft ? draft.size : 0; },
        /** Рассинхронизация черновика с картой — повод не сохранять. */
        problems: consistencyProblems,
        isConsistent: () => consistencyProblems().length === 0,
        /**
         * После успешного сохранения карта уже записана: черновик очищается,
         * а `tw.map` пересобран в picker.js (MapPickerMapEdit.reload).
         */
        markSaved() {
          draft = null;
          baseline.clear();
          selection = null;
          paintLayers();
        },
        setEditMode,
        /**
         * Почему режим правки недоступен, или '' если доступен. Нужно панели,
         * чтобы не показывать общий текст «нужен игровой слой» там, где
         * редактор просто ничего не объявил.
         */
        editUnavailableReason() {
          if (!editableGameIds.length && !editableSwitchIds.length) return text.editNothingDeclared;
          // До загрузки карты размеров ещё нет: это не «недоступно», а «ещё
          // не готово», иначе кнопка осталась бы выключенной навсегда.
          if (!sizes.game && !sizes.switch) return '';
          if (!canEditLayer()) {
            return layer === 'switch'
              ? (editableSwitchIds.length ? text.editNoLayer : text.editNoSwitchDeclared)
              : (editableGameIds.length ? text.editNoGame : text.editNoGameDeclared);
          }
          return '';
        },
        startPlacement() {
          if (!editEnabled || !canEditLayer()) return false;
          if (!editMode) setEditMode(true);
          return true;
        },
        cancelPlacement() { if (editMode) setEditMode(false); },
        selectCell: (target, index) => selectCell(target, index, { reveal: true }),
        /** Слой правки по кнопке/клавише: недоступный слой не включается. */
        selectLayer(target) {
          if (target === 'switch' && !switchLayer()) {
            setStatus(editableSwitchIds.length ? 'editNoLayer' : 'editNoSwitchDeclared');
            return false;
          }
          if (target === 'game' && !gameLayer()) {
            setStatus(editableGameIds.length ? 'editNoGame' : 'editNoGameDeclared');
            return false;
          }
          layer = target;
          if (selection && selection.target !== layer) selection = null;
          paintLayers();
          return true;
        },
        /** «Сбросить правки» кнопкой и клавишей R — одно и то же действие. */
        resetEdits,
        deleteSelected,
        setStatus,
        paintStatus,
        /** Готовый текст (например ошибка сохранения) без ключа словаря. */
        setStatusText(message) {
          statusKey = '';
          statusArgs.length = 0;
          const value = String(message || '');
          if (el['edit-status'].textContent !== value) el['edit-status'].textContent = value;
          el['edit-status'].classList.toggle('is-warning', !!value);
        },
        /** Вызывается после (пере)загрузки карты и после сохранения. */
        ready() {
          // Размеры слоёв берём из уже разобранной карты (gameplay/switchLayer):
          // так панель работает и там, где датафайл недоступен (предпросмотр,
          // тесты), а правка просто останется черновиком без сохранения.
          const gameplay = tw.map?.gameplay;
          const switchTiles = tw.map?.switchLayer;
          sizes.game = gameplay ? { width: gameplay.width, height: gameplay.height } : null;
          sizes.switch = switchTiles ? { width: switchTiles.width, height: switchTiles.height } : null;
          // Удобный для оверлея вид switch-клеток с координатами: он же
          // источник правды для черновика.
          if (switchTiles && tw.map) {
            const cells = new Map();
            for (const tile of switchTiles.tiles || []) {
              const x = Math.floor(tile.x / TILE), y = Math.floor(tile.y / TILE);
              cells.set(y * switchTiles.width + x, { number: tile.number, type: tile.type });
            }
            tw.map.switchCells = cells;
          }
          draft = null;
          baseline.clear();
          selection = null;
          // Если объявлены ID только одного слоя, начинаем с него.
          if (!canEditLayer()) layer = gameLayer() ? 'game' : 'switch';
          fillGameTiles();
          fillSwitchTypes();
          for (const id of ['sec-edit', 'sec-points']) el[id].hidden = !editEnabled && !linkedEnabled;
          el['sec-edit'].hidden = !editEnabled;
          el['sec-points'].hidden = !linkedEnabled;
          el['linked-tiles-wrap'].hidden = !linkedEnabled;
          el['edit-layer-switch'].disabled = !switchLayer();
          el['edit-layer-game'].disabled = !gameLayer();
          paintLayers();

          el.spawn.disabled = !tw.map?.spawnPoint;
          el.collision.disabled = !tw.map?.gameplay;
          refreshLinkedPoints();

          el['layer-list'].replaceChildren();
          layers = [];
          for (const [g, group] of (tw.map?.groups || []).entries()) {
            for (const [l, decoration] of (group.layers || []).entries()) {
              const label = document.createElement('label');
              const input = document.createElement('input');
              input.type = 'checkbox';
              input.checked = decoration.editorHidden !== true;
              const caption = document.createElement('span');
              caption.textContent = `${text.group} ${g + 1} / ${l + 1}: `
                + `${decoration.type === tw.LAYERTYPE_TILES ? text.tileLayer : text.quadLayer}`
                + `${decoration.width ? ` (${decoration.width}×${decoration.height})` : ''}`;
              input.addEventListener('change', () => { decoration.editorHidden = !input.checked; redraw(); });
              label.append(input, caption);
              el['layer-list'].appendChild(label);
              layers.push({ layer: decoration, input });
            }
          }
        },
        draw(ctx) {
          const game = tw.map?.gameplay;
          const screen = screenSize();
          const bounds = draw.visibleTiles(game, screenToWorld(0, 0), screenToWorld(screen.w, screen.h));
          let status = '';
          if (el.collision.checked) {
            status = !game ? text.noGame : !bounds ? text.zoom : text.legend;
            if (Array.isArray(options.safe) && options.safe.length) status += `${status ? ' · ' : ''}${text.blocked}`;
            // Третий аргумент — объявленные highlightTiles: спавны и прочие
            // сущности редактора рисуются зелёными, как обещает легенда.
            draw.drawCollision(ctx, game, bounds, worldToScreen, options.safe, options.highlightTiles);
          }
          if (options.highlightLayer === 'switch') {
            const count = draw.drawSwitchTiles(ctx, tw.map?.switchLayer, worldToScreen,
              options.highlightTiles, options.switchShowID);
            const nodeStatus = `${text.linkedNodes}: ${count}${tw.map?.switchLayer ? '' : ` (${text.noSwitchLayer})`}`;
            status += `${status ? ' · ' : ''}${nodeStatus}`;
            if (options.preview) get('note').textContent = nodeStatus;
          } else if (options.highlightTiles?.length) {
            draw.drawHighlightedTiles(ctx, game, bounds, worldToScreen, options.highlightTiles);
          }

          // В режиме правки поверх карты видны только объявленные точки слоя,
          // черновик и выделенная клетка: пользователь видит ровно то, что
          // может менять, и не путает чужие сущности с доступными.
          if (editMode) {
            if (layer === 'game' && game) {
              draw.drawGamePoints(ctx, game, bounds, worldToScreen,
                (id) => isDeclared('game', id), { color: '#52df9a' });
            } else if (layer === 'switch' && tw.map?.switchCells) {
              const declaredCells = new Map([...tw.map.switchCells]
                .filter(([, value]) => value && isDeclared('switch', value.type)));
              draw.drawSwitchPoints(ctx, declaredCells, rawSize('switch'), worldToScreen, { color: '#52df9a', labels: true });
            }
            if (draft) {
              const marks = model.LAYERS.flatMap((target) => draft.list(target).map((entry) => ({
                x: entry.x, y: entry.y,
                kind: !entry.after || entry.after.type === 0 || entry.after === 0 ? 'delete'
                  : (!entry.before || entry.before.type === 0 || entry.before === 0 ? 'add' : 'move'),
              })));
              draw.drawEditMarks(ctx, marks, worldToScreen);
            }
            if (selection && selection.target === layer) {
              const spot = cell(layer, selection.index);
              const value = draft ? draft.get(layer, selection.index) : readCell(layer, selection.index);
              const caption = layer === 'switch'
                ? `#${value?.number ?? '—'} · ${value?.type ?? '—'}`
                : `ID ${value ?? 0}`;
              draw.drawSelection(ctx, spot, worldToScreen, caption);
            }
          }

          if (state.hasPoint && game) {
            const index = draw.tileAt(game, state.point);
            const kind = index === null ? text.outside
              : index === 0 ? text.empty
                : `${({ 1: text.solid, 2: text.death, 3: text.nohook })[index] || text.tileLabel} (ID ${index})`;
            status += `${status ? ' · ' : ''}${text.point}: ${kind}`;
          }
          const cursor = api.getCursor();
          if (el.collision.checked && cursor && game) {
            const index = draw.tileAt(game, screenToWorld(cursor.x, cursor.y));
            status += ` · ${text.cursor}: ${index === null ? text.outside : `ID ${index}`}`;
          }
          const selectedTile = state.hasPoint ? draw.tileAt(game, state.point) : null;
          const blocked = draw.blockedTile(game, state.hasPoint ? state.point : null, options.safe);
          el['collision-status'].classList.toggle('is-warning', blocked !== null || [1, 2, 3].includes(selectedTile));
          if (el['collision-status'].textContent !== status) el['collision-status'].textContent = status;
        },
      };
    },
  };
})();
