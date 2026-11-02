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

  let csrfRevision = 0;
  function setCsrf(token) {
    if (typeof token !== 'string' || !CSRF_RE.test(token)) return;
    if (token === csrfToken) return;
    csrfToken = token;
    csrfRevision += 1;
    try { csrfStorage()?.setItem(CSRF_KEY, token); } catch { /* private browsing */ }
  }

  function clearCsrf(expected) {
    // A late 403 must not clear a newer token issued by login/another frame.
    if (expected !== undefined && expected !== csrfToken) return;
    if (!csrfToken) return;
    csrfToken = '';
    csrfRevision += 1;
    try { csrfStorage()?.removeItem(CSRF_KEY); } catch { /* private browsing */ }
  }

  // Login/logout in another iframe uses the same session cookie. Follow its
  // rotated token instead of keeping a stale in-memory copy until a write fails.
  window.addEventListener?.('storage', (event) => {
    if (event.key !== CSRF_KEY && event.key !== null) return;
    if (event.storageArea && event.storageArea !== csrfStorage()) return;
    const next = event.newValue || '';
    if (CSRF_RE.test(next)) setCsrf(next);
    else clearCsrf();
  });

  const isStudioApi = (url) => {
    const value = String(url || '');
    try {
      if (typeof URL === 'function' && window.location?.href) {
        const resolved = new URL(value, window.location.href);
        if (resolved.origin !== window.location.origin) return false;
        return /(?:^|\/)api\/[^/]+\.php$/.test(resolved.pathname)
          || /(?:^|\/)api\.php$/.test(resolved.pathname);
      }
    } catch { return false; }
    // VM/older-webview fallback: relative URLs only, never credentials in a
    // query string mistaken for an API path or an absolute foreign origin.
    if (/^(?:[a-z][a-z\d+.-]*:|\/\/)/i.test(value)) return false;
    const path = value.split(/[?#]/, 1)[0];
    return /(?:^|\/)api\/[^/]+\.php$/.test(path) || /(?:^|\/)api\.php$/.test(path);
  };
  const csrfUrlFor = (url) => {
    if (!isStudioApi(url)) return '';
    const path = String(url).split(/[?#]/, 1)[0];
    const base = path.slice(0, path.lastIndexOf('/') + 1);
    return `${base}${/(?:^|\/)api\.php$/.test(path) ? 'api/' : ''}auth.php?action=csrf`;
  };

  let csrfRequest = null;
  async function fetchCsrf(hintUrl, force = false, options = {}) {
    if (csrfToken && !force) return csrfToken;
    const target = csrfUrlFor(hintUrl || 'api/db.php');
    if (!target) return '';
    // Concurrent stale POSTs share one refresh, including a force refresh.
    if (csrfRequest) return csrfRequest;
    const revision = csrfRevision;
    csrfRequest = text(target, options)
      .then((response) => {
        const { data } = parseBody(response.data);
        // An auth response received while refresh was in flight is newer.
        if (response.ok && revision === csrfRevision && data && typeof data === 'object') setCsrf(data.csrf);
        return csrfToken;
      })
      .catch(() => '')
      .finally(() => { csrfRequest = null; });
    return csrfRequest;
  }

  // A caller may cancel its own wait without cancelling a token refresh shared
  // by other frames/requests. The shared refresh still has its own HTTP timeout.
  const waitForToken = (promise, signal) => {
    if (!signal) return promise;
    let abort;
    const cancelled = new Promise((resolve, reject) => {
      abort = () => reject(new DOMException('Request cancelled', 'AbortError'));
      if (signal.aborted) abort();
      else signal.addEventListener('abort', abort, { once: true });
    });
    return Promise.race([promise, cancelled]).finally(() => signal.removeEventListener('abort', abort));
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

  /** One timeout covers CSRF preparation, headers and reading the body. */
  const text = async (url, options = {}) => {
    const {
      method = 'GET', body = null, query = null, headers = null,
      timeout: requestedTimeout = DEFAULT_TIMEOUT, signal: externalSignal = null,
      credentials = 'same-origin', cache = 'no-store',
    } = options;
    const timeout = Number.isFinite(Number(requestedTimeout)) && Number(requestedTimeout) >= 0
      ? Number(requestedTimeout) : DEFAULT_TIMEOUT;
    const controller = new AbortController();
    const { signal, cleanup } = combineSignals(controller.signal, externalSignal);
    let timedOut = false;
    const startedAt = Date.now();
    // Nested CSRF preparation has its own timer. Check the absolute deadline
    // too: under load that timer may finish just before the parent timer runs.
    const expired = () => {
      if (timeout > 0 && Date.now() - startedAt >= timeout) {
        timedOut = true;
        controller.abort();
      }
      return timedOut;
    };
    const timer = timeout > 0 ? setTimeout(expired, timeout) : null;
    const cancelled = () => ({ status: 0, ok: false, data: '', aborted: true,
      cancelled: !timedOut, retryAfter: null,
      error: timedOut ? TIMEOUT_ERROR : CANCELED_ERROR });
    try {
      if (signal.aborted) return cancelled();
      const payload = body == null || typeof body === 'string' ? body : JSON.stringify(body);
      const requestHeaders = typeof Headers === 'function' && headers instanceof Headers
        ? Object.fromEntries(headers.entries()) : { ...(headers || {}) };
      const hasContentType = Object.keys(requestHeaders).some(key => key.toLowerCase() === 'content-type');
      if (payload != null && !hasContentType) requestHeaders['Content-Type'] = 'application/json';
      const writeMethod = !['GET', 'HEAD'].includes(String(method).toUpperCase());
      const apiWrite = writeMethod && isStudioApi(url);
      if (apiWrite && !csrfToken) {
        await waitForToken(fetchCsrf(url, false, {
          timeout: timeout > 0 ? Math.max(1, timeout - (Date.now() - startedAt)) : 0,
        }), signal);
      }
      if (signal.aborted || expired()) return cancelled();
      const csrfAttempt = apiWrite ? csrfToken : '';
      if (csrfAttempt) requestHeaders['X-Editor-CSRF'] = csrfAttempt;
      const response = await fetch(buildUrl(url, query), {
        method, credentials, cache, headers: requestHeaders,
        body: payload ?? undefined, signal,
      });
      if (signal.aborted || expired()) return cancelled();
      if (response.status === 401 && isStudioApi(url)) notifyAuthRequired();
      let retryAfter = null;
      try {
        const value = response.headers?.get?.('retry-after');
        const seconds = Number(value);
        const date = Date.parse(value);
        const delay = Number.isFinite(seconds) ? seconds : (date - Date.now()) / 1000;
        if (Number.isFinite(delay) && delay > 0) retryAfter = Math.ceil(delay);
      } catch { /* Headers are unavailable in some test environments. */ }
      const data = await response.text();
      return { status: response.status, ok: response.ok,
        error: response.ok ? '' : `HTTP ${response.status}`, retryAfter, data, csrfAttempt };
    } catch (error) {
      if (signal.aborted || error?.name === 'AbortError' || expired()) return cancelled();
      return { status: 0, ok: false, data: '', aborted: false, cancelled: false,
        retryAfter: null, error: NETWORK_ERROR };
    } finally {
      if (timer) clearTimeout(timer);
      cleanup();
    }
  };

  const json = async (url, options = {}) => {
    const result = await text(url, options);
    const { csrfAttempt, ...response } = result;
    const { data, invalid } = parseBody(response.data);
    if (isStudioApi(url) && data && typeof data === 'object' && typeof data.csrf === 'string') setCsrf(data.csrf);
    const writeMethod = !['GET', 'HEAD'].includes(String(options.method || 'GET').toUpperCase());
    if (writeMethod && isStudioApi(url) && response.status === 403 && data?.code === 'csrf_token' && !options.__csrfRetried) {
      clearCsrf(csrfAttempt);
      const token = await waitForToken(fetchCsrf(url, true, { timeout: options.timeout }), options.signal)
        .catch(() => '');
      if (options.signal?.aborted) {
        return { ...response, status: 0, ok: false, data: null, aborted: true,
          cancelled: true, retryAfter: null, error: CANCELED_ERROR };
      }
      // Never replay a token already tried.
      if (token && token !== csrfAttempt) {
        return json(url, { ...options, __csrfRetried: true });
      }
    }
    const failedByPayload = data !== null && typeof data === 'object'
      && (data.ok === false || data.success === false);
    const ok = response.ok && !failedByPayload && !invalid;
    let error = response.error;
    if (!ok && response.status !== 0) {
      error = (data && typeof data === 'object' && data.error) ? String(data.error)
        : invalid ? BAD_RESPONSE_ERROR : `HTTP ${response.status}`;
    }
    const retryAfter = Number(data?.retry_after) > 0 ? Number(data.retry_after) : response.retryAfter;
    return { ...response, ...(data && typeof data === 'object' ? data : {}),
      status: response.status, ok, error, retryAfter: retryAfter ?? null, data };
  };

  /** Как json(), но бросает Error с HTTP-статусом и исходным payload. */
  const request = async (url, options = {}) => {
    const response = await json(url, options);
    if (!response.ok) {
      const error = new Error(response.error || 'The operation failed');
      error.status = response.status;
      error.payload = response.data;
      error.code = response.code;
      error.retryAfter = response.retryAfter;
      error.cancelled = !!response.cancelled;
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
      refresh: (hintUrl) => fetchCsrf(hintUrl || 'api/db.php', true),
    },
    postMessage,
    isSameOriginMessage,
    DEFAULT_TIMEOUT,
    NETWORK_ERROR,
    TIMEOUT_ERROR,
    notifyAuthRequired,
  };
})();
