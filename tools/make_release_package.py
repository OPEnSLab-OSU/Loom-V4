"""Make a reviewable ZIP without deleting Git history or changing installed libraries.

Pass explicit library roots (Loom, TinyGSM, etc.). The ZIP excludes development/generated files,
credentials and nested Git metadata. No builds, downloads or publishing take place.
"""
import argparse
import hashlib
import json
import zipfile
from datetime import datetime, timezone
from pathlib import Path

EXCLUDED_DIRECTORIES = {'.git', '.codex', '.agents', '__pycache__', 'node_modules', 'build', '.pio'}
GENERATED_PREFIXES = ('loom_compile_audit_', 'sketch_compile_', 'release_package_')
CREDENTIAL_FILES = {'arduino_secrets.h', 'mqtt_creds.json', 'wifi_creds.json', 'lte_creds.json', '.env'}
GENERATED_SUFFIXES = {'.exe', '.obj', '.o', '.elf', '.bin', '.hex', '.pyc', '.su'}


def include(path):
    return (not any(part.lower() in EXCLUDED_DIRECTORIES or part.lower().startswith(GENERATED_PREFIXES)
                    for part in path.parts)
            and path.name.lower() not in CREDENTIAL_FILES
            and path.suffix.lower() not in GENERATED_SUFFIXES)


def make_package(roots, output):
    output = Path(output).resolve()
    roots = [Path(root).resolve() for root in roots]
    if len({root.name.casefold() for root in roots}) != len(roots):
        raise ValueError('library roots need distinct names')
    if output.exists():
        raise FileExistsError('refusing to overwrite an existing review artifact')
    manifest = {'created_utc': datetime.now(timezone.utc).isoformat(),
                'compiled': False, 'files': {}, 'excluded': []}
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for root in roots:
            if not root.is_dir():
                raise NotADirectoryError(root)
            for source in sorted(root.rglob('*')):
                if not source.is_file():
                    continue
                relative = source.relative_to(root)
                name = root.name + '/' + relative.as_posix()
                # Do not follow symlinks out of a checkout, nor include the ZIP being produced.
                if source.is_symlink() or not source.resolve().is_relative_to(root) or source.resolve() == output:
                    manifest['excluded'].append(name)
                    continue
                if not include(relative):
                    manifest['excluded'].append(name)
                    continue
                data = source.read_bytes()
                archive.writestr(name, data)
                manifest['files'][name] = hashlib.sha256(data).hexdigest()
        archive.writestr('PACKAGE_MANIFEST.json', json.dumps(manifest, indent=2))
    # Validate the delivered bytes against their hashes before reporting a successful package.
    with zipfile.ZipFile(output) as archive:
        for name, expected in manifest['files'].items():
            if hashlib.sha256(archive.read(name)).hexdigest() != expected:
                raise ValueError('packaged bytes do not match manifest: ' + name)
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library-root', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = make_package(args.library_root, args.output)
    print(json.dumps({'files': len(result['files']), 'excluded': len(result['excluded']),
                      'compiled': False, 'published': False}))
