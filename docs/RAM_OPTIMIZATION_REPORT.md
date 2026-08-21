# Feather M0 field-hang diagnosis and RAM optimization report

## Target and conclusion

This work targets the Adafruit Feather M0: ATSAMD21G18A, Cortex-M0+, 32,768 bytes of SRAM and
256 KB of flash.

The reported ~17 KB free after setup is not a complete safety margin. It excludes future stack
high-water, allocator holes, retained network buffers, and allocations that occur only during the
first LTE/MQTT upload. The eight-hour hang is most consistent with a lifecycle memory problem that
becomes visible after networking, not with an eight-hour timer.

The WISP sketches sample every five minutes and publish a 72-record batch every six hours. In the
old path, the first publish could cause ArduinoMqttClient to allocate and retain a 2,000-byte
unknown-length payload buffer. That creates a strong timing explanation: the stack runs normally
until the first six-hour publish, loses roughly 1.7 KB relative to the library's normal 256-byte
buffer behavior, and fails during a later sensor/SD/LTE cycle. SEN55 churn can contribute, but it is
not sufficient as the sole diagnosis because TinyGSM, ArduinoMqttClient, SD, JSON, and sensor paths
all share the same heap and stack.

RTClib/wake behavior remains an independent suspect. Stable memory through `pre_sleep` followed by
no `post_wake` record implicates RTC, alarm, interrupt, or rail state. Declining/fragmented memory
before that point implicates RAM.

## Final measured static SRAM

These values come from clean Feather M0 linked ELF files and include field memory diagnostics.

| Deployment sketch | `.data` | `.bss` | Static total | SRAM left before runtime heap/stack |
|---|---:|---:|---:|---:|
| Wisp_Batch_Logging | 976 B | 6,952 B | **7,928 B** | **24,840 B** |
| Wisp_Mux_BatchLogging | 976 B | 6,936 B | **7,912 B** | **24,856 B** |
| WispV2_Deploy_2026 | 976 B | 6,824 B | **7,800 B** | **24,968 B** |

These are the final hardened-OPEnS_RTC builds. At the RTC migration checkpoint, static SRAM changed
by only +4 B, +8 B, and 0 B relative to the immediately preceding guarded RTClib builds; later
sensor/MQTT state cleanup recovered 8 B, 8 B, and 0 B from those checkpoints. OPEnS_RTC performs no runtime
allocation, so it also removes RTClib/Adafruit BusIO's retained 16-byte I2C helper and its allocator
metadata. The small RTC static difference is from the checked driver's error state and one shared
legacy text buffer; it is not per timestamp.

The comparable basic image before this pass used 10,092 bytes of static SRAM. The final
instrumented basic image recovers **2,164 permanently reserved bytes**. The diagnostic object and
newlib allocator counters account for 84 bytes of the final image, so an uninstrumented production
build would be smaller, but telemetry should remain enabled for the three-week field test.

Static SRAM is only the starting line. Known runtime allocations include:

- Manager JSON pool: 2,000 bytes, allocated once and retained.
- SDManager: **1,664 bytes**, measured from the generated SAMD21 constructor assembly; it was
  previously 2,584 bytes.
- TinyGSM adapter/client: 224 bytes for SARA-R5 or 228 bytes for SARA-R4, retained.
- Hardened OPEnS_RTC driver: no heap allocation; its fixed state is already included in `.bss`.
- Manager module pointer table: 32 bytes, now reserved once for eight modules instead of growing
  through 4/8/16-byte allocations.
- MQTT ID/username/password: actual string lengths retained by ArduinoMqttClient. Loom no longer
  keeps a second 200-byte username/password copy.

## Compiler-measured stack reductions

The values below are each function's own frame from `-fstack-usage`. Callers and callees nest, so a
log or library call can add to the listed frame.

