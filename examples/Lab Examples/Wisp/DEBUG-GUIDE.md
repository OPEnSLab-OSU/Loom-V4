# Wisp debug sketches

Start with the matching `*_debug_minimal` sketch. It keeps its ordinary deployment sibling's hardware configuration, watchdog handling, SD retry rules, 72-record batch, and five-minute sleep. The local V2 minimal copy keeps the local deployment's A0 input, sensor addresses, and port filter; library examples keep their own configuration.

## Minimal: two flags, capture plumbing

Edit the flags at the top of the `.ino`, then rebuild:

| LOOM_TRACE | LOOM_TRACE_HEAP | Adds |
| --- | --- | --- |
| 0 | 0 | No recorder or allocator hooks; deployment baseline |
| 1 | 0 | Function timeline and automatic RAM totals |
| 1 | 1 | The above plus bounded allocation/free/reallocation windows |

`LOOM_TRACE_HEAP` does nothing when `LOOM_TRACE=0`. Keep the same wiring and workload between runs. Trace capture itself consumes RAM and adds SD writes, so the trace-on run measures that overhead too.

The marked `MINIMAL_TRACE_*` blocks are the only additions to the deployment sketch:

1. **Controls and recorder:** select capture and declare its buffer. `Loom_TraceSketch.h` supplies the macros and selects heap hooks when needed.
2. **Start after `manager.initialize()`:** SD is ready; `LOOM_TRACE_BEGIN` attaches to Loom's existing `FUNCTION_START` instrumentation and records an automatic memory baseline. Startup allocations and initialization calls before this point are outside capture.
3. **Setup/loop scope and save guard:** capture the sketch's function entry/exit and save on return. The guard goes before `FUNCTION_START` so the exit event is recorded first.
4. **Flush before sleep:** save while SD is available. Hypnos marks storage unavailable during standby and restores it on wake; the loop return saves the remaining events.

The minimal version has no manually placed memory checkpoints, serial fragmentation reports, global-object registration list, mux scan debug, SD write debug, sample printing, extra sensor packet fields, or sleep-test helpers. Loom can still report its own runtime sensor objects. Allocator capture is bounded (default 16 events per window), with pauses marked; it is not a complete allocation history. Trace time excludes standby.

Loom currently shares a switch between function tracing and DEBUG serial output. The minimal sketch uses `setDebugOutput(LOOM_TRACE != 0)` so call capture works. It does not enable the additional SD text log or function summaries. Account for serial output overhead when comparing runs.

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

**V2 full-debug workload:** the V2 full sketch additionally writes and verifies `loom_sleep_settings.json`, advances through 3/10/30-minute stages (five verified wakes each), then sleeps at a two-hour cadence indefinitely. `prepareSleepSettings`, `reportSavedSampleCadence`, and `waitForScheduledWake` implement the bench schedule, saved timestamp checks, and RTC wake validation. These helpers remain active even with all trace/diagnostic flags off. This differs from the minimal version's ordinary five-minute sleep: use matching workloads for strict timing comparisons. Trace flags select capture, not the bench schedule.

## Build and inspect

Ordinary Arduino IDE Verify/Upload honors the sketch flags on Loom SAMD with the optional `Loom_TraceHeap` companion installed beside Loom. Capture files are `/debug/trace_N.perfetto.json` (timeline) and `/debug/trace_N.ndjson` (detailed inspector). They are separate from the full sketch's `/debug/output_N.log`.

For repeatable compile-only builds, use Loom's `tools/build_loom_trace.ps1` with the sketch folder:

```powershell
./tools/build_loom_trace.ps1 -Sketch 'examples/Lab Examples/Wisp/WispV2_Deploy_2026_debug_minimal' -Mode sketch
```

`-Mode sketch` uses the `.ino` flags and the IDE companion. `-Mode off`, `calls`, or `heap` override both trace flags for a build; `heap` supplies explicit linker hooks. The helper saves logs and the matching ELF in a unique temporary build folder and compiles without uploading unless `-Upload -Port ...` is supplied.

The portable launcher in `tools/sketch-launcher` also reads sketch flags. Copy its files into the minimal sketch folder if you want a self-contained launcher. Use `--no-upload` for compile-only work; the existing build-upload launchers otherwise upload by default. The local minimal folder supplied here has `upload:false` in its settings.
