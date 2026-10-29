#!/usr/bin/env python3
"""Install and run from an unrelated directory, including packaged artifacts."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get('NEXTVIPER_BIN', ROOT / 'bin/nextviper')).resolve()
LSP = Path(os.environ.get('NEXTVIPER_LSP_BIN', BIN.with_name('nextviper-lsp'))).resolve()
ARCHIVE = None
if len(sys.argv) >= 3 and sys.argv[1] == '--archive':
    ARCHIVE = Path(sys.argv[2]).resolve()
    del sys.argv[1:3]


class DistributionContract(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='nv-install-')
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)
        self.install = self.work / 'installed binaries'
        self.build = self.work / 'build'
        (self.build / 'bin').mkdir(parents=True)
        self.environment = dict(os.environ, NEXTVIPER_INSTALL_DIR=str(self.install),
                                NEXTVIPER_BUILD_DIR=str(self.build))
        self.installer = ROOT / 'install.sh'
        if ARCHIVE:
            checksum = ARCHIVE.with_name(ARCHIVE.name + '.sha256').read_text().split()[0]
            self.assertEqual(hashlib.sha256(ARCHIVE.read_bytes()).hexdigest(), checksum)
            with tarfile.open(ARCHIVE) as package:
                package.extractall(self.work / 'package', filter='data')
            roots = list((self.work / 'package').iterdir())
            self.assertEqual(len(roots), 1)
            self.installer = roots[0] / 'install.sh'
            self.assertTrue((roots[0] / 'examples/todo-api/app.nv').is_file())
            self.assertTrue((roots[0] / 'LICENSE').is_file())
            self.environment.pop('NEXTVIPER_BUILD_DIR')
        else:
            shutil.copy2(BIN, self.build / 'bin/nextviper')
            shutil.copy2(LSP, self.build / 'bin/nextviper-lsp')

    def test_install_run_native_build_language_server_and_uninstall(self):
        result = subprocess.run(['bash', str(self.installer)], cwd=self.work, env=self.environment,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        binary = self.install / 'nextviper'
        source = self.work / 'hello.nv'
        source.write_text('print("Installed NextViper works")\n')
        run = subprocess.run([str(binary), 'run', str(source)], cwd=self.work,
                             capture_output=True, text=True, timeout=10)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(run.stdout, 'Installed NextViper works\n')
        native = self.work / 'native app'
        built = subprocess.run([str(binary), 'build', str(source), '-o', str(native)], cwd=self.work,
                               capture_output=True, text=True, timeout=60)
        self.assertEqual(built.returncode, 0, built.stderr)
        self.assertEqual(subprocess.check_output([str(native)], text=True), run.stdout)
        initialize = json.dumps({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}})
        shutdown = json.dumps({'jsonrpc': '2.0', 'id': 2, 'method': 'shutdown', 'params': {}})
        exit_message = json.dumps({'jsonrpc': '2.0', 'method': 'exit', 'params': {}})
        stream = ''.join(f'Content-Length: {len(message)}\r\n\r\n{message}'
                         for message in (initialize, shutdown, exit_message))
        result = subprocess.run([str(self.install / 'nextviper-lsp')], input=stream,
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('capabilities', result.stdout)
        result = subprocess.run(['bash', str(self.installer), '--uninstall'], cwd=self.work,
                                env=self.environment, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(binary.exists())
        self.assertFalse((self.install / 'nextviper-lsp').exists())

    @unittest.skipIf(ARCHIVE, 'source-only missing build check')
    def test_missing_build_reports_failure(self):
        environment = dict(self.environment, NEXTVIPER_BUILD_DIR=str(self.work / 'missing'))
        result = subprocess.run(['bash', str(self.installer)], cwd=self.work,
                                env=environment, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertIn('Missing executable', result.stderr)
        self.assertFalse(self.install.exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