| Function/path | Before | Final |
|---|---:|---:|
| `SDManager::log()` | 2,120 B | **144 B** |
| `SDManager::logBatch()` | 2,288 B | **88 B** |
| Mongo batch publish | 2,648 B | **176 B** |
| Manager JSON display | 2,064 B | **88 B** |
| SEN55 measurement | 664 B | **184 B** |
| SEN55 device status | 608 B | **72 B** |
| SEN66 measurement | 288 B | **232 B** |
| Mux measure/package/power | 296 B | **32 B** |
| Mux select channel | 296 B | **40 B** |
| LTE initialization | 464 B | **192 B** |
| LTE boot retry | 448 B | **192 B** |
| LTE connection | 600 B | **352 B** |
| LTE config parse | 696 B | **280 B** |
| Hypnos RTC initialization | 424 B | **168 B** |
| Hypnos network-time update | 392 B | **136 B** |
| Hypnos manual time entry | 584 B | **136 B** |
| Hypnos SD config parse | 536 B | **376 B** |
| Disabled function-summary constructor | 1,040 B | **32 B** |

The major change is that JSON, CSV, and MQTT bodies are streamed rather than materialized in
2,000-byte local arrays. Logger formatting now uses one final buffer instead of nested message,
flash-copy, and filename buffers. Mux debug formatting no longer reserves 256 bytes in every field
cycle when debugging is disabled.

## Persistent and recurring allocation changes

- Known-length MQTT publishing selects ArduinoMqttClient's streaming path. The old retained 2 KB
  transmit payload buffer is no longer used.
- Batch records stream from SD through 64-byte chunks after a length scan.
- SD JSON/CSV logging writes values directly to the file.
- FlashStorage's unused 1,027-byte EEPROM emulator is no longer linked. Loom now has a focused,
  fixed-record SAMD21 flash writer for the 204-byte WiFi credential record.
- MQTT topic/name fields are bounded from repository and protocol requirements. MQTT credentials
  are copied once into ArduinoMqttClient and reused across reconnects.
- Module names are bounded to 31 characters; Manager device names and topic components are bounded
  to 63 characters.
- The timezone name lookup is flash-resident instead of a 23-node heap `std::map`.
- Hypnos interrupt registrations use two compact fixed records instead of allocating red-black-tree
  nodes. All in-repository Hypnos sketches register one RTC source.
- Manager reserves its eight-pointer module table once during construction.
- Multiplexer discovery reserves its known sensor tuple capacity before allocating sensor objects,
  avoiding repeated vector growth between those setup allocations; refresh reuses that capacity.
- Actuators retain the inherited `Module` name only once (Relay previously kept three copies), and
  formatting-only 256-byte caller buffers were removed from actuator, WiFi, Ethernet, Max,
  Freewave, EZO, SDI-12, and several sensor paths. Logger text and prefixes are unchanged.
- SEN55/SEN66 error/status reporting no longer creates `std::string`, `to_string`, or large textual
  error buffers. Numeric error codes and hexadecimal status preserve diagnostic value.
- The packaged Sensirion SEN5x/Core/SEN66 dependencies were searched directly: they do not use
  Arduino `String` or `std::string`; `errorToString()` writes into a caller-supplied character
  buffer. The recurring SEN55 string churn was in Loom's former status conversion, not the current
  vendor driver.
- The DFRobot gas type is queried once during configuration rather than once per measurement.
- Loom now bypasses DFRobot's global Arduino-`String` analysis path for its per-sample availability
  check and uses the same fixed nine-byte protocol response instead. Gas labels (`O2`, `CO`, `H2S`,
  and the existing remaining labels) are mapped to literals and cached in the existing character
  buffer, so valid packaged field names are unchanged.
- LTE's recurring AT matcher uses finite match state instead of Arduino `String`. The redundant
  modem-identity String query was removed after AT readiness is already proved.
- Hypnos manual RTC recovery uses one fixed 12-byte input buffer rather than six Arduino Strings.
- LoRa and Freewave own their RadioHead reliability managers in-place instead of permanently
  allocating them with `new` during global construction.
