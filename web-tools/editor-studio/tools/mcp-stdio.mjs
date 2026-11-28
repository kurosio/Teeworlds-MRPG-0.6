#!/usr/bin/env node
/**
 * Model-agnostic MCP stdio bridge for Teeworlds-MRPG-0.6 Editor Studio.
 *
 * Bridges newline-delimited JSON-RPC 2.0 stdin/stdout (used by Claude Desktop,
 * Codex CLI, Cursor, Windsurf, Cline, Zed, etc.) to Editor Studio's HTTP MCP
 * endpoint (/mcp or /api/mcp.php) using an explicit Bearer AI token.
 *
 * Usage:
 *   EDITOR_STUDIO_URL="https://studio.example.com" \
 *   EDITOR_STUDIO_TOKEN="esai_..." \
 *   node tools/mcp-stdio.mjs
 *
 * Or via CLI flags:
 *   node tools/mcp-stdio.mjs --url https://studio.example.com --token esai_...
 *   node tools/mcp-stdio.mjs --url https://studio.example.com --token esai_... --check
 */

import readline from 'node:readline';

const argv = process.argv.slice(2);
function getFlag(name, fallback = '') {
  const idx = argv.indexOf(name);
  if (idx >= 0 && argv[idx + 1] && !argv[idx + 1].startsWith('--')) {
    return argv[idx + 1];
  }
  const eq = argv.find((a) => a.startsWith(`${name}=`));
  return eq ? eq.slice(name.length + 1) : fallback;
}
function hasFlag(name) {
  return argv.includes(name);
}

const rawBaseUrl = getFlag('--url', process.env.EDITOR_STUDIO_URL || '').trim();
const token = getFlag('--token', process.env.EDITOR_STUDIO_TOKEN || '').trim();
const timeoutMs = Math.max(2000, Math.min(300000, Number(getFlag('--timeout', process.env.EDITOR_STUDIO_TIMEOUT || '30000')) || 30000));
const isHealthCheck = hasFlag('--check') || hasFlag('--health');

if (!rawBaseUrl) {
  process.stderr.write('Error: provide EDITOR_STUDIO_URL or --url https://your-editor-studio\n');
  process.exit(2);
}

const baseUrl = rawBaseUrl.replace(/\/+$/, '');
const primaryEndpoint = baseUrl.endsWith('/mcp') || baseUrl.endsWith('/mcp.php')
  ? baseUrl
  : `${baseUrl}/api/mcp.php`;
const fallbackEndpoint = baseUrl.endsWith('/api/mcp.php')
  ? baseUrl
  : `${baseUrl.replace(/\/(mcp|ai)\/?$/, '')}/api/mcp.php`;

let activeEndpoint = primaryEndpoint;

function buildHeaders() {
  const headers = {
    'Content-Type': 'application/json',
    'Accept': 'application/json',
  };
  if (token) {
    headers.Authorization = `Bearer ${token}`;
  }
  return headers;
}

async function rpcPost(payload) {
  const doPost = async (url) => fetch(url, {
    method: 'POST',
    headers: buildHeaders(),
    body: JSON.stringify(payload),
    signal: AbortSignal.timeout(timeoutMs),
  });

  let res = await doPost(activeEndpoint);
  if (res.status === 404 && activeEndpoint !== fallbackEndpoint) {
    activeEndpoint = fallbackEndpoint;
    res = await doPost(activeEndpoint);
  }
  return res;
}

