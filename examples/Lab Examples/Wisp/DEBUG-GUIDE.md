# Wisp debug sketches

Start with the matching `*_debug_minimal` sketch. It keeps its ordinary deployment sibling's hardware configuration, watchdog handling, SD retry rules, 72-record batch, and five-minute sleep. The local V2 minimal copy keeps the local deployment's A0 input, sensor addresses, and port filter; library examples keep their own configuration.

## Minimal: two flags and one attach call

Edit the flags at the top of the `.ino`, then rebuild:

| LOOM_TRACE | LOOM_TRACE_HEAP | Adds |
| --- | --- | --- |
| 0 | 0 | No recorder or allocator hooks; deployment baseline |
| 1 | 0 | Function timeline and automatic RAM totals |
| 1 | 1 | The above plus bounded allocation/free/reallocation windows |

`LOOM_TRACE_HEAP` does nothing when `LOOM_TRACE=0`. Keep the same wiring and workload between runs. Trace capture itself consumes RAM and adds SD writes, so the trace-on run measures that overhead too.

The only capture additions are the two flags, the header, and one call in `setup()` before `manager.initialize()`:

```cpp
#define LOOM_TRACE 1
#define LOOM_TRACE_HEAP 1
#include <Diagnostics/Loom_TraceSketch.h>

// In setup(), before manager.initialize():
LOOM_TRACE_ATTACH(manager, hypnos);
```

Loom owns the fixed recorder buffer only when attachment is enabled. Calling attach before initialization configures capture without SD writes. Manager starts it after module/SD initialization and saves an automatic RAM baseline. Calling attach after initialization starts immediately. Startup allocations and initialization calls before this point are outside capture. The health observer is independent and is not replaced.

Existing Loom `FUNCTION_START` calls supply the timeline. The adapter automatically saves after the outermost recorded function returns, including early returns, rather than writing after every nested sensor call. Hypnos saves pending trace records before disabling SPI/SD and restores availability on wake; returning from the recorded sleep operation saves the remaining events. No explicit recorder, begin call, save guard, sketch function scope, or pre-sleep flush is needed.

The adapter supports one Manager/SD pair per boot, both with sketch/static lifetime. Identical repeated attachment is harmless; conflicting managers/settings and an already active manual recorder are rejected. A failed start or uncertain/failed append stops capture for this boot without automatically retrying. Normal device operation continues. Trace-off does not evaluate the attach arguments and links no automatic recorder. Flags stay in the sketch header so IDE heap-library discovery works without relying on sketch defines reaching library source files.

The minimal version has no manually placed memory checkpoints, serial fragmentation reports, global-object registration list, mux scan debug, SD write debug, sample printing, extra sensor packet fields, or sleep-test helpers. Loom can still report its own runtime sensor objects. Allocator capture is bounded (default 16 events per window), with pauses marked; it is not a complete allocation history. Trace time excludes standby.

Trace capture is independent of DEBUG text output and function summaries. The minimal sketch keeps `setDebugOutput(false)` for every capture mode, preserving warnings/errors while omitting routine DEBUG messages, the extra SD text log, and function summaries. Automatic startup reports the capture paths and whether heap capture is active. The minimal timeline covers Loom calls; add optional `FUNCTION_START` scopes if you also want custom sketch functions shown.

## Full: add investigation layers

The existing `*_debug` sketches keep the detailed phase checkpoints and helper functions. Their flags are documented beside their definitions:

| Flag | Adds |
| --- | --- |
| `LOOM_TRACE` | Structured SD timeline and explicitly named RAM snapshots |
| `LOOM_TRACE_HEAP` | Bounded allocator event windows; requires trace |
| `LOOM_DEBUG_DIAGNOSTICS` | Default for the three extra diagnostics below; does not control trace |
| `LOOM_DEBUG_MEMORY` | Serial checkpoint reports: allocator use/free blocks, stack gap, fragmentation and contiguous-growth estimates, JSON use/overflow, batch count, reset cause |
| `LOOM_DEBUG_MUX_SCAN` | Verbose discovery/scan output in mux sketches |
| `LOOM_DEBUG_SD_WRITES` | Verbose SD write decisions/results |
| `LOOM_DEBUG_PRINT_SAMPLES` | Pretty-printed sensor JSON; independent of the diagnostic default |

