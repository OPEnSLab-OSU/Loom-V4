# Optional Loom tracing

Regular lab sketches use their existing logging. Enable trace capture in a profiling sketch
with SD support by setting the flags before including the trace header.

```cpp
#define LOOM_TRACE 1
#define LOOM_TRACE_HEAP 0
#include <Diagnostics/Loom_TraceSketch.h>
```

Call `LOOM_TRACE_ATTACH(manager, hypnos)` in `setup()` before `manager.initialize()`.
Capture starts after Loom initializes its modules and SD. `FUNCTION_START(this)` records
call entry, return, and object identity. Optional `LOOM_TRACE_CHECKPOINT` and
`LOOM_TRACE_MARKER` label memory measurements and events. Set `LOOM_TRACE_HEAP` to `1`
to record bounded allocator events after capture starts. Ordinary IDE builds discover
the [SAMD21 heap companion](../dependencies/Loom_TraceHeap/README.md) when both flags
are enabled. The CLI helper can also supply the allocator hooks directly.
The recorder keeps up to 16 individual heap events per save window by default.
`LOOM_TRACE_HEAP_WINDOW_EVENTS` accepts 1 through 23. Trace Studio marks periods
when capture was capped or the SD card was unavailable.

With an initialized SD card, the recorder saves `/debug/<name>_trace_N.ndjson` and
`/debug/<name>_trace_N.perfetto.json`. The diagnostic session number stays fixed across
sleep and CSV rotation. Text logging and SD function summaries can be configured
independently.

The [Wisp profiling sketches](../tools/trace/sketches/Wisp) are developer examples.
The full sketches save text logs and include detailed diagnostics. The minimal sketches
record calls and heap activity with routine Serial and SD text logging off. Direct-sensor,
mux, and Wisp V2 variants preserve their respective hardware and measurement workloads.
The PowerShell helper compiles the Wisp V2 profiling sketch by default, or accepts a
custom `-Sketch` folder. It saves the matching ELF for symbolizing addresses.

```powershell
./tools/build_loom_trace.ps1 -Mode off
./tools/build_loom_trace.ps1 -Mode calls
./tools/build_loom_trace.ps1 -Mode heap
```

For the sketch-controlled Node launcher, see [Sketch Launcher](../tools/sketch-launcher/README.md).
Use its `--no-upload` option for compile-only checks. Open the files from SD in
[Trace Studio](../tools/trace-studio/README.md), or convert an NDJSON capture offline.

```text
node tools/trace/trace_to_perfetto.js path/to/trace.ndjson path/to/output-base
```

## Multiplexer diagnostics

In a powered profiling sketch, enable these before `manager.initialize()` to see each
controller and sensor probe in Serial, including the address and port before entering
Wire. The last message can identify the operation when a transaction never returns.
`setScanDebug(true)` includes failed probe status codes. You can also call `mux.debugScan()`
explicitly to inspect connected addresses without constructing or initializing the sensors.

```cpp
mux.setDebug(true);
mux.setScanDebug(true);
```

The full Wisp mux and Wisp V2 profiles use `LOOM_DEBUG_MUX_SCAN` for these settings.
Regular lab sketches leave mux diagnostics off. `LOOM_COMPILE_MUX_DEBUG=0` removes detailed
mux output at compile time.
