"""Fault tests for audit-product reuse; no compiler or installed SDK is modified."""
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

import verified_library_cache as cache


class VerifiedCacheTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="loom-cache-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.cache = self.root / "verified"
        self.headers = self.root / "headers"
        self.other_headers = self.root / "other-headers"
        self.compiler = self.root / "tool/bin/compiler.exe"
        for folder in (self.build / "libraries/test", self.headers, self.other_headers, self.compiler.parent):
            folder.mkdir(parents=True)
        self.compiler.write_bytes(b"fake-compiler-identity")
        self.source = self.headers / "reader.cpp"
        self.header = self.headers / "reader.h"
        self.source.write_text("source", encoding="utf-8")
        self.header.write_text("header", encoding="utf-8")
        self.object = self.build / "libraries/test/reader.cpp.o"
        self.object.write_bytes(b"compiled-product")
        self.depfile = self.object.with_suffix(".d")
        self.depfile.write_text(f"{self.object}: {self.source} \\\n {self.header}\n", encoding="utf-8")
        self.options = {"hardwareFolders": str(self.root), "fqbn": "test:board", "customBuildProperties": "unchanged"}
        self.write_json(self.build / "build.options.json", self.options)
        self.write_json(self.build / "compile_commands.json", [{"arguments": [str(self.compiler)]}])
        self.set_include_order([self.headers, self.other_headers])

    def write_json(self, path, value):
        path.write_text(json.dumps(value), encoding="utf-8")

    def set_include_order(self, folders):
        self.write_json(self.build / "includes.cache", [{"added_include_path": str(p)} for p in folders])

    def save_and_remove(self):
        with contextlib.redirect_stdout(io.StringIO()):
            cache.save(self.build, self.cache)
        self.object.unlink()
        self.depfile.unlink()

    def restore(self):
        with contextlib.redirect_stdout(io.StringIO()):
            cache.restore(self.build, self.cache)

    def test_safe_order_change_restores_exact_bytes(self):
        self.save_and_remove()
        self.set_include_order([self.other_headers, self.headers])
        self.restore()
        self.assertEqual(self.object.read_bytes(), b"compiled-product")

    def test_changed_source_header_compiler_or_board_is_not_reused(self):
        self.save_and_remove()
        for path in (self.source, self.header, self.compiler):
            original = path.read_bytes()
            path.write_bytes(original + b"changed")
            self.restore()
            self.assertFalse(self.object.exists())
            path.write_bytes(original)
        self.write_json(self.build / "build.options.json", {**self.options, "fqbn": "other:board"})
        self.restore()
        self.assertFalse(self.object.exists())

    def test_header_shadowing_blocks_reuse(self):
        (self.other_headers / "reader.h").write_text("different", encoding="utf-8")
        self.save_and_remove()
        self.set_include_order([self.other_headers, self.headers])
        self.restore()
        self.assertFalse(self.object.exists())

    def test_missing_dependency_blocks_reuse(self):
        self.save_and_remove()
        self.header.unlink()
        self.restore()
        self.assertFalse(self.object.exists())

    def test_corrupt_product_is_rejected(self):
        self.save_and_remove()
        (self.cache / "objects/libraries/test/reader.cpp.o").write_bytes(b"corrupt")
        with self.assertRaises(ValueError):
            self.restore()
        self.assertFalse(self.object.exists())

    def test_manifest_cannot_write_outside_libraries(self):
        self.save_and_remove()
        manifest_path = self.cache / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["objects"][0]["path"] = "../outside.o"
        self.write_json(manifest_path, manifest)
        with self.assertRaises(ValueError):
            self.restore()
        self.assertFalse((self.build.parent / "outside.o").exists())

    def test_escaped_space_dependencies(self):
        header = self.headers / "header with spaces.h"
        header.write_text("input", encoding="utf-8")
        escaped = str(header).replace(" ", "\\ ")
        self.depfile.write_text(f"{self.object}: {escaped}\n", encoding="utf-8")
        self.assertEqual(cache.dependencies(self.depfile), [header])


if __name__ == "__main__":
    unittest.main()
