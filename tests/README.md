# Loom compile tests

Place this `tests` folder inside the Loom library folder:

```text
Loom\
  examples\
  tests\
    loom_compile_engine.bat
    loom_compile_check_tools.bat
    loom_compile_smoke_quiet_no_files.bat
    loom_compile_audit_quiet.bat
    loom_compile_audit_v3.bat
    loom_warning_filter.ps1
```

Version: `2026-06-27-v18-warning-scope`

## Scripts

```bat
loom_compile_check_tools.bat
```

Checks Arduino CLI, installed cores, the Loom package folder, the examples folder, package libraries, sketchbook libraries, and the selected FQBN.

```bat
loom_compile_smoke_quiet_no_files.bat
```

Compiles every example and prints result lines. It uses temporary build/log folders and deletes them at the end.

```bat
loom_compile_audit_quiet.bat
```

Compiles every example, prints concise console output, saves raw per-sketch logs, CSV, summary, warnings, and errors. Build folders are created under `%TEMP%` by default to keep generated Arduino dependency paths short on Windows. The report includes both raw `warnings_all.txt` and deduplicated `warnings_unique.txt`.

Audit reports also split warning lines into `warnings_loom.txt` and `warnings_external.txt`, with unique variants for each. Raw per-sketch logs remain complete, including Arduino core and third-party library diagnostics.

```bat
loom_compile_audit_v3.bat
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
loom_compile_audit_quiet.bat
```

`show` includes warning lines from the Loom library `src`, Loom examples, the active sketch folder, and generated sketch code. Arduino core and third-party package warnings are still counted and saved, but they are not printed.

To print every warning line in the console:

```bat
set CONSOLE_WARNINGS=all
loom_compile_audit_quiet.bat
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
