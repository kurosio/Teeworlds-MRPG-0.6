<?php
// Public model-agnostic AI onboarding & auto-discovery entrypoint (/ai and /ai/).
//
// Zero private server secrets, zero filesystem paths, zero game DB content:
// exposes only public connection instructions, machine-readable URLs,
// OpenAPI / MCP capability descriptors, auth scopes, and safe workflow rules.
//
// Modes:
//   - GET /ai?format=json or Accept: application/json -> pure JSON onboarding manifest
//   - GET /ai (HTML) -> human-friendly & LLM-readable HTML onboarding + embedded JSON + token tester

declare(strict_types=1);

require_once __DIR__ . '/../api/db-core.php';

apply_security_headers();

$scheme = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off')
    || strtolower((string)($_SERVER['HTTP_X_FORWARDED_PROTO'] ?? '')) === 'https'
    ? 'https' : 'http';
$host = trim((string)($_SERVER['HTTP_HOST'] ?? 'localhost'));
$origin = $scheme . '://' . $host;
$scriptName = str_replace('\\', '/', (string)($_SERVER['SCRIPT_NAME'] ?? '/ai/index.php'));
// Strip /ai/index.php or /ai to resolve Studio root subpath cleanly.
$studioPath = preg_replace('#/ai(?:/index\.php)?$#', '', $scriptName);
$studioPath = rtrim((string)$studioPath, '/');
$baseUrl = $origin . $studioPath;

