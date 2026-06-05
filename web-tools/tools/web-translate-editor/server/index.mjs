import http from 'node:http';
import { readFile, writeFile, mkdir, stat, rename } from 'node:fs/promises';
import { gzipSync } from 'node:zlib';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const PROJECT_ROOT = path.resolve(__dirname, '..');
const DIST_ROOT = path.resolve(PROJECT_ROOT, 'dist');

// Use PORT first because it is the common env variable on servers/PaaS.
// API_PORT is kept for compatibility with older commands.
const PORT = Number(process.env.PORT || process.env.API_PORT || 3001);

// For remote server access bind to 0.0.0.0, not only localhost.
const HOST = process.env.HOST || process.env.API_HOST || '0.0.0.0';

const TRANSLATION_ROOT = path.resolve(
  process.env.TRANSLATION_ROOT || '/root/mmorpg/server_lang'
);

const TOP_CONTRIBUTORS_FILE = path.resolve(
  process.env.TOP_CONTRIBUTORS_FILE || path.join(TRANSLATION_ROOT, 'top_contributors.json')
);

function isLeaderboardSystemAuthor(author) {
  return String(author || '').trim().toLowerCase() === 'administrator';
}

const CHANGE_REQUESTS_FILE = path.resolve(
  process.env.CHANGE_REQUESTS_FILE || path.join(TRANSLATION_ROOT, 'change_requests.json')
);

const EVENT_SETTINGS_FILE = path.resolve(
  process.env.EVENT_SETTINGS_FILE || path.join(TRANSLATION_ROOT, 'event_settings.json')
);

const DEFAULT_EVENT_SETTINGS = {
  topResetAt: '',
  translationRewards: {
    enabled: false,
    endDate: '',
    rewards: '',
    updatedAt: '',
  },
};

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.ico': 'image/x-icon',
  '.txt': 'text/plain; charset=utf-8',
};

function getRemoteUrls(port) {
  const urls = [];
  const interfaces = os.networkInterfaces();

  for (const records of Object.values(interfaces)) {
    for (const record of records || []) {
      if (record.family === 'IPv4' && !record.internal) {
        urls.push(`http://${record.address}:${port}`);
      }
    }
  }

  return urls;
}

function sendJson(res, statusCode, payload) {
  const body = statusCode === 204 ? '' : JSON.stringify(payload);
  const headers = {
    'Content-Type': 'application/json; charset=utf-8',
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET,POST,PATCH,DELETE,OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type',
    'Cache-Control': 'no-store',
  };

  const acceptsGzip = String(res._acceptEncoding || '').includes('gzip');
  if (body && acceptsGzip && Buffer.byteLength(body) > 4096) {
    const compressed = gzipSync(Buffer.from(body, 'utf-8'), { level: 5 });
    res.writeHead(statusCode, {
      ...headers,
      'Content-Encoding': 'gzip',
      'Vary': 'Accept-Encoding',
      'Content-Length': compressed.length,
    });
    res.end(compressed);
    return;
  }

  res.writeHead(statusCode, {
    ...headers,
    'Content-Length': Buffer.byteLength(body),
  });
  res.end(body);
}

function sendText(res, statusCode, body, contentType = 'text/plain; charset=utf-8') {
  res.writeHead(statusCode, {
    'Content-Type': contentType,
    'Content-Length': Buffer.byteLength(body),
  });
  res.end(body);
}

function safeTranslationPath(fileName) {
  const rootPath = path.resolve(TRANSLATION_ROOT);
  const fullPath = path.resolve(rootPath, fileName);

  if (!fullPath.startsWith(rootPath + path.sep) && fullPath !== rootPath) {
    throw new Error('Invalid translation file path');
  }

  return fullPath;
}

function safeDistPath(urlPath) {
  const rootPath = path.resolve(DIST_ROOT);
  const decodedPath = decodeURIComponent(urlPath.split('?')[0]);
  const normalizedPath = decodedPath === '/' ? '/index.html' : decodedPath;
  const fullPath = path.resolve(rootPath, `.${normalizedPath}`);

  if (!fullPath.startsWith(rootPath + path.sep) && fullPath !== rootPath) {
    throw new Error('Invalid static file path');
  }

  return fullPath;
}

