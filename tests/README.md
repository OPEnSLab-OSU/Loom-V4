# Loom compile tests

## Source-only preflight

These checks do not invoke compilers:

```powershell
./tests/verify_source_preflight.ps1 -DeploymentFolder 'C:\Users\brews\Documents\Arduino\WispV2_Deploy_2026'
```

The PowerShell 7 entry point runs the five checks below and fails if any one fails. Pass
`-FormatterPath` with an installed `clang-format.exe` path to add a read-only formatting check.
It does not start the compile-audit launchers or the host tests. Individual checks remain available:

```powershell
./tests/verify_warning_scope.ps1
./tests/verify_example_includes.ps1
./tests/verify_wisp_example_mirrors.ps1 -DeploymentFolder 'C:\Users\brews\Documents\Arduino\WispV2_Deploy_2026'
./tests/verify_wisp_diagnostic_boundaries.ps1 -DeploymentFolder 'C:\Users\brews\Documents\Arduino\WispV2_Deploy_2026'
./tests/verify_patched_dependencies.ps1
```

Warning-scope verification checks the current external include groups. Diagnostic-boundary
verification also checks that canonical logger/memory headers exist and are explicitly included
by their sketches. A source-only pass does not substitute for a successful board build or runtime
verification. Compiler execution was authorized earlier on 2026-09-30, then paused by the user.
Do not run the native/firmware runners while that pause is in effect. New optional-feature tests
are uncompiled source candidates; dated earlier results retain their exact source hashes.

## Core and driver regression tests

The local `.github/workflows/core-boundaries.yml` prepares these host/source/mock checks on
push, PR and manual dispatch using Windows 2022 and pinned ArduinoJson v6.20.1. It has not been
pushed, dispatched or remotely accepted. It complements the existing formatting workflow;
it does not run the board firmware audit or prove physical behavior. Local compilers stay paused.

Run `./tests/verify_core_boundaries.ps1` from the repository root. MSVC C++ Build Tools
and the installed ArduinoJson headers are required. Assertions stay enabled; raw logs and
compiler products stay in a unique temporary folder. The runner checks production date, CSV,
watchdog, radio bookkeeping, SDI-12 parsing, averaging, AS5311, Analog and ADS1115 code, plus the
reed-switch anemometer and MQTT failure paths. Ten new optional-feature/lifecycle cases bring the source
inventory to 27 programs; the new cases and current source are uncompiled while compilers are
paused. The fixtures exercise production helpers or selected implementations with fake clock,
GPIO, Wire, watchdog and MQTT surfaces. They do not model real initialization register reads,
interrupt synchronization, modem/carrier behavior, flash/SD durability or end-to-end database
delivery. New sources include health journaling, GNSS metadata, heartbeat payloads, Manager
lifecycle, checksums, scheduling and buffer-pool ownership. Changed sources need retesting even
when an earlier version passed. See the [issue tally](../docs/ISSUE_TALLY.md) for current limits.

## Data safety regression tests

Run `./tests/verify_data_safety.ps1` from the repository root on Windows with Visual Studio C++ Build Tools installed.
It uses the package's ArduinoJson headers and writes compiler products only to a new temporary
directory. These host tests inject failed SD reads and closes into the production helpers and
cover sparse/orphaned filenames, case-insensitive matching, length/counter boundaries, JSON
overflow and recovery, and equivalence of buffered versus streamed pretty JSON.
They do not emulate SD hardware, MQTT transport, or prove that a board cannot freeze. The new
JSON-row-tail cases and current replay source are uncompiled after the compiler pause.

On a host with GCC, the same tests can be run with:

```sh
g++ -std=c++11 -Wall -Wextra -Isrc -I../ArduinoJson/src tests/data_safety_regression.cpp -o /tmp/loom-data-safety
/tmp/loom-data-safety
```

Hardware acceptance checks: remove/fault the SD card during batch replay, test failed debug-log
flushes, reboot with a sparse set of numbered files, and force JSON overflow before `logToSD()`
and `publish()`. Failed packets must not enter CSV/batch output or be published; the next valid
packet must work. Confirm `/debug/output_N.log` again includes the pretty JSON payload.

## Selected firmware matrix

`verify_sketch_compilation.ps1 -SketchList <file>` compiles every directory in a saved
one-directory-per-line list. It preserves sketch bytes, renames only the main `.ino` to
`LoomAudit.ino` in a validated temporary staging folder, and saves hashes, raw logs and firmware
under `tests/sketch_compile_<timestamp>`. Parent-relative includes require the original-folder
batch audit. Generated reports stay on disk and are ignored by Git.

