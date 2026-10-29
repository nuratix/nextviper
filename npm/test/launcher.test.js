'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '..');

test('launcher forwards literal arguments and propagates exit status', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'nv-launcher-'));
  try {
    const binary = path.join(directory, 'native executable');
    fs.writeFileSync(binary, `#!${process.execPath}\nconsole.log(JSON.stringify(process.argv.slice(2))); process.exit(7);\n`, { mode: 0o755 });
    const args = ['run', 'a file.nv', '$(echo literal)', '; literal'];
    const result = spawnSync(process.execPath, [path.join(root, 'bin/nextviper.js'), ...args], {
      encoding: 'utf8', env: { ...process.env, NEXTVIPER_BIN: binary }
    });
    assert.equal(result.status, 7, result.stderr);
    assert.deepEqual(JSON.parse(result.stdout), args);
  } finally { fs.rmSync(directory, { recursive: true, force: true }); }
});

test('explicit missing executable fails with useful installation instructions', () => {
  const result = spawnSync(process.execPath, [path.join(root, 'bin/nextviper.js'), '--version'], {
    encoding: 'utf8', env: { ...process.env, NEXTVIPER_BIN: path.join(root, 'missing-native') }
  });
  assert.equal(result.status, 1);
  assert.match(result.stderr, /Install the Linux toolchain/);
});

test('npm executable link cannot recursively launch itself', () => {
  const result = spawnSync(process.execPath, [path.join(root, 'bin/nextviper.js')], {
    encoding: 'utf8', timeout: 3000,
    env: { ...process.env, NEXTVIPER_BIN: path.join(root, 'bin/nextviper.js') }
  });
  assert.equal(result.status, 1);
  assert.match(result.stderr, /native executable not found/);
});