$doc = [
    'schema_version' => 'v1',
    'api_version' => '2.0',
    'service' => 'Teeworlds-MRPG-0.6 Editor Studio AI Platform',
    'model_agnostic' => true,
    'description' => 'Universal AI content creation, analysis, validation, and Change Request platform for Teeworlds-MRPG-0.6.',
    'urls' => [
        'studio' => $baseUrl . '/',
        'onboarding_html' => $baseUrl . '/ai/',
        'onboarding_json' => $baseUrl . '/ai/?format=json',
        'llms_txt' => $baseUrl . '/llms.txt', // alias, requires rewrite
        'llms_txt_api' => $baseUrl . '/api/ai.php?action=llms_txt',
        'discovery_api' => $baseUrl . '/api/ai.php?action=discovery',
        'discovery_manifest' => $baseUrl . '/.well-known/ai-plugin.json', // alias, requires rewrite
        'openapi_3_1' => $baseUrl . '/api/ai.php?action=openapi',
        'openapi_direct' => $baseUrl . '/openapi.json', // alias, requires rewrite
        'mcp_api' => $baseUrl . '/api/mcp.php',
        'mcp_http' => $baseUrl . '/mcp', // alias, requires rewrite
        'rest_api' => $baseUrl . '/api/ai.php',
        'auth_status' => $baseUrl . '/api/auth.php?action=status',
        'guest_login' => $baseUrl . '/api/auth.php?action=guest_login',
    ],
    'auth' => [
        'recommended' => 'Scoped Bearer AI Token (issued by Studio administrator in Guest users)',
        'header' => 'Authorization: Bearer esai_<token>',
        'alt_header' => 'X-AI-Token: esai_<token>',
        'scopes' => ['read', 'create', 'update', 'delete'],
        'fallback' => 'Guest cookie session via POST api/auth.php?action=guest_login (requires X-Editor-CSRF on POST)',
        'safety_boundary' => 'AI tokens strictly map to guest permissions. Direct SQL/disk mutations are impossible; writes create Change Requests queued for human administrator approval.',
    ],
    'quickstart_steps' => [
        '1. Authenticate with Authorization: Bearer esai_<token> (or guest cookie session).',
        '2. Fetch capability manifest & cookbook: GET api/ai.php?action=manifest (or MCP get_manifest + resource mrpg://cookbook).',
        '3. Orient in 1 call: GET api/ai.php?action=search&q=<term> (MCP search_editor) and GET api/ai.php?action=context&resource=worlds|quests|items|bots_info&id=<id> (MCP get_context).',
        '4. Analyze before authoring: GET api/ai.php?action=analyze&resource=scenarios (corpus overview) and GET api/ai.php?action=similar&resource=<res>&name=<Name> (MCP find_similar) to avoid duplicates.',
        '5. For spatial placement on maps, call GET api/ai.php?action=analyze&resource=maps&world=<id> (MCP analyze_map) and GET api/ai.php?action=map_screenshot&world=<id>&target=overview|guild_house:1|house:1 (MCP get_map_screenshot) to visually inspect a real PNG screenshot.',
        '6. Read multiple records in 1 call with GET action=get&resource=<res>&ids=1,2,3 (MCP get_record ids=[...]); filter with operators ({"Level":{"gte":5}}) and sort=-ID in list. Keep sha256 from scenario get as baseHash.',
        '7. Author multi-object batches cleanly using symbolic "ref":"alias" and "$alias" across FKs, DSL ("[$ore/3]"), and JSON, or draft minimal patch_json (RFC 7396 merge patch) / type="copy".',
        '8. Verify patch_json server-side without writing: POST api/ai.php?action=apply_patch (MCP apply_patch), then validate & dry-run via action=validate and action=preview.',
        '9. Submit Change Request: POST api/ai.php?action=propose with descriptive message and optional idempotency_key (confirm_dangerous=true only after reviewing dangerous_actions).',
        '10. Close the review loop: inspect admin feedback, reply, revise, or withdraw pending requests via GET action=request / POST action=request_reply / POST action=request_withdraw (MCP manage_change_request).',
    ],
    'operation_formats' => [
        'supported_types' => ['create', 'update', 'copy', 'delete'],
        'type_alias' => 'Either "type" or "action" may be supplied on each operation object.',
        'patch_json' => 'On update/copy, supply patch_json: {"Data": {"bonus": {...}}} (DB) or patch_json: {...} (scenarios) to preserve unrelated existing fields.',
        'symbolic_refs' => 'Declare "ref": "my_item" on any DB create/copy op and reference "$my_item" in FK columns, DSL strings ("[$my_item/3]"), or JSON within the same batch.',
        'temp_id' => 'Omit tempId on DB create/copy for automatic 2000000001+ assignment, or pass explicit tempId (2000000000..2000999999) for cross-references in the same batch.',
        'verify_before_propose' => 'POST action=apply_patch {resource, id|filename, patch_json, baseHash?} — server-side merge of patch_json against the live record/file (read-only). Returns before/after/diff/noop and the same errors action=propose would emit. Never propose an unverified patch.',
    ],
    'mcp' => [
        'protocolVersion' => '2024-11-05',
        'supportedVersions' => ['2024-11-05', '2025-03-26', '2025-06-18'],
        'transports' => [
            'http_json_rpc_2_0' => $baseUrl . '/api/mcp.php', // alias: /mcp (requires rewrite)
            'stdio_bridge_command' => 'node tools/mcp-stdio.mjs --url ' . $baseUrl . ' --token <AI_TOKEN>',
            'stdio_health_check' => 'node tools/mcp-stdio.mjs --url ' . $baseUrl . ' --token <AI_TOKEN> --check',
        ],
        'resources' => [
            'mrpg://manifest', 'mrpg://cookbook', 'mrpg://openapi', 'mrpg://llms.txt',
            'mrpg://schema/{resource}', 'mrpg://context/{resource}/{id}',
        ],
        'tools' => [
            'get_manifest', 'get_schema', 'search_editor', 'get_context',
            'list_records', 'get_record', 'get_references',
            'analyze_scenario', 'analyze_map', 'get_map_screenshot', 'find_similar',
            'validate_operations', 'preview_operations', 'apply_patch', 'propose_changes',
            'manage_change_request', 'copy_record', 'patch_json_record', 'get_history',
        ],
    ],
];

$format = strtolower(trim((string)($_GET['format'] ?? '')));
$accept = strtolower((string)($_SERVER['HTTP_ACCEPT'] ?? ''));
$wantsJson = $format === 'json' || (str_contains($accept, 'application/json') && !str_contains($accept, 'text/html'));

