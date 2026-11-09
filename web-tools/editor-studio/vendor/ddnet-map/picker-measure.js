/* Замер расстояния: отдельный инструмент панели карты.
 *
 * Раньше замер жил в picker-workbench.js и делил переменные с черновиком
 * тайлов: обработчики мыши, тача и клавиатуры переплетались, а выключение
 * одного режима включало другой. Здесь замер замкнут на себя и общается с
 * окном карты только через колбэки:
 *
 *   state        — { ready, hasPoint } окна карты (только чтение);
 *   point()      — текущая точка контента в пикселях мира;
 *   setPoint(p)  — «B → позиция»: явное действие, попадает в историю;
 *   worldPoint() — координаты события с учётом привязки к центрам тайлов;
 *   draw()/callbacks — перерисовка кадра и синхронизация переключателя.
 *
 * Клики в режиме замера не двигают позицию записи: picker.js ставит точку
 * только в режиме «точка». Перетаскивание не перехватывается — камера
 * панорамируется тем же жестом, как в остальных режимах.
 * Тексты — из picker-text.js (единый словарь панели).
 */
(() => {
  'use strict';

  window.MapPickerMeasure = {
    create(api) {
      const { canvas, state, text, worldPoint, setPoint, redraw } = api;
      let measuring = false;
      let a = null;
      let b = null;
      let preview = null;
      let waiting = false;
      let press = null;
      let touch = null;

      const result = (value) => api.onResult?.(value);
      const sync = () => api.onStateChange?.(measuring);

      const updateResult = () => {
        const end = b || preview;
        if (!a || !end) { result(measuring ? text.start : ''); return; }
        const d = window.MapPickerOverlays.distance(a, end);
        result(`A (${a.x}, ${a.y}) → B (${end.x}, ${end.y}) · `
          + `${d.length.toFixed(1)} px / ${(d.length / 32).toFixed(2)} ${text.tiles} · ΔX ${d.dx}, ΔY ${d.dy}`);
      };

      const select = (event) => {
        const point = worldPoint(event);
        if (!point) return;
        if (waiting) { b = point; waiting = false; }
        else { a = point; b = null; preview = point; waiting = true; }
        updateResult();
        // Кнопка «B → позиция» активна только когда конец замера уже выбран:
        // иначе она молча ничего не делает.
        api.onEndpoint?.(!!b);
        redraw();
      };

      const reset = () => {
        a = b = preview = null;
        waiting = false;
        api.onEndpoint?.(false);
      };

      const setMeasuring = (value) => {
        press = touch = null;
        measuring = !!value;
        // «Измерить» стартует с пустого замера: первый клик — точка A,
        // второй — B. Начать сразу от позиции записи умеет отдельная кнопка
        // «От выбранной точки», поэтому режимы не подменяют друг друга.
        reset();
        sync();
        updateResult();
        redraw();
      };

      // Тап/клик ставит концы замера; перетаскивание отдаём рендереру —
      // камера панорамируется той же ЛКМ/пальцем, как в режиме точки.
      // Тот же экранный порог, что и у клика по карте: панорамирование и
      // перетаскивание не должны ставить концы замера.
      window.addEventListener('mousedown', (event) => {
        if (!measuring || !state.ready || event.target !== canvas || event.button !== 0) return;
        canvas.focus({ preventScroll: true });
        press = { x: event.clientX, y: event.clientY };
      }, true);

      window.addEventListener('mouseup', (event) => {
        if (!press) return;
        const start = press;
        press = null;
        if (Math.hypot(start.x - event.clientX, start.y - event.clientY) < 6
          && event.target === canvas) select(event);
      }, true);

      window.addEventListener('mousemove', (event) => {
        if (!measuring || !waiting || !state.ready || event.target !== canvas) return;
        preview = worldPoint(event);
        updateResult();
        redraw();
      }, true);

      // Короткий тап ставит конец замера, движение — панорама рендерера.
      window.addEventListener('touchstart', (event) => {
        if (!measuring || event.target !== canvas) return;
        if (event.touches.length !== 1) { touch = null; return; }
        const point = event.touches[0];
        touch = { x: point.clientX, y: point.clientY, moved: false };
      }, { capture: true, passive: true });

      window.addEventListener('touchmove', (event) => {
        if (!touch) return;
        if (event.touches.length !== 1) { touch = null; return; }
        const point = event.touches[0];
        if (Math.hypot(point.clientX - touch.x, point.clientY - touch.y) > 10) touch.moved = true;
      }, { capture: true, passive: true });

      window.addEventListener('touchend', (event) => {
        if (!touch) return;
        const start = touch;
        touch = null;
        // preventDefault глушит совместимые mouse-события (iOS Safari): иначе
        // синтетический mousedown/mouseup поставил бы вторую точку. Панораму
        // это не мешает — рендерер слушает сами touch-события, не default.
        event.preventDefault();
        if (start.moved) return;
        const point = event.changedTouches[0];
        if (point) select(point);
      }, { capture: true, passive: false });

      window.addEventListener('blur', () => { press = touch = null; });
      window.addEventListener('touchcancel', () => { press = touch = null; });

      return {
        get active() { return measuring; },
        setMeasuring,
        reset() { reset(); updateResult(); redraw(); },
        startFromPoint() {
          if (!state.hasPoint) return;
          a = { ...api.point() };
          b = null;
          preview = null;
          waiting = true;
          if (!measuring) setMeasuring(true);
          else { api.onEndpoint?.(false); updateResult(); redraw(); }
        },
        useEndpoint() {
          if (!b) return;
          setPoint(b.x, b.y);
          setMeasuring(false);
        },
        draw(ctx) {
          api.onCanStart?.(!state.hasPoint);
          updateResult();
          window.MapPickerOverlays.drawMeasure(ctx, a, b || (waiting ? preview : null), api.worldToScreen);
        },
      };
    },
  };
})();
