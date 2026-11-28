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
        '2. Fetch capability manifest: GET api/ai.php?action=manifest (or MCP tool get_manifest).',
        '3. Analyze before authoring: GET api/ai.php?action=analyze&resource=scenarios (corpus: component histogram, missing dependencies, invalid files; &id=file.json for one scenario) — reuse existing components instead of cloning, and fix missing_references.',
        '4. Find duplicates before creating: GET api/ai.php?action=similar&resource=<res>&name=<Name> (or MCP find_similar). Creates also return warning similar_records_found when similar rows exist.',
        '5. Read existing records/scenarios/maps: list, get (with include_references=1), references endpoints. For scenario updates remember sha256 from get — it is the baseHash for propose.',
        '6. Draft minimal patch_json (RFC 7396 merge patch) instead of full data replacement; duplicate objects via type="copy".',
        '7. Verify the merged result server-side (writes nothing): POST api/ai.php?action=apply_patch {resource, id|filename, patch_json, baseHash?} — it returns before/after/diff plus the exact errors propose would emit (or MCP apply_patch).',
        '8. Validate & dry-run: POST api/ai.php?action=validate and action=preview.',
        '9. Submit Change Request: POST api/ai.php?action=propose with descriptive message and optional idempotency_key.',
        '10. For deletions, clearing non-empty JSON, bulk >10 ops, or editing worlds/dungeons/vouchers, inspect dangerous_actions from preview and confirm explicitly with confirm_dangerous=true.',
    ],
    'operation_formats' => [
        'supported_types' => ['create', 'update', 'copy', 'delete'],
        'type_alias' => 'Either "type" or "action" may be supplied on each operation object.',
        'patch_json' => 'On update/copy, supply patch_json: {"Data": {"bonus": {...}}} (DB) or patch_json: {...} (scenarios) to preserve unrelated existing fields.',
        'temp_id' => 'Omit tempId on DB create/copy for automatic 2000000001+ assignment, or pass explicit tempId (2000000000..2000999999) for cross-references in the same batch.',
        'verify_before_propose' => 'POST action=apply_patch {resource, id|filename, patch_json, baseHash?} — server-side merge of patch_json against the live record/file (read-only). Returns before/after/diff/noop and the same errors action=propose would emit. Never propose an unverified patch.',
    ],
    'mcp' => [
        'protocolVersion' => '2024-11-05',
        'transports' => [
            'http_json_rpc_2_0' => $baseUrl . '/api/mcp.php', // alias: /mcp (requires rewrite)
            'stdio_bridge_command' => 'node tools/mcp-stdio.mjs --url ' . $baseUrl . ' --token <AI_TOKEN>',
        ],
        'tools' => [
            'get_manifest', 'get_schema', 'list_records', 'get_record', 'get_references',
            'analyze_scenario', 'find_similar',
            'validate_operations', 'preview_operations', 'apply_patch', 'propose_changes',
            'copy_record', 'patch_json_record', 'get_history',
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
  </style>
</head>
<body class="editor-theme">
  <main class="ai-wrap">
    <section class="ai-hero">
      <div class="flex flex-wrap items-center justify-between gap-3">
        <span class="ai-pill"><i class="fa-solid fa-robot"></i> Model-Agnostic Universal AI &amp; MCP Entrypoint</span>
        <div class="flex flex-wrap gap-2">
          <a class="editor-btn editor-btn-secondary" href="?format=json"><i class="fa-solid fa-code"></i><span>JSON Discovery</span></a>
          <a class="editor-btn editor-btn-secondary" href="../api/ai.php?action=openapi"><i class="fa-solid fa-file-code"></i><span>OpenAPI 3.1.0</span></a>
          <a class="editor-btn editor-btn-secondary" href="../"><i class="fa-solid fa-arrow-left"></i><span>Open Studio</span></a>
        </div>
      </div>
      <h1 class="text-2xl font-bold mt-3">Teeworlds-MRPG-0.6 Editor Studio — AI Connection Guide</h1>
      <p class="editor-muted-text mt-2 text-sm">
        Share this single URL (<code><?= $h($baseUrl . '/ai/') ?></code>) with any AI assistant (ChatGPT / OpenAI, Claude, Gemini, Codex, Cursor, Windsurf, or local LLMs). Everything needed for schema discovery, safe JSON editing, MCP connection, and server-compatible Change Request submission is exposed here without exposing any secrets.
      </p>
    </section>

    <div class="ai-grid">
      <section class="ai-card">
        <h2 class="font-bold text-base mb-3"><i class="fa-solid fa-compass text-accent mr-1.5"></i> Discovery &amp; Endpoints</h2>
        <dl class="ai-kv">
          <dt>JSON Onboarding</dt><dd><a class="underline" href="?format=json"><?= $h($baseUrl . '/ai/?format=json') ?></a></dd>
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
          <li><strong>Safe Editing:</strong> pass <code>patch_json</code> to merge JSON fields without dropping existing keys; use <code>copy</code> to duplicate objects</li>
          <li><strong>Confirmation Gate:</strong> deletions, bulk &gt;10 ops, clearing JSON, or editing worlds/dungeons/vouchers require <code>confirm_dangerous: true</code></li>
        </ul>
      </section>
    </div>

    <div class="ai-grid">
      <section class="ai-card">
        <div class="flex items-center justify-between gap-2 mb-2">
          <h2 class="font-bold text-base"><i class="fa-solid fa-terminal text-accent mr-1.5"></i> Prompt for Any Chat / Agent</h2>
          <button type="button" class="editor-btn editor-btn-secondary editor-btn--xs" id="copy-prompt"><i class="fa-solid fa-copy"></i><span>Copy</span></button>
        </div>
        <pre class="ai-pre" id="prompt-snippet">Connect to Teeworlds-MRPG-0.6 Editor Studio at <?= $h($baseUrl) ?>:
1. Discovery: GET <?= $h($baseUrl . '/ai/?format=json') ?> and GET <?= $h($baseUrl . '/api/ai.php?action=openapi') ?>.
2. Auth header: Authorization: Bearer &lt;YOUR_AI_TOKEN&gt;
3. Inspect GET api/ai.php?action=manifest and action=resources&amp;resource=&lt;name&gt;.
4. Analyze first: GET action=analyze&amp;resource=scenarios (structure, missing dependencies, reusable components) and find duplicates with GET action=similar&amp;resource=&lt;res&gt;&amp;name=&lt;Name&gt;.
5. Read existing items/worlds/scenarios via action=list and action=get before proposing changes.
6. Prefer patch_json for edits; verify it server-side with POST action=apply_patch (read-only before/after/diff).
7. Then call action=validate and action=preview; submit only a clean package via action=propose.</pre>
      </section>

      <section class="ai-card">
        <div class="flex items-center justify-between gap-2 mb-2">
          <h2 class="font-bold text-base"><i class="fa-solid fa-plug text-accent mr-1.5"></i> MCP Client Config (HTTP / Stdio)</h2>
          <button type="button" class="editor-btn editor-btn-secondary editor-btn--xs" id="copy-mcp"><i class="fa-solid fa-copy"></i><span>Copy</span></button>
        </div>
        <pre class="ai-pre" id="mcp-snippet"><?= $h((string)json_encode([
            'mcpServers' => [
                'mrpg-editor-studio' => [
                    'url' => $baseUrl . '/api/mcp.php',
                    'headers' => ['Authorization' => 'Bearer esai_<YOUR_TOKEN>'],
                    'stdioFallback' => [
                        'command' => 'node',
                        'args' => ['tools/mcp-stdio.mjs', '--url', $baseUrl],
                        'env' => ['EDITOR_STUDIO_TOKEN' => 'esai_<YOUR_TOKEN>'],
                    ],
                ],
            ],
        ], JSON_UNESCAPED_SLASHES | JSON_PRETTY_PRINT)) ?></pre>
      </section>
    </div>

    <section class="ai-card mt-4">
      <h2 class="font-bold text-base mb-2"><i class="fa-solid fa-key text-accent mr-1.5"></i> Interactive Token &amp; Capabilities Check</h2>
      <p class="text-xs editor-muted-text mb-3">Paste a guest-scoped Bearer AI token (starts with <code>esai_</code>) to verify its active scopes, tabs, and live manifest. The token stays strictly in page memory and is never stored.</p>
      <div class="flex flex-wrap gap-2">
        <input id="ai-token-input" type="password" class="editor-input form-input flex-1 min-w-[240px]" placeholder="esai_..." autocomplete="off" />
        <button type="button" class="editor-btn editor-btn-primary" id="ai-test-btn"><i class="fa-solid fa-circle-check"></i><span>Test Connection</span></button>
      </div>
      <pre class="ai-pre mt-3 hidden" id="ai-test-out"></pre>
    </section>
  </main>

  <!-- Machine-readable JSON manifest embedded for HTML-only web fetchers -->
  <script type="application/json" id="ai-discovery-manifest"><?= str_replace('</', '<\/', $docJson) ?></script>

  <script>
  (() => {
    const copyText = async (text) => {
      try {
        await navigator.clipboard.writeText(text);
        return true;
      } catch {
        return false;
      }
    };
    document.getElementById('copy-prompt')?.addEventListener('click', () => {
      copyText(document.getElementById('prompt-snippet')?.textContent || '');
    });
    document.getElementById('copy-mcp')?.addEventListener('click', () => {
      copyText(document.getElementById('mcp-snippet')?.textContent || '');
    });
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