function isSafeLanguageFileName(file) {
  return /^[a-zA-Z0-9_-]+$/.test(file);
}

function isEnglishLanguage(lang) {
  const file = String(lang?.file || '').trim().toLowerCase();
  const name = String(lang?.name || '').trim().toLowerCase();
  return file === 'en' || file === 'eng' || name === 'english';
}

function normalizeLocalizationLanguages(languages) {
  return languages.filter(lang => lang?.file && !isEnglishLanguage(lang));
}


async function atomicWriteJson(filePath, payload) {
  await mkdir(path.dirname(filePath), { recursive: true });
  const tempPath = `${filePath}.${process.pid}.${Date.now()}.tmp`;
  await writeFile(tempPath, JSON.stringify(payload, null, 2), 'utf-8');
  await rename(tempPath, filePath);
}

async function backupCorruptedJson(filePath, raw) {
  try {
    const backupPath = `${filePath}.corrupt.${Date.now()}`;
    await writeFile(backupPath, raw, 'utf-8');
  } catch (_error) {
    // Best-effort backup only.
  }
}

async function readJsonBody(req) {
  const chunks = [];
  for await (const chunk of req) {
    chunks.push(chunk);
  }
  const raw = Buffer.concat(chunks).toString('utf-8');
  return raw ? JSON.parse(raw) : {};
}

async function handleHealth(_req, res) {
  sendJson(res, 200, {
    ok: true,
    translationRoot: TRANSLATION_ROOT,
    port: PORT,
    host: HOST,
  });
}


async function readTranslationManifest() {
  const indexPath = safeTranslationPath('index.json');
  const indexRaw = await readFile(indexPath, 'utf-8');
  const indexJson = JSON.parse(indexRaw);
  const languages = normalizeLocalizationLanguages(Array.isArray(indexJson['language indices'])
    ? indexJson['language indices']
    : []);

  const indexStat = await stat(indexPath);
  const versions = { __index: indexStat.mtimeMs };
  const errors = [];

  for (const lang of languages) {
    if (!lang?.file || !isSafeLanguageFileName(lang.file)) {
      errors.push(`Skipped invalid language file name: ${String(lang?.file || '')}`);
      continue;
    }

    const fileName = `${lang.file}.txt`;
    try {
      const fullPath = safeTranslationPath(fileName);
      const fileStat = await stat(fullPath);
      versions[lang.file] = fileStat.mtimeMs;
    } catch (_error) {
      versions[lang.file] = 0;
      errors.push(`File not found or unreadable: ${fileName}`);
    }
  }

  return { languages, versions, errors };
}

async function handleGetTranslationManifest(_req, res) {
  const manifest = await readTranslationManifest();
  sendJson(res, 200, manifest);
}

async function readTranslationsPayload() {
  const { languages, versions, errors } = await readTranslationManifest();
  const files = {};

  // Read all localization files concurrently. The client still receives one
  // full payload with every file, but large projects no longer wait for slow
  // sequential disk reads language-by-language.
  const readJobs = languages
    .filter(lang => lang?.file && isSafeLanguageFileName(lang.file))
    .map(async (lang) => {
      const fileName = `${lang.file}.txt`;
      try {
        const content = await readFile(safeTranslationPath(fileName), 'utf-8');
        return { file: lang.file, content, error: null };
      } catch (_error) {
        return { file: lang.file, content: '', error: `File not found or unreadable: ${fileName}` };
      }
    });

  const results = await Promise.all(readJobs);
  for (const result of results) {
    files[result.file] = result.content;
    if (result.error && !errors.includes(result.error)) errors.push(result.error);
  }

  return { languages, files, versions, errors, loadedAt: new Date().toISOString() };
}

async function handleGetTranslations(_req, res) {
  sendJson(res, 200, await readTranslationsPayload());
}

