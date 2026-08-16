const { spawnSync, execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

const HOME = os.homedir();
const NEXTVIPER_HOME = process.env.NEXTVIPER_HOME || path.join(HOME, '.nextviper');
const BIN_PATH = path.join(NEXTVIPER_HOME, 'bin', 'nextviper');

function getBinaryPath() {
  if (process.env.NEXTVIPER_BIN && fs.existsSync(process.env.NEXTVIPER_BIN)) {
    return process.env.NEXTVIPER_BIN;
  }
  if (fs.existsSync(BIN_PATH)) {
    return BIN_PATH;
  }
  try {
    const which = execSync('which nextviper 2>/dev/null', { encoding: 'utf-8' }).trim();
    if (which && fs.existsSync(which)) return which;
  } catch (e) {}
  return null;
}

function run(filePath, args = [], options = {}) {
  const bin = getBinaryPath();
  if (!bin) throw new Error("NextViper binary not found. Run 'npx nextviper' to initialize toolchain.");
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
  try {
    return JSON.parse(res.stdout || '{}');
  } catch (e) {
    return { raw: res.stdout, error: res.stderr };
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
