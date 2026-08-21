# Feather M0 beta readiness

## Release decision

This tree is ready to enter a controlled Adafruit Feather M0 / ATSAMD21G18A beta. It is not ready
to be called stable until the hardware acceptance gates below pass. The code and linked-layout
gates are green; RTC wake, I2C fault recovery, SD fault behavior, brownout behavior, and the
three-week lifecycle trace require real Feather M0 + Hypnos hardware.

Target FQBN:

`loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off`

The target has 32,768 bytes of SRAM and 256 KB of flash. Every release decision must use the linked
ELF and runtime lifecycle telemetry, not the IDE's pre-runtime “free RAM” value alone.

## Compatibility gate

The release intentionally preserves:

- top-level packet fields, module labels, sensor field labels, and configuration keys;
- CSV serial/header/data ordering and line layout;
- numbered `.csv` and `-Batch.txt` filenames;
- newline-delimited compact JSON batch records;
- MongoDB, ThingSpeak, and RemoteManager MQTT topics and payload layouts;
- LoRa/Freewave MessagePack fields and framing.

A static token comparison against the tracked 4.9 source found no renamed built-in JSON/config key
or module label. The only removed bracket literal was OLED's private `flatObj` scratch object; it
was not part of any serialized output. The detailed contract is in `COMPATIBILITY_CONTRACT.md`.

Reliability changes are deliberately format-neutral. A failed or partial SD write is rolled back,
a pending MongoDB/LoRa batch is retained until all records succeed, and retries continue above the
configured batch threshold. Delivery is at least once: loss is avoided, but a server acceptance
followed by a local clear failure can produce a duplicate on retry.

ThingSpeak's formerly declared-but-undefined batch overload now returns `false`. ThingSpeak fields
come from live callbacks, so replaying Loom JSON as if it contained those callback results would be
incorrect. Single-packet ThingSpeak behavior and bytes are unchanged.

## SAMD21 memory gate

The final instrumented WISP v1 link uses 7,928 bytes of static SRAM, leaving 24,840 bytes before
runtime heap and stack. Known retained runtime ownership then includes at least:

| Owner | Retained bytes |
|---|---:|
| Manager JSON document | 2,000 |
| SDManager | 1,664 |
| SARA-R4 TinyGSM adapter/client | 228 |
| Manager module pointer capacity | 32 |
| MQTT credentials/topics | Actual configured lengths plus allocator metadata |

The known fixed heap items leave about 20,916 bytes before MQTT strings, allocator metadata,
dependency-owned buffers, sensor objects, and stack demand. This explains why an observed value
near 17 KB can be real but is not itself a safety proof.

The first six-hour publish is the important lifecycle boundary. Unknown-length 2 KB MQTT payload
allocation has been removed; MQTT and SD bodies stream with small buffers. The largest Loom-owned
WISP frames were reduced from multi-kilobyte automatic arrays to hundreds of bytes or less.
Multiplexer discovery reserves its tuple capacity before constructing sensors, and recurring Loom
sensor/SD paths no longer use Arduino `String` or `std::string`. TinyGSM and ArduinoMqttClient still
use `String` internally and must be judged from runtime telemetry.

Final linked checks include:

| Sketch/test | Flash | Static SRAM | Result |
|---|---:|---:|---|
| Wisp_Batch_Logging | 150,724 B | 7,928 B | Pass |
| Wisp_Mux_BatchLogging | 176,212 B | 7,912 B | Pass |
| WispV2_Deploy_2026 | 177,676 B | 7,800 B | Pass |
| Dendrometer hub | 132,436 B | 8,268 B | Pass |
| Dendrometer node | 111,308 B | 7,428 B | Pass |
| WiFiMongoDBBatch | 114,620 B | 7,804 B | Pass |
| LoRa Batch Transmit | 92,436 B | 7,016 B | Pass |
| ThingSpeak | 92,732 B | 7,244 B | Pass |
| Hypnos DST boundary test | 36,656 B | 5,892 B | Pass |
| OPEnS_RTC compatibility | 17,308 B | 3,568 B | Pass |

The full WISP warnings-all link also passed with no Loom-owned warning. Board-core and third-party
warnings are tracked separately from release-owned code.

Provisional beta stop-ship criteria:

