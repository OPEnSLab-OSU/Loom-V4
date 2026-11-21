"""Make a reviewable ZIP without deleting Git history or changing installed libraries.

Pass explicit library roots for a source bundle, or an installed Loom SAMD platform
and any additional library roots for a Boards Manager archive. The input stays in place.
"""
import argparse
import hashlib
import json
import os
import subprocess
import zipfile
from pathlib import Path

EXCLUDED_DIRECTORIES = {
    '.git', '.codex', '.agents', '.local', '.secrets', '.vscode', '__pycache__',
    'node_modules', 'build', '.pio', '.loom-build',
}
EXCLUDED_SUBTREES = {
    ('tools', 'trace-studio'), ('tools', 'trace'), ('auxilary', 'node-red-config'),
}
GENERATED_PREFIXES = ('loom_compile_audit_', 'sketch_compile_', 'release_package_')
CREDENTIAL_FILES = {'arduino_secrets.h', 'mqtt_creds.json', 'wifi_creds.json', 'lte_creds.json', '.env'}
GENERATED_SUFFIXES = {'.exe', '.obj', '.o', '.elf', '.bin', '.hex', '.pyc', '.su'}
LOCAL_FILES = {'installed.json', 'platform.local.txt', 'boards.local.txt', 'package_manifest.json'}
LOOM_DEVELOPMENT = {'.github', 'auxilary', 'dependencies', 'docs', 'tests', 'tools'}
PLATFORM_ASSETS = {'bootloaders', 'drivers'}
SOURCE_MANIFEST = 'PACKAGE_MANIFEST.json'
PLATFORM_MANIFEST = 'loom4/' + SOURCE_MANIFEST
REQUIRED_LIBRARIES = {'loom': 'Loom', 'arduinomqttclient': 'ArduinoMqttClient'}
PLATFORM_REQUIRED = {
    'loom4/boards.txt', 'loom4/platform.txt',
    'loom4/libraries/Loom/src/Loom_Manager.h',
    'loom4/libraries/ArduinoMqttClient/src/ArduinoMqttClient.h',
}


def include(path, omit=(), platform=False):
    parts = tuple(part.casefold() for part in path.parts)
    return (parts[0] not in omit and parts[:2] not in EXCLUDED_SUBTREES
            and not any(part in EXCLUDED_DIRECTORIES or part.startswith(GENERATED_PREFIXES)
                        for part in parts)
            and parts[-1] not in CREDENTIAL_FILES | LOCAL_FILES
            and (path.suffix.casefold() not in GENERATED_SUFFIXES
                 or platform and parts[0] in PLATFORM_ASSETS))


def package_candidates(root, omit=(), platform=False):
    metadata = root / '.git'
    if metadata.is_file() or (metadata / 'HEAD').is_file():
        # Package this working tree, including new source, and never descend into ignored
        # workspaces. A dependency supplied as a plain directory uses the filter below.
        listed = subprocess.check_output(
            ['git', '-C', str(root), 'ls-files', '--cached', '--others', '--exclude-standard', '-z']
        )
        for name in sorted(set(listed.split(b'\0')) - {b''}):
            yield root / os.fsdecode(name)
        return

    for folder, directories, files in os.walk(root):
        base = Path(folder)
        directories[:] = sorted(
            name for name in directories
            if include((base / name).relative_to(root), omit, platform)
            and (base / name).resolve() == base / name
        )
        for name in sorted(files):
            yield base / name


def platform_groups(platform, extra_roots):
    if not all((platform / name).is_file() for name in ('boards.txt', 'platform.txt')):
        raise ValueError('platform root must contain boards.txt and platform.txt')
    library_folder = platform / 'libraries'
    if not library_folder.is_dir() or library_folder.resolve() != library_folder:
        raise ValueError('platform root must contain its own libraries directory')
    libraries = {folder.name.casefold(): folder for folder in library_folder.iterdir()
                 if folder.is_dir() and folder.resolve() == folder}
    libraries.update((root.name.casefold(), root) for root in extra_roots)
    loom = libraries.get('loom')
    if loom is not None:
        trace_heap = loom / 'dependencies' / 'Loom_TraceHeap'
        if (trace_heap / 'library.properties').is_file():
            libraries.setdefault('loom_traceheap', trace_heap)
    for key, required in REQUIRED_LIBRARIES.items():
        library = libraries.get(key)
        if library is None or not (library / 'library.properties').is_file():
            raise ValueError(f'platform archive requires the {required} library')
    yield platform, 'loom4', {'libraries'}, True
    for name, root in sorted(libraries.items()):
        if include(Path(root.name)):
            omit = LOOM_DEVELOPMENT if name == 'loom' else set()
            yield root, 'loom4/libraries/' + REQUIRED_LIBRARIES.get(name, root.name), omit, False


