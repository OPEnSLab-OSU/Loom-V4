# Loom 4.9.1

GET NEW PACKAGE DEPENDENCIES HERE (too big for github): https://drive.google.com/file/d/1-3h9KJZLhEqDoYGxGSycEwhwRnLGpojW/view?usp=sharing

Updated libraries in the zip (you cannot get all of these off Arduino library manager!):
- ArduinoMqttClient
- SDS011-master (fixes a build warning present in every sketch)
- ADS1232_Library (fixed and brought up another student's work)
- SparkFun_LTE_Shield_Arduino_Library-master (SARA R5 support, custom edit)
- TinyGSM (adds SARA_R5 profile)

You may also want to update the DS3231 library.

Loom 4.9.1 is a bug-fix and hardware-support release built directly on Loom 4.9. It preserves the 4.9 APIs and packaged field names while correcting sleep, SD, multiplexer, LTE, networking, sensor, and example failures found during field deployment and the full example compile audit.

Comparison used for this changelog: Git tag `v4.9` through branch `4.9-joshfixes`.

## Issue and field-report traceability

| Report | 4.9.1 status and resolution | Primary files/libraries |
| --- | --- | --- |
| [#252 — SARA-R4 to SARA-R5 conversion](https://github.com/OPEnSLab-OSU/Loom-V4/issues/252) | **Addressed in Loom.** Added runtime R4/R5 selection, separate TinyGSM adapters, Jolteon power timing, AT/PDP diagnostics, retry handling, and R5 test sketches without requiring an Arduino CLI-only flag. Updated TinyGSM and LTE support libraries are included in the package-library archive. Final Jolteon end-to-end hardware validation is still recommended before closing the issue. | `src/Internet/Connectivity/Loom_LTE/`; `examples/Lab Examples/Jolteon/`; packaged `TinyGSM` and SparkFun LTE libraries |
| [#268 — SD log bounds checks and Arduino String removal](https://github.com/OPEnSLab-OSU/Loom-V4/issues/268) | **Partially addressed.** SD header, row, filename, and batch-name construction is bounded; initialization/open failures propagate correctly; and file reads use checked, exact-size allocation. Complete removal of temporary Arduino `String` values, CSV escaping, explicit truncation status, FNV schema rotation, and a memory-pool pipeline are not part of 4.9.1. | `src/Hardware/Loom_Hypnos/SDManager.cpp`; `SDManager.h`; `Loom_Hypnos.cpp` |
| [#288 — LoRa + LTE Mongo upload failures](https://github.com/OPEnSLab-OSU/Loom-V4/issues/288) | **Targeted; soak testing required.** MQTT now defaults to QoS 1, uses a Loom-sized transmit buffer, and returns publish/delete status. LTE now separates modem boot, registration, PDP, and socket failures and adds bounded recovery. LoRa no longer treats incomplete fragments as completed messages. The reported multi-day degradation still requires a long-running hub test. | `src/Internet/Logging/MQTTComponent/`; `src/Internet/Connectivity/Loom_LTE/`; `src/Internet/Logging/Loom_MongoDB/`; `src/Radio/Loom_LoRa/` |
| [#290 — AS7263 example typo](https://github.com/OPEnSLab-OSU/Loom-V4/issues/290) | **Addressed.** Replaced the invalid `Loom_AwS7262` type with `Loom_AS7263` and corrected the example constructor documentation. | `examples/Sensors/I2C/AS7263/AS7263.ino` |
| [#291 — Various example compile errors](https://github.com/OPEnSLab-OSU/Loom-V4/issues/291) | **Partially addressed.** Corrected invalid sketch-folder/main-file layouts, duplicate SmartRock entry points, stale class and method names, missing constants, and R5 profile selection. Credential-bearing examples still require local, ignored `arduino_secrets.h` or credential JSON files. | `examples/`; corresponding `src/` fixes; `tests/` compile-audit harness |
| [#299 — MongoDB multi-project support](https://github.com/OPEnSLab-OSU/Loom-V4/issues/299) | **Client side partially addressed.** Batch topics now include `project/database/device`, matching project-aware single-packet publishing. Dynamic MongoDB URI construction remains an external `mqtt-bridge` change. | `src/Internet/Logging/Loom_MongoDB/Loom_MongoDB.cpp`; external `mqtt-bridge` still required |
| [#300 — Multiplexer power-down prevents Hypnos wake](https://github.com/OPEnSLab-OSU/Loom-V4/issues/300) | **Addressed in code.** Multiplexer power-down selects each sensor's owning port, completes its lifecycle call, and explicitly disables all mux channels. Hypnos clears stale RTC/SAMD interrupt state and retains the exact scheduled alarm. The replacement-alarm behavior is hardware-verified; a complete Wisp V2 mux-stack sleep test remains recommended. | `src/Hardware/Loom_Multiplexer/`; `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`; `Loom_Hypnos.h` |
| [#301 — LTE network time 15 hours behind](https://github.com/OPEnSLab-OSU/Loom-V4/issues/301) | **Addressed.** SARA-R4 clock fields are retained as UTC instead of being shifted by the modem timezone a second time before Hypnos updates the DS3231. Network-time methods initialize outputs and return their real success status. Wisp V2 synchronizes during connected startup and whenever a batch window has LTE online. | `src/Internet/Connectivity/Loom_LTE/Loom_LTE.cpp`; `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`; `examples/Lab Examples/Wisp/WispV2_Deploy_2026/` |
| [#302 — SmartRock first-wake freeze](https://github.com/OPEnSLab-OSU/Loom-V4/issues/302) | **Targeted; full stack validation remains in progress.** The Hypnos stale-alarm and interrupt fixes address the first-wake deadlock path and pass the standalone replacement-alarm test. SmartRock now passes sketch compile time, handles nested SD sleep configuration, allows rail settling, starts the ADS1115 I²C bus reliably, and uses corrected MS5803 initialization. | `src/Hardware/Loom_Hypnos/`; `src/Sensors/I2C/Loom_ADS1115/`; `src/Sensors/I2C/Loom_MS5803/`; `examples/Lab Examples/SmartRock/` |

### July 4.9 team bug roundup

| Team complaint | 4.9.1 status and resolution | Primary files/libraries |
| --- | --- | --- |
| Wisp: DS3231 Alarm 1 was not cleared after the RTClib migration | **Addressed and hardware-verified.** Alarm sources are disabled and cleared before replacement, the exact alarm `DateTime` is retained, pending EIC/NVIC state is cleared, and Hypnos does not reattach the active-low RTC interrupt while its line is asserted. | `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`; `Loom_Hypnos.h` |
| Wisp/Chimes: SEN66, SHT31, and three DF Multi-Gas sensors behind a mux | **Addressed in the Loom integration.** Added SEN66, both DF gas addresses, verified mux selection, per-port names, refresh/retry behavior, and channel shutdown. Failed auto-loads are discarded rather than retained as stale sensor objects. | `src/Sensors/I2C/Loom_SEN66/`; `src/Sensors/I2C/Loom_DFMultiGasSensor/`; `src/Hardware/Loom_Multiplexer/` |
| Wisp: local DFRobot Multi-Gas patch was required | **Partially addressed.** The Loom wrapper honors its configured address and acquisition/temperature-compensation arguments, owns the gas-type string, reconnects after rail cycles, and retains the previous sample when new data is unavailable. Unspecified external-library patch behavior is not claimed. | `src/Sensors/I2C/Loom_DFMultiGasSensor/`; packaged `DFRobot_MultiGasSensor` dependency |
| WiFiMongoDBBatch: false low-battery warning and approximately 2.7 V readings | **Addressed.** Battery reads set ADC resolution before sampling, discard the first conversion, average samples, and use the correct 12-bit maximum. Mongo batch gating performs the corrected direct reading and does not depend on a registered `Loom_Analog` object. | `src/Sensors/Loom_Analog/`; `src/Internet/Logging/Loom_MongoDB/Loom_MongoDB.cpp` |
| SmartRock: freeze on first wake with the EC/I2C board attached or longer sleep intervals | **Targeted; stack validation recommended.** Hypnos wake handling is hardware-verified independently, and the SmartRock configuration, rail-settling, and MS5803 paths are corrected. | `src/Hardware/Loom_Hypnos/`; `src/Sensors/I2C/Loom_MS5803/`; `examples/Lab Examples/SmartRock/` |
| Slow first compile and hundreds of warnings exposed by the CLI audit | **Partially addressed.** Loom-owned errors, missing returns, signedness warnings, unsafe formatting, and unused locals were cleaned up. The audit separates Loom and external warnings. Dependency pruning remains future work. | Broad `src/` and `examples/` cleanup; `tests/` audit tooling |

### Open issues reviewed but not claimed by 4.9.1

| Issue | Status in this release |
| --- | --- |
| [#220 — LTE location metadata](https://github.com/OPEnSLab-OSU/Loom-V4/issues/220) | Not implemented. LTE identity, diagnostics, and connectivity changed, but location acquisition and CSV/metadata integration were not added. |
| [#267 — Memory pools](https://github.com/OPEnSLab-OSU/Loom-V4/issues/267) | Not implemented. This release reduces several unsafe allocations but does not add a shared deterministic pool. |
| [#271 — LoRa groups and scheduling](https://github.com/OPEnSLab-OSU/Loom-V4/issues/271) | Not implemented. Fragment handling was corrected, but address partitioning and time-slot scheduling were not added. |
| [#272 — Sensor dependency review](https://github.com/OPEnSLab-OSU/Loom-V4/issues/272) | Updated dependency replacements are packaged, but the full review and removal of unused libraries is not complete. |
| [#273](https://github.com/OPEnSLab-OSU/Loom-V4/issues/273) / [#281](https://github.com/OPEnSLab-OSU/Loom-V4/issues/281) — Manager idle/standby mode | Not implemented. Existing full power-down and Hypnos standby behavior was repaired; a separate manager idle mode was not introduced. |
| [#287 — Logger filename truncation](https://github.com/OPEnSLab-OSU/Loom-V4/issues/287) | The reported missing first character is already corrected in the 4.9 baseline. No separate 4.9.1 behavior change is claimed. |

## Core correctness and compatibility

- Preserved the established 4.9 packaged field names. This release does not require a repository-wide downstream database or dashboard key migration.
- Added only new SEN55/SEN66 particle-number and environmental outputs where the earlier modules did not provide those values.
- Corrected logger macro expansion and retained `SLOGF` silent/SD-log behavior.
- Corrected inaccurate or missing return values across network, radio, SD, and sensor paths so callers receive actual operation status.
- Added a module retry hook so devices that failed initialization because their power rail was unavailable can retry during `power_up()`.
- Standardized watchdog wrapper use around long initialization, LTE, and lifecycle operations.
- Corrected actuator cleanup and power-down behavior, including releasing dynamically owned display/driver objects and avoiding invalid empty method definitions.

## Hypnos, RTC, sleep, and SD

- Corrected RTC interrupt registration and reattachment so Hypnos tracks the real pin/callback pair, avoids duplicate handlers, clears stale DS3231 and SAMD state, and detaches the correct wake interrupt.
- Prevented an asserted DS3231 active-low interrupt from immediately retriggering when a replacement alarm is attached.
- Retained the exact scheduled alarm `DateTime`, fixing month/year boundary comparisons and alarm-overrun detection.
- Rejected zero-length alarms and sleep attempts without a registered wake interrupt.
- Added `setCompileTime(__DATE__, __TIME__)` so sketches can supply their own build timestamp, for quicker testing when needing to supply hypnos with current time.
- Corrected timezone/DST handling, network-time result propagation, and zero-padded ISO timestamps.
- Expanded SD sleep configuration parsing to accept root or nested `SleepInterval`, `sleepInterval`, and `sleep_interval` objects, tolerate a UTF-8 BOM, validate timezone values, and use a safe fallback for missing/invalid configuration.
- Hardened SD filenames, CSV rows, and batch filenames against overflow. SD initialization, logging, and file-open results now report actual success.
- Separated current SD-card reachability from session filename selection so a transient card initialization failure during wake resumes the same CSV instead of advancing to a new numbered file.
- Reworked `readFile()` to reject oversized files, allocate only the required buffer, null-terminate it, and return `nullptr` safely on failures.
- Retained and corrected `Loom_Hypnos::checkVoltage()`: configurable Loom analog settings are used, threshold classifications have no gaps, and voltage flags are updated consistently.
- Kept SD at 4 MHz for cross-card and shared-bus stability.

## I2C multiplexer and sensors

- Reworked TCA9548 discovery to verify the control register instead of accepting any I2C ACK as a mux.
- Added configurable known sensor addresses, alternate mux addresses, port enable/disable selection, scan diagnostics, and per-sensor auto-load options.
- Added safe sensor refresh/reload after Hypnos rail cycles. Old objects and stale address state are cleared before rescanning, and failed objects are deleted.
- Ensured every lifecycle operation selects the sensor's port and checks the mux transaction before use.
- Explicitly closes all mux channels during power-down, resolving the shared-bus sleep hang path.
- Added mux support for SEN66, SEN55, both DF Multi-Gas addresses, TSL2591 options, and MMA8451 address `0x1C`.
- Added a complete SEN66 Loom module with particulate, particle-count, humidity, temperature, VOC, NOx, and CO2 packaging.
- Extended SEN55 with number concentrations and typical particle size and improved measurement error handling.
- Corrected DF Multi-Gas address/configuration ownership, rail-cycle reconnection, and unavailable-sample behavior.
- Corrected MS5803 address detection, PROM/CRC validation, initialization cleanup, and measurement readiness behavior.
- Made ADS1115 initialize the shared I²C controller itself, discover valid `0x48`–`0x4B` address straps, restore its configured state after wake, and avoid repeated allocating `begin()` calls.
- Added TSL2591 `power_up()` and `power_down()` lifecycle methods for current WeatherChimes and mux workflows.
- Corrected smaller sensor defects in ADS1115, K30, MB1232, MMA8451, STEMMA, T6793, VCNL4020, Digital, SDI-12, and MAX31856 paths.

## Analog and ADS1232

- Configured the SAMD ADC before battery sampling, discarded the first conversion after configuration, and averaged repeated samples.
- Corrected battery conversion to use the 12-bit ADC maximum and configurable reference, divider, pin, resolution, and sample-count constants guarded with `#ifndef`.
- Added a parameterized static battery-reading path for MongoDB gating and Hypnos voltage checks.
- Added a bounded ADS1232 implementation for Evaporometer workflows with ready timeouts, controlled discard/settling reads, and explicit failure status instead of indefinite blocking.

## LTE, networking, MQTT, and MongoDB

- Preserved SARA-R4 as the compatibility default while allowing each Arduino sketch to select `LTE_MODEM::SARA_R4` or `LTE_MODEM::SARA_R5` at runtime.
- Added separate TinyGSM R4/R5 adapters so Arduino IDE sketches do not require a library-wide compiler flag.
- Added Jolteon R5 power polarity/timing, 115200-baud startup, optional baud diagnostics, and optional reset recovery while retaining the proven R4 9600-baud path.
- Split modem boot/AT readiness from SIM, registration, APN/PDP, and TCP verification so field logs identify the failing stage.
- Added bounded boot and connection retries, stale-PDP cleanup, explicit PDP configuration, raw AT diagnostics, and serial passthrough support.
- Corrected LTE credential ownership and JSON parsing so temporary strings cannot leave dangling pointers or unterminated buffers.
- Corrected SARA-R4 network-time handling so its UTC clock is not shifted by the timezone offset a second time; initialized every time output before use.
- Updated Wisp V2 deployment timing to sync its RTC while LTE is connected at startup and during batch upload windows; intentionally offline checks between batches now exit quietly.
- Hardware-validated SARA-R4 recovery: the R410M answered AT at 9600, recovered from an initial TinyGSM initialization miss, registered, obtained an IP address, and reached a remote HTTP server.
- Hardened Wi-Fi credential copies, AP fallback names, default client mode, and failed verification returns.
- Corrected Ethernet status and network-time result handling.
- Changed default MQTT QoS from 2 to 1 and increased its transmit payload capacity to `MAX_JSON_SIZE`.
- Corrected retained-message deletion to return publish status.
- Added project-aware MongoDB batch topics, CR/LF/CRLF handling, oversized-line rejection, and publication of the final unterminated line.
- Corrected MongoDB metadata status propagation and used the fixed direct battery reading for low-voltage batch gating.
- Corrected LoRa incomplete-fragment handling and missing batch-send status returns.

## Examples and field workflows

- Added Jolteon examples for Loom and non-Loom SARA-R5 bring-up, AT passthrough, registration diagnostics, I2C bus health, SEN66/TSL2591 mux testing, SAMD21J18 board checks, and SWD flashing.
- Added a Hypnos ADC/alarm/standby hardware validation sketch. The analog and replacement-alarm tests now pass on hardware.
- Added an explicit SARA-R4 + Hypnos example that enables the power rails before LTE initialization.
- Added a corrected Evaporometer V1 workflow with bounded ADS1232 reads, channel-settling/discard cycles, sketch compile-time RTC recovery, stable Hypnos log filenames, and SPI restoration before SD writes.
- Updated SmartRock examples for Hypnos 3.3, nested SD sleep settings, MS5803 addressing, VCNL4010 readings, calibrated conductivity/turbidity output, and a single valid Arduino entry point.
- Added a WeatherChimes 4G example and updated LTE/MongoDB deployment examples for corrected connection and configuration behavior.
- Updated Wisp, WeedWarden, LilyPad, LoRa-to-4G, tipping-bucket, DF Multi-Gas, VCNL, Dendrometer, and related examples to match current module APIs and wiring.
- Normalized Arduino sketch folder and primary `.ino` names so Arduino IDE and CLI discovery no longer sees missing main files or duplicate entry points.

## Compile-audit tooling

- Added a Windows/Arduino CLI harness for full, no-binary, smoke, Jolteon-R5, and retry-failed audits.
- Added per-sketch timing, warning, Loom/internal warning, external warning, and error summaries.
- Added retry-manifest generation from the newest audit logs so only relevant failures need to be rebuilt.
- Added temporary warning-scope instrumentation support with one-click strip and restore commands.
- Preserved the exact stripped warning-scope state in `tests/.warning_scope_state/restore.patch` plus a readable 58-file manifest. The restore patch remains visible to Git and validates against the stripped source.
- Kept generated compile audit folders, retry manifests, credentials, and binaries out of version control.

## Packaged libraries and distribution

- Updated the 4.9 package-index metadata and retained the upstream QoS 1 correction merged after the `v4.9` tag.
- Added `Updated_Package_Libraries(place_one_level_above).zip` containing updated `TinyGSM`, SparkFun LTE Shield, ADS1232, and SDS011 libraries for the board-package library level.
- The package archive must be extracted one directory above Loom so the Arduino board package uses the updated dependencies during IDE builds.

## Validation status

- **Passed:** Hypnos analog voltage sampling on hardware.
- **Passed:** DS3231 first alarm, alarm clearing/replacement, and repeated Hypnos standby wake behavior on hardware.
- **Passed:** SARA-R4 AT initialization, network registration, PDP/IP acquisition, and outbound HTTP reachability on hardware.
- **Passed:** Smartrock flash and wake/log cycle, checking that it's not randomly making new files as we introduced that last night pre-4.9 bringup.
- **Passed:** Wisp V2 Deploy sketch has correct timestamps.
- **Compile coverage:** the audit harness discovers the complete example tree and the 4.9.1 fixes address the Loom-owned failures found during the 123-sketch run.
- **Still recommended:** a multi-day LoRa/LTE/MongoDB soak test, a complete Wisp V2 SEN66/DF-gas mux sleep test, final SmartRock EC-board wake validation, and final Jolteon SARA-R5 end-to-end validation.
