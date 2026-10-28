/* ============================================================
   Scenario events (execution blocks) — shared metadata.
   
   The server starts every event block (`on_end`, `on_recieve_objectives`, …)
   as its own scenario (screen `StartUniversalScenario`): the blocks are
   independent, so a step of one event can never jump into a step of another.
   
   This module is the single source of truth for how an event is presented
   (colour + icon), so the step list, the navigator, the graph and the
   preview page all paint the same block with the same colour.
   
   It is pure data — no DOM, no side effects.
   ============================================================ */
(() => {
  const COLORS = {
    on_recieve_objectives: { color: '#60a5fa', icon: 'fa-solid fa-inbox' },
    on_complete_objectives: { color: '#34d399', icon: 'fa-solid fa-circle-check' },
    on_end: { color: '#f59e0b', icon: 'fa-solid fa-flag-checkered' },
    on_got: { color: '#f472b6', icon: 'fa-solid fa-box-open' },
    on_lost: { color: '#fb7185', icon: 'fa-solid fa-box-archive' },
    on_equip: { color: '#a78bfa', icon: 'fa-solid fa-shirt' },
    on_unequip: { color: '#94a3b8', icon: 'fa-solid fa-toggle-off' },
    on_use: { color: '#22d3ee', icon: 'fa-solid fa-hand-pointer' }
  };

  // The base block: ungrouped steps and groups without an event.
  const BASE = { color: '#64748b', icon: 'fa-solid fa-layer-group' };

  /**
   * Presentation of one event key ('' / unknown = the base block).
   * `isEvent` tells whether the steps of this scope are a triggered block.
   */
  const style = (eventKey, available = null) => {
    const isEvent = typeof eventKey === 'string'
      && eventKey !== ''
      && eventKey !== 'general'
      && (!available || Object.prototype.hasOwnProperty.call(available, eventKey));
    const meta = (isEvent && COLORS[eventKey]) || BASE;
    return {
      key: isEvent ? eventKey : '',
      isEvent,
      color: meta.color,
      icon: meta.icon
    };
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.ScenarioEvents = { COLORS, BASE, style };
})();
