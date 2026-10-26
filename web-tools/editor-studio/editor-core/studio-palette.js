// Command palette (Ctrl+K) for the Studio shell: fuzzy search across editors
// and shell actions, fully keyboard-driven. The matching helpers are pure and
// exported (`filter`, `scoreItem`) so they can be unit-tested without a DOM.

(function () {
  if (typeof window === 'undefined') return;

  const normalize = (value) => String(value || '').toLowerCase().trim();

  /** How well one text matches one search term. -1 = no match. */
  const scoreText = (text, term) => {
    const t = normalize(text);
    const q = normalize(term);
    if (!t || !q) return -1;
    if (t === q) return 100;
    if (t.startsWith(q)) return 82 - Math.min(12, t.length - q.length);
    if (t.split(/[\s\-_/]+/).some((word) => word.startsWith(q))) return 62;
    const idx = t.indexOf(q);
    if (idx >= 0) return 42 - Math.min(16, idx);
    // Subsequence: "itm" finds "Items".
    let i = 0;
    for (const ch of t) {
      if (ch === q[i]) i++;
      if (i === q.length) return 14;
    }
    return -1;
  };

  const keywordsOf = (item) => {
    if (Array.isArray(item?.keywords)) return item.keywords;
    return String(item?.keywords || '').split(/\s+/).filter(Boolean);
  };

  /** Score of one palette item for a query (all words must match). -1 = hide. */
  const scoreItem = (item, query) => {
    const terms = normalize(query).split(/\s+/).filter(Boolean);
    if (!terms.length) return 0;
    let total = 0;
    for (const term of terms) {
      const fields = [
        scoreText(item?.title, term) >= 0 ? scoreText(item?.title, term) + 24 : -1,
        ...keywordsOf(item).map((word) => (scoreText(word, term) >= 0 ? scoreText(word, term) + 6 : -1)),
        scoreText(item?.hint, term) >= 0 ? scoreText(item.hint, term) - 8 : -1,
        scoreText(item?.group, term) >= 0 ? scoreText(item.group, term) - 14 : -1,
      ];
      const best = Math.max(...fields);
      if (best < 0) return -1;
      total += best;
    }
    return total;
  };

  /** Filter + rank items for a query, keeping the original order on ties. */
  const filterItems = (items, query, limit = 30) => (items || [])
    .map((item, index) => ({ item, index, score: scoreItem(item, query) }))
    .filter((entry) => entry.score >= 0)
    .sort((a, b) => (b.score - a.score) || (a.index - b.index))
    .slice(0, limit)
    .map((entry) => entry.item);

  // ── DOM ───────────────────────────────────────────────────────────────────
  let state = null;

  const close = () => {
    if (!state) return;
    state.backdrop.remove();
    document.removeEventListener('keydown', state.onKeydown, true);
    const restore = state.restoreFocus;
    state = null;
    try { restore?.focus?.(); } catch { /* element may be gone */ }
  };

  const isOpen = () => !!state;

  const open = ({
    items = [],
    onPick = null,
    placeholder = 'Search editors and actions…',
    emptyText = 'Nothing found. Try a different query.',
  } = {}) => {
    if (typeof document === 'undefined' || !document.body) return;
    close();

    const backdrop = document.createElement('div');
    backdrop.className = 'studio-palette-backdrop';
    const panel = document.createElement('div');
    panel.className = 'studio-palette';
    panel.setAttribute('role', 'dialog');
    panel.setAttribute('aria-modal', 'true');
    panel.setAttribute('aria-label', 'Command palette');

    const inputRow = document.createElement('div');
    inputRow.className = 'studio-palette-input';
    const icon = document.createElement('i');
    icon.className = 'fa-solid fa-magnifying-glass';
    const input = document.createElement('input');
    input.type = 'search';
    input.placeholder = placeholder;
    input.setAttribute('role', 'combobox');
    input.setAttribute('aria-expanded', 'true');
    input.setAttribute('aria-controls', 'studio-palette-list');
    input.setAttribute('aria-label', placeholder);
    input.autocomplete = 'off';
    input.spellcheck = false;
    const escHint = document.createElement('span');
    escHint.className = 'studio-kbd';
    escHint.textContent = 'Esc';
    inputRow.append(icon, input, escHint);

    const list = document.createElement('div');
    list.className = 'studio-palette-list';
    list.id = 'studio-palette-list';
    list.setAttribute('role', 'listbox');

    const foot = document.createElement('div');
    foot.className = 'studio-palette-foot';
    foot.innerHTML = '<span><span class="studio-kbd">↑↓</span> navigate</span>'
      + '<span><span class="studio-kbd">Enter</span> open</span>'
      + '<span><span class="studio-kbd">Esc</span> close</span>';

    panel.append(inputRow, list, foot);
    backdrop.appendChild(panel);

    state = {
      backdrop, input, list, items, onPick, emptyText,
      filtered: [], selected: 0,
      restoreFocus: document.activeElement || null,
      onKeydown: null,
    };

    const pick = (item) => {
      if (!item) return;
      const callback = state?.onPick;
      close();
      callback?.(item);
    };

    const render = () => {
      if (!state) return;
      state.filtered = filterItems(state.items, state.input.value);
      if (state.selected >= state.filtered.length) state.selected = Math.max(0, state.filtered.length - 1);
      list.innerHTML = '';
      if (!state.filtered.length) {
        const empty = document.createElement('div');
        empty.className = 'studio-palette-empty';
        empty.textContent = state.emptyText;
        list.appendChild(empty);
        input.removeAttribute('aria-activedescendant');
        return;
      }
      state.filtered.forEach((item, i) => {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = `studio-palette-item${i === state.selected ? ' is-selected' : ''}`;
        btn.id = `studio-palette-item-${i}`;
        btn.setAttribute('role', 'option');
        btn.setAttribute('aria-selected', i === state.selected ? 'true' : 'false');

        const iconWrap = document.createElement('span');
        iconWrap.className = 'studio-item-icon';
        const glyph = document.createElement('i');
        glyph.className = `fa-solid ${/^fa-[a-z0-9-]+$/.test(item.icon || '') ? item.icon : 'fa-circle'}`;
        iconWrap.appendChild(glyph);

        const text = document.createElement('span');
        text.className = 'studio-item-text';
        const title = document.createElement('span');
        title.className = 'studio-item-title block';
        title.textContent = item.title || item.key || '';
        text.appendChild(title);
        if (item.hint) {
          const hint = document.createElement('span');
          hint.className = 'studio-item-hint block';
          hint.textContent = item.hint;
          text.appendChild(hint);
        }

        btn.append(iconWrap, text);
        const metaText = item.group || (item.kind === 'action' ? 'Action' : '');
        if (metaText) {
          const meta = document.createElement('span');
          meta.className = 'studio-palette-meta';
          meta.textContent = metaText;
          btn.appendChild(meta);
        }

        btn.addEventListener('click', () => pick(item));
        btn.addEventListener('mousemove', () => {
          if (state && state.selected !== i) { state.selected = i; render(); }
        });
        list.appendChild(btn);
        if (i === state.selected) input.setAttribute('aria-activedescendant', btn.id);
      });
      list.querySelector('.is-selected')?.scrollIntoView({ block: 'nearest' });
    };

    state.onKeydown = (event) => {
      if (!state) return;
      if (event.key === 'Escape') { event.preventDefault(); close(); return; }
      if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
        event.preventDefault();
        const delta = event.key === 'ArrowDown' ? 1 : -1;
        const count = state.filtered.length;
        if (count) state.selected = (state.selected + delta + count) % count;
        render();
        return;
      }
      if (event.key === 'Enter') {
        event.preventDefault();
        pick(state.filtered[state.selected]);
        return;
      }
      if (event.key === 'Tab') {
        // Keep the focus inside the palette while it is open.
        event.preventDefault();
        state.input.focus();
      }
    };

    input.addEventListener('input', () => { if (state) state.selected = 0; render(); });
    backdrop.addEventListener('mousedown', (event) => { if (event.target === backdrop) close(); });
    document.addEventListener('keydown', state.onKeydown, true);

    document.body.appendChild(backdrop);
    render();
    input.focus();
  };

  /** Items whose `key` appears in `keys` go first (in keys order). */
  const prioritize = (items, keys) => {
    const list = [...(items || [])];
    if (!Array.isArray(keys) || !keys.length) return list;
    const byKey = new Map(list.map((item) => [String(item?.key), item]));
    const first = keys.map((k) => byKey.get(String(k))).filter(Boolean);
    const seen = new Set(first.map((item) => String(item.key)));
    return [...first, ...list.filter((item) => !seen.has(String(item?.key)))];
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.StudioPalette = {
    open,
    close,
    isOpen,
    filter: filterItems,
    prioritize,
    scoreItem,
    scoreText,
  };
})();
