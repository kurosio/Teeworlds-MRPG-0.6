(() => {
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
            const comboClear = combo.querySelector('.editor-dbcombo-clear');
            if (comboInput) comboInput.disabled = !enabled;
            if (comboClear) comboClear.disabled = !enabled;
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
      textPlaceholder = 'Диалог...',
    } = {}) {
      const speakerList = Array.isArray(speakerOptions) && speakerOptions.length
        ? speakerOptions
        : extraSpeakers;
      return {
        DialogData: {
          type: 'list',
          label: 'Диалоги',
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
              label: 'Текст',
              ui: { rows: 2, placeholder: textPlaceholder, hideLabel: true, colSpan: 4 }
            },
            left_speaker_id: {
              type: 'select',
              label: 'Left',
              ui: {
                type: 'select',
                options: speakerList,
                hideLabel: true,
                controlMinWidth: 140,
              }
            },
            side: {
              type: 'select',
              label: 'Side',
              ui: { type: 'select', options: sideOptions, hideLabel: true, controlMinWidth: 120 }
            },
            right_speaker_id: {
              type: 'select',
              label: 'Right',
              ui: {
                type: 'select',
                options: speakerList,
                hideLabel: true,
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
            addLabel: 'Добавить реплику',
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
      textPlaceholder = 'Диалог...',
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

      const render = () => {
        if (!root) return;
        const list = Array.isArray(state.data.DialogData) ? state.data.DialogData : [];

        const renderField = window.EditorCore?.FieldRenderer?.renderField;
        const frOptions = fieldOptions || window.EditorCore?.defaults?.fieldOptions;
        if (!renderField) {
          root.innerHTML = '<div class="editor-muted-text text-sm">FieldRenderer не загружен</div>';
          return;
        }

        const renderItemField = (index, key) => renderField({
          path: `DialogData.${index}.${key}`,
          field: itemFields[key],
          data: state.data,
          options: frOptions,
          isNested: true,
        });

        const itemsHtml = list.map((item, index) => {
          const text = String(item?.text || '').trim();
          const summary = text ? (text.length > 70 ? `${text.slice(0, 70)}…` : text) : '—';
          return `
            <div class="task-item" data-dialog-item="${index}">
              <div class="task-item-header">
                <div>
                  <span class="editor-badge">#${index + 1}</span>
                  <span class="task-item-summary">${summary}</span>
                </div>
                <div class="task-item-actions">
                  <span class="task-item-handle"><i class="fa-solid fa-arrows-up-down-left-right"></i></span>
                  <button type="button" class="editor-btn editor-btn-secondary" data-dialog-toggle="${index}">
                    <i class="fa-solid fa-chevron-down"></i>
                  </button>
                  <button type="button" class="editor-btn editor-btn-secondary" data-dialog-remove="${index}">
                    <i class="fa-solid fa-trash"></i>
                  </button>
                </div>
              </div>
              <div class="task-item-body">
                <div>${renderItemField(index, 'text')}</div>
                <div class="task-item-grid">
                  ${renderItemField(index, 'left_speaker_id')}
                  ${renderItemField(index, 'side')}
                  ${renderItemField(index, 'right_speaker_id')}
                  ${renderItemField(index, 'action')}
                </div>
              </div>
            </div>
          `;
        }).join('') || '<div class="editor-muted-text text-sm">Нет элементов</div>';

        root.innerHTML = `
          <div class="task-items" data-dialog-items="list">${itemsHtml}</div>
          <button type="button" class="editor-btn editor-btn-secondary mt-3" data-dialog-add="1">
            <i class="fa-solid fa-plus"></i><span>Добавить реплику</span>
          </button>
        `;

        window.EditorCore?.UIManager?.init(root);

        const listEl = root.querySelector('[data-dialog-items="list"]');
        if (window.Sortable && listEl) {
          if (state.sortable) state.sortable.destroy();
          state.sortable = new Sortable(listEl, {
            handle: '.task-item-handle',
            animation: 150,
            onEnd: (evt) => {
              const items = Array.isArray(state.data.DialogData) ? state.data.DialogData : [];
              const [moved] = items.splice(evt.oldIndex, 1);
              items.splice(evt.newIndex, 0, moved);
              state.data.DialogData = items;
              if (typeof onChange === 'function') {
                onChange(state.data, { path: 'DialogData', value: items });
              }
              render();
            }
          });
        }

        root.querySelectorAll('[data-dialog-remove]').forEach((btn) => {
          btn.addEventListener('click', () => {
            const idx = Number(btn.getAttribute('data-dialog-remove'));
            const items = Array.isArray(state.data.DialogData) ? state.data.DialogData : [];
            if (!Number.isFinite(idx) || idx < 0 || idx >= items.length) return;
            items.splice(idx, 1);
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
              const text = String(item?.text || '').trim();
              const summary = text ? (text.length > 70 ? `${text.slice(0, 70)}…` : text) : '—';
              const summaryEl = root.querySelector(`[data-dialog-item="${index}"] .task-item-summary`);
              if (summaryEl) summaryEl.textContent = summary;
            }
            if (typeof onChange === 'function') onChange(state.data, { path, value: val });
          };
          input.addEventListener('input', handler);
          input.addEventListener('change', handler);
        });

        const addBtn = root.querySelector('[data-dialog-add]');
        addBtn?.addEventListener('click', () => {
          const items = Array.isArray(state.data.DialogData) ? state.data.DialogData : [];
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
        }
      };

      render();
      return api;
    },
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DialogEditor = DialogEditor;
})();