Use the normal mode for acceptance builds. For a large API/example sweep, the optional
`-ReuseVerifiedLibraryObjects` mode restores only products whose board settings, source/header
bytes, header search winners and compiler programs still match. Arduino still discovers libraries,
compiles changed inputs and links each firmware. Cache fault tests run with
`python -m unittest discover -s tests -p test_verified_library_cache.py`.

A temporary archive can be made with `make_verified_audit_archive.py --cache <verified-libraries>
--source <Loom> --output <new-temporary-library>`. Pass it as `-PrecompiledLoom <library>` to
compile and link sketches against accepted library objects and byte-identical headers. This mode
checks archive/input hashes and compiles any directly discovered SDKs normally. It is an API/link
sweep, not a release firmware build: ordinary static-archive extraction can omit unreferenced
initializers. Do not upload these accelerated artifacts or use their sizes as release memory
measurements. Keep normal original-folder cold builds as the reference.

All modes retain raw external warnings. Cached warnings are not independent clean-build evidence.
`loom_warning_filter.ps1 -AdditionalLoomDirs` classifies verified copies of Loom headers as
Loom-owned too; include traces through them do not relabel a vendor warning.

## Board-package verification

### Recovery and wake acceptance

The WISP sketches opt into `hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS)`. The guard is
restored before USB/SD/RTC/sensor wake work and between mux sensors. LTE startup and explicit
network-time/publish windows remain exceptions because healthy network operations can exceed
the SAMD21 watchdog interval. Initial setup is not covered by this opt-in wake guard.

At boot, new data still goes into a new numbered CSV/batch pair. Older nonempty matching batch
files are selected separately, one at a time in directory order, and their record count is rebuilt
from SD. Even a recovered batch below the normal threshold is eligible for upload. No filenames,
packet fields, CSV columns, or line formats are changed. Recovery adds fixed-size bookkeeping
and one filename buffer, not a vector of files or an additional JSON document.

Recovery checks record framing and length, not full JSON syntax. Unterminated or oversized files
are left untouched, reported, and skipped for this boot so they cannot block the other files.
They need manual inspection; no partial-tail truncation or guessed data repair is performed.
The existing at-least-once delivery semantics remain: a reset after broker acknowledgement but
before clearing the file can cause duplicate delivery.

`logToSD()` now returns false when an enabled batch append fails, even if CSV succeeded.
`getSDManager()->getLastLogResult()` distinguishes each output. WISP performs at most one
`retryBatch()` for the same packaged sample following a confirmed rollback; it never retries
the CSV row or an uncertain append. Uncertain writes block further appends to that active batch;
CSV logging continues. A failed clear is retried on SD reinitialization or the next batch append
before that file can be replayed/appended again. Reboot starts a fresh active file and rediscovers
old batches.

In addition to host fault tests, verify on a Feather M0:

- Two old nonempty batches (including one below threshold) replay independently of new CSV data.
- An empty old file is ignored; a torn/oversized old file is preserved and does not block others.
- A power cut during upload/clear leaves data recoverable with at-least-once semantics.
- A failed batch append followed by a successful batch-only retry produces one CSV row.
- A packet that fits the JSON pool but exceeds encoded length stays in CSV, never the batch queue.
- Forced stalls in SD wake initialization and mux sensor power-up reset the board; healthy LTE
  startup does not. These hardware checks have not been substituted by the host helper tests.

Before building a board-package release, run `verify_patched_dependencies.ps1`. It compares the
installed package-level OPEnS_RTC, SparkFun AS726X, and SparkFun AS7265X build inputs against the
authoritative copies under `Loom/dependencies`. It also verifies that active SAMD21 SERCOM/Wire
files match the checksum-verified official Loom 4.9 versions. Experimental core snapshots under
`dependencies` are inactive engineering notes and are never promoted by the verifier.

Place this `tests` folder inside the Loom library folder:

```text
Loom\
  examples\
  tests\
    loom_compile_engine.bat
    loom_compile_get_cli_tools.bat
    loom_compile_smoke_no_logs_or_bins.bat
    loom_compile_audit_no_bins.bat
    loom_compile_retry_failed.bat
    loom_compile_audit_jolteon_r5_no_bins.bat
    loom_compile_audit_full.bat
    loom_retry_failed.ps1
    loom_warning_filter.ps1
    verify_wisp_diagnostic_boundaries.ps1
    verify_wisp_example_mirrors.ps1
    warning_scope_strip.cmd
    warning_scope_restore.cmd
    warning_scope_architecture.ps1
    WARNING_SCOPE.md
```

Version: `2026-09-30-v23-checked-artifact-copy`

The warning-scope headers remain present at the user's request. See
[`WARNING_SCOPE.md`](WARNING_SCOPE.md) for their narrow scope and maintenance rules.
Do not strip them as part of this branch's validation.

