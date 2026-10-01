# Sketch-only heap toggle verification

Verified on 2026-10-01 with Arduino CLI 1.5.1 using the same board build recipes as Arduino IDE:
`loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off`.
All runs compiled only; none uploaded firmware. No platform/board recipe changes were made.

| Build | Sketch trace flags | Extra compiler/linker properties | Result |
|---|---|---|---|
| Source Wisp V2 debug | trace 1, heap 1 | none | 215,620 B; companion discovered; allocator wrappers and hook-availability symbol in ELF |
| Live Wisp V2 debug | trace 1, heap 1 | none | 222,524 B; companion discovered; allocator wrappers and hook-availability symbol in ELF |
| Temporary source debug copy | trace 1, heap 0 | none | 215,028 B; companion absent from compiler database; all allocator wrapper/getter symbols absent; call recorder remains |

The source enabled build preceded the final startup-message edit; these numbers are verification
artifacts, not a controlled firmware-size comparison. See `BUILD_SIZE_COMPARISON.md` for that
separate matrix. The live build includes the final actual-capture ON/OFF message.

Every ordinary build's `build.options.json` had an empty `customBuildProperties` field.
`arm-none-eabi-nm` confirmed the expected wrapper inclusion/exclusion, rather than relying only
on the precompiled-library message. The optional archive contains just the 8-byte GNU archive
header; hooks compile from `src/Diagnostics/Loom_TraceHeapHooks.inc`. The installed companion
matches `dependencies/Loom_TraceHeap` byte-for-byte.

Additional checks passed: native allocator/serializer/recursion/loss boundary tests, actual
capture-status getter tests, Wisp deployment safeguards, portable-launcher scenarios and
release packaging (including preservation of the empty archive). CLI launcher builds also
passed for both debug sketches. A small real-board compiler test confirmed that header aliases,
board conditions and inactive definitions are evaluated correctly by the launcher.

Normal IDE use now needs only these sketch controls before `Loom_TraceSketch.h`:

```cpp
#define LOOM_TRACE 1
#define LOOM_TRACE_HEAP 1
```

The companion must be installed as a separate top-level Arduino library beside Loom. It is
installed in the current Loom board package. Future board releases must include it according
to `dependencies/README.md`; it is not discovered while nested under Loom's dependencies folder.