async function runHealthCheck() {
  try {
    const initRes = await rpcPost({ jsonrpc: '2.0', id: 1, method: 'initialize', params: {} });
    if (!initRes.ok) {
      throw new Error(`initialize failed with HTTP ${initRes.status}`);
    }
    const initJson = await initRes.json();
    const toolsRes = await rpcPost({ jsonrpc: '2.0', id: 2, method: 'tools/list', params: {} });
    const toolsJson = toolsRes.ok ? await toolsRes.json() : null;
    const summary = {
      ok: true,
      endpoint: activeEndpoint,
      server: initJson?.result?.serverInfo || null,
      protocolVersion: initJson?.result?.protocolVersion || null,
      tools_count: toolsJson?.result?.tools?.length ?? 0,
      authenticated: false,
      auth: null,
    };
    if (token) {
      const manRes = await rpcPost({
        jsonrpc: '2.0',
        id: 3,
        method: 'tools/call',
        params: { name: 'get_manifest', arguments: { include_schemas: false } },
      });
      const manJson = await manRes.json().catch(() => null);
      if (!manRes.ok || manJson?.error || manJson?.result?.isError) {
        summary.ok = false;
        summary.error = manJson?.error?.message || manJson?.result?.structuredContent?.error || `Auth check failed (HTTP ${manRes.status})`;
        process.stdout.write(`${JSON.stringify(summary, null, 2)}\n`);
        process.exit(1);
      }
      summary.authenticated = true;
      summary.auth = manJson?.result?.structuredContent?.manifest?.auth || null;
    }
    process.stdout.write(`${JSON.stringify(summary, null, 2)}\n`);
    process.exit(0);
  } catch (err) {
    process.stdout.write(`${JSON.stringify({ ok: false, endpoint: activeEndpoint, error: err?.message || String(err) }, null, 2)}\n`);
    process.exit(1);
  }
}

if (isHealthCheck) {
  await runHealthCheck();
}

async function forwardJsonRpc(line) {
  const trimmed = line.trim();
  if (!trimmed) return;

  let parsed;
  try {
    parsed = JSON.parse(trimmed);
  } catch (err) {
    process.stdout.write(`${JSON.stringify({
      jsonrpc: '2.0',
      id: null,
      error: { code: -32700, message: `Parse error: ${err.message}` },
    })}\n`);
    return;
  }

  if (parsed === null || typeof parsed !== 'object') {
    process.stdout.write(`${JSON.stringify({
      jsonrpc: '2.0',
      id: null,
      error: { code: -32600, message: 'Invalid Request: expected JSON-RPC object or batch array.' },
    })}\n`);
    return;
  }

  const reqId = (!Array.isArray(parsed) && parsed.id !== undefined && parsed.id !== null) ? parsed.id : null;

  let res;
  try {
    res = await rpcPost(parsed);
  } catch (err) {
    const isTimeout = err?.name === 'TimeoutError' || err?.name === 'AbortError';
    const msg = isTimeout
      ? `Request timed out after ${timeoutMs}ms (${activeEndpoint})`
      : `Network error reaching ${activeEndpoint}: ${err?.message || String(err)}`;
    if (reqId !== null) {
      process.stdout.write(`${JSON.stringify({
        jsonrpc: '2.0',
        id: reqId,
        error: { code: -32002, message: msg },
      })}\n`);
    } else {
      process.stderr.write(`${msg}\n`);
    }
    return;
  }

  if (res.status === 204) {
    return;
  }

  const text = await res.text();
  if (!res.ok) {
    let message = `HTTP ${res.status}`;
    try {
      const errBody = JSON.parse(text);
      message = errBody?.error?.message || errBody?.error || message;
    } catch {}
    if (reqId !== null) {
      process.stdout.write(`${JSON.stringify({
        jsonrpc: '2.0',
        id: reqId,
        error: { code: res.status === 401 ? -32001 : -32000, message },
      })}\n`);
    }
    return;
  }

  if (text.trim()) {
    // Emit compact single-line JSON-RPC message on stdout; never corrupt framing with non-JSON HTML.
    try {
      process.stdout.write(`${JSON.stringify(JSON.parse(text))}\n`);
    } catch {
      const preview = text.trim().slice(0, 160).replace(/\s+/g, ' ');
      const msg = `Server returned non-JSON response from ${activeEndpoint}: ${preview}`;
      if (reqId !== null) {
        process.stdout.write(`${JSON.stringify({
          jsonrpc: '2.0',
          id: reqId,
          error: { code: -32000, message: msg },
        })}\n`);
      } else {
        process.stderr.write(`${msg}\n`);
      }
    }
  }
}

const rl = readline.createInterface({
  input: process.stdin,
  crlfDelay: Infinity,
});

let chain = Promise.resolve();
rl.on('line', (line) => {
  chain = chain.then(() => forwardJsonRpc(line)).catch((err) => {
    process.stderr.write(`MCP bridge error: ${err.stack || err.message}\n`);
  });
});

rl.on('close', async () => {
  await chain;
  process.exit(0);
});
