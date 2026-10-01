# Wisp / Loom Trace Studio

A local tinybuild project for the optional Loom SD recorder. It combines a readable memory
inspector, uPlot charts, and the Perfetto timeline. The demo is fictional, not a board run.

From this directory, in CMD with your globally installed tinybuild:

```text
npm install
tinybuild
```

Open http://127.0.0.1:8080/. Stop the server with Ctrl+C. `tinybuild build` builds without
starting the server. The server binds to 127.0.0.1; its settings are in tinybuild.config.js.
The pinned uPlot dependency and lockfile are included; tinybuild itself is your global install.

Drop either SD file here:

- `trace_N.ndjson`: reconstruct lifetimes and generate Chrome JSON on the computer.
- `trace_N.perfetto.json`: inspect the same Loom records and open the enriched timeline in Perfetto.

Use **Choose sensor CSV** to load the matching Loom sensor file alongside the trace. The
**Sensor samples** tab plots any numeric sensor field and shows every recorded field in a
scrollable table, with 100 samples per page. It reports the recorded UTC range, sample intervals
and whether optional additive-16 row checksums are present and valid. It preserves both files
when switching tabs. CSV times include sleep; the trace active clock excludes sleep, so these
clocks are shown separately. CSV inspection accepts up to 10 MB. Files remain in the browser.

The selected-event navigator stays visible above the inspector while scrolling. It shows the
event number/name, awake time, estimated UTC where available and enclosing call. Step, enter
an event number, or **Show event on timeline** to reveal the matching page/marker. Memory cards
and object/allocation tables follow that selection; the RAM card identifies its latest measured
checkpoint. Whole-recording SD overhead lives under capture coverage. The wall-clock chart uses
larger measurement dots and **Zoom to selected event** for the selected awake period.

Choose a call, then **Call entry** or **Call return**. Choose a phase or step one event at a
time to inspect the running call stack, live allocation addresses and requested sizes,
known objects, mux ports, I2C addresses, and initialization state. Select a block to see its
creation and release stacks, source locations, caller instruction address, and recorded lifetime.
Call comparisons show heap totals on entry/return and which allocations survive that call.
Free heap is displayed at the selected event and compared on call entry/return. The available
RAM estimate adds the measured stack-to-heap gap to reusable free heap, without reserving future
stack growth. Fragmentation means this is not a single-allocation guarantee. The chart includes
both free heap and available RAM estimates, using the last recorded measurement at each point.
The stepped chart shows captured requested bytes beside the last measured allocator total;
hover for values, drag to zoom, click a recorded point, and double-click to reset.
The memory chart defaults to estimated UTC wall time when complete RTC/sleep diagnostics exist.
Captured pre/post RTC seconds are anchored to measured wake/restoration timing, rather than the
later diagnostic-write time. Standby gaps are blank, and event positions also show estimated UTC.
Choose **Awake execution time** for precise profiling in seconds. Missing wake diagnostics,
lost events or backwards reconstructed time leave the chart in awake time. Perfetto and the
call/event timeline keep their precise active-time clock. Wall reconstruction does not imply
that memory was sampled during standby. Allocation-capture status is stated above the timeline.

**View in Perfetto** opens the embedded timeline. **Open full Perfetto** uses a new browser
tab. Both use the stable buffer postMessage interface with checked source/origin and a readiness
handshake. Downloads preserve the generated Perfetto JSON or complete Loom memory report.
General Chrome JSON and `.pftrace` / `.perfetto-trace` files open in Perfetto without detailed
Loom inspection. The iframe remains available for repeated recordings. Switching views preserves
its zoom, selection and SQL workspace; **Show selection in Perfetto** moves to the selected call
or event without importing the recording again. **Find selected event** brings the selected marker
into the scrollable inspector timeline. Exported Perfetto JSON also preserves the original Loom
records, so reopening a download retains the memory and object inspection.

Files are read locally in a worker and are not posted to the localhost server. Perfetto loads
its UI and processing engine from ui.perfetto.dev when requested; trace data is passed to that
browser window for local processing. The Loom inspector works without that connection after
its local assets have loaded. Detailed inspection is capped at 50 MB. Perfetto handoff is capped
at 256 MB because stable Perfetto requires an additional file buffer; open larger files directly
in Perfetto. Convert larger NDJSON files with the CLI in ../trace/trace_to_perfetto.js.

These are instrumented stacks and reconstructed block lifetimes, not a debugger's complete
heap dump. Baseline objects can be observed while their allocation times remain unknown.
Allocator totals include baseline storage and allocator overhead; captured totals count requested
sizes. Internal buffers are separate blocks. Loss invalidates affected histories and is labeled.
A retained allocation is not by itself proof of a leak. Caller addresses need the matching ELF.
SAMD standby is excluded from the recorder's active clock. Enabled recording adds runtime/SD
latency; use capture-off for ordinary operation.

See [firmware setup and coverage](../../docs/TRACE_DEBUGGING.md),
[Perfetto external formats](https://perfetto.dev/docs/getting-started/other-formats), and
[Perfetto embedding](https://perfetto.dev/docs/visualization/embedding-the-ui).

Verified on 2026-09-30: global tinybuild 1.0.6 build/run, uPlot 1.6.32; fictional-cycle inspection
and embedded import; production recorder JSON from the native SD harness imported directly;
second trace replaced the first using the same frame; creation/release stacks and entry/return
live-block comparisons. This is desktop/native evidence, not real-board timing or SD acceptance.

Rechecked on 2026-10-01: recorder fixture import; same-filename reload; invalid-file recovery;
call-only capture and incomplete-call boundary labels; an 18,004-event fictional recording with
7,000 calls and 1,000 allocations, including paging, sensor filtering and navigation to its final
event. Embedded Perfetto displayed call and memory tracks for both the demo and recorder fixture.
All 27 converter cases plus selection and Perfetto handoff tests pass, including enriched-export
reimport, exact event ordering, range-only navigation and stale-load cancellation.
