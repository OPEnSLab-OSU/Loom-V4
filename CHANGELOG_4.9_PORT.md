# Loom 4.9 Updates

This release expands cellular, air-quality, multiplexer, power-management, and
field-deployment support while preserving the existing Loom 4.9 APIs and
packaged data schema.

## Highlights

- Added SARA-R5 LTE support for OPEnS/Jolteon hardware.
- Added the Sensirion SEN66 air-quality sensor.
- Expanded automatic I2C multiplexer discovery and port control.
- Improved Hypnos RTC recovery, interrupt handling, SD configuration, and
  battery-voltage checks.
- Updated SmartRock, Evaporometer, WeatherChimes, Wisp, LilyPad, and cellular
  deployment examples.

## LTE

### SARA-R5 support

- Added library-wide SARA-R4/SARA-R5 modem selection through
  `Loom_LTE_Config.h`.
- Added the OPEnS/Jolteon SARA-R5 PWR_ON sequence and optional RESET_N recovery.
- Added configurable R5 power timing, control-pin polarity, UART baud probing,
  optional rail control, and forced operator registration.
- Added raw AT-command logging, modem identity checks, SIM diagnostics,
  registration diagnostics, signal reporting, and serial passthrough support.
- Added bounded APN, username, and password storage.
- Added a configurable modem reset pin.

### Reliability

- Separated modem boot readiness from carrier registration and PDP activation.
- LTE network failures can now be retried on later wake cycles without requiring
  a complete device restart.
- Recoverable LTE boot failures can be retried by the Loom manager.
- Corrected the manager recovery hook so recovery runs during `power_up()`
  rather than during JSON packaging.
- Corrected R4 boot probing so the UART remains at 9600 baud instead of being
  reopened at the R5 baud rate.
- Boot retries pulse PWR_ON only once, preventing a later retry from switching
  an already-running modem back off.
- Powered modems can be shut down cleanly after partial initialization.
- Removed redundant `AT+CFUN=1` commands during connection attempts.
- Increased the MQTT transmit payload capacity to `MAX_JSON_SIZE`.

### Compatibility

- SARA-R4 remains the default modem profile.
- The existing R4 board sequences remain unchanged:
  - OPEnS boards use the 5-second active-high power pulse and release the pin.
  - SparkFun boards use the 3-second active-low power pulse.
  - R4 communication remains at 9600 baud.
- Enable R5 for the complete library build with `LOOM_LTE_USE_SARA_R5` or by
  selecting it in `Loom_LTE_Config.h`.
- R5-specific examples now stop at compile time when the library is still using
  the default R4 profile, instead of silently building the wrong modem driver.

## I2C multiplexer

- Added individual mux-port enable and disable controls.
- Added port allow-list configuration.
- Added configurable mux addresses.
- Added detailed mux and downstream I2C scan diagnostics.
- Added safe clearing and rebuilding of automatically loaded sensors.
- Added sensor power-down forwarding.
- Added configurable TSL2591, SEN55, and SEN66 auto-load options.
- Added automatic SEN66 loading at `0x6B`.
- Added DFRobot MultiGas loading at `0x74` and `0x75`.
- Mux discovery verifies TCA9548 control-register readback rather than treating
  every ACK at `0x70` through `0x77` as a multiplexer.
- Channel selection reports failure and sensor operations are skipped when the
  requested channel cannot be selected.
- Sensors that fail initialization are deleted instead of being retained in
  the active sensor list.
- Sensor re-scanning is available through the public `refreshSensors()` API.
- Retained SEN55 loading at `0x69`.
- K30 auto-loading at `0x68` remains disabled because the address cannot be
  identified safely in every mux configuration.

## SEN66

- Added the `Loom_SEN66` module for the Sensirion SEN66 at address `0x6B`.
- Added measurements for:
  - PM1.0, PM2.5, PM4.0, and PM10.0 mass concentration
  - PM0.5, PM1.0, PM2.5, PM4.0, and PM10.0 number concentration
  - Ambient humidity and temperature
  - VOC and NOx indexes
  - CO2
- Added device reset, continuous-measurement startup, and device-status logging.
- Added temperature-offset configuration.
- Added VOC and NOx algorithm tuning accessors.
- Added error handling for startup, data-ready checks, mass measurements, and
  number-concentration measurements.
- Mass and number-concentration averages use their respective successful sample
  counts, preventing failed reads from biasing the result.
- SEN66 restarts its continuous-measurement sequence after Hypnos rail cycling.

## SEN55

