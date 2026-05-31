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
    'Access-Control-Allow-Methods': 'GET,POST,OPTIONS',
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

async function handleGetTranslations(_req, res) {
  const indexRaw = await readFile(safeTranslationPath('index.json'), 'utf-8');
  const indexJson = JSON.parse(indexRaw);
  const languages = Array.isArray(indexJson['language indices'])
    ? indexJson['language indices']
    : [];

  const files = {};
  const errors = [];

  for (const lang of languages) {
    if (!lang?.file || !isSafeLanguageFileName(lang.file)) {
      errors.push(`Skipped invalid language file name: ${String(lang?.file || '')}`);
      continue;
    }

    const fileName = `${lang.file}.txt`;
    try {
      files[lang.file] = await readFile(safeTranslationPath(fileName), 'utf-8');
    } catch (_error) {
      files[lang.file] = '';
      errors.push(`File not found or unreadable: ${fileName}`);
    }
  }

  sendJson(res, 200, { languages, files, errors });
}

async function handleSaveTranslation(req, res, file) {
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

    if (req.method === 'GET' && url.pathname === '/api/translations') {
      await handleGetTranslations(req, res);
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
