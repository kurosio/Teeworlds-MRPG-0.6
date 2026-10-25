/**
 * editor-core/map-worlds.js — одна точка правды о мирах для выбора позиции.
 *
 * Раньше `position-picker.js` и `point-map.js` независимо:
 *   • парсили ID мира (и по-разному относились к миру 0 — Tutorilishe);
 *   • читали/писали «последний выбранный мир» в localStorage;
 *   • грузили `DBCrud.list('worlds')` каждый в свой `<select>`.
 *
 * Теперь это делает один небольшой модуль: карточка позиции и окно карты
 * получают одинаковый список и одинаковый мир. Новых колонок БД не нужно.
 */
(() => {
  const namespace = (window.EditorCore = window.EditorCore || {});
  if (namespace.MapWorlds) return;

  const STORAGE_KEY = 'editor-map-world';

  /** ID 0 — полноценный мир, а не «мир не выбран». */
  const parseWorldId = (value) => {
    if (value === null || value === undefined || String(value).trim() === '') return null;
    const n = Number(value);
    return Number.isSafeInteger(n) && n >= 0 ? n : null;
  };

  const rememberedWorld = () => {
    try { return parseWorldId(window.localStorage.getItem(STORAGE_KEY)); } catch { return null; }
  };

  const rememberWorld = (value) => {
    const id = parseWorldId(value);
    if (id === null) return;
    try { window.localStorage.setItem(STORAGE_KEY, String(id)); } catch { /* приватный режим */ }
  };

  let cache = null;
  let pending = null;
  /** Один запрос на страницу: `[{ id, name }]`, отсортированный как в БД. */
  const list = () => {
    if (cache) return Promise.resolve(cache);
    if (!pending) {
      pending = (async () => {
        let rows = [];
        try {
          const res = await window.EditorCore?.DBCrud?.list?.('worlds', { limit: 1000, offset: 0 });
          rows = Array.isArray(res?.rows) ? res.rows : [];
        } catch { rows = []; }
        cache = rows
          .map((row) => ({ id: parseWorldId(row?.ID ?? row?.id), name: String(row?.Name ?? row?.name ?? '') }))
          .filter((row) => row.id !== null);
        return cache;
      })().catch(() => { cache = []; return cache; });
    }
    return pending;
  };

  const name = (worlds, id) => {
    const worldId = parseWorldId(id);
    if (worldId === null) return '';
    return String((Array.isArray(worlds) ? worlds.find((w) => w.id === worldId) : null)?.name || '');
  };

  namespace.MapWorlds = { STORAGE_KEY, parseWorldId, rememberedWorld, rememberWorld, list, name };
})();
