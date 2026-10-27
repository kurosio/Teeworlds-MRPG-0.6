/** Related records: one adapter for schema fields, custom combos and map worlds.
 * The source document stays alive, including its form, scroll and selection.
 * Each drill-down is modal; returning never reloads either editor.
 */
(() => {
  const core = (window.EditorCore = window.EditorCore || {});
  if (core.RelatedRecords) return;
  const SOURCES = Object.freeze({
    items: 'items', bots: 'bots_info', worlds: 'worlds', quests: 'quests',
    quest_boards: 'quest_boards',
  });
  const t = (text) => core.I18n?.t?.(text) || text;
  const MAX_DEPTH = 8;
  let panel = null;
  let closing = false;
  let requestSeq = 0;
  let openSeq = 0;

  const targetFor = (source, value) => {
    const key = Object.hasOwn(SOURCES, source) ? SOURCES[source] : null;
    const raw = String(value ?? '').trim();
    const id = Number(raw);
    if (!key || !raw || !Number.isSafeInteger(id) || id < 0 || (id === 0 && source !== 'worlds')) return null;
    const editor = (window.STUDIO_MANIFEST || []).flatMap((group) => group.items || []).find((entry) => entry.key === key);
    if (!editor) return null;
    return { editor, id, resource: key };
  };

  const requestDirty = (frame) => new Promise((resolve) => {
    const requestId = `related:${Date.now()}:${++requestSeq}`;
    let timer;
    const finish = (dirty) => {
      clearTimeout(timer);
      window.removeEventListener('message', receive);
      resolve(dirty);
    };
    const receive = (event) => {
      if (!core.Http.isSameOriginMessage(event, frame.contentWindow)) return;
      if (event.data?.type === 'editor-shell:dirty-status' && event.data.requestId === requestId) finish(!!event.data.dirty);
    };
    window.addEventListener('message', receive);
    // Unknown/loading editors are not assumed clean.
    timer = setTimeout(() => finish(true), 1800);
    core.Http.postMessage(frame.contentWindow, { type: 'editor-shell:request-dirty-status', requestId });
  });

  const close = async () => {
    if (!panel || closing) return;
    closing = true;
    const current = panel.entries.at(-1);
    try {
      if (await requestDirty(current.frame)) {
        if (!await core.Page.confirm('You have unsaved changes. Close without saving?')) return;
      }
      current.frame.remove();
      panel.entries.pop();
      if (!panel.entries.length) {
        panel.dialog.close();
        panel.dialog.remove();
        panel = null;
      } else {
        panel.entries.at(-1).frame.hidden = false;
        updateHeader();
      }
      // Refresh lookup labels, not the source model: never replace a dirty form.
      core.DB?.invalidateCache?.();
      core.DBCrud?.invalidateCache?.();
      const previous = panel?.entries.at(-1)?.frame.contentWindow?.EditorCore;
      previous?.DB?.invalidateCache?.();
      previous?.DBCrud?.invalidateCache?.();
      current.trigger?.focus?.({ preventScroll: true });
    } finally { closing = false; }
  };

  const open = async (source, value, trigger = null) => {
    // Reuse the owning workbench, rather than nesting full-screen headers
    // inside iframes (which quickly consumes a phone's entire viewport).
    try {
      const owner = window.parent !== window && window.parent.EditorCore?.RelatedRecords;
      if (owner?.ownsFrame(window)) return owner.open(source, value, trigger);
    } catch { /* standalone or external embedding */ }
    const seq = ++openSeq;
    const target = targetFor(source, value);
    if (!target || closing) return false;
    const depth = panel?.entries.length || 0;
    if (depth >= MAX_DEPTH) {
      core.Page.alert(t('Too many related editors. Return to a previous record first.'));
      return false;
    }
    const guest = core.Guest;
    if (guest?.ready) {
      await guest.ready();
      if (guest.isGuest?.() && !guest.accountPages?.().includes(target.editor.key)) {
        if (panel) core.Page.alert(t('You do not have access to this editor.'));
        else core.UI?.toast?.(t('You do not have access to this editor.'), 'error');
        return false;
      }
    }
    if (seq !== openSeq || closing || (trigger && !trigger.isConnected)) return false;
    if ((panel?.entries.length || 0) >= MAX_DEPTH) return false;
    if (!panel) createPanel();
    panel.entries.forEach((entry) => { entry.frame.hidden = true; });
    const frame = document.createElement('iframe');
    frame.title = `${t(target.editor.title)} #${target.id}`;
    const url = new URL(target.editor.src, location.href);
    url.searchParams.set('resource', target.resource);
    url.searchParams.set('record', String(target.id));
    frame.src = url.href;
    panel.entries.push({ frame, trigger, source, id: target.id });
    panel.dialog.appendChild(frame);
    updateHeader();
    return true;
  };

  const updateHeader = () => {
    panel.title.textContent = panel.entries.map((entry) => entry.frame.title).join(' › ');
  };

  const createPanel = () => {
    const dialog = document.createElement('dialog');
    dialog.className = 'related-record-dialog';
    dialog.setAttribute('aria-label', t('Related record'));
    const header = document.createElement('header');
    header.className = 'related-record-header';
    const back = document.createElement('button');
    back.type = 'button';
    back.className = 'editor-btn editor-btn-secondary';
    back.textContent = t('Return to editing');
    back.addEventListener('click', close);
    const title = document.createElement('strong');
    title.className = 'related-record-trail';
    const hint = document.createElement('span');
    hint.className = 'related-record-hint';
    hint.textContent = t('The previous form is kept open.');
    header.append(back, title, hint);
    dialog.appendChild(header);
    dialog.addEventListener('cancel', (event) => { event.preventDefault(); close(); });
    document.body.appendChild(dialog);
    panel = { dialog, title, entries: [] };
    dialog.showModal();
    back.focus();
  };

  const bind = (control, source, input) => {
    if (!SOURCES[source] || !input) return;
    if (control.dataset.relatedBound === '1') {
      control.dispatchEvent(new Event('related-record:refresh'));
      return;
    }
    control.dataset.relatedBound = '1';
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'editor-icon-btn related-record-link';
    button.title = t('Edit related record');
    button.setAttribute('aria-label', t('Edit related record'));
    button.innerHTML = '<i class="fa-solid fa-arrow-up-right-from-square" aria-hidden="true"></i>';
    const update = () => { button.disabled = !targetFor(source, input.value); };
    button.addEventListener('click', (event) => {
      event.preventDefault();
      event.stopPropagation();
      open(source, input.value, button);
    });
    control.addEventListener('related-record:refresh', update);
    input.addEventListener('input', update);
    input.addEventListener('change', update);
    const row = document.createElement('div');
    row.className = 'related-record-control';
    control.before(row);
    row.append(control, button);
    update();
  };

  const bindList = (input) => {
    if (input.dataset.relatedBound === '1') return;
    input.dataset.relatedBound = '1';
    const links = document.createElement('div');
    links.className = 'related-record-list';
    input.after(links);
    const render = () => {
      links.replaceChildren();
      const source = input.dataset.relatedListSource;
      const values = [...new Set(String(input.value).split(',').map((v) => v.trim()))];
      values.filter((value) => targetFor(source, value)).slice(0, 100).forEach((value) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'editor-btn editor-btn-secondary editor-btn--sm';
        button.textContent = `↗ #${value}`;
        button.title = t('Edit related record');
        button.addEventListener('click', () => open(source, value, button));
        links.appendChild(button);
      });
    };
    input.addEventListener('input', render);
    input.addEventListener('change', render);
    render();
  };

  const init = (root = document) => {
    root.querySelectorAll('input[data-related-list-source]').forEach(bindList);
    root.querySelectorAll('.editor-dbcombo[data-datasource]').forEach((combo) => {
      bind(combo, combo.dataset.datasource, combo.querySelector('.editor-dbcombo-bound'));
    });
    root.querySelectorAll('select[data-datasource], select[data-position-world-select], input[data-related-source]').forEach((input) => {
      bind(input, input.dataset.relatedSource || input.dataset.datasource || 'worlds', input);
    });
  };

  // Only the iframe owned by this module may navigate its parent. Forward the
  // shell protocol one level at a time so nested editors keep working in Studio.
  window.addEventListener('message', (event) => {
    if (!panel || !core.Http.isSameOriginMessage(event, panel.entries.at(-1)?.frame.contentWindow)) return;
    const data = event.data || {};
    if (data.type === 'editor-shell:navigate' && data.action === 'history-back') { close(); return; }
    const forwarded = ['editor-shell:auth-required', 'editor-shell:guest-draft-changed',
      'editor-shell:requests-changed', 'editor-shell:open-editor', 'editor-shell:navigate'];
    if (forwarded.includes(data.type) && window.parent !== window) core.Http.postMessage(window.parent, data);
  });
  window.addEventListener('beforeunload', (event) => {
    if (!panel) return;
    event.preventDefault();
    event.returnValue = '';
  });
  core.RelatedRecords = { init, open, close, targetFor, hasOpenEditors: () => !!panel,
    ownsFrame: (child) => !!panel?.entries.some((entry) => entry.frame.contentWindow === child) };
})();
