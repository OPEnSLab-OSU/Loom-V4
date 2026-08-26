# Loom patch notes: SparkFun Spectral Triad AS7265X 1.0.3

Modified files:

- `src/SparkFun_AS7265X.cpp`: bounds every virtual-register bridge wait to 100 ms so a missing or
  wedged spectral triad cannot hold the SAMD21 in an infinite polling loop.
- `src/SparkFun_AS7265X.h`: defines `LOOM_AS7265X_PATCH_LEVEL=1` so Loom can verify that the matching
  bounded implementation was selected by Arduino's library resolver.

The public sensor API, successful register transactions, calibrated values, and Loom JSON field
labels are unchanged. Loom separately applies a 2.5-second overall measurement deadline.