The memory/mux/SD flags default to `LOOM_DEBUG_DIAGNOSTICS`, preserving the existing full-debug behavior. Set that default to `0`, then explicitly enable only the layer you need. Set sample printing to `0` separately. `WISP_DIAGNOSTIC_CHECKPOINT` prints a detailed serial report; the adjacent `LOOM_TRACE_CHECKPOINT` writes a trace RAM snapshot. One can be enabled without the other. Trace checkpoints before capture starts are no-ops; early serial memory reports still work.

The full sketches also keep their existing `ENABLE_SD_LOGGING` text log and normal DEBUG output. Turning off the extra diagnostic flags does not remove those. The minimal sibling omits the SD text log. V2 full debug limits compiled mux drivers with `LOOM_MUX_COMPILED_ADDRESSES`, whereas its deployment/minimal siblings use the normal full driver bundle; use `-MuxDrivers all` for both when comparing build sizes, or apply the same compiled address list to both.

The object-registration list adds human-readable labels to the inspector. `sizeof(object)` describes the fixed object, not every allocation it owns. The sketches do not call `Loom_MemoryDiagnostics::addToPacket`; memory reports do not add fields to sensor CSV/MQTT payloads.

Full sketches use the same `LOOM_TRACE_ATTACH` adapter as the minimal versions. They retain their explicit checkpoints, object labels, diagnostic values, and custom function scopes, while Loom handles startup and saving. `LOOM_TRACE_MARKER` and `LOOM_TRACE_VALUE` safely do nothing if capture never started or has stopped. A retained sketch `FUNCTION_START` scope saves automatically when that outer scope returns; nested Loom calls do not each trigger a save.

The manual `LOOM_TRACE_RECORDER` / `LOOM_TRACE_BEGIN` API remains available for other sketches needing custom recorder ownership. Manual capture does not enable automatic saves. Use either manual capture or `LOOM_TRACE_ATTACH` for a session, rather than combining them.

**V2 full-debug workload:** the V2 full sketch additionally writes and verifies `loom_sleep_settings.json`, advances through 3/10/30-minute stages (five verified wakes each), then sleeps at a two-hour cadence indefinitely. `prepareSleepSettings`, `reportSavedSampleCadence`, and `waitForScheduledWake` implement the bench schedule, saved timestamp checks, and RTC wake validation. These helpers remain active even with all trace/diagnostic flags off. This differs from the minimal version's ordinary five-minute sleep: use matching workloads for strict timing comparisons. Trace flags select capture, not the bench schedule.

## Build and inspect

Ordinary Arduino IDE Verify/Upload honors the sketch flags on Loom SAMD with the optional `Loom_TraceHeap` companion installed beside Loom. Capture files are `/debug/trace_N.perfetto.json` (timeline) and `/debug/trace_N.ndjson` (detailed inspector). They are separate from the full sketch's `/debug/output_N.log`.

For repeatable compile-only builds, use Loom's `tools/build_loom_trace.ps1` with the sketch folder:

```powershell
./tools/build_loom_trace.ps1 -Sketch 'examples/Lab Examples/Wisp/WispV2_Deploy_2026_debug_minimal' -Mode sketch
```

`-Mode sketch` uses the `.ino` flags and the IDE companion. `-Mode off`, `calls`, or `heap` override both trace flags for a build; `heap` supplies explicit linker hooks. The helper saves logs and the matching ELF in a unique temporary build folder and compiles without uploading unless `-Upload -Port ...` is supplied.

The portable launcher in `tools/sketch-launcher` also reads sketch flags. Copy its files into the minimal sketch folder if you want a self-contained launcher. Use `--no-upload` for compile-only work; the existing build-upload launchers otherwise upload by default. The local minimal folder supplied here has `upload:false` in its settings.