- `brk` must stabilize after the first successful LTE/MQTT cycle and after the first reconnect;
- `frag`/`holes` must not rise monotonically from cycle to cycle;
- `min_contig` must remain at least 6 KB, and at least 4 KB above the largest measured subsequent
  drop from a shallow checkpoint into a full SD + LTE/MQTT call chain;
- Manager JSON `ovf` must remain zero;
- every `pre_sleep` checkpoint must have a `post_wake` checkpoint or a diagnosed reset cause.

If a known-good full-stack unit establishes a higher floor, use that measured floor plus normal
variation as the team threshold. Do not lower the 6 KB provisional floor merely to pass a unit.

## Mandatory hardware acceptance

### RTC, DST, and sleep

1. Run at least 100 accelerated alarm/wake cycles before the long soak.
2. Verify exact scheduled-alarm readback and one `post_wake` per `pre_sleep`.
3. Set the RTC on both sides of the second-Sunday-in-March and first-Sunday-in-November boundaries;
   verify UTC/local timestamps and the repeated fall-back hour.
4. Cross a month boundary and a December/January boundary.
5. Test oscillator-stop/lost-power recovery and the non-interactive compile-time fallback.

### Batch, SD, and network failure

1. At record 72, force the first broker publish to fail. Confirm the batch file is not truncated.
2. Append record 73. Confirm LTE/WiFi powers again, all pending records are retried, and the file is
   cleared only after complete success.
3. Fail one record in the middle of a batch and confirm the entire local batch remains available.
4. Remove or fault the SD card during a CSV row and during a batch append. After reinsertion,
   confirm no partial tail is present and the selected filename does not roll over because of the
   transient failure.
5. Interrupt power after remote acceptance but before local clear and verify the documented
   at-least-once duplicate behavior is acceptable to the bridge/database.
6. Verify low-battery behavior and modem current draw during an extended outage; retries above the
   threshold intentionally favor retaining/delivering data and may need a deployment-level retry
   cadence if the battery budget cannot support every sample interval.

### I2C and sensor rail

1. Hold SDA low, then SCL low, separately. Each transaction must return within the documented
   bound rather than freezing the MCU.
2. Remove the fault, cycle the sensor rail, and verify the next transaction succeeds.
3. Exercise normal clock-stretching sensors and the mux with every intended WISP sensor populated.
4. Run refresh only as a recovery test; field loops should retain the discovered sensor set.

### Three-week soak

At a five-minute sample interval, 21 days produces 6,048 sample/sleep/wake cycles and 84 nominal
six-hour upload boundaries. The soak must include successful uploads, broker outages, reconnects,
SD failures, and at least one controlled reset. Preserve the Serial memory log and reset cause for
the complete run.

The beta passes only if there is no progressive contiguous-memory decline, no unexplained missing
wake, no corrupt/partial stored record, no lost failed batch, and no output-contract drift.

## Golden-output acceptance

Before distributing the archive, capture representative 4.9 and beta output from the same sensor
fixture and compare:

- recursive JSON field paths and built-in module labels;
- CSV header bytes, column order, comma placement, and one representative data row;
- generated CSV/batch filenames;
- MQTT topics and ThingSpeak payload text;
- decoded and encoded LoRa/Freewave MessagePack fields.

Value corrections from repaired sensors or DST are expected; label, ordering, delimiter, topic,
and framing changes are not. Keep these golden captures with the beta test report.

## Known limitations

- Batch count and queue selection are RAM-only. A reset selects a new numbered batch file instead
  of rediscovering and replaying the prior file. Reset-resilient queue discovery needs its own
  compatibility and corruption-recovery design before stable.
- TinyGSM and ArduinoMqttClient retain internal Arduino Strings. Their runtime behavior is measured,
  not assumed safe.
- Optional board/library globals still consume static SRAM in LTE builds. Removing them safely
  requires package/core library separation, not risky sketch macros.
- The 2 KB Manager document remains heap-owned. Moving it to static storage would improve placement
  determinism but would not create more RAM.
- The bounded Wire/SERCOM changes live in the surrounding board package. A beta archive that ships
  this Loom repository without every file in `PLATFORM_PATCH_MANIFEST.md` is incomplete.

## Promotion rule

Ship this as a telemetry-enabled beta. Promote it to stable only after the mandatory hardware
matrix, golden-output comparison, and three-week soak have artifacts showing every gate passed.
