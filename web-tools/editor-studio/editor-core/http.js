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
  const NETWORK_ERROR = 'No connection to the server';
  const TIMEOUT_ERROR = 'The server took too long to respond';
  const BAD_RESPONSE_ERROR = 'The server returned an unexpected response — check that PHP is running (api/*.php)';
  const CANCELED_ERROR = 'Request cancelled';

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
      // A source editor with an open drill-down must never be evicted as clean.
      const payload = data?.type === 'editor-shell:dirty-status' && namespace.RelatedRecords?.hasOpenEditors?.()
        ? { ...data, dirty: true } : data;
      target.postMessage(payload, origin);
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

  // ── CSRF ────────────────────────────────────────────────────────────────
  // Сервер требует заголовок X-Editor-CSRF в каждом POST (токен сессии).
  // Токен приходит в ответах auth (status/login/logout/csrf) и кэшируется в
  // sessionStorage: оболочка и редакторы в iframe одной вкладки делят его.
  const CSRF_KEY = 'editorStudio.csrf';
  const CSRF_RE = /^[a-f0-9]{64}$/;
  const csrfStorage = () => {
    try { return typeof sessionStorage !== 'undefined' ? sessionStorage : null; } catch { return null; }
  };
  let csrfToken = '';
  try { csrfToken = csrfStorage()?.getItem(CSRF_KEY) || ''; } catch { /* приватный режим */ }
  if (!CSRF_RE.test(csrfToken)) csrfToken = '';

  function setCsrf(token) {
    if (typeof token !== 'string' || !CSRF_RE.test(token)) return;
    csrfToken = token;
    try { csrfStorage()?.setItem(CSRF_KEY, token); } catch { /* приватный режим */ }
  }

  function clearCsrf() {
    csrfToken = '';
    try { csrfStorage()?.removeItem(CSRF_KEY); } catch { /* приватный режим */ }
  }

  /** Адрес выдачи токена по адресу вызываемого API ('' — не наш API). */
  const csrfUrlFor = (url) => {
    const value = String(url || '');
    const idx = value.indexOf('api/');
    if (idx >= 0) return `${value.slice(0, idx)}api/auth.php?action=csrf`;
    const fileApi = value.match(/^(.*?)api\.php(?:\?.*)?$/);
    return fileApi ? `${fileApi[1]}api/auth.php?action=csrf` : '';
  };

  let csrfPromise = null;
  async function fetchCsrf(hintUrl, force = false) {
    if (csrfToken && !force) return csrfToken;
    const target = csrfUrlFor(hintUrl || 'api/');
    if (!target) return '';
    if (!csrfPromise) {
      csrfPromise = text(target)
        .then((response) => {
          const { data } = parseBody(response.data);
          if (data && typeof data === 'object') setCsrf(data.csrf);
          return csrfToken;
        })
        .catch(() => '')
        .finally(() => { csrfPromise = null; });
    }
    return csrfPromise;
  }

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

    const writeMethod = !['GET', 'HEAD'].includes(String(method).toUpperCase());
    let requestHeaders = headers;
    if (writeMethod) {
      if (!csrfToken) { try { await fetchCsrf(url); } catch { /* отправим без токена — сервер ответит 403 csrf_token */ } }
      if (csrfToken) requestHeaders = { ...(headers || {}), 'X-Editor-CSRF': csrfToken };
    }

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
        headers: payload == null ? (requestHeaders || undefined) : { 'Content-Type': 'application/json', ...(requestHeaders || {}) },
        body: payload ?? undefined,
        signal,
      });
      if (response.status === 401) notifyAuthRequired();
      let retryAfter = null;
      try {
        const seconds = Number(response.headers?.get?.('retry-after'));
        if (Number.isFinite(seconds) && seconds > 0) retryAfter = seconds;
      } catch { /* заголовки недоступны в тестовом окружении */ }
      const data = await response.text();
      return {
        status: response.status,
        ok: response.ok,
        error: response.ok ? '' : `HTTP ${response.status}`,
        retryAfter,
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
    // Ответы auth несут актуальный токен (вход/выход ротируют его).
    if (data && typeof data === 'object' && typeof data.csrf === 'string') setCsrf(data.csrf);
    // Устаревший токен: запрос отклонён ДО обработки, поэтому повтор безопасен.
    const writeMethod = !['GET', 'HEAD'].includes(String(options.method || 'GET').toUpperCase());
    if (writeMethod && response.status === 403 && data?.code === 'csrf_token' && !options.__csrfRetried) {
      clearCsrf();
      await fetchCsrf(url, true);
      if (csrfToken) return json(url, { ...options, __csrfRetried: true });
    }
    const failedByPayload = data !== null && typeof data === 'object'
      && (data.ok === false || data.success === false);
    const ok = response.ok && !failedByPayload && !invalid;

    let error = response.error;
    if (!ok && response.status !== 0) {
      error = (data && typeof data === 'object' && data.error) ? String(data.error)
        : invalid ? BAD_RESPONSE_ERROR
          : `HTTP ${response.status}`;
    }

    const retryAfter = Number(data?.retry_after) > 0 ? Number(data.retry_after) : response.retryAfter;
    return {
      ...response,
      ...(data && typeof data === 'object' ? data : {}),
      status: response.status,
      ok,
      error,
      retryAfter: retryAfter ?? null,
      data,
    };
  };

  /** Как json(), но бросает Error с HTTP-статусом и исходным payload. */
  const request = async (url, options = {}) => {
    const response = await json(url, options);
    if (!response.ok) {
      const error = new Error(response.error || 'The operation failed');
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
    csrf: {
      get: () => csrfToken,
      set: setCsrf,
      clear: clearCsrf,
      refresh: (hintUrl) => fetchCsrf(hintUrl || 'api/', true),
    },
    postMessage,
    isSameOriginMessage,
    DEFAULT_TIMEOUT,
    NETWORK_ERROR,
    TIMEOUT_ERROR,
    notifyAuthRequired,
  };
})();
