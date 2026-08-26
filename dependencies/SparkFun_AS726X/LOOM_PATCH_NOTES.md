# Loom patch notes: SparkFun AS726X 1.0.4

Modified files:

- `src/AS726X.cpp`: bounds each virtual-register bridge wait to 100 ms so a missing or wedged
  AS7262/AS7263 cannot hold the SAMD21 in an infinite polling loop.
- `src/AS726X.h`: exposes the library's existing `clearDataAvailable()` operation so Loom can start
  a one-shot conversion and apply its own 2.5-second overall deadline; defines
  `LOOM_AS726X_PATCH_LEVEL=1` for dependency-selection validation.

The successful register transactions, calibrated sensor values, and Loom JSON field labels are
unchanged. A timeout returns failure/zero to the Loom recovery path instead of hanging forever.
