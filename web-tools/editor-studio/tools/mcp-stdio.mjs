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

const rawBaseUrl = getFlag('--url', process.env.EDITOR_STUDIO_URL || '').trim();
const token = getFlag('--token', process.env.EDITOR_STUDIO_TOKEN || '').trim();

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

  const headers = {
    'Content-Type': 'application/json',
    'Accept': 'application/json',
  };
  if (token) {
    headers.Authorization = `Bearer ${token}`;
  }

  const doPost = async (url) => fetch(url, {
    method: 'POST',
    headers,
    body: JSON.stringify(parsed),
  });

  let res = await doPost(activeEndpoint);
  if (res.status === 404 && activeEndpoint !== fallbackEndpoint) {
    activeEndpoint = fallbackEndpoint;
    res = await doPost(activeEndpoint);
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
    if (parsed.id !== undefined && parsed.id !== null) {
      process.stdout.write(`${JSON.stringify({
        jsonrpc: '2.0',
        id: parsed.id,
        error: { code: res.status === 401 ? -32001 : -32000, message },
      })}\n`);
    }
    return;
  }

  if (text.trim()) {
    // Emit compact single-line JSON-RPC message on stdout.
    try {
      process.stdout.write(`${JSON.stringify(JSON.parse(text))}\n`);
    } catch {
      process.stdout.write(`${text.trim()}\n`);
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