async function getTopContributorsForRead(changeRequests, eventSettings) {
  const cachedTop = await readTopContributors().catch(() => []);
  if (cachedTop.length > 0) return cachedTop;
  return await recomputeAndSaveTopContributors(changeRequests, eventSettings);
}

async function handleGetBootstrap(_req, res) {
  const [translationsResult, requestsResult, eventResult] = await Promise.allSettled([
    readTranslationsPayload(),
    readChangeRequests(),
    readEventSettings(),
  ]);

  const translationsPayload = translationsResult.status === 'fulfilled'
    ? translationsResult.value
    : { languages: [], files: {}, versions: {}, errors: [translationsResult.reason?.message || 'Failed to load translation files'], loadedAt: new Date().toISOString() };

  const eventSettings = eventResult.status === 'fulfilled' ? eventResult.value : DEFAULT_EVENT_SETTINGS;
  let changeRequests = [];
  let topContributors = [];

  if (requestsResult.status === 'fulfilled') {
    changeRequests = requestsResult.value;
    try {
      // Bootstrap is called by every page refresh. Use the cached global top for
      // fast reads and recompute only if the cache is missing/corrupted.
      topContributors = await getTopContributorsForRead(changeRequests, eventSettings);
    } catch (error) {
      translationsPayload.errors.push(error?.message || 'Failed to load global top list');
    }
  } else {
    translationsPayload.errors.push(requestsResult.reason?.message || 'Failed to load global requests list');
  }

  sendJson(res, 200, { ...translationsPayload, changeRequests, topContributors, eventSettings });
}



function normalizeEventSettings(value) {
  const root = value && typeof value === 'object' ? value : {};
  const input = root.eventSettings && typeof root.eventSettings === 'object' ? root.eventSettings : root;
  const rewards = input.translationRewards && typeof input.translationRewards === 'object' ? input.translationRewards : {};
  return {
    topResetAt: input.topResetAt ? String(input.topResetAt) : '',
    translationRewards: {
      enabled: Boolean(rewards.enabled),
      endDate: rewards.endDate ? String(rewards.endDate) : '',
      rewards: rewards.rewards ? String(rewards.rewards) : '',
      updatedAt: rewards.updatedAt ? String(rewards.updatedAt) : '',
    },
  };
}

async function readEventSettings() {
  try {
    const raw = await readFile(EVENT_SETTINGS_FILE, 'utf-8');
    return normalizeEventSettings(JSON.parse(raw));
  } catch (error) {
    if (error?.code === 'ENOENT') return DEFAULT_EVENT_SETTINGS;
    if (error instanceof SyntaxError) {
      const raw = await readFile(EVENT_SETTINGS_FILE, 'utf-8').catch(() => '');
      await backupCorruptedJson(EVENT_SETTINGS_FILE, raw);
      return DEFAULT_EVENT_SETTINGS;
    }
    throw error;
  }
}

async function writeEventSettings(eventSettings) {
  const normalized = normalizeEventSettings(eventSettings);
  await atomicWriteJson(EVENT_SETTINGS_FILE, { eventSettings: normalized });
  return normalized;
}

async function handleGetEventSettings(_req, res) {
  sendJson(res, 200, { eventSettings: await readEventSettings() });
}

async function handleUpdateEventSettings(req, res) {
  const body = await readJsonBody(req);
  const result = await withChangeRequestLock(async () => {
    const current = await readEventSettings();
    const incoming = body.eventSettings && typeof body.eventSettings === 'object' ? body.eventSettings : body;
    const eventSettings = await writeEventSettings({
      ...current,
      ...incoming,
      translationRewards: {
        ...current.translationRewards,
        ...(incoming.translationRewards || {}),
      },
    });
    const changeRequests = await readChangeRequests();
    const topContributors = await recomputeAndSaveTopContributors(changeRequests, eventSettings);
    return { eventSettings, topContributors };
  });
  sendJson(res, 200, result);
}

async function handleResetTopContributors(_req, res) {
  const result = await withChangeRequestLock(async () => {
    const current = await readEventSettings();
    const eventSettings = await writeEventSettings({ ...current, topResetAt: new Date().toISOString() });
    const changeRequests = await readChangeRequests();
    const topContributors = await recomputeAndSaveTopContributors(changeRequests, eventSettings);
    return { ok: true, eventSettings, topContributors };
  });
  sendJson(res, 200, result);
}

