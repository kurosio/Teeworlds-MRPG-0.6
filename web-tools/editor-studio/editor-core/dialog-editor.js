(() => {
  /**
   * Форматы реплики — подстановки, которые игровой сервер заменяет в тексте
   * (`src/game/server/core/components/Bots/DialogsData.cpp`):
   *   <bot_12> <item_31> <world_3> — имя записи по её ID;
   *   <player> <time> <here> <eidolon> — данные игрока в момент показа.
   */
  const RECORD_FORMATS = {
    bot:   { source: 'bots',   label: 'Bot',   icon: 'fa-user-ninja', title: 'Insert a bot name',   search: 'Search bots…' },
    item:  { source: 'items',  label: 'Item',  icon: 'fa-cube',       title: 'Insert an item name', search: 'Search items…' },
    world: { source: 'worlds', label: 'World', icon: 'fa-globe',      title: 'Insert a world name', search: 'Search worlds…' },
  };

  const SIMPLE_FORMATS = {
    player:  { icon: 'fa-user',         title: "Insert the player's name" },
    time:    { icon: 'fa-clock',        title: 'Insert the in-game time' },
    here:    { icon: 'fa-location-dot', title: 'Insert the world you are in' },
    eidolon: { icon: 'fa-ghost',        title: "Insert the summoned Eidolon's name" },
  };

  // На сервере текст реплики живёт в char aBufText[1024] — длиннее обрежется.
  const GAME_TEXT_LIMIT = 1024;

  const TOKEN_PATTERN = '<(bot|item|world)_(\\d+)>|<(player|time|here|eidolon)>';

  /** Все форматы в тексте реплики — с позициями, чтобы их можно было менять. */
  const parseTokens = (text) => {
    const value = String(text ?? '');
    const tokens = [];
    const re = new RegExp(TOKEN_PATTERN, 'g');
    let match;
    while ((match = re.exec(value))) {
      tokens.push({
        raw: match[0],
        kind: match[1] || match[3],
        id: match[2] === undefined ? null : Number(match[2]),
        start: match.index,
        end: match.index + match[0].length,
      });
    }
    return tokens;
  };

  /** Длина текста в байтах: сервер считает именно байты, а не символы. */
  const byteLength = (text) => {
    const value = String(text ?? '');
    if (typeof TextEncoder === 'function') {
      try { return new TextEncoder().encode(value).length; } catch { /* portable UTF-8 fallback below */ }
    }
    let bytes = 0;
    for (const char of value) {
      const code = char.codePointAt(0);
      bytes += code <= 0x7f ? 1 : code <= 0x7ff ? 2 : code <= 0xffff ? 3 : 4;
    }
    return bytes;
  };

  // Подписи записей для форматов: <bot_12> → имя бота. Кэш общий на страницу:
  // NPC-редактор и шаг квеста показывают одни и те же записи.
  const recordLabels = new Map();
  const completeRecords = new Set();
  const recordLoads = new Map();
  const text = (value) => String(value ?? '').trim();

  /** Приводим Map / массив / {value,label} к одному виду {value,label}. */
  const normalizeOptions = (input) => {
    const out = [];
    const seen = new Set();
    const add = (value, label) => {
      const id = Number(value);
      if (!Number.isFinite(id) || id < 0 || seen.has(id)) return;
      seen.add(id);
      out.push({ value: id, label: text(label) });
    };
    // Map проверяем по методам, а не instanceof: справочник может прийти из
    // другого документа (оболочка студии держит редактор в iframe).
    const isMap = input && typeof input.forEach === 'function'
      && typeof input.get === 'function' && typeof input.entries === 'function';
    if (isMap) {
      input.forEach((label, value) => add(value, label));
    } else if (Array.isArray(input)) {
      input.forEach((option) => {
        if (option === null || option === undefined) return;
        if (typeof option === 'object') add(option.value ?? option.id, option.label ?? option.name ?? '');
        else add(option, '');
      });
    }
    return out.sort((a, b) => a.value - b.value);
  };

  const rememberRecords = (kind, options, { complete = false } = {}) => {
    if (!RECORD_FORMATS[kind] || !Array.isArray(options) || !options.length) return;
    const labels = recordLabels.get(kind) || new Map();
    options.forEach((option) => {
      const id = Number(option?.value);
      if (!Number.isFinite(id) || id < 0) return;
      const label = text(option?.label);
      if (label) labels.set(id, label);
    });
    recordLabels.set(kind, labels);
    if (complete) completeRecords.add(kind);
  };

  const recordLabel = (kind, id) => recordLabels.get(kind)?.get(Number(id)) || '';
  const recordsComplete = (kind) => completeRecords.has(kind);

  /** Список записей из БД — один запрос на вид формата, с кэшем ядра. */
  const loadRecords = (kind) => {
    const meta = RECORD_FORMATS[kind];
    if (!meta) return Promise.resolve([]);
    if (recordLoads.has(kind)) return recordLoads.get(kind);
    const promise = (async () => {
      const db = window.EditorCore?.DB;
      if (!db?.list) return [];
      const result = await db.list(meta.source, { limit: 5000 });
      const options = normalizeOptions(result?.items);
      rememberRecords(kind, options, { complete: true });
      return options;
    })().catch((error) => {
      // Неудачную загрузку не кэшируем: следующий клик попробует снова.
      recordLoads.delete(kind);
      throw error;
    });
    recordLoads.set(kind, promise);
    return promise;
  };

  const ensureRecords = (kind) => (recordsComplete(kind) ? Promise.resolve([]) : loadRecords(kind));

  // Открытый список записей один на документ: второй mount (переход на другой
  // шаг квеста) не должен оставлять висеть список от прежней формы.
  let closeActivePicker = null;
  let activePickerAnchor = null;

  const DialogEditor = {
    buildSpeakerCandidates(map, extraSpeakers = []) {
      const base = Array.isArray(extraSpeakers)
        ? extraSpeakers.map((opt) => Number(opt?.value)).filter((v) => Number.isFinite(v))
        : [];
      const entries = map instanceof Map
        ? Array.from(map.entries())
          .map(([id]) => Number(id))
          .filter((id) => Number.isFinite(id) && id > 0)
          .sort((a, b) => a - b)
        : [];
      return [...base, ...entries];
    },

    buildSpeakerOptions(map, extraSpeakers = []) {
      const options = [];
      const seen = new Set();
      if (Array.isArray(extraSpeakers)) {
        extraSpeakers.forEach((opt) => {
          const value = Number(opt?.value);
          if (!Number.isFinite(value) || seen.has(value)) return;
          seen.add(value);
          options.push({ value, label: String(opt?.label ?? value) });
        });
      }
      const candidates = DialogEditor.buildSpeakerCandidates(map);
      candidates.forEach((value) => {
        if (value <= 0 || seen.has(value)) return;
        seen.add(value);
        const label = map?.get(String(value)) || '';
        options.push({ value, label: label ? label : `#${value}` });
      });
      return options;
    },

    getSideConfig(side) {
      if (side === 'author') return { left: false, right: false };
      if (side === 'thoughts') return { left: true, right: false };
      return { left: true, right: true };
    },

    pickAlternativeSpeaker(current, candidates) {
      const currentVal = Number(current);
      const list = Array.isArray(candidates) ? candidates : [];
      const found = list.find((value) => Number(value) !== currentVal);
      return found ?? currentVal;
    },

    applyConstraints({
      root,
      data,
      meta = {},
      sideOptions = [],
      speakerCandidates = [],
    } = {}) {
      const items = Array.isArray(data?.DialogData) ? data.DialogData : [];
      let changed = false;

      items.forEach((rawItem, index) => {
        const item = rawItem && typeof rawItem === 'object' ? rawItem : {};
        if (item !== rawItem) {
          items[index] = item;
          changed = true;
        }

        if (Array.isArray(sideOptions) && sideOptions.length) {
          if (!sideOptions.some((opt) => opt?.value === item.side)) {
            item.side = sideOptions[0]?.value ?? 'default';
            changed = true;
          }
        }

        const leftVal = Number(item.left_speaker_id ?? 0);
        const rightVal = Number(item.right_speaker_id ?? 0);
        if (!Number.isFinite(leftVal)) {
          item.left_speaker_id = 0;
          changed = true;
        } else if (item.left_speaker_id !== leftVal) {
          item.left_speaker_id = leftVal;
          changed = true;
        }
        if (!Number.isFinite(rightVal)) {
          item.right_speaker_id = 0;
          changed = true;
        } else if (item.right_speaker_id !== rightVal) {
          item.right_speaker_id = rightVal;
          changed = true;
        }

        const sideConfig = DialogEditor.getSideConfig(item.side);
        const sameSpeakers = sideConfig.left && sideConfig.right
          && Number(item.left_speaker_id) === Number(item.right_speaker_id);
        if (sameSpeakers) {
          const path = String(meta?.path || '');
          const isLeftChanged = path.endsWith(`DialogData.${index}.left_speaker_id`);
          const target = isLeftChanged ? 'right' : 'left';
          const nextValue = DialogEditor.pickAlternativeSpeaker(item.left_speaker_id, speakerCandidates);
          if (target === 'right') item.right_speaker_id = nextValue;
          else item.left_speaker_id = nextValue;
          changed = true;
        }

        if (root) {
          const leftPath = `DialogData.${index}.left_speaker_id`;
          const rightPath = `DialogData.${index}.right_speaker_id`;
          const leftInput = root.querySelector(`[data-path="${leftPath}"]`);
          const rightInput = root.querySelector(`[data-path="${rightPath}"]`);
          const leftCombo = root.querySelector(`.editor-dbcombo[data-bind-input-path="${leftPath}"]`);
          const rightCombo = root.querySelector(`.editor-dbcombo[data-bind-input-path="${rightPath}"]`);
          const applyComboState = (combo, enabled) => {
            if (!combo) return;
            const comboInput = combo.querySelector('.editor-dbcombo-input');
            const clearBtn = combo.querySelector('.editor-dbcombo-clear');
            if (comboInput) comboInput.disabled = !enabled;
            if (clearBtn && !enabled) clearBtn.hidden = true;
            combo.classList.toggle('is-disabled', !enabled);
          };
          if (leftInput) {
            leftInput.disabled = !sideConfig.left;
            if (leftInput.value !== String(item.left_speaker_id ?? '')) {
              leftInput.value = String(item.left_speaker_id ?? '');
              leftInput.dispatchEvent(new Event('input', { bubbles: true }));
              leftInput.dispatchEvent(new Event('change', { bubbles: true }));
            }
          }
          if (rightInput) {
            rightInput.disabled = !sideConfig.right;
            if (rightInput.value !== String(item.right_speaker_id ?? '')) {
              rightInput.value = String(item.right_speaker_id ?? '');
              rightInput.dispatchEvent(new Event('input', { bubbles: true }));
              rightInput.dispatchEvent(new Event('change', { bubbles: true }));
            }
          }
          applyComboState(leftCombo, sideConfig.left);
          applyComboState(rightCombo, sideConfig.right);
        }
      });

      if (changed && data) data.DialogData = items;
    },

    buildFields({
      sideOptions = [],
      extraSpeakers = [],
      speakerOptions = [],
      textPlaceholder = 'Dialogue...',
    } = {}) {
      const speakerList = Array.isArray(speakerOptions) && speakerOptions.length
        ? speakerOptions
        : extraSpeakers;
      return {
        DialogData: {
          type: 'list',
          label: 'Dialogues',
          itemDefault: {
            text: '',
            side: sideOptions?.[0]?.value ?? 'default',
            left_speaker_id: 0,
            right_speaker_id: -1,
            action: false,
          },
          itemFields: {
            text: {
              type: 'textarea',
              label: 'Line text',
              // Подпись не нужна: над полем стоит панель форматов, а сам текст
              // узнаётся по плейсхолдеру — так реплика занимает меньше места.
              // `format` включает многострочное поле: по типу поля FieldRenderer
              // textarea не создаёт.
              ui: { format: 'textarea', rows: 2, placeholder: textPlaceholder, hideLabel: true, colSpan: 4 }
            },
            left_speaker_id: {
              type: 'select',
              label: 'Left speaker',
              ui: {
                type: 'select',
                options: speakerList,
                hideLabel: false,
                controlMinWidth: 140,
              }
            },
            side: {
              type: 'select',
              label: 'Line mode',
              ui: { type: 'select', options: sideOptions, hideLabel: false, controlMinWidth: 120 }
            },
            right_speaker_id: {
              type: 'select',
              label: 'Right speaker',
              ui: {
                type: 'select',
                options: speakerList,
                hideLabel: false,
                controlMinWidth: 140,
              }
            },
            action: { type: 'toggle', label: 'Action' },
          },
          ui: {
            layout: 'grid',
            gridGap: '12px',
            gridTemplate: 'minmax(140px,1fr) minmax(120px,1fr) minmax(140px,1fr) 90px',
            gridTemplateMd: 'minmax(140px,1fr) minmax(120px,1fr) minmax(140px,1fr) 90px',
            addLabel: 'Add a line',
          },
        }
      };
    },

    mount(root, {
      data,
      fieldOptions,
      sideOptions = [],
      extraSpeakers = [],
      speakerOptions = [],
      textPlaceholder = 'Dialogue...',
      formats = {},
      onChange,
    } = {}) {
      const state = { data: data || { DialogData: [] } };
      const fields = DialogEditor.buildFields({
        sideOptions,
        extraSpeakers,
        speakerOptions,
        textPlaceholder,
      });
      const itemFields = fields?.DialogData?.itemFields || {};
      const defaults = fields?.DialogData?.itemDefault || {};
      const esc = window.EditorCore?.utils?.escapeHtml
        || ((v) => String(v ?? '').replace(/[&<>"']/g, (ch) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[ch])));

      // Подписи записей для форматов: <bot_12> → «Имя». Карта общая на все
      // редакторы страницы (NPC и шаг квеста показывают одни и те же записи).
      // `formats` — объект или функция: справочники страницы могут догружаться.
      closeActivePicker?.();
      const formatsOf = () => ((typeof formats === 'function' ? formats() : formats) || {});
      const declared = (kind) => normalizeOptions(formatsOf()?.[kind]);
      Object.keys(RECORD_FORMATS).forEach((kind) => {
        rememberRecords(kind, declared(kind), { complete: true });
      });

      /**
       * Реплики диалога всегда хранятся в том порядке, в котором их видит
       * пользователь. Перестановка (перетаскивание, кнопки ↑/↓, клавиатура)
       * собирает порядок из DOM: индексы «как было» превращаются в новую
       * последовательность элементов, поэтому скрытые поля, индексы в путях
       * (`DialogData.N.*`) и данные формы не могут разъехаться.
       */
      const dialogItems = () => {
        if (!Array.isArray(state.data.DialogData)) state.data.DialogData = [];
        return state.data.DialogData;
      };

      const commitOrder = (order) => {
        const items = dialogItems();
        if (!Array.isArray(order) || order.length !== items.length) return false;
        const valid = order.every((index) => Number.isInteger(index) && index >= 0 && index < items.length);
        if (!valid || new Set(order).size !== items.length) return false;
        if (order.every((index, position) => index === position)) return false;
        const next = order.map((index) => items[index]);
        state.data.DialogData = next;
        if (typeof onChange === 'function') {
          onChange(state.data, { path: 'DialogData', value: next, oldValue: items });
        }
        render();
        return true;
      };

      const moveItem = (from, to) => {
        const items = dialogItems();
        const start = Number(from);
        if (!Number.isInteger(start) || start < 0 || start >= items.length) return false;
        const target = Math.min(Math.max(Number(to) || 0, 0), items.length - 1);
        if (target === start) return false;
        const order = items.map((_, index) => index);
        const [moved] = order.splice(start, 1);
        order.splice(target, 0, moved);
        if (!commitOrder(order)) return false;
        // Перерисовка заменяет узлы: фокус возвращаем на рукоятку реплики,
        // иначе клавиатура «теряет» строку после первого нажатия.
        const nextHandle = root?.querySelector(`[data-dialog-handle="${target}"]`);
        try { nextHandle?.focus({ preventScroll: true }); } catch { nextHandle?.focus?.(); }
        return true;
      };

      /**
       * Перетаскивание живёт в самом модуле, без Sortable с CDN: страницы
       * редактора работают и без интернета, а на странице NPC внешняя
       * библиотека не подключена вовсе — из-за этого реплики не двигались.
       */
      const startDrag = (event, itemEl, listEl) => {
        if (!listEl || !itemEl || !listEl.isConnected) return;
        if (event.pointerType === 'mouse' && event.button !== 0) return;
        const startY = event.clientY;
        let active = false;
        let finished = false;

        const onMove = (ev) => {
          if (finished || !listEl.isConnected) return;
          if (!active) {
            if (Math.abs(ev.clientY - startY) < 4) return;
            active = true;
            itemEl.classList.add('is-dragging');
            document.body.classList.add('is-dialog-dragging');
          }
          if (ev.cancelable) ev.preventDefault();
          const siblings = Array.from(listEl.querySelectorAll('[data-dialog-item]'))
            .filter((el) => el !== itemEl);
          const before = siblings.find((el) => {
            const rect = el.getBoundingClientRect();
            return ev.clientY < rect.top + rect.height / 2;
          });
          if (before) {
            if (itemEl.nextElementSibling !== before) listEl.insertBefore(itemEl, before);
          } else if (itemEl.nextElementSibling) {
            listEl.appendChild(itemEl);
          }
        };

        const finish = () => {
          if (finished) return;
          finished = true;
          window.removeEventListener('pointermove', onMove);
          window.removeEventListener('pointerup', finish);
          window.removeEventListener('pointercancel', finish);
          document.body.classList.remove('is-dialog-dragging');
          itemEl.classList.remove('is-dragging');
          if (!active || !listEl.isConnected) return;
          active = false;
          const order = Array.from(listEl.querySelectorAll('[data-dialog-item]'))
            .map((el) => Number(el.getAttribute('data-dialog-item')));
          commitOrder(order);
        };

        window.addEventListener('pointermove', onMove, { passive: false });
        window.addEventListener('pointerup', finish);
        window.addEventListener('pointercancel', finish);
      };

      // ── Всплывающий выбор записи для формата ──────────────────────────────
      // Одна панель на редактор: создаётся при открытии и удаляется при
      // закрытии, поэтому перерисовки и повторные mount не оставляют мусора.
      let closePicker = null;
      const closeRecordPicker = () => { closePicker?.(); closePicker = null; };

      const openRecordPicker = ({ kind, anchor, current = null, onPick }) => {
        const meta = RECORD_FORMATS[kind];
        if (!meta || !anchor || !document?.body) return;
        // Повторный клик по той же кнопке закрывает список, а не открывает заново.
        if (closeActivePicker && activePickerAnchor === anchor) {
          closeActivePicker();
          return;
        }
        closeRecordPicker();
        closeActivePicker?.();

        const box = document.createElement('div');
        box.className = 'dlg-picker';
        box.setAttribute('role', 'dialog');
        box.setAttribute('aria-label', meta.title);
        box.innerHTML = `
          <div class="dlg-picker-head">
            <i class="fa-solid fa-magnifying-glass" aria-hidden="true"></i>
            <input type="search" class="dlg-picker-search" autocomplete="off"
                   placeholder="${esc(meta.search)}" aria-label="${esc(meta.search)}">
          </div>
          <div class="dlg-picker-list" role="listbox" aria-label="${esc(meta.label)}"></div>
          <div class="dlg-picker-foot">${current === null ? '' : `
            <button type="button" class="dlg-picker-remove" data-dialog-picker-remove>
              <i class="fa-solid fa-trash" aria-hidden="true"></i><span>Remove the format</span>
            </button>`}
          </div>`;

        const listEl = box.querySelector('.dlg-picker-list');
        const searchEl = box.querySelector('.dlg-picker-search');
        let options = [];
        let visible = [];
        let active = 0;
        let closed = false;

        const place = () => {
          if (closed) return;
          const rect = anchor.getBoundingClientRect();
          const width = Math.max(220, Math.min(340, window.innerWidth - 16));
          box.style.width = `${width}px`;
          box.style.visibility = 'hidden';
          const height = box.offsetHeight || 300;
          let left = rect.left;
          if (left + width > window.innerWidth - 8) left = window.innerWidth - width - 8;
          let top = rect.bottom + 6;
          if (top + height > window.innerHeight - 8) {
            const above = rect.top - 6 - height;
            top = above > 8 ? above : Math.max(8, window.innerHeight - height - 8);
          }
          box.style.left = `${Math.max(8, left)}px`;
          box.style.top = `${Math.max(8, top)}px`;
          box.style.visibility = '';
        };

        const close = () => {
          if (closed) return;
          closed = true;
          document.removeEventListener('pointerdown', onOutside, true);
          document.removeEventListener('keydown', onKey, true);
          window.removeEventListener('resize', place);
          window.removeEventListener('scroll', onScroll, true);
          box.remove();
          closePicker = null;
          if (closeActivePicker === close) closeActivePicker = null;
          if (activePickerAnchor === anchor) activePickerAnchor = null;
        };

        const pick = (option) => {
          if (!option) return;
          close();
          onPick(String(option.value));
        };

        const optionHtml = (option, index) => {
          const label = option.label
            ? `<span class="dlg-picker-label">${esc(option.label)}</span>`
            : `<span class="dlg-picker-label">Use ID ${esc(String(option.value))}</span>`;
          return `<button type="button" class="dlg-picker-option${index === active ? ' is-active' : ''}${Number(option.value) === Number(current) ? ' is-current' : ''}"
                          role="option" aria-selected="${index === active ? 'true' : 'false'}"
                          data-picker-index="${index}">
                    <span class="dlg-picker-id">#${esc(String(option.value))}</span>${label}
                  </button>`;
        };

        const renderList = () => {
          if (closed) return;
          if (!visible.length) {
            listEl.innerHTML = '<div class="editor-dbcombo-empty">Nothing found</div>';
            return;
          }
          listEl.innerHTML = visible.slice(0, 300).map(optionHtml).join('');
        };

        const applyFilter = () => {
          const query = String(searchEl.value || '').trim().toLowerCase();
          active = 0;
          if (!query) {
            visible = options.slice();
          } else {
            visible = options.filter((option) => String(option.label || '').toLowerCase().includes(query)
              || String(option.value) === query);
            // Число в поиске — это всегда готовый ID, даже если записи нет в списке.
            if (/^\d+$/.test(query) && !visible.some((option) => String(option.value) === query)) {
              visible = [{ value: Number(query), label: '' }, ...visible];
            }
          }
          renderList();
        };

        const onKey = (event) => {
          if (event.key === 'Escape') {
            event.preventDefault();
            event.stopPropagation();
            close();
            return;
          }
          if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
            event.preventDefault();
            if (!visible.length) return;
            const step = event.key === 'ArrowDown' ? 1 : -1;
            active = (active + step + visible.length) % visible.length;
            renderList();
            listEl.querySelector('.is-active')?.scrollIntoView?.({ block: 'nearest' });
            return;
          }
          if (event.key === 'Enter') {
            event.preventDefault();
            pick(visible[active]);
          }
        };

        const onOutside = (event) => {
          if (box.contains(event.target) || anchor.contains(event.target)) return;
          close();
        };

        const onScroll = (event) => {
          if (box.contains(event.target)) return;
          close();
        };

        box.addEventListener('click', (event) => {
          const optionBtn = event.target.closest('[data-picker-index]');
          if (optionBtn) {
            pick(visible[Number(optionBtn.getAttribute('data-picker-index'))]);
            return;
          }
          if (event.target.closest('[data-dialog-picker-remove]')) {
            close();
            onPick(null);
          }
        });
        searchEl.addEventListener('input', applyFilter);

        const load = async () => {
          listEl.innerHTML = '<div class="editor-dbcombo-empty">Loading…</div>';
          try {
            const local = declared(kind);
            options = local.length ? local : await loadRecords(kind);
            if (closed) return;
            rememberRecords(kind, options, { complete: true });
            refreshTokens();
          } catch {
            if (closed) return;
            options = [];
            listEl.innerHTML = '<div class="editor-dbcombo-empty">Could not load the list — type an ID</div>';
            searchEl.focus({ preventScroll: true });
            return;
          }
          applyFilter();
          place();
          searchEl.focus({ preventScroll: true });
        };

        document.body.appendChild(box);
        place();
        document.addEventListener('pointerdown', onOutside, true);
        document.addEventListener('keydown', onKey, true);
        window.addEventListener('resize', place);
        window.addEventListener('scroll', onScroll, true);
        closePicker = close;
        closeActivePicker = close;
        activePickerAnchor = anchor;
        load();
      };

      // Перерисовка списка заменяет узлы форматов — обновляем их по ссылкам.
      let refreshTokens = () => {};

      const render = () => {
        if (!root) return;
        closeRecordPicker();
        const list = Array.isArray(state.data.DialogData) ? state.data.DialogData : [];

        const renderField = window.EditorCore?.FieldRenderer?.renderField;
        const frOptions = fieldOptions || window.EditorCore?.defaults?.fieldOptions;
        if (!renderField) {
          root.innerHTML = '<div class="editor-muted-text text-sm">FieldRenderer is not loaded</div>';
          return;
        }

        const renderItemField = (index, key) => renderField({
          path: `DialogData.${index}.${key}`,
          field: itemFields[key],
          data: state.data,
          options: frOptions,
          isNested: true,
        });

        const recordButtons = Object.entries(RECORD_FORMATS).map(([kind, meta]) => `
          <button type="button" class="dlg-toolbar-btn" data-dialog-insert="${kind}" title="${esc(meta.title)}">
            <i class="fa-solid ${meta.icon}" aria-hidden="true"></i><span>${esc(meta.label)}</span>
          </button>`).join('');

        const simpleButtons = Object.entries(SIMPLE_FORMATS).map(([kind, meta]) => `
          <button type="button" class="dlg-toolbar-btn" data-dialog-insert="${kind}" title="${esc(meta.title)}">
            <i class="fa-solid ${meta.icon}" aria-hidden="true"></i><code>&lt;${kind}&gt;</code>
          </button>`).join('');

        const itemsHtml = list.map((item, index) => {
          const lineText = String(item?.text || '').trim();
          const flat = lineText.replace(/\s+/g, ' ');
          const summary = flat ? (flat.length > 70 ? `${flat.slice(0, 70)}…` : flat) : 'Empty line';
          return `
            <div class="editor-item" data-dialog-item="${index}">
              <div class="editor-item-header">
                <div class="flex items-center gap-2 min-w-0">
                  <span class="editor-badge">#${index + 1}</span>
                  <span class="editor-item-summary truncate">${esc(summary)}</span>
                </div>
                <div class="editor-item-actions">
                  <span class="editor-item-handle" tabindex="0" title="Drag the line"
                        data-dialog-handle="${index}"><i class="fa-solid fa-grip-vertical"></i></span>
                  <button type="button" class="editor-icon-btn" title="Move the line up"
                          data-dialog-move="up" data-dialog-move-index="${index}"${index === 0 ? ' disabled' : ''}>
                    <i class="fa-solid fa-arrow-up"></i>
                  </button>
                  <button type="button" class="editor-icon-btn" title="Move the line down"
                          data-dialog-move="down" data-dialog-move-index="${index}"${index === list.length - 1 ? ' disabled' : ''}>
                    <i class="fa-solid fa-arrow-down"></i>
                  </button>
                  <button type="button" class="editor-icon-btn" title="Duplicate the line" data-dialog-copy="${index}">
                    <i class="fa-regular fa-clone"></i>
                  </button>
                  <button type="button" class="editor-icon-btn" title="Collapse or expand" data-dialog-toggle="${index}">
                    <i class="fa-solid fa-chevron-down"></i>
                  </button>
                  <button type="button" class="editor-icon-btn editor-icon-danger" title="Delete the line" data-dialog-remove="${index}">
                    <i class="fa-solid fa-trash"></i>
                  </button>
                </div>
              </div>
              <div class="editor-item-body">
                <div class="dlg-line" data-dialog-line="${index}">
                  <div class="dlg-toolbar" role="toolbar" aria-label="Insert a format" data-dialog-toolbar="${index}">
                    ${recordButtons}
                    <span class="dlg-toolbar-sep" aria-hidden="true"></span>
                    ${simpleButtons}
                    <span class="dlg-counter" data-dialog-counter="${index}"
                          title="Line length in bytes (game limit 1024)"></span>
                  </div>
                  ${renderItemField(index, 'text')}
                  <div class="dlg-tokens" data-dialog-tokens="${index}"></div>
                </div>
                <div class="editor-item-grid">
                  ${renderItemField(index, 'left_speaker_id')}
                  ${renderItemField(index, 'side')}
                  ${renderItemField(index, 'right_speaker_id')}
                  ${renderItemField(index, 'action')}
                </div>
              </div>
            </div>
          `;
        }).join('') || '<div class="editor-empty-state">No lines yet — click “Add line”.</div>';

        root.innerHTML = `
          <div class="editor-items" data-dialog-items="list">${itemsHtml}</div>
          <button type="button" class="editor-btn editor-btn-secondary mt-3" data-dialog-add="1">
            <i class="fa-solid fa-plus"></i><span>Add a line</span>
          </button>
        `;

        window.EditorCore?.UIManager?.init(root);

        const listEl = root.querySelector('[data-dialog-items="list"]');
        if (listEl) {
          listEl.querySelectorAll('[data-dialog-item]').forEach((itemEl) => {
            const handle = itemEl.querySelector('[data-dialog-handle]');
            if (!handle) return;
            handle.addEventListener('pointerdown', (event) => startDrag(event, itemEl, listEl));
            handle.addEventListener('keydown', (event) => {
              const index = Number(handle.getAttribute('data-dialog-handle'));
              if (event.key === 'ArrowUp') {
                if (moveItem(index, index - 1)) event.preventDefault();
              } else if (event.key === 'ArrowDown') {
                if (moveItem(index, index + 1)) event.preventDefault();
              }
            });
          });
        }

        root.querySelectorAll('[data-dialog-move]').forEach((btn) => {
          btn.addEventListener('click', () => {
            const index = Number(btn.getAttribute('data-dialog-move-index'));
            const direction = btn.getAttribute('data-dialog-move');
            moveItem(index, direction === 'up' ? index - 1 : index + 1);
          });
        });

        // ── Форматы реплики: панель над полем и чипы под ним ────────────────
        const tokenSyncs = [];
        root.querySelectorAll('[data-dialog-item]').forEach((itemEl) => {
          const index = Number(itemEl.getAttribute('data-dialog-item'));
          // Поле текста: textarea или input — зависит от FieldRenderer страницы.
          const textarea = itemEl.querySelector(`[data-path="DialogData.${index}.text"]`);
          const tokensEl = itemEl.querySelector(`[data-dialog-tokens="${index}"]`);
          const counterEl = itemEl.querySelector(`[data-dialog-counter="${index}"]`);
          const toolbarEl = itemEl.querySelector(`[data-dialog-toolbar="${index}"]`);
          if (!textarea || !tokensEl) return;

          // Каретка теряется при клике по кнопке панели — запоминаем последнюю.
          let caret = null;
          const rememberCaret = () => {
            const start = Number(textarea.selectionStart);
            const end = Number(textarea.selectionEnd);
            if (Number.isInteger(start)) caret = { start, end: Number.isInteger(end) ? end : start };
          };
          ['click', 'keyup', 'select', 'focus', 'blur'].forEach((name) => {
            textarea.addEventListener(name, rememberCaret);
          });

          const emit = () => textarea.dispatchEvent(new Event('input', { bubbles: true }));

          const insertAtCaret = (insert) => {
            const value = textarea.value;
            const position = caret || { start: value.length, end: value.length };
            const start = Math.min(Math.max(position.start, 0), value.length);
            const end = Math.min(Math.max(position.end, start), value.length);
            textarea.value = value.slice(0, start) + insert + value.slice(end);
            const next = start + insert.length;
            caret = { start: next, end: next };
            try { textarea.setSelectionRange(next, next); } catch { /* не все браузеры */ }
            emit();
            textarea.focus();
          };

          const tokenHtml = (token) => {
            const meta = RECORD_FORMATS[token.kind];
            const position = `data-token-start="${token.start}" data-token-end="${token.end}" data-token-raw="${esc(token.raw)}" data-token-kind="${token.kind}"`;
            const removeBtn = `<button type="button" class="dlg-token-x" data-dialog-token-remove title="Remove the format">
                                 <i class="fa-solid fa-xmark" aria-hidden="true"></i>
                               </button>`;
            if (!meta) {
              const simple = SIMPLE_FORMATS[token.kind] || {};
              return `
                <span class="dlg-token" data-dialog-token ${position} title="${esc(simple.title || '')}">
                  <i class="fa-solid ${simple.icon || 'fa-code'}" aria-hidden="true"></i>
                  <span class="dlg-token-label dlg-token-code">${esc(token.raw)}</span>
                  ${removeBtn}
                </span>`;
            }
            const label = recordLabel(token.kind, token.id);
            const invalid = !label && recordsComplete(token.kind);
            const title = invalid ? `Unknown record #${token.id}` : 'Change the record';
            // Имя записи показываем, когда оно известно; иначе — только номер.
            const labelHtml = label
              ? `<span class="dlg-token-label">${esc(label)}</span><span class="dlg-token-id">#${token.id}</span>`
              : `<span class="dlg-token-label dlg-token-id">#${token.id}</span>`;
            return `
              <span class="dlg-token${invalid ? ' is-invalid' : ''}" data-dialog-token ${position} data-token-id="${token.id}">
                <button type="button" class="dlg-token-main" data-dialog-token-edit title="${esc(title)}">
                  <i class="fa-solid ${meta.icon}" aria-hidden="true"></i>
                  ${labelHtml}
                </button>
                <button type="button" class="dlg-token-x" data-dialog-token-open
                  title="Edit related record" aria-label="Edit related record">
                  <i class="fa-solid fa-arrow-up-right-from-square" aria-hidden="true"></i>
                </button>
                ${removeBtn}
              </span>`;
          };

          const syncFormats = () => {
            tokensEl.innerHTML = parseTokens(textarea.value).map(tokenHtml).join('');
            if (counterEl) {
              const bytes = byteLength(textarea.value);
              counterEl.textContent = `${bytes}/${GAME_TEXT_LIMIT}`;
              counterEl.classList.toggle('is-over', bytes > GAME_TEXT_LIMIT);
            }
          };
          tokenSyncs.push(syncFormats);

          // Позиции чипа сверяем с текущим текстом: если поле успели
          // поправить руками, ищем формат заново по его виду.
          const locate = (chip) => {
            const raw = chip.getAttribute('data-token-raw') || '';
            const value = textarea.value;
            const start = Number(chip.getAttribute('data-token-start'));
            const end = Number(chip.getAttribute('data-token-end'));
            if (Number.isInteger(start) && value.slice(start, end) === raw) return { raw, start, end };
            const found = raw ? value.indexOf(raw) : -1;
            return found < 0 ? null : { raw, start: found, end: found + raw.length };
          };

          const replaceToken = (token, next) => {
            if (!token) return;
            const value = textarea.value;
            textarea.value = value.slice(0, token.start) + next + value.slice(token.end);
            const position = token.start + next.length;
            caret = { start: position, end: position };
            try { textarea.setSelectionRange(position, position); } catch { /* не все браузеры */ }
            emit();
          };

          tokensEl.addEventListener('click', (event) => {
            const chip = event.target.closest('[data-dialog-token]');
            if (!chip) return;
            const token = locate(chip);
            if (!token) return;
            const openButton = event.target.closest('[data-dialog-token-open]');
            if (openButton) {
              const source = RECORD_FORMATS[chip.dataset.tokenKind]?.source;
              window.EditorCore.RelatedRecords?.open(source, Number(chip.dataset.tokenId), openButton);
              return;
            }
            if (event.target.closest('[data-dialog-token-remove]')) {
              replaceToken(token, '');
              textarea.focus();
              return;
            }
            const editBtn = event.target.closest('[data-dialog-token-edit]');
            const kind = chip.getAttribute('data-token-kind');
            if (!editBtn || !RECORD_FORMATS[kind]) return;
            const current = Number(chip.getAttribute('data-token-id'));
            openRecordPicker({
              kind,
              anchor: editBtn,
              current,
              onPick: (value) => {
                if (value === null) replaceToken(locate(chip) || token, '');
                else replaceToken(locate(chip) || token, `<${kind}_${value}>`);
                textarea.focus();
              },
            });
          });

          toolbarEl?.querySelectorAll('[data-dialog-insert]').forEach((btn) => {
            btn.addEventListener('click', () => {
              const kind = btn.getAttribute('data-dialog-insert');
              if (RECORD_FORMATS[kind]) {
                openRecordPicker({
                  kind,
                  anchor: btn,
                  current: null,
                  onPick: (value) => insertAtCaret(`<${kind}_${value}>`),
                });
                return;
              }
              if (SIMPLE_FORMATS[kind]) insertAtCaret(`<${kind}>`);
            });
          });

          textarea.addEventListener('input', syncFormats);
          syncFormats();
        });
        refreshTokens = () => tokenSyncs.forEach((sync) => sync());

        // Подписи записей могут прийти позже (БД, справочники) — показываем
        // номера сразу, а имена подставляем, как только они известны.
        root.querySelectorAll('[data-dialog-token][data-token-id]').forEach((chip) => {
          const kind = chip.getAttribute('data-token-kind');
          if (recordLabel(kind, Number(chip.getAttribute('data-token-id'))) || recordsComplete(kind)) return;
          ensureRecords(kind).then(() => refreshTokens()).catch(() => { /* без подписей */ });
        });

        root.querySelectorAll('[data-dialog-remove]').forEach((btn) => {
          btn.addEventListener('click', () => {
            const idx = Number(btn.getAttribute('data-dialog-remove'));
            const items = dialogItems();
            if (!Number.isFinite(idx) || idx < 0 || idx >= items.length) return;
            items.splice(idx, 1);
            state.data.DialogData = items;
            if (typeof onChange === 'function') {
              onChange(state.data, { path: 'DialogData', value: items });
            }
            render();
          });
        });

        root.querySelectorAll('[data-dialog-copy]').forEach((btn) => {
          btn.addEventListener('click', () => {
            const idx = Number(btn.getAttribute('data-dialog-copy'));
            const items = dialogItems();
            if (!Number.isFinite(idx) || idx < 0 || idx >= items.length) return;
            const copy = JSON.parse(JSON.stringify(items[idx] ?? defaults));
            items.splice(idx + 1, 0, copy);
            state.data.DialogData = items;
            if (typeof onChange === 'function') {
              onChange(state.data, { path: 'DialogData', value: items });
            }
            render();
          });
        });

        root.querySelectorAll('[data-dialog-toggle]').forEach((btn) => {
          btn.addEventListener('click', () => {
            const idx = Number(btn.getAttribute('data-dialog-toggle'));
            const item = root.querySelector(`[data-dialog-item="${idx}"]`);
            if (item) item.classList.toggle('is-collapsed');
          });
        });

        root.querySelectorAll('[data-path]').forEach((input) => {
          const handler = () => {
            const path = input.getAttribute('data-path');
            if (!path) return;
            const val = window.EditorCore?.FieldRenderer?.getInputValue
              ? window.EditorCore.FieldRenderer.getInputValue(input)
              : input.value;
            if (window.EditorCore?.FieldRenderer?.setValueAtPath) {
              window.EditorCore.FieldRenderer.setValueAtPath(state.data, path, val);
            }
            if (input.type === 'checkbox' && input.closest('.editor-toggle')) {
              input.closest('.editor-toggle')?.setAttribute('data-checked', input.checked ? '1' : '0');
            }
            const textMatch = path.match(/^DialogData\.(\d+)\.text$/);
            if (textMatch) {
              const index = Number(textMatch[1]);
              const item = state.data.DialogData?.[index];
              const lineText = String(item?.text || '').trim();
              const summary = lineText ? (lineText.length > 70 ? `${lineText.slice(0, 70)}…` : lineText) : '—';
              const summaryEl = root.querySelector(`[data-dialog-item="${index}"] .editor-item-summary`);
              if (summaryEl) summaryEl.textContent = summary;
            }
            if (typeof onChange === 'function') onChange(state.data, { path, value: val });
          };
          input.addEventListener('input', handler);
          input.addEventListener('change', handler);
        });

        const addBtn = root.querySelector('[data-dialog-add]');
        addBtn?.addEventListener('click', () => {
          const items = dialogItems();
          items.push(JSON.parse(JSON.stringify(defaults)));
          state.data.DialogData = items;
          if (typeof onChange === 'function') {
            onChange(state.data, { path: 'DialogData', value: items });
          }
          render();
        });
      };

      const api = {
        render,
        getData: () => state.data,
        setData: (next) => {
          state.data = next || { DialogData: [] };
          render();
        },
        destroy: () => { closeRecordPicker(); },
      };

      render();
      return api;
    },
  };

  // Разбор форматов нужен и снаружи (проверки, отчёты по текстам реплик).
  DialogEditor.parseTokens = parseTokens;

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DialogEditor = DialogEditor;
})();