def write_file(archive, name, data):
    # A fixed timestamp and file mode keep a rebuild independent of the host clock.
    entry = zipfile.ZipInfo(name)
    entry.create_system = 3
    entry.external_attr = 0o100644 << 16
    archive.writestr(entry, data, compress_type=zipfile.ZIP_DEFLATED)


def verify_package(path):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len({name.casefold() for name in names}) != len(names):
            raise ValueError('archive contains duplicate paths')
        for name in names:
            if '\\' in name or any(part in {'', '.', '..'} or ':' in part
                                   for part in name.split('/')):
                raise ValueError('invalid archive path: ' + name)
        paths = set(names)
        manifests = paths & {SOURCE_MANIFEST, PLATFORM_MANIFEST}
        if len(manifests) != 1:
            raise ValueError('archive must contain exactly one package manifest')
        manifest_path = manifests.pop()
        if manifest_path == PLATFORM_MANIFEST:
            if any(not name.startswith('loom4/') for name in names):
                raise ValueError('platform archive must have a single loom4 root')
            missing = PLATFORM_REQUIRED - paths
            if missing:
                raise ValueError('platform archive is missing: ' + ', '.join(sorted(missing)))
        manifest = json.loads(archive.read(manifest_path))
        files = manifest.get('files') if isinstance(manifest, dict) else None
        if not isinstance(files, dict) or files.keys() != paths - {manifest_path}:
            raise ValueError('archive paths do not match the package manifest')
        for name, expected in files.items():
            if hashlib.sha256(archive.read(name)).hexdigest() != expected:
                raise ValueError('packaged bytes do not match manifest: ' + name)
    return manifest


def make_package(roots, output, platform=None):
    output = Path(output).resolve()
    roots = sorted((Path(root).resolve() for root in roots), key=lambda root: root.name.casefold())
    if len({root.name.casefold() for root in roots}) != len(roots):
        raise ValueError('library roots need distinct names')
    if not roots and platform is None:
        raise ValueError('at least one library root is required')
    for root in roots:
        if not root.is_dir():
            raise NotADirectoryError(root)
    if platform is not None:
        groups = list(platform_groups(Path(platform).resolve(), roots))
        manifest_path = PLATFORM_MANIFEST
    else:
        groups = [(root, root.name, set(), False) for root in roots]
        manifest_path = SOURCE_MANIFEST
    manifest = {'compiled': False, 'files': {}, 'excluded': []}
    created = False
    try:
        with output.open('xb') as destination:
            created = True
            with zipfile.ZipFile(destination, 'w') as archive:
                for root, prefix, omit, is_platform in groups:
                    for source in package_candidates(root, omit, is_platform):
                        relative = source.relative_to(root)
                        name = prefix + '/' + relative.as_posix()
                        if source == output:
                            continue
                        if not include(relative, omit, is_platform) or source.is_symlink():
                            manifest['excluded'].append(name)
                            continue
                        resolved = source.resolve()
                        if not resolved.is_relative_to(root) or resolved == output:
                            manifest['excluded'].append(name)
                            continue
                        if not source.is_file():
                            continue  # A tracked file may have been removed in the working tree.
                        data = source.read_bytes()
                        write_file(archive, name, data)
                        manifest['files'][name] = hashlib.sha256(data).hexdigest()
                write_file(archive, manifest_path, json.dumps(manifest, indent=2, sort_keys=True))
        # Verify the delivered bytes before reporting success.
        verify_package(output)
    except BaseException:
        if created:
            output.unlink(missing_ok=True)
        raise
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform-root', type=Path)
    parser.add_argument('--library-root', type=Path, action='append', default=[])
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--output', type=Path)
    action.add_argument('--verify', type=Path, metavar='ZIP')
    args = parser.parse_args()
    if args.verify:
        if args.library_root or args.platform_root is not None:
            parser.error('--verify takes an existing archive without source roots')
        result = verify_package(args.verify)
        checksum = hashlib.sha256()
        with args.verify.open('rb') as archive:
            for block in iter(lambda: archive.read(1024 * 1024), b''):
                checksum.update(block)
        print(json.dumps({'files': len(result['files']), 'size': args.verify.stat().st_size,
                          'checksum': 'SHA-256:' + checksum.hexdigest()}))
        raise SystemExit(0)
    if not args.library_root and args.platform_root is None:
        parser.error('provide --platform-root or --library-root')
    result = make_package(args.library_root, args.output, args.platform_root)
    print(json.dumps({'files': len(result['files']), 'excluded': len(result['excluded']),
                      'compiled': False, 'published': False}))
