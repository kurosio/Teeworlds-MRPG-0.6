// Runs the PHP parts of the studio in a wasm PHP runtime.
//
//   node tests/php-harness.mjs            # syntax check of every api/*.php + helper tests
//
// The browser studio is checked by node:test; the server keeps its value rules
// (NULL / '' / 0 comparison) in plain PHP functions, so they are exercised here
// instead of being reviewed by eye. Requires the optional `php-wasm` package:
//   npm install --no-save php-wasm
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

let PhpNode;
try {
  ({ PhpNode } = await import('php-wasm/PhpNode.mjs'));
} catch {
  console.log('skip: php-wasm is not installed (npm install --no-save php-wasm)');
  process.exit(0);
}

const php = new PhpNode();
const errors = [];

/** Runs PHP code and returns everything it printed. */
async function run(code) {
  let output = '';
  const listener = (event) => { output += event.detail.join(''); };
  php.addEventListener('output', listener);
  const exitCode = await php.run(code);
  php.removeEventListener('output', listener);
  return { output, exitCode };
}

// php-wasm has its own virtual file system: copy the sources into it once.
const VFS_ROOT = '/studio';
await php.mkdir('/studio');
for (const dir of ['api']) {
  const target = `${VFS_ROOT}/${dir}`;
  await php.mkdir(target);
  for (const name of fs.readdirSync(path.join(ROOT, dir))) {
    if (!name.endsWith('.php')) continue;
    await php.writeFile(`${target}/${name}`, fs.readFileSync(path.join(ROOT, dir, name), 'utf8'), { encoding: 'utf8' });
  }
}

async function syntaxCheck(file) {
  // token_get_all() compiles the source: a parse error shows up here.
  const { output } = await run(`<?php token_get_all(file_get_contents(${JSON.stringify(`${VFS_ROOT}/${file}`)})); echo 'ok';`);
  if (!output.includes('ok')) errors.push(`${file}: ${output.trim() || 'parse error'}`);
  return output.includes('ok');
}

const phpFiles = fs.readdirSync(path.join(ROOT, 'api'))
  .filter((name) => name.endsWith('.php'))
  .map((name) => `api/${name}`);

let checked = 0;
for (const file of phpFiles) {
  if (await syntaxCheck(file)) checked += 1;
}
console.log(`php syntax: ${checked}/${phpFiles.length} files`);

// ── Value comparison (NULL / '' / 0) ────────────────────────────────────────
const cases = [
  // a, b, json, expected — «нет значения» с обеих сторон считается равенством
  [null, '', false, true],
  [null, '0', false, true],
  [null, 0, false, true],
  ['', 0, false, true],
  ['', '0', false, true],
  [null, 'Material', false, false],
  [7, '7', false, true],
  [7, 0, false, false],
  ['7.00', 7, false, true],
  ['Text', 'text', false, false],
  [null, '[]', true, true],
  [null, [], true, true],
  ['{"a":1}', '{"a":1}', true, true],
  ['{"a":1}', '{"a":2}', true, false],
  ['{"a":1}', null, true, false],
];

const phpCaseList = JSON.stringify(cases);
const { output: valueOutput } = await run(`<?php
require '${VFS_ROOT}/api/db-resources.php';
$cases = json_decode(${JSON.stringify(phpCaseList)}, true);
$failed = 0;
foreach ($cases as [$a, $b, $isJson, $expected]) {
    $actual = editor_values_equal($a, $b, (bool)$isJson);
    if ($actual !== $expected) {
        $failed++;
        echo "FAIL editor_values_equal(" . var_export($a, true) . ', ' . var_export($b, true) . '): '
            . var_export($actual, true) . " expected " . var_export($expected, true) . "\\n";
    }
}
echo $failed === 0 ? "values: ok\\n" : "values: {$failed} failed\\n";
`);
if (!valueOutput.includes('values: ok')) errors.push(valueOutput.trim());
process.stdout.write(valueOutput);

// ── Resource / request helpers still load together ──────────────────────────
const { output: libOutput } = await run(`<?php
require '${VFS_ROOT}/api/change-requests-lib.php';
$checks = [
    cr_values_equal(null, '', false),
    cr_values_equal(null, 0, false),
    !cr_values_equal(5, 0, false),
    cr_values_equal('7', 7, false),
];
echo in_array(false, $checks, true) ? "cr_values_equal: FAIL\\n" : "cr_values_equal: ok\\n";
`);
if (!libOutput.includes('cr_values_equal: ok')) errors.push(libOutput.trim());
process.stdout.write(libOutput);

if (errors.length) {
  console.log(`\n${errors.length} problem(s):`);
  errors.forEach((line) => console.log(' - ' + line));
  process.exit(1);
}
console.log('php checks passed');
