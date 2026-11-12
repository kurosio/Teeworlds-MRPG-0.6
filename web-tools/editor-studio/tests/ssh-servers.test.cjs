const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const page = fs.readFileSync(path.join(root, 'ssh-servers.html'), 'utf8');
const api = fs.readFileSync(path.join(root, 'api/ssh-servers.php'), 'utf8');
const manifest = fs.readFileSync(path.join(root, 'studio.manifest.js'), 'utf8');
const russian = fs.readFileSync(path.join(root, 'localization/lang_ru.js'), 'utf8');

test('SSH fleet page exposes bulk server controls and per-server connection/command configuration', () => {
  for (const action of ['update', 'start', 'stop']) {
    assert.match(page, new RegExp(`data-action="${action}"`));
    assert.match(api, new RegExp(`'${action}'`));
  }
  for (const field of ['f-host', 'f-port', 'f-user', 'f-key', 'f-directory', 'f-update', 'f-start', 'f-stop']) {
    assert.match(page, new RegExp(`id="${field}"`));
  }
  assert.match(page, /data-editor="ssh-servers"/);
  assert.match(manifest, /key: 'ssh_servers'.*adminOnly: true/);
});

test('SSH API is admin-only, protects writes and uses safe SSH process args', () => {
  assert.match(api, /bootstrap_editor_api\(\)/);
  assert.match(api, /require_same_origin_json_post\(\)/);
  assert.match(api, /proc_open\(\$args/);
  assert.doesNotMatch(api, /shell_exec\s*\(|exec\s*\(|system\s*\(/);
  assert.match(api, /\$directory\[0\] !== '\/'/);
});

test('relay installation is not available from the SSH editor', () => {
  assert.doesNotMatch(page, /relay/i);
  assert.doesNotMatch(api, /relay|install-relay/i);
  assert.doesNotMatch(manifest, /relay deployment/i);
});

test('SSH fleet strings are localized in Russian', () => {
  for (const text of ['SSH servers', 'Manage selected servers', 'Game server commands', 'Private key path on the web server']) {
    assert.ok(russian.includes(`"${text}"`), `missing ru translation for ${text}`);
  }
});
