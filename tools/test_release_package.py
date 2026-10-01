import tempfile
import unittest
import zipfile
from pathlib import Path
from make_release_package import make_package


class PackageTests(unittest.TestCase):
    def test_heap_link_archive_is_preserved_and_staged_sketches_are_excluded(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'Loom_TraceHeap'
            archive_path = root / 'src/cortex-m0plus/libLoomTraceHeapLink.a'
            archive_path.parent.mkdir(parents=True)
            archive_path.write_bytes(b'!<arch>\n')
            staged = root / '.loom-build/stage/LoomLauncherBuild.ino'
            staged.parent.mkdir(parents=True)
            staged.write_text('temporary sketch', encoding='utf-8')
            output = Path(folder) / 'heap.zip'
            manifest = make_package([root], output)
            self.assertEqual(set(manifest['files']), {'Loom_TraceHeap/src/cortex-m0plus/libLoomTraceHeapLink.a'})
            with zipfile.ZipFile(output) as package:
                self.assertEqual(package.read('Loom_TraceHeap/src/cortex-m0plus/libLoomTraceHeapLink.a'), b'!<arch>\n')

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


if __name__ == '__main__':
    unittest.main()