- Freewave parses received MessagePack into the existing Manager document. It no longer retains a
  second 1,000-byte receive document; malformed input is rejected and the partial document cleared.
- WiFi `getUDP()` returns one lazy, non-owning singleton. This removes the old per-call leak without
  adding the approximately 1.4 KB `WiFiUDP` object to sketches that never request it.
- Small WiFi, Ethernet, ThingSpeak, and RemoteManager configuration objects use exact ArduinoJson
  capacities and mutable-input zero-copy parsing instead of 300-byte catch-all stack documents.
- OLED walks the Manager `contents` array directly instead of allocating a second 2,000-byte
  flattened JSON pool. Max reuses the Manager pool for inbound commands instead of retaining a
  second 1,000-byte command document. Max also reserves its actuator pointer table exactly once
  during construction. Their visible formats and command fields are unchanged.
- LoRa parses each received packet into the existing Manager document using RadioHead's actual
  received length. This removes the former 300-byte fragment stack document; batch records also
  parse directly from SD, avoiding a 2 KB temporary stack buffer.
- Digital pin sampling retains the old sorted/deduplicated key order but reuses two compact vectors;
  it no longer frees and reallocates `std::map` nodes on every measurement.
- Tipping-bucket hourly history is a fixed 60-minute, 480-byte ring. The prior two deques appended
  on every measurement and could require thousands of heap nodes at a 500 ms sample interval.
- LoRa fragment reassembly lazily allocates one 2,000-byte document on the first fragmented receive,
  retains it, and reuses it. Transmit-only nodes allocate none; corrupt headers cannot request an
  arbitrary document size.
- SD batch files append until explicitly acknowledged. MongoDB and LoRa clear a batch only after
  all records succeed, and LTE/WiFi/radio power gating retries once the count is at or above the
  threshold. Failed SD row/record writes roll back to the previous file size, avoiding a corrupt
  tail without adding a staging buffer.
- RemoteManager retained controls use a caller-sized 256-byte receive buffer, and ThingSpeak uses a
  1,024-byte eight-field message buffer, reducing peak automatic storage by 1,744 B and 976 B.
- AS7262, AS7263, and AS7265X conversion waits and their virtual-register bridges are bounded.
- The shared EZO reader now copies payload bytes directly into its existing 33-byte buffer. The old
  per-byte `strncat()` call could scan beyond a non-terminated character, and a short response could
  index outside the response-code table. The table and scratch byte were removed from each object.
- WiFi credential flash writes pad their final 32-bit word explicitly and propagate bounded NVM
  readiness failures instead of reporting success unconditionally.
- Function summaries remain available but are disabled in field sketches.

## Heap telemetry added to WISP and Dendrometer sketches

`Loom_MemoryDiagnostics` records checkpoints throughout setup, measure, package, SD, LTE/MQTT, RTC,
sleep, wake, and network-time phases. It does not probe the heap with malloc/free.

Each `[MEM]` line reports:

- `gap`: current program-break-to-stack-marker distance.
- `min`: minimum checkpoint gap observed.
- `brk`: program break; a monotonic rise means the heap high-water increased.
- `heap_free`: free bytes already inside newlib-nano's heap arena.
- `frag`: free bytes in holes below the top free chunk.
- `holes`: allocator free-chunk count.
- `contig`: top free chunk plus currently unclaimed gap, an estimate of the present contiguous
  allocation ceiling before future stack demand.
- `min_contig`: minimum observed contiguous estimate.
- `json/capacity` and `ovf`: ArduinoJson pool use and overflow state.
- `cycle`, uptime, phase, and current batch index.

The release WISP, Dendrometer hub, and Dendrometer node sketches emit these checkpoints to Serial
only, so their established JSON, MQTT, and CSV schemas remain unchanged. `addToPacket()` remains an
explicit opt-in for a dedicated diagnostic build that intentionally wants to persist the latest
values. Per-function SD summaries are disabled on the endurance hub to avoid extra
file-open/write/close traffic.

Interpretation:

- Rising `brk` after each measure indicates sensor/library heap growth.
- Stable `brk` with rising `frag`/`holes` indicates allocation/free churn and fragmentation.
- A single drop at the first six-hour upload that then stabilizes is retained initialization, not a
  continuing leak; it still reduces the safety margin.
- Loss immediately after `pre_sd` points to SD/file/serialization work.
- Loss between `pre_mqtt` and `post_mqtt` points to LTE, TinyGSM, ArduinoMqttClient, or batch
  publishing.
- Stable diagnostics through `pre_sleep` with no `post_wake` points away from RAM and toward RTC,
  interrupt, or power sequencing.

`contig` is not permission to use all reported bytes: the current checkpoint stack is shallow.
Future nested calls still require their own stack plus an emergency margin.

## Daylight-saving and RTC changes

The previous month-only DST test was replaced with the North American boundary rule:

- start: second Sunday in March at 02:00 local standard time;
- end: first Sunday in November at 02:00 local daylight time;
- transitions are compared in UTC, including the repeated fall-back hour.

`getLocalTime()` evaluates DST for the timestamp being converted, not whatever time happens to be
in the RTC when conversion occurs. Compile time is converted from local wall time to UTC with the
same boundary rule. HST is corrected to UTC-10 and ACST to UTC+09:30. Network timezone offsets now
preserve half-hour zones.

The RTC sleep path also clears stale alarm flags, verifies the alarm-register readback, checks the
active-low interrupt line before standby, and refuses unbounded sleep when no valid wake source is
attached.

## OPEnS_RTC versus RTClib source audit

The supplied `OPEnS_RTC.cpp/.h` is a JeeLabs-derived OPEnS fork, not merely an older copy of the
currently installed Adafruit library. It has no version manifest, so the exact release age cannot
be established from those two files alone. Repository history does, however, independently record
the Adafruit migration as not working and later restores OPEnS_RTC for deployed-unit stability.

The strongest wake-behavior difference is the alarm-arm transaction:

- OPEnS_RTC first writes the four Alarm 1 registers. Its following two-register arm transaction
  enables `A1IE`, forces `INTCN`, and auto-increments from the control register to write the complete
  status register to zero. This unconditionally clears `A1F` and `A2F`, releasing the active-low
  `INT/SQW` line before the next sleep.
- Installed Adafruit RTClib 2.1.4 writes the Alarm 1 registers and enables `A1IE`, but intentionally
  does not clear `A1F` or `A2F`. Its `clearAlarm()` calls are separate read-modify-write I2C
  transactions. A caller that ports only `setAlarm()` to `setAlarm1()` therefore changes behavior
  even though the calls look equivalent.
- The hardened OPEnS-backed Hypnos path explicitly disables both alarms, clears both flags, programs
  Alarm 1, reads the alarm and mode back, clears a possible race flag, validates the active-low
  line, and aborts standby if no usable wake source exists. This reproduces the important OPEnS
  invariant without writing unrelated status bits to zero.

The supplied old library was not automatically more RAM efficient overall:

- It performs no heap allocation in the RTC driver.
- Its `DateTime` object embeds a 24-byte text buffer and is approximately 30 bytes on SAMD21. The
  installed RTClib `DateTime` stores only six one-byte date/time fields. The three persistent
  timestamps in Hypnos therefore consume about 90 bytes with OPEnS_RTC versus 18 bytes with
  RTClib, before counting temporary stack timestamps.
- RTClib allocates one 16-byte `Adafruit_I2CDevice` object during the one-time RTC `begin()`, plus
  allocator metadata. Hypnos guards initialization with `RTC_initialized`, so this is a retained
  setup allocation rather than recurring heap churn. Loom does not call RTClib's
  `DateTime::timestamp()` String-returning API.

