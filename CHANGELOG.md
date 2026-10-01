# Loom 4.9.1

LTE now keeps its RSSI field in every packet while deliberately powered off between batch
uploads, using null for an unavailable reading without querying the modem. Normal wakes
therefore keep the same CSV columns and append to the current file. Debug output and function
summaries use the fixed session number, independently of legitimate CSV schema rotation.
Hypnos adds an opt-in interval-change overload that preserves the last due wake as the anchor;
the existing one-argument scheduling API retains its behavior. Gas wake configuration checks
the acquisition-mode acknowledgement and labels individual I2C configuration calls in traces.
The active retained-rail bench sketch reuses gas-board configuration on normal wakes, settles
SEN66 before starting its timed sleeps, and reports actual saved sample-timestamp intervals.
Trace filenames now share the immutable debug/data session number. New sessions account for
existing trace names as well as CSV/batch files; legacy captures are preserved without renaming.

Optional Wisp debug call/heap recording now saves directly loadable Chrome JSON and detailed
NDJSON records to SD for Perfetto and an offline
live-allocation inspector. Off/calls/heap modes keep the default extra toggle off; the recorder
uses a fixed buffer, checked SD appends, explicit loss/coverage labels, and separate recording
overhead. See `docs/TRACE_DEBUGGING.md` for scope, baseline limitations, and verification.

GET NEW PACKAGE DEPENDENCIES HERE (too big for github): https://drive.google.com/file/d/1-3h9KJZLhEqDoYGxGSycEwhwRnLGpojW/view?usp=sharing

