/**
 * editor-core/http.js — единственный сетевой слой клиента.
 *
 * Раньше каждая часть редактора (utils, db, db-crud, страницы) делала свой
 * fetch со своими правилами: где-то не было таймаута, где-то ошибка терялась,
 * где-то 401 не доходил до оболочки. Здесь это одна функция с предсказуемым
 * результатом:
 *
 *   const res = await EditorCore.Http.json(url, { method: 'POST', body });
 *   res.ok      — сервер ответил и не сообщил об ошибке;
 *   res.status  — HTTP-код (0 — сеть/таймаут);
 *   res.error   — понятный текст ошибки (пусто при успехе);
 *   res.data    — разобранный JSON (или null);
 *   ...res      — поля ответа лежат и на верхнем уровне (res.rows и т. п.).
 *
 * `Http.request()` — тот же вызов, но бросает Error при ошибке: удобно там,
 * где исключение ловит внешний код (CRUD-редакторы).
 */
(() => {
  if (window.EditorCore?.Http) return;
  const namespace = (window.EditorCore = window.EditorCore || {});

  const DEFAULT_TIMEOUT = 15000;
  const NETWORK_ERROR = 'Нет связи с сервером';
  const TIMEOUT_ERROR = 'Превышено время ожидания ответа сервера';
  // Ответ не JSON: чаще всего веб-сервер не выполняет PHP и отдаёт файл как текст
  // либо страница ошибки. Подсказка экономит время новичку.
  const BAD_RESPONSE_ERROR = 'Сервер вернул неожиданный ответ — проверьте, что на сервере выполняется PHP (api/*.php)';
  const CANCELED_ERROR = 'Запрос отменён';

  /** Оболочка студии должна узнать, что сессия кончилась. */
  const notifyAuthRequired = () => {
    try { window.top?.postMessage({ type: 'editor-shell:auth-required' }, '*'); } catch { /* не в iframe */ }
  };

  /** Свой таймаут + внешний сигнал отмены (поиск с вводом, закрытие формы). */
  const combineSignals = (own, external) => {
    if (!external) return own;
    if (typeof AbortSignal !== 'undefined' && typeof AbortSignal.any === 'function') {
      return AbortSignal.any([own, external]);
    }
    const fallback = new AbortController();
    const abort = () => fallback.abort();
    if (external.aborted || own.aborted) abort();
    external.addEventListener('abort', abort, { once: true });
    own.addEventListener('abort', abort, { once: true });
    return fallback.signal;
  };

  const buildUrl = (url, query) => {
    if (!query) return url;
    const params = new URLSearchParams();
    Object.entries(query).forEach(([key, value]) => {
      if (value !== undefined && value !== null) params.append(key, String(value));
    });
    const qs = params.toString();
    if (!qs) return url;
    return `${url}${url.includes('?') ? '&' : '?'}${qs}`;
  };

  const parseBody = (text) => {
    if (!text) return { data: null, invalid: false };
    try {
      return { data: JSON.parse(text), invalid: false };
    } catch {
      // Сервер мог добавить предупреждение PHP перед JSON — берём объект/массив.
      const startObj = text.indexOf('{');
      const startArr = text.indexOf('[');
      const start = startObj < 0 ? startArr : (startArr < 0 ? startObj : Math.min(startObj, startArr));
      if (start >= 0) {
        const end = Math.max(text.lastIndexOf('}'), text.lastIndexOf(']'));
        if (end > start) {
          try { return { data: JSON.parse(text.slice(start, end + 1)), invalid: false }; } catch { /* ниже */ }
        }
      }
      return { data: null, invalid: true };
    }
  };

  /**
   * Один запрос. Никогда не бросает: сетевые сбои и таймауты возвращаются
   * как `{ ok: false, status: 0, error }`.
   */
  const json = async (url, options = {}) => {
    const {
      method = 'GET',
      body = null,
      query = null,
      headers = null,
      timeout = DEFAULT_TIMEOUT,
      signal: externalSignal = null,
      credentials = 'same-origin',
      cache = 'no-store',
    } = options;

    const payload = body == null || typeof body === 'string' ? body : JSON.stringify(body);
    const controller = new AbortController();
    const signal = combineSignals(controller.signal, externalSignal);
    let timedOut = false;
    const timer = timeout > 0 ? setTimeout(() => { timedOut = true; controller.abort(); }, timeout) : null;

    try {
      const res = await fetch(buildUrl(url, query), {
        method,
        credentials,
        cache,
        headers: payload == null ? (headers || undefined) : { 'Content-Type': 'application/json', ...(headers || {}) },
        body: payload ?? undefined,
        signal,
      });

      if (res.status === 401) notifyAuthRequired();

      const { data, invalid } = parseBody(await res.text());
      const failedByPayload = data !== null && typeof data === 'object' && data.ok === false;
      // Непустое тело, которое не разобрать как JSON, — не успех, даже при HTTP 200:
      // иначе вызывающий код получит data = null и упадёт на res.rows / res.ok.
      const ok = res.ok && !failedByPayload && !invalid;
      let error = '';
      if (!ok) {
        error = (data && typeof data === 'object' && data.error) ? String(data.error)
          : invalid ? BAD_RESPONSE_ERROR
            : `HTTP ${res.status}`;
      }

      return {
        ...(data && typeof data === 'object' ? data : {}),
        status: res.status,
        ok,
        error,
        data,
      };
    } catch (err) {
      const aborted = err?.name === 'AbortError';
      // Отмена вызывающим кодом — не ошибка: так прерывают устаревший поиск.
      return {
        status: 0,
        ok: false,
        data: null,
        aborted,
        cancelled: aborted && !timedOut,
        error: aborted ? (timedOut ? TIMEOUT_ERROR : CANCELED_ERROR) : NETWORK_ERROR,
      };
    } finally {
      if (timer) clearTimeout(timer);
    }
  };

  /** Как `json()`, но бросает Error — для кода, который ждёт исключения. */
  const request = async (url, options = {}) => {
    const res = await json(url, options);
    if (!res.ok) {
      const error = new Error(res.error || 'Операция не выполнена');
      error.status = res.status;
      error.payload = res.data;
      throw error;
    }
    return res.data;
  };

  /** Текст ответа (сценарии, файлы). Та же нормализация, что и у `json()`. */
  const text = async (url, options = {}) => {
    const {
      method = 'GET',
      body = null,
      query = null,
      headers = null,
      timeout = DEFAULT_TIMEOUT,
      signal: externalSignal = null,
      credentials = 'same-origin',
      cache = 'no-store',
    } = options;
    const payload = body == null || typeof body === 'string' ? body : JSON.stringify(body);
    const controller = new AbortController();
    const signal = combineSignals(controller.signal, externalSignal);
    let timedOut = false;
    const timer = timeout > 0 ? setTimeout(() => { timedOut = true; controller.abort(); }, timeout) : null;
    try {
      const res = await fetch(buildUrl(url, query), {
        method,
        credentials,
        cache,
        headers: payload == null ? (headers || undefined) : { 'Content-Type': 'application/json', ...(headers || {}) },
        body: payload ?? undefined,
        signal,
      });
      if (res.status === 401) notifyAuthRequired();
      const data = await res.text();
      return {
        status: res.status,
        ok: res.ok,
        error: res.ok ? '' : `HTTP ${res.status}`,
        data,
      };
    } catch (err) {
      const aborted = err?.name === 'AbortError';
      return {
        status: 0,
        ok: false,
        data: '',
        aborted,
        cancelled: aborted && !timedOut,
        error: aborted ? (timedOut ? TIMEOUT_ERROR : CANCELED_ERROR) : NETWORK_ERROR,
      };
    } finally {
      if (timer) clearTimeout(timer);
    }
  };

  namespace.Http = {
    json,
    text,
    request,
    DEFAULT_TIMEOUT,
    NETWORK_ERROR,
    TIMEOUT_ERROR,
    notifyAuthRequired,
  };
})();