There is also a transaction-level SAMD21 risk independent of heap fragmentation. OPEnS_RTC ends
the register-address write with an I2C STOP before reading. RTClib/Adafruit BusIO uses a repeated
start for multi-byte `now()` and alarm reads. Before this pass, both eventually reached unbounded
polling loops in the package's SAMD `Wire`/SERCOM core. The beta backport now rejects a non-idle,
non-owned bus, checks SERCOM error flags, and caps address/data/command waits at 100 ms. A stuck
SDA/SCL line now returns an I2C failure for the normal Loom retry/power-cycle path instead of
freezing the MCU. Hardware fault-injection testing is still required.

The supplied old OPEnS implementation also has costs that should not be copied blindly:

- `begin()` always returns true and does not probe the DS3231.
- I2C return codes and byte counts are not checked.
- Arming an alarm writes the entire status register to zero, which also clears `OSF` and disables
  the 32 kHz output instead of changing only the alarm flags.
- The day-of-week alarm path applies `DY/DT` to the hour byte; the date-matched Alarm 1 path used by
  Hypnos is unaffected.

That recommendation is now implemented as a hardened OPEnS_RTC fork. It retains direct `Wire`, the
STOP-separated read transaction, alarm-register-first programming, and the contiguous
control/status arm write used by the stable field version. It does not copy the unsafe portions:

- `DateTime` is a six-byte value type. The legacy `text()` API uses one shared 24-byte buffer, and a
  caller-owned `text(buffer, length)` overload is provided for reentrant code.
- No `String`, `new`, `malloc`, Adafruit BusIO object, or other dynamic allocation is used.
- `begin()` probes the DS3231. Writes, reads, byte counts, BCD values, dates, and alarm readback are
  checked, with `lastOperationSucceeded()` and `lastI2CError()` available to callers.
- Arming preserves `OSF` and `EN32kHz`, clears only `A1F/A2F`, and verifies the enable bit,
  interrupt mode, and cleared flags after the contiguous write.
- The broken weekday `DY/DT` encoding is corrected; RTClib-style Alarm 1/2 names are supported so
  the guarded Hypnos logic does not have to regress.
- Temperature conversion polling has a one-second software bound.

The core timeout is a beta safeguard, not proof of electrical bus recovery. It bounds the software
wait and lets Loom continue, but it cannot release a line physically held low. Bench testing should
hold SDA and SCL low separately, verify the 100 ms failure return, remove the fault, power-cycle the
sensor rail, and confirm that the next transaction succeeds.

## Remaining risks and next decision points

1. TinyGSM and ArduinoMqttClient still use Arduino Strings internally. Their first-use behavior and
   reconnect paths must be judged from the lifecycle trace before vendoring or rewriting them.
2. ArduinoMqttClient retains topic and credential String capacities. Same-sized repeated topics
   should reuse capacity, but changing topic/credential sizes during runtime can still reallocate.
3. The 2 KB Manager JSON pool is long-lived. Moving it from heap to static memory would make its
   placement deterministic but would not create 2 KB of additional RAM.
4. The Loom Arduino package links unused board/library globals into LTE sketches, notably
   `WiFiSocket` (620 B), `WiFi` (116 B), and the unused Feather `Serial5` object (572 B). Recovering
   those bytes cleanly requires splitting optional connectivity into separate Arduino libraries or
   changing the board core/variant; a sketch-level macro is not safe.
5. Mux auto-discovery allocates each discovered sensor once and retains it. Repeated
   `refreshSensors()` deletes and recreates sensors and should not be used in the field loop without
   reviewing the fragmentation trace.
6. A successful compile verifies types, capacities, and link layout but cannot prove RTC wake or
   modem behavior. Hardware soak testing remains required.
7. The new Wire/SERCOM timeout needs stuck-line, clock-stretching, and post-wake recovery testing on
   Feather M0/Hypnos hardware before the beta is promoted to stable.
8. Batch acknowledgement survives ordinary publish failures, but the counter and selected queue
   are RAM-only. A reset selects a new numbered batch instead of rediscovering the prior pending
   file; reset-resilient replay needs a separately validated storage design.

## Field acceptance criteria

