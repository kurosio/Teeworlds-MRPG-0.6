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

test('create-ssh-key.sh creates a passwordless OpenSSH keypair with strict permissions', (t) => {
  const { execFileSync, spawnSync } = require('node:child_process');
  const os = require('node:os');
  const script = path.join(root, 'tools/create-ssh-key.sh');
  assert.ok(fs.existsSync(script), 'tools/create-ssh-key.sh must exist');
  assert.ok((fs.statSync(script).mode & 0o111) !== 0, 'tools/create-ssh-key.sh must be executable');

  const helpOut = execFileSync(script, ['--help'], { encoding: 'utf8' });
  assert.match(helpOut, /Private key path on the web server|--docker|--copy-id/);

  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'editor-ssh-key-test-'));
  t.after(() => fs.rmSync(tmpDir, { recursive: true, force: true }));

  const out = execFileSync(script, ['--dir', tmpDir, '--name', 'test_srv', '--docker'], { encoding: 'utf8' });
  const priv = path.join(tmpDir, 'test_srv');
  const pub = `${priv}.pub`;
  assert.ok(fs.existsSync(priv) && fs.existsSync(pub));
  assert.equal(fs.statSync(priv).mode & 0o777, 0o600);
  assert.equal(fs.statSync(pub).mode & 0o777, 0o644);
  assert.match(out, /\/run\/editor-ssh-keys\/test_srv/);
  assert.match(fs.readFileSync(pub, 'utf8'), /^ssh-ed25519 /);

  // BatchMode=yes требует ключ без парольной фразы
  const derived = execFileSync('ssh-keygen', ['-y', '-P', '', '-f', priv], { encoding: 'utf8' }).trim();
  assert.ok(fs.readFileSync(pub, 'utf8').startsWith(derived));

  // Без --force перезапись запрещена
  const denied = spawnSync(script, ['--dir', tmpDir, '--name', 'test_srv'], { encoding: 'utf8' });
  assert.notEqual(denied.status, 0);
  assert.match(denied.stderr, /уже существует/);

  // С --force перезапись работает
  execFileSync(script, ['--dir', tmpDir, '--name', 'test_srv', '--force'], { encoding: 'utf8' });
  assert.equal(fs.statSync(priv).mode & 0o777, 0o600);
});
