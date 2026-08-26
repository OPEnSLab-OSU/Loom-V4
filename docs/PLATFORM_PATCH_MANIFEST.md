# Loom 4.9.1 dependency and platform manifest

The Feather M0 hardening release changes Loom-owned code and three reviewed third-party sensor
libraries. It does not modify the official `loom4:samd` board core. Reviewed third-party sources
are vendored under `Loom/dependencies`; the release packager must promote those directories into
the surrounding package's top-level `libraries` directory.

## Official board-core policy

The active release uses the exact files from the checksum-verified official Loom 4.9 archive:

- `cores/arduino/SERCOM.cpp`
  - SHA-256: `E0ABC9FAF850762A966D0C6A64A048CEC8B8A0C0E37290454FE97A840510ADB2`
- `libraries/Wire/Wire.cpp`
  - SHA-256: `4975914E951A003DE39EB3A6DA72E721B48F1156E890A0A3A2EF8A81C871592C`

`tests/verify_patched_dependencies.ps1` rejects drift from these official hashes. The timeout-edited
files under `Loom/dependencies/Loom_SAMD21_Core_Patches` are inactive investigation notes only.
They must not be copied into the platform or included as release build inputs.

The Loom wrappers add 2.5-second overall AS7262, AS7263, and AS7265X conversion deadlines, and the
vendored spectral libraries bound their virtual-register bridge polling. Those bounds reduce known
sensor-level hangs but do not prove that official lower-level Wire/SERCOM polling can escape every
electrically wedged SDA/SCL condition. Hardware fault injection remains an explicit beta test item.

## Required patched sensor libraries

Authoritative source directories:

- `Loom/dependencies/OPEnS_RTC`
  - checked, allocation-free DS3231 operations and field-proven alarm behavior;
  - defines `LOOM_OPENS_RTC_PATCH_LEVEL=1`.
- `Loom/dependencies/SparkFun_AS726X`
  - exposes the existing data-ready clear operation used by Loom's bounded conversion path;
  - caps both virtual-register bridge waits at 100 ms;
  - defines `LOOM_AS726X_PATCH_LEVEL=1`.
- `Loom/dependencies/SparkFun_Spectral_Triad_AS7265X`
  - caps all virtual-register bridge waits at 100 ms;
  - defines `LOOM_AS7265X_PATCH_LEVEL=1`.

The release archive must also install byte-equivalent build inputs at `libraries/OPEnS_RTC`,
`libraries/SparkFun_AS726X`, and `libraries/SparkFun_Spectral_Triad_AS7265X`. Arduino does not
reliably resolve libraries nested below Loom. Loom's compile-time marker checks now reject an old
RTC or upstream spectral dependency explicitly; beta.5's private `clearDataAvailable()` error
means the old AS726X header was selected.

## Compatibility and validation

No I2C public signature, successful read/write byte, sensor JSON field, SD format, MQTT topic, or
radio framing changes. Sensor-level AS726x stalls have bounded failure paths; no blanket bounded-
recovery claim is made for the unmodified official Wire/SERCOM core.

The prior broad compile matrix covered WISP batch/mux/v2, Dendrometer hub/node, Digital, tipping
bucket, AS7262/3/5X, VCNL4020, RemoteManager, ThingSpeak, LoRa transmit/receive, and the OPEnS_RTC
compatibility sketch. After restoring the official core, targeted Feather M0 warnings-all builds
of AS7262 and WISP batch logging pass. Rerun the complete matrix on the release archive before
promotion; do not treat the inactive core experiment's earlier matrix as release evidence.

Hardware beta acceptance must include SDA-low and SCL-low fault injection, observation of whether
the official core returns or stalls, watchdog/power-cycle recovery, a successful subsequent
transaction, RTC sleep/wake, and normal clock-stretching sensors. If a lower-core stall is
reproducible, treat it as a board-core limitation requiring a separately reviewed upstream/core
change rather than silently substituting the inactive experiment.