Double-click `warning_scope_strip.cmd` to remove the source instrumentation
while saving an exact restore patch. Double-click `warning_scope_restore.cmd`
to reinsert it. The saved state lives in `tests/.warning_scope_state` and is
left visible to Git so a stripped branch can preserve the exact patch and
manifest needed to restore its instrumentation.

## Scripts

```powershell
.\verify_wisp_diagnostic_boundaries.ps1
```

Checks that quiet Wisp sketches contain no added telemetry or SD debug logging, `_debug`
sketches retain compile-gated, tagged diagnostics, and both retain production watchdog/retry
coverage. Both Wisp checks accept `-DeploymentFolder` to check the active deployment pair too.
The diagnostic boundary check also accepts an array of saved sketch roots, so the older
five-minute debug copy and the separate staged V2 debug folder can be checked together:

```powershell
.\verify_wisp_diagnostic_boundaries.ps1 -DeploymentFolder @(
    "$env:USERPROFILE/Documents/Arduino/WispV2_Deploy_2026",
    "$env:USERPROFILE/Documents/Arduino/WispV2_Deploy_2026_debug"
)
```

The marker policy is defined in
[Loom style guide](../docs/STYLE_GUIDE.md).

```powershell
.\verify_wisp_example_mirrors.ps1
```

The legacy filename now checks operational parity between each quiet/debug pair, allowing only
the documented diagnostic/logging/serial-wait differences. It also rejects the obsolete nested
Wisp examples folder. There are no longer nested sketch mirrors to maintain.

```bat
loom_compile_audit_wisp_no_bins.bat
```

Manual warning-separated audit of the six library Wisp sketches. This launcher does not run
automatically as part of the source-only checks. Builds are a separate manual validation step.

```bat
loom_compile_get_cli_tools.bat
```

Checks Arduino CLI, installed cores, the Loom package folder, the examples folder, package libraries, sketchbook libraries, and the selected FQBN.

```bat
loom_compile_smoke_no_logs_or_bins.bat
```

Compiles every example and prints result lines. It uses temporary build/log folders and deletes them at the end.

```bat
loom_compile_audit_no_bins.bat
```

Compiles every example, prints concise console output, saves raw per-sketch logs, CSV, summary, warnings, and errors. Build folders are created under `%TEMP%` by default to keep generated Arduino dependency paths short on Windows. The report includes both raw `warnings_all.txt` and deduplicated `warnings_unique.txt`.

The normal smoke and audit launchers compile every LTE example without a hidden
modem-profile compiler flag. Jolteon examples select `LTE_MODEM::SARA_R5` in
their `Loom_LTE` constructor, so the same sketch works unchanged from Arduino
IDE and from the command-line audit. Other sketches retain the SARA-R4 default.
Loom instantiates the matching TinyGSM R4 or R5 adapter internally, so runtime
selection does not substitute one modem family's driver for the other.

Audit reports also split warning lines into `warnings_loom.txt` and `warnings_external.txt`, with unique variants for each. Raw per-sketch logs remain complete, including Arduino core and third-party library diagnostics.

```bat
loom_compile_retry_failed.bat
```

After the newest compile audit reaches its final summary, this reruns only its
`FAIL` rows. A failed row is eligible only when its original per-sketch log still
exists and its sketch folder still contains a correctly named main `.ino` file.
Delete a failed sketch's log to intentionally exclude it from retry; sketches
that were deleted or made redundant are excluded automatically. The retry
produces a normal saved audit report and stores its exact input list as
`requested_sketches.txt`.

```bat
loom_compile_audit_jolteon_r5_no_bins.bat
```

Compiles only the Jolteon examples. The R5 examples select their modem profile
in their `Loom_LTE` constructors; this launcher only narrows the example folder
and does not change global compiler settings.

```bat
loom_compile_audit_full.bat
```

Same audit plus keeps build folders and copies firmware artifacts into `complete_builds`. Copy failures make the run fail; filenames use the sketch index and original product name. Because the build root defaults to `%TEMP%`, the kept build folders are reported in `compile_report.csv` rather than nested under the audit folder.

## Console warning policy

The compiler still runs with `--warnings all`. Result lines always show total warning count plus `loom=` and `external=` counts.

By default, warning detail is hidden from the console:

```bat
set CONSOLE_WARNINGS=suppress
```

This avoids GCC warning cascades where the actual warning is surrounded by include traces, source snippets, caret lines, and note lines. Audit modes still save the full raw compiler logs, `warnings_all.txt`, `warnings_loom.txt`, and `warnings_external.txt`.

To print Loom-owned warning lines in the console:

```bat
set CONSOLE_WARNINGS=show
loom_compile_audit_no_bins.bat
```

