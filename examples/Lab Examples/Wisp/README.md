# Wisp sketches

See the [short debug flag reference](DEBUG-FLAGS.md) for Serial text, SD logs, memory reports, and heap tracing.

Each example has a quiet sketch and a separate Arduino sketch ending in `_debug`.
Open the `.ino` that matches the enclosing folder name; do not combine the two variants
in one Arduino sketch folder.

| Quiet sketch | Instrumented sketch |
| --- | --- |
| `Wisp_Batch_Logging/Wisp_Batch_Logging.ino` | `Wisp_Batch_Logging_debug/Wisp_Batch_Logging_debug.ino` |
| `Wisp_Mux_BatchLogging/Wisp_Mux_BatchLogging.ino` | `Wisp_Mux_BatchLogging_debug/Wisp_Mux_BatchLogging_debug.ino` |
| `WispV2_Deploy_2026/WispV2_Deploy_2026.ino` | `WispV2_Deploy_2026_debug/WispV2_Deploy_2026_debug.ino` |

The quiet variants omit memory checkpoints, mux scans, SD write traces, JSON display,
and `/debug/<name>_debug_N.log` logging. They suppress the logger's `DEBUG` messages and skip
the serial-console wait at boot. Warnings, errors, and existing direct hardware status
messages remain available. CSV records and MQTT batch records are still saved normally.

The debug variants retain timestamped SD debug logging, JSON display, memory/reset
checkpoints, and applicable mux/SD traces. Function summaries remain an optional logger
feature. `LOOM_WISP_BETA_DIAGNOSTICS=0` disables the marked memory/mux/SD instrumentation
within a debug sketch; it does not make that sketch equivalent to a quiet sketch.

Both variants preserve sensor definitions, enabled mux ports, batch size, rail settings,
RTC/time-sync ordering, watchdog protection, and the single safe batch-only retry.
Keep each pair's configuration synchronized. The active sketch under
`Documents/Arduino/WispV2_Deploy_2026` also has its own `_debug` subfolder; its A0 input
and selected mux ports differ from the library deployment example and remain intentional.

Run the source-only checks from the Loom library folder:

```powershell
./tests/verify_wisp_example_mirrors.ps1
./tests/verify_wisp_diagnostic_boundaries.ps1
```

Both checks accept `-DeploymentFolder` to include the active sketch pair. The old mirror
check filename is retained for existing callers; it now checks clean/debug operational
parity and rejects a reintroduced nested `Wisp/examples` folder.

When builds are authorized, `tests/loom_compile_audit_wisp_no_bins.bat` runs the existing
warning-separated audit against these six sketches only. It is manual and is not
triggered by either source-only check. No compiler validation has been completed for
this readability refactor.
