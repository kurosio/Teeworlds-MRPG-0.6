(() => {
  // Generic CRUD client for DB-backed editors.
  const API = 'api/db-crud.php';
  const listCache = window.EditorCore.utils.createRequestCache({ ttl: 30000, max: 100 });
  const jsonFetch = (url, options = {}) => window.EditorCore.Http.request(url, options);
  const qs = (...args) => window.EditorCore.utils.qs(...args);

  // Guest writes go to the local change-request draft; reads are overlaid with it.
  const guestApi = () => window.EditorCore?.Guest || null;
  const requestApi = () => window.EditorCore?.RequestContext || null;
  const assertRequestWritable = () => {
    if (requestApi()?.isReadOnly?.()) {
      throw new Error('A read-only snapshot of the request is open. Go back to the request and save the fixed version there.');
    }
  };
  const isGuestMode = async () => {
    const guest = guestApi();
    return guest ? (await guest.ready()) === 'guest' : false;
  };

  const rawGet = (resource, id) => jsonFetch(`${API}?${qs({ action: 'get', resource, id })}`);
  const rawGetRow = async (resource, id) => {
    try {
      const response = await rawGet(resource, id);
      return response?.row || null;
    } catch {
      return null;
    }
  };

  const recordSync = (resource, action, { id, data = null, skip = false } = {}) => {
    const sync = window.EditorCore?.Sync;
    if (skip || !DBCrud._syncRecordEnabled || !sync || (action === 'create' && !id)) return;
    const labels = { create: 'Create', update: 'Refresh', remove: 'Delete' };
    try {
      sync.record(resource, action === 'remove' ? 'delete' : action, {
        id,
        ...(data === null ? {} : { data }),
        label: `${labels[action]} ${resource} #${id}`,
      });
      sync.updateAllBadges();
    } catch (error) {
      console.warn(`[DBCrud] Failed to record ${action} operation:`, error.message);
    }
  };

  /** One mutation pipeline: invalidation, request revision, guest draft, HTTP and sync history. */
  const mutate = async (resource, action, { id = null, data = null, skipSync = false, guest } = {}) => {
    const preview = requestApi();
    if (preview?.active?.()) await preview.ready();
    listCache.clear();
    if (preview?.isEditing?.()) {
      const numericId = Number(id) || 0;
      const baseRow = action !== 'create' && numericId > 0 && numericId < 2000000000
        ? await rawGetRow(resource, numericId) : null;
      return preview.mutate({ action, resource, id, data, baseRow });
    }
    assertRequestWritable();
    if (await isGuestMode()) return guest(guestApi());

    const serverAction = action === 'remove' ? 'delete' : action;
    const response = await jsonFetch(`${API}?${qs({ action: serverAction, resource, id })}`, {
      method: 'POST',
      body: action === 'remove' ? {} : { data },
    });
    recordSync(resource, action, {
      id: action === 'create' ? response?.id : id,
      data,
      // Nothing was written: the payload matched the stored row (the server
      // compares NULL / '' / 0 as the same absence). Keep the history clean.
      skip: skipSync || (action === 'update' && response?.unchanged === true),
    });
    return response;
  };

  const DBCrud = {
    _syncRecordEnabled: true,

    invalidateCache() {
      listCache.clear();
    },

    async list(resource, { search = '', limit = 100, offset = 0, signal } = {}) {
      const preview = requestApi();
      if (preview?.active?.()) await preview.ready();
      const key = `list:${resource}:${search}:${limit}:${offset}`;
      const request = listCache.load(
        key,
        () => jsonFetch(`${API}?${qs({ action: 'list', resource, search, limit, offset })}`, { signal }),
        { dedupe: !signal, shouldCache: response => !!response?.ok },
      );
      if (preview?.hasResource?.(resource)) {
        const response = await request;
        return { ...response, rows: preview.overlayRows(resource, response?.rows || []) };
      }
      if (await isGuestMode()) {
        const response = await request;
        return {
          ...response,
          rows: guestApi().Draft.overlayRows(resource, response?.rows || [], { search, offset }),
        };
      }
      return request;
    },

    async renameCraftGroup(target, newGroup, mergeExisting = false) {
      const preview = requestApi();
      if (preview?.active?.()) await preview.ready();
      listCache.clear();
      if (preview?.isEditing?.()) {
        const list = await DBCrud.list('crafts', { search: '', limit: 5000, offset: 0 });
        return preview.renameCraftGroup(list?.rows || [], target, newGroup, mergeExisting);
      }
      assertRequestWritable();
      if (await isGuestMode()) {
        const list = await DBCrud.list('crafts', { search: '', limit: 5000, offset: 0 });
        return guestApi().renameCraftGroup(list?.rows || [], target, newGroup, mergeExisting);
      }
      const response = await jsonFetch(`${API}?${qs({ action: 'rename_group', resource: 'crafts' })}`, {
        method: 'POST',
        body: { ...target, newGroup, mergeExisting },
      });
      const changed = Array.isArray(response?.changed) ? response.changed : [];
      const sync = window.EditorCore?.Sync;
      if (DBCrud._syncRecordEnabled && sync && changed.length) {
        const operations = changed.map(row => ({
          id: row.ID,
          data: { GroupName: row.newGroupName },
          label: `Renaming group #${row.ID}`,
        }));
        if (sync.recordMany) sync.recordMany('crafts', 'update', operations);
        else operations.forEach(operation => sync.record('crafts', 'update', operation));
        sync.updateAllBadges();
      }
      return response;
    },

    async get(resource, id) {
      const preview = requestApi();
      if (preview?.active?.()) await preview.ready();
      if (preview?.hasResource?.(resource)) {
        const op = preview.operation(resource, id);
        if (op?.type === 'create' && Number(id) >= 2000000000) {
          return { ok: true, row: preview.overlayRow(resource, id, null), preview: true };
        }
        let response;
        try {
          response = await rawGet(resource, id);
        } catch (error) {
          const fallback = preview.overlayRow(resource, id, null);
          if (fallback) return { ok: true, row: fallback, preview: true };
          throw error;
        }
        const row = preview.overlayRow(resource, id, response?.row || null);
        return { ...response, ok: row ? true : response?.ok, row, preview: true };
      }
      if (await isGuestMode()) {
        const guest = guestApi();
        if (guest.isTempId(id)) return { ok: true, row: guest.Draft.overlayRow(resource, id, null) };
        const response = await rawGet(resource, id);
        return { ...response, row: guest.Draft.overlayRow(resource, id, response?.row || null) };
      }
      return rawGet(resource, id);
    },

    /**
     * Строка ровно в том виде, в каком она лежит в базе: без черновика гостя и
     * без снимка запроса. Нужна действиям вида «вернуть значение из базы».
     * null, если строка недоступна (нет записи или нет прав на чтение).
     */
    rawRow(resource, id) {
      const numericId = Number(id) || 0;
      if (!resource || numericId <= 0) return Promise.resolve(null);
      return rawGetRow(resource, numericId);
    },

    async create(resource, data, { _skipSyncRecord = false } = {}) {
      if (!resource || !data) throw new Error('Resource and data are required for create');
      return mutate(resource, 'create', {
        data,
        skipSync: _skipSyncRecord,
        guest: async (guest) => {
          await guest.loadConfig();
          return guest.Draft.create(resource, data);
        },
      });
    },

    async update(resource, id, data, { _skipSyncRecord = false } = {}) {
      if (!resource || !id || !data) throw new Error('Resource, id, and data are required for update');
      return mutate(resource, 'update', {
        id,
        data,
        skipSync: _skipSyncRecord,
        guest: async (guest) => {
          const base = guest.isTempId(id) ? null : await rawGetRow(resource, id);
          return guest.Draft.update(resource, id, data, base);
        },
      });
    },

    async remove(resource, id, { _skipSyncRecord = false } = {}) {
      if (!resource || !id) throw new Error('Resource and id are required for remove');
      return mutate(resource, 'remove', {
        id,
        skipSync: _skipSyncRecord,
        guest: async (guest) => {
          const base = guest.isTempId(id) ? null : await rawGetRow(resource, id);
          return guest.Draft.remove(resource, id, base);
        },
      });
    },
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DBCrud = DBCrud;
})();