Updated libraries in the zip (you cannot get all of these off Arduino library manager!):
- OPEnS_RTC (hardened no-allocation DS3231 fork for Feather M0/Hypnos)
- ArduinoMqttClient
- SDS011-master (fixes a build warning present in every sketch)
- ADS1232_Library (fixed and brought up another student's work)
- SparkFun_LTE_Shield_Arduino_Library-master (SARA R5 support, custom edit)
- TinyGSM (adds SARA_R5 profile)
- SparkFun AS726X (bounded virtual-register polling)
- SparkFun Spectral Triad AS7265X (bounded virtual-register polling)

The `loom4:samd` platform package uses the checksum-verified official Loom 4.9 Wire/SERCOM core.
The bounded AS726x changes are confined to the reviewed third-party dependencies and Loom wrappers.
See `docs/PLATFORM_PATCH_MANIFEST.md` for exact package inputs and beta validation requirements.

The modified OPEnS_RTC, SparkFun AS726X, and SparkFun Spectral Triad AS7265X sources are now
vendored under `dependencies/`. Release archives must promote byte-equivalent copies into the
board package's top-level `libraries` directory; nested copies alone are not discoverable reliably
by Arduino. The spectral wrappers validate Loom patch-level markers so a stale dependency fails
with an explicit message rather than a private-method compiler error.

An experimental SAMD21 `SERCOM.cpp`/`Wire.cpp` timeout edit is retained under
`dependencies/Loom_SAMD21_Core_Patches` as inactive investigation notes. It must not be promoted;
the release verifier instead enforces the official Loom 4.9 core hashes.

Use the packaged OPEnS_RTC dependency instead of Adafruit RTClib for this stable re-release.

Loom 4.9.1 is a bug-fix and hardware-support release built directly on Loom 4.9. It preserves the 4.9 APIs and packaged field names while correcting sleep, SD, multiplexer, LTE, networking, sensor, and example failures found during field deployment and the full example compile audit.

The sensor/card timing pass validates SDI-12 M! replies, waits the advertised measurement time,
reads all D blocks, accepts adjacent signed values and rejects stale/noisy discovery responses.
TEROS 21 adds matric potential and TEROS 54 adds four depths; GS3/TER11/TER12 labels/order are
preserved. Existing TER11/TER12 water-content labels still contain calibrated ADC counts; selecting
soil calibration remains explicit future work. ADS1115 now checks I2C transfers, limits conversion
polling to 25 ms, fixes differential-only reads, avoids the vendor helper allocation, and adds
opt-in channel/voltage-column controls while retaining its default schema. Three sensor headers
forward-declare Manager instead of exposing its packet dependency.

SEN66 now waits the documented typical 30-second PM startup before sampling, retains running
measurement on a powered sensor rail, checks raw unavailable sentinels per field and averages
independently. The installed driver's 1000 ms stop delay is extended to the current 1400 ms
requirement; duplicate reset delay is removed. Cold-start awake time increases; gas learning,
physical power behavior and field accuracy remain bench concerns. Manufacturer references,
local card status and remaining acceptance work are consolidated in `docs/ISSUE_TALLY.md`.
Compilation was authorized earlier on 2026-09-30; host parser/averaging, watchdog, CSV, ADC
and reed-switch regressions passed at those revisions. Compilers are now paused again; those
results do not validate subsequent source changes or physical behavior.

The follow-through adds opt-in Analog raw/millivolt columns (both remain enabled by default),
separates its Manager/JSON implementation dependency, validates seven-bit I2C addresses before
narrowing, and gives the AS5311 reader portable constant-time parity checking. Three E102 examples
now explicitly include Hypnos. A small, allocation-free reed-switch anemometer samples an explicitly
calibrated two-second interrupt window; it adds no weather-kit dependency. Its example documents
SparkFun SEN-15901 calibration and excludes powered analog-output hardware. Calibration/contact
bounce and interrupt wiring still need physical validation. Manual CI formatting now fails on errors
instead of silently succeeding.

The compatibility-preserving professionalism pass also bounds string/config parsing; removes the
duplicate 2 KB OLED, 1 KB Max, and 1 KB Freewave JSON workspaces; removes LoRa/Freewave manager heap
allocations; corrects retained MQTT and EZO payload handling; makes hardware ownership explicit;
and rejects malformed commands and partial file reads. See `docs/COMPATIBILITY_CONTRACT.md` for the
storage, topic, and wire-format invariants retained by these changes.

The Cortex-M0 second pass removes recurring Digital map and tipping-bucket deque allocations,
retains one lazy LoRa fragment workspace, reduces RemoteManager/ThingSpeak stack peaks, and bounds
AS726x measurement and vendored virtual-register waits. WISP and
Dendrometer sketches emit heap/stack lifecycle checkpoints to Serial without changing stored
packets, and WISP supplies a non-interactive compile-time RTC fallback using the corrected DST path.
Actuator names now use the base module's single fixed buffer, formatting-only 256-byte caller
buffers were removed from several communication/sensor paths, and MQTT keep-alive configuration now
updates the client instead of retaining an unused field.

The final beta pass makes SD rows and batch records transactional at the file boundary, retains a
pending batch until every MongoDB/LoRa record succeeds, and retries connectivity above the batch
threshold instead of only at one exact count. It adds JSON-pool overflow diagnostics, pre-reserves
multiplexer discovery capacity, and gives ThingSpeak's formerly undefined batch overload a safe
`false` result. These changes preserve JSON labels, CSV ordering, numbered filenames, MQTT topics,
and radio framing.

Comparison used for this changelog: Git tag `v4.9` through branch `4.9-joshfixes`.

### 2026-09-29 SD/network/Dendrometer follow-through (not compiled)

- **#207:** share a checked sync/rollback/close finalizer between batch and text/pretty-JSON debug
  appends. Preserve uncertain debug files and stop SD debug output for that boot; skip summaries
  before SD initialization. Remove the five-second stall from unavailable optional debug logging.
  This hardens software failure handling; it does not establish the root cause of FAT corruption.
- **#288:** share MQTT frame start/finish checks across text, JSON and stream publishing, recheck
  connection after poll, and close failed frames before a later reconnect. Restore cellular PDP
  before retrying TCP; preserve watchdog state around blocking network operations. Give MongoDB
  the physical Feather's stable MQTT client ID; verify the broker's 32-character/ACL support.
- **#349:** correct the Dendrometer node's ambiguous LoRa constructor, optional includes/radio path,
  unbounded alignment/Serial startup, bad AS5311 conversion handling and position rollover average.
  Make the hub queue/retry on SD and preserve each queued node's topic with an explicit opt-in;
  restore hub identity for heartbeats and remove the missing private secrets-header prerequisite.
  The card's AS5111/checked-in AS5311 discrepancy and 4.8 hardware compatibility remain unresolved.
- **#318:** source review finds exact/yearly North American transition rules, MST and UTC/local/SD
  timezone separation already present. Extend deferred tests to the card's 2027-2029 boundaries.
- **#345/#341:** add file/offset/topic/length diagnostics and precise broker-ACK messages. Server
  receipt/insert-time evidence and bounded broker/database logging remain external follow-ups.
- Source-only warning scopes, quiet/debug pairs, diagnostic boundaries, installed dependency/core
  hashes and formatting passed. Additional fault/rollover/DST tests are written but uncompiled and
  unexecuted. No compilers, firmware uploads, server changes or GitHub card mutations were used.

### 2026-09-29 major task-board candidates (not compiled)

Major-card work resumed at the user's request. Current per-card status and remaining work
are consolidated in `docs/ISSUE_TALLY.md`.

- **#353 / LoRa:** bound the whole receive loop by elapsed time and datagram count; expire
  abandoned fragments, distinguish fresh full/custom packets from module fragments, track batch
  counts by sender and release failed blocking batch loops. Reject oversized/truncated outbound
  bodies and headers while retaining existing padded RadioHead framing.
- **#268 / SD CSV:** stream correct CSV quoting for text/nested JSON, compare existing headers
  exactly and rotate CSV independently from pending upload batches. Use actual file size for new
  headers, check header byte writes and preserve uncertain append/close outcomes before using a
  fresh CSV. Ordinary cells, column ordering and filename patterns remain unchanged.
- **#328 / sampling:** add opt-in `Hypnos::setSampleInterval()` with a fixed UTC grid, skipped
  overrun slots and shared checked alarm arming. All three SmartRock examples now schedule before
  measurement. `setInterruptDuration()` keeps its existing relative rest-after-work meaning.
- **#346 / SEN55:** discard startup PM readings for 30 seconds before the existing averaging
  window; mark missing/current-cycle failures as unavailable instead of zero/stale readings.
  Retain real zero values and existing field labels. Query firmware before direct mode switching;
  older/unknown firmware keeps PM running between reads rather than resetting gas-learning state.
  This adds awake-time/power cost and does not prove long-term optical degradation is resolved.
- **Validation:** source-only warning headers, four quiet/debug pairs, diagnostic boundaries,
  reviewed dependency/official core integrity and source formatting passed. Added portable
  regression cases remain uncompiled and unexecuted. Radio/SD/sampling/sensor integration, hardware
  soak, golden outputs and RAM/stack measurements remain pending. No GitHub cards were closed.

### 2026-09-29 restore comparison and focused source fixes (not compiled)

All 15 `4.8-restore` remote heads were compared by SHA/history/source. Existing DST, UTC/local
separation, ISO formatting, lower SD speed, and memory fixes were distinguished from missing
features and incompatible older designs. Significant fixes were selectively applied instead
of importing a whole legacy branch. The latest saved 55-card membership and remaining work
are consolidated in `docs/ISSUE_TALLY.md`.

- **Watchdog/sleep:** suspend even runtime-enabled SAMD watchdogs during standby and bounded LTE
  operations, then restore the retained configuration on every return. Add opt-in Serial feed
  call-site tracing; retain existing no-argument progress callbacks and function start/end cues.
- **Reset health:** record raw hardware causes, Feather serial, intent status, and CSV session in
  a separate boot JSONL journal, independent of ordinary SD debug logging. Add an explicit Hypnos
  reset-request method that commits SD intent before resetting, or stays awake on a write failure.
  This does not introduce automatic resets or claim every field watchdog issue is resolved.
- **LTE:** disconnect before shutdown, honor its acknowledgment, probe uncertain power state
  before another power pulse, avoid off-modem connectivity queries, and bound response readers
  under continuous UART input. Keep the R4/R5 adapters and existing batch/data contracts.
- **Configuration/time:** reject invalid original UTC fields before narrowing, checked-accumulate
  SD interval seconds, and reject malformed MQTT configuration before copying null strings.
- **Core boundaries:** isolate SD reset diagnostics in their own source, split analog settings
  from the sensor implementation, forward-declare the network interface, lazily initialize the
  hardware ID for early SD startup, and guard SD-disabled convenience methods.

Source-only preflight and formatting were used. New portable date/interval and watchdog-pause
regression cases are written but not run. No compiler, firmware upload, or board soak was run;
RAM measurements and hardware/fault validation remain deferred.

Line-level review anchors below use line numbers from `4.9-joshfixes`; they identify the main implementation entry points, while the accompanying file bundles describe the complete affected surface.

## Issue and field-report traceability

All paths below are relative to the Loom repository root. “Implementation” identifies files that change runtime behavior; “validation/examples” identifies sketches or audit files used to exercise that fix. Every repository path listed in these two tables is changed from `v4.9` unless the entry explicitly says it is an unchanged validation call site.

| Report | 4.9.1 status and resolution | Exact implementation and validation files |
| --- | --- | --- |
| [#252 — SARA-R4 to SARA-R5 conversion](https://github.com/OPEnSLab-OSU/Loom-V4/issues/252) | **Addressed in Loom.** Added runtime R4/R5 selection, separate TinyGSM adapters, Jolteon power timing, AT/PDP diagnostics, retry handling, and Arduino IDE-compatible R5 sketches. Updated TinyGSM and SparkFun LTE libraries are included in the package archive. Final Jolteon end-to-end hardware validation is still recommended. | **Implementation:** `src/Internet/Connectivity/Loom_LTE/Loom_LTE.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE.h`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_Config.h`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_Modem.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_Modem.h`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_SaraR4.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_SaraR5.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_TinyGsmAdapter.h`.<br>**Validation/examples:** `examples/Lab Examples/Jolteon/Loomified_LTE_R5_example/Loomified_LTE_R5_example.ino`, `examples/Lab Examples/Jolteon/Loomified_LTE_R5_debug_passthrough/Loomified_LTE_R5_debug_passthrough.ino`, `examples/Lab Examples/Jolteon/SARA_R5_Test_No_Loom/SARA_R5_Test_No_Loom.ino`, `examples/Lab Examples/Jolteon/WC_FastRegisterR5Compat/WC_FastRegisterR5Compat.ino`; packaged `TinyGSM` and `SparkFun_LTE_Shield_Arduino_Library-master`. |
| [#268 — SD log bounds checks and Arduino String removal](https://github.com/OPEnSLab-OSU/Loom-V4/issues/268) | **Partially addressed.** Bounded SD header, row, filename, and batch-name construction; corrected initialization/open status; checked exact-size reads; and rolled failed row/record writes back to the prior file size. Later source candidates add CSV escaping, exact header comparison/rotation and optional row checksums. Complete dependency-level `String` removal and a framework-wide memory-pool pipeline remain outside this pass. | **Implementation:** `src/Hardware/Loom_Hypnos/SDManager.cpp`, `src/Hardware/Loom_Hypnos/SDManager.h`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h`.<br>**Validation/examples:** `examples/Lab Examples/SmartRock/SmartRock/SD_config_nested_example.json`, `examples/Lab Examples/Evaporometer/Evaporometer_V1_fixed/Evaporometer_V1_fixed.ino`. |
| [#288 — LoRa + LTE Mongo upload failures](https://github.com/OPEnSLab-OSU/Loom-V4/issues/288) | **Targeted; soak testing required.** MQTT uses QoS 1, supports Loom-sized payloads, and propagates publish/delete status. LTE separates boot, registration, PDP, and socket failures. LoRa rejects incomplete fragment sets. The reported multi-day degradation still requires a long-running hub test. | **Implementation:** `src/Internet/Logging/MQTTComponent/MQTTComponent.cpp`, `src/Internet/Logging/MQTTComponent/MQTTComponent.h`; `src/Internet/Logging/Loom_MongoDB/Loom_MongoDB.cpp`; `src/Internet/Connectivity/Loom_LTE/Loom_LTE.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE.h`; `src/Radio/Loom_LoRa/Loom_LoRa.cpp`.<br>**Validation/examples:** `examples/Lab Examples/LoRa_To_4G/LoRa_To_4G.ino`, `examples/Internet/Logging/LTEMongoDBBatch/LTEMongoDBBatch.ino`. |
| [#290 — AS7263 example typo](https://github.com/OPEnSLab-OSU/Loom-V4/issues/290) | **Addressed.** Replaced the invalid `Loom_AwS7262` type with `Loom_AS7263` and corrected the constructor documentation. | **Example:** `examples/Sensors/I2C/AS7263/AS7263.ino`. |
| [#291 — Various example compile errors](https://github.com/OPEnSLab-OSU/Loom-V4/issues/291) | **Partially addressed.** Corrected invalid sketch-folder/main-file layouts, duplicate SmartRock entry points, stale class/method names, missing constants, and R5 selection. Credential-bearing examples still require local ignored secrets. | **Renamed/fixed sketches:** `examples/ClassExamples/E102/Adalogger_i2cSensorsSD_STEMMA/Adalogger_i2cSensorsSD_STEMMA.ino`; `examples/Lab Examples/MultipleInterrupts/MultipleInterrupts.ino`; `examples/Lab Examples/SmartRock/SmartRock_2026/SmartRock_2026.ino`; `examples/Lab Examples/WC_FastRegister/WC_FastRegister.ino`; `examples/Lab Examples/WeatherChimes/Configurable_Chime_Code_2026/Configurable_Chime_Code_2026.ino`; `examples/Lab Examples/WeatherChimes/Cumulative_Chimes_Code_2025/Cumulative_Chimes_Code_2025.ino`; `examples/Lab Examples/Wisp/Wisp_Batch_Logging/Wisp_Batch_Logging.ino`; `examples/Sensors/I2C/VCNL2/VCNL2.ino`.<br>**Audit files:** `tests/loom_compile_engine.bat`, `tests/loom_compile_audit_no_bins.bat`, `tests/loom_compile_retry_failed.bat`, `tests/loom_retry_failed.ps1`. |
| [#299 — MongoDB multi-project support](https://github.com/OPEnSLab-OSU/Loom-V4/issues/299) | **Client routing checked; server acceptance pending.** Batch topics include `project/database/device`, matching project-aware single-packet publishing. The upstream discussion reports cached dynamic server routing working across projects; that production bridge is external to this repository. | **Implementation:** `src/Internet/Logging/Loom_MongoDB/Loom_MongoDB.cpp`.<br>**Validation:** `tests/Core_Boundaries/test_mongo_batch.cpp`.<br>**External:** verify actual receipts and MongoDB inserts against the deployed `mqtt-bridge`; no replacement server change is implied. |
| [#300 — Multiplexer power-down prevents Hypnos wake](https://github.com/OPEnSLab-OSU/Loom-V4/issues/300) | **Addressed in code.** Multiplexer lifecycle calls select the owning port, failed selections stop the operation, and power-down closes every channel. Hypnos clears stale RTC/SAMD interrupt state and retains the exact scheduled alarm. | **Implementation:** `src/Hardware/Loom_Multiplexer/Loom_Multiplexer.cpp`, `src/Hardware/Loom_Multiplexer/Loom_Multiplexer.h`; `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h`; `src/Module.h`, `src/Loom_Manager.cpp`.<br>**Validation/examples:** `examples/Lab Examples/Wisp/Wisp_Mux_BatchLogging/Wisp_Mux_BatchLogging.ino`, `examples/Lab Examples/Jolteon/debug_hypnos_adc_sleep_test/debug_hypnos_adc_sleep_test.ino`. |
| [#301 — LTE network time 15 hours behind](https://github.com/OPEnSLab-OSU/Loom-V4/issues/301) | **Addressed.** SARA-R4 clock fields remain UTC instead of receiving a second timezone shift before Hypnos updates the DS3231. Offline batch intervals now skip synchronization quietly; connected startup and upload windows refresh the RTC. | **Implementation:** `src/Internet/Connectivity/Loom_LTE/Loom_LTE.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE.h`; `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h`; `src/Internet/Connectivity/NetworkComponent.h`.<br>**Existing validation call site (unchanged from `v4.9`):** `examples/Lab Examples/Wisp/WispV2_Deploy_2026/WispV2_Deploy_2026.ino`. |
| [#302 — SmartRock first-wake freeze](https://github.com/OPEnSLab-OSU/Loom-V4/issues/302) | **Targeted; full-stack validation remains in progress.** Corrected Hypnos alarm/wake state, SmartRock compile-time and nested SD configuration, rail settling, ADS1115 bus/address startup, and MS5803 initialization. The absent MS5803/VCNL sensors in the latest bench run are not claimed as validated. | **Implementation:** `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h`, `src/Hardware/Loom_Hypnos/SDManager.cpp`, `src/Hardware/Loom_Hypnos/SDManager.h`; `src/Sensors/I2C/Loom_ADS1115/Loom_ADS1115.cpp`; `src/Sensors/I2C/Loom_MS5803/Loom_MS5803.cpp`, `src/Sensors/I2C/Loom_MS5803/Loom_MS5803.h`.<br>**Examples/config:** `examples/Lab Examples/SmartRock/SmartRock/SmartRock.ino`, `examples/Lab Examples/SmartRock/SmartRock/SD_config_nested_example.json`, `examples/Lab Examples/SmartRock/SmartRock2.5/SmartRock2.5.ino`, `examples/Lab Examples/SmartRock/SmartRock_2026/SmartRock_2026.ino`.<br>**Hardware validation:** `examples/Lab Examples/Jolteon/debug_hypnos_adc_sleep_test/debug_hypnos_adc_sleep_test.ino`. |

### July 4.9 team bug roundup

| Team complaint | 4.9.1 status and resolution | Exact implementation and validation files |
| --- | --- | --- |
| Wisp: DS3231 Alarm 1 was not cleared after the RTClib migration | **Addressed in code; hardened fork needs soak validation.** Restores the field-proven OPEnS direct-`Wire`/contiguous arm sequence without dynamic allocation, while retaining checked I2C, alarm readback, exact scheduled `DateTime`, and active-low interrupt guards. | **Implementation:** `dependencies/OPEnS_RTC`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h`.<br>**Validation:** `tests/OPEnS_RTC_Compatibility/OPEnS_RTC_Compatibility.ino`, `tests/Hypnos_DST_Boundaries/Hypnos_DST_Boundaries.ino`.<br>**Hardware soak:** still required on Feather M0 + Hypnos. |
| Wisp/Chimes: SEN66, SHT31, and three DF Multi-Gas sensors behind a mux | **Addressed in the Loom integration.** Added SEN66, both DF-gas addresses, verified port selection, per-port names, refresh/retry behavior, and channel shutdown. Failed auto-loads are deleted rather than retained. | **Implementation:** `src/Hardware/Loom_Multiplexer/Loom_Multiplexer.cpp`, `src/Hardware/Loom_Multiplexer/Loom_Multiplexer.h`; `src/Sensors/I2C/Loom_SEN66/Loom_SEN66.cpp`, `src/Sensors/I2C/Loom_SEN66/Loom_SEN66.h`; `src/Sensors/I2C/Loom_SEN55/Loom_SEN55.cpp`, `src/Sensors/I2C/Loom_SEN55/Loom_SEN55.h`; `src/Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.cpp`, `src/Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h`.<br>**Examples:** `examples/Lab Examples/Wisp/Wisp_Mux_BatchLogging/Wisp_Mux_BatchLogging.ino`, `examples/Lab Examples/Jolteon/Jolteon_TSL_Sen66_Test_Mux/Jolteon_TSL_Sen66_Test_Mux.ino`. |
| Wisp: local DFRobot Multi-Gas patch was required | **Partially addressed.** The wrapper honors its address and acquisition/temperature-compensation arguments, owns its gas-type string, reconnects after rail cycles, and retains the previous sample when new data is unavailable. | **Implementation:** `src/Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.cpp`, `src/Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h`.<br>**Example:** `examples/Sensors/I2C/DFMultiGasSensor/DFMultiGasSensor.ino`.<br>**Dependency:** packaged `DFRobot_MultiGasSensor-main`. |
| WiFiMongoDBBatch: false low-battery warning and approximately 2.7 V readings | **Addressed.** Battery reads configure ADC resolution, discard the first conversion, average samples, and use the 12-bit maximum. Mongo batch gating uses the corrected direct reading without requiring a registered analog module. | **Implementation:** `src/Sensors/Loom_Analog/Loom_Analog.cpp`, `src/Sensors/Loom_Analog/Loom_Analog.h`; `src/Internet/Logging/Loom_MongoDB/Loom_MongoDB.cpp`.<br>**Example:** `examples/Internet/Logging/WiFiMongoDBBatch/WiFiMongoDBBatch.ino`.<br>**Hardware test:** `examples/Lab Examples/Jolteon/debug_hypnos_adc_sleep_test/debug_hypnos_adc_sleep_test.ino`. |
| SmartRock: freeze on first wake with the EC/I2C board attached or longer sleep intervals | **Targeted; stack validation recommended.** Hypnos wake behavior is independently hardware-verified; SmartRock configuration, rail settling, ADS1115 startup, and MS5803 paths are corrected. | **Implementation:** `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h`; `src/Sensors/I2C/Loom_ADS1115/Loom_ADS1115.cpp`; `src/Sensors/I2C/Loom_MS5803/Loom_MS5803.cpp`, `src/Sensors/I2C/Loom_MS5803/Loom_MS5803.h`.<br>**Examples:** `examples/Lab Examples/SmartRock/SmartRock/SmartRock.ino`, `examples/Lab Examples/SmartRock/SmartRock2.5/SmartRock2.5.ino`, `examples/Lab Examples/SmartRock/SmartRock_2026/SmartRock_2026.ino`. |
| Slow first compile and hundreds of warnings exposed by the CLI audit | **Partially addressed.** Cleaned Loom-owned errors, missing returns, signedness issues, unsafe formatting, and unused locals. The audit distinguishes Loom and dependency warnings; dependency pruning remains future work. | **Audit engine/filter:** `tests/loom_compile_engine.bat`, `tests/loom_warning_filter.ps1`.<br>**Entry points:** `tests/loom_compile_audit_full.bat`, `tests/loom_compile_audit_no_bins.bat`, `tests/loom_compile_smoke_no_logs_or_bins.bat`, `tests/loom_compile_audit_jolteon_r5_no_bins.bat`.<br>**Retry/state tooling:** `tests/loom_compile_retry_failed.bat`, `tests/loom_retry_failed.ps1`, `tests/warning_scope_architecture.ps1`, `tests/warning_scope_strip.cmd`, `tests/warning_scope_restore.cmd`, `tests/.warning_scope_state/restore.patch`. |

### Additional cards and current source limits

The latest source status below supersedes the initial release review. These optional candidates
remain uncompiled under the current compiler pause. Full per-card status is in `docs/ISSUE_TALLY.md`.

| Issue | Current local status |
| --- | --- |
| [#220 — LTE location metadata](https://github.com/OPEnSLab-OSU/Loom-V4/issues/220) | Optional R5 GNSS acquisition, fresh metadata and stable nullable CSV columns are implemented as source candidates. Defaults stay off; hardware and live schema acceptance remain. |
| [#267 — Memory pools](https://github.com/OPEnSLab-OSU/Loom-V4/issues/267) | Optional fixed receive slots and bounded allocation utilities are present. Whole-framework/SDK migration and a measured runtime budget remain larger work. |
| [#271 — LoRa groups and scheduling](https://github.com/OPEnSLab-OSU/Loom-V4/issues/271) | Optional group filtering and UTC slots are source candidates; radio ACKs are not application authorization or storage acceptance. Airtime, retries and RTC synchronization need bench checks. |
| [#272 — Sensor dependency review](https://github.com/OPEnSLab-OSU/Loom-V4/issues/272) | Direct include cleanup and dependency profiling tools are retained. Required drivers remain; team review and a slim board package are still separate work. |
| [#273](https://github.com/OPEnSLab-OSU/Loom-V4/issues/273) / [#281](https://github.com/OPEnSLab-OSU/Loom-V4/issues/281) — Manager idle/standby mode | Optional Manager hooks and mux forwarding support SEN55/SEN66 retained-rail standby. Unsupported drivers stay powered; Dendrometer/Evaporometer contracts and broader ownership redesign remain. |
| [#287 — Logger filename truncation](https://github.com/OPEnSLab-OSU/Loom-V4/issues/287) | Already corrected in the 4.9 baseline; retained without claiming a new fix. |

## Core correctness and compatibility

- **Release-wide data contract:** preserved established 4.9 packaged field names, so 4.9.1 does not require a repository-wide database or dashboard key migration. Only newly supported SEN55/SEN66 measurements add fields.
- **Review anchors — core lifecycle/logging:** `src/Logger.h:52` — `SLOGF`; `src/Module.h:60` — failed-initialization power-up policy; `src/Loom_Manager.cpp:154` — `power_up()`; `src/Loom_Manager.cpp:224` — `initialize()`; `src/Hardware/Loom_OLED/Loom_OLED.cpp:26` — display destructor behavior.
- **`src/Logger.h`:** corrected formatted logger macro expansion while retaining `SLOGF` silent/SD-log behavior.
- **`src/Module.h`, `src/Loom_Manager.cpp`:** added the opt-in failed-initialization retry hook, standardized lifecycle/watchdog handling, and ensured manager operations respect actual module state.
- **`src/Internet/Connectivity/NetworkComponent.h`, `src/Internet/Communication/Loom_Max/Loom_Max.cpp`, `src/Internet/Communication/Loom_Max/Loom_Max.h`, `src/Internet/Communication/Loom_RemoteManager/Loom_RemoteManager.cpp`, `src/Radio/Loom_Freewave/Loom_Freewave.cpp`:** corrected missing or inaccurate status returns so callers receive the real operation result.
- **`src/Actuators.h`, `src/Hardware/Actuators/Loom_Neopixel/Loom_Neopixel.cpp`, `src/Hardware/Actuators/Loom_Relay/Loom_Relay.cpp`, `src/Hardware/Actuators/Loom_Servo/Loom_Servo.cpp`, `src/Hardware/Actuators/Loom_Servo/Loom_Servo.h`:** corrected actuator construction, cleanup, and power-down behavior.
- **`src/Hardware/Loom_OLED/Loom_OLED.cpp`, `src/Hardware/Loom_OLED/Loom_OLED.h`:** restored proper display ownership cleanup and valid lifecycle method declarations.

## Hypnos, RTC, sleep, and SD

- **Review anchors — `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`:** line 222 — `registerInterrupt()`; line 264 — `reattachRTCInterrupt()`; line 331 — `initializeRTC()`; line 423 — `networkTimeUpdate()`; line 580 — `setInterruptDuration()`; line 657 — `sleep()`; line 728 — `pre_sleep()`; line 757 — `post_sleep()`; line 806 — `getConfigFromSD()`.
- **Review anchors — `src/Hardware/Loom_Hypnos/SDManager.cpp`:** line 150 — `log()`; line 256 — `begin()`; line 330 — `updateCurrentFileName()`; line 432 — `logBatch()`.
- **`src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h` — RTC interrupts and sleep:** track the real pin/callback pair, avoid duplicate handlers, clear stale DS3231/EIC/NVIC state, detach the correct wake interrupt, recover a prematurely asserted RTC line, and refuse standby when the wake handler cannot be attached.
- **`src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h` — alarm scheduling:** retain the exact scheduled `DateTime`, verify the Alarm 1 register readback, fix month/year boundary and overrun comparisons, reject zero-length alarms, and abort sleep cleanly when no valid alarm or wake interrupt is registered.
- **`src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h` — time initialization and packaging:** add `setCompileTime(__DATE__, __TIME__)`, correct RTC lost-power recovery, timezone/DST conversion, network-time status propagation, and zero-padded UTC/local ISO timestamps.
- **`src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.h` — configuration and voltage:** accept root or nested `SleepInterval`, `sleepInterval`, and `sleep_interval` objects, report the parsed day/hour/minute/second components, tolerate a UTF-8 BOM, validate timezone values, retain safe fallbacks, and keep `checkVoltage()` classifications/flags consistent with configurable Loom analog settings.
- **`src/Hardware/Loom_Hypnos/SDManager.cpp`, `src/Hardware/Loom_Hypnos/SDManager.h` — logging safety:** bound header, row, numbered filename, and batch filename construction; report SD initialization/open/logging results accurately; and keep the SD bus at 4 MHz for shared-bus/card stability.
- **`src/Hardware/Loom_Hypnos/SDManager.cpp`, `src/Hardware/Loom_Hypnos/SDManager.h` — file continuity:** separate card reachability from the one-time session filename selection so a transient wake-time SD failure resumes the existing CSV instead of selecting another numbered file.
- **`src/Hardware/Loom_Hypnos/SDManager.cpp`, `src/Hardware/Loom_Hypnos/SDManager.h` — file reads:** reject oversized files, allocate only the required buffer, null-terminate it, and return `nullptr` safely after allocation/open/read failures.

## I2C multiplexer and sensors

- **Review anchors — `src/Hardware/Loom_Multiplexer/Loom_Multiplexer.cpp`:** line 250 — `initialize()`; line 502 — `power_up()`; line 531 — `power_down()`; line 627 — `probeAddress()`.
- **Review anchors — particulate/gas sensors:** `src/Sensors/I2C/Loom_SEN66/Loom_SEN66.cpp:21` — `initialize()`; `src/Sensors/I2C/Loom_SEN66/Loom_SEN66.cpp:79` — `measure()`; `src/Sensors/I2C/Loom_SEN66/Loom_SEN66.cpp:210` — `package()`; `src/Sensors/I2C/Loom_SEN55/Loom_SEN55.cpp:31` — `initialize()`; `src/Sensors/I2C/Loom_SEN55/Loom_SEN55.cpp:62` — `measure()`; `src/Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.cpp:21` — `initialize()`; `src/Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.cpp:104` — `power_up()`.
- **Review anchors — field sensors:** `src/Sensors/I2C/Loom_ADS1115/Loom_ADS1115.cpp:17` — `initialize()`; `src/Sensors/I2C/Loom_MS5803/Loom_MS5803.cpp:64` — `initialize()`; `src/Sensors/I2C/Loom_MS5803/Loom_MS5803.cpp:102` — `measure()`; `src/Sensors/I2C/Loom_TSL2591/Loom_TSL2591.cpp:94` — `power_up()`; `src/Sensors/I2C/Loom_TSL2591/Loom_TSL2591.cpp:108` — `power_down()`.
- **`src/Hardware/Loom_Multiplexer/Loom_Multiplexer.cpp`, `src/Hardware/Loom_Multiplexer/Loom_Multiplexer.h` — discovery/configuration:** verify the TCA9548 control register rather than accepting any ACK, support alternate mux/known sensor addresses, add port enable/disable and scan diagnostics, and expose per-sensor auto-load options.
- **`src/Hardware/Loom_Multiplexer/Loom_Multiplexer.cpp`, `src/Hardware/Loom_Multiplexer/Loom_Multiplexer.h` — lifecycle safety:** select and verify the owning port before each sensor lifecycle call, clear stale sensor/address state before rescans, delete failed auto-loaded objects, and close every channel during power-down.
- **`src/Hardware/Loom_Multiplexer/Loom_Multiplexer.cpp`, `src/Hardware/Loom_Multiplexer/Loom_Multiplexer.h` — device coverage:** add mux loading for SEN66, SEN55, both DF Multi-Gas addresses, TSL2591 options, and MMA8451 address `0x1C`.
- **`src/Sensors/I2C/Loom_SEN66/Loom_SEN66.cpp`, `src/Sensors/I2C/Loom_SEN66/Loom_SEN66.h`:** add the SEN66 module with particulate mass, particle counts, humidity, temperature, VOC, NOx, and CO2 packaging.
- **`src/Sensors/I2C/Loom_SEN55/Loom_SEN55.cpp`, `src/Sensors/I2C/Loom_SEN55/Loom_SEN55.h`:** add number concentrations and typical particle size and improve measurement error handling.
- **`src/Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.cpp`, `src/Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h`:** own the configured address/options/label data, bypass the vendor global Arduino-`String` analysis path with fixed protocol responses, reconnect after rail cycles, and retain the previous sample when new data is unavailable.
- **`src/Sensors/I2C/Loom_MS5803/Loom_MS5803.cpp`, `src/Sensors/I2C/Loom_MS5803/Loom_MS5803.h`:** probe the configured address and calibration PROM, avoid trusting the legacy library's unreliable initialization return, prime the first reading, and report actual readiness.
- **`src/Sensors/I2C/Loom_ADS1115/Loom_ADS1115.cpp`:** initialize the shared I²C controller, probe the configured address, discover valid `0x48`–`0x4B` address straps, and avoid repeated allocating `begin()` calls after Hypnos wakes.
- **`src/Sensors/I2C/Loom_TSL2591/Loom_TSL2591.cpp`, `src/Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h`:** add functional `power_up()` and `power_down()` lifecycle methods for WeatherChimes and mux workflows.
- **`src/Sensors/I2C/Loom_K30/Loom_K30.cpp`, `src/Sensors/I2C/Loom_MB1232/Loom_MB1232.cpp`, `src/Sensors/I2C/Loom_MMA8451/Loom_MMA8451.cpp`, `src/Sensors/I2C/Loom_MMA8451/Loom_MMA8451.h`, `src/Sensors/I2C/Loom_STEMMA/Loom_STEMMA.cpp`, `src/Sensors/I2C/Loom_T6793/Loom_T6793.cpp`, `src/Sensors/I2C/Loom_VCNL4020/Loom_VCNL4020.cpp`:** correct smaller initialization, return-value, address, conversion, and packaging defects in the affected I²C modules.
- **`src/Sensors/Loom_Digital/Loom_Digital.cpp`, `src/Sensors/Loom_Digital/Loom_Digital.h`, `src/Sensors/SDI12/Loom_SDI12/Loom_SDI12.cpp`, `src/Sensors/SDI12/Loom_SDI12/Loom_SDI12.h`, `src/Sensors/SPI/Loom_MAX318XX/Loom_MAX31856.cpp`, `src/Sensors/SPI/Loom_MAX318XX/Loom_MAX31856.h`:** correct related non-I²C sensor return, initialization, and packaging behavior found during the same audit.

## Analog and ADS1232

- **Review anchors — `src/Sensors/Loom_Analog/Loom_Analog.cpp`:** line 4 — `measure()`; line 45 — parameterized `getBatteryVoltage()`; line 67 — `readBatteryVoltage()`.
- **Review anchors — `src/Sensors/SPI/Loom_ADS1232/Loom_ADS1232.cpp`:** line 37 — `measure()`; line 55 — `calibrate()`.
- **`src/Sensors/Loom_Analog/Loom_Analog.cpp`, `src/Sensors/Loom_Analog/Loom_Analog.h`:** configure the SAMD ADC before battery sampling, discard the first conversion, average repeated samples, use the correct 12-bit maximum, and expose guarded reference/divider/pin/resolution/sample-count settings.
- **`src/Sensors/Loom_Analog/Loom_Analog.cpp`, `src/Sensors/Loom_Analog/Loom_Analog.h`:** add the parameterized static battery-reading path used by MongoDB transmission gating and Hypnos voltage checks.
- **`src/Sensors/SPI/Loom_ADS1232/ADS1232_Lib_Fixed.cpp`, `src/Sensors/SPI/Loom_ADS1232/ADS1232_Lib_Fixed.h`, `src/Sensors/SPI/Loom_ADS1232/Loom_ADS1232.cpp`, `src/Sensors/SPI/Loom_ADS1232/Loom_ADS1232.h`:** add bounded ready waits, controlled discard/settling reads, calibration handling, and explicit failure status instead of indefinite blocking.

## LTE, networking, MQTT, and MongoDB

- **2026-10-01 scoped review:** preserve watchdog state during retained command reads and broker
  disconnects; require the requested command topic; reject invalid/truncated routing configuration
  and malformed batch lines before sending. Production Mongo/MQTT host tests exercise whole-batch
  retention/replay, project and hub routing, heartbeat identity, metadata ACK failure and SD boundary
  failures. Six selected native programs, SD safety cases, 11 offline receipts and the viewer passed;
  the active Wisp trace sketch compiled at 244,628 bytes without upload. Live broker/database receipts,
  multi-day delivery and production duplicate handling remain acceptance work. See `docs/ISSUE_TALLY.md`.

- **Review anchors — `src/Internet/Connectivity/Loom_LTE/Loom_LTE.cpp`:** line 519 — `initialize()`; line 600 — `power_up()`; line 785 — `connect()`; line 998 — `getNetworkTime()`.
- **Review anchors — `src/Internet/Logging/Loom_MongoDB/Loom_MongoDB.cpp`:** line 31 — single publish; line 75 — metadata publish; line 122 — batch publish.
- **Review anchors — `src/Radio/Loom_LoRa/Loom_LoRa.cpp`:** lines 227–288 — fragment handling; line 424 — batch-send path.
- **`src/Internet/Connectivity/Loom_LTE/Loom_LTE.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE.h`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_Config.h`:** retain SARA-R4 as the default while allowing per-object R4/R5 selection, configure R5 power polarity/timing/115200-baud startup, preserve R4 9600-baud behavior, and expose optional baud/reset diagnostics.
- **`src/Internet/Connectivity/Loom_LTE/Loom_LTE_Modem.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_Modem.h`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_SaraR4.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_SaraR5.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE_TinyGsmAdapter.h`:** separate TinyGSM R4/R5 implementations so Arduino IDE sketches no longer depend on a library-wide compiler flag.
- **`src/Internet/Connectivity/Loom_LTE/Loom_LTE.cpp`, `src/Internet/Connectivity/Loom_LTE/Loom_LTE.h`:** separate AT boot readiness from SIM/registration/APN/PDP/TCP failures, add bounded recovery and stale-PDP cleanup, provide raw AT/passthrough diagnostics, and copy credentials/configuration into owned bounded storage.
- **`src/Internet/Connectivity/Loom_LTE/Loom_LTE.cpp`, `src/Internet/Connectivity/NetworkComponent.h`, `src/Hardware/Loom_Hypnos/Loom_Hypnos.cpp`:** preserve SARA-R4 clock fields as UTC, propagate actual time-update status, and make intentionally offline batch intervals a quiet no-op. The existing Wisp V2 call sites then synchronize at connected startup and upload windows.
- **Hardware result for the preceding LTE files:** SARA-R410M answered AT at 9600, recovered from an initial TinyGSM initialization miss, registered, acquired an IP address, and reached a remote HTTP server.
- **`src/Internet/Connectivity/Loom_Wifi/Loom_Wifi.cpp`:** harden credential copies, AP fallback names, default client mode, and failed verification returns.
- **`src/Internet/Connectivity/Loom_Ethernet/Loom_Ethernet.cpp`:** return accurate connection and network-time status.
- **`src/Internet/Logging/MQTTComponent/MQTTComponent.cpp`, `src/Internet/Logging/MQTTComponent/MQTTComponent.h`:** use QoS 1, support `MAX_JSON_SIZE` payloads with the packaged ArduinoMqttClient version, and return retained-message deletion status.
- **`src/Internet/Logging/Loom_MongoDB/Loom_MongoDB.cpp`:** add project-aware batch topics, accept CR/LF/CRLF batch files, reject oversized/malformed records and preserve an unterminated possible torn tail, propagate metadata status, and use the corrected direct battery reading for low-voltage gating.
- **`src/Radio/Loom_LoRa/Loom_LoRa.cpp`:** reject incomplete fragment sets and return actual batch-send status.

## Examples and field workflows

- **Jolteon/SARA-R5 bring-up:** `examples/Lab Examples/Jolteon/Loomified_LTE_R5_example/Loomified_LTE_R5_example.ino`, `examples/Lab Examples/Jolteon/Loomified_LTE_R5_debug_passthrough/Loomified_LTE_R5_debug_passthrough.ino`, `examples/Lab Examples/Jolteon/SARA_R5_Test_No_Loom/SARA_R5_Test_No_Loom.ino`, and `examples/Lab Examples/Jolteon/WC_FastRegisterR5Compat/WC_FastRegisterR5Compat.ino` cover Loom/non-Loom startup, passthrough, registration, and WeatherChimes compatibility.
- **Jolteon board validation:** `examples/Lab Examples/Jolteon/I2CBusHealth/I2CBusHealth.ino`, `examples/Lab Examples/Jolteon/Jolteon_TSL_Sen66_Test_Mux/Jolteon_TSL_Sen66_Test_Mux.ino`, `examples/Lab Examples/Jolteon/SAMD21J18_Blinky/SAMD21J18_Blinky.ino`, `examples/Lab Examples/Jolteon/SAMD21J18_Blinky_UART/SAMD21J18_Blinky_UART.ino`, and `examples/Lab Examples/Jolteon/SAMD21J18_Blinky_nRFEnable/SAMD21J18_Blinky_nRFEnable.ino` cover I²C/mux health and SAMD21J18 board checks.
- **Hypnos hardware test:** `examples/Lab Examples/Jolteon/debug_hypnos_adc_sleep_test/debug_hypnos_adc_sleep_test.ino` validates ADC sampling, first/replacement alarms, stale interrupt clearing, and repeated standby wakes; those checks pass on hardware.
- **Jolteon SWD support:** `examples/Lab Examples/Jolteon/swd_flash/Arduino Flashing with JTAG_SWD.pdf`, `examples/Lab Examples/Jolteon/swd_flash/flash_j18.bat`, `examples/Lab Examples/Jolteon/swd_flash/flash_j18.ps1`, and `examples/Lab Examples/Jolteon/swd_flash/flash_j18.cfg` document and automate SAMD21J18 flashing.
- **SARA-R4 + Hypnos:** `examples/Internet/Connectivity/LTE_with_Hypnos/LTE_with_Hypnos.ino` and `examples/Internet/Connectivity/LTE_with_Hypnos/README.md` explicitly enable Hypnos rails before LTE initialization.
- **Evaporometer:** `examples/Lab Examples/Evaporometer/Evaporometer_V1_fixed/Evaporometer_V1_fixed.ino` adds bounded ADS1232 waits, settling/discard reads, sketch compile-time RTC recovery, stable log naming, and SPI restoration; `examples/Lab Examples/Evaporometer/Evaporometer/Evaporometer.ino`, `examples/Lab Examples/Evaporometer/rev01_SDI12_Slave/rev01_SDI12_Slave.ino`, and `examples/Lab Examples/Evaporometer/rev01_SD_only/rev01_SD_only.ino` have valid Arduino folder/main-file layouts.
- **SmartRock:** `examples/Lab Examples/SmartRock/SmartRock/SmartRock.ino`, `examples/Lab Examples/SmartRock/SmartRock/SD_config_nested_example.json`, `examples/Lab Examples/SmartRock/SmartRock2.5/SmartRock2.5.ino`, and `examples/Lab Examples/SmartRock/SmartRock_2026/SmartRock_2026.ino` cover Hypnos 3.3, nested SD sleep settings, rail settling, ADS1115/MS5803 addressing, VCNL4010 handling, and calibrated conductivity/turbidity output.
- **Internet logging:** `examples/Internet/Logging/LTEMongoDBBatch/LTEMongoDBBatch.ino`, `examples/Internet/Logging/ThingSpeak/ThingSpeak.ino`, `examples/Internet/Logging/WiFiMongoDB/WiFiMongoDB.ino`, and `examples/Internet/Logging/WiFiMongoDBBatch/WiFiMongoDBBatch.ino` match corrected connection, configuration, publish-status, and battery-gating behavior.
- **WeatherChimes:** `examples/Lab Examples/WeatherChimes/WeatherChimes4G/WeatherChimes4G.ino`, `examples/Lab Examples/WeatherChimes/Configurable_Chime_Code_2026/Configurable_Chime_Code_2026.ino`, `examples/Lab Examples/WeatherChimes/Cumulative_Chimes_Code_2025/Cumulative_Chimes_Code_2025.ino`, and `examples/Lab Examples/WeatherChimes/WeatherChimesTippingBucketI2C/WeatherChimesTippingBucketI2C.ino` use current LTE, TSL2591, tipping-bucket, and naming APIs.
- **Other field workflows:** `examples/Lab Examples/Dendrometer/node/AS5311.cpp`, `examples/Lab Examples/Dendrometer/node/node.ino`, `examples/Lab Examples/LilyPad/LilyPad.ino`, `examples/Lab Examples/LoRa_To_4G/LoRa_To_4G.ino`, `examples/Lab Examples/MultipleInterrupts/MultipleInterrupts.ino`, `examples/Lab Examples/WC_FastRegister/WC_FastRegister.ino`, and `examples/Lab Examples/WeedWarden/WeedWarden.ino` correct stale APIs, missing symbols, and return/configuration behavior.
- **Wisp and sensor examples:** `examples/Lab Examples/Wisp/Wisp_Batch_Logging/Wisp_Batch_Logging.ino`, `examples/Lab Examples/Wisp/Wisp_Mux_BatchLogging/Wisp_Mux_BatchLogging.ino`, `examples/Sensors/I2C/AS7263/AS7263.ino`, `examples/Sensors/I2C/DFMultiGasSensor/DFMultiGasSensor.ino`, `examples/Sensors/I2C/VCNL/VCNL.ino`, and `examples/Sensors/I2C/VCNL2/VCNL2.ino` correct class names, module APIs, and Arduino sketch discovery layouts.
- **Class example layout:** `examples/ClassExamples/E102/Adalogger_i2cSensorsSD_STEMMA/Adalogger_i2cSensorsSD_STEMMA.ino` replaces the invalid `.ino`-named folder/main-file arrangement.

## Compile-audit tooling

- **`tests/loom_compile_engine.bat`, `tests/loom_warning_filter.ps1`:** implement sketch discovery/compilation, per-sketch timing, warning/error counts, and Loom-versus-external warning classification.
- **`tests/loom_compile_audit_full.bat`, `tests/loom_compile_audit_no_bins.bat`, `tests/loom_compile_smoke_no_logs_or_bins.bat`, `tests/loom_compile_audit_jolteon_r5_no_bins.bat`:** provide full, no-binary, smoke, and Jolteon-R5 audit entry points.
- **`tests/loom_compile_retry_failed.bat`, `tests/loom_retry_failed.ps1`:** locate the newest audit, build a retry manifest from eligible failures, and rerun only sketches still requiring verification.
- **`tests/warning_scope_architecture.ps1`, `tests/warning_scope_strip.cmd`, `tests/warning_scope_restore.cmd`:** add and remove the temporary warning-scope header/guard architecture with one-click commands.
- **`tests/.warning_scope_state/restore.patch`, `tests/.warning_scope_state/metadata.txt`:** preserve the exact stripped state and readable affected-file manifest so the instrumentation can be restored without shipping it.
- **`tests/README.md`, `tests/WARNING_SCOPE.md`:** document compile-audit usage, retry paths, and warning-scope strip/restore behavior.
- **`tests/.gitignore`:** exclude generated audit directories, manifests, credentials, and binaries while keeping the saved warning-scope restore state visible to Git.

## Packaged libraries and distribution

- **Package-index/distribution metadata:** retain the upstream QoS 1 correction merged after `v4.9` and identify the release as the 4.9.1 bugfix layer.
- **`Updated_Package_Libraries(place_one_level_above).zip`:** contains updated `TinyGSM`, `SparkFun_LTE_Shield_Arduino_Library-master`, `ADS1232_Library`, and `SDS011-master` dependencies.
- **Installation location:** extract the archive one directory above Loom so Arduino IDE builds select the board-package copies of those dependencies.

## Validation status

- **Passed:** Hypnos analog voltage sampling on hardware.
- **Passed:** DS3231 first alarm, alarm clearing/replacement, and repeated Hypnos standby wake behavior on hardware.
- **Passed:** SARA-R4 AT initialization, network registration, PDP/IP acquisition, and outbound HTTP reachability on hardware.
- **Passed:** Smartrock flash and wake/log cycle, checking that it's not randomly making new files as we introduced that last night pre-4.9 bringup.
- **Passed:** Wisp V2 Deploy sketch has correct timestamps.
- **Compile coverage:** the audit harness discovers the complete example tree and the 4.9.1 fixes address the Loom-owned failures found during the 123-sketch run.
- **Still recommended:** a multi-day LoRa/LTE/MongoDB soak test, a complete Wisp V2 SEN66/DF-gas mux sleep test, final SmartRock EC-board wake validation, and final Jolteon SARA-R5 end-to-end validation.

- Build follow-through: restore top-level Loom include discovery before nested BatchSD includes in
  LTE/WiFi batch examples and both Wisp batch pairs; retain all operational code and visual cues.
- Add host fault coverage for production MQTT publishing/reconnect/retained-message paths, including
  short writes, failed completion, post-poll disconnect, bounded retries and watchdog restoration.

- Earlier 2026-09-30 validation accepted 126 sketch/API-link checks, four beta-diagnostic-off
  builds, nine normal cold reference builds, eleven host programs and the data-safety suite,
  with zero Loom-owned compiler warnings at those revisions. Later accepted host coverage
  reached 17 programs. Current source changes and ten additional host programs remain
  uncompiled under the renewed compiler pause. See `docs/ISSUE_TALLY.md` for current limits;
  retained build products describe historical results, not acceptance of the current source.


### 2026-09-30 optional support and final source refinements (not compiled)

- Add optional Manager idle/resume and borrowed health-observer hooks; preserve the watchdog
  and supported retained-rail sensor behavior. Observer completion events do not prove driver
  success. Flash health remains an infrequent two-slot opt-in with writes/automatic attempts off
  by default, not a durable record of every operation before a crash.
- Keep optional R5 GNSS separate from measurement UTC and modem network operations. Validate
  checksums/calendar/coordinates/freshness; provide metadata and stable nullable CSV location
  fields. The example defaults GNSS and SD off and does not assume GNSS hardware is installed.
- Add optional CSV identity suppression, row checksums, sensor column selection, radio groups/
  UTC slots and heartbeat modes while preserving defaults, packet identity and pending batches.
  CSV identity suppression affects CSV only; sensor column selection also changes sample JSON.
- Add optional recharge hysteresis using the user's 3.7/4.2 V policy, bounded loop-side brownout
  handling, and an internal-timer sleep choice. MCU supply/flash safety, current and recovery
  margins remain physical acceptance work.
- Extend checked RTC reads, bounded AT replies, stream forwarding, receive-buffer ownership,
  packet-view lifetime and command/publish failure handling. Retain explicit function start/end
  debug spelling, thorough comments, slash dividers and the compiler warning-scope header.
- Retain offline receipt/routing/dashboard mocks and their interpreted checks. They separate
  measurement, receipt and insertion times without claiming production-service integration.
- Prepare read-only source/host/mock CI with pinned ArduinoJson v6.20.1 and preserve separate
  Loom/external warning audits. The workflow has not been pushed, dispatched or accepted.
- Consolidate the saved 55-card audit into `docs/ISSUE_TALLY.md` and beginner/embedded coding
  guidance into `docs/STYLE_GUIDE.md`. Remove generated narrative reports, board snapshots and
  redundant new READMEs at the user's request; retain original project documentation, code,
  functional build manifests, tests/tools and existing build products.
- Source-only/interpreted checks passed before consolidation. No compiler or hardware test
  validates these latest candidates; the current compiler pause remains in effect.
