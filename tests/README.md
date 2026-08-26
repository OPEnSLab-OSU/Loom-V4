# Loom compile tests

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
    warning_scope_strip.cmd
    warning_scope_restore.cmd
    warning_scope_architecture.ps1
    WARNING_SCOPE.md
```

Version: `2026-07-15-v21-retry-failed`

The warning-scope headers are temporary audit instrumentation. See
[`WARNING_SCOPE.md`](WARNING_SCOPE.md) for the rules for adding them to new
source files and removing the entire structure before release.

Double-click `warning_scope_strip.cmd` to remove the source instrumentation
while saving an exact restore patch. Double-click `warning_scope_restore.cmd`
to reinsert it. The saved state lives in `tests/.warning_scope_state` and is
left visible to Git so a stripped branch can preserve the exact patch and
manifest needed to restore its instrumentation.

## Scripts

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

Same audit plus keeps build folders and copies firmware artifacts into `complete_builds`. Because the build root defaults to `%TEMP%`, the kept build folders are reported in `compile_report.csv` rather than nested under the audit folder.

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
