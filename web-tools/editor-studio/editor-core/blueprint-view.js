/* ============================================================
   Blueprint (Unreal Engine style) graph view for the Scenario
   Editor.  Additive feature — it visualises the SAME data
   (steps as nodes, components as rows, link fields as wires)
   on a pannable / zoomable canvas.

   It does NOT change the scenario data structure.  The only thing
   it persists is editor-only metadata inside `editor_data.blueprint`
   (node positions + empty-group anchors) — exactly like the existing
   `editor_data.groups`.  Camera (pan/zoom) is kept in localStorage so
   panning never marks the document dirty.

   Features:
   - grey group frames (name + event flag) that auto-grow / shrink
     around their member steps; drag steps in/out to change membership;
   - a connection dot next to every component to drag a link to a step
     (drop on empty canvas creates + links a new step);
   - implicit "by list order" arrows when a step has no explicit links;
   - add components / edit / delete without leaving the graph.
   ============================================================ */
(() => {
  const CLASS_COLORS = {
    info: '#3b82f6',
    interactive: '#f59e0b',
    combat: '#ef4444',
    camera: '#14b8a6',
    condition: '#a78bfa',
    logic: '#22d3ee',
    quest: '#34d399',
    default: '#64748b'
  };

  const WIRE_COLORS = {
    next_step_id: 'var(--bp-wire-exec)',
    true_step_id: 'var(--bp-wire-true)',
    false_step_id: 'var(--bp-wire-false)',
    in_range_step_id: 'var(--bp-wire-true)',
    out_of_range_step_id: 'var(--bp-wire-false)',
    pass_step_id: 'var(--bp-wire-true)',
    fail_step_id: 'var(--bp-wire-false)',
    _data: 'var(--bp-wire-data)'
  };

  // Short pin labels for fields when a component exposes several.
  const FIELD_SHORT = {
    next_step_id: '',
    true_step_id: 'T',
    false_step_id: 'F',
    in_range_step_id: 'IN',
    out_of_range_step_id: 'OUT',
    pass_step_id: 'OK',
    fail_step_id: 'TO',
    target_step_id: '→'
  };

  const NODE_W = 260;
  const COL_GAP = 150;        // horizontal gap between layout columns
  const ROW_GAP = 48;         // vertical gap between nodes in a column
  const MIN_SCALE = 0.2;
  const MAX_SCALE = 2.2;
  const GROUP_PAD = 26;       // padding inside a group frame
  const GROUP_HEADER = 36;    // group frame header height
  const GROUP_GAP = 70;       // vertical gap between group bands
  const SVG_NS = 'http://www.w3.org/2000/svg';

  class BlueprintView {
    constructor(editor, canvas) {
      this.editor = editor;
      this.canvas = canvas;
      this.world = canvas.querySelector('#bp-world');
      this.wires = canvas.querySelector('#bp-wires');
      this.zoomLabel = document.getElementById('bp-zoom-label');

      this.nodeEls = new Map();      // stepId -> node element
      this.groupEls = new Map();     // groupId -> frame element
      this.pinOut = new Map();       // `${stepId}:${componentId}:${field}` -> { pin, row }
      this.stepGroup = new Map();    // stepId -> groupId | null

      this.view = { x: 80, y: 80, scale: 1 };
      this.active = false;
      this._raf = null;
      this._connect = null;          // live connection-drag state
      this.selected = new Set();          // selected step ids (marquee / shift-click)
      this.selectedGroups = new Set();    // selected group ids
      this.selectedComments = new Set();  // selected comment ids
      this._marquee = null;               // live marquee state

      this.minimap = document.getElementById('bp-minimap');
      this.minimapOn = false;
      try { this.minimapOn = localStorage.getItem('scenario-studio:bp-minimap') === '1'; } catch { /* ignore */ }

      // Remember what kind of pointer was used last: a few interactions
      // (tap-to-rename) make sense with a mouse but not with a finger.
      this.lastPointerType = 'mouse';
      document.addEventListener('pointerdown', (e) => { this.lastPointerType = e.pointerType || 'mouse'; }, true);

      this.bindCanvasEvents();
      this.bindMinimap();
    }

    // ---- pointer plumbing (mouse / touch / pen) ----------------
    // Every drag in the graph goes through this one helper. Pointer events
    // cover mouse, touch and pen with the same code path, so a finger can
    // move nodes / groups / comments / wires exactly like a cursor does.
    // The returned function cancels the drag (used by pinch-to-zoom, which
    // must be able to abort a one-finger drag when a second finger lands).
    startPointerDrag(startEvent, handlers = {}) {
      // While two fingers pinch, a second pointerdown must NOT start a new
      // drag (that finger belongs to the zoom gesture).
      if (this._pinch) return () => {};
      const id = startEvent?.pointerId;
      const isTouch = startEvent?.pointerType === 'touch';
      let finished = false;
      const samePointer = (ev) => id == null || ev.pointerId === id;

      const onMove = ev => {
        if (!samePointer(ev) || finished) return;
        // Touch scrolling/zooming is disabled on the canvas (touch-action:none),
        // this only stops the browser from also panning the page on some
        // Android builds.
        if (isTouch && ev.cancelable) ev.preventDefault();
        handlers.move?.(ev);
      };
      const finish = (ev, cancelled) => {
        if (finished) return;
        finished = true;
        window.removeEventListener('pointermove', onMove);
        window.removeEventListener('pointerup', onUp);
        window.removeEventListener('pointercancel', onCancel);
        if (this._dragCancel === cancel) this._dragCancel = null;
        handlers.end?.(ev, cancelled);
      };
      const onUp = ev => { if (samePointer(ev)) finish(ev, false); };
      const onCancel = ev => { if (samePointer(ev)) finish(ev, true); };
      const cancel = () => finish(null, true);

      this._dragCancel?.();     // never keep two drags alive at once
      this._dragCancel = cancel;
      window.addEventListener('pointermove', onMove, { passive: false });
      window.addEventListener('pointerup', onUp);
      window.addEventListener('pointercancel', onCancel);
      return cancel;
    }

    // ---- persistence helpers ----------------------------------
    // Layout metadata lives in editor_data.blueprint while editing; on save it
    // is emitted as a SEPARATE top-level `blueprint` block (see editor's
    // buildScenarioContent), so the gameplay structure (steps/components) is
    // never touched.
    get bp() {
      const ed = this.editor.scenarioData.editor_data || (this.editor.scenarioData.editor_data = {});
      if (!ed.blueprint || typeof ed.blueprint !== 'object') ed.blueprint = {};
      if (!ed.blueprint.positions || typeof ed.blueprint.positions !== 'object') ed.blueprint.positions = {};
      // Manual group frames: groupId -> { x, y, w, h }. Auto-fit always wins
      // if members exceed the manual box (union), so groups grow with content.
      if (!ed.blueprint.groupRects || typeof ed.blueprint.groupRects !== 'object') ed.blueprint.groupRects = {};
      if (!Array.isArray(ed.blueprint.comments)) ed.blueprint.comments = [];
      // Migrate the older `groupPos` (anchor-only) format into groupRects.
      if (ed.blueprint.groupPos && typeof ed.blueprint.groupPos === 'object') {
        Object.entries(ed.blueprint.groupPos).forEach(([gid, p]) => {
          if (p && Number.isFinite(p.x) && !ed.blueprint.groupRects[gid]) {
            ed.blueprint.groupRects[gid] = { x: p.x, y: p.y, w: 240, h: 150 };
          }
        });
        delete ed.blueprint.groupPos;
      }
      return ed.blueprint;
    }

    viewStorageKey() {
      const file = this.editor.scenarioSelect?.value || 'scenario';
      return `scenario-studio:bp-view:${file}`;
    }

    loadView() {
      try {
        const raw = localStorage.getItem(this.viewStorageKey());
        if (raw) {
          const v = JSON.parse(raw);
          if (v && Number.isFinite(v.x) && Number.isFinite(v.y) && Number.isFinite(v.scale)) {
            this.view = { x: v.x, y: v.y, scale: Math.min(MAX_SCALE, Math.max(MIN_SCALE, v.scale)) };
            return true;
          }
        }
      } catch { /* ignore */ }
      return false;
    }

    saveView() {
      try { localStorage.setItem(this.viewStorageKey(), JSON.stringify(this.view)); } catch { /* ignore */ }
    }

    // ---- show / hide ------------------------------------------
    show() {
      this.active = true;
      document.body.classList.add('bp-mode');
      const restored = this.loadView();
      this.render();
      // First visit for this scenario (or a brand-new device): frame the whole
      // graph. Without this a phone opens the graph on an empty patch of canvas.
      if (!restored) requestAnimationFrame(() => this.fitToView());
      this.updateMinimapVisibility();
      this.drawMinimap();
    }

    hide() {
      this.active = false;
      this.cancelConnect();
      this.clearSelection();
      this.closeCreateMenu();
      document.body.classList.remove('bp-mode');
    }

    // ---- view-state snapshot ----------------------------------
    snapshotViewState() {
      const vs = this.editor.getCurrentViewState
        ? this.editor.getCurrentViewState()
        : { groups: [], ungroupedSteps: [] };
      this.viewState = vs;
      this._scopeCache = new Map();     // event block per step, rebuilt on render
      this.stepGroup.clear();
      (vs.groups || []).forEach(g => (g.steps || []).forEach(id => this.stepGroup.set(id, g.id)));
      // flattened "list order" for implicit arrows
      const order = [];
      (vs.groups || []).forEach(g => (g.steps || []).forEach(id => order.push(id)));
      (vs.ungroupedSteps || []).forEach(id => order.push(id));
      this.flatOrder = order;
      return vs;
    }

    // ---- rendering --------------------------------------------
    render() {
      if (!this.active) return;
      const steps = this.editor.scenarioData.steps || [];

      this.snapshotViewState();

      // reset layers (keep the wire svg element)
      Array.from(this.world.querySelectorAll('.bp-node, .bp-group, .bp-comment')).forEach(n => n.remove());
      this.nodeEls.clear();
      this.groupEls.clear();
      this.pinOut.clear();
      this.canvas.querySelector('.bp-empty')?.remove();

      const hasComments = (this.bp.comments || []).length > 0;
      if (steps.length === 0 && (this.viewState.groups || []).length === 0 && !hasComments) {
        this.renderEmpty();
        this.applyTransform();
        this.clearWires();
        this.renderEventLegend();
        return;
      }

      // Comments are the BACK layer: render them first so groups / steps /
      // components sit on top and can be placed inside a comment's area.
      this.renderComments();

      if (steps.length === 0 && (this.viewState.groups || []).length === 0) {
        // Empty graph but with comments: only comments to show.
        this.applyTransform();
        this.clearWires();
        this.renderEventLegend();
        return;
      }

      const allIds = new Set(steps.map(s => s.id));

      // group frames first (behind nodes)
      (this.viewState.groups || []).forEach(group => {
        const frame = this.buildGroupFrame(group);
        this.world.appendChild(frame);
        this.groupEls.set(group.id, frame);
      });

      // nodes
      steps.forEach(step => {
        const node = this.buildNode(step, allIds);
        this.world.appendChild(node);
        this.nodeEls.set(step.id, node);
      });

      // positions (auto-layout any missing)
      this.ensurePositions(steps);
      steps.forEach(step => {
        const pos = this.bp.positions[step.id];
        const node = this.nodeEls.get(step.id);
        if (node && pos) {
          node.style.left = `${Math.round(pos.x)}px`;
          node.style.top = `${Math.round(pos.y)}px`;
        }
      });

      this.positionPins();
      this.updateGroupFrames();
      // Snapshot effective rects so empty/auto-grown groups keep their box.
      this.groupEls.forEach((_, gid) => {
        if (!this.bp.groupRects[gid]) this.syncGroupRect(gid);
      });
      this.applyTransform();
      this.drawWires();
      this.renderEventLegend();
      // prune selection of ids that no longer exist, then re-apply styles
      this.selected.forEach(id => { if (!this.nodeEls.has(id)) this.selected.delete(id); });
      this.selectedGroups.forEach(id => { if (!this.groupEls.has(id)) this.selectedGroups.delete(id); });
      const liveComments = new Set((this.bp.comments || []).map(c => c.id));
      this.selectedComments.forEach(id => { if (!liveComments.has(id)) this.selectedComments.delete(id); });
      this.applySelectionStyles();
      this.updateMinimapVisibility();
      this.drawMinimap();
    }

    renderEmpty() {
      const div = document.createElement('div');
      div.className = 'bp-empty';
      div.innerHTML = `
        <i class="fa-solid fa-diagram-project"></i>
        <div class="bp-empty-title">Graph is empty</div>
        <div>Add a step with the “+” button on the graph toolbar or by double-clicking empty space.</div>`;
      this.canvas.appendChild(div);
    }

    // ---- comments (sticky notes) ------------------------------
    renderComments() {
      (this.bp.comments || []).forEach(cmt => {
        const el = this.buildCommentEl(cmt);
        this.world.appendChild(el);
      });
    }

    buildCommentEl(cmt) {
      const el = document.createElement('div');
      el.className = 'bp-comment';
      el.dataset.commentId = cmt.id;
      el.style.left = `${Math.round(cmt.x)}px`;
      el.style.top = `${Math.round(cmt.y)}px`;
      el.style.width = `${Math.round(cmt.w || 220)}px`;
      el.style.height = `${Math.round(cmt.h || 120)}px`;
      if (cmt.color) el.style.setProperty('--bp-comment-color', cmt.color);

      el.innerHTML = `
        <div class="bp-comment-head" title="Drag to move the note">
          <i class="fa-solid fa-note-sticky"></i>
          <span class="bp-comment-label">Comment</span>
          <span class="bp-comment-actions">
            <button data-cmt-action="color" title="Color"><i class="fa-solid fa-palette"></i></button>
            <button data-cmt-action="delete" title="Delete the note"><i class="fa-solid fa-trash-can"></i></button>
          </span>
        </div>
        <div class="bp-comment-body" data-role="text" contenteditable="false" spellcheck="false" title="Double-click to edit the text"></div>
        <div class="bp-comment-resize" title="Resize"></div>`;

      const body = el.querySelector('[data-role="text"]');
      body.textContent = cmt.text || '';
      body.setAttribute('data-placeholder', 'Double-click to add text…');

      // The body is read-only until double-clicked, so that pressing over it
      // pans the canvas (it's the back layer). Editing is opt-in.
      const beginEdit = () => {
        body.contentEditable = 'true';
        body.classList.add('is-editing');
        body.focus();
      };
      const endEdit = () => {
        body.contentEditable = 'false';
        body.classList.remove('is-editing');
      };
      body.addEventListener('dblclick', e => { e.stopPropagation(); beginEdit(); });
      body.addEventListener('pointerdown', e => { if (body.classList.contains('is-editing')) e.stopPropagation(); });
      body.addEventListener('blur', endEdit);
      body.addEventListener('input', () => {
        cmt.text = body.textContent;
        this.editor.dirty.markDirty?.();
      });

      // actions
      el.querySelectorAll('[data-cmt-action]').forEach(btn => {
        btn.addEventListener('pointerdown', e => e.stopPropagation());
        btn.addEventListener('click', e => {
          e.stopPropagation();
          const a = btn.dataset.cmtAction;
          if (a === 'delete') this.deleteComment(cmt.id);
          else if (a === 'color') this.cycleCommentColor(cmt);
        });
      });

      // drag (by head)
      const head = el.querySelector('.bp-comment-head');
      head.addEventListener('pointerdown', e => {
        if (e.button !== 0 || e.target.closest('[data-cmt-action]')) return;
        e.preventDefault(); e.stopPropagation();
        this.startCommentDrag(el, cmt, e);
      });
      // resize
      const grip = el.querySelector('.bp-comment-resize');
      grip.addEventListener('pointerdown', e => {
        if (e.button !== 0) return;
        e.preventDefault(); e.stopPropagation();
        this.startCommentResize(el, cmt, e);
      });

      return el;
    }

    addComment(x, y) {
      const cmt = {
        id: `cmt_${this.editor.generateUUID ? this.editor.generateUUID() : Date.now()}`,
        x: Math.round(x), y: Math.round(y), w: 240, h: 130, text: ''
      };
      this.bp.comments.push(cmt);
      this.editor.dirty.markDirty?.();
      this.render();
      // focus the new note
      requestAnimationFrame(() => {
        const el = this.world.querySelector(`.bp-comment[data-comment-id="${cmt.id}"] [data-role="text"]`);
        el?.focus();
      });
      return cmt.id;
    }

    deleteComment(id) {
      const i = this.bp.comments.findIndex(c => c.id === id);
      if (i < 0) return;
      this.bp.comments.splice(i, 1);
      this.editor.dirty.markDirty?.();
      this.render();
    }

    cycleCommentColor(cmt) {
      const palette = ['#ffcf5a', '#5ad1b4', '#6fa8ff', '#ff8fa3', '#c79bff', '#9be37d', '#ff9d5c'];
      const cur = palette.indexOf(cmt.color);
      cmt.color = palette[(cur + 1) % palette.length];
      this.editor.dirty.markDirty?.();
      this.render();
    }

    startCommentDrag(el, cmt, e) {
      // If this comment is part of a multi-selection, drag the whole selection.
      const multi = this.selectedComments.has(cmt.id) &&
        (this.selected.size + this.selectedGroups.size + this.selectedComments.size) > 1;
      if (multi) {
        const moveSet = this.collectMoveSet();
        moveSet.commentIds.add(cmt.id);
        this.startMultiDrag(moveSet, e);
        return;
      }
      el.classList.add('is-dragging');
      const startMouse = { x: e.clientX, y: e.clientY };
      const start = { x: cmt.x, y: cmt.y };
      let moved = false;
      this.startPointerDrag(e, {
        move: ev => {
          const dx = (ev.clientX - startMouse.x) / this.view.scale;
          const dy = (ev.clientY - startMouse.y) / this.view.scale;
          if (Math.abs(dx) > 1 || Math.abs(dy) > 1) moved = true;
          cmt.x = start.x + dx; cmt.y = start.y + dy;
          el.style.left = `${Math.round(cmt.x)}px`;
          el.style.top = `${Math.round(cmt.y)}px`;
        },
        end: () => {
          el.classList.remove('is-dragging');
          if (moved) this.editor.dirty.markDirty?.();
        }
      });
    }

    startCommentResize(el, cmt, e) {
      const startMouse = { x: e.clientX, y: e.clientY };
      const start = { w: cmt.w || 240, h: cmt.h || 130 };
      let moved = false;
      this.startPointerDrag(e, {
        move: ev => {
          const dx = (ev.clientX - startMouse.x) / this.view.scale;
          const dy = (ev.clientY - startMouse.y) / this.view.scale;
          if (Math.abs(dx) > 1 || Math.abs(dy) > 1) moved = true;
          cmt.w = Math.max(140, start.w + dx);
          cmt.h = Math.max(80, start.h + dy);
          el.style.width = `${Math.round(cmt.w)}px`;
          el.style.height = `${Math.round(cmt.h)}px`;
        },
        end: () => {
          if (moved) this.editor.dirty.markDirty?.();
        }
      });
    }

    // ---- group frames -----------------------------------------
    buildGroupFrame(group) {
      const frame = document.createElement('div');
      frame.className = 'bp-group';
      frame.dataset.groupId = group.id;

      const header = document.createElement('div');
      header.className = 'bp-group-header';

      // The event of a group is the block the server runs it in — the frame is
      // tinted with that event's colour (and the legend in the corner lists
      // them), so two events never look like one continuous flow.
      const eventStyle = this.eventStyle(group.event_key);
      frame.style.setProperty('--bp-event-color', eventStyle.color);
      frame.classList.add(eventStyle.isEvent ? 'is-event' : 'is-base');
      frame.dataset.eventKey = eventStyle.key;
      frame.title = `${group.name || 'Group'} — ${eventStyle.label}`;

      const flag = document.createElement('i');
      flag.className = `bp-group-flag ${eventStyle.icon}`;
      flag.title = eventStyle.isEvent
        ? `Event:  ${eventStyle.label}  (${group.event_key}) — its steps never link outside this event`
        : 'Group without an event';
      header.appendChild(flag);

      const title = document.createElement('span');
      title.className = 'bp-group-title';
      title.textContent = group.name || 'Group';
      title.contentEditable = 'true';
      title.spellcheck = false;
      title.addEventListener('pointerdown', e => e.stopPropagation());
      title.addEventListener('click', e => e.stopPropagation());
      title.addEventListener('keydown', e => {
        if (e.key === 'Enter') { e.preventDefault(); title.blur(); }
      });
      title.addEventListener('blur', () => {
        const name = title.textContent.trim() || 'Group';
        this.editor.renameGroupSilent?.(group.id, name) ?? this.editor.renameGroup?.(group.id, name);
      });
      header.appendChild(title);

      // event flag selector
      const sel = document.createElement('select');
      sel.className = 'bp-group-event';
      sel.title = 'Group event';
      sel.addEventListener('pointerdown', e => e.stopPropagation());
      sel.innerHTML = ['<option value="">No event</option>']
        .concat(Object.entries(this.editor.scenarioEvents || {})
          .filter(([k]) => k !== 'general')
          .map(([k, label]) => `<option value="${k}">${this.esc(label)}</option>`)).join('');
      sel.value = eventStyle.isEvent ? group.event_key : '';
      sel.addEventListener('change', e => {
        e.stopPropagation();
        this.editor.setGroupEventKey?.(group.id, sel.value || null);
      });
      header.appendChild(sel);

      const del = document.createElement('button');
      del.className = 'bp-group-del';
      del.title = 'Delete the group';
      del.innerHTML = '<i class="fa-solid fa-xmark"></i>';
      del.addEventListener('pointerdown', e => e.stopPropagation());
      del.addEventListener('click', e => {
        e.stopPropagation();
        this.editor.openDeleteGroupModal?.(group.id);
      });
      header.appendChild(del);

      frame.appendChild(header);

      // resize handle (bottom-right) — lets the user enlarge the group band
      const grip = document.createElement('div');
      grip.className = 'bp-group-resize';
      grip.title = 'Drag to resize the group';
      grip.innerHTML = '<i class="fa-solid fa-up-right-and-down-left-from-center"></i>';
      grip.addEventListener('pointerdown', e => {
        if (e.button !== 0) return;
        e.preventDefault();
        e.stopPropagation();
        this.startGroupResize(group.id, e);
      });
      frame.appendChild(grip);

      // drag whole group by header
      header.addEventListener('pointerdown', e => {
        if (e.button !== 0) return;
        if (e.target.closest('.bp-group-title, .bp-group-event, .bp-group-del')) return;
        e.preventDefault();
        e.stopPropagation();
        this.startGroupDrag(group.id, e);
      });

      return frame;
    }

    groupMembers(groupId) {
      const g = (this.viewState.groups || []).find(x => x.id === groupId);
      if (!g) return [];
      return (g.steps || []).filter(id => this.nodeEls.has(id));
    }

    // Tight bounding box (incl. padding + header) around the member nodes.
    memberBounds(groupId, excludeStepId = null) {
      let members = this.groupMembers(groupId);
      if (excludeStepId) members = members.filter(id => id !== excludeStepId);
      let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
      members.forEach(id => {
        const p = this.bp.positions[id];
        const node = this.nodeEls.get(id);
        if (!p || !node) return;
        minX = Math.min(minX, p.x);
        minY = Math.min(minY, p.y);
        maxX = Math.max(maxX, p.x + node.offsetWidth);
        maxY = Math.max(maxY, p.y + node.offsetHeight);
      });
      if (!Number.isFinite(minX)) return null;
      return {
        x: minX - GROUP_PAD,
        y: minY - GROUP_PAD - GROUP_HEADER,
        w: (maxX - minX) + GROUP_PAD * 2,
        h: (maxY - minY) + GROUP_PAD * 2 + GROUP_HEADER
      };
    }

    // Effective rectangle = union of the user-resized rect and member bounds,
    // so the frame always *contains* its steps (it can be grown manually but
    // never shrinks below its content — this is what stops steps "escaping").
    groupRect(groupId, excludeStepId = null) {
      const stored = this.bp.groupRects[groupId];
      const bounds = this.memberBounds(groupId, excludeStepId);

      if (!stored && !bounds) {
        const anchor = this.defaultGroupAnchor(groupId);
        return { x: anchor.x, y: anchor.y, w: 260, h: 160, empty: true };
      }
      if (stored && !bounds) {
        return { ...stored, empty: true };
      }
      if (!stored && bounds) {
        return { ...bounds, empty: false };
      }
      // union(stored, bounds)
      const x = Math.min(stored.x, bounds.x);
      const y = Math.min(stored.y, bounds.y);
      const r = Math.max(stored.x + stored.w, bounds.x + bounds.w);
      const b = Math.max(stored.y + stored.h, bounds.y + bounds.h);
      return { x, y, w: r - x, h: b - y, empty: false };
    }

    defaultGroupAnchor(groupId) {
      const idx = (this.viewState.groups || []).findIndex(g => g.id === groupId);
      const anchor = { x: 60 + idx * 40, y: 60 + idx * 200 };
      this.bp.groupRects[groupId] = { x: anchor.x, y: anchor.y, w: 280, h: 180 };
      return anchor;
    }

    // Persist the current effective rect so groups remember manual sizing and
    // auto-grown content between renders / saves.
    syncGroupRect(groupId) {
      const r = this.groupRect(groupId);
      this.bp.groupRects[groupId] = { x: r.x, y: r.y, w: r.w, h: r.h };
      return r;
    }

    updateGroupFrames() {
      this.groupEls.forEach((frame, groupId) => {
        const r = this.groupRect(groupId);
        frame.style.left = `${r.x}px`;
        frame.style.top = `${r.y}px`;
        frame.style.width = `${r.w}px`;
        frame.style.height = `${r.h}px`;
        frame.classList.toggle('is-empty', !!r.empty);
      });
    }

    startGroupDrag(groupId, e) {
      // If this group is part of a multi-selection, drag the whole selection.
      const multi = this.selectedGroups.has(groupId) &&
        (this.selected.size + this.selectedGroups.size + this.selectedComments.size) > 1;
      if (multi) {
        const moveSet = this.collectMoveSet();
        moveSet.groupIds.add(groupId);
        this.groupMembers(groupId).forEach(id => moveSet.stepIds.add(id));
        this.startMultiDrag(moveSet, e);
        return;
      }

      const frame = this.groupEls.get(groupId);
      frame?.classList.add('is-dragging');
      const startMouse = { x: e.clientX, y: e.clientY };
      const members = this.groupMembers(groupId);
      const startPos = new Map(members.map(id => [id, { ...this.bp.positions[id] }]));
      const startRect = { ...this.groupRect(groupId) };
      let moved = false;

      this.startPointerDrag(e, {
        move: ev => {
          const dx = (ev.clientX - startMouse.x) / this.view.scale;
          const dy = (ev.clientY - startMouse.y) / this.view.scale;
          if (Math.abs(dx) > 1 || Math.abs(dy) > 1) moved = true;
          // Move the frame and all its members together.
          members.forEach(id => {
            const sp = startPos.get(id);
            const np = { x: sp.x + dx, y: sp.y + dy };
            this.bp.positions[id] = np;
            const node = this.nodeEls.get(id);
            if (node) { node.style.left = `${np.x}px`; node.style.top = `${np.y}px`; }
          });
          this.bp.groupRects[groupId] = {
            x: startRect.x + dx, y: startRect.y + dy, w: startRect.w, h: startRect.h
          };
          this.updateGroupFrames();
          this.scheduleWireRedraw();
        },
        end: () => {
          frame?.classList.remove('is-dragging');
          if (moved) { this.syncGroupRect(groupId); this.editor.dirty.markDirty?.(); }
        }
      });
    }

    // Generic multi-selection drag (used when grabbing a selected group/comment).
    startMultiDrag(moveSet, e) {
      const startMouse = { x: e.clientX, y: e.clientY };
      const startNode = new Map();
      moveSet.stepIds.forEach(id => startNode.set(id, { ...(this.bp.positions[id] || { x: 0, y: 0 }) }));
      const startComment = new Map();
      moveSet.commentIds.forEach(id => { const c = (this.bp.comments || []).find(x => x.id === id); if (c) startComment.set(id, { x: c.x, y: c.y }); });
      const startGroupRect = new Map();
      moveSet.groupIds.forEach(gid => startGroupRect.set(gid, { ...this.groupRect(gid) }));
      moveSet.stepIds.forEach(id => this.nodeEls.get(id)?.classList.add('is-dragging'));
      let moved = false;

      this.startPointerDrag(e, {
        move: ev => {
          const dx = (ev.clientX - startMouse.x) / this.view.scale;
          const dy = (ev.clientY - startMouse.y) / this.view.scale;
          if (Math.abs(dx) > 1 || Math.abs(dy) > 1) moved = true;
          this.translateSelection(moveSet, startNode, startComment, startGroupRect, dx, dy);
          this.scheduleWireRedraw();
        },
        end: () => {
          moveSet.stepIds.forEach(id => this.nodeEls.get(id)?.classList.remove('is-dragging'));
          if (moved) {
            this.groupEls.forEach((_, gid) => this.syncGroupRect(gid));
            this.editor.dirty.markDirty?.();
          }
        }
      });
    }

    startGroupResize(groupId, e) {
      const frame = this.groupEls.get(groupId);
      frame?.classList.add('is-resizing');
      const startMouse = { x: e.clientX, y: e.clientY };
      const startRect = { ...this.groupRect(groupId) };
      let moved = false;

      this.startPointerDrag(e, {
        move: ev => {
          const dx = (ev.clientX - startMouse.x) / this.view.scale;
          const dy = (ev.clientY - startMouse.y) / this.view.scale;
          if (Math.abs(dx) > 1 || Math.abs(dy) > 1) moved = true;
          // Grow/shrink from bottom-right; auto-fit (union with member bounds)
          // guarantees we never clip the contained steps.
          const w = Math.max(180, startRect.w + dx);
          const h = Math.max(120, startRect.h + dy);
          this.bp.groupRects[groupId] = { x: startRect.x, y: startRect.y, w, h };
          this.updateGroupFrames();
        },
        end: () => {
          frame?.classList.remove('is-resizing');
          if (moved) { this.syncGroupRect(groupId); this.editor.dirty.markDirty?.(); }
        }
      });
    }

    // ---- nodes ------------------------------------------------
    buildNode(step, allIds) {
      const node = document.createElement('div');
      node.className = 'bp-node';
      node.dataset.stepId = step.id;

      const components = (step.components || []).filter(Boolean);
      const isRoot = !this.hasIncoming(step.id);
      // Which event block the step runs in — drives the accent colour and the
      // "start of an event" marker on the node.
      const eventStyle = this.eventStyle(this.eventKeyOfStep(step.id));
      node.dataset.eventKey = eventStyle.key;
      node.style.setProperty('--bp-event-color', eventStyle.color);
      node.classList.toggle('has-event', eventStyle.isEvent);
      const headColor = isRoot
        ? 'linear-gradient(180deg, rgba(255,255,255,.16), rgba(255,255,255,0)), #7c2430'
        : 'linear-gradient(180deg, rgba(255,255,255,.16), rgba(255,255,255,0)), #243049';
      node.style.setProperty('--bp-head', headColor);
      // The first step of an event is where that event starts running — label
      // it, otherwise "this node has no incoming wire" looks like a mistake.
      const eventRootChip = (isRoot && eventStyle.isEvent)
        ? `<span class="bp-node-event" title="Start of the event:  ${this.esc(eventStyle.label)}"><i class="${eventStyle.icon}"></i><span>${this.esc(eventStyle.label)}</span></span>`
        : '';

      // completion-logic presentation (shared with the list view)
      const logic = this.editor.getCompletionLogic
        ? this.editor.getCompletionLogic(step)
        : ((step.completion_logic === 'any_of' || step.completion_logic === 'sequential') ? step.completion_logic : 'all_of');
      const lmeta = this.editor.completionLogicMeta
        ? this.editor.completionLogicMeta(logic)
        : ({ all_of: { short: 'AND', sep: 'AND' }, any_of: { short: 'OR', sep: 'OR' }, sequential: { short: '1·2·3', sep: '→' } }[logic]);
      // Completion logic only matters with 2+ components — hide all logic UI otherwise.
      const showLogic = components.length > 1;

      // header
      const header = document.createElement('div');
      header.className = 'bp-node-header';
      header.innerHTML = `
        <span class="bp-drag-zone" title="Drag to move the step"><i class="fa-solid fa-grip-vertical"></i></span>
        <span class="bp-node-title" data-role="title" title="Click to rename; double-click for quick settings">${this.esc(step.id)}</span>
        ${eventRootChip}
        ${showLogic ? `<span class="bp-logic-badge" data-logic="${logic}" title="Completion logic:  ${this.esc(lmeta.short)}  — click to switch">${this.esc(lmeta.short)}</span>` : ''}
        <span class="bp-node-actions">
          <button data-bp-action="settings" title="Quick settings"><i class="fa-solid fa-sliders"></i></button>
          <button data-bp-action="edit-step" title="Show in the list"><i class="fa-solid fa-up-right-from-square"></i></button>
          <button data-bp-action="add-comp" title="Add a component"><i class="fa-solid fa-plus"></i></button>
          <button data-bp-action="del-step" title="Delete the step"><i class="fa-solid fa-trash-can"></i></button>
        </span>`;
      // input exec pin on the header
      const inPin = document.createElement('span');
      inPin.className = 'bp-pin bp-pin-in';
      inPin.style.setProperty('--bp-pin-color', 'var(--bp-wire-exec)');
      header.appendChild(inPin);
      node.appendChild(header);

      // body
      const body = document.createElement('div');
      body.className = 'bp-node-body';

      if (components.length === 0) {
        const empty = document.createElement('div');
        empty.className = 'bp-empty-comp';
        empty.textContent = 'No components';
        body.appendChild(empty);
      }

      components.forEach((comp, index) => {
        // AND / OR separator between component rows (mirrors the list view).
        if (showLogic && index > 0 && (logic === 'all_of' || logic === 'any_of')) {
          const sep = document.createElement('div');
          sep.className = 'bp-comp-sep';
          sep.dataset.logic = logic;
          sep.innerHTML = `<span>${this.esc(lmeta.sep)}</span>`;
          body.appendChild(sep);
        }

        const info = this.editor.componentTypes[comp.type]
          || this.editor.allComponentTypes[comp.type]
          || { name: comp.type, icon: 'fa-solid fa-question-circle', class: 'default' };
        const color = CLASS_COLORS[info.class] || CLASS_COLORS.default;
        const fields = this.editor.getComponentLinkFields(comp) || ['next_step_id'];
        const links = this.editor.getComponentLinks(comp) || [];
        const linkByField = new Map(links.map(l => [l.field, l.targetId]));

        const row = document.createElement('div');
        row.className = 'bp-comp';
        row.dataset.componentId = comp.component_id;
        row.style.setProperty('--bp-comp-color', color);

        const broken = links.some(l => !allIds.has(l.targetId));
        if (broken) row.classList.add('is-broken');
        // Links pointing into another event block: they are in the data, but
        // never drawn and never executed. Flag the row so the dead link can be
        // removed from the source instead of silently disappearing.
        const crossEvent = links.find(l => allIds.has(l.targetId)
          && this.eventKeyOfStep(l.targetId) !== this.eventKeyOfStep(step.id));
        if (crossEvent) row.classList.add('is-cross-event');
        const crossEventTag = crossEvent
          ? `<span class="bp-cross-event-tag" title="Link to «${this.esc(this.eventStyle(this.eventKeyOfStep(crossEvent.targetId)).label)}» — a different event, so this link is ignored. Double-click the link dot to remove it.">⚠</span>`
          : '';

        const orderChip = (showLogic && logic === 'sequential')
          ? `<span class="bp-comp-order" title="Sequence step #${index + 1}">${index + 1}</span>`
          : '';
        row.innerHTML = `
          ${orderChip}
          <i class="bp-comp-icon ${info.icon}" title="Drag by the icon to move the component to another step"></i>
          <div class="bp-comp-text">
            <div class="bp-comp-name">${this.esc(info.name)}${broken ? '<span class="bp-broken-tag">⚠</span>' : ''}${crossEventTag}</div>
            <div class="bp-comp-sum">${this.editor.getComponentSummary(comp) || ''}</div>
          </div>
          <span class="bp-comp-actions">
            <button data-bp-comp-action="edit" title="Edit the component"><i class="fa-solid fa-pencil"></i></button>
            <button data-bp-comp-action="duplicate" title="Duplicate the component"><i class="fa-regular fa-copy"></i></button>
            <button data-bp-comp-action="delete" title="Delete the component"><i class="fa-regular fa-trash-can"></i></button>
          </span>`;

        // Component action buttons (edit / duplicate / delete) — don't start a drag.
        row.querySelectorAll('[data-bp-comp-action]').forEach(btn => {
          btn.addEventListener('pointerdown', ev => ev.stopPropagation());
          btn.addEventListener('click', ev => {
            ev.stopPropagation();
            const action = btn.dataset.bpCompAction;
            if (action === 'edit') {
              this.editor.openEditModal(step.id, comp.component_id);
            } else if (action === 'duplicate') {
              this.editor.handleDuplicate(step.id, comp.component_id);
            } else if (action === 'delete') {
              this.editor.handleDelete(step.id, comp.component_id);
            }
          });
        });

        // Drag a component (by its icon) between steps.
        const iconEl = row.querySelector('.bp-comp-icon');
        iconEl.addEventListener('pointerdown', ev => {
          if (ev.button !== 0) return;
          ev.preventDefault();
          ev.stopPropagation();
          this.startCompDrag(step.id, comp.component_id, row, ev);
        });

        // connection dots — one per link field (filled or empty)
        fields.forEach(field => {
          const pin = document.createElement('span');
          const connected = !!linkByField.get(field);
          const targetId = linkByField.get(field);
          const targetEvent = connected ? this.eventStyle(this.eventKeyOfStep(targetId)) : null;
          const pinCrossEvent = !!connected && this.eventKeyOfStep(targetId) !== this.eventKeyOfStep(step.id);
          pin.className = `bp-pin bp-pin-out bp-pin-dot${connected ? ' is-connected' : ''}${pinCrossEvent ? ' is-cross-event' : ''}`;
          const color2 = pinCrossEvent ? 'var(--bp-wire-false)' : (WIRE_COLORS[field] || WIRE_COLORS._data);
          pin.style.setProperty('--bp-pin-color', color2);
          pin.dataset.field = field;
          const short = FIELD_SHORT[field] ?? '';
          if (short) {
            const lbl = document.createElement('span');
            lbl.className = 'bp-pin-label';
            lbl.textContent = short;
            pin.appendChild(lbl);
          }
          pin.title = pinCrossEvent
            ? `Link:  ${field} → ${targetId}  — «${targetEvent.label}» is a different event, so this link is ignored (double-click to delete)`
            : connected
              ? `Link:  ${field} → ${targetId}  (drag to redirect; double-click to delete)`
              : `Drag to create a link (${field})`;
          pin.addEventListener('pointerdown', ev => {
            if (ev.button !== 0) return;
            ev.preventDefault();
            ev.stopPropagation();
            this.startConnect(step.id, comp.component_id, field, pin, ev);
          });
          pin.addEventListener('click', ev => ev.stopPropagation());
          pin.addEventListener('dblclick', ev => {
            ev.stopPropagation();
            if (connected) this.clearLink(step.id, comp.component_id, field);
          });
          row.appendChild(pin);
          this.pinOut.set(`${step.id}:${comp.component_id}:${field}`, { pin, row, count: fields.length });
        });

        body.appendChild(row);
      });

      node.appendChild(body);
      this.bindNodeEvents(node, step);
      return node;
    }

    positionPins() {
      // place out-pins vertically along the right edge of each row
      const byRow = new Map();
      this.pinOut.forEach(({ pin, row }) => {
        if (!byRow.has(row)) byRow.set(row, []);
        byRow.get(row).push(pin);
      });
      byRow.forEach((pins, row) => {
        const h = row.offsetHeight || 36;
        const n = pins.length;
        pins.forEach((pin, i) => {
          const cy = (h * (i + 1)) / (n + 1);
          const ph = pin.offsetHeight || 12;
          pin.style.top = `${cy - ph / 2}px`;
        });
      });
    }

    // ---- event scopes -----------------------------------------
    // Each scenario event (on_end, on_recieve_objectives, …) is started by the
    // server as its OWN scenario. A step of one event can therefore never jump
    // into a step of another one — such a wire describes a transition that
    // cannot happen, so wires are never drawn across an event boundary.
    // `''` = the base block (ungrouped steps + groups without an event).
    eventKeyOfStep(stepId) {
      if (!this._scopeCache) this._scopeCache = new Map();
      if (this._scopeCache.has(stepId)) return this._scopeCache.get(stepId);
      let key = '';
      if (this.editor.getStepEventKey) {
        key = this.editor.getStepEventKey(stepId) || '';
      } else {
        const gid = this.stepGroup.get(stepId);
        const group = (this.viewState?.groups || []).find(g => g.id === gid);
        key = this.editor.isKnownScenarioEvent?.(group?.event_key) ? group.event_key : '';
      }
      this._scopeCache.set(stepId, key);
      return key;
    }

    eventStyle(eventKey) {
      const style = this.editor.getEventStyle?.(eventKey);
      if (style) return style;
      const isEvent = this.editor.isKnownScenarioEvent?.(eventKey);
      return {
        key: isEvent ? eventKey : '',
        isEvent: !!isEvent,
        color: '#64748b',
        icon: 'fa-solid fa-layer-group',
        label: isEvent ? (this.editor.getGroupEventLabel?.(eventKey) || eventKey) : 'No event'
      };
    }

    stepsShareEvent(a, b) {
      return this.eventKeyOfStep(a) === this.eventKeyOfStep(b);
    }

    // Event block of a group ('' = base block).
    groupEventKey(groupId) {
      const group = (this.viewState?.groups || []).find(g => g.id === groupId);
      return group ? this.eventStyle(group.event_key).key : '';
    }

    // ---- link/topology helpers --------------------------------
    // Any component of the SAME event block links explicitly to this step.
    hasExplicitIncoming(stepId) {
      const steps = this.editor.scenarioData.steps || [];
      const scope = this.eventKeyOfStep(stepId);
      for (const s of steps) {
        if (this.eventKeyOfStep(s.id) !== scope) continue;
        for (const c of (s.components || [])) {
          if (!c) continue;
          for (const l of (this.editor.getComponentLinks(c) || [])) {
            if (l.targetId === stepId) return true;
          }
        }
      }
      return false;
    }

    hasIncoming(stepId) {
      if (this.hasExplicitIncoming(stepId)) return true;
      // A step also has an incoming edge if the PREVIOUS step in list order
      // falls through to it (i.e. there is a valid implicit edge into it).
      const order = this.flatOrder || [];
      const idx = order.indexOf(stepId);
      if (idx > 0) {
        const prev = order[idx - 1];
        const prevStep = this.editor.findStep?.(prev);
        // Same condition as getImplicitEdges(): prev has no explicit out AND
        // this step is not itself an explicit jump target — and both steps
        // must belong to the same event block.
        if (prevStep && this.stepsShareEvent(prev, stepId)
            && !this.stepHasExplicitOut(prevStep) && !this.hasExplicitIncoming(stepId)) {
          return true;
        }
      }
      return false;
    }

    // Only links that can actually fire count: a stored link into another
    // event never runs, so it must not suppress the fall-through of its block.
    stepHasExplicitOut(step) {
      const key = this.eventKeyOfStep(step.id);
      return (step.components || []).some(c => (this.editor.getComponentLinks(c) || [])
        .some(l => this.eventKeyOfStep(l.targetId) === key));
    }

    // `crossEvent` edges exist in the data but are never drawn: the two steps
    // belong to different scenario blocks, which the server runs separately.
    getEdges() {
      const edges = [];
      (this.editor.scenarioData.steps || []).forEach(step => {
        const fromKey = this.eventKeyOfStep(step.id);
        (step.components || []).filter(Boolean).forEach(comp => {
          (this.editor.getComponentLinks(comp) || []).forEach(link => {
            edges.push({
              from: step.id, to: link.targetId, comp, field: link.field,
              crossEvent: this.eventKeyOfStep(link.targetId) !== fromKey
            });
          });
        });
      });
      return edges;
    }

    getImplicitEdges() {
      const order = this.flatOrder || [];
      const edges = [];
      for (let i = 0; i < order.length - 1; i++) {
        const fromId = order[i];
        const toId = order[i + 1];
        const fromStep = this.editor.findStep?.(fromId);
        if (!fromStep) continue;
        // "Next step in list order" only means something inside ONE event
        // block: after the last step of `on_end` the flow does not continue
        // into `on_recieve_objectives` — that block has its own trigger.
        if (!this.stepsShareEvent(fromId, toId)) continue;
        // The step's own explicit links define its flow -> no fall-through.
        if (this.stepHasExplicitOut(fromStep)) continue;
        // Don't draw a fall-through INTO a step that is an explicit jump target:
        // that step is reached via the explicit wire, not "by list order".
        // (Fixes the spurious dashed line between two branch endpoints.)
        if (this.hasExplicitIncoming(toId)) continue;
        edges.push({ from: fromId, to: toId, eventKey: this.eventKeyOfStep(fromId) });
      }
      return edges;
    }

    // ---- auto layout (clustered by group) ---------------------
    computeDepths(steps) {
      const depth = new Map();
      // Cross-event links are dead data — they must not stretch the layout.
      const edges = this.getEdges().filter(e => !e.crossEvent);
      const calc = (id, guard) => {
        if (depth.has(id)) return depth.get(id);
        if (guard.has(id)) return 0;
        guard.add(id);
        let d = 0;
        edges.forEach(({ from, to }) => {
          if (to === id && from !== id) d = Math.max(d, calc(from, guard) + 1);
        });
        guard.delete(id);
        depth.set(id, d);
        return d;
      };
      steps.forEach(s => calc(s.id, new Set()));
      return depth;
    }

    layoutCluster(ids, depth, originX, originY, positions) {
      // place ids into columns by (local) depth, top-to-bottom
      if (ids.length === 0) return { w: 0, h: 0 };
      const minDepth = Math.min(...ids.map(id => depth.get(id) || 0));
      const cols = new Map();
      ids.forEach(id => {
        const c = (depth.get(id) || 0) - minDepth;
        if (!cols.has(c)) cols.set(c, []);
        cols.get(c).push(id);
      });
      let maxH = 0;
      let maxX = originX;
      Array.from(cols.keys()).sort((a, b) => a - b).forEach(c => {
        let y = originY;
        const x = originX + c * (NODE_W + COL_GAP);
        cols.get(c).forEach(id => {
          const node = this.nodeEls.get(id);
          const h = node ? node.offsetHeight : 120;
          positions[id] = { x, y };
          y += h + ROW_GAP;
        });
        maxH = Math.max(maxH, y - originY);
        maxX = Math.max(maxX, x + NODE_W);
      });
      return { w: maxX - originX, h: maxH };
    }

    computeLayout(steps) {
      const positions = {};
      const depth = this.computeDepths(steps);
      const groups = this.viewState.groups || [];
      const ungrouped = this.viewState.ungroupedSteps || [];

      let y = 60 + GROUP_HEADER + GROUP_PAD;
      const baseX = 60 + GROUP_PAD;

      groups.forEach(group => {
        const ids = (group.steps || []).filter(id => this.nodeEls.has(id));
        if (ids.length === 0) {
          // reserve a small band + rect for an empty group
          this.bp.groupRects[group.id] = { x: 40, y: y - GROUP_HEADER - GROUP_PAD, w: 280, h: 160 };
          y += 160 + GROUP_GAP;
          return;
        }
        const { h } = this.layoutCluster(ids, depth, baseX, y, positions);
        y += h + GROUP_PAD + GROUP_HEADER + GROUP_GAP;
      });

      if (ungrouped.length) {
        const { h } = this.layoutCluster(ungrouped, depth, 60, y, positions);
        y += h + GROUP_GAP;
      }

      return positions;
    }

    ensurePositions(steps) {
      const positions = this.bp.positions;
      const missing = steps.filter(s => !positions[s.id]
        || !Number.isFinite(positions[s.id].x)
        || !Number.isFinite(positions[s.id].y));
      if (missing.length === 0) return;

      // Fresh graph (most nodes missing): full clustered layout.
      if (missing.length > steps.length / 2) {
        const computed = this.computeLayout(steps);
        Object.entries(computed).forEach(([id, p]) => { positions[id] = p; });
      }

      // Place any node still missing near its group (or stacked).
      let fallbackY = 60;
      steps.forEach(s => {
        if (positions[s.id] && Number.isFinite(positions[s.id].x)) return;
        const gid = this.stepGroup.get(s.id);
        if (gid) {
          const r = this.groupRect(gid);
          // drop it just inside the group, below existing members
          positions[s.id] = { x: r.x + GROUP_PAD, y: r.y + r.h };
        } else {
          positions[s.id] = { x: 60, y: fallbackY };
          const node = this.nodeEls.get(s.id);
          fallbackY += (node ? node.offsetHeight : 120) + ROW_GAP;
        }
      });
    }

    autoLayoutAll() {
      this.bp.positions = {};
      this.bp.groupRects = {};
      this.editor.dirty.markDirty?.();
      this.render();
      // After auto-layout, snapshot each frame so its size persists.
      this.groupEls.forEach((_, gid) => this.syncGroupRect(gid));
      this.fitToView();
    }

    // ---- wires ------------------------------------------------
    clearWires() {
      while (this.wires.firstChild) this.wires.removeChild(this.wires.firstChild);
    }

    nodeInPinPos(stepId) {
      const node = this.nodeEls.get(stepId);
      const pos = this.bp.positions[stepId];
      if (!node || !pos) return null;
      const header = node.querySelector('.bp-node-header');
      const hy = header ? header.offsetHeight / 2 : 16;
      return { x: pos.x - 2, y: pos.y + hy };
    }

    nodeOutCenter(stepId) {
      const node = this.nodeEls.get(stepId);
      const pos = this.bp.positions[stepId];
      if (!node || !pos) return null;
      return { x: pos.x + node.offsetWidth + 2, y: pos.y + node.offsetHeight / 2 };
    }

    pinOutPos(stepId, componentId, field) {
      const entry = this.pinOut.get(`${stepId}:${componentId}:${field}`);
      const node = this.nodeEls.get(stepId);
      const pos = this.bp.positions[stepId];
      if (!entry || !node || !pos) return null;
      const row = entry.row;
      const pin = entry.pin;
      const x = pos.x + node.offsetWidth + 2;
      const y = pos.y + row.offsetTop + pin.offsetTop + (pin.offsetHeight || 12) / 2;
      return { x, y };
    }

    drawWires() {
      this.clearWires();
      const allIds = new Set((this.editor.scenarioData.steps || []).map(s => s.id));

      // implicit "by list order" arrows (dashed) — tinted with the colour of the
      // event they belong to, so a dashed line never looks like it leaves its block
      this.getImplicitEdges().forEach(({ from, to, eventKey }) => {
        if (!allIds.has(to)) return;
        const a = this.nodeOutCenter(from);
        const b = this.nodeInPinPos(to);
        if (!a || !b) return;
        const path = document.createElementNS(SVG_NS, 'path');
        path.setAttribute('d', this.bezier(a, b));
        path.setAttribute('class', 'bp-wire bp-wire-implicit');
        const style = this.eventStyle(eventKey);
        if (style.isEvent) {
          path.style.stroke = style.color;
          path.style.opacity = '.55';
        }
        this.wires.appendChild(path);
      });

      // explicit links
      this.getEdges().forEach(({ from, to, comp, field, crossEvent }) => {
        // A link between two events can never fire (each event is its own
        // scenario) — no wire is drawn for it. The source row and the pin show
        // a warning marker instead, so the data problem stays visible.
        if (crossEvent) return;
        if (!allIds.has(to)) return;
        const a = this.pinOutPos(from, comp.component_id, field);
        const b = this.nodeInPinPos(to);
        if (!a || !b) return;

        const color = WIRE_COLORS[field] || WIRE_COLORS._data;
        const d = this.bezier(a, b);

        const hit = document.createElementNS(SVG_NS, 'path');
        hit.setAttribute('d', d);
        hit.setAttribute('class', 'bp-wire-hit');

        const path = document.createElementNS(SVG_NS, 'path');
        path.setAttribute('d', d);
        path.setAttribute('class', 'bp-wire');
        path.style.stroke = color;
        path.style.color = color;

        hit.addEventListener('mouseenter', () => path.classList.add('is-highlight'));
        hit.addEventListener('mouseleave', () => path.classList.remove('is-highlight'));
        // Single click = highlight the target step (no destructive action).
        hit.addEventListener('click', (ev) => { ev.stopPropagation(); this.flashNode(to); });
        // Double click = remove this link. Stop propagation so the canvas
        // dblclick handler does NOT also create a step underneath the wire.
        hit.addEventListener('dblclick', (ev) => {
          ev.preventDefault();
          ev.stopPropagation();
          this.clearLink(from, comp.component_id, field);
        });

        this.wires.appendChild(path);
        this.wires.appendChild(hit);
      });
    }

    bezier(a, b) {
      const dx = Math.max(60, Math.abs(b.x - a.x) * 0.5);
      return `M ${a.x} ${a.y} C ${a.x + dx} ${a.y}, ${b.x - dx} ${b.y}, ${b.x} ${b.y}`;
    }

    scheduleWireRedraw() {
      if (this._raf) return;
      this._raf = requestAnimationFrame(() => {
        this._raf = null;
        this.drawWires();
      });
    }

    // ---- connection drag (create / redirect links) ------------
    startConnect(stepId, componentId, field, pin, e) {
      this.cancelConnect();
      const a = this.pinOutPos(stepId, componentId, field);
      if (!a) return;
      pin.classList.add('is-connecting');

      const tmp = document.createElementNS(SVG_NS, 'path');
      tmp.setAttribute('class', 'bp-wire bp-wire-temp');
      const color = WIRE_COLORS[field] || WIRE_COLORS._data;
      tmp.style.stroke = color;
      this.wires.appendChild(tmp);

      this._connect = { stepId, componentId, field, pin, a, tmp };

      // Only steps of the SAME event can receive this wire. Dim the rest while
      // the wire is being dragged: it is the clearest possible explanation of
      // why a connection cannot be made.
      const fromKey = this.eventKeyOfStep(stepId);
      this.world.classList.add('is-linking');
      this.nodeEls.forEach((n, id) => {
        n.classList.toggle('is-event-mismatch', this.eventKeyOfStep(id) !== fromKey);
      });

      const toWorld = ev => ({
        x: (ev.clientX - this.canvas.getBoundingClientRect().left - this.view.x) / this.view.scale,
        y: (ev.clientY - this.canvas.getBoundingClientRect().top - this.view.y) / this.view.scale
      });

      this.startPointerDrag(e, {
        move: ev => {
          const b = toWorld(ev);
          tmp.setAttribute('d', this.bezier(a, b));
          // hover highlight target node (only a step of the same event can take it)
          const node = this.nodeAtClient(ev.clientX, ev.clientY);
          this.world.querySelectorAll('.bp-node.is-drop-target, .bp-node.is-drop-blocked')
            .forEach(n => n.classList.remove('is-drop-target', 'is-drop-blocked'));
          const targetId = node?.dataset.stepId;
          if (node && targetId && targetId !== stepId) {
            node.classList.add(this.stepsShareEvent(stepId, targetId) ? 'is-drop-target' : 'is-drop-blocked');
          }
          // An empty spot inside a group of ANOTHER event cannot take the wire
          // either (dropping there would create a step in the wrong block).
          const hoverGroup = this.groupAtPoint(b.x, b.y);
          const groupBlocked = !!hoverGroup && this.groupEventKey(hoverGroup) !== fromKey;
          this.groupEls.forEach((frame, gid) => {
            frame.classList.toggle('is-drop-blocked', gid === hoverGroup && groupBlocked);
          });
          // Let the temp wire show the answer before the user releases.
          const blocked = groupBlocked || !!(node && targetId && !this.stepsShareEvent(stepId, targetId));
          tmp.style.stroke = blocked ? 'var(--bp-wire-false)' : color;
        },
        end: (ev, cancelled) => {
          pin.classList.remove('is-connecting');
          this.world.classList.remove('is-linking');
          this.world.querySelectorAll('.bp-node.is-drop-target, .bp-node.is-drop-blocked, .bp-node.is-event-mismatch')
            .forEach(n => n.classList.remove('is-drop-target', 'is-drop-blocked', 'is-event-mismatch'));
          this.groupEls.forEach(frame => frame.classList.remove('is-drop-blocked'));
          tmp.remove();
          this._connect = null;
          if (cancelled || !ev) { this.drawWires(); return; }

          const node = this.nodeAtClient(ev.clientX, ev.clientY);
          if (node) {
            const targetId = node.dataset.stepId;
            if (targetId && targetId !== stepId) {
              this.applyLink(stepId, componentId, field, targetId);
            } else {
              this.drawWires();
            }
          } else {
            // dropped on empty canvas -> create a linked step there.
            // If the drop landed inside a group band, add it to that group;
            // otherwise keep it ungrouped (don't fall into the default group).
            const b = toWorld(ev);
            const dropGroup = this.groupAtPoint(b.x, b.y);
            // Never create a step in another event just to link it: the link would
            // be dead. Explain instead.
            if (dropGroup && this.groupEventKey(dropGroup) !== fromKey) {
              this.explainCrossEvent(fromKey, this.groupEventKey(dropGroup));
              this.drawWires();
              return;
            }
            const newId = this.editor.addNewStepAt?.(b.x - NODE_W / 2, b.y, {
              group: dropGroup || null,
              ungrouped: !dropGroup
            });
            if (newId) this.applyLink(stepId, componentId, field, newId);
            else this.drawWires();
          }
        }
      });
      // On touch the temp wire follows the finger; the canvas must not pan.
      this.canvas.classList.remove('is-panning');
    }

    cancelConnect() {
      if (!this._connect) return;
      this._connect.pin?.classList.remove('is-connecting');
      this._connect.tmp?.remove();
      this._connect = null;
      this.world.classList.remove('is-linking');
      this.world.querySelectorAll('.bp-node.is-event-mismatch, .bp-node.is-drop-blocked')
        .forEach(n => n.classList.remove('is-event-mismatch', 'is-drop-blocked'));
      this.groupEls.forEach(frame => frame.classList.remove('is-drop-blocked'));
    }

    // ---- component drag (reorder within a step / move between steps) -------
    startCompDrag(fromStepId, componentId, row, e) {
      // Floating ghost that follows the cursor.
      const ghost = document.createElement('div');
      ghost.className = 'bp-comp-ghost';
      ghost.textContent = row.querySelector('.bp-comp-name')?.textContent || 'Component';
      document.body.appendChild(ghost);
      const moveGhost = (cx, cy) => { ghost.style.left = `${cx + 12}px`; ghost.style.top = `${cy + 12}px`; };
      moveGhost(e.clientX, e.clientY);
      row.classList.add('is-dragging-out');
      let moved = false;

      const clearMarks = () => {
        this.world.querySelectorAll('.bp-node.is-drop-target')
          .forEach(n => n.classList.remove('is-drop-target'));
        this.world.querySelectorAll('.bp-comp.is-drop-before, .bp-comp.is-drop-after')
          .forEach(r => r.classList.remove('is-drop-before', 'is-drop-after'));
      };

      // Find the component row under the cursor + whether to drop before/after it.
      const rowAtClient = (cx, cy) => {
        const el = document.elementFromPoint(cx, cy);
        const r = el ? el.closest('.bp-comp') : null;
        if (!r) return null;
        const rect = r.getBoundingClientRect();
        const after = (cy - rect.top) > rect.height / 2;
        return { row: r, after };
      };

      this.startPointerDrag(e, {
        move: ev => {
          if (Math.abs(ev.clientX - e.clientX) > 2 || Math.abs(ev.clientY - e.clientY) > 2) moved = true;
          moveGhost(ev.clientX, ev.clientY);
          clearMarks();
          const node = this.nodeAtClient(ev.clientX, ev.clientY);
          if (node) node.classList.add('is-drop-target');
          const hit = rowAtClient(ev.clientX, ev.clientY);
          if (hit && hit.row !== row) {
            hit.row.classList.add(hit.after ? 'is-drop-after' : 'is-drop-before');
          }
        },
        end: (ev, cancelled) => {
          ghost.remove();
          row.classList.remove('is-dragging-out');
          clearMarks();
          if (cancelled || !ev) return;

          if (!moved) { this.editor.openEditModal?.(fromStepId, componentId); return; }

          const node = this.nodeAtClient(ev.clientX, ev.clientY);
          if (!node) return;
          const toStepId = node.dataset.stepId;
          if (!toStepId) return;

          const hit = rowAtClient(ev.clientX, ev.clientY);
          let beforeId = null;
          if (hit && hit.row !== row) {
            const targetId = hit.row.dataset.componentId;
            beforeId = hit.after ? this.nextComponentId(toStepId, targetId) : targetId;
          }

          const ok = this.editor.moveComponent?.(fromStepId, componentId, toStepId, beforeId);
          if (ok) {
            this.editor.renderAll();
            this.flashNode(toStepId);
          }
        }
      });
    }

    // id of the component that follows targetId in its step (or null = append).
    nextComponentId(stepId, targetId) {
      const step = this.editor.findStep?.(stepId);
      if (!step) return null;
      const comps = (step.components || []).filter(Boolean);
      const i = comps.findIndex(c => c.component_id === targetId);
      if (i < 0 || i + 1 >= comps.length) return null;
      return comps[i + 1].component_id;
    }

    nodeAtClient(clientX, clientY) {
      const el = document.elementFromPoint(clientX, clientY);
      return el ? el.closest('.bp-node') : null;
    }

    applyLink(stepId, componentId, field, targetId) {
      // Different events are different scenarios on the server: such a link
      // would never fire. Refuse it and say why instead of writing dead data.
      if (!this.stepsShareEvent(stepId, targetId)) {
        this.explainCrossEventLink(stepId, targetId);
        this.drawWires();
        return false;
      }
      const comp = this.editor.findComponent?.(stepId, componentId);
      if (!comp) { this.render(); return false; }
      comp[field] = targetId;
      this.editor.dirty.markDirty?.();
      this.editor.renderAll();   // keeps list + graph in sync
      this.flashNode(targetId);
      return true;
    }

    // One shared explanation for every place that can attempt a cross-event
    // link (wire drag, link on drop, dropping into a foreign group).
    explainCrossEvent(fromKey, toKey) {
      const a = this.eventStyle(fromKey);
      const b = this.eventStyle(toKey);
      const message = `«${a.label}» and «${b.label}» are different events: each one runs as its own scenario, so steps cannot link between them.`;
      this.editor.showToast?.(message, 'warning');
    }

    explainCrossEventLink(fromStepId, toStepId) {
      this.explainCrossEvent(this.eventKeyOfStep(fromStepId), this.eventKeyOfStep(toStepId));
      this.flashBlocked?.(toStepId);
    }

    // Shake the refused target a little — the wire is already gone by then.
    flashBlocked(stepId) {
      const node = this.nodeEls.get(stepId);
      if (!node) return;
      node.classList.remove('is-blocked-flash');
      void node.offsetWidth;
      node.classList.add('is-blocked-flash');
      setTimeout(() => node.classList.remove('is-blocked-flash'), 700);
    }

    clearLink(stepId, componentId, field) {
      const comp = this.editor.findComponent?.(stepId, componentId);
      if (!comp) return;
      if (!comp[field]) return;
      delete comp[field];
      this.editor.dirty.markDirty?.();
      this.editor.renderAll();
    }

    // ---- transform / camera -----------------------------------
    applyTransform() {
      const { x, y, scale } = this.view;
      // Round the translation to whole pixels: fractional offsets on a
      // GPU-composited layer are what cause the intermittent text blur.
      this.world.style.transform = `translate(${Math.round(x)}px, ${Math.round(y)}px) scale(${scale})`;
      if (this.zoomLabel) this.zoomLabel.textContent = `${Math.round(scale * 100)}%`;
      this.markAnimating();
      this.drawMinimap();
    }

    // Promote the layer only briefly around interactions, then drop the hint so
    // the browser re-rasterizes crisply at the current zoom.
    markAnimating() {
      this.world.classList.add('is-animating');
      clearTimeout(this._animTimer);
      this._animTimer = setTimeout(() => {
        this.world.classList.remove('is-animating');
      }, 220);
    }

    setScale(scale, centerX, centerY) {
      const old = this.view.scale;
      const next = Math.min(MAX_SCALE, Math.max(MIN_SCALE, scale));
      if (next === old) return;
      const rect = this.canvas.getBoundingClientRect();
      const cx = centerX != null ? centerX - rect.left : rect.width / 2;
      const cy = centerY != null ? centerY - rect.top : rect.height / 2;
      const wx = (cx - this.view.x) / old;
      const wy = (cy - this.view.y) / old;
      this.view.scale = next;
      this.view.x = cx - wx * next;
      this.view.y = cy - wy * next;
      this.applyTransform();
      this.saveView();
    }

    zoomIn() { this.setScale(this.view.scale * 1.2); }
    zoomOut() { this.setScale(this.view.scale / 1.2); }
    resetZoom() { this.view.scale = 1; this.applyTransform(); this.saveView(); }

    // World-space bounds of everything currently drawn (private helper so
    // "fit the graph" and "focus one event" share one implementation).
    graphBounds(filterStepId = null) {
      let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
      const extend = (x, y, w, h) => {
        minX = Math.min(minX, x); minY = Math.min(minY, y);
        maxX = Math.max(maxX, x + w); maxY = Math.max(maxY, y + h);
      };
      this.nodeEls.forEach((node, id) => {
        if (filterStepId && !filterStepId(id)) return;
        const p = this.bp.positions[id];
        if (p) extend(p.x, p.y, node.offsetWidth, node.offsetHeight);
      });
      if (!filterStepId) {
        this.groupEls.forEach((frame, id) => {
          const r = this.groupRect(id);
          extend(r.x, r.y, r.w, r.h);
        });
      }
      return { minX, minY, maxX, maxY };
    }

    fitToView() {
      const { minX, minY, maxX, maxY } = this.graphBounds();
      if (!Number.isFinite(minX)) { this.resetZoom(); return; }
      this.fitBounds(minX, minY, maxX, maxY, this.topOverlayInset());
    }

    // Height of the floating event legend (it covers the top of the canvas).
    topOverlayInset() {
      const legend = document.getElementById('bp-event-legend');
      if (!legend || !legend.offsetHeight) return 0;
      return legend.offsetHeight + 14;
    }

    // Centre + zoom the camera on one rectangle of the world.
    fitBounds(minX, minY, maxX, maxY, topInset = 0) {
      const rect = this.canvas.getBoundingClientRect();
      // Keep the margin proportional: 80px is right on a desktop canvas but
      // eats most of a 390px phone screen.
      const pad = Math.max(16, Math.min(80, Math.min(rect.width, rect.height) * 0.08));
      // Overlays: the event legend at the top, the toolbar at the bottom.
      const inset = this.toolbarInset();
      const usableH = Math.max(120, rect.height - inset - topInset);
      const sx = (rect.width - pad * 2) / Math.max(1, maxX - minX);
      const sy = (usableH - pad * 2) / Math.max(1, maxY - minY);
      let scale = Math.min(MAX_SCALE, Math.max(MIN_SCALE, Math.min(sx, sy, 1)));
      // On a phone, "fit the whole graph" shrinks a 20-step scenario into an
      // unreadable speck. Prefer legibility: show the start of the graph at a
      // readable zoom and let the user pan from there.
      // "Compact" = phone portrait OR phone landscape: in both cases fitting
      // the whole graph would shrink it to unreadable specks.
      const compact = rect.width < 600 || rect.height < 520;
      if (compact) scale = Math.max(scale, 0.44);
      this.view.scale = scale;
      const fitsWidth = (maxX - minX) * scale <= rect.width - pad * 2;
      const fitsHeight = (maxY - minY) * scale <= usableH - pad * 2;
      this.view.x = fitsWidth
        ? pad - minX * scale + (rect.width - pad * 2 - (maxX - minX) * scale) / 2
        : pad - minX * scale;
      this.view.y = fitsHeight
        ? topInset + pad - minY * scale + (usableH - pad * 2 - (maxY - minY) * scale) / 2
        : topInset + pad - minY * scale;
      this.applyTransform();
      this.saveView();
    }

    // ---- event legend (colours + focus) ------------------------
    // One chip per event that is actually present in the graph. Clicking a chip
    // frames that event's steps, which is the fastest way to jump between the
    // blocks of a scenario on a small screen.
    eventSummary() {
      const counts = new Map();
      this.nodeEls.forEach((_, id) => {
        const key = this.eventKeyOfStep(id);
        counts.set(key, (counts.get(key) || 0) + 1);
      });
      const order = Object.keys(this.editor.scenarioEvents || {});
      const items = Array.from(counts.entries()).map(([key, count]) => {
        const style = this.eventStyle(key);
        return { ...style, count };
      });
      items.sort((a, b) => {
        if (a.isEvent !== b.isEvent) return a.isEvent ? 1 : -1;   // "No event" first
        const ia = order.indexOf(a.key);
        const ib = order.indexOf(b.key);
        return (ia < 0 ? 99 : ia) - (ib < 0 ? 99 : ib);
      });
      return items;
    }

    renderEventLegend() {
      const box = document.getElementById('bp-event-legend');
      if (!box) return;
      const items = this.eventSummary();
      if (items.length === 0) { box.innerHTML = ''; box.classList.add('is-empty'); return; }
      box.classList.remove('is-empty');
      box.innerHTML = items.map(item => `
        <button type="button" class="bp-event-chip${item.isEvent ? ' is-event' : ''}${this._focusEventKey === item.key ? ' is-active' : ''}"
                data-event-key="${this.esc(item.key)}" style="--event-color:${item.color}"
                title="«${this.esc(item.label)}»: ${item.count} step(s) — click to focus. Steps of one event never link to another event.">
          <i class="${item.icon}"></i><span>${this.esc(item.label)}</span><b>${item.count}</b>
        </button>`).join('');
      box.querySelectorAll('.bp-event-chip').forEach(btn => {
        btn.addEventListener('pointerdown', e => e.stopPropagation());
        btn.addEventListener('click', e => {
          e.stopPropagation();
          this.focusEvent(btn.dataset.eventKey || '');
        });
      });
    }

    focusEvent(eventKey) {
      const ids = [];
      this.nodeEls.forEach((_, id) => { if (this.eventKeyOfStep(id) === eventKey) ids.push(id); });
      if (ids.length === 0) return;
      const inScope = new Set(ids);
      const { minX, minY, maxX, maxY } = this.graphBounds(id => inScope.has(id));
      if (!Number.isFinite(minX)) return;
      this._focusEventKey = eventKey;
      this.fitBounds(minX, minY, maxX, maxY, this.topOverlayInset());
      this.renderEventLegend();
      ids.forEach(id => this.flashNode(id));
    }

    // Height covered by the floating toolbar (phone layout only).
    toolbarInset() {
      if (window.innerWidth > 767) return 0;
      const bar = this.canvas.querySelector('.bp-toolbar');
      return bar ? bar.offsetHeight + 16 : 0;
    }

    flashNode(stepId) {
      const node = this.nodeEls.get(stepId);
      if (!node) return;
      node.classList.remove('is-flash');
      void node.offsetWidth;
      node.classList.add('is-flash');
    }

    focusStep(stepId) {
      const node = this.nodeEls.get(stepId);
      const p = this.bp.positions[stepId];
      if (!node || !p) return;
      const rect = this.canvas.getBoundingClientRect();
      this.view.x = rect.width / 2 - (p.x + node.offsetWidth / 2) * this.view.scale;
      this.view.y = rect.height / 2 - (p.y + node.offsetHeight / 2) * this.view.scale;
      this.applyTransform();
      this.saveView();
      this.flashNode(stepId);
    }

    // ---- interaction ------------------------------------------
    bindNodeEvents(node, step) {
      const header = node.querySelector('.bp-node-header');
      const dragZone = header.querySelector('.bp-drag-zone');
      const titleEl = header.querySelector('[data-role="title"]');
      const badge = header.querySelector('.bp-logic-badge');

      node.querySelectorAll('[data-bp-action]').forEach(btn => {
        btn.addEventListener('pointerdown', e => e.stopPropagation());
        btn.addEventListener('click', e => {
          e.stopPropagation();
          const action = btn.dataset.bpAction;
          if (action === 'edit-step') {
            this.editor.switchViewMode?.('list');
            requestAnimationFrame(() => {
              const card = document.getElementById(`step-card-${step.id}`);
              card?.scrollIntoView({ behavior: 'smooth', block: 'center' });
              card?.classList.add('ring-2', 'ring-blue-500');
              setTimeout(() => card?.classList.remove('ring-2', 'ring-blue-500'), 1500);
            });
          } else if (action === 'add-comp') {
            // Stay in the graph; open the shared add-component menu here.
            this.editor.showAddComponentMenu(btn, step.id);
          } else if (action === 'del-step') {
            this.editor.handleDelete(step.id, null);
          } else if (action === 'settings') {
            this.editor.openStepQuickPopover?.(step.id, btn.getBoundingClientRect());
          }
        });
      });

      // Logic badge: click cycles the completion logic (AND → OR → 1·2·3).
      badge?.addEventListener('pointerdown', e => e.stopPropagation());
      badge?.addEventListener('click', e => {
        e.stopPropagation();
        this.editor.cycleCompletionLogic?.(step.id);
      });

      // Title: single click = inline rename (mouse), double click = quick
      // popover. On touch the title doubles as a drag handle — it is the
      // biggest free area of the header, and a tap no longer renames.
      titleEl?.addEventListener('pointerdown', e => {
        e.stopPropagation();
        if (e.pointerType === 'touch' && e.button === 0) {
          e.preventDefault();
          this.startNodeDrag(node, step, e);
        }
      });
      titleEl?.addEventListener('dblclick', e => {
        e.stopPropagation();
        this.editor.openStepQuickPopover?.(step.id, header.getBoundingClientRect());
      });
      titleEl?.addEventListener('click', e => {
        e.stopPropagation();
        // A finger tap should not pop up the keyboard: on touch the rename is
        // available from the quick-settings popover (double tap).
        if (this.lastPointerType === 'touch') return;
        this.startNodeRename(titleEl, step.id);
      });

      node.querySelectorAll('.bp-comp').forEach(row => {
        row.addEventListener('pointerdown', e => {
          if (e.target.closest('.bp-pin') || e.target.closest('.bp-comp-icon') || e.target.closest('[data-bp-comp-action]')) return;
          e.stopPropagation();
        });
        row.addEventListener('click', e => {
          // pins handle their own clicks; the icon is a drag handle that also
          // opens the editor on a plain click (handled in startCompDrag).
          // action buttons handle their own clicks too.
          if (e.target.closest('.bp-pin') || e.target.closest('.bp-comp-icon') || e.target.closest('[data-bp-comp-action]')) return;
          e.stopPropagation();
          const componentId = row.dataset.componentId;
          if (componentId) this.editor.openEditModal(step.id, componentId);
        });
      });

      // Node moves via the drag-zone grip (so header clicks/edits are safe).
      dragZone?.addEventListener('pointerdown', e => {
        if (e.button !== 0) return;
        e.preventDefault();
        e.stopPropagation();
        this.startNodeDrag(node, step, e);
      });

      // …and by the rest of the header on touch screens, where a 20px grip is
      // a cruel target. Buttons / title / badge / pins keep their own handlers.
      header.addEventListener('pointerdown', e => {
        if (e.button !== 0) return;
        if (e.pointerType !== 'touch') return;
        if (e.target.closest('.bp-node-actions, [data-bp-action], .bp-node-title, .bp-logic-badge, .bp-pin')) return;
        e.preventDefault();
        e.stopPropagation();
        this.startNodeDrag(node, step, e);
      });

      // Tap to select on touch (reveals the row of node actions); tapping
      // elsewhere on the canvas clears the selection.
      node.addEventListener('pointerup', e => {
        if (e.pointerType !== 'touch') return;
        if (e.target.closest('.bp-node-actions, [data-bp-action], .bp-comp, .bp-pin, .bp-logic-badge')) return;
        this.selected = new Set([step.id]);
        this.selectedGroups.clear();
        this.selectedComments.clear();
        this.applySelectionStyles();
      });
    }

    startNodeRename(titleEl, stepId) {
      if (titleEl.isContentEditable) return;
      const original = stepId;
      titleEl.contentEditable = 'true';
      titleEl.classList.add('is-editing');
      titleEl.focus();
      const range = document.createRange();
      range.selectNodeContents(titleEl);
      const sel = window.getSelection();
      sel.removeAllRanges(); sel.addRange(range);

      let done = false;
      const finish = (commit) => {
        if (done) return; done = true;
        titleEl.contentEditable = 'false';
        titleEl.classList.remove('is-editing');
        const val = titleEl.textContent.trim();
        titleEl.removeEventListener('blur', onBlur);
        titleEl.removeEventListener('keydown', onKey);
        if (commit && val && val !== original && this.editor.renameStep) {
          if (!this.editor.renameStep(original, val)) { titleEl.textContent = original; }
          // renameStep triggers renderAll which rebuilds nodes
        } else {
          titleEl.textContent = original;
        }
      };
      const onBlur = () => finish(true);
      const onKey = (ev) => {
        ev.stopPropagation();
        if (ev.key === 'Enter') { ev.preventDefault(); finish(true); }
        else if (ev.key === 'Escape') { ev.preventDefault(); finish(false); }
      };
      titleEl.addEventListener('blur', onBlur);
      titleEl.addEventListener('keydown', onKey);
    }

    startNodeDrag(node, step, e) {
      // Multi-move when the grabbed node is part of any active selection
      // (nodes / groups / comments move together).
      const multi = this.selected.has(step.id) && this.hasSelection() &&
        (this.selected.size + this.selectedGroups.size + this.selectedComments.size) > 1;
      const moveSet = multi ? this.collectMoveSet() : { stepIds: new Set([step.id]), commentIds: new Set(), groupIds: new Set() };

      moveSet.stepIds.forEach(id => this.nodeEls.get(id)?.classList.add('is-dragging'));
      const startMouse = { x: e.clientX, y: e.clientY };
      const startNode = new Map();
      moveSet.stepIds.forEach(id => startNode.set(id, { ...(this.bp.positions[id] || { x: 0, y: 0 }) }));
      const startComment = new Map();
      moveSet.commentIds.forEach(id => { const c = (this.bp.comments || []).find(x => x.id === id); if (c) startComment.set(id, { x: c.x, y: c.y }); });
      const startGroupRect = new Map();
      moveSet.groupIds.forEach(gid => startGroupRect.set(gid, { ...this.groupRect(gid) }));
      let moved = false;

      this.startPointerDrag(e, {
        move: ev => {
          const dx = (ev.clientX - startMouse.x) / this.view.scale;
          const dy = (ev.clientY - startMouse.y) / this.view.scale;
          if (Math.abs(dx) > 1 || Math.abs(dy) > 1) moved = true;
          this.translateSelection(moveSet, startNode, startComment, startGroupRect, dx, dy);
          if (!multi) this.highlightDropGroup(node);
          this.scheduleWireRedraw();
        },
        end: () => {
          moveSet.stepIds.forEach(id => this.nodeEls.get(id)?.classList.remove('is-dragging'));
          this.world.querySelectorAll('.bp-group.is-drop-target')
            .forEach(g => g.classList.remove('is-drop-target'));
          if (moved) {
            let changed = false;
            // Membership re-evaluation only for single-node drags (keeps multi-move simple & predictable).
            if (!multi) changed = this.commitMembershipFromPosition(step.id, node);
            // Persist any auto-grown frames so the box keeps the new size.
            this.groupEls.forEach((_, gid) => this.syncGroupRect(gid));
            this.editor.dirty.markDirty?.();
            if (changed) this.editor.renderAll();
          }
        }
      });
    }

    nodeCenter(node) {
      const id = node.dataset.stepId;
      const p = this.bp.positions[id];
      if (!p) return null;
      return { x: p.x + node.offsetWidth / 2, y: p.y + node.offsetHeight / 2 };
    }

    groupAtPoint(cx, cy, excludeStepId = null) {
      // last (topmost) frame whose rect (excluding the dragged node) contains the point
      let found = null;
      this.groupEls.forEach((frame, gid) => {
        const r = this.groupRect(gid, excludeStepId);
        if (cx >= r.x && cx <= r.x + r.w && cy >= r.y && cy <= r.y + r.h) found = gid;
      });
      return found;
    }

    highlightDropGroup(node) {
      const c = this.nodeCenter(node);
      this.world.querySelectorAll('.bp-group.is-drop-target')
        .forEach(g => g.classList.remove('is-drop-target'));
      if (!c) return;
      const stepId = node.dataset.stepId;
      const gid = this.groupAtPoint(c.x, c.y, stepId);
      const cur = this.stepGroup.get(stepId) || null;
      if ((gid || null) !== cur) {
        if (gid) this.groupEls.get(gid)?.classList.add('is-drop-target');
        // also flag the current group as "leaving"
      }
    }

    commitMembershipFromPosition(stepId, node) {
      const c = this.nodeCenter(node);
      if (!c) return false;
      const targetGroup = this.groupAtPoint(c.x, c.y, stepId);
      const current = this.stepGroup.get(stepId) || null;
      if ((targetGroup || null) === current) return false;
      this.editor.moveStepToGroup?.(stepId, targetGroup || null);
      return true;
    }

    bindCanvasEvents() {
      // Background drag: Shift (or active "select" mode) = marquee selection;
      // otherwise pan the canvas.
      this.canvas.addEventListener('pointerdown', e => {
        if (e.button !== 0 && e.button !== 1) return;
        // A comment's BODY area must NOT block panning/marquee (it's the back
        // layer). Only its interactive controls (move-head, resize, buttons or
        // a body that is currently being edited) capture the press.
        const commentBlocks = e.target.closest('.bp-comment-head')
          || e.target.closest('.bp-comment-resize')
          || e.target.closest('.bp-comment-actions')
          || e.target.closest('.bp-comment-body.is-editing');
        if (e.target.closest('.bp-node') || e.target.closest('.bp-group') || commentBlocks ||
            e.target.closest('.bp-toolbar') || e.target.closest('.bp-legend') ||
            e.target.closest('.bp-event-legend')) return;

        const wantSelect = e.button === 0 && (e.shiftKey || this.selectMode);
        if (wantSelect) { e.preventDefault(); this.startMarquee(e); return; }

        // Plain background click clears selection.
        if (e.button === 0 && this.selected.size) this.clearSelection();

        e.preventDefault();
        this.canvas.classList.add('is-panning');
        const start = { x: e.clientX, y: e.clientY };
        const startView = { x: this.view.x, y: this.view.y };
        let moved = 0;
        this.startPointerDrag(e, {
          move: ev => {
            moved = Math.max(moved, Math.abs(ev.clientX - start.x) + Math.abs(ev.clientY - start.y));
            this.view.x = startView.x + (ev.clientX - start.x);
            this.view.y = startView.y + (ev.clientY - start.y);
            this.applyTransform();
          },
          end: (ev, cancelled) => {
            this.canvas.classList.remove('is-panning');
            this.saveView();
            // A tap (no movement) on empty canvas = double-tap-to-create on touch.
            if (!cancelled && ev && ev.pointerType === 'touch' && moved < 8) {
              this.registerCanvasTap(ev.clientX, ev.clientY);
            }
          }
        });
      });

      // ---- two-finger pinch zoom (phones / tablets) -------------
      // One finger pans or drags (handled above), two fingers zoom the camera
      // around their midpoint — the gesture people expect from a node graph.
      const touches = new Map();
      const pinchPoints = () => Array.from(touches.values());
      const onPinchMove = ev => {
        if (!touches.has(ev.pointerId)) return;
        touches.set(ev.pointerId, { x: ev.clientX, y: ev.clientY });
        if (this._pinch && touches.size >= 2) {
          if (ev.cancelable) ev.preventDefault();
          this._pinch.update(pinchPoints());
        }
      };
      // Capture phase on purpose: nodes/components stop pointerdown from
      // bubbling, and the pinch must still see the second finger.
      this.canvas.addEventListener('pointerdown', e => {
        if (e.pointerType !== 'touch') return;
        touches.set(e.pointerId, { x: e.clientX, y: e.clientY });
        if (touches.size === 2) {
          // A second finger means the user wants to zoom: abort whatever
          // one-finger drag started a moment ago.
          this._dragCancel?.();
          this.canvas.classList.remove('is-panning');
          this.beginPinch(pinchPoints());
        }
      }, { capture: true, passive: true });
      this.canvas.addEventListener('pointermove', onPinchMove, { capture: true, passive: false });
      const dropTouch = e => {
        touches.delete(e.pointerId);
        if (touches.size < 2) this.endPinch();
      };
      this.canvas.addEventListener('pointerup', dropTouch, true);
      this.canvas.addEventListener('pointercancel', dropTouch, true);
      window.addEventListener('blur', () => { touches.clear(); this.endPinch(); });

      this.canvas.addEventListener('wheel', e => {
        if (!this.active) return;
        e.preventDefault();
        const factor = e.deltaY < 0 ? 1.12 : 1 / 1.12;
        this.setScale(this.view.scale * factor, e.clientX, e.clientY);
      }, { passive: false });

      this.canvas.addEventListener('dblclick', e => {
        if (e.target.closest('.bp-node') || e.target.closest('.bp-toolbar') || e.target.closest('.bp-legend')
            || e.target.closest('.bp-event-legend')) return;
        if (e.target.closest('.bp-comment')) return;
        if (e.target.closest('.bp-group-header') || e.target.closest('.bp-group-resize')) return;
        // Ignore double-clicks that land on a connection wire (those delete the
        // link via the wire's own handler) — never create anything on top of it.
        if (e.target.closest && e.target.closest('.bp-wire-hit')) return;
        if (e.target.tagName === 'path') return;
        e.preventDefault();
        // Open a small picker near the cursor instead of creating a step directly.
        this.openCreateMenu(e.clientX, e.clientY);
      });
    }

    // Double-tap on empty canvas opens the "what to add" picker (touch only —
    // a touch double-tap does not always produce a dblclick event).
    registerCanvasTap(clientX, clientY) {
      const now = Date.now();
      const last = this._lastTap;
      if (last && now - last.t < 350 && Math.abs(last.x - clientX) < 28 && Math.abs(last.y - clientY) < 28) {
        this._lastTap = null;
        this.openCreateMenu(clientX, clientY);
        return;
      }
      this._lastTap = { t: now, x: clientX, y: clientY };
    }

    // ---- pinch zoom -------------------------------------------
    beginPinch(points) {
      if (!points || points.length < 2) return;
      const [a, b] = points;
      const rect = this.canvas.getBoundingClientRect();
      const startDist = Math.max(1, Math.hypot(a.x - b.x, a.y - b.y));
      const startScale = this.view.scale;
      // Keep the world point under the pinch midpoint pinned to the midpoint.
      const midX = (a.x + b.x) / 2;
      const midY = (a.y + b.y) / 2;
      const wx = (midX - rect.left - this.view.x) / startScale;
      const wy = (midY - rect.top - this.view.y) / startScale;
      this._pinch = {
        update: pts => {
          if (!pts || pts.length < 2) return;
          const [p, q] = pts;
          const dist = Math.max(1, Math.hypot(p.x - q.x, p.y - q.y));
          const scale = Math.min(MAX_SCALE, Math.max(MIN_SCALE, startScale * (dist / startDist)));
          const cx = (p.x + q.x) / 2;
          const cy = (p.y + q.y) / 2;
          this.view.scale = scale;
          this.view.x = cx - rect.left - wx * scale;
          this.view.y = cy - rect.top - wy * scale;
          this.applyTransform();
        }
      };
    }

    endPinch() {
      if (!this._pinch) return;
      this._pinch = null;
      this.saveView();
    }

    // ---- "what to add" picker (double-click on empty canvas) ----
    openCreateMenu(clientX, clientY) {
      this.closeCreateMenu();

      const world = this.clientToWorld(clientX, clientY);
      const groupId = this.groupAtPoint(world.x, world.y);

      const menu = document.createElement('div');
      menu.className = 'bp-create-menu';
      menu.innerHTML = `
        <button data-create="step"><i class="fa-solid fa-bolt"></i> Step</button>
        <button data-create="group"><i class="fa-solid fa-layer-group"></i> Group</button>
        <button data-create="comment"><i class="fa-solid fa-note-sticky"></i> Comment</button>`;
      document.body.appendChild(menu);
      this._createMenu = menu;

      // position near the cursor, clamped to the viewport
      const margin = 8;
      const mw = menu.offsetWidth || 160;
      const mh = menu.offsetHeight || 120;
      let left = clientX + 2;
      let top = clientY + 2;
      if(left + mw + margin > window.innerWidth) left = window.innerWidth - mw - margin;
      if(top + mh + margin > window.innerHeight) top = window.innerHeight - mh - margin;
      menu.style.left = `${Math.max(margin, left)}px`;
      menu.style.top = `${Math.max(margin, top)}px`;

      const act = (kind) => {
        this.closeCreateMenu();
        if(kind === 'step') {
          this.editor.addNewStepAt?.(world.x - NODE_W / 2, world.y, { group: groupId || null, ungrouped: !groupId });
        } else if(kind === 'group') {
          // place an empty group frame centred on the cursor
          this.editor.addNewGroup?.({ x: world.x - 150, y: world.y - 40, w: 300, h: 200 });
        } else if(kind === 'comment') {
          this.addComment?.(world.x - 120, world.y - 20);
        }
      };

      menu.querySelectorAll('[data-create]').forEach(btn => {
        btn.addEventListener('pointerdown', ev => ev.stopPropagation());
        btn.addEventListener('click', ev => { ev.stopPropagation(); act(btn.dataset.create); });
      });

      // dismiss on outside click / escape / scroll-zoom
      this._createMenuDismiss = (ev) => {
        if(ev.type === 'keydown' && ev.key !== 'Escape') return;
        if((ev.type === 'pointerdown' || ev.type === 'mousedown') && menu.contains(ev.target)) return;
        this.closeCreateMenu();
      };
      setTimeout(() => {
        document.addEventListener('pointerdown', this._createMenuDismiss, true);
        document.addEventListener('keydown', this._createMenuDismiss, true);
        this.canvas.addEventListener('wheel', this._createMenuDismiss, { passive: true });
      }, 0);
    }

    closeCreateMenu() {
      if(this._createMenu) { this._createMenu.remove(); this._createMenu = null; }
      if(this._createMenuDismiss) {
        document.removeEventListener('pointerdown', this._createMenuDismiss, true);
        document.removeEventListener('keydown', this._createMenuDismiss, true);
        this.canvas.removeEventListener('wheel', this._createMenuDismiss);
        this._createMenuDismiss = null;
      }
    }

    // ---- marquee selection ------------------------------------
    toScreenForWorld(wx, wy) {
      const r = this.canvas.getBoundingClientRect();
      return { x: r.left + this.view.x + wx * this.view.scale, y: r.top + this.view.y + wy * this.view.scale };
    }
    clientToWorld(cx, cy) {
      const r = this.canvas.getBoundingClientRect();
      return { x: (cx - r.left - this.view.x) / this.view.scale, y: (cy - r.top - this.view.y) / this.view.scale };
    }

    startMarquee(e) {
      const startW = this.clientToWorld(e.clientX, e.clientY);
      const additive = e.ctrlKey || e.metaKey;
      if (!additive) this.clearSelection();

      // Selection mode depends on WHERE the marquee starts:
      //  - started inside a group  -> select ONLY steps (work within the group);
      //  - started outside groups  -> select groups as wholes (+ ungrouped steps
      //    & comments), never the individual member-steps of a group.
      const startGroupId = this.groupAtPoint(startW.x, startW.y);
      const stepsOnly = !!startGroupId;

      const box = document.createElement('div');
      box.className = 'bp-marquee';
      this.world.appendChild(box);
      let moved = false;

      // Iterate candidates for a given rect, marking/collecting via callbacks.
      const forEachHit = (rect, onNode, onGroup, onComment) => {
        this.nodeEls.forEach((node, id) => {
          const p = this.bp.positions[id];
          const inRect = p && this.rectsIntersect(rect, { x: p.x, y: p.y, w: node.offsetWidth, h: node.offsetHeight });
          // In "groups" mode skip steps that belong to a group (the group represents them).
          const eligible = inRect && (stepsOnly || !this.stepGroup.get(id));
          onNode(node, id, !!eligible);
        });
        this.groupEls.forEach((frame, gid) => {
          const inRect = !stepsOnly && this.rectsIntersect(rect, this.groupRect(gid));
          onGroup(frame, gid, inRect);
        });
        (this.bp.comments || []).forEach(c => {
          const el = this.world.querySelector(`.bp-comment[data-comment-id="${c.id}"]`);
          const inRect = !stepsOnly && this.rectsIntersect(rect, { x: c.x, y: c.y, w: c.w || 220, h: c.h || 120 });
          if (el) onComment(el, c, inRect);
        });
      };

      const hitTest = (rect) => {
        forEachHit(rect,
          (node, id, hit) => node.classList.toggle('is-marquee-hit', hit),
          (frame, gid, hit) => frame.classList.toggle('is-marquee-hit', hit),
          (el, c, hit) => el.classList.toggle('is-marquee-hit', hit)
        );
      };

      this.startPointerDrag(e, {
        move: ev => {
          const cur = this.clientToWorld(ev.clientX, ev.clientY);
          const x = Math.min(startW.x, cur.x), y = Math.min(startW.y, cur.y);
          const w = Math.abs(cur.x - startW.x), h = Math.abs(cur.y - startW.y);
          if (w > 2 || h > 2) moved = true;
          box.style.left = `${x}px`; box.style.top = `${y}px`;
          box.style.width = `${w}px`; box.style.height = `${h}px`;
          hitTest({ x, y, w, h });
        },
        end: (ev, cancelled) => {
          box.remove();
          this.world.querySelectorAll('.is-marquee-hit').forEach(n => n.classList.remove('is-marquee-hit'));
          if (cancelled || !ev || !moved) return;
          const cur = this.clientToWorld(ev.clientX, ev.clientY);
          const x = Math.min(startW.x, cur.x), y = Math.min(startW.y, cur.y);
          const w = Math.abs(cur.x - startW.x), h = Math.abs(cur.y - startW.y);
          forEachHit({ x, y, w, h },
            (node, id, hit) => { if (hit) this.selected.add(id); },
            (frame, gid, hit) => { if (hit) this.selectedGroups.add(gid); },
            (el, c, hit) => { if (hit) this.selectedComments.add(c.id); }
          );
          this.applySelectionStyles();
        }
      });
      // A single tap without dragging already cleared the selection in pointerdown.
    }

    rectsIntersect(a, b) {
      return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
    }

    hasSelection() {
      return this.selected.size + this.selectedGroups.size + this.selectedComments.size > 0;
    }

    clearSelection() {
      this.selected.clear();
      this.selectedGroups.clear();
      this.selectedComments.clear();
      this.applySelectionStyles();
    }

    toggleSelectMode() {
      this.selectMode = !this.selectMode;
      this.canvas.classList.toggle('is-select-mode', this.selectMode);
      const btn = document.getElementById('bp-select-mode');
      btn?.classList.toggle('is-primary', this.selectMode);
    }

    selectAll() {
      this.selected = new Set(this.nodeEls.keys());
      this.selectedGroups = new Set(this.groupEls.keys());
      this.selectedComments = new Set((this.bp.comments || []).map(c => c.id));
      this.applySelectionStyles();
    }

    applySelectionStyles() {
      this.nodeEls.forEach((node, id) => node.classList.toggle('is-selected', this.selected.has(id)));
      this.groupEls.forEach((frame, gid) => frame.classList.toggle('is-selected', this.selectedGroups.has(gid)));
      (this.bp.comments || []).forEach(c => {
        const el = this.world.querySelector(`.bp-comment[data-comment-id="${c.id}"]`);
        if (el) el.classList.toggle('is-selected', this.selectedComments.has(c.id));
      });
    }

    // Build the full move-set for a drag: explicitly selected nodes/comments +
    // every step that belongs to a selected group (so groups move with members).
    collectMoveSet() {
      const stepIds = new Set(this.selected);
      const commentIds = new Set(this.selectedComments);
      this.selectedGroups.forEach(gid => {
        this.groupMembers(gid).forEach(id => stepIds.add(id));
      });
      return { stepIds, commentIds, groupIds: new Set(this.selectedGroups) };
    }

    // Apply a delta to a whole move-set (nodes, group frames, comments).
    translateSelection(moveSet, startNode, startComment, startGroupRect, dx, dy) {
      moveSet.stepIds.forEach(id => {
        const sp = startNode.get(id);
        if (!sp) return;
        const np = { x: sp.x + dx, y: sp.y + dy };
        this.bp.positions[id] = np;
        const n = this.nodeEls.get(id);
        if (n) { n.style.left = `${np.x}px`; n.style.top = `${np.y}px`; }
      });
      moveSet.commentIds.forEach(id => {
        const sc = startComment.get(id);
        const cmt = (this.bp.comments || []).find(c => c.id === id);
        if (!sc || !cmt) return;
        cmt.x = sc.x + dx; cmt.y = sc.y + dy;
        const el = this.world.querySelector(`.bp-comment[data-comment-id="${id}"]`);
        if (el) { el.style.left = `${Math.round(cmt.x)}px`; el.style.top = `${Math.round(cmt.y)}px`; }
      });
      // Move each selected group's stored rect by the same delta. For groups
      // WITH members this keeps the frame travelling with them (the effective
      // rect is union(stored, members); both shift, so the box moves cleanly
      // instead of stretching). For empty groups it is the only anchor.
      moveSet.groupIds.forEach(gid => {
        const sr = startGroupRect.get(gid);
        if (!sr) return;
        this.bp.groupRects[gid] = { x: sr.x + dx, y: sr.y + dy, w: sr.w, h: sr.h };
      });
      this.updateGroupFrames();
    }

    // ---- mini-map ---------------------------------------------
    bindMinimap() {
      if (!this.minimap) return;
      const navigateTo = (clientX, clientY) => {
        const b = this.worldBounds();
        if (!b) return;
        const rect = this.minimap.getBoundingClientRect();
        const mx = (clientX - rect.left) / rect.width;
        const my = (clientY - rect.top) / rect.height;
        const wx = b.minX + mx * (b.maxX - b.minX);
        const wy = b.minY + my * (b.maxY - b.minY);
        const cr = this.canvas.getBoundingClientRect();
        this.view.x = cr.width / 2 - wx * this.view.scale;
        this.view.y = cr.height / 2 - wy * this.view.scale;
        this.applyTransform();
        this.drawMinimap();
        this.saveView();
      };
      this.minimap.addEventListener('pointerdown', e => {
        e.preventDefault();
        navigateTo(e.clientX, e.clientY);
        this.startPointerDrag(e, { move: ev => navigateTo(ev.clientX, ev.clientY) });
      });
    }
    toggleMinimap() {
      this.minimapOn = !this.minimapOn;
      try { localStorage.setItem('scenario-studio:bp-minimap', this.minimapOn ? '1' : '0'); } catch { /* ignore */ }
      this.updateMinimapVisibility();
      if (this.minimapOn) this.drawMinimap();
    }
    updateMinimapVisibility() {
      if (!this.minimap) return;
      this.minimap.classList.toggle('is-on', this.minimapOn && this.active);
      const btn = document.getElementById('bp-minimap-toggle');
      btn?.classList.toggle('is-primary', this.minimapOn);
    }
    worldBounds() {
      let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
      const ext = (x, y, w, h) => { minX = Math.min(minX, x); minY = Math.min(minY, y); maxX = Math.max(maxX, x + w); maxY = Math.max(maxY, y + h); };
      this.nodeEls.forEach((node, id) => { const p = this.bp.positions[id]; if (p) ext(p.x, p.y, node.offsetWidth, node.offsetHeight); });
      this.groupEls.forEach((_, id) => { const r = this.groupRect(id); ext(r.x, r.y, r.w, r.h); });
      if (!Number.isFinite(minX)) return null;
      const pad = 120;
      return { minX: minX - pad, minY: minY - pad, maxX: maxX + pad, maxY: maxY + pad };
    }
    drawMinimap() {
      if (!this.minimap || !this.minimapOn || !this.active) return;
      const b = this.worldBounds();
      const ctx = this.minimap.getContext('2d');
      const W = this.minimap.width, H = this.minimap.height;
      ctx.clearRect(0, 0, W, H);
      if (!b) return;
      const bw = b.maxX - b.minX, bh = b.maxY - b.minY;
      const sx = W / bw, sy = H / bh;
      const sc = Math.min(sx, sy);
      const ox = (W - bw * sc) / 2, oy = (H - bh * sc) / 2;
      const tx = wx => ox + (wx - b.minX) * sc;
      const ty = wy => oy + (wy - b.minY) * sc;

      // groups
      ctx.fillStyle = 'rgba(120,132,150,0.18)';
      this.groupEls.forEach((_, id) => { const r = this.groupRect(id); ctx.fillRect(tx(r.x), ty(r.y), r.w * sc, r.h * sc); });
      // nodes
      ctx.fillStyle = 'rgba(142,168,255,0.9)';
      this.nodeEls.forEach((node, id) => { const p = this.bp.positions[id]; if (p) ctx.fillRect(tx(p.x), ty(p.y), Math.max(2, node.offsetWidth * sc), Math.max(2, node.offsetHeight * sc)); });
      // viewport rectangle
      const cr = this.canvas.getBoundingClientRect();
      const vx = -this.view.x / this.view.scale, vy = -this.view.y / this.view.scale;
      const vw = cr.width / this.view.scale, vh = cr.height / this.view.scale;
      ctx.strokeStyle = 'rgba(255,255,255,0.85)';
      ctx.lineWidth = 1.5;
      ctx.strokeRect(tx(vx), ty(vy), vw * sc, vh * sc);
    }

    esc(s) {
      return window.EditorCore?.utils?.escapeHtml
        ? window.EditorCore.utils.escapeHtml(s)
        : String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
    }
  }

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.BlueprintView = BlueprintView;
})();
