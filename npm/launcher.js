'use strict';
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');

function executable(candidate) {
  try {
    const real = fs.realpathSync(candidate);
    // npm's executable links can point back to this launcher. Never recurse.
    if (real.startsWith(path.join(__dirname, 'bin') + path.sep)) return null;
    fs.accessSync(real, fs.constants.X_OK);
    return fs.statSync(real).isFile() ? real : null;
  } catch { return null; }
}

function resolveBinary(name = 'nextviper') {
  const explicit = process.env[name === 'nextviper' ? 'NEXTVIPER_BIN' : 'NEXTVIPER_LSP_BIN'];
  if (explicit) return executable(explicit);
  const candidates = [];
  if (name === 'nextviper-lsp' && process.env.NEXTVIPER_BIN)
    candidates.push(path.join(path.dirname(process.env.NEXTVIPER_BIN), name));
  const toolchainHome = process.env.NEXTVIPER_HOME || path.join(os.homedir(), '.nextviper');
  candidates.push(path.join(toolchainHome, 'bin', name));
  for (const directory of (process.env.PATH || '').split(path.delimiter))
    if (directory) candidates.push(path.join(directory, name));
  for (const candidate of candidates) {
    const found = executable(candidate);
    if (found) return found;
  }
  return null;
}

function launch(name) {
  const binary = resolveBinary(name);
  if (!binary) {
    console.error(`${name}: native executable not found. Install the Linux toolchain from https://github.com/nuratix/nextviper and set NEXTVIPER_BIN or NEXTVIPER_LSP_BIN to its executable.`);
    process.exitCode = 1;
    return;
  }
  const result = spawnSync(binary, process.argv.slice(2), { stdio: 'inherit', shell: false });
  if (result.error) console.error(`${name}: ${result.error.message}`);
  process.exitCode = result.status ?? (result.signal ? 128 + (os.constants.signals[result.signal] || 0) : 1);
}

module.exports = { resolveBinary, launch };
