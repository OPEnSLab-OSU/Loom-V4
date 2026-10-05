# Optional calls and heap recording

The Wisp **debug** examples and the local Wisp V2 deployment debug copy have an extra trace toggle.
The standalone and source `WispV2_Deploy_2026_debug` copies now default to tracing ON and request
heap capture. Other debug examples retain their existing defaults. The source Wisp V2 debug
example shares the live copy's timing/diagnostics while keeping its original mux filter and all-port discovery. A recording
shows the nested instrumented calls, individual captured allocations, and allocator totals
at named checkpoints. Trace-off sketches omit the recorder buffer and SD trace writes. Logger
keeps a fixed interface and lightweight null-hook checks, so separately compiled library code
can obey a sketch-only flag without different class layouts.

## Choose the extra debug mode

In the active standalone debug sketch, change this flag and use ordinary Arduino IDE
Verify/Upload; no global compiler flags are needed for calls, objects and memory checkpoints:

```cpp
#define LOOM_TRACE 1 // 0 = off; 1 = calls, active objects, heap/free-RAM checkpoints
#define LOOM_TRACE_HEAP 1 // IDE discovers the optional heap companion when enabled
```

Individual malloc/free/realloc events use the allocator linker hooks supplied by the optional
`Loom_TraceHeap` library. The installed board package now includes it beside Loom. With both
flags enabled, ordinary IDE Verify/Upload discovers that library and adds the linker settings;
no launcher, extra compiler flag or board recipe edit is required. Turning either flag off
excludes the companion and allocation hooks. The startup log reports actual capture availability.
The saved session's `heap_hooks` field and the initial marker also report whether hooks are linked.
If `Loom_TraceHeapLink.h` is missing, install [the companion library](../dependencies/Loom_TraceHeap/README.md)
beside Loom, or disable allocation capture. The validated IDE target is SAMD21 Cortex M0+.

The live and source V2 debug folders include `build-upload.bat` (Windows),
`build-upload.sh` (macOS/Linux), `loom-build.cjs` and `loom-build.json`.
Run the Windows launcher or `sh build-upload.sh` to **build and upload** using the sketch's
flags; there is no trace mode to maintain separately. Set the board/port in JSON if automatic
port selection is ambiguous. Use `--no-upload` to compile only or `--check` to check tools/settings.
The launcher evaluates the flags with the real board preprocessor, then applies the matching
compiler/linker settings to a fresh build. It preserves logs and the matching ELF, reports
missing dependencies with installation links, and never uploads after a failed build.
See [the portable launcher guide](../tools/sketch-launcher/README.md) to copy it into another sketch.
Node.js 18+ and Arduino CLI (standalone or bundled with Arduino IDE) are required only for
these launchers; the ordinary IDE workflow does not need Node.js.

For explicit comparison modes, the older PowerShell helper remains available.
From the Loom folder, compile one of these modes. This helper only compiles by default.
It prints the temporary build folder, including its log and the matching firmware ELF.

```powershell
./tools/build_loom_trace.ps1 -Mode off
./tools/build_loom_trace.ps1 -Mode calls
./tools/build_loom_trace.ps1 -Mode heap
```

- `off`: ordinary sketch debug behavior, with the extra recording disabled.
- `calls`: nested calls and allocator-total checkpoints, without allocator interception.
- `heap`: calls plus allocation, free, realloc, and failure events. Use this to inspect live blocks.
- `sketch`: use the sketch's own flags with no extra compiler/linker settings, like an IDE build.

For the mux example:

```powershell
./tools/build_loom_trace.ps1 -Mode heap -Sketch "examples/Lab Examples/Wisp/Wisp_Mux_BatchLogging_debug"
```

To build the deployment copy with its own sensor settings:

```powershell
./tools/build_loom_trace.ps1 -Mode heap -Sketch "$env:USERPROFILE/Documents/Arduino/WispV2_Deploy_2026_debug"
```

The helper overrides the sketch toggle for comparison builds. Heap mode also sets
`LOOM_TRACE_HEAP`, `LOOM_TRACE_LINKER_HEAP_HOOKS` and the GNU linker wrapping flags.
All three source Wisp debug examples and the live deployment copy use the same reusable header and sketch flags. The old Wisp flags and build helper remain compatibility aliases.
Use a fresh build when changing linker modes.
The board's existing `debug=off` setting and this recording toggle are independent.

## Select mux drivers in a sketch