async function readChangeRequests() {
  try {
    const raw = await readFile(CHANGE_REQUESTS_FILE, 'utf-8');
    const parsed = JSON.parse(raw);
    if (Array.isArray(parsed)) return normalizeChangeRequests(parsed);
    if (Array.isArray(parsed?.changeRequests)) return normalizeChangeRequests(parsed.changeRequests);
    return [];
  } catch (error) {
    if (error?.code === 'ENOENT') {
      return [];
    }
    if (error instanceof SyntaxError) {
      const raw = await readFile(CHANGE_REQUESTS_FILE, 'utf-8').catch(() => '');
      await backupCorruptedJson(CHANGE_REQUESTS_FILE, raw);
      throw Object.assign(new Error('Global change_requests.json is corrupted. A backup was created and the file was not overwritten.'), { statusCode: 500 });
    }
    throw error;
  }
}

function normalizeChangeRequests(value) {
  if (!Array.isArray(value)) return [];
  return value
    .filter(item => item && typeof item.id === 'string')
    .map(item => ({
      id: String(item.id),
      name: String(item.name || ''),
      author: String(item.author || ''),
      languageFile: String(item.languageFile || ''),
      languageName: String(item.languageName || item.languageFile || ''),
      entries: Array.isArray(item.entries)
        ? item.entries.map(entry => ({
          entryId: String(entry?.entryId || ''),
          original: String(entry?.original || ''),
          oldTranslation: String(entry?.oldTranslation || ''),
          newTranslation: String(entry?.newTranslation || ''),
        })).filter(entry => entry.original)
        : [],
      status: ['pending', 'approved', 'rejected'].includes(item.status) ? item.status : 'pending',
      createdAt: item.createdAt ? String(item.createdAt) : new Date().toISOString(),
      resolvedAt: item.resolvedAt ? String(item.resolvedAt) : undefined,
    }))
    .filter(item => item.name && item.author && item.languageFile && item.entries.length > 0)
    .sort((a, b) => new Date(b.createdAt).getTime() - new Date(a.createdAt).getTime());
}

async function writeChangeRequests(changeRequests) {
  const normalized = normalizeChangeRequests(changeRequests);
  await atomicWriteJson(CHANGE_REQUESTS_FILE, { changeRequests: normalized });
  return normalized;
}

async function handleGetChangeRequests(_req, res) {
  const [changeRequests, eventSettings] = await Promise.all([
    readChangeRequests(),
    readEventSettings(),
  ]);
  const topContributors = await getTopContributorsForRead(changeRequests, eventSettings);
  sendJson(res, 200, { changeRequests, topContributors, eventSettings });
}

async function handleCreateChangeRequest(req, res) {
  const body = await readJsonBody(req);
  const incoming = normalizeChangeRequests([{ ...body, status: body.status || 'pending', createdAt: body.createdAt || new Date().toISOString() }]);
  if (incoming.length !== 1) {
    sendJson(res, 400, { error: 'Invalid change request' });
    return;
  }

  const result = await withChangeRequestLock(async () => {
    const current = await readChangeRequests();
    if (current.some(request => request.id === incoming[0].id)) {
      return { statusCode: 409, payload: { error: 'Change request already exists' } };
    }

    const changeRequests = await writeChangeRequests([incoming[0], ...current]);
    const eventSettings = await readEventSettings();
    const topContributors = await recomputeAndSaveTopContributors(changeRequests, eventSettings);
    return { statusCode: 200, payload: { ok: true, request: incoming[0], changeRequests, topContributors, eventSettings } };
  });

  sendJson(res, result.statusCode, result.payload);
}


function getTranslatedTextAmount(entries) {
  return entries.reduce((total, entry) => {
    const text = String(entry?.newTranslation ?? '').trim();
    return total + text.length;
  }, 0);
}

