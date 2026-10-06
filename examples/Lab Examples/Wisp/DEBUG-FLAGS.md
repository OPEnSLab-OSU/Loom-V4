# Debug flags

Set flags at the top of the sketch, then rebuild/upload. `1` = on; `0` = off.
**Debug text explains what happened; trace files show when calls ran and how memory changed.** Both work together or independently.

| Flag | What it enables |
| --- | --- |
| `LOOM_DEBUG_TEXT` | Routine Logger text in Serial Monitor. Off still allows warnings/errors and separately enabled direct Serial reports. |
| `LOOM_DEBUG_SD_LOG` | Copies Logger messages to `/debug/<name>_debug_N.log`; direct Serial reports are not copied. |
| `LOOM_DEBUG_PRINT_SAMPLES` | Pretty sensor JSON through Logger; also needs `LOOM_DEBUG_TEXT=1`. |
| `LOOM_DEBUG_MEMORY` | Serial memory checkpoints: allocator/stack estimates, changes, JSON usage, and batch count. |
| `LOOM_DEBUG_MUX_SCAN` | Verbose sensor discovery/scan reports; does not change selected sensors or ports. |
| `LOOM_DEBUG_SD_WRITES` | Extra Serial reports about SD write decisions/results. |
| `LOOM_DEBUG_DIAGNOSTICS` | Default for MEMORY, MUX_SCAN, and SD_WRITES above; individual overrides win. Does not control text, samples, or trace. |
| `LOOM_TRACE` | Call timeline, object labels, and RAM snapshots in `/debug/<name>_trace_N.perfetto.json` and `<name>_trace_N.ndjson`. |
| `LOOM_TRACE_HEAP` | Adds individual allocation/free/reallocation events; requires `LOOM_TRACE=1` and heap linker hooks (IDE: `Loom_TraceHeap`). |
| `LOOM_TRACE_HEAP_WINDOW_EVENTS` | Allocation events captured per bounded burst: `1..23`, default `16`; not an on/off flag. |
| `LOOM_DEBUG_LOG_NAME` | Optional string prefix for text, summaries, and traces; omit to use the Manager name. Uncomment the example at the top of the sketch to customize. |

For `Manager manager("Deploy_Test_", 8)`, files are `Deploy_Test_debug_N.log`, `Deploy_Test_funcSummaries_N.log` (if enabled), and `Deploy_Test_trace_N.*` under `/debug`. `<name>` in the paths means this Manager/custom prefix. Trailing underscores are trimmed; other characters besides letters, digits, `_`, and `-` become `_`. Custom names are limited to 63 characters. All files share one boot/session number, retained on normal wakes; existing legacy filenames also reserve numbers. CSV/batch names are separate.

`LOOM_DEBUG_CHECKPOINT(...)` uses one phase label for the Serial memory report and trace snapshot; each obeys its own flag.

You can add debugging progressively; the flags are independent except for the dependencies above:

1. Start with the minimal sketch and all on/off flags at `0` for a quiet baseline.
2. Set `LOOM_TRACE=1` for calls/RAM, then `LOOM_TRACE_HEAP=1` for allocation capture.
3. Enable whichever text, SD text logging, and extra reports you need. Use the full debug sketch for its additional checkpoints/helpers.

Rebuild/upload after each change. Normal sensor/CSV/MQTT logging is independent; turning flags off in the full sketch leaves its extra bench routines/workload in place.

**Off does not always mean fully compiled out:**

- TRACE off omits the recorder/buffer; HEAP off omits allocation hooks while ordinary trace can remain on.
- MEMORY off removes its reporter/calls; PRINT_SAMPLES off removes the sketch's display calls.
- TEXT and SD_LOG off stop their output, but some underlying logging code remains. Warnings/errors still print with TEXT off.
- MUX_SCAN and SD_WRITES off remove their enabling calls. To remove verbose library code too, pass `LOOM_COMPILE_MUX_DEBUG=0` and `LOOM_COMPILE_SD_WRITE_DEBUG=0` to all library compilations; defining them only inside the sketch is insufficient.

The build helper's `-Diagnostics off` disables text/reports/samples and applies those library compile flags; `-Mode` selects trace separately. Small shared logging/instrumentation checks remain even with everything off.
