const { spawnSync } = require('node:child_process');
const { resolveBinary } = require('./launcher');

function getBinaryPath() { return resolveBinary('nextviper'); }

function run(filePath, args = [], options = {}) {
  const bin = getBinaryPath();
  if (!bin) throw new Error("NextViper native binary not found. Install the toolchain and set NEXTVIPER_BIN.");
  return spawnSync(bin, ['run', filePath, ...args], {
    encoding: 'utf-8',
    ...options
  });
}

function check(filePath, options = {}) {
  const bin = getBinaryPath();
  if (!bin) throw new Error("NextViper binary not found.");
  const res = spawnSync(bin, ['check', filePath, '--format=json'], {
    encoding: 'utf-8',
    ...options
  });
  if (res.error) throw res.error;
  try {
    const diagnostics = JSON.parse(res.stdout || '[]');
    if (!Array.isArray(diagnostics)) throw new Error('Expected a diagnostic array');
    return { success: res.status === 0, diagnostics };
  } catch (error) {
    return { success: false, diagnostics: [], raw: res.stdout, error: res.stderr || error.message };
  }
}

function evalCode(code, options = {}) {
  const bin = getBinaryPath();
  if (!bin) throw new Error("NextViper binary not found.");
  return spawnSync(bin, ['eval', code], {
    encoding: 'utf-8',
    ...options
  });
}

module.exports = {
  version: "1.0.0",
  getBinaryPath,
  run,
  check,
  eval: evalCode
};
