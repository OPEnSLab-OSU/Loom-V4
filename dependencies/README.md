# Loom-patched dependencies

The board package installs these source copies as sibling libraries of `Loom`. Arduino
cannot discover them reliably from inside `Loom/dependencies`.

| Library | Loom patch |
| --- | --- |
| `OPEnS_RTC` | Checked DS3231 clock and alarm transactions |
| `SparkFun_AS726X` | Data-ready clear hook and bounded virtual-register waits |
| `SparkFun_Spectral_Triad_AS7265X` | Bounded virtual-register waits |
| `Loom_TraceHeap` | Opt-in SAMD21 allocator hooks for sketches that enable heap recording |

Package copies must match the source here. Run `tests/verify_patched_dependencies.ps1`
before building a release. It also checks the official Loom 4.9 `Wire` and `SERCOM` files.
The board profile supplies RadioHead and TinyGSM. SIM7090 uses TinyGSM's SIM7080 driver.
Include ArduinoMqttClient in release archives so MQTT examples compile in a fresh sketchbook.

The [SAMD21 I²C timeout investigation](Loom_SAMD21_Core_Patches/README.md) is available as two patches for an isolated test platform.
