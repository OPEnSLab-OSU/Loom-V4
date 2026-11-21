import json
import os
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch
from make_release_package import make_package, verify_package


class PackageTests(unittest.TestCase):
    def test_failed_package_removes_partial_output(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'Loom'
            root.mkdir()
            output = Path(folder) / 'review.zip'
            with patch('make_release_package.package_candidates', side_effect=OSError('read failed')):
                with self.assertRaises(OSError):
                    make_package([root], output)
            self.assertFalse(output.exists())

    def test_checkout_packages_current_and_new_source_without_ignored_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'Loom'
            root.mkdir()
            subprocess.run(['git', 'init', '--quiet', str(root)], check=True)
            (root / '.gitignore').write_text('local-probe/\n', encoding='utf-8')
            fixture = {
                'src/live.cpp': 'current source',
                'src/added.cpp': 'new source',
                'src/removed.cpp': 'old source',
                'local-probe/staged.ino': 'local sketch',
                'tools/feature-studio/.local/config.json': 'local settings',
                'tools/trace-studio/dist/index.html': 'retired build',
                'auxilary/Node-Red-Config/.node-red/flows_cred.json': 'private runtime state',
            }
            for name, contents in fixture.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(contents, encoding='utf-8')
            subprocess.run(['git', '-C', str(root), 'add', 'src/live.cpp', 'src/removed.cpp'], check=True)
            (root / 'src/removed.cpp').unlink()
            (root / 'src/live.cpp').write_text('edited source', encoding='utf-8')

            output = Path(folder) / 'release.zip'
            manifest = make_package([root], output)
            self.assertEqual(set(manifest['files']),
                             {'Loom/.gitignore', 'Loom/src/live.cpp', 'Loom/src/added.cpp'})
            with zipfile.ZipFile(output) as package:
                self.assertEqual(package.read('Loom/src/live.cpp'), b'edited source')
            self.assertTrue((root / 'local-probe/staged.ino').is_file())

    def test_static_library_archive_is_preserved_and_staged_sketches_are_excluded(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'FixtureLibrary'
            archive_path = root / 'src/cortex-m0plus/libFixture.a'
            archive_path.parent.mkdir(parents=True)
            archive_path.write_bytes(b'!<arch>\n')
            staged = root / '.loom-build/stage/LoomLauncherBuild.ino'
            staged.parent.mkdir(parents=True)
            staged.write_text('temporary sketch', encoding='utf-8')
            output = Path(folder) / 'package.zip'
            manifest = make_package([root], output)
            self.assertEqual(set(manifest['files']), {'FixtureLibrary/src/cortex-m0plus/libFixture.a'})
            with zipfile.ZipFile(output) as package:
                self.assertEqual(package.read('FixtureLibrary/src/cortex-m0plus/libFixture.a'), b'!<arch>\n')

    def test_metadata_credentials_and_products_are_excluded_without_deletion(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'TinyGSM'
            files = ('src/TinyGsmClient.h', '.git/objects/object', 'examples/arduino_secrets.h',
                     'examples/mqtt_creds.json', 'build/sketch.elf', 'README.md')
            for name in files:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name, encoding='utf-8')
            output = Path(folder) / 'review.zip'
            manifest = make_package([root], output)
            self.assertEqual(set(manifest['files']), {'TinyGSM/src/TinyGsmClient.h', 'TinyGSM/README.md'})
            self.assertTrue(all((root / name).is_file() for name in files))
            with zipfile.ZipFile(output) as archive:
                self.assertIsNone(archive.testzip())
                self.assertIn('PACKAGE_MANIFEST.json', archive.namelist())
            with self.assertRaises(FileExistsError):
                make_package([root], output)

    def test_platform_archive_keeps_bootloaders_and_has_one_root(self):
        with tempfile.TemporaryDirectory() as folder:
            platform = Path(folder) / '4.9'
            mqtt = Path(folder) / 'arduinomqttclient'
            mqtt.mkdir()
            (mqtt / 'library.properties').write_text('name=ArduinoMqttClient', encoding='utf-8')
            (mqtt / 'src').mkdir()
            (mqtt / 'src/ArduinoMqttClient.h').write_text('', encoding='utf-8')
            files = (
                'boards.txt', 'platform.txt', 'bootloaders/board.bin', 'drivers/driver.exe',
                'installed.json', 'boards.local.txt', 'cores/arduino/cached.o',
                'libraries/Loom/library.properties', 'libraries/Loom/src/Loom_Manager.h',
                'libraries/Loom/docs/index.html', 'libraries/Loom/tools/dev.py',
                'libraries/Loom/examples/Minimal/Minimal.ino',
                'libraries/RadioHead/RH_RF95.h',
                'libraries/Loom/dependencies/Loom_SAMD21_Core_Patches/Wire-timeout.patch',
                'libraries/Loom/dependencies/Loom_TraceHeap/library.properties',
                'libraries/Loom/dependencies/Loom_TraceHeap/src/Loom_TraceHeapLink.cpp',
                'libraries/Loom/dependencies/Loom_TraceHeap/src/cortex-m0plus/libLoomTraceHeapLink.a',
            )
            for name in files:
                path = platform / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name, encoding='utf-8')
            output = Path(folder) / 'loom4.zip'
            with self.assertRaisesRegex(ValueError, 'ArduinoMqttClient'):
                make_package([], output, platform)
            self.assertFalse(output.exists())

            manifest = make_package([mqtt], output, platform)
            self.assertEqual(set(manifest['files']), {
                'loom4/boards.txt', 'loom4/platform.txt', 'loom4/bootloaders/board.bin',
                'loom4/drivers/driver.exe', 'loom4/libraries/ArduinoMqttClient/library.properties',
                'loom4/libraries/ArduinoMqttClient/src/ArduinoMqttClient.h',
                'loom4/libraries/Loom/library.properties', 'loom4/libraries/Loom/src/Loom_Manager.h',
                'loom4/libraries/Loom/examples/Minimal/Minimal.ino',
                'loom4/libraries/RadioHead/RH_RF95.h',
                'loom4/libraries/Loom_TraceHeap/library.properties',
                'loom4/libraries/Loom_TraceHeap/src/Loom_TraceHeapLink.cpp',
                'loom4/libraries/Loom_TraceHeap/src/cortex-m0plus/libLoomTraceHeapLink.a',
            })
            with zipfile.ZipFile(output) as archive:
                self.assertTrue(all(name.startswith('loom4/') for name in archive.namelist()))
                self.assertIn('loom4/PACKAGE_MANIFEST.json', archive.namelist())
                self.assertIsNone(archive.testzip())
            self.assertTrue(all((platform / name).is_file() for name in files))

    def test_rebuild_matches_with_different_root_order_and_mtimes(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            roots = [base / 'Loom', base / 'TinyGSM']
            for root in roots:
                root.mkdir()
                (root / 'library.properties').write_text('name=' + root.name, encoding='utf-8')
            first, second = base / 'first.zip', base / 'second.zip'
            make_package(roots, first)
            for root in roots:
                os.utime(root / 'library.properties', (0, 0))
            make_package(reversed(roots), second)
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_verifier_rejects_changed_mismatched_or_duplicate_members(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            root = base / 'Loom'
            root.mkdir()
            (root / 'file.h').write_bytes(b'fixed bytes')
            original = base / 'original.zip'
            manifest = make_package([root], original)
            self.assertEqual(verify_package(original), manifest)
            for name, contents, expected in (
                    ('Loom/file.h', b'changed bytes', 'do not match manifest'),
                    ('Loom/extra.h', b'extra file', 'paths do not match'),
                    ('Loom/other.h', b'fixed bytes', 'paths do not match'),
                    ('Loom/../file.h', b'fixed bytes', 'invalid archive path'),
                    ('Loom/FILE.h', b'fixed bytes', 'duplicate paths')):
                modified = base / 'modified.zip'
                with zipfile.ZipFile(modified, 'w') as archive:
                    archive.writestr('PACKAGE_MANIFEST.json', json.dumps(manifest))
                    if name in {'Loom/extra.h', 'Loom/FILE.h'}:
                        archive.writestr('Loom/file.h', b'fixed bytes')
                    archive.writestr(name, contents)
                with self.subTest(name=name), self.assertRaisesRegex(ValueError, expected):
                    verify_package(modified)


if __name__ == '__main__':
    unittest.main()