- Added the following optional packaged values:
  - `N_PM0_5`
  - `N_PM1_0`
  - `N_PM2_5`
  - `N_PM4_0`
  - `N_PM10_0`
  - `Typical_Particle_Size`
- Added error handling for measurement startup, PM reads, environmental reads,
  and mode changes.
- PM averages now include only successful sensor reads.
- Data-ready timeouts are safe across `millis()` rollover.
- Existing Loom 4.9 SEN55 mass and environmental field names are unchanged.
- SEN55 re-establishes its I2C driver and presence state after wake without
  unnecessarily resetting VOC/NOx state when power was retained.

## DFRobot MultiGas

- Gas-type names are copied into module-owned storage instead of retaining a
  pointer into a temporary Arduino `String`.
- Initialization now checks the configured sensor address directly.
- Corrected success and retry-count logging.
- Sensor acquire-mode and temperature-compensation arguments are now applied.
- Measurements are skipped when the sensor reports that data is unavailable.
- Added mux integration for addresses `0x74` and `0x75`.
- Added recovery support for sensors that lose power during sleep.
- Mux-loaded MultiGas sensors are marked as rail-cycled and restore their
  acquisition configuration after reconnecting.

## Hypnos and RTC

- Added `setCompileTime(__DATE__, __TIME__)` so a sketch can supply its current
  build timestamp before `hypnos.enable()`.
- Sketch compile time is converted from configured local time to UTC before it
  is stored in the DS3231.
- Retained the RTClib `DateTime` and `TimeSpan` API.
- Added SAMD EIC pending-interrupt clearing before wake interrupts are attached.
- Hypnos now tracks the registered SLEEP interrupt pin rather than assuming pin
  12 or using the first registered interrupt.
- Alarm overruns clear the expired alarm, restore powered modules, and call the
  registered wake callback once to request another sample.
- Removed duplicate interrupt attachment.
- Fixed interrupt-map access that could create an invalid entry before checking
  whether the pin was registered.
- Re-registering an interrupt pin now replaces its stored callback and mode.
- Alarm overrun checks use the exact scheduled `DateTime`, including
  month/year rollover.
- Sleep is aborted safely when no SLEEP interrupt has been registered.
- `networkTimeUpdate()` and `logToSD()` now return their actual success state.
- Corrected MST daylight-saving selection in the existing timezone logic.

## Hypnos SD configuration

- `getConfigFromSD()` now accepts these interval layouts:

```json
{
  "days": 0,
  "hours": 0,
  "minutes": 20,
  "seconds": 0
}
```

```json
{
  "SleepInterval": {
    "days": 0,
    "hours": 0,
    "minutes": 20,
    "seconds": 0
  }
}
```

- The nested keys `sleepInterval` and `sleep_interval` are also accepted.
- Added safe handling for an optional UTF-8 byte-order mark.
- Unknown timezone strings retain the configured timezone and generate a
  warning.
- Missing, invalid, or zero-length intervals fall back to 20 minutes.
- JSON input remains valid until all zero-copy values have been consumed.
- `SDManager::log()` now returns whether the log operation succeeded.
- SD file reads allocate only the required bounded buffer and reject oversized
  files instead of writing past a fixed 5000-byte allocation.
- CSV headers and rows use capacity-aware appends instead of unsafe `strncat`
  lengths.
- The LoRa chip-select pin is configured as an output before SD access drives
  it high.
- Log and batch filenames use bounded formatting and explicit null termination.
- SD initialization remains at the Loom 4.9 4 MHz setting for card stability.

## Analog and battery monitoring

- Added sketch-overridable Analog defaults:
  - `LOOM_ANALOG_BATTERY_PIN`
  - `LOOM_ANALOG_ADC_RESOLUTION_BITS`
  - `LOOM_ANALOG_ADC_REFERENCE_VOLTAGE`
  - `LOOM_ANALOG_BATTERY_DIVIDER_SCALE`
  - `LOOM_ANALOG_BATTERY_SAMPLE_COUNT`
  - `LOOM_ANALOG_ADC_MAX_READING`
- Every default is guarded with `#ifndef` so a project-wide definition can
  override it.
- Battery readings now use configurable, averaged ADC samples.
- Battery voltage is read once per measurement update.
- Missing analog mappings return `0.0f`.
- Analog `_MV` field-name construction is bounded.
- `Loom_Hypnos::checkVoltage()` now:
  - Rejects zero or negative sample counts
  - Uses the Loom Analog configuration
  - Handles volt and millivolt thresholds consistently
  - Updates its voltage-status flags
  - Correctly classifies the previous 3.20-3.35 V gap
