# Wisp firmware size comparison — October 1, 2026

All builds passed for `loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off`.
The limit is 262,144 bytes. Sizes below are Arduino CLI's **program-storage** measurement,
not raw `.bin` file lengths. No firmware was uploaded.

| Sketch / enabled features | Full mux loader | Selected mux drivers | Driver savings |
|---|---:|---:|---:|
| Live debug: heap tracing, normal debug logs, extra memory/mux/SD diagnostics, JSON printing | 245,436 B (93.6%) | 222,396 B (84.8%) | 23,040 B |
| Live debug: tracing and extra diagnostics/JSON printing off; normal debug logs retained | 224,780 B (85.7%) | 201,756 B (77.0%) | 23,024 B |
| Ordinary source deployment example: existing warnings/errors setting, tracing and verbose diagnostics off | 216,820 B (82.7%) | 187,404 B (71.5%) | 29,416 B |

The source debug example with heap tracing and all extra diagnostics, using its original
sensor selection, also passed at **215,564 B (82.2%)**. Against the quiet source example
with the same selected sensor addresses, its complete debug configuration costs **28,160 B**.
That includes the debug sketch's additional sleep-test/settings code as well as recording and
diagnostics; it is not a measurement of the recorder alone.

For the same live debug sketch, turning off heap tracing and extra diagnostics saves
**20,656 B** with the full loader or **20,640 B** with selected drivers. Combining driver
selection with those toggles saves **43,680 B**, from 245,436 B to 201,756 B.

The live selection remains `0x74,0x6B,0x44,0x45,0x36,0x49,0x29` (six driver types).
The source selection remains `0x74,0x15,0x6B,0x44` (four driver types). Compare rows within
each sensor set: different sensor sets retain different dependencies. Normal CSV/batch
storage, sensor measurement, and SD append safety checks remain in every configuration.
`-Diagnostics off` removes extra diagnostics and JSON printing; the debug sketch's ordinary
Logger messages remain. The quiet example uses its existing warnings/errors runtime setting.

## Reproduce

Run from the Loom folder. Each invocation makes a fresh temporary build, records all flags,
and prints its build/log/ELF folder. Compilation is the default; upload requires explicit flags.

```powershell
$live = "$env:USERPROFILE/Documents/Arduino/WispV2_Deploy_2026_debug"
./tools/build_loom_trace.ps1 -Mode heap -MuxDrivers all -Sketch $live
./tools/build_loom_trace.ps1 -Mode heap -Sketch $live
./tools/build_loom_trace.ps1 -Mode off -Diagnostics off -MuxDrivers all -Sketch $live
./tools/build_loom_trace.ps1 -Mode off -Diagnostics off -Sketch $live

$quiet = 'examples/Lab Examples/Wisp/WispV2_Deploy_2026'
./tools/build_loom_trace.ps1 -Mode off -Diagnostics off -MuxDrivers all -Sketch $quiet
./tools/build_loom_trace.ps1 -Mode off -Diagnostics off -MuxDrivers selected -MuxAddresses '0x74,0x15,0x6B,0x44' -Sketch $quiet
./tools/build_loom_trace.ps1 -Mode heap -Sketch 'examples/Lab Examples/Wisp/WispV2_Deploy_2026_debug'
```

`-MuxDrivers all` preserves the sketch's runtime scan filter. `-MuxDrivers selected` supplies
the compile-time factory list without editing the ordinary deployment sketch. Existing sketches
without a selection define still use the full loader. Sensor headers are parsed, and Arduino
can compile dependency objects; unused implementations are removed from the linked firmware.

## Verification and retained artifacts

ARM ELF symbol checks confirmed:

- Selected builds contain their required sensor constructors and omit `loomMuxAllSensors`
  and every unselected Loom sensor implementation.
- Full-loader builds contain the full loader and all previously supported sensor types.
- Trace-off builds omit the recorder implementation and allocator wrappers; heap builds retain both.
- Verbose mux/SD messages and mux debug implementations are absent with diagnostics off.
  The SD warning for an uncertain append remains.
- Function-summary formatting is absent when `ENABLE_FUNC_SUMMARIES` is unused. A separate
  opt-in sketch compiled successfully and retained the summary writer/format strings.
- Default, vector, brace-list, and empty-list mux constructors passed focused ARM compile checks
  in legacy and selected modes. An unsupported `0x68` selection failed with the intended error.
- Both actual debug sketches also compiled with their entire selection block deleted and
  referenced the full loader. Their runtime scan filters are explicit and stay unchanged.
  With selection present, their firmware section sizes match the measured builds above.
- Function instrumentation native tests and quiet/debug sketch safeguard checks passed.

Temporary artifacts are under `C:/Users/brews/AppData/Local/Temp/`:

| Build | Artifact folder |
|---|---|
| Live full debug / full mux | `loom-trace-e03de2b16c4a485ab8e1bc5d9019beed` |
| Live full debug / selected mux | `loom-trace-3e0f0c5e2bd941bc9f75ee4b9fbc3207` |
| Live reduced debug / full mux | `loom-trace-4efbfbdc100e4ddca124e287d310745b` |
| Live reduced debug / selected mux | `loom-trace-beee1dcb68114b498ac1a923b1d51d96` |
| Quiet source / full mux | `loom-trace-a418af42b37a4f84b99ba0073142e673` |
| Quiet source / selected mux | `loom-trace-c0c5dab70d2045b587e329fc1703bb8b` |
| Source debug / selected mux | `loom-trace-b4a1577a188244a9a6972efcdb946939` |
| Summary opt-in probe | `loom-trace-29886510031b4724943500555df54c0f` |
| Mux constructor/address checks | `loom-mux-check-c570414324844ce0b150fe79ddd6aa52` |

These checks verify firmware composition and compilation. Sensor I/O, sleep timing, and
the new firmware's SD output still require a hardware run after an intentional upload.
