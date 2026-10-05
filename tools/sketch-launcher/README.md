# Sketch-controlled trace build and upload

Copy `build-upload.bat`, `build-upload.sh`, and `loom-build.cjs` into your sketch folder. Copy `loom-build.json.example` as `loom-build.json` and choose your board and port. Node.js 18+ and Arduino CLI are required; no npm packages are needed. The launcher also finds the Arduino CLI bundled with a standard Windows/macOS Arduino IDE installation.

Keep the controls in your sketch, before `Loom_TraceSketch.h`:

```cpp
#define LOOM_TRACE 1
#define LOOM_TRACE_HEAP 1
#include <Diagnostics/Loom_TraceSketch.h>

// In setup(), before manager.initialize():
LOOM_TRACE_ATTACH(manager, hypnos);
```

The optional adapter owns the recorder, starts after module/SD initialization, and saves
after the outermost recorded Loom call returns. No manual recorder/start/save block is
needed. Trace files show call timing and memory history; DEBUG text explains progress and
reported failures in Serial Monitor. Use both for complementary evidence, or keep text quiet
for a timing baseline. The Wisp sketches group `LOOM_DEBUG_TEXT` and `LOOM_DEBUG_SD_LOG`
beside the trace flags; setup applies them to Logger. The launcher follows the sketch without
another set of output settings in JSON. See [the trace guide](../../docs/TRACE_DEBUGGING.md)
for lifecycle behavior and the manual API.

`LOOM_TRACE=0` removes the trace recorder and allocation hooks. `LOOM_TRACE=1, LOOM_TRACE_HEAP=0` captures calls, objects and memory checkpoints. Both set to `1` also capture allocator activity. The launcher asks the actual board preprocessor to evaluate the sketch (including aliases, conditions and included configuration headers), then applies matching flags to library compilation and matching heap hooks to linking. There is no second trace mode to maintain in JSON. Allocations before the recorder starts are still baseline objects, without observed allocation times.

On Windows, double-click `build-upload.bat`, or run it from a terminal. On macOS/Linux, run `sh build-upload.sh`. Both **build and upload** by default. An ambiguous port stops before building/uploading; set `"port": "COM5"` or `"port": "/dev/ttyACM0"` rather than guessing. Close the serial monitor before uploading.

Use `build-upload.bat --no-upload` or `sh build-upload.sh --no-upload` to compile only. `--check` checks tools/settings without compiling or flashing. `--help` lists overrides for board, port, sketch folder, settings file and CLI path. Set `"upload": false` to make compile-only the default; `--upload` overrides it.

Optional settings: `arduinoCli` (executable path), `libraryPaths` (array), `additionalUrls` (board-index URL array), `outputDirectory`, and `jobs` (default 4). Relative library/output paths are resolved beside the settings file. Builds default to the system temporary folder's `loom-build/<unique-run>/`, keeping logs, exact flags, the matching ELF for allocation caller symbolization and upload binaries. This short path avoids older Windows SAMD compiler path-length limits. The launcher prints the complete location. `latest.json` identifies the latest successful compile. Every run has a separate build directory so toggling flags cannot reuse incompatible objects. Choose a short `outputDirectory` for permanent retention; temporary files may be removed by the operating system. These artifacts accumulate; remove old runs when no longer needed. If you store them under the sketch, add that output folder to Git ignore rules.

The launcher stages sketch code and its `src`/`data` folders without changing the original. Put sketch-local headers in the sketch folder or `src`; parent-relative includes outside the sketch are not portable through staging. Use `libraryPaths` for external libraries. For other architectures, allocation capture requires the GNU/newlib allocator symbols wrapped by Loom; the supplied heap setup is verified on Loom SAMD/Feather M0, not every Arduino board. Calls/checkpoints do not need those allocator symbols.

Ordinary Arduino IDE Verify/Upload now supports the same flags through the optional `Loom_TraceHeap` companion library, installed beside Loom. The companion is included only when both trace flags are enabled and supplies the linker settings automatically on SAMD21. This launcher remains useful for repeatable CLI builds, uploads, saved logs and matching ELF artifacts. Explicit CLI heap builds supply their own hooks and skip the companion during the final build.

Missing dependencies are reported with installation links:

- [Node.js](https://nodejs.org/en/download)
- [Arduino CLI](https://docs.arduino.cc/arduino-cli/installation/) or [Arduino IDE](https://www.arduino.cc/en/software)
- [Loom board and supplied dependencies](https://github.com/OPEnSLab-OSU/Loom-V4#install); preserve Loom's patched dependencies
- [Arduino Library Manager instructions](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-installing-a-library/)

Loom's board index is `https://raw.githubusercontent.com/OPEnSLab-OSU/Loom-V4/main/auxilary/package_loom4_index.json`; the Adafruit index is `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`. Install the `loom4:samd` core and the dependencies listed by Loom before building. The launcher reports missing dependencies; it does not silently install or replace tools/libraries.
