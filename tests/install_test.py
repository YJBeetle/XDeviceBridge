"""Installer upgrade, rollback and preservation checks using disposable packages."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

INSTALLER = Path(__file__).resolve().parents[1] / 'tools/packaging/install.sh'

class InstallerTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='xdb-install-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.package = self.root / 'extracted'
        (self.package / 'bin').mkdir(parents=True)
        (self.package / 'lib').mkdir()
        shutil.copy2(INSTALLER, self.package / 'install.sh')
        executable = self.package / 'bin/xdb'
        executable.write_text('#!/bin/sh\nif [ "$1" = kill-server ]; then echo stop >> "$CALLS"; exit "${STOP_STATUS:-0}"; fi\n[ "$1" = --help ]\n')
        executable.chmod(0o755)
        (self.package / 'lib/new.dylib').write_text('new')
        self.prefix = self.root / 'installed'
        self.bindir = self.root / 'commands'
        self.calls = self.root / 'calls'
        self.env = dict(os.environ, HOME=str(self.root / 'home'), CALLS=str(self.calls))
        self.credentials = self.root / 'home/.local/share/XDeviceBridge/pairings'
        self.credentials.mkdir(parents=True)
        (self.credentials / 'record').write_text('fixture-record')

    def install(self, success=True, **changes):
        result = subprocess.run(['sh', str(self.package / 'install.sh'), str(self.prefix), str(self.bindir)],
                                env=dict(self.env, **changes), text=True, capture_output=True)
        self.assertEqual(result.returncode == 0, success, result.stderr)
        self.assertEqual((self.credentials / 'record').read_text(), 'fixture-record')
        return result

    def test_upgrade_stops_old_server_and_removes_stale_libraries(self):
        self.install()
        (self.prefix / 'lib/stale.dylib').write_text('old')
        self.install()
        self.assertFalse((self.prefix / 'lib/stale.dylib').exists())
        self.assertEqual(self.calls.read_text(), 'stop\n')
        self.assertEqual((self.bindir / 'xdb').resolve(), (self.prefix / 'bin/xdb').resolve())

    def test_refuse_unmanaged_install(self):
        self.prefix.mkdir()
        (self.prefix / 'keep').write_text('user')
        self.install(False)
        self.assertEqual((self.prefix / 'keep').read_text(), 'user')

    def test_preserve_other_command_before_modifying_install(self):
        self.install()
        (self.bindir / 'xdb').unlink()
        (self.bindir / 'xdb').symlink_to('/unrelated/xdb')
        self.install(False)
        self.assertFalse(self.calls.exists())
        self.assertTrue(((self.prefix / 'bin/xdb').resolve()).exists())

    def test_failed_stop_preserves_old_install(self):
        self.install()
        (self.prefix / 'lib/old').write_text('preserve')
        self.install(False, STOP_STATUS='1')
        self.assertEqual((self.prefix / 'lib/old').read_text(), 'preserve')
        self.assertFalse(Path(str(self.prefix) + '.install-lock').exists())

    def test_invalid_package_never_stops_old_server(self):
        self.install()
        (self.package / 'bin/xdb').write_text('#!/bin/sh\nexit 1\n')
        self.install(False)
        self.assertFalse(self.calls.exists())
        self.assertTrue(((self.prefix / 'bin/xdb').resolve()).is_file())

    def test_activation_failure_restores_previous_install(self):
        self.install()
        (self.prefix / 'lib/old').write_text('preserve')
        tools = self.root / 'tools'
        tools.mkdir()
        link = tools / 'ln'
        link.write_text('#!/bin/sh\nexit 1\n')
        link.chmod(0o755)
        self.install(False, PATH=str(tools) + ':' + os.environ['PATH'])
        self.assertEqual((self.prefix / 'lib/old').read_text(), 'preserve')
        self.assertEqual((self.bindir / 'xdb').resolve(), (self.prefix / 'bin/xdb').resolve())

if __name__ == '__main__':
    unittest.main()
