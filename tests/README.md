# Loom verification

Run source checks in PowerShell 7 from the Loom directory.

```powershell
./tests/verify_source_preflight.ps1
```

Add `-DeploymentFolder <path>` to include a local sketch in the include-order check. The
preflight also checks the ordinary and profiling Wisp sketch boundaries, the installed RTC
and SparkFun patches, and the official SAMD21 core hashes.

Run native regressions on Windows with Visual Studio C++ Build Tools and the board package's
ArduinoJson headers installed alongside Loom. Use `-TestName` to select a boundary program.

```powershell
./tests/verify_core_boundaries.ps1
./tests/verify_data_safety.ps1
```

Compile a sketch with the installed Loom 4.9 platform using Arduino CLI.

```powershell
arduino-cli compile --fqbn loom4:samd:adafruit_feather_m0 "examples/Radio/LoRa_SD_Journal"
```

`loom_compile_audit_no_bins.bat` builds the example suite.
`loom_compile_audit_wisp_no_bins.bat` builds the Wisp examples.
Some sketches require a local `arduino_secrets.h`. Keep actual credentials outside Git.
`verify_sketch_compilation.ps1` can check a requested sketch list and save per-sketch results.

Field validation uses a real RTC alarm and SD card. For scheduled LoRa, verify a missed
receipt leaves the node cursor unchanged, then restore the hub and verify backfill survives
a node reboot. The host receipt test verifies that a matching SD receipt stops retries
when the link ACK is missing, and that retries stay inside their slot. For Chimes, exercise
several tips before the next RTC sample.

For optional function and heap tracing, see the [trace guide](../docs/TRACE_DEBUGGING.md).
The existing native boundary runner covers trace startup, append recovery, and scope capture.
Run the viewer and launcher fixtures with Node.js. The wall-clock display check uses a separate
recorded SD trace with a known sleep count.

```powershell
$fixtures = Get-ChildItem tests/test_*.cjs | Where-Object Name -ne test_trace_display_clocks.cjs
node --test $fixtures.FullName
node tests/test_trace_display_clocks.cjs path/to/recorded.ndjson 18
```