if ($wantsJson) {
    respond($doc);
}

$h = static fn(string $v): string => htmlspecialchars($v, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
$docJson = (string)json_encode($doc, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES | JSON_PRETTY_PRINT);
header('Content-Type: text/html; charset=utf-8');
?>
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>AI &amp; MCP Onboarding • Teeworlds-MRPG-0.6 Editor Studio</title>
  <link rel="alternate" type="application/json" title="AI Onboarding JSON" href="?format=json" />
  <link rel="service-desc" type="application/json" title="OpenAPI 3.1.0" href="../api/ai.php?action=openapi" />
  <link rel="ai-plugin" type="application/json" href="../api/ai.php?action=discovery" />
  <link rel="stylesheet" href="../vendor/fontawesome/css/all.min.css" />
  <link rel="stylesheet" href="../editor-core/editor-theme.css" />
  <link rel="stylesheet" href="../editor-core/editor-template.css" />
  <link rel="stylesheet" href="../editor-core/tailwind.css" />
  <script src="../editor-core/tailwind-theme.js"></script>
  <style>
    .ai-wrap { max-width: 1100px; margin: 0 auto; padding: 28px 18px 60px; }
    .ai-hero { border: 1px solid var(--editor-line); background: var(--editor-surface); border-radius: var(--editor-radius-lg); padding: 24px; }
    .ai-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 16px; margin-top: 18px; }
    .ai-card { border: 1px solid var(--editor-line); background: var(--editor-surface); border-radius: var(--editor-radius-md); padding: 18px; }
    .ai-pre { background: var(--editor-soft-bg); border: 1px solid var(--editor-soft-border); border-radius: 10px; padding: 12px; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: 12px; overflow-x: auto; white-space: pre-wrap; word-break: break-word; }
    .ai-pill { display: inline-flex; align-items: center; gap: 6px; padding: 3px 10px; border-radius: 999px; font-size: 12px; font-weight: 600; border: 1px solid color-mix(in srgb, var(--editor-accent) 45%, transparent); color: var(--editor-accent); background: color-mix(in srgb, var(--editor-accent) 10%, transparent); }
    .ai-kv { display: grid; grid-template-columns: max-content minmax(0, 1fr); gap: 6px 14px; font-size: 13px; }
    .ai-kv dt { color: var(--editor-text-muted); font-weight: 600; }
    .ai-kv dd { word-break: break-all; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: 12.5px; }
    .ai-tabs { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 10px; }
    .ai-tab { padding: 5px 11px; border-radius: 8px; font-size: 12px; font-weight: 600; border: 1px solid var(--editor-soft-border-strong); background: var(--editor-soft-bg); color: var(--editor-text-muted); cursor: pointer; }
    .ai-tab.is-active { color: var(--editor-accent); border-color: color-mix(in srgb, var(--editor-accent) 55%, transparent); background: color-mix(in srgb, var(--editor-accent) 12%, transparent); }
  </style>