function buildTopContributorsFromRequests(changeRequests, eventSettings = DEFAULT_EVENT_SETTINGS) {
  const authorStats = new Map();
  const resetTime = eventSettings?.topResetAt ? new Date(eventSettings.topResetAt).getTime() : 0;
  for (const request of normalizeChangeRequests(changeRequests)) {
    if (request.status !== 'approved') continue;
    if (isLeaderboardSystemAuthor(request.author)) continue;
    const acceptedTime = new Date(request.resolvedAt || request.createdAt).getTime();
    if (resetTime && (!acceptedTime || acceptedTime < resetTime)) continue;
    const translatedTextAmount = getTranslatedTextAmount(request.entries);
    if (translatedTextAmount <= 0) continue;
    authorStats.set(request.author, (authorStats.get(request.author) || 0) + translatedTextAmount);
  }
  return Array.from(authorStats.entries())
    .map(([author, count]) => ({ author, count }))
    .filter(item => item.author && item.count > 0)
    .sort((a, b) => b.count - a.count || a.author.localeCompare(b.author))
    .slice(0, 100);
}

async function recomputeAndSaveTopContributors(changeRequests, eventSettings = null) {
  const settings = eventSettings || await readEventSettings().catch(() => DEFAULT_EVENT_SETTINGS);
  const topContributors = normalizeTopContributors(buildTopContributorsFromRequests(changeRequests, settings));
  await atomicWriteJson(TOP_CONTRIBUTORS_FILE, { topContributors });
  return topContributors;
}

async function applyChangesToTranslationFile(file, changes) {
  if (isEnglishLanguage({ file, name: file })) {
    throw Object.assign(new Error('English is the source language and cannot be changed as a localization file'), { statusCode: 400 });
  }

  if (!isSafeLanguageFileName(file)) {
    throw Object.assign(new Error('Invalid file name'), { statusCode: 400 });
  }

  const rawChanges = Array.isArray(changes) ? changes : [];
  const changeMap = new Map();
  const entryMap = new Map();

  for (const change of rawChanges) {
    const original = String(change?.original ?? '').trim();
    if (!original) continue;
    const normalizedEntry = {
      entryId: String(change?.entryId || ''),
      original,
      oldTranslation: String(change?.oldTranslation ?? ''),
      newTranslation: String(change?.newTranslation ?? ''),
    };
    changeMap.set(original, normalizedEntry.newTranslation);
    entryMap.set(original, normalizedEntry);
  }

  if (changeMap.size === 0) {
    throw Object.assign(new Error('No valid changed keys were provided'), { statusCode: 400 });
  }

  const filePath = safeTranslationPath(`${file}.txt`);
  const currentContent = await readFile(filePath, 'utf-8');
  const lines = currentContent.split('\n');
  const resultLines = [];
  const foundKeys = new Set();
  const appliedEntries = [];
  let applied = 0;
  let i = 0;

  while (i < lines.length) {
    const line = lines[i].trim();

    if (line === '' || line.startsWith('$')) {
      resultLines.push(lines[i]);
      i++;
      continue;
    }

    let j = i + 1;
    while (j < lines.length && lines[j].trim() === '') {
      j++;
    }

    if (j < lines.length && lines[j].trim().startsWith('==')) {
      const original = line;
      resultLines.push(lines[i]);

      if (changeMap.has(original)) {
        foundKeys.add(original);
        resultLines.push(`== ${changeMap.get(original)}`);
        appliedEntries.push(entryMap.get(original));
        applied++;
      } else {
        resultLines.push(lines[j]);
      }

      i = j + 1;
      continue;
    }

    resultLines.push(lines[i]);
    i++;
  }

  if (applied === 0) {
    const fileStat = await stat(filePath).catch(() => null);
    return {
      ok: true,
      applied: 0,
      skipped: changeMap.size,
      content: currentContent,
      version: fileStat?.mtimeMs || 0,
      appliedKeys: [],
      appliedEntries: [],
    };
  }

  const newContent = resultLines.join('\n');
  await writeFile(filePath, newContent, 'utf-8');
  const fileStat = await stat(filePath);

  return {
    ok: true,
    applied,
    skipped: changeMap.size - foundKeys.size,
    content: newContent,
    version: fileStat.mtimeMs,
    appliedKeys: Array.from(foundKeys),
    appliedEntries,
  };
}

