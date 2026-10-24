/**
 * editor-core/http.js — единый сетевой слой клиента.
 *
 * Http.json() возвращает нормализованный результат и не бросает исключения:
 *   { ok, status, error, data, ...поля ответа }
 * Http.request() — вариант с исключением для CRUD-кода.
 * Http.text() использует тот же таймаут, внешний AbortSignal и обработку 401.
 */
(() => {
  if (window.EditorCore?.Http) return;
  const namespace = (window.EditorCore = window.EditorCore || {});

  const DEFAULT_TIMEOUT = 15000;
  const NETWORK_ERROR = 'Нет связи с сервером';
  const TIMEOUT_ERROR = 'Превышено время ожидания ответа сервера';
  const BAD_RESPONSE_ERROR = 'Сервер вернул неожиданный ответ — проверьте, что на сервере выполняется PHP (api/*.php)';
  const CANCELED_ERROR = 'Запрос отменён';

  // Все окна Studio работают на одном origin. Явный targetOrigin не даёт
  // отправить служебные данные стороннему окну после смены его навигации.
  const messageOrigin = () => {
    const origin = String(window.location?.origin || '');
    return origin !== 'null' ? origin : '';
  };

  const postMessage = (target, data) => {
    const origin = messageOrigin();
    if (!target || !origin) return false;
    try {
      target.postMessage(data, origin);
      return true;
    } catch {
      return false;
    }
  };

  const isSameOriginMessage = (event, source = null) => {
    const origin = messageOrigin();
    if (!event || !origin || event.origin !== origin) return false;
    return !source || event.source === source;
  };

  const notifyAuthRequired = () => {
    postMessage(window.top, { type: 'editor-shell:auth-required' });
  };

  /** Свой таймаут + внешний сигнал отмены (поиск с вводом, закрытие формы). */
  const combineSignals = (own, external) => {
    if (!external) return { signal: own, cleanup: () => {} };
    if (typeof AbortSignal !== 'undefined' && typeof AbortSignal.any === 'function') {
      return { signal: AbortSignal.any([own, external]), cleanup: () => {} };
    }
    const fallback = new AbortController();
    const abort = () => fallback.abort();
    if (external.aborted || own.aborted) abort();
    external.addEventListener('abort', abort, { once: true });
    own.addEventListener('abort', abort, { once: true });
    return {
      signal: fallback.signal,
      cleanup: () => {
        external.removeEventListener('abort', abort);
        own.removeEventListener('abort', abort);
      },
    };
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
      // Старые PHP-установки могут добавить warning перед JSON.
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

  /** Общая реализация запроса; текстовый ответ затем разбирается в json(). */
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

    const controller = new AbortController();
    const { signal, cleanup } = combineSignals(controller.signal, externalSignal);
    let timedOut = false;
    const timer = timeout > 0 ? setTimeout(() => { timedOut = true; controller.abort(); }, timeout) : null;

    try {
      const payload = body == null || typeof body === 'string' ? body : JSON.stringify(body);
      const response = await fetch(buildUrl(url, query), {
        method,
        credentials,
        cache,
        headers: payload == null ? (headers || undefined) : { 'Content-Type': 'application/json', ...(headers || {}) },
        body: payload ?? undefined,
        signal,
      });
      if (response.status === 401) notifyAuthRequired();
      const data = await response.text();
      return {
        status: response.status,
        ok: response.ok,
        error: response.ok ? '' : `HTTP ${response.status}`,
        data,
      };
    } catch (error) {
      const aborted = error?.name === 'AbortError';
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
      cleanup();
    }
  };

  const json = async (url, options = {}) => {
    const response = await text(url, options);
    const { data, invalid } = parseBody(response.data);
    const failedByPayload = data !== null && typeof data === 'object'
      && (data.ok === false || data.success === false);
    const ok = response.ok && !failedByPayload && !invalid;

    let error = response.error;
    if (!ok && response.status !== 0) {
      error = (data && typeof data === 'object' && data.error) ? String(data.error)
        : invalid ? BAD_RESPONSE_ERROR
          : `HTTP ${response.status}`;
    }

    return {
      ...response,
      ...(data && typeof data === 'object' ? data : {}),
      status: response.status,
      ok,
      error,
      data,
    };
  };

  /** Как json(), но бросает Error с HTTP-статусом и исходным payload. */
  const request = async (url, options = {}) => {
    const response = await json(url, options);
    if (!response.ok) {
      const error = new Error(response.error || 'Операция не выполнена');
      error.status = response.status;
      error.payload = response.data;
      throw error;
    }
    return response.data;
  };

  namespace.Http = {
    json,
    text,
    request,
    postMessage,
    isSameOriginMessage,
    DEFAULT_TIMEOUT,
    NETWORK_ERROR,
    TIMEOUT_ERROR,
    notifyAuthRequired,
  };
})();