- MongoDB batch publishing retains its 3.4 V battery requirement and reports the
  measured voltage when publishing is skipped.

## MongoDB and MQTT

- MongoDB metadata publishing rejects null input and returns the actual publish
  result.
- Retained-message deletion returns the actual MQTT publish result.
- Batch topics include the configured project-server prefix.
- Batch files may use CRLF, CR-only, or LF-only line endings.
- A final batch packet without a trailing newline is published correctly.
- Oversized batch packets are rejected without overflowing the packet buffer.
- MQTT QoS remains 1 by default.

## MS5803 and SmartRock

- MS5803 initialization probes the requested I2C address and calibration PROM.
- Added first-measurement priming.
- Added recovery after I2C and measurement failures.
- Retained the existing Loom 4.9 MS5803 packaged field names.
- Updated SmartRock to use the intended `0x76` MS5803 address.
- Updated SmartRock to use `getConfigFromSD()` and sketch compile-time RTC
  handoff.
- Added top-level and nested SmartRock SD configuration examples.

## MAX31856

- Hardware SPI is now the default.
- Software SPI remains available when MOSI, MISO, and SCLK pins are supplied.
- Corrected the constructor implementation to match the public argument order:
  `chip_select`, `samples`, `mosi`, `miso`, `sclk`.
- Invalid sample counts are clamped to one.
- Only fault-free, non-NaN samples are included in the temperature average.

## Core framework

- Added a virtual `Module` destructor for safe destruction through `Module*`.
- Added an opt-in manager recovery hook used by LTE.
- Added rollover-safe serial wait and pause timing.
- Corrected function-instrumentation macro name expansion.
- Corrected `SLOGF` so formatted silent logs remain silent.
- Preserved the 4.9 watchdog timeout and compile-time watchdog guards.

## Examples

- Added Loom and standalone SARA-R5 bring-up examples.
- Added an R5 serial passthrough example.
- Added R5 fast-registration diagnostics.
- Added SEN66 plus TSL2591 mux testing.
- Added I2C bus-health diagnostics.
- Added SAMD21/Jolteon board and SWD flashing diagnostics.
- Added a packaged ADS1232 driver with bounded ready waits, correct 24-bit data
  retrieval, and the 25th clock required to identify the next conversion.
- Updated the current Evaporometer to use the packaged driver rather than an
  uncompiled example-local source copy.
- Separated the legacy SD-only Evaporometer into its own correctly named sketch
  directory and removed its duplicate delay-before-sleep interval.
- Converted the SDI-12 Evaporometer source into a correctly named sketch and
  aligned its advertised response count with its four returned values.
- Corrected the Evaporometer 2026 primary sketch filename, retained the 4.9
  load-cell excitation enable before sampling, and retained VREF shutdown.
- Added current Wisp batch and mux examples.
- Added SmartRock 2026 and nested configuration examples.
- Added WeatherChimes 4G and credential-file examples.
- Added a separate VCNL2 sketch directory.
- Updated MongoDB and ThingSpeak examples to pass a `NetworkComponent` instead
  of a raw client.
- Corrected the LTEMongoDBBatch syntax error.
- Corrected the AS7263 class name.
- Corrected SmartRock 2.5 configuration loading.
- Corrected LilyPad sensor API and variable-scope issues.
- Updated VCNL and WeatherChimes examples for the current Loom APIs.
- LoRa-to-4G explicitly enables packet proxying with `receive(5000, true)`.
- Moved the STEMMA Adalogger sketch into a correctly named Arduino sketch
  directory. No example directory name ends in `.ino`.
- Corrected the Jolteon UART diagnostic filename so Arduino recognizes it as a
  sketch.

## Data compatibility

Existing Loom 4.9 packaged keys and units are retained. The only new packaged
sensor fields are the optional SEN55 number-concentration and particle-size
values and the new SEN66 module fields described above.

## Verification

The following sketches compile successfully for
`loom4:samd:adafruit_feather_m0`:

- E102 Basic/full-library build
- SEN66 and TSL2591 multiplexer build
- Default SARA-R4 LTE MongoDB batch build
- SARA-R5 build with `LOOM_LTE_USE_SARA_R5`
- Evaporometer V1 fixed build with the packaged ADS1232 driver
- SmartRock with Hypnos and MS5803
- Wisp mux batch logging with Hypnos, SEN55, SEN66/DFgas-capable mux support
- LoRa-to-4G with LTE, MQTT, and MongoDB