Place an optional list **before the first mux header include**:

```cpp
#define LOOM_MUX_COMPILED_ADDRESSES 0x74, 0x6B, 0x44
#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>

Loom_Multiplexer mux(manager); // Scans only the selected addresses by default.
```

The factory references only the listed sensor types; the linker removes the full loader and
unreferenced driver implementations. This is firmware/link-time removal: sensor headers are
still parsed, and Arduino may compile unused library/dependency objects. Defining the list in
the sketch works with ordinary IDE builds because the constructor receives a selected loader
at the call site, with one consistent class layout in sketch and library files.

Omit the define to retain the original full automatic loader and address list. Existing vector
and brace-list constructors still work; their runtime scan list is independent of the compiled
driver list. An address without a compiled factory produces no sensor object and reports the
missing loader. Unsupported compile-list addresses fail compilation instead of silently adding
all drivers. The original address-to-driver mapping is preserved; an address cannot distinguish
different sensor models that share the same I2C address.

The live V2 debug copy selects `0x74,0x6B,0x44,0x45,0x36,0x49,0x29`; the source V2 debug
example selects its original `0x74,0x15,0x6B,0x44`. The debug sketches keep explicit runtime
scan lists independent of the optional compile define, so deleting its entire `#ifndef` /
`#define` / `#endif` block restores the full loader while preserving the existing scan filter.
TSL2591 gain/integration, SEN66 options, gas power retention, and traced object sizes still use
the selected sensor's actual type and existing runtime settings.

For repeatable size comparisons, `-MuxDrivers all` uses the full loader while preserving the
sketch's scan list. `-MuxDrivers selected -MuxAddresses '0x74,0x15,0x6B,0x44'` selects drivers
for a sketch without editing its source. The default `-MuxDrivers sketch` obeys the sketch.
The comparison override is `LOOM_MUX_FORCE_ALL_DRIVERS=1`.

To compile and upload that exact mode, add `-Upload -Port COM5` (replace COM5 with your board's
port). Upload is optional and requires an explicit port. The helper uploads the verified binary
without recompiling. An ordinary Arduino IDE upload honors the standalone sketch's trace and heap
flags through the optional companion. The active deployment debug sketch prints an explicit
`[TRACE] SD trace capture OFF` message when recording is compiled out.

At runtime the shared adapter starts capture at the end of `manager.initialize()`,
when module/SD initialization has finished. It selects a new
`/debug/trace_N.ndjson` and `/debug/trace_N.perfetto.json` pair for each boot, with the same
session number as `output_N.log`, function summaries, the initial sensor CSV, and its batch.
SDManager scans both root data files and debug/trace names when selecting a new session, so
legacy captures and orphaned companion files are preserved. Old independently numbered
captures are not renamed. A genuine later CSV schema rotation does not rename the trace or
output log. Trace startup rejects a filename collision instead of silently renumbering.
The adapter prints both exact paths, and capture metadata includes `session_number`.
The outermost recorded function return is saved automatically, including early returns.
Nested recorded calls do not each cause a save. Hypnos also drains the recorder immediately before disabling SD/SPI and resumes writes only after SD initialization succeeds on wake. While SD is unavailable, events remain bounded in RAM; overflow is explicitly recorded, including omitted function boundaries. The clock
measures **active MCU time**; SAMD standby is not added to call durations.

## Use the same controls in any Loom sketch

`Diagnostics/Loom_TraceSketch.h` is shared by all the debug examples. The flags are generic,
independent of the board's ordinary debug setting, and default to zero when omitted. Define
both before including the header. No Wisp-specific helper structure or macros need copying:

```cpp
#ifndef LOOM_TRACE
#define LOOM_TRACE 1
#endif
#ifndef LOOM_TRACE_HEAP
#define LOOM_TRACE_HEAP 1
#endif
#include <Logger.h>
#include <Diagnostics/Loom_TraceSketch.h>

// After your existing Manager/Hypnos object declarations:
void setup() {
    Logger::getInstance()->setDebugOutput(false); // Trace does not require DEBUG text.
    LOOM_TRACE_ATTACH(manager, hypnos);
    // Keep your existing power/setup work before initializing modules.
    manager.initialize();
}

void loop() {
    // Your existing measurement, logging and sleep logic goes here.
}
```