</head>
<body class="editor-theme">
  <main class="ai-wrap">
    <section class="ai-hero">
      <div class="flex flex-wrap items-center justify-between gap-3">
        <span class="ai-pill"><i class="fa-solid fa-robot"></i> Model-Agnostic Universal AI &amp; MCP Entrypoint</span>
        <div class="flex flex-wrap gap-2">
          <a class="editor-btn editor-btn-secondary" href="?format=json"><i class="fa-solid fa-code"></i><span>JSON Discovery</span></a>
          <a class="editor-btn editor-btn-secondary" href="../api/ai.php?action=llms_txt"><i class="fa-solid fa-file-lines"></i><span>llms.txt</span></a>
          <a class="editor-btn editor-btn-secondary" href="../api/ai.php?action=openapi"><i class="fa-solid fa-file-code"></i><span>OpenAPI 3.1.0</span></a>
          <a class="editor-btn editor-btn-secondary" href="../"><i class="fa-solid fa-arrow-left"></i><span>Open Studio</span></a>
        </div>
      </div>
      <h1 class="text-2xl font-bold mt-3">Teeworlds-MRPG-0.6 Editor Studio — AI Connection Guide</h1>
      <p class="editor-muted-text mt-2 text-sm">
        Share this single URL (<code><?= $h($baseUrl . '/ai/') ?></code>) or <code><?= $h($baseUrl . '/llms.txt') ?></code> with any AI assistant (ChatGPT / OpenAI, Claude, Gemini, Codex, Cursor, Windsurf, VS Code, or local LLMs). Everything needed for schema discovery, cross-resource search, composite world/quest context, map screenshots, safe JSON editing, symbolic batch linking, and MCP connection is exposed here without leaking secrets.
      </p>
    </section>

    <div class="ai-grid">
      <section class="ai-card">
        <h2 class="font-bold text-base mb-3"><i class="fa-solid fa-compass text-accent mr-1.5"></i> Discovery &amp; Endpoints</h2>
        <dl class="ai-kv">
          <dt>JSON Onboarding</dt><dd><a class="underline" href="?format=json"><?= $h($baseUrl . '/ai/?format=json') ?></a></dd>
          <dt>llms.txt Guide</dt><dd><a class="underline" href="../api/ai.php?action=llms_txt"><?= $h($baseUrl . '/api/ai.php?action=llms_txt') ?></a> <span class="editor-muted-text">(alias: /llms.txt)</span></dd>
          <dt>Plugin Descriptor</dt><dd><a class="underline" href="../api/ai.php?action=discovery"><?= $h($baseUrl . '/api/ai.php?action=discovery') ?></a> <span class="editor-muted-text">(alias: /.well-known/ai-plugin.json)</span></dd>
          <dt>OpenAPI 3.1.0</dt><dd><a class="underline" href="../api/ai.php?action=openapi"><?= $h($baseUrl . '/api/ai.php?action=openapi') ?></a> <span class="editor-muted-text">(alias: /openapi.json — only with rewrite)</span></dd>
          <dt>MCP (HTTP RPC 2.0)</dt><dd><a class="underline" href="../api/mcp.php"><?= $h($baseUrl . '/api/mcp.php') ?></a> <span class="editor-muted-text">(alias: /mcp — only with rewrite)</span></dd>
          <dt>REST API</dt><dd><?= $h($baseUrl . '/api/ai.php?action=manifest') ?></dd>
        </dl>
      </section>

      <section class="ai-card">
        <h2 class="font-bold text-base mb-3"><i class="fa-solid fa-shield-halved text-accent mr-1.5"></i> Scoped Auth &amp; Safety Rules</h2>
        <ul class="text-sm space-y-1.5">
          <li><strong>Header:</strong> <code>Authorization: Bearer esai_&lt;token&gt;</code> (or <code>X-AI-Token</code>)</li>
          <li><strong>Scopes:</strong> <code>read</code>, <code>create</code>, <code>update</code>, <code>delete</code> — enforced server-side per operation</li>
          <li><strong>No Direct DB Writes:</strong> every write becomes a Change Request reviewed by an administrator</li>
          <li><strong>Fast Context &amp; Maps:</strong> use <code>search_editor</code>, <code>get_context</code>, <code>analyze_map</code>, and <code>get_map_screenshot</code> (native PNG)</li>
          <li><strong>Atomic Linking &amp; Safe Patch:</strong> link new batch rows via <code>"ref":"alias"</code> / <code>"$alias"</code>; edit JSON via <code>patch_json</code> + <code>apply_patch</code></li>
          <li><strong>Confirmation Gate:</strong> deletions, bulk &gt;10 ops, clearing JSON, or editing worlds/dungeons/vouchers require <code>confirm_dangerous: true</code></li>
        </ul>
      </section>
    </div>

    <section class="ai-card mt-4">
      <div class="flex flex-wrap items-center justify-between gap-2 mb-2">
        <h2 class="font-bold text-base"><i class="fa-solid fa-plug text-accent mr-1.5"></i> Ready-to-Paste Client Configs &amp; Prompts</h2>
        <button type="button" class="editor-btn editor-btn-secondary editor-btn--xs" id="copy-active-snippet"><i class="fa-solid fa-copy"></i><span>Copy</span></button>
      </div>
      <div class="ai-tabs" role="tablist" id="snippet-tabs">
        <button type="button" class="ai-tab is-active" data-tab="mcp-http" role="tab" aria-selected="true">MCP HTTP (Cursor / Windsurf / VS Code / Claude Code)</button>
        <button type="button" class="ai-tab" data-tab="mcp-stdio" role="tab" aria-selected="false">MCP Stdio (Claude Desktop / Codex)</button>
        <button type="button" class="ai-tab" data-tab="chat-prompt" role="tab" aria-selected="false">Chat Prompt (ChatGPT / Claude / Gemini)</button>
        <button type="button" class="ai-tab" data-tab="curl" role="tab" aria-selected="false">cURL Quickstart</button>
      </div>
      <pre class="ai-pre" id="snippet-output"></pre>
      <div class="text-xs editor-muted-text mt-2">Tip: paste your token below and enable “Insert token into snippets” to generate copy-ready configs in browser memory.</div>
    </section>

    <section class="ai-card mt-4">
      <h2 class="font-bold text-base mb-2"><i class="fa-solid fa-key text-accent mr-1.5"></i> Interactive Token &amp; Capabilities Check</h2>
      <p class="text-xs editor-muted-text mb-3">Paste a guest-scoped Bearer AI token (starts with <code>esai_</code>) to verify its active scopes, tabs, and live manifest. The token stays strictly in page memory and is never stored.</p>
      <div class="flex flex-wrap gap-2">
        <input id="ai-token-input" type="password" class="editor-input form-input flex-1 min-w-[240px]" placeholder="esai_..." autocomplete="off" />
        <button type="button" class="editor-btn editor-btn-primary" id="ai-test-btn"><i class="fa-solid fa-circle-check"></i><span>Test Connection</span></button>
      </div>
      <label class="inline-flex items-center gap-2 text-xs editor-muted-text mt-2 cursor-pointer">
        <input id="ai-fill-snippets" type="checkbox" />
        <span>Insert pasted token into the copyable snippets above (memory only)</span>
      </label>
      <pre class="ai-pre mt-3 hidden" id="ai-test-out"></pre>
    </section>
  </main>

  <!-- Machine-readable JSON manifest embedded for HTML-only web fetchers -->
  <script type="application/json" id="ai-discovery-manifest"><?= str_replace('</', '<\/', $docJson) ?></script>

  <script>
  (() => {
    const BASE_URL = <?= json_encode($baseUrl, JSON_UNESCAPED_SLASHES) ?>;
    let activeTab = 'mcp-http';

    const getTokenPlaceholder = () => {
      const fill = document.getElementById('ai-fill-snippets')?.checked;
      const val = (document.getElementById('ai-token-input')?.value || '').trim();
      return (fill && val) ? val : 'esai_<YOUR_TOKEN>';
    };

    const buildSnippet = (tab) => {
      const tok = getTokenPlaceholder();
      if (tab === 'mcp-http') {
        return JSON.stringify({
          mcpServers: {
            'mrpg-editor-studio': {
              url: `${BASE_URL}/api/mcp.php`,
              headers: {
                Authorization: `Bearer ${tok}`,
              },
            },
          },
        }, null, 2);
      }
      if (tab === 'mcp-stdio') {
        return JSON.stringify({
          mcpServers: {
            'mrpg-editor-studio': {
              command: 'node',
              args: ['tools/mcp-stdio.mjs', '--url', BASE_URL],
              env: {
                EDITOR_STUDIO_TOKEN: tok,
              },
            },
          },
        }, null, 2) + `\n\n# Health check:\n# node tools/mcp-stdio.mjs --url ${BASE_URL} --token ${tok} --check`;
      }
      if (tab === 'chat-prompt') {
        return `Connect to Teeworlds-MRPG-0.6 Editor Studio at ${BASE_URL}:
1. Discovery: GET ${BASE_URL}/api/ai.php?action=llms_txt (or ${BASE_URL}/ai/?format=json) and ${BASE_URL}/api/ai.php?action=openapi.
2. Auth header: Authorization: Bearer ${tok}
3. Inspect GET api/ai.php?action=manifest and orient quickly with action=search&q=<term> and action=context&resource=worlds|quests|items&id=<id>.
4. For maps/placement: GET action=analyze&resource=maps&world=<id> and GET action=map_screenshot&world=<id>&target=overview|guild_house:1|house:1.
5. Check duplicates via action=similar and scenarios via action=analyze&resource=scenarios.
6. Read batches via action=get&resource=<res>&ids=1,2,3; link new batch rows via "ref":"alias" / "$alias".
7. Prefer patch_json verified via POST action=apply_patch, then validate -> preview -> propose. Track/reply/withdraw pending requests via action=request / request_reply / request_withdraw.`;
      }
      return `# 1. Public discovery
curl -s "${BASE_URL}/api/ai.php?action=llms_txt"

# 2. Check token & capabilities
curl -s -H "Authorization: Bearer ${tok}" "${BASE_URL}/api/ai.php?action=manifest&include_schemas=0"

# 3. Cross-resource search
curl -s -H "Authorization: Bearer ${tok}" "${BASE_URL}/api/ai.php?action=search&q=sword"

# 4. World composite context
curl -s -H "Authorization: Bearer ${tok}" "${BASE_URL}/api/ai.php?action=context&resource=worlds&id=0"`;
    };

    const renderSnippet = () => {
      const out = document.getElementById('snippet-output');
      if (out) out.textContent = buildSnippet(activeTab);
    };

    const copyText = async (text) => {
      try {
        await navigator.clipboard.writeText(text);
        return true;
      } catch {
        return false;
      }
    };

    document.getElementById('snippet-tabs')?.addEventListener('click', (e) => {
      const btn = e.target.closest('[data-tab]');
      if (!btn) return;
      activeTab = btn.dataset.tab || 'mcp-http';
      document.querySelectorAll('#snippet-tabs [data-tab]').forEach((b) => {
        const on = b === btn;
        b.classList.toggle('is-active', on);
        b.setAttribute('aria-selected', on ? 'true' : 'false');
      });
      renderSnippet();
    });

    document.getElementById('copy-active-snippet')?.addEventListener('click', async (e) => {
      const btn = e.currentTarget;
      const ok = await copyText(buildSnippet(activeTab));
      if (ok && btn) {
        const span = btn.querySelector('span');
        const prev = span ? span.textContent : '';
        if (span) span.textContent = 'Copied!';
        setTimeout(() => { if (span) span.textContent = prev || 'Copy'; }, 1400);
      }
    });

    document.getElementById('ai-fill-snippets')?.addEventListener('change', renderSnippet);
    document.getElementById('ai-token-input')?.addEventListener('input', () => {
      if (document.getElementById('ai-fill-snippets')?.checked) renderSnippet();
    });

    renderSnippet();

    document.getElementById('ai-test-btn')?.addEventListener('click', async () => {
      const tok = (document.getElementById('ai-token-input')?.value || '').trim();
      const out = document.getElementById('ai-test-out');
      if (!out) return;
      out.classList.remove('hidden');
      if (!tok) {
        out.textContent = 'Enter a Bearer token (esai_...) first.';
        return;
      }
      out.textContent = 'Checking status & manifest...';
      try {
        const headers = { Authorization: `Bearer ${tok}` };
        const [stRes, manRes] = await Promise.all([
          fetch('../api/auth.php?action=status', { headers }),
          fetch('../api/ai.php?action=manifest&include_schemas=0', { headers }),
        ]);
        const st = await stRes.json();
        const man = await manRes.json();
        out.textContent = JSON.stringify({ auth_status: st, manifest_summary: man?.manifest || man }, null, 2);
      } catch (err) {
        out.textContent = `Request failed: ${err.message || err}`;
      }
    });
  })();
  </script>
</body>
</html>
