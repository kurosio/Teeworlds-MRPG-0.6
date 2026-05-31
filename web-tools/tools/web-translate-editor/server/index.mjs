import http from 'node:http';
import { readFile, writeFile, mkdir, stat } from 'node:fs/promises';
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

const CHANGE_REQUESTS_FILE = path.resolve(
  process.env.CHANGE_REQUESTS_FILE || path.join(TRANSLATION_ROOT, 'change_requests.json')
);

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
  res.writeHead(statusCode, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(body),
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET,POST,PATCH,DELETE,OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type',
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

async function handleGetTranslations(_req, res) {
  const { languages, versions, errors } = await readTranslationManifest();
  const files = {};

  for (const lang of languages) {
    if (!lang?.file || !isSafeLanguageFileName(lang.file)) {
      continue;
    }

    const fileName = `${lang.file}.txt`;
    try {
      files[lang.file] = await readFile(safeTranslationPath(fileName), 'utf-8');
    } catch (_error) {
      files[lang.file] = '';
    }
  }

  sendJson(res, 200, { languages, files, versions, errors });
}


async function readChangeRequests() {
  try {
    const raw = await readFile(CHANGE_REQUESTS_FILE, 'utf-8');
    const parsed = JSON.parse(raw);
    if (Array.isArray(parsed)) return normalizeChangeRequests(parsed);
    if (Array.isArray(parsed?.changeRequests)) return normalizeChangeRequests(parsed.changeRequests);
  } catch (_error) {
    // Missing file is allowed on first launch.
  }
  return [];
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
  await mkdir(path.dirname(CHANGE_REQUESTS_FILE), { recursive: true });
  await writeFile(CHANGE_REQUESTS_FILE, JSON.stringify({ changeRequests: normalized }, null, 2), 'utf-8');
  return normalized;
}

async function handleGetChangeRequests(_req, res) {
  const changeRequests = await readChangeRequests();
  sendJson(res, 200, { changeRequests });
}

async function handleCreateChangeRequest(req, res) {
  const body = await readJsonBody(req);
  const incoming = normalizeChangeRequests([{ ...body, status: body.status || 'pending', createdAt: body.createdAt || new Date().toISOString() }]);
  if (incoming.length !== 1) {
    sendJson(res, 400, { error: 'Invalid change request' });
    return;
  }

  const current = await readChangeRequests();
  if (current.some(request => request.id === incoming[0].id)) {
    sendJson(res, 409, { error: 'Change request already exists' });
    return;
  }

  const changeRequests = await writeChangeRequests([incoming[0], ...current]);
  sendJson(res, 200, { ok: true, request: incoming[0], changeRequests });
}


function getTranslatedTextAmount(entries) {
  return entries.reduce((total, entry) => {
    const text = String(entry?.newTranslation ?? '').trim();
    return total + text.length;
  }, 0);
}

function buildTopContributorsFromRequests(changeRequests) {
  const authorStats = new Map();
  for (const request of normalizeChangeRequests(changeRequests)) {
    if (request.status !== 'approved') continue;
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

async function recomputeAndSaveTopContributors(changeRequests) {
  const topContributors = normalizeTopContributors(buildTopContributorsFromRequests(changeRequests));
  await mkdir(path.dirname(TOP_CONTRIBUTORS_FILE), { recursive: true });
  await writeFile(TOP_CONTRIBUTORS_FILE, JSON.stringify({ topContributors }, null, 2), 'utf-8');
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
  const current = await readChangeRequests();
  const index = current.findIndex(request => request.id === requestId);
  if (index === -1) {
    sendJson(res, 404, { error: 'Change request not found' });
    return;
  }

  const merged = {
    ...current[index],
    ...body,
    id: current[index].id,
    createdAt: current[index].createdAt,
  };
  const normalized = normalizeChangeRequests([merged]);
  if (normalized.length !== 1) {
    sendJson(res, 400, { error: 'Invalid change request update' });
    return;
  }

  current[index] = normalized[0];
  const changeRequests = await writeChangeRequests(current);
  const topContributors = await recomputeAndSaveTopContributors(changeRequests);
  sendJson(res, 200, { ok: true, request: normalized[0], changeRequests, topContributors });
}

async function handleDeleteChangeRequest(_req, res, requestId) {
  const current = await readChangeRequests();
  const next = current.filter(request => request.id !== requestId);
  if (next.length === current.length) {
    sendJson(res, 404, { error: 'Change request not found' });
    return;
  }
  const changeRequests = await writeChangeRequests(next);
  const topContributors = await recomputeAndSaveTopContributors(changeRequests);
  sendJson(res, 200, { ok: true, changeRequests, topContributors });
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
    const topContributors = await recomputeAndSaveTopContributors(changeRequests);

    return {
      statusCode: 200,
      payload: {
        ok: true,
        approved: true,
        request: approvedRequest,
        changeRequests,
        topContributors,
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
    if (Array.isArray(parsed)) return parsed;
    if (Array.isArray(parsed?.topContributors)) return parsed.topContributors;
  } catch (_error) {
    // Missing file is allowed on first launch.
  }
  return [];
}

function normalizeTopContributors(value) {
  if (!Array.isArray(value)) return [];
  return value
    .filter(item => item && typeof item.author === 'string')
    .map(item => ({ author: item.author.trim(), count: Number(item.count) || 0 }))
    .filter(item => item.author && item.count > 0)
    .sort((a, b) => b.count - a.count)
    .slice(0, 100);
}

async function handleGetTopContributors(_req, res) {
  // The global request history is the source of truth for the leaderboard.
  // Recompute on read so old installations that already have approved requests
  // but no top_contributors.json still show the correct global top.
  const changeRequests = await readChangeRequests();
  const topContributors = await recomputeAndSaveTopContributors(changeRequests);
  sendJson(res, 200, { topContributors });
}

async function handleSaveTopContributors(req, res) {
  const body = await readJsonBody(req);
  const topContributors = normalizeTopContributors(body.topContributors);
  await mkdir(path.dirname(TOP_CONTRIBUTORS_FILE), { recursive: true });
  await writeFile(TOP_CONTRIBUTORS_FILE, JSON.stringify({ topContributors }, null, 2), 'utf-8');
  sendJson(res, 200, { ok: true, topContributors });
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
    sendJson(res, 500, {
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
