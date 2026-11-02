/**
 * editor-core/db-history.js — модель вкладки «История изменений».
 *
 * Записи истории приходят с сервера (api/db-history.php) уже с полями
 * before/after; здесь только чистые преобразования: подписи, различия по
 * колонкам, вопрос подтверждения отмены. Благодаря этому правила отмены
 * проверяются тестами без браузера и без базы данных.
 *
 * Термины:
 *  • запись истории — одно изменение одного элемента (создание, изменение,
 *    удаление); у гостевого запроса — по записи на каждый элемент;
 *  • отмена — обратное изменение, которое само становится записью истории.
 */
(() => {
  if (window.EditorCore?.DbHistory) return;
  const namespace = (window.EditorCore = window.EditorCore || {});

  /**
   * Подписи разделов. Сервер присылает их по-русски (общие названия таблиц
   * игровой базы), а интерфейс студии — английский источник, поэтому известные
   * разделы подписаны здесь и переводятся словарём.
   */
  const RESOURCE_LABELS = {
    vouchers: 'Vouchers',
    crafts: 'Crafts',
    warehouses: 'Warehouses and shops',
    items: 'Items',
    bots_info: 'Bots',
    bots_mobs: 'Mobs',
    bots_npc: 'NPC',
    quests: 'Quests',
    quest_boards: 'Quest boards',
    quest_board_links: 'Quest board links',
    quest_bots: 'Quest steps',
    dungeons: 'Dungeons',
    worlds: 'Worlds',
    nodes_collecting: 'Collecting nodes',
    aethers: 'Aethers (teleports)',
    accounts: 'Player accounts',
  };

  /**
   * Раздел истории → вкладка редактора, в которой запись можно открыть.
   * Несколько разделов живут в одном редакторе (доска и её назначения, квест и
   * его шаги), поэтому ключи не совпадают один-к-одному.
   */
  const RESOURCE_EDITORS = {
    vouchers: 'vouchers',
    crafts: 'crafts',
    warehouses: 'warehouses',
    items: 'items',
    bots_info: 'bots_info',
    bots_mobs: 'bots_mobs',
    bots_npc: 'bots_npc',
    quests: 'quests',
    quest_boards: 'quest_boards',
    quest_board_links: 'quest_boards',
    quest_bots: 'quests',
    dungeons: 'dungeons',
    worlds: 'worlds',
    nodes_collecting: 'nodes_collecting',
    aethers: 'aethers',
  };

  /** Ключ вкладки редактора для записи истории ('' — открывать некуда). */
  const editorFor = (entry) => RESOURCE_EDITORS[String(entry?.resource || '')] || '';

  /** Страница редактора для записи истории: ключ вкладки и файл из манифеста. */
  const editorPage = (entry) => {
    const key = editorFor(entry);
    if (!key) return '';
    const groups = Array.isArray(window.STUDIO_MANIFEST) ? window.STUDIO_MANIFEST : [];
    for (const group of groups) {
      for (const item of group?.items || []) {
        if (item?.key === key) return String(item.src || '');
      }
    }
    return '';
  };

  /** Тип изменения: подпись, иконка и тон оформления. */
  const OP = {
    create: { label: 'Create', icon: 'fa-plus', tone: 'create' },
    update: { label: 'Edit', icon: 'fa-pen', tone: 'update' },
    delete: { label: 'Delete', icon: 'fa-trash', tone: 'delete' },
  };

  /** Откуда пришло изменение. */
  const SOURCE = {
    editor: { label: 'Editor', icon: 'fa-pen-to-square' },
    request: { label: 'Guest request', icon: 'fa-code-pull-request' },
    revert: { label: 'Undo', icon: 'fa-rotate-left' },
  };

  const hasOwn = (object, key) => Object.prototype.hasOwnProperty.call(object || {}, key);

  /** Подпись раздела: своя английская, иначе серверная. */
  const resourceLabel = (entry) => {
    const key = String(entry?.resource || '');
    return RESOURCE_LABELS[key] || String(entry?.resource_title || key);
  };

  /** Заголовок записи: «Items · #12 · Iron Sword». */
  const entryTitle = (entry) => {
    const parts = [resourceLabel(entry)];
    const id = Number(entry?.record_id);
    if (Number.isFinite(id) && (id > 0 || entry?.resource === 'worlds')) parts.push(`#${id}`);
    const label = String(entry?.label || '').trim();
    if (label) parts.push(label);
    return parts.join(' · ');
  };

  /** Значение для показа: пустое — пустая строка, объект — компактный JSON. */
  const formatValue = (value) => {
    if (value === null || value === undefined) return '';
    if (typeof value === 'string') return value;
    if (typeof value === 'number' || typeof value === 'boolean') return String(value);
    try {
      return JSON.stringify(value);
    } catch {
      return String(value);
    }
  };

  const valueKey = (value) => {
    if (value === null || value === undefined) return '';
    if (typeof value === 'object') {
      try { return JSON.stringify(value); } catch { return String(value); }
    }
    return String(value);
  };

  const sameValue = (a, b) => {
    if (typeof a === 'object' && a !== null || typeof b === 'object' && b !== null) {
      return valueKey(a) === valueKey(b);
    }
    // NULL, '' и 0 — «нет значения»: так же их сравнивает сервер, поэтому в
    // списке изменений ложных строк не появляется.
    const empty = (v) => v === null || v === undefined || v === '' || v === 0 || v === '0';
    if (empty(a) && empty(b)) return true;
    return String(a) === String(b);
  };

  /**
   * Различия по колонкам для окна отмены.
   * @returns {Array<{column: string, before: string, after: string, kind: string}>}
   */
  const diffRows = (entry) => {
    const before = entry?.before && typeof entry.before === 'object' ? entry.before : {};
    const after = entry?.after && typeof entry.after === 'object' ? entry.after : {};
    const op = String(entry?.op || 'update');
    const columns = [];
    if (op === 'create') {
      columns.push(...Object.keys(after));
    } else if (op === 'delete') {
      columns.push(...Object.keys(before));
    } else {
      for (const column of Object.keys(before)) if (!columns.includes(column)) columns.push(column);
      for (const column of Object.keys(after)) if (!columns.includes(column)) columns.push(column);
    }
    return columns.map((column) => {
      const hasBefore = hasOwn(before, column);
      const hasAfter = hasOwn(after, column);
      const oldValue = hasBefore ? formatValue(before[column]) : '';
      const newValue = hasAfter ? formatValue(after[column]) : '';
      let kind = 'unchanged';
      if (op === 'create' || (!hasBefore && hasAfter)) kind = 'added';
      else if (op === 'delete' || (hasBefore && !hasAfter)) kind = 'removed';
      else if (!sameValue(before[column], after[column])) kind = 'changed';
      return { column, before: oldValue, after: newValue, kind };
    });
  };

  /** Сколько полей реально изменено (для короткой подписи в списке). */
  const changedCount = (entry) => {
    const rows = diffRows(entry);
    if (!rows.length) return Number(entry?.changes) || 0;
    return rows.filter((row) => row.kind !== 'unchanged').length;
  };

  /**
   * Короткий список изменённых колонок: «Name, Path, Type».
   * В списке сервер не присылает before/after (запись может быть тяжёлой),
   * поэтому при их отсутствии берём готовый список `changed`.
   */
  const changedColumns = (entry) => {
    const rows = diffRows(entry);
    if (rows.length) return rows.filter((row) => row.kind !== 'unchanged').map((row) => row.column);
    return Array.isArray(entry?.changed) ? entry.changed.map(String) : [];
  };

  const canRevert = (entry) => !!entry && entry.can_revert === true;

  /** Почему отменить нельзя (английская строка для перевода) или ''. */
  const revertBlockReason = (entry) => {
    if (!entry) return '';
    if (entry.reverted_at) return 'This change has already been undone.';
    // Порядок причин важен: «раздел больше не редактируется» точнее, чем
    // «нет данных», поэтому проверяется раньше.
    if (entry.known_resource === false) return 'This section is no longer editable in the studio.';
    if (entry.revertable === false) return 'The journal has no full data for this change, so it cannot be undone automatically.';
    if (!OP[String(entry.op || '')]) return 'Unknown change type.';
    return '';
  };

  /** Вопрос подтверждения: что именно сделает отмена. */
  const revertQuestion = (entry) => {
    const columns = changedColumns(entry);
    const fields = columns.length;
    if (String(entry?.op) === 'create') {
      return 'Undo the creation: delete this record and everything it holds?';
    }
    if (String(entry?.op) === 'delete') {
      return 'Undo the deletion: restore the record with the same ID?';
    }
    return `Undo the edit: restore ${fields} previous field value(s)?`;
  };

  /** Группировка по дням для заголовков списка. */
  const groupByDay = (rows) => {
    const groups = [];
    const index = new Map();
    for (const row of Array.isArray(rows) ? rows : []) {
      const key = String(row?.at || '').slice(0, 10) || 'unknown';
      if (!index.has(key)) {
        const group = { key, rows: [] };
        index.set(key, group);
        groups.push(group);
      }
      index.get(key).rows.push(row);
    }
    return groups;
  };

  /** Автор изменения: «Admin · admin» или «Guest · nickname · approved by admin». */
  const actorText = (entry) => {
    const name = String(entry?.actor_name || entry?.actor_id || '').trim();
    const role = String(entry?.actor_role || '');
    const roleLabel = role === 'admin' ? 'Administrator' : (role === 'guest' ? 'Guest' : '');
    const parts = [];
    if (roleLabel) parts.push(roleLabel);
    if (name) parts.push(name);
    const approved = String(entry?.approved_by || '').trim();
    if (approved && approved !== name) parts.push(`approved by ${approved}`);
    return parts.join(' · ');
  };

  namespace.DbHistory = {
    RESOURCE_LABELS,
    RESOURCE_EDITORS,
    editorFor,
    editorPage,
    OP,
    SOURCE,
    resourceLabel,
    entryTitle,
    formatValue,
    sameValue,
    diffRows,
    changedCount,
    changedColumns,
    canRevert,
    revertBlockReason,
    revertQuestion,
    groupByDay,
    actorText,
  };
})();