let changeRequestWriteQueue = Promise.resolve();
async function withChangeRequestLock(work) {
  const previous = changeRequestWriteQueue;
  let release;
  changeRequestWriteQueue = new Promise(resolve => { release = resolve; });
  await previous;
  try {
    return await work();
  } finally {
    release();
  }
}

async function handleUpdateChangeRequest(req, res, requestId) {
  const body = await readJsonBody(req);

  const result = await withChangeRequestLock(async () => {
    const current = await readChangeRequests();
    const index = current.findIndex(request => request.id === requestId);
    if (index === -1) {
      return { statusCode: 404, payload: { error: 'Change request not found' } };
    }

    const merged = {
      ...current[index],
      ...body,
      id: current[index].id,
      createdAt: current[index].createdAt,
    };
    const normalized = normalizeChangeRequests([merged]);
    if (normalized.length !== 1) {
      return { statusCode: 400, payload: { error: 'Invalid change request update' } };
    }

    current[index] = normalized[0];
    const changeRequests = await writeChangeRequests(current);
    const eventSettings = await readEventSettings();
    const topContributors = await recomputeAndSaveTopContributors(changeRequests, eventSettings);
    return { statusCode: 200, payload: { ok: true, request: normalized[0], changeRequests, topContributors, eventSettings } };
  });

  sendJson(res, result.statusCode, result.payload);
}

async function handleDeleteChangeRequest(_req, res, requestId) {
  const result = await withChangeRequestLock(async () => {
    const current = await readChangeRequests();
    const next = current.filter(request => request.id !== requestId);
    if (next.length === current.length) {
      return { statusCode: 404, payload: { error: 'Change request not found' } };
    }
    const changeRequests = await writeChangeRequests(next);
    const eventSettings = await readEventSettings();
    const topContributors = await recomputeAndSaveTopContributors(changeRequests, eventSettings);
    return { statusCode: 200, payload: { ok: true, changeRequests, topContributors, eventSettings } };
  });

  sendJson(res, result.statusCode, result.payload);
}


async function handleApproveChangeRequest(req, res, requestId) {
  const body = await readJsonBody(req);

  const result = await withChangeRequestLock(async () => {
    const current = await readChangeRequests();
    const index = current.findIndex(request => request.id === requestId);
    if (index === -1) {
      return { statusCode: 404, payload: { error: 'Change request not found' } };
    }

    const request = current[index];
    if (request.status !== 'pending') {
      return { statusCode: 409, payload: { error: 'Only pending requests can be approved' } };
    }

    const incomingEntries = Array.isArray(body.entries) && body.entries.length > 0
      ? normalizeChangeRequests([{ ...request, entries: body.entries }])[0]?.entries || []
      : request.entries;

    if (incomingEntries.length === 0) {
      return { statusCode: 400, payload: { error: 'No valid entries to approve' } };
    }

    let applyResult;
    try {
      applyResult = await applyChangesToTranslationFile(request.languageFile, incomingEntries);
    } catch (error) {
      return { statusCode: error.statusCode || 500, payload: { error: error.message || 'Failed to apply request' } };
    }

    if (applyResult.applied === 0) {
      return {
        statusCode: 200,
        payload: {
          ok: true,
          approved: false,
          reason: 'No request keys were found in the current translation file',
          applyResult,
          changeRequests: current,
          eventSettings: await readEventSettings(),
          topContributors: normalizeTopContributors(await readTopContributors()),
        },
      };
    }

    const resolvedAt = new Date().toISOString();
    const approvedRequest = {
      ...request,
      entries: applyResult.appliedEntries,
      status: 'approved',
      resolvedAt,
    };

    current[index] = approvedRequest;
    const changeRequests = await writeChangeRequests(current);
    const eventSettings = await readEventSettings();
    const topContributors = await recomputeAndSaveTopContributors(changeRequests, eventSettings);

    return {
      statusCode: 200,
      payload: {
        ok: true,
        approved: true,
        request: approvedRequest,
        changeRequests,
        topContributors,
        eventSettings,
        applyResult,
      },
    };
  });

  sendJson(res, result.statusCode, result.payload);
}