- Run at least 16 hours (twice the historical window); the planned three-week run is stronger.
- Include at least two batch uploads, so testing crosses the six-hour first-publish boundary more
  than once.
- No progressive decline in `min_contig`, no progressive increase in `frag`/`holes`, and no
  unexplained program-break growth after the first complete cycle of each subsystem.
- No JSON overflow, failed allocation symptom, short MQTT write, failed SD seek/write, or partial
  batch record.
- Maintain at least 2 KB above the largest observed nested demand; 4 KB is preferable with TinyGSM
  and SD active.
- Every `pre_sleep` checkpoint has a corresponding `post_wake` and subsequent heartbeat.
- Run an old-RTC control with identical memory instrumentation. That cleanly separates wake-library
  behavior from RAM behavior.

## Build verification

Clean builds passed for `loom4:samd:adafruit_feather_m0`:

- Wisp_Batch_Logging: 150,724 bytes flash; 7,928 bytes static SRAM.
- Wisp_Mux_BatchLogging: 176,212 bytes flash; 7,912 bytes static SRAM.
- WispV2_Deploy_2026: 177,676 bytes flash; 7,800 bytes static SRAM.
- DST boundary test: 36,656 bytes flash; 5,892 bytes static SRAM.
- OPEnS_RTC legacy/modern compatibility test: 17,308 bytes flash; 3,568 bytes static SRAM;
  compile-time assertion confirms `sizeof(DateTime) == 6` on Feather M0.
- Basic Hypnos sleep example: 67,232 bytes flash; 6,256 bytes static SRAM.
- WiFi connectivity example: 75,664 bytes flash; 6,836 bytes static SRAM.
- Ethernet connectivity example: 59,136 bytes flash; 6,468 bytes static SRAM.
- Dendrometer hub with Serial memory telemetry: 132,436 bytes flash; 8,268 bytes static SRAM.
- Dendrometer node with Serial memory telemetry: 111,308 bytes flash; 7,428 bytes static SRAM.
- LoRa single-packet receiver before first fragment workspace allocation: 71,256 bytes flash;
  6,860 bytes static SRAM. Its first fragmented header allocates and retains 2,000 heap bytes. The
  static increase moves the RadioHead reliability manager out of the heap; total runtime ownership
  is explicit and allocator metadata is eliminated.

Focused compatibility builds from the final professionalism pass also passed with Loom-owned
warnings filtered separately from board-core and third-party dependency warnings:

- ThingSpeak: 92,732 bytes flash; 7,244 bytes static SRAM.
- RemoteManager over LTE: 110,476 bytes flash; 7,040 bytes static SRAM.
- Freewave transmit: 63,520 bytes flash; 6,584 bytes static SRAM.
- LoRa batch transmit: 92,436 bytes flash; 7,016 bytes static SRAM. As with the receiver, the
  272-byte static increase replaces the equivalent long-lived RadioHead manager heap allocation.
- OLED: 67,528 bytes flash; 6,400 bytes static SRAM.
- Max Servo: 93,492 bytes flash; 9,792 bytes static SRAM. Its heap-owned Servo object is 32 bytes
  smaller because the duplicate actuator name was removed.
- SDI-12: 65,912 bytes flash; 6,300 bytes static SRAM.
- Stepper: 56,272 bytes flash; 6,220 bytes static SRAM.
- Fixed evaporometer: 76,088 bytes flash; 6,296 bytes static SRAM.
- Digital: 55,680 bytes flash; 6,168 bytes static SRAM.
- Tipping bucket: 68,620 bytes flash; 6,804 bytes static SRAM.
- AS7262 / AS7263 / AS7265X: 56,408 / 58,032 / 59,296 bytes flash; 6,176 / 6,176 /
  6,200 bytes static SRAM.
- VCNL4020: 81,156 bytes flash; 6,324 bytes static SRAM.

These builds validate source/API compatibility and linked SRAM layout. DS3231 register behavior,
active-low wake, oscillator-loss handling, and long-duration stability still require the planned
hardware soak test.