`LOOM_TRACE_ATTACH` configures one library-owned fixed recorder without SD writes when
called before initialization. Manager starts it at the end of initialization and saves an
automatic memory baseline. Calling attach after initialization starts immediately. The
Manager and SDManager must have sketch/static lifetime. Identical duplicate attachment is
harmless; conflicting managers/settings and an already active manual recorder are rejected.
A failed start or append stops this boot's capture attempt without hidden retries, while
normal operation continues. The existing health observer is not replaced.

Existing `FUNCTION_START;` / `FUNCTION_START(this);` calls supply nested function entry/return
and memory measurements throughout Loom. The adapter saves after the outermost recorded
return. Minimal sketches need no explicit recorder, begin block, function scopes, or save
guards. Full debug sketches keep their extra checkpoints, labels, values, and custom function
scopes using the same adapter. The named checkpoint/marker/value helpers safely do nothing
before capture starts or after it stops. `setDebugOutput(false)` suppresses DEBUG text and
summaries without suppressing trace calls. With `LOOM_TRACE=0`, attachment arguments are
not evaluated and the automatic recorder and heap hooks are not linked.

The manual `LOOM_TRACE_RECORDER` / `LOOM_TRACE_BEGIN` API is still available for custom
recorder ownership. Manual capture retains explicit save boundaries: put
`LOOM_TRACE_SAVE_ON_RETURN()` before `FUNCTION_START` at deliberate setup/loop boundaries.
Use either manual capture or the automatic adapter for a session. Hypnos already flushes
before disabling SD and resumes after SD is ready on wake in both cases.

Set `LOOM_TRACE_HEAP=0` for calls, observed objects and allocator totals alone. With it set
to one, ordinary IDE uploads capture allocation/free/reallocation events when the heap companion
is installed. The portable launcher also follows these flags; alternatively select heap mode
explicitly with the older comparison helper:

```powershell
./tools/build_loom_trace.ps1 -Mode heap -Sketch "C:/path/to/YourLoomSketch"
```

The helper accepts other Loom sketches and copies their local source files, including an
Arduino `src` subfolder. Its default board remains the Loom SAMD Feather M0; supply `-Fqbn`
for a different compatible target. Heap wrapping requires a GNU linker and matching allocator
symbols; it is not a portable heap interceptor for every Arduino core. Capture starts when
SD is available, so objects allocated earlier have unknown allocation times. The old
`LOOM_WISP_TRACE` / `LOOM_WISP_TRACE_HEAP` flags are fallback aliases when the generic flags
are absent; new sketches should use the generic names. `build_wisp_trace.ps1` forwards to
the generic helper for existing commands.
## Load the SD file directly in Perfetto

Hypnos supplies a checked RTC clock for all SD file/folder creation and file modification dates,
including debug, trace, batch and settings files. FAT metadata stores configured local wall time
(with DST), with two-second modified-time precision; JSON timestamps and RTC alarms remain UTC.
RTC observations are shared for less than one second to avoid a new I2C read for every write;
the cache is invalidated before standby and refreshed on wake and after clock corrections.
Existing files acquire a current modified date on their next write. Old creation dates and
archived files are not retroactively guessed. A clock fault preserves an existing date; new
files without established RTC time retain SdFat's default date.

