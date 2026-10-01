"""Save hashes of the current source candidates; never turn an old build into new acceptance."""
import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--head', required=True)
    parser.add_argument('--branch', required=True)
    args = parser.parse_args()
    files = {}
    groups = ('src', 'examples', 'tools', 'tests/Core_Boundaries')
    suffixes = {'.h', '.cpp', '.ino', '.py', '.js', '.html', '.json'}
    for group in groups:
        for source in (ROOT / group).rglob('*'):
            if not source.is_file() or source.suffix not in suffixes or '__pycache__' in source.parts:
                continue
            if source.name in {'arduino_secrets.h', 'mqtt_creds.json'}:
                continue
            files[source.relative_to(ROOT).as_posix()] = hashlib.sha256(source.read_bytes()).hexdigest()
    # These settings/runners decide how the candidate will be checked later. Capture them too;
    # matching C++ alone cannot identify a changed fixture include path or formatter policy.
    support_files = ('tests/verify_core_boundaries.ps1', 'tests/verify_source_preflight.ps1',
                     'tests/data_safety_regression.cpp', 'tests/verify_data_safety.ps1',
                     '.github/workflows/core-boundaries.yml', '.github/workflows/githubci.yml',
                     '.clang-format', 'library.properties')
    for relative in support_files:
        files[relative] = hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
    result = {'saved_utc': datetime.now(timezone.utc).isoformat(), 'board_url':
              'https://github.com/orgs/OPEnSLab-OSU/projects/30', 'branch': args.branch,
              'head': args.head, 'candidate_compiled': False,
              'status': 'Source/mock checks only; new C++ tests and firmware are uncompiled.',
              'files_sha256': dict(sorted(files.items()))}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
