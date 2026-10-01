"""Report Loom's source/include needs for known examples without compiling or removing drivers."""
import argparse
import hashlib
import json
import re
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILES = {
    'wisp': 'examples/Lab Examples/Wisp/Wisp_Batch_Logging/Wisp_Batch_Logging.ino',
    'smartrock': 'examples/Lab Examples/SmartRock/SmartRock_2026/SmartRock_2026.ino',
    'dendrometer-hub': 'examples/Lab Examples/Dendrometer/hub/hub.ino',
    'dendrometer-node': 'examples/Lab Examples/Dendrometer/node/node.ino',
    'weather-chimes': 'examples/Lab Examples/WeatherChimes/WeatherChimes_WithMax/WeatherChimes_WithMax.ino',
    'lora-heartbeat-only': 'examples/Radio/LoRa_Heartbeat_Only/LoRa_Heartbeat_Only.ino',
    'flash-health': 'examples/Diagnostics/Flash_Health_Checkpoint/Flash_Health_Checkpoint.ino',
}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
STANDARD = set('algorithm array assert.h cctype cerrno cfloat climits cmath cstddef cstdarg cstdint '
               'cstdio cstdlib cstring ctype.h errno.h float.h functional initializer_list inttypes.h '
               'limits limits.h malloc.h map math.h memory new stddef.h stdint.h stdio.h stdlib.h '
               'string string.h time.h tuple utility vector'.split())
CORE = set('Arduino.h Client.h IPAddress.h Print.h Stream.h WString.h Wire.h SPI.h '
           'avr/pgmspace.h sam.h'.split())


def inspect(entry):
    """Conservative text closure: both sides of conditional includes remain in the report."""
    entry = Path(entry).resolve()
    pending, visited, external = [entry], {}, set()
    while pending:
        path = pending.pop()
        if path in visited:
            continue
        if not path.is_file():
            raise FileNotFoundError(path)
        if not path.is_relative_to(ROOT):
            raise ValueError('entry/source closure must stay inside this Loom checkout')
        raw = path.read_bytes()
        visited[path] = hashlib.sha256(raw).hexdigest()
        for header in INCLUDE.findall(raw.decode('utf-8', errors='replace')):
            candidates = (path.parent / header, ROOT / 'src' / header)
            owned = next((p.resolve() for p in candidates if p.is_file()), None)
            if owned is not None:
                pending.append(owned)
                implementation = owned.with_suffix('.cpp')
                if implementation.is_file():
                    pending.append(implementation)
            elif header not in STANDARD:
                external.add(header)
    return {
        'entry': entry.relative_to(ROOT).as_posix(),
        'loom_sources_sha256': {p.relative_to(ROOT).as_posix(): digest
                                for p, digest in sorted(visited.items())},
        'core_headers': sorted(external & CORE),
        'vendor_headers': sorted(external - CORE),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=PROFILES, action='append')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = {'saved_utc': datetime.now(timezone.utc).isoformat(),
              'limitations': ['Text include closure, not compiler dependency discovery.',
                              'Conditional branches are included conservatively.',
                              'Arduino may compile every library translation unit; this is not a slim board package.',
                              'No supported driver or installed dependency is removed.'],
              'profiles': {name: inspect(ROOT / PROFILES[name]) for name in args.profile or PROFILES}}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
