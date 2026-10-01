# Loom-patched dependencies

These directories contain authoritative source copies for third-party libraries modified by the
Loom 4.9.1 Feather M0 hardening release, plus one explicitly inactive core investigation snapshot:

| Directory | Upstream version | Loom-required change |
|---|---:|---|
| `OPEnS_RTC` | OPEnS/JeeLabs-derived | Checked, allocation-free DS3231 and alarm handling |
| `SparkFun_AS726X` | 1.0.4 | Public data-ready clear hook and 100 ms virtual-register waits |
| `SparkFun_Spectral_Triad_AS7265X` | 1.0.3 | 100 ms virtual-register waits |
| `Loom_SAMD21_Core_Patches` | loom4:samd 4.9 base | Inactive SERCOM/Wire timeout investigation notes; do not install |
| `Loom_TraceHeap` | Loom 1.0.0 | Optional IDE-discovered allocator hooks selected by sketch trace flags |

## Packaging rule

Arduino does not reliably discover libraries nested below another library. A board-package release
must copy the three third-party library directories and `Loom_TraceHeap` into the package's top-level `libraries`
directory alongside `Loom`; leaving the only copy under `Loom/dependencies` is not sufficient.

For a `loom4-beta` package, the installed layout must therefore contain:

```text
hardware/samd/<release>/libraries/
  Loom/
    dependencies/                 # reviewed source copies retained in the release/repository
  OPEnS_RTC/
  SparkFun_AS726X/
  SparkFun_Spectral_Triad_AS7265X/
  Loom_TraceHeap/                 # needed only by sketches with allocation capture enabled
```

Replace older copies rather than merging directories. The package-level copies must be byte-for-byte
equivalent to these authoritative source directories at release time. A user-installed library with
the same header name can otherwise be selected by Arduino's library resolver.

`Loom_TraceHeap` lets ordinary IDE Verify/Upload obey `LOOM_TRACE_HEAP=1` without a
launcher or board recipe changes. Its small empty `.a` archive activates Arduino's
library-scoped linker flags; the allocator hook implementation is built from Loom source.
Preserve that archive when packaging. Disabling `LOOM_TRACE` or `LOOM_TRACE_HEAP` excludes
the companion. Do not add unconditional allocator linker flags to the board platform.

The headers define `LOOM_OPENS_RTC_PATCH_LEVEL`, `LOOM_AS726X_PATCH_LEVEL`, or
`LOOM_AS7265X_PATCH_LEVEL`. Loom checks those markers at compile time, turning an accidentally
selected upstream/old library into an explicit dependency error instead of the opaque
private-method error seen in beta.5 or silently selecting the older RTC implementation.

`Wire` and `SERCOM` are official core components and remain unmodified. The files under
`Loom_SAMD21_Core_Patches` are historical investigation notes only: do not package, promote, or
copy them over the official platform files. The release verifier checks the official core hashes.
