# Loom 4.9.1 platform patch manifest

The complete Feather M0 hardening release includes changes outside the Loom library repository.
They live in the surrounding `loom4:samd` 4.9 package and must be included in the dependency/core
archive used by beta teams.

## Required platform files

Paths are relative to `packages/loom4/hardware/samd/4.9`:

- `cores/arduino/SERCOM.cpp`
  - rejects a bus state that is neither idle nor locally owned;
  - checks SERCOM error flags during address and data operations;
  - caps master command, address, write, and read waits at 100 ms.
- `libraries/Wire/Wire.cpp`
  - waits at most 100 ms for each requested read byte;
  - returns the actual number of valid bytes after a timeout or lost bus;
  - preserves successful `Wire` transaction bytes and public signatures.
- `libraries/SparkFun_AS726X/src/AS726X.cpp`
- `libraries/SparkFun_AS726X/src/AS726X.h`
  - exposes the existing data-ready clear operation used by Loom's bounded conversion path;
  - caps both virtual-register bridge waits at 100 ms.
- `libraries/SparkFun_Spectral_Triad_AS7265X/src/SparkFun_AS7265X.cpp`
  - caps all virtual-register bridge waits at 100 ms.

The Loom wrappers add the 2.5-second overall AS7262, AS7263, and AS7265X conversion deadlines.
Shipping only this Git repository without the platform files above leaves lower-level I2C polling
unbounded.

## Compatibility and validation

No I2C public signature, successful read/write byte, sensor JSON field, SD format, MQTT topic, or
radio framing changes. A stalled transaction now returns failure instead of blocking forever.

Final `loom4:samd:adafruit_feather_m0` builds passed for WISP batch/mux/v2, Dendrometer hub/node,
Digital, tipping bucket, AS7262/3/5X, VCNL4020, RemoteManager, ThingSpeak, LoRa transmit/receive, and
the OPEnS_RTC compatibility sketch. The warnings-all pass reported zero Loom-owned warnings and
zero warnings in these patched platform/dependency sources.

Hardware beta acceptance must include SDA-low and SCL-low fault injection, removal of the fault,
sensor-rail power cycling, a successful subsequent transaction, RTC sleep/wake, and normal
clock-stretching sensors. The timeout bounds software waiting; it cannot electrically release a
line held low by external hardware.