async function readTopContributors() {
  try {
    const raw = await readFile(TOP_CONTRIBUTORS_FILE, 'utf-8');
    const parsed = JSON.parse(raw);
    if (Array.isArray(parsed)) return normalizeTopContributors(parsed);
    if (Array.isArray(parsed?.topContributors)) return normalizeTopContributors(parsed.topContributors);
    return [];
  } catch (error) {
    if (error?.code === 'ENOENT') return [];
    if (error instanceof SyntaxError) {
      const raw = await readFile(TOP_CONTRIBUTORS_FILE, 'utf-8').catch(() => '');
      await backupCorruptedJson(TOP_CONTRIBUTORS_FILE, raw);
      // The request history is the source of truth. Corrupted cached top file
      // must not reset the leaderboard; it will be regenerated from requests.
      return [];
    }
    throw error;
  }
}

function normalizeTopContributors(value) {
  if (!Array.isArray(value)) return [];
  return value
    .filter(item => item && typeof item.author === 'string')
    .map(item => ({ author: item.author.trim(), count: Number(item.count) || 0 }))
    .filter(item => item.author && item.count > 0 && !isLeaderboardSystemAuthor(item.author))
    .sort((a, b) => b.count - a.count)
    .slice(0, 100);
}

async function handleGetTopContributors(_req, res) {
  const [changeRequests, eventSettings] = await Promise.all([
    readChangeRequests(),
    readEventSettings(),
  ]);
  const topContributors = await getTopContributorsForRead(changeRequests, eventSettings);
  sendJson(res, 200, { topContributors, eventSettings });
}

async function handleSaveTopContributors(_req, res) {
  // The leaderboard is derived exclusively from the global approved request
  // history. Clients must not overwrite it with local browser state.
  sendJson(res, 405, { error: 'Top contributors are global and read-only; approve/reject requests to update them.' });
}


async function handleSaveTranslation(req, res, file) {
  if (isEnglishLanguage({ file, name: file })) {
    sendJson(res, 400, { error: 'English is the source language and cannot be added to Localization files' });
    return;
  }

  if (!isSafeLanguageFileName(file)) {
    sendJson(res, 400, { error: 'Invalid file name' });
    return;
  }

  const body = await readJsonBody(req);
  const content = String(body.content ?? '');

  await mkdir(TRANSLATION_ROOT, { recursive: true });
  await writeFile(safeTranslationPath(`${file}.txt`), content, 'utf-8');

  sendJson(res, 200, { ok: true, file });
}

async function handleApplyTranslationChanges(req, res, file) {
  const body = await readJsonBody(req);
  try {
    const applyResult = await applyChangesToTranslationFile(file, body.changes);
    sendJson(res, 200, applyResult);
  } catch (error) {
    sendJson(res, error.statusCode || 500, { error: error.message || 'Failed to apply changes' });
  }
}

async function handleStatic(req, res, urlPath) {
  if (req.method !== 'GET' && req.method !== 'HEAD') {
    sendText(res, 405, 'Method not allowed');
    return;
  }

  let filePath = safeDistPath(urlPath);

  try {
    const fileStat = await stat(filePath);
    if (fileStat.isDirectory()) {
      filePath = path.join(filePath, 'index.html');
    }
  } catch (_error) {
    // SPA fallback: if a route is not a real file, return dist/index.html.
    filePath = path.join(DIST_ROOT, 'index.html');
  }

  try {
    const body = await readFile(filePath);
    const ext = path.extname(filePath).toLowerCase();
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';
    res.writeHead(200, {
      'Content-Type': contentType,
      'Content-Length': body.length,
    });
    res.end(req.method === 'HEAD' ? undefined : body);
  } catch (_error) {
    sendText(
      res,
      404,
      'Frontend build not found. Run npm run build, then open this server URL again.'
    );
  }
}

