# Inactive Loom SAMD21 core investigation snapshot

This directory preserves an experimental Feather M0 timeout patch for engineering review only.
It is not an Arduino library, release input, or approved board-core modification.

| Investigation snapshot | Official package file that must remain active |
|---|---|
| `cores/arduino/SERCOM.cpp` | `<platform>/cores/arduino/SERCOM.cpp` |
| `libraries/Wire/Wire.cpp` | `<platform>/libraries/Wire/Wire.cpp` |

The SERCOM patch rejects invalid bus ownership/state, checks hardware error flags, and caps address,
data, and command waits at 100 ms. The Wire patch caps requested-byte waits at 100 ms and reports
the number of bytes actually received.

The experiment was intended to make stalled transactions return failure instead of holding the
ATSAMD21G18A indefinitely. It is retained because the lower-level wait risk is relevant to hardware
fault-injection testing, not because this implementation has been accepted for release.

Do **not** copy these files into the platform. The beta must use the checksum-verified official
Loom 4.9 board-core files. `tests/verify_patched_dependencies.ps1` enforces the official hashes:

- `SERCOM.cpp`: `E0ABC9FAF850762A966D0C6A64A048CEC8B8A0C0E37290454FE97A840510ADB2`
- `Wire.cpp`: `4975914E951A003DE39EB3A6DA72E721B48F1156E890A0A3A2EF8A81C871592C`

Only Loom-owned code and reviewed third-party libraries are modified for this beta.