`show` includes warning lines from the Loom library `src`, Loom examples, the active sketch folder, and generated sketch code. Arduino core and third-party package warnings are still counted and saved, but they are not printed.

To print every warning line in the console:

```bat
set CONSOLE_WARNINGS=all
loom_compile_audit_no_bins.bat
```

Console output modes:

```bat
set CONSOLE_OUTPUT=important
```

Default. Prints result lines, hard errors, and size/memory lines.

```bat
set CONSOLE_OUTPUT=none
```

Prints only progress and result lines.

```bat
set CONSOLE_OUTPUT=full
set CONSOLE_WARNINGS=all
```

Prints the full raw compiler output.

## Interrupting A Run

Pressing `Ctrl-C` stops the active `arduino-cli compile` process. The harness records that sketch as `STOPPED`, then asks:

```text
Stop all remaining compilations? [Y/N]
```

Answer `Y` to jump straight to the summary and leave the remaining sketches uncompiled. Answer `N` to continue with the next sketch.

## Defaults

```bat
set FQBN=loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off
set WARNINGS=all
set STRICT_WARNINGS=0
set CONSOLE_OUTPUT=important
set CONSOLE_WARNINGS=suppress
```

## Latest accepted results

See the [current issue tally](../docs/ISSUE_TALLY.md) for the compiler pause and historical build limits. To combine staged reports, run `summarize_sketch_compilation.py --requested <list> --reports <oldest> ... <newest> --output <summary.json>`. It checks current input hashes and requires the latest result for every requested sketch to pass; an earlier success cannot mask a failed retest.


## Offline bridge and viewer checks

`tools/service_mock` is a local prototype; it does not contact a broker, MongoDB or an account.
Open `viewer.html` in a browser to inspect explicitly fictional data or import a local receipt
JSON export. Python's SQLite mock separates device measurement UTC, arrival and acknowledged
mock insertion. Insertion failure retains pending work. Exact topic/payload retries share a mock
document while each arrival remains visible. This is not production deduplication or durability.

Routes accept `project/database/NameInstance`; the legacy two-part route uses `RemoteTest`.
Configure actual project routes explicitly. Payloads/queues have bounds and backpressure. A live
bridge still needs authentication, durable spooling, disk monitoring, actual broker client/IP
metadata and database acknowledgments. Map measured wind, battery and location fields/units
explicitly. Missing measurement UTC is never filled from receipt or insertion time.

Offline replay input is JSONL with `topic`, `packet`, and optional `client_id`, `source_ip`,
`received_utc`, `inserted_utc`, and `fail`. These interpreted checks do not invoke a compiler:

```text
python -B tools/service_mock/receipt_store.py replay.jsonl --output receipts.json
python -B -m unittest discover -s tools/service_mock -v
node tools/service_mock/test_viewer.js
```

Their success does not authorize firmware/native compiler execution or establish live delivery.

## Optional call and heap recorder

See [TRACE_DEBUGGING](../docs/TRACE_DEBUGGING.md) for the off/calls/heap debug toggle,
SD record format, coverage, performance cost, and offline snapshot inspector.
`node tests/test_trace_converter.cjs` checks live-block reconstruction, nested calls,
reallocation success/failure, missing records, partial tails, baseline totals, and labels.
Wisp mirror checks strip the separate trace blocks/tags while still comparing operational
code; the deployment copy retains its own analog and mux configuration.

Focused native programs `test_trace` and `test_trace_json_append` check both production serializers, copied object names, SD power-off buffering, bounded loss, stopped capture, JSON trailer restoration, short writes, sync/close faults and preservation of damaged tails. The final SD pair includes a directly loadable Chrome `.perfetto.json` plus recoverable `.ndjson`.

`test_function_start` and `test_function_start_off` exercise the production scope-capture
implementation with fake output sinks: quiet text does not suppress trace, nested calls
save only at the outermost recorded return, early returns balance capture, and unrecorded
enclosing scopes do not delay a save. `test_trace_auto` checks deferred/immediate startup,
identical/conflicting attachment, rejected manual-recorder overlap, and no retries after
startup/append failure. Its runner uses separate processes for each boot scenario.
`test_trace_attach_off` verifies that trace-off attachment evaluates neither argument and
needs no recorder symbols. Wisp boundary checks accept both automatic full and minimal
sketches while checking that their production safeguards remain present.

`test_debug_sketch` compiles the production shared checkpoint header in all four
trace/memory flag combinations. It checks consistent labels, single document/batch evaluation
for Serial reports, and no argument evaluation or reporter requirement when those reports
are disabled. Wisp sources must use the shared Loom header and native mux/SD methods;
local Wisp diagnostic wrappers are rejected.
