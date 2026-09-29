# Embedded design and readability review — 2026-09-29

This pass applies the Loom [SAMD21 coding profile](SAMD21_CODING_PROFILE.md) and selected guidance
from Elecia White's *Making Embedded Systems*, second edition (2024). The supplied book was
indexed locally and the relevant architecture, error-handling, stack/heap, state-machine, and
watchdog sections were reviewed. The book is reference material, not project instructions.

## Design decisions reflected in the source

| Principle | Loom change | Preserved behavior |
| --- | --- | --- |
| Make module boundaries small and ownership visible | Logger RTC/SD implementations moved to `Logger.cpp`; headers use forward declarations where only a pointer/reference is needed | Serial output, RTC prefixes, SD logs, pretty JSON, function summaries, and logger switches |
| Name the data instead of its container position | ThingSpeak uses named field/callback records; networking fields describe the manager, credentials, batching, and power intent | Callback order, eight-field limit, topics, payload formatting, and configuration keys |
| Separate retry policy from one hardware attempt | WiFi uses one bounded connection loop; LTE has one data-session-attempt helper | Credential overload selection, attempt limits, delays, AP fallback, modem commands, and watchdog enable/disable sequence |
| Keep operational paths visible | 23 larger sensor operations use an early initialization guard; SEN55 modes are separate helpers; SEN66 rejects invalid samples before accumulation | Sensor call order, timeouts, averaging order, number-concentration accounting, JSON keys, and NaN rejection |
| Make resource cleanup easy to find | AS7265X bulb control is shared by normal and timeout exits; Max command dispatch has explicit target/instance checks | White/IR/UV order, timeout cleanup, command order, and Relay/Neopixel broadcast behavior |
| Avoid unnecessary dependencies and setup recursion | Removed unused map/functional/vector/algorithm includes; analog/digital/actuator argument lists use direct loops | Analog pin order and battery field; digital sorting/deduplication; actuator null filtering and order |

`FUNCTION_START` and `FUNCTION_END` remain the source spelling for debug summaries.
`FUNCTION_START` constructs the existing scoped instrumentor. Its destructor writes the exit
summary on every return path. `FUNCTION_END` marks the normal exit in source and expands to
nothing; it does not emit a duplicate exit. `INSTRUMENT()` remains a compatible alternative.

The presentation targets readers learning embedded programming. Paired slash dividers mark
function boundaries, and the core headers have plain-language section labels. Comments explain
block purpose, constraints, ownership, units, timing, protocol behavior, and failure handling.
Useful explanations and future-work notes are retained or updated when implementation changes.
See [Following one Loom measurement](HOW_LOOM_WORKS.md) for the cycle and memory/ownership model.
This follows the selected NASA/MIT-inspired profile; it is not a certification claim.

## Include and dependency boundaries

- `Module.h` no longer imports JSON or Wire on behalf of unrelated modules. JSON and I2C users
  include the appropriate interface directly.
- Mux auto-loader drivers live in the implementation file. Its public header retains the TSL2591
  types required by its existing option API. All auto-loading cases remain available.
- Batch definitions move into WiFi, LTE, MongoDB, and LoRa implementations where needed.
  Repository sketches constructing a batch object now include `Loom_BatchSD.h` explicitly.
- Logger memory summaries use the small `LoomMemory::freeMemoryBytes()` utility on ARM. It reports
  the stack-to-heap gap without allocating, matching the former metric. Non-ARM targets retain
  MemoryFree's platform-specific implementation. This is not a contiguous-allocation guarantee.
- Standard containers/algorithms and hardware/protocol drivers remain where they provide required
  behavior. A shorter include list does not prove a smaller linked binary; that needs measurement.

External sketches that relied on indirect includes should include the classes they construct and
the logger/core interfaces they use. Public module functions and serialized field names remain
available. Exact stack/flash costs and overload resolution still require compiler verification.

## Audit instrumentation

The compiler warning-scope header was already stripped when this work began. It is now restored
around the current external include groups, including new/moved includes. The historical restore
patch was archived under the local temporary folder `loom-warning-scope-reapply-20260929` before
its stale saved state was consumed. The existing strip/restore commands remain usable.

`tests/verify_warning_scope.ps1` is a source-only preflight: it checks the header, balanced scopes,
external coverage, and the absence of Loom code or standard/runtime headers inside scopes.
The compiler's existing Loom-versus-external report classification is retained. Warning scopes
remain temporary audit instrumentation with the release-removal procedure in `tests/WARNING_SCOPE.md`.

## Evidence and remaining work

Source-only checks passed for all four Wisp quiet/debug pairs, canonical diagnostic/logger
headers, and 72 warning-scope groups across 62 files. A strip/restore cycle on an isolated temporary
copy restored every source file byte-for-byte.

Saved-source comparisons checked the five moved logger implementations, both SEN55 mode bodies,
23 sensor guard bodies, SEN66 accumulation order and packaging, and LTE/RemoteManager/ThingSpeak
message literals. These are source comparisons, not execution tests. Helper extraction changes
the function name shown in some diagnostic prefixes, which now identifies the specific operation.

The final entry-point pass names JSON views explicitly, writes the board serial number directly
into its existing member buffer instead of using a second 33-byte local array, and flattens mux
probe failures before sensor loading. The serial format, register read order, mux discovery order,
and failed-driver deletion behavior are retained. Restoring dividers around 482 definitions in
56 implementation files changed no code tokens; actual stack-frame savings still need a build.
The completed visual-cue/comment/formatting pass preserved code tokens in all 133 source files.
The combined source preflight passed warning scopes, all four quiet/debug pairs, diagnostic
boundaries, installed dependency/official-core hashes, and formatting across those 133 files.

No compiler was run during these additional passes. Next validation, once authorized: exact-board
quiet/debug builds, overload/API smoke cases, linked RAM/flash and stack-frame comparisons,
golden payloads, and hardware fault/standby/reconnect tests. Existing blocking driver calls and
interactive UART drains still need bounds/watchdog review; this pass does not claim every path
meets the complete coding profile.

`tests/verify_source_preflight.ps1` now combines warning-scope checks, quiet/debug parity,
diagnostic boundaries, dependency/core hashes, and optional read-only formatting checks.
It is a manually runnable entry point, ready for a future CI/presubmit job. Keep compiler audits
separate and compare their new warnings and memory/stack growth against an approved baseline.