const server = http.createServer(async (req, res) => {
  res._acceptEncoding = req.headers['accept-encoding'] || '';
  try {
    if (req.method === 'OPTIONS') {
      sendJson(res, 204, {});
      return;
    }

    const url = new URL(req.url || '/', `http://${req.headers.host || 'localhost'}`);

    if (req.method === 'GET' && url.pathname === '/api/health') {
      await handleHealth(req, res);
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/bootstrap') {
      await handleGetBootstrap(req, res);
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/translations/manifest') {
      await handleGetTranslationManifest(req, res);
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/translations') {
      await handleGetTranslations(req, res);
      return;
    }


    if (req.method === 'GET' && url.pathname === '/api/change-requests') {
      await handleGetChangeRequests(req, res);
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/change-requests') {
      await handleCreateChangeRequest(req, res);
      return;
    }

    const approveRequestMatch = url.pathname.match(/^\/api\/change-requests\/([^/]+)\/approve$/);
    if (approveRequestMatch && req.method === 'POST') {
      await handleApproveChangeRequest(req, res, decodeURIComponent(approveRequestMatch[1]));
      return;
    }

    const requestMatch = url.pathname.match(/^\/api\/change-requests\/([^/]+)$/);
    if (requestMatch && req.method === 'PATCH') {
      await handleUpdateChangeRequest(req, res, decodeURIComponent(requestMatch[1]));
      return;
    }

    if (requestMatch && req.method === 'DELETE') {
      await handleDeleteChangeRequest(req, res, decodeURIComponent(requestMatch[1]));
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/event-settings') {
      await handleGetEventSettings(req, res);
      return;
    }

    if (req.method === 'PATCH' && url.pathname === '/api/event-settings') {
      await handleUpdateEventSettings(req, res);
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/top-contributors/reset') {
      await handleResetTopContributors(req, res);
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/top-contributors') {
      await handleGetTopContributors(req, res);
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/top-contributors') {
      await handleSaveTopContributors(req, res);
      return;
    }


    const applyMatch = url.pathname.match(/^\/api\/translations\/([a-zA-Z0-9_-]+)\/apply$/);
    if (req.method === 'POST' && applyMatch) {
      await handleApplyTranslationChanges(req, res, applyMatch[1]);
      return;
    }

    const saveMatch = url.pathname.match(/^\/api\/translations\/([a-zA-Z0-9_-]+)$/);
    if (req.method === 'POST' && saveMatch) {
      await handleSaveTranslation(req, res, saveMatch[1]);
      return;
    }

    if (url.pathname.startsWith('/api/')) {
      sendJson(res, 404, { error: 'API route not found' });
      return;
    }

    await handleStatic(req, res, url.pathname);
  } catch (error) {
    console.error(error);
    sendJson(res, error?.statusCode || 500, {
      error: error instanceof Error ? error.message : 'Server error',
    });
  }
});

server.on('error', (error) => {
  if (error?.code === 'EADDRINUSE') {
    console.error(`\nPort ${PORT} is already in use.`);
    console.error('Use another port, for example:');
    console.error(`  PORT=${PORT + 1} TRANSLATION_ROOT=${TRANSLATION_ROOT} npm run start`);
    console.error('Or stop the old process:');
    console.error(`  ./start-web-editor.sh stop`);
  } else if (error?.code === 'EACCES') {
    console.error(`\nNo permission to listen on ${HOST}:${PORT}. Use a port above 1024 or run with proper permissions.`);
  } else {
    console.error(error);
  }
  process.exit(1);
});

server.listen(PORT, HOST, () => {
  console.log('Translation web editor started');
  console.log(`Local URL:  http://127.0.0.1:${PORT}`);

  const remoteUrls = getRemoteUrls(PORT);
  if (remoteUrls.length > 0) {
    console.log('Remote URL candidates:');
    for (const url of remoteUrls) {
      console.log(`  ${url}`);
    }
  }

  console.log(`Bind host: ${HOST}`);
  console.log(`Translation root: ${TRANSLATION_ROOT}`);
  console.log('API health check: /api/health');
});
