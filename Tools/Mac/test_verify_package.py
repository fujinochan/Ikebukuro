"""Offline staging-check regression tests, using project-local temporary files."""
import json
import plistlib
import shutil
import tempfile
import unittest
from pathlib import Path

from verify_package import ROOT, verify


class StagingTests(unittest.TestCase):
    def setUp(self):
        base = ROOT / 'Saved/Validation'
        base.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='mac-staging-test-', dir=base)
        self.package = Path(self.temp.name)
        self.app = self.package / 'IkebukuroNinjaCity.app'
        contents = self.app / 'Contents'
        (contents / 'MacOS').mkdir(parents=True)
        (contents / 'Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'Game'}))
        self.executable = contents / 'MacOS/Game'
        self.executable.write_text('test fixture, not a real executable')
        self.executable.chmod(0o755)
        manifest = json.loads((ROOT / 'doc/project-runtime-files.json').read_text())
        self.runtime = []
        for row in manifest['files']:
            dest = self.package / 'IkebukuroNinjaCity' / row['path']
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / row['path'], dest)
            self.runtime.append(dest)
        shutil.copytree(ROOT / 'Content/Distribution',
                        self.package / 'IkebukuroNinjaCity/Content/Distribution')
        (self.package / 'game.utoc').touch()
        (self.package / 'game.ucas').touch()

    def tearDown(self):
        self.temp.cleanup()

    def test_complete_staging(self):
        self.assertEqual(verify(self.package)['runtime_files_verified'], len(self.runtime))

    def test_bundled_content(self):
        data = self.package / 'IkebukuroNinjaCity'
        shutil.move(str(data), self.app / 'Contents/UE')
        self.assertEqual(verify(self.package)['runtime_files_verified'], len(self.runtime))

    def test_corrupt_city_data(self):
        self.runtime[0].write_bytes(b'corrupt')
        with self.assertRaisesRegex(ValueError, 'altered staged'):
            verify(self.package)

    def test_lost_executable_permission(self):
        self.executable.chmod(0o644)
        with self.assertRaisesRegex(ValueError, 'executable permission'):
            verify(self.package)

    def test_missing_license(self):
        license_file = next((self.package / 'IkebukuroNinjaCity/Content/Distribution/Licenses').rglob('*'))
        license_file.unlink()
        with self.assertRaisesRegex(ValueError, 'attribution/instructions'):
            verify(self.package)

    def test_missing_iostore(self):
        (self.package / 'game.ucas').unlink()
        with self.assertRaisesRegex(ValueError, 'IoStore'):
            verify(self.package)


if __name__ == '__main__':
    unittest.main()