Copy `/debug/trace_N.perfetto.json` from the SD card and choose **Open trace file** at
[Perfetto](https://ui.perfetto.dev/). This is standard Chrome Trace Event JSON; no conversion is
required. It contains nested `B/E` function slices, allocation/free/realloc/failure instants,
heap measurements at every call entry and return, named memory checkpoints, observed objects,
and trace-saving duration on separate labelled tracks.

Each successful drain leaves a closed, valid JSON document. Updating it verifies and replaces
only its four-byte closing trailer. Short writes or failed sync restore the previous trailer
and length; an uncertain result stops debug appends. Sudden power loss during the trailer update
can still damage that companion file. Keep the independent append-only `.ndjson` recording:
the converter can recover its completed prefix and produce a fresh Perfetto file. The two files
are saved in order, not as an atomic pair; on a companion-write failure NDJSON may be more complete.

The direct file avoids an on-device allocation ledger: free instants contain the address, while
requested-size lifetime matching and live-block sums are computed by the desktop inspector.

## Inspect the recording

Run [Trace Studio](../tools/trace-studio/README.md), the tinybuild project, for the full
call-stack, object-inventory, and memory views with embedded Perfetto. From the Loom folder:

```text
cd tools/trace-studio
npm install
tinybuild
```

Open the printed localhost URL (normally http://127.0.0.1:8080/), then choose either SD
trace file or drop it onto the page. Inspection runs in a browser worker. Only the requested
Perfetto view needs an internet connection; files are passed to its browser frame for local
processing. The fictional example demonstrates the controls and is clearly labelled.

1. Select a function from the nested list. Search by function or object label if needed.
2. Choose **Call entry** or **Call return**, or move the event slider.
3. Read **Live captured bytes** and the table of blocks live at that exact event boundary.
   **Free heap (reusable)** shows allocator free blocks. **Available RAM estimate** adds
   the last measured stack-to-heap gap, with no reserve for future stack growth. Fragmentation
   means this total is not a guarantee for one allocation. Call comparisons show free heap
   and stack gap on entry and recorded return, as well as heap usage.
4. Read the active call stack and observed object inventory (including baseline mux sensors).
5. Select a block for its address, requested size, creation/release time, recorded call stack,
   object context, and raw caller instruction address.
6. Choose **View in Perfetto**, or **Download Perfetto JSON** and open that file in [Perfetto](https://ui.perfetto.dev/).
   **Download memory report** retains the complete call list, allocation lifetimes, and checkpoints.
   Each checkpoint lists its live allocation IDs, which refer to the full block records.

Detailed browser inspection accepts files up to 50 MB. General Chrome JSON and native Perfetto
files can be passed through without Loom memory inspection; the embedded handoff is capped
at 256 MB to bound its extra buffer copy. Larger files can be opened directly in Perfetto.
For larger NDJSON recordings, or command-line conversion:

```text
node tools/trace/trace_to_perfetto.js path/to/trace_1.ndjson
```

This writes `trace_1.perfetto.json` and `trace_1.memory.json` beside the input.
The older [offline inspector](../tools/trace/trace_viewer.html) provides a package-free
call/live-block view, without Trace Studio's object table or embedded Perfetto. If your browser
restricts local script files, `node tools/trace/serve_preview.cjs` opens a localhost-only
preview server and prints its URL. Stop that server with Ctrl+C when finished.

Perfetto has four labelled tracks: **Nested function calls**, **Heap and memory**,
**Checkpoints and capture quality**, and **Trace recording overhead**. The export uses
standard Chrome JSON `X`, `I`, `C`, and `M` events, supported by
[Perfetto's external-format importer](https://perfetto.dev/docs/getting-started/other-formats).

## Read the memory labels correctly

| Label | What it measures |
| --- | --- |
| Live captured bytes | Sum of requested sizes for blocks individually observed since capture began, still live at the selected event. |
| Heap bytes in use | `mallinfo().uordblks` at the latest checkpoint or function boundary; includes pre-capture allocations and allocator rounding/metadata. It is not a fresh measurement between checkpoints. |
| Reusable free bytes | `mallinfo().fordblks`; free storage inside the allocator's existing arena. |
| Free chunks | `mallinfo().ordblks`; a fragmentation indicator, not a guarantee that a requested block fits. |
| Stack-to-heap gap | Distance from the current stack marker to `sbrk(0)`. An estimate, not total free heap or stack high-water usage. |
| Object context | A labelled object whose method was executing. Context is attribution, not proof of ownership. |
| Unlabelled heap block | Address/size are known, but a class or purpose was not explicitly supplied. |
| Still live at last event | No release was captured before the recording ended. This does not by itself establish a leak. |
| Incomplete call | Events were lost, or recording ended before its return. Its shown duration stops at the last known boundary and is not a confirmed return time. |

The two byte totals are intentionally separate. A 128-byte observed allocation can coexist
with several kilobytes of earlier heap storage. The inspector never subtracts them and claims
the remainder is an exact list of unidentified blocks. It also does not read object contents,
infer C++ types from arbitrary pointers, or provide a garbage-collector-style reference graph.

## What is logged

The SD format is append-only JSON, one complete object per line. It remains readable if the
device resets without a closing JSON array. The converter wraps/reconstructs it into Chrome
trace JSON and recovers a partially written final line while flagging an incomplete tail.
Malformed interior records and concatenated boots are rejected.

Every buffered event has a 64-bit active timestamp in microseconds. The firmware extends
`millis()` rollover and takes the sub-millisecond part from `micros()`. It must observe the
clock at least once every 49 days. Event ordering is retained even for identical timestamps;
the inspector selects by event index as well as time.

| Event | Recorded information |
| --- | --- |
| Function begins / returns (`B` / `E`) | Timestamp, stack-to-heap gap, allocator bytes in use and reusable free bytes. Entry adds the full C++ function signature, source file, source line, and optional object address. Scope destruction records every normal return path. |
| Allocation (`A`) | Returned address, requested size, timestamp, raw caller return address. Converter adds the active instrumented call stack/object context. |
| Free (`F`) | Address and timestamp. Converter matches the recorded lifetime and size; freeing a baseline block is labelled uncaptured. Null free is ignored in the ledger. |
| Reallocation (`R`) | Old address, successful returned address, new requested size, timestamp, caller address. An in-place realloc closes the old size's lifetime and starts the new size's lifetime. |
| Allocation failure (`N`) | Requested size, old address for realloc, timestamp, caller address. Failed nonzero realloc keeps the old captured block live. |
| Zero-size realloc (`Z`) | Old/returned address and timestamp. Its allocator-dependent outcome is labelled uncertain instead of assuming it freed the old block. |
| Object label (`T`) | Static name, object address, optional informational container size. Labels never add heap bytes. |
| Object observed (`U`) / retired (`D`) | Copied module name (31 characters), address, optional known container size, owning mux address, zero-based mux port, I2C address and ready/unavailable state. Retirement is recorded before sensor deletion, including failed initialization. Observation is not proof of allocation time. |
| Memory checkpoint (`C`) | Heap bytes in use, reusable free bytes, free-chunk count, top free chunk, stack-to-heap gap, phase label and timestamp. |
| Marker (`I`) | Static phase label and timestamp. |
| Recorder overhead (`O`) | Time spent streaming, synchronizing/rolling back, and closing the trace batch. Saved on the next drain; the final unsaved overhead record may be absent. |
| Lost events (`lost`) | Number of events omitted when the fixed buffer filled, and last dropped timestamp. All affected lifetime/call claims are explicitly invalidated. |

Memory checkpoints occur at capture baseline, setup complete, cycle start, after measuring,
after JSON packaging, after SD storage, after the MQTT window, before standby, and after waking.
Allocation event timestamps reflect completion of successful allocation/reallocation; a free
event is recorded immediately before its allocator call. This is block-lifetime tracing,
not timing of allocator execution itself.

## Cost, failure handling, and coverage

Normal wakes append to the current sensor CSV, upload batch, and debug logs. LTE's `RSSI`
column stays present while the modem is off: JSON null (serialized as `null` in the CSV) means
there is no live reading, and packaging does not send an AT command in that state. Diagnostic
`output_N.log` and `funcSummaries_N.log` use the fixed boot/session number instead of following
a CSV schema rotation. A reboot still starts a new session; genuine schema changes or failed
CSV integrity checks still preserve the old file and select another rather than append
misaligned or uncertain data.

The active bench sketch finishes SEN66's bounded PM settling before its first timed sleep,
then waits for a verified RTC wake before recording its first sample. Settling must happen
while `millis()` runs: a retained sensor continues measuring during MCU standby, but the
MCU's elapsed-time counter pauses. The driver preserves its settled state on retained-power
wakes and still settles after a detected restart. This avoids a boot-only extra warmup in the
first recorded measurement. When SD settings
advance the stress interval, `setSampleInterval(interval, true)` anchors the new period to the
previous due wake instead of the end of active work. This schedules wake deadlines, not
fabricated sample timestamps: variable sensor restoration and measurement time can still
produce small variations between completed records. If work exceeds a period, missed slots
are skipped and remain visible in the timing diagnostics.

`[SAMPLE TIMING]` compares `timestamp.time_utc` from successfully saved records, rather than
alarm deadlines or a later RTC read. It reports the actual gap and the configured interval;
Perfetto also gets the gap, expected interval, and signed interval error. No sample timestamp
is rewritten to appear regular. A backwards clock correction starts a new comparison.

For this bench sketch, both Hypnos rails stay on through standby. Before initialization it
therefore calls `mux.setDFGasPowerRetained(true)`. Normal gas-board wakes check communication
and reuse acquisition settings instead of repeating `begin()` and the mode-change command.
Unavailable gas boards still attempt reconnection/configuration. Other sketches retain the
default power-cycled behavior; this option is appropriate only while gas-board power is retained.

Gas-board traces name acquisition-mode acknowledgement, compensation/temperature, and gas
identification transactions separately. The watchdog remains active for those calls; a stuck
underlying I2C transaction can still reset the board. Failure to acknowledge acquisition mode
disables that sensor until its later recovery attempt. An unreturned call is not reported as
successful, and unsaved buffered trace events may be absent after a watchdog reset.

The recorder keeps **24 fixed events** and small state in the opted-in sketch (**1,872 bytes**
in the verified SAMD21 heap build, plus a few pointer/guard bytes and per-scope stack storage).
Function and phase names borrow static strings in flash; object names are copied into the fixed event union so deletion or renaming cannot leave a dangling label.
It does not allocate a JSON document, a `String`, an on-device live-block table, or a growing
buffer. The desktop reconstructs live blocks and snapshots from allocation lifetimes.

Function boundaries drain a full buffer through two checked SD writes (NDJSON and closed Chrome JSON). Allocator hooks only append
small records in RAM and never write SD. Individual allocation capture defaults to **16 events
per saved window**. Before the window limit or queue capacity is exhausted, one reserved `L`
record marks an intentional heap-capture pause and counts subsequent omitted events with fixed
work and no SD writes. The next safe boundary drains that window and resumes capture.
Heap capture is also paused while SD/SPI is unavailable. Calls, object observations and
allocator/RAM totals continue through these allocation-only pauses; a pause invalidates block
lifetimes across the gap and must never be interpreted as a free or evidence of a leak.

Define `LOOM_TRACE_HEAP_WINDOW_EVENTS` (1..23, default 16) before `Loom_TraceSketch.h` to adjust
the cap in Arduino IDE or the launcher. Define `LOOM_TRACE_HEAP 0` to disable individual
allocation tracking while retaining call traces and heap/free-RAM measurements. A native
`begin(sd, true, limit)` call accepts the same cap. A safe boundary after a capped burst may
save a partially full buffer; it does not write for every allocation or every empty poll.

Non-heap events can still overflow while SD is unavailable. An explicit `lost` record identifies the affected
event types: `loss_flags` bits 1 = heap, 2 = calls, 4 = objects, 8 = clock/values,
16 = other events. Chrome JSON carries the same mask as `Loss flags`. A heap-only gap
invalidates allocation lifetimes while retaining call boundaries and observed objects.
Call/clock gaps prevent reliable wall-time reconstruction. Calls still open at the final
saved event are labelled `open at end`, rather than implying a missing return within the recording.

For older native v1 recordings without a mask, the converter recognizes the recorder's
canonical heap-hook and SD-power markers. While SD is available, that recorder drains
at call boundaries, so those gaps are classified as heap-only and labelled as inferred.
Gaps while SD is unavailable, or in unrecognized exports, remain conservatively uncertain.
No missing allocation events are recovered by this classification.
Increase `EVENT_CAPACITY` only after reviewing the extra SRAM cost; it is not an unlimited queue.

The writer uses SDManager's checked append, sync, close, and rollback policy. A failed append
stops optional capture for that boot; an uncertain append blocks normal SD debug writes as
before. Sample/batch storage retains its own existing safety paths. Capture suppresses its
own filesystem/logger/allocator events while draining, so the recorder cannot recurse into
itself. It does not change watchdog or power policy. Loss, failure, and unsaved tail events
remain possible; power-cut durability and watchdog timing require board testing.

SD writes and detailed logging add latency when enabled. The separate overhead track measures
saved-batch time, not all instrumentation overhead. Capture-off is the performance-preserving
mode; detailed recording is a bench/diagnostic mode. Do not claim zero timing impact for a
trace-enabled build without measurements on the actual board/card.

Only functions using `FUNCTION_START` / `INSTRUMENT()` are in the nested stack. Selected manager,
mux, LTE, and MQTT methods use `FUNCTION_START(this)` so object context is available.
Uninstrumented third-party functions are absent from that stack. Heap mode wraps public
`malloc/calloc/realloc/free` and newlib `_malloc_r/_calloc_r/_realloc_r/_free_r`, deduplicating
nested wrappers. C++ `new/delete` are covered when they delegate to those intercepted APIs;
custom allocators and references resolved internally without a linker-wrappable call can bypass
capture. The first hardware acceptance recording should exercise `malloc/free`, `new/delete`,
`String`, JSON growth, and realloc to establish actual toolchain coverage.

Global constructors and `manager.initialize()` complete before this Wisp recorder starts.
Their totals are visible in the baseline checkpoint, but individual baseline identities are
unknown. Interrupts and SD trace internals are also outside individual capture. A full list of
**all** live objects from boot would require an earlier recorder or allocator-specific baseline
enumeration; this implementation deliberately does not guess that list or scan undocumented
heap headers. Caller PCs require the **matching build's ELF**, not a later recompiled firmware.

For additional instance methods use `FUNCTION_START(this);`. A static function or sketch
function uses `FUNCTION_START;` (or `FUNCTION_START();`). For a descriptive nested scope use
`FUNCTION_START(object, "Scope label");`. The same macro feeds ordinary debug summaries and
optional tracing, whose enable switches remain independent. The old object/trace macro names
remain compatibility aliases; new code uses `FUNCTION_START`. To supply known meanings inside
an `#if LOOM_TRACE` block, use `if (auto *trace = Loom_Trace::current())
trace->label("Packet buffer", pointer, requestedSize)` with a static string. Label the exact allocation pointer to
name a block. Labelling a container gives object context and does not identify its separately
allocated internal storage.

## Verification

Additional debug-sketch size controls are independent of tracing:
`LOOM_DEBUG_DIAGNOSTICS=0` removes the extra memory checkpoint helper and skips enabling verbose
mux scans/SD write commentary. `LOOM_DEBUG_PRINT_SAMPLES=0` omits full sensor JSON printing.
They do not remove sensor measurement, packaging, normal CSV saving, or batch storage.
`build_loom_trace.ps1 -Diagnostics off` applies both switches plus
`LOOM_COMPILE_MUX_DEBUG=0` and `LOOM_COMPILE_SD_WRITE_DEBUG=0` to a fresh build. These latter
library switches remove the verbose implementation/message strings, rather than just leaving
them disabled at runtime. Mux diagnostic arguments are removed too. SD failure warnings and
append safety checks remain. Pass the library switches through `compiler.cpp.extra_flags`
for every translation unit; defining them only inside an `.ino` does not configure library
source files. Both default to `1` to preserve existing runtime setter behavior.

The function-summary writer is linked only when a sketch calls `ENABLE_FUNC_SUMMARIES`
or `Logger::enableSummaries()`. Omitting that call removes the writer and summary format strings
without another build define. Lightweight `FUNCTION_START` bookkeeping still remains for
normal debug logging and optional traces. This is link-time removal: Arduino may compile an
unused source file, but its implementation occupies no space in the resulting firmware.
The live/source V2 debug sketches use generic sleep labels and `loom_sleep_settings.json`;
the sketch writes/read-verifies it using Loom's existing SleepInterval layout. Configured
intervals and mux wiring are unchanged by the label update.

LTE captures raw CSQ signal quality while initializing/connecting with the modem awake,
then refreshes it during packaging if the modem is already powered. A single `RSSI_cached`
boolean marks a reused valid reading; `RSSI: null` means no valid reading has been captured
in this boot. Raw CSQ 99/transport failures do not replace a valid cache. No SD persistence,
age/timestamp strings, or modem wake-up is added for RSSI. The cache itself is one byte.

The converter's allocation/lifetime and corruption checks run with no compiler or dependencies:

```text
node tests/test_trace_converter.cjs
```

The existing Wisp mirror and diagnostic-boundary checks validate that debug additions do not
alter the quiet sketch's operational code or deployment sensor configuration. These checks,
browser demonstration, firmware compilation, and real-board recording are separate evidence;
a fictional browser trace is not a hardware recording.

Serial memory checkpoints now spell out **heap in use**, **change since previous checkpoint**,
**reusable free bytes/chunks**, **stack-to-heap gap estimate**, and **JSON pool used/capacity**
with byte units. JSON pool storage is already included in heap totals. Call-boundary `mallinfo()`
measurements and the second SD output add diagnostic cost; trace-off builds retain no recorder
or heap hooks. Actual SD latency, watchdog margin, and long-run coverage still require a board run.

Free heap (`mallinfo().fordblks`) is recorded on every traced function entry/return and phase
checkpoint, including call-only mode; the direct SD Chrome file already has its Perfetto counter.
The desktop adds the available-RAM estimate from these existing measurements without extra
firmware logging. Static/global RAM is outside those free regions. The measured stack gap is
only an estimate at that instant, not an allocation budget or a stack high-water mark.
