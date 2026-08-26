# Loom-patched dependencies

These directories contain authoritative source copies for third-party libraries modified by the
Loom 4.9.1 Feather M0 hardening release, plus one explicitly inactive core investigation snapshot:

| Directory | Upstream version | Loom-required change |
|---|---:|---|
| `OPEnS_RTC` | OPEnS/JeeLabs-derived | Checked, allocation-free DS3231 and alarm handling |
| `SparkFun_AS726X` | 1.0.4 | Public data-ready clear hook and 100 ms virtual-register waits |
| `SparkFun_Spectral_Triad_AS7265X` | 1.0.3 | 100 ms virtual-register waits |
| `Loom_SAMD21_Core_Patches` | loom4:samd 4.9 base | Inactive SERCOM/Wire timeout investigation notes; do not install |

## Packaging rule

Arduino does not reliably discover libraries nested below another library. A board-package release
must copy the three third-party library directories into the package's top-level `libraries`
directory alongside `Loom`; leaving the only copy under `Loom/dependencies` is not sufficient.

For a `loom4-beta` package, the installed layout must therefore contain:

```text
hardware/samd/<release>/libraries/
  Loom/
    dependencies/                 # reviewed source copies retained in the release/repository
  OPEnS_RTC/
  SparkFun_AS726X/
  SparkFun_Spectral_Triad_AS7265X/
```

Replace older copies rather than merging directories. The package-level copies must be byte-for-byte
equivalent to these authoritative source directories at release time. A user-installed library with
the same header name can otherwise be selected by Arduino's library resolver.

The headers define `LOOM_OPENS_RTC_PATCH_LEVEL`, `LOOM_AS726X_PATCH_LEVEL`, or
`LOOM_AS7265X_PATCH_LEVEL`. Loom checks those markers at compile time, turning an accidentally
selected upstream/old library into an explicit dependency error instead of the opaque
private-method error seen in beta.5 or silently selecting the older RTC implementation.

`Wire` and `SERCOM` are official core components and remain unmodified. The files under
`Loom_SAMD21_Core_Patches` are historical investigation notes only: do not package, promote, or
copy them over the official platform files. The release verifier checks the official core hashes.
