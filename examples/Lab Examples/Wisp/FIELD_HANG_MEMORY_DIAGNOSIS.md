# Loom field-hang and 32 KB RAM diagnosis

Date: 2026-08-19  
Analyzed checkout: `4.9-joshfixes` at `7ebbaa68834e33350e5471ffdb0dccdcac706de3`  
Target: SAMD21 Cortex-M0+ with 32,768 bytes SRAM  
Status: diagnosis retained; RAM fixes, diagnostics, DST correction, and hardened OPEnS_RTC path
implemented for Feather M0 build/soak validation

## Implementation update — 2026-08-20

The original diagnosis below is preserved so the causal reasoning and before-state measurements
remain reviewable. The recommended stable RTC direction is now implemented in the tracked
`dependencies/OPEnS_RTC` fork and integrated into Loom:

- field-proven direct-`Wire`, STOP-separated reads and contiguous alarm arm behavior are retained;
- `DateTime` is six bytes and the driver performs no heap allocation;
- DS3231 presence, I2C results, short reads, values, alarm modes, and alarm readback are checked;
- the old full-status clear and weekday-bit bug are corrected;
- Hypnos fails closed instead of entering standby after an RTC programming/readback error;
- RTClib-style alarm names remain source-compatible with the guarded 4.9 Hypnos code.

The three final instrumented WISP builds use 7,928 B, 7,912 B, and 7,800 B of static SRAM. See
`docs/RAM_OPTIMIZATION_REPORT.md` for the final lifecycle fixes, exact linked sizes, and acceptance
criteria. Hardware wake and long-duration soak validation remain required.

The Cortex-M0 second pass also removed recurring Digital map and tipping-bucket deque allocation,
made LoRa fragment reassembly allocate once and retain its workspace, reduced RemoteManager and
ThingSpeak stack buffers, bounded all AS726x conversion/bridge polling, and instrumented both
Dendrometer roles. A SAMD21 Wire/SERCOM timeout patch was evaluated but is retained only as an
inactive investigation snapshot; the beta uses the official Loom 4.9 core. Lower-core stuck-line
behavior therefore still requires fault-injection, watchdog, and sensor-rail recovery validation.

The installed Sensirion SEN5x/Core/SEN66 drivers do not use Arduino `String` or `std::string`.
The packaged DFRobot multi-gas driver does update a global Arduino `String` from
`dataIsAvailable()` on every sample. Loom now uses a protocol-compatible fixed-byte availability
check and literal gas-type mapping, preserving the existing gas field names without entering that
vendor String path.

## Forensic update — 2026-08-27 (current 90-minute report)

This section supersedes the pre-hardening RAM sizes, stack frames, and first-batch timing diagnosis
below for the current checkout. Those older sections remain as before-state history. The initial
forensic audit changed no production source or stored format; the subsequently authorized beta
safeguards are recorded below.

### Current conclusion

The new approximately 90-minute failure is unlikely to be the original LTE/MQTT high-water event
or a normal-cycle stack/heap collision:

- a 72-record batch with five-minute sleeps cannot reach its first publish before roughly six
  hours; after 90 minutes the batch is only around record 16–18;
- before the batch threshold, LTE is powered off, `Loom_LTE::package()` does not call TinyGSM, and
  `networkTimeUpdate()` returns immediately because the network is disconnected;
- the active Sensirion SEN5x/SEN66 libraries contain no Arduino `String`, `std::string`, or dynamic
  allocation, and the fixed DFRobot availability/type path bypasses its recurring vendor String;
- the 2,000-byte ArduinoJson pool is allocated once by the global `Manager`; `doc.clear()` reuses
  that pool rather than reallocating it each cycle;
- SD CSV and batch JSON are streamed directly to `File`; the old overlapping 2 KB automatic
  buffers no longer exist.

The leading diagnosis is now a **blocking peripheral transaction amplified by field debug
logging**, with conditional mux reconstruction as the main remaining heap-fragmentation path.

### Current linked RAM and stack evidence

Exact Feather M0 builds using
`loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off` report:

| Sketch | Static SRAM | Nominal SRAM after `.data`/`.bss` |
|---|---:|---:|
| WISP direct batch | 7,928 B | 24,840 B |
| WISP mux batch | 7,912 B | 24,856 B |
| WISP v2 | 7,800 B | 24,968 B |

The reported approximately 17 KB free RAM after constructors is consistent with the linked image
and the persistent 2,000-byte JSON pool, 1,664-byte `SDManager`, network objects, vectors, and
allocator metadata. Exact linked object sizes include `Manager` 184 B, `Loom_Hypnos` 144 B,
`Loom_LTE` 384 B, `Loom_Multiplexer` 100 B, `Loom_SEN55` 116 B, and `Loom_SHT31` 80 B.

The current `-fstack-usage` WISP mux build has no large normal-cycle Loom frame: logger formatting
is about 360 B, SEN55 measurement 192 B, Manager measure/package 136 B, and SD logging 144 B.
LTE connect is about 352 B and batch MQTT streaming 248 B, but neither runs at 90 minutes in a
normal 72-record deployment. This substantially lowers stack collision and raw exhaustion as the
current explanation.

### Why the ordinary logger was the strongest 90-minute timing match

Before the minimum-change safeguards, the WISP sketches called `ENABLE_SD_LOGGING`. A healthy
direct-sensor cycle executed roughly 17–18 ordinary `LOG` calls; mux gas boards added more. Each
call did all of the following:

1. formats into a bounded 256-byte stack buffer;
2. reads the DS3231 to add a timestamp;
3. opens `/debug/output_N.log`, appends one line, and closes it.

At cycle 16–18 this was already hundreds of extra RTC reads and SD open/append/close operations,
and commonly tens of kilobytes of debug-file growth. A filesystem allocation boundary, marginal
card, marginal I2C device, or stuck SDA/SCL state can therefore produce a repeatable-looking
90-minute failure without any elapsed-time bug or heap leak. The guarded beta removes this extra
SD/RTC debug path while leaving normal sensor CSV and batch JSON logging enabled.

This checkout deliberately uses the official Loom 4.9 SAMD core. Its `Wire`/`SERCOM` master waits
for bus ownership, address completion, receive data, and synchronization with unbounded `while`
loops. A wedged I2C bus therefore presents as a permanent MCU hang. The same official SERCOM SPI
implementation also has unbounded peripheral-completion waits. The inactive core-patch snapshots
in `dependencies` remain notes only and must not be selected by the beta.

### Remaining real heap-fragmentation path

The mux owns auto-loaded sensors with `new`. In the healthy path it allocates them once during
initialization and reuses them across every wake. If the mux is not initialized, however,
`power_up()` retries `initialize()`, which deletes the current sensor set, scans, and allocates new
objects. An intermittent mux/power/bus failure can therefore create genuine repeated heap churn.

Treat mux reconstruction as causal only if the log repeatedly shows `Multiplexer found`, `Found
I2C device`, or `Loaded sensor` after normal wakes, or if cycle-aligned `contig` falls and `frag` or
`holes` rises. A stable `brk`, `contig`, `frag`, and `holes` trace across the same phase rules it
down much more strongly than a single free-RAM number.

### Ranked current hypotheses

| Rank | Hypothesis | What would confirm it |
|---:|---|---|
| 1 | I2C/SPI/SD operation blocks; the former debug logger multiplied exposure | A phase loses its matching post-marker while memory remains stable; reset cause and the last phase identify the path |
| 2 | Mux intermittently fails and repeatedly deletes/recreates sensors | Repeated mux discovery/loading messages plus falling `contig` or rising `frag`/`holes` |
| 3 | Power or brownout/reset, especially with sensor rails retained during sleep | A new setup/boot sequence, low battery values, or reset-cause evidence instead of a stationary last phase |
| 4 | 2,000-byte JSON pool overflows on a populated mux | `post_package` reports `ovf=1`; this explains incomplete output, not by itself a hard hang |
| 5 | TinyGSM/ArduinoMqttClient fragmentation or an uncovered LTE wait | Failure moves to the first upload when the batch threshold changes; cannot explain a normal 90-minute record-18 failure |
| 6 | Stack/heap collision in the constrained-buffer code | Current linked stack report does not support it; require a falling stack/heap gap or fault evidence |

### Other beta correctness findings

- `SDManager::current_batch` is RAM-only. After an MCU reset it starts at zero and is not rebuilt
  from an existing nonempty batch file. The later file/count mismatch is retained safely rather
  than deleted, but upload recovery is incomplete. This is not the cause of a pre-reset hang.
- All guarded WISP sketches now use a 16-second active watchdog. One SEN66 measurement waits about
  10 seconds and a T6793 adds about 1.5 seconds, before mux switching, I2C, logging, and filesystem
  latency. The intentional long sensor loops feed an already-enabled watchdog only between bounded
  units of useful work; a lower-level stuck transaction still produces a diagnostic reset.
- The active ArduinoMqttClient is currently selected from the sketchbook (`0.1.8`), not the package
  dependency. Its String activity is relevant at setup/upload, and the override is a beta
  reproducibility risk, but it is inactive during the record-18 steady-state window.
- The WISP sketch calls `reattachRTCInterrupt()` explicitly, and `sleep()` calls it again in
  `pre_sleep()`. This duplicates work but does not allocate. It should be simplified only after the
  hardware trace establishes the wake behavior.

### Implemented minimum-change beta safeguards — 2026-08-27

The diagnosis-driven safeguards are now applied to all three canonical WISP sketches and their
packaged mirrors:

- ordinary Logger output is Serial-only in these endurance sketches; `hypnos.logToSD()` remains
  enabled, so sensor CSV and batch JSON filenames, keys, labels, and byte formats are unchanged;
- RTC timestamps are disabled only for ordinary WISP Logger messages, eliminating an RTC I2C read
  for every debug line while leaving packet timestamps unchanged;
- a 16-second SAMD21 watchdog covers steady-state measurement, packaging, display, CSV/batch SD,
  below-threshold batch checks, RTC programming, and the explicit interrupt reattach;
- SEN55, SEN66, and DFRobot retry loops feed an already-enabled watchdog between bounded units of
  useful work, so their intentional 10–20 second sampling sequences do not cause false resets;
- cellular/MQTT and network-time windows remain outside the 16-second watchdog because legitimate
  modem operations can exceed it; they remain distinguished by the existing phase markers;
- every `[MEM]` line now includes the SAMD reset-cause register as `reset=0x...`, distinguishing a
  watchdog/brownout/reset cycle from a stationary peripheral hang;
- temporary heap/reset checkpoints are gated by `LOOM_WISP_BETA_DIAGNOSTICS`, their definitions are
  enclosed by `BEGIN/END LOOM_BETA_DIAGNOSTICS`, and every in-flow call is tagged
  `LOOM_BETA_DIAGNOSTIC` for mechanical canonical cleanup;
- official `Wire` and `SERCOM` remain unchanged.

The final exact Feather M0 builds remain at 7,928 B, 7,912 B, and 7,800 B of static SRAM. All three
compile successfully. The first soak comparison should use these guarded sketches against the
previous field binary; re-enable ordinary SD debug logging only in a deliberately short diagnostic
run.

## Historical pre-fix diagnosis (retained for traceability)

The remainder of this section records the baseline risks that motivated the hardening work. Its
RTClib, 2 KB MQTT payload allocation, long LTE retry, large stack-frame, and ordinary SD-debug
logger descriptions are historical; they are not claims about the guarded current build. The
current diagnosis and implemented safeguards are documented above and in
`docs/RAM_OPTIMIZATION_REPORT.md`.

The evidence does not support a single universal "eight-hour bug." There are at least two failure classes:

1. The RTClib migration can produce a true sleep/wake failure. In that case the MCU reaches standby but the DS3231 interrupt does not wake it correctly. Reverting to the proven OPEnS_RTC path is a reasonable release-stabilization decision.
2. WISP v1 has an independent RAM-risk path that remains relevant even with OPEnS_RTC. Its first 72-record LTE/MQTT batch occurs at six hours. That operation adds a 2,648-byte stack frame and asks ArduinoMqttClient for a new 2,000-byte heap buffer. The MQTT library does not check whether that allocation succeeded before calling `memcpy`. Regular SD and sensor processing churns the heap before this point. A failed allocation or stack/heap collision on Cortex-M0 can therefore appear as an unexplained hang near the first upload window.
3. Current 4.9 LTE behavior can look dead without actually faulting. With no registration, `connect()` performs five 600-second `waitForNetwork()` calls, plus diagnostics and inter-attempt delays, while the watchdog is disabled. The equivalent 4.8 path called TinyGSM's 60-second default wait once and returned on failure. This is approximately a 50-fold increase in the base registration wait.

The leading WISP conclusion is consequently not "the SEN55 String alone." It is:

> The first scheduled LTE/MQTT boundary is the trigger to isolate. Current 4.9 can either spend tens of minutes inside uncovered LTE waits or expose an unsafe first-use stack/heap demand; the trace must distinguish them.

This explains why removing one `std::string` may improve runtime without making the design safe. It also gives a fast falsification test: change the batch from 72 samples to 6 while leaving the five-minute sampling interval unchanged. If the failure moves from roughly 6–8 hours to roughly 30–90 minutes, the batch/LTE path is causal. The phase trace then distinguishes an LTE wait from a first-write memory fault.

### Why "about eight hours" is plausible

The current code supplies a concrete near-eight-hour timeline under poor LTE conditions:

1. Initial `manager.initialize()` may spend about 50 minutes in LTE registration retries.
2. Record 1 is then logged immediately; reaching the wake before record 72 requires 71 five-minute intervals, or 5 hours 55 minutes.
3. LTE is deliberately powered on during that wake because the batch count is 71. Registration may consume another approximately 50 minutes before the sketch can collect and publish record 72.

That totals roughly **7 hours 35 minutes**, before modem boot time, raw-AT diagnostics, MQTT broker retries, SD activity, and observation/reporting delay. This does not prove which of the two batch-boundary failures occurred, but it shows that the reported timing is produced by code paths rather than by an eight-hour MCU timer rollover.

### 4.8 versus 4.9: introduced risks versus inherited risks

| Path | 4.8 behavior | Current 4.9 behavior | Diagnosis |
|---|---|---|---|
| MQTT transmit buffer | ArduinoMqttClient default: 256 B | Explicit `setTxPayloadSize(2000)` | **4.9 adds 1,744 B of retained heap demand on first publish** |
| MQTT allocation failure | Unchecked `malloc(256)` in installed library | Unchecked `malloc(2000)` in installed library | Existing unsafe dependency made materially more dangerous |
| No-registration LTE wait | One default 60-second TinyGSM wait, then return | Five explicit 600-second waits, plus diagnostics/delays | **4.9 increases base blocking window from ~1 min to ~50 min** |
| RTC | OPEnS_RTC | RTClib with subsequent alarm fixes | 4.9-specific wake regression class |
| SD CSV conversion | Per-value `as<String>()` | Per-value `as<String>()` | Inherited churn, not a 4.9 introduction |
| SD/batch stack arrays | 2 KB buffers in `log()` and `logBatch()` | Same basic 2 KB buffers | Inherited stack risk |
| SD config read | Always allocates/frees 5,000 B | Allocates exact file size with checks | Current code is safer, but changes allocator history |

The MQTT change was introduced in the 4.9 fixes and is especially important. The 4.8 default buffer was too small for a normal WISP JSON payload and could silently truncate it; simply reverting that line is therefore not a complete fix. The appropriate direction is the library's known-size streaming `beginMessage(topic, size, ...)` overload, which writes directly to the network client without allocating the transmit payload buffer.

## What is established

### Actual binary RAM baseline

The current WISP v1 sketch (`examples/Lab Examples/Wisp/Wisp_Batch_Logging/Wisp_Batch_Logging.ino`) was compiled for:

`loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off`

The ELF contains:

| Region | Bytes | Share of 32 KB |
|---|---:|---:|
| `.data` | 976 | 3.0% |
| `.bss` | 9,116 | 27.8% |
| Static SRAM total | **10,092** | **30.8%** |
| Nominal remainder for constructors, heap, and all stack | **22,676** | **69.2%** |

The nominal remainder is not the usable safety margin. Global C++ constructors allocate before `setup()`, and the linker summary does not include those heap allocations or runtime stack depth.

Large static objects already included in the 10,092-byte baseline include:

| Object | Bytes |
|---|---:|
| `mqtt` | 1,348 |
| `lte` | 452 |
| `manager` | 216 |
| `hypnos` | 244 |
| USB endpoint cache buffers | 896 combined |
| `Wire` | 564 |
| `Serial1` | 572 |

Known constructor-time/runtime heap commitments include:

- `Manager` constructs a `DynamicJsonDocument` with a 2,000-byte pool.
- `Loom_Hypnos` requests exactly **2,584 bytes** from `operator new` for `SDManager` in the linked WISP v1 image.
- Hypnos creates 23 `std::map` timezone nodes; on this 32-bit ABI their node payload is approximately 24 bytes each, or roughly 552 bytes before allocator metadata. The interrupt map and Manager module vector also allocate nodes/storage.
- LTE requests **228 bytes** for the selected SARA-R4 TinyGSM adapter (224 bytes for the R5 alternative).
- ArduinoMqttClient allocates another 2,000-byte transmit payload buffer on the first buffered publish and retains it for the object's lifetime.

The directly accounted live constructor heap is therefore approximately **5.4 KB** before allocator metadata, Logger/vector details, and any heap owned by dependent libraries. Static SRAM plus that known heap is about 15.5 KB. Adding the first 2 KB MQTT buffer and the 2,648-byte publish frame brings the directly accounted batch-boundary total to about 20.2 KB, leaving roughly 12.5 KB for deeper call frames, library heap, temporary strings, and allocator overhead.

This is a constrained design, but it is an important calibration: the directly visible allocations do **not** prove that a clean heap must be out of memory. A hard failure at first publish would require fragmentation, unaccounted allocations, deeper stack use, corruption, or some combination. That is why the allocation result and phase trace are required rather than assuming SEN55 fragmentation from the elapsed time alone.

### Measured stack frames

The WISP sketch was rebuilt with GCC `-fstack-usage`. Important frames are:

| Function | Own frame |
|---|---:|
| `Loom_MongoDB::publish(Loom_BatchSD&)` | **2,648 B** |
| `SDManager::logBatch()` | **2,288 B** |
| `SDManager::log(DateTime)` | **2,120 B** |
| `Manager::display_data()` | **2,064 B** |
| `FunctionInstrumentor` constructor | **1,040 B** |
| `Logger::genericLog(..., const char*)` | **680 B** |
| `Loom_SEN55::measure()` | **664 B** |
| `Loom_SEN55::logDeviceStatus()` | **608 B** |
| `MQTTComponent::connectToBroker()` | 344 B |
| `Loom_LTE::connect()` | 336 B |

Frames in a call chain overlap. In particular, `SDManager::log()` calls `logBatch()` before returning, so those two own frames alone consume approximately **4,408 bytes at once**, before SdFat/SPI callees. This happens on every sample. Function summaries also create transient 1,040-byte instrumentation frames at instrumented function boundaries. WISP v1 currently enables both SD logging and function summaries in field-style code.

The batch publish frame remains live while MQTT, TinyGSM, logging, and client code runs. Its 2,648 bytes therefore cannot be evaluated in isolation.

### Six-hour trigger

WISP samples every five minutes and sets `Loom_BatchSD batchSD(hypnos, 72)`. The first upload becomes ready after:

`5 minutes × 72 = 360 minutes = 6 hours`

The LTE batch state machine begins powering LTE for the pending upload just before that boundary, and the 72nd loop publishes the file. A reported hang "after about eight hours" is close enough to this deterministic first high-water event to rank it above an arbitrary timer rollover, especially when field observation time, boot/setup time, carrier retries, and log-reporting delay are considered.

Eight hours is not a natural SAMD21 `millis()` rollover. A 32-bit millisecond counter rolls over after about 49.7 days.

## Hypotheses (not mutually exclusive)

### H1 — first batch upload exposes OOM, fragmentation, or stack/heap collision

Confidence: **high that the path is unsafe and occurs at the right boundary; medium that it caused the field hang**

Evidence:

- The failure window is adjacent to the deterministic six-hour upload.
- `Loom_MongoDB::publish(batch)` has a 2,648-byte frame, primarily its 2,000-byte line buffer.
- `MQTTComponent` configures a 2,000-byte transmit payload. This explicit sizing is a 4.9 addition; 4.8 left the installed library at its 256-byte default. The new first-publish heap commitment is therefore 1,744 bytes larger.
- ArduinoMqttClient allocates that buffer lazily in `MqttClient::write()`:
  - `_txPayloadBuffer = malloc(_tx_payload_buffer_size);`
  - it immediately calls `memcpy` through the returned pointer;
  - there is no null check.
- MQTT also assigns the topic into a heap-backed `String` for each message.
- Network activity introduces TinyGSM response `String` growth and other transient allocations at the same time.

Expected signature:

- Accelerating only the batch threshold accelerates the failure.
- The last durable phase marker is LTE power-up, broker connection, or the first packet publish.
- A checked allocation at the actual ArduinoMqttClient boundary fails, or the MCU faults/resets during the first MQTT write.

### H2 — regular-cycle heap churn reduces the largest contiguous block

Confidence: **high that churn exists; medium that it alone causes the hang**

Evidence:

- `SDManager::log()` calls `keyValue.value().as<String>()` for every JSON value on every five-minute sample. A WISP packet contains many values, so this creates and destroys many temporary Arduino Strings per cycle.
- `Loom_SEN55::logDeviceStatus()` creates a 32-character `std::string` using `std::bitset<32>::to_string()`.
- SEN55 status logging already occurs at the end of `Loom_SEN55::measure()`, and the WISP sketch calls it a second time in each loop. It therefore may allocate twice per sample.
- `Loom_LTE::package()` queries `modem->getSignalQuality()` whenever `moduleInitialized` is true, but `power_down()` clears `powered` without clearing `moduleInitialized`. Normal batch cycles can therefore invoke TinyGSM's response parser against a powered-down modem, adding avoidable waits and temporary `String` activity.
- TinyGSM and ArduinoMqttClient use Arduino Strings during network operations.

Important distinction: repeated allocation is not automatically a leak. The decisive quantities are:

- current stack-to-heap gap;
- heap high-water mark;
- minimum-ever stack margin;
- largest contiguous allocatable block;
- allocation failures by requested size and phase.

`freeMemory()` alone does not prove or disprove fragmentation.

### H3 — oversized overlapping stack buffers cause collision without heap fragmentation

Confidence: **high design risk; medium as immediate cause**

The SD path uses approximately 4.4 KB in two directly overlapping frames. Logging and filesystem callees increase that peak. The MQTT path has another 2.6 KB frame and runs while heap demand is highest. A stack collision can corrupt heap metadata, an object, a return address, or a peripheral driver and only manifest later.

Moving every large array from stack to independent globals would merely move the pressure. The preferred direction is one deliberately owned, size-checked scratch buffer or streaming APIs, with compile-time and runtime high-water checks.

### H4 — RTClib alarm/wake failure

Confidence: **high for the multi-team wake symptom; separate from WISP memory risk**

Repository history contains several RTC alarm clear/disable/readback fixes following the migration. A stale alarm flag, a low INT line, a missed reattachment, or a wrong Alarm 1 mode can make the unit appear dead in `sleep()`. Returning the stable release to OPEnS_RTC is justified while RTClib is tested separately.

Expected signature:

- The last phase marker is immediately before standby.
- No reset/boot marker follows.
- The RTC alarm time passes but the MCU never records a wake.
- The problem reproduces in an RTC-only sleep/wake sketch with SD, LTE, SEN55, heartbeat, and handshake removed.

### H5 — LTE/network code blocks for a long time and the watchdog does not cover it

Confidence: **high that the long uncovered block exists; unknown whether the field unit eventually returned**

Current `Loom_LTE::connect()` makes five attempts. Each attempt permits `waitForNetwork(600000L)`, so registration alone can block for approximately 3,000 seconds (50 minutes), before raw-AT diagnostics, PDP setup, and delays. `TIMER_DISABLE` is active across this path. WISP v1 has no sketch-level loop watchdog, and WISP v2 explicitly disables its watchdog before `mqtt.publish()`. Hypnos also disables the watchdog before powering LTE during wake.

The 4.8 implementation called `waitForNetwork()` with TinyGSM's 60-second default and immediately returned false when it timed out. The 4.9 LTE change is therefore not a small retry adjustment; it turns a roughly one-minute absence-of-network case into roughly fifty minutes of no sketch progress.

This hypothesis is especially relevant if the last phase marker precedes `connect()`, `waitForNetwork()`, broker connection, or QoS acknowledgement and the unit remains electrically awake.

### H6 — batch lifecycle loses failed uploads and obscures diagnosis

Confidence: **high; code-path defect, but not the direct cause of a CPU hang**

The original path truncated the batch on the next sample once the count reached 72, regardless of
whether MongoDB or LoRa had accepted the records. It also powered LTE/WiFi/LoRa only at the exact
threshold, so a failed attempt moved the count past the only retry point.

This defect is now corrected without changing the batch filename or newline-delimited JSON format.
Records continue to append while a batch is pending, readiness uses `>=`, and the network/radio
path retries at or above the threshold. `markPublished()` clears the file only after every record
reports success. CSV and batch writes also roll back a partially written row/record when the SD
write or sync fails. This is at-least-once delivery: a failure after the server accepts data but
before the local clear succeeds can produce a duplicate, but it no longer silently discards the
only local copy.

The batch count remains RAM-only and is not reconstructed from an existing batch file after reset;
a reboot selects a new numbered batch file. Reset-resilient queue discovery is still a release
limitation and should be addressed as a separately tested storage migration rather than inferred
from filenames during this compatibility-preserving beta.

### H7 — heartbeat/handshake adds bounded RAM pressure and receiver-state complexity

Confidence: **medium for the modified dendrometer build, low as a universal root cause**

The heartbeat document was changed from a 1,024-byte dynamic document to a 300-byte static document. A repository commit explicitly records a hub receive failure described as running out of memory during heartbeat deserialization; its receiver document was then increased from 300 to 1,500 bytes. The handshake/heartbeat branch consequently places a 1,500-byte JSON document on the LoRa receive stack, plus packet buffers and logging frames.

The later `implementation/mempool` work capped fragmented-packet working documents at 3 KB and checked pool allocations. That history is evidence that memory bounds were already a real concern. It does not prove that the entire mempool branch should be merged into the re-release.

The handshake retry loops are mostly bounded. One retry delay uses `currentTime + 10000 > millis()`, which is rollover-unsafe near the 49.7-day boundary, not an eight-hour explanation.

### H8 — field debug logging amplifies SD load and transient stack depth

Confidence: **high that the load exists; medium that it contributes to the reported SD failures**

This was originally diagnosed when both SD logging and function summaries were enabled and display
copied the full JSON packet. The current sketches keep function summaries disabled and stream
display/CSV/batch output, so the former 2 KB transient buffers are gone. Ordinary SD logging is
still enabled, however. Every `LOG` still reads the RTC and separately opens, appends, and closes
the debug-output file. The current risk is transaction count and unbounded peripheral waits, not a
large logger stack frame.

## Why the SEN55 fix was necessary but is no longer the leading 90-minute theory

The active SEN55 wrapper and Sensirion dependency now use fixed storage, the duplicate sketch-level
status call is gone, and CSV output streams variants without `as<String>()`. MQTT batch payloads
also use the known-size streaming path rather than a retained 2,000-byte payload allocation. These
changes remove the recurring SEN55/SD heap theory from the normal cycle. TinyGSM and MQTT Strings
remain relevant during setup and upload, but those paths are not active at a normal 90-minute
failure.

## Diagnostic instrumentation now present

Diagnostic-only instrumentation has been added to:

- `examples/Lab Examples/Wisp/Wisp_Batch_Logging/Wisp_Batch_Logging.ino`
- `examples/Lab Examples/Wisp/Wisp_Mux_BatchLogging/Wisp_Mux_BatchLogging.ino`
- `examples/Lab Examples/Wisp/WispV2_Deploy_2026/WispV2_Deploy_2026.ino`
- shared helper: `src/Diagnostics/Loom_MemoryDiagnostics.h`

All three sketches compile for `loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off`. The WISP v1 linked-image cost is:

| Metric | Baseline | Instrumented | Delta |
|---|---:|---:|---:|
| `.data` | 976 B | 976 B | 0 B |
| `.bss` | 9,116 B | 9,140 B | **+24 B** |
| Static SRAM | 10,092 B | 10,116 B | **+24 B** |
| Program text | 148,460 B | 150,252 B | +1,792 B |

Each phase line is emitted directly with fixed `Serial.print()` calls and contains:

`cycle, millis, reset cause, phase, gap, checkpoint minimum, delta, sbrk(0), contiguous ceiling/minimum, free
heap blocks, fragmented free bytes, hole count, JSON usage/capacity/overflow, batch count`

Setup checkpoints bracket global-constructor state, MQTT credential loading, Manager/LTE
initialization, and initial time sync. Loop checkpoints bracket measure, package, display, SD,
MQTT, RTC programming, standby/wake, and network time sync.

The current WISP sketches intentionally do **not** call `addToPacket()`. Diagnostics are Serial-only,
so no `Memory` object is added and the saved JSON/CSV/batch schemas remain unchanged. The helper
retains an optional `addToPacket()` API for a deliberately schema-changing diagnostic build.

Interpretation limits are important:

- `gap` is the stack-marker-to-current-program-break distance at a checkpoint. It is not total free heap.
- `min_ckpt` is the minimum of shallow checkpoints. The compiler stack report covers transient
  call depth; the old 4.4 KB SD and 2.6 KB batch frames no longer exist.
- `brk` movement reveals the allocator extending or contracting the heap boundary. A flat `brk` does not reveal holes below it.
- No diagnostic `malloc/free` probe is performed. A 2 KB probe immediately before MQTT would free a convenient 2 KB block that the real MQTT allocation could reuse, masking fragmentation.
- The actual allocation-success check belongs inside ArduinoMqttClient's first-write boundary, where the return value can be checked without a preparatory allocation.

Useful signatures in the Serial capture:

| Last/next marker | Meaning |
|---|---|
| `pre_sd` without `post_sd` | SD/SdFat/logBatch path stalled or faulted |
| `pre_mqtt` below `batch=72` without `post_mqtt` | steady-state battery check or the logger's RTC/SD write stalled; LTE/MQTT transport is not active |
| `pre_mqtt` at/above `batch=72` without `post_mqtt` | LTE/broker/batch streaming path stalled or faulted |
| existing `Device has awoken from sleep!` log, but no sketch `post_wake` | wake occurred; Manager module power-up, especially LTE, did not return |
| `pre_sleep` and no wake log | power-down logging, the second interrupt reattach, RTC/standby, or wake power-up; inspect the last ordinary log |
| steadily rising `brk` across identical phase/cycle points | retained allocation or heap high-water growth |
| stable `brk`, `contig`, `frag`, and `holes` at identical phases | strong evidence against recurring normal-cycle fragmentation |

## Diagnostic test plan before fixes

### 1. Make the failure phase durable

The sketch-level Serial markers above cover the outer operation boundaries. For a field unit without a continuously captured Serial port, add a compact retained numeric phase/reset code rather than opening a second SD log for every checkpoint:

1. wake entered;
2. measure begin/end;
3. package begin/end;
4. CSV log begin/end;
5. batch append begin/end;
6. LTE power-up begin/end;
7. broker connect begin/end;
8. batch packet number before/after publish;
9. RTC alarm programmed and verified;
10. standby entered;
11. wake ISR/post-sleep reached.

Also record a boot counter and SAMD21 reset cause before ordinary initialization. This separates hard fault/watchdog reset, brownout, and "slept but did not wake."

### 2. Record the right memory measurements

At the same phase boundaries record, without Arduino String formatting:

- stack pointer;
- heap break/high-water location;
- current stack-to-heap gap;
- minimum observed gap;
- Manager JSON `memoryUsage()`, `capacity()`, and `overflowed()`;
- requested size and result for important allocations;
- whether a 2,000-byte MQTT payload buffer is already committed.

If practical, wrap `malloc`, `realloc`, and `free` into a fixed-size in-RAM event ring, or add checked allocation at the library boundary. Avoid verbose allocation logging to SD inside the allocator.

### 3. Run accelerated discriminating tests

Run each configuration on at least two units:

| Test | One change | Interpretation |
|---|---|---|
| A | Batch size 72 → 6 | Failure moves near 30 minutes: batch/LTE path confirmed |
| B | LTE/MQTT omitted | Stable: network/batch path required |
| C | SD CSV and batch logging omitted | Stable: SD stack/churn path required |
| D | Function summaries disabled | Improvement: instrumentation stack/SD load contributes |
| E | SEN55 fixed-buffer status and no duplicate call | Improvement only: SEN55 contributes but is not sufficient |
| F | RTC-only sleep sketch, RTClib | Failure: independent RTC wake bug |
| G | Same RTC-only sketch, OPEnS_RTC | Stable: RTC migration causality strengthened |

Shortening only the sleep interval is useful for allocation-cycle testing. Changing the batch threshold independently is more diagnostic because it distinguishes "number of sample cycles" from "first LTE upload."

### 4. Define pass criteria

Before calling a beta stable:

- no unhandled allocation path;
- no single routine owns an unnecessary 2 KB automatic array;
- at least 6 KB measured worst-case stack/heap separation during SD and LTE high-water phases, or another explicitly justified margin;
- stable largest-block behavior across repeated sample and upload cycles;
- at least 10 complete accelerated upload cycles per unit;
- at least one test longer than the intended field upload interval and one multi-week soak;
- watchdog or other recovery covers every potentially unbounded external-device wait;
- RTC-only wake testing is passed independently of application testing.

## Original proposed fix order (preserved as before-state)

1. Change MQTT publishing to the known-size streaming overload and remove the 2,000-byte buffered-payload requirement. Check `beginMessage`, every write count, and `endMessage`; never permit unchecked `malloc` followed by `memcpy`.
2. Reduce LTE registration to a bounded field-appropriate budget and keep watchdog/recovery coverage active through modem boot, registration, broker connection, publish, and acknowledgement. Do not query RSSI while the modem is powered down. Log distinct timeout versus allocation/fault outcomes.
3. Make batch deletion acknowledgement-driven. Preserve a batch until all records are confirmed, and reconstruct/persist count across reset.
4. Remove the duplicate SEN55 status log and replace `std::string` conversion with fixed characters.
5. Remove `as<String>()` from SD CSV formatting; serialize values directly into a bounded writer/buffer.
6. Eliminate overlapping 2 KB SD stack buffers by streaming or using one deliberately owned scratch buffer.
7. Disable function summaries and full pretty-packet debug logging in field firmware; enable them only in an intentionally observed diagnostic build.
8. Add explicit JSON overflow checks and publish actual measured message lengths.
9. Run the new memory/phase diagnostics through the accelerated matrix; add reset-cause and an allocation-boundary event only where needed.
10. Keep OPEnS_RTC for the stable re-release; validate RTClib in an isolated wake test before reconsidering it.
11. Pull only narrowly reviewed allocation bounds from the mempool/handshake work rather than merging the entire branch.

## Original pre-fix conclusion

The release should be treated as having an unverified RAM safety margin even though the compiler reports only 30.8% static SRAM. Static percentage omits constructor-time heap objects, fragmented temporary allocations, overlapping call-stack frames, and the first-use MQTT buffer. The directly accounted budget does not prove exhaustion, which makes measurement at the real failure boundary essential.

The two leading WISP v1 sequences share the same deterministic boundary. The long LTE wait currently has the stronger direct timing match; the first-write memory path remains a release-blocking safety defect but needs the new trace to establish causality.

1. **Apparent hang:** `normal sampling` → `batch count 71 wake` → `LTE power-up` → `up to ~50 minutes of registration retries with watchdog disabled` → eventual return or continued broker delay.
2. **Hard memory failure:** `normal sampling and SD allocation churn` → `six-hour LTE wake/connect` → `2.6 KB publish frame + 2 KB lazy MQTT allocation + TinyGSM activity` → `allocation failure or stack/heap corruption` → `Cortex-M0 fault/hang with no final log`.

Both are testable in under two hours by reducing only the batch threshold and capturing the new phase/memory trace. They should be distinguished before attributing the field hang solely to SEN55 or before importing broad branch changes.

## Post-hardening conclusion

The deterministic hazards identified above are now addressed in code: MQTT and SD bodies stream,
large automatic arrays are removed, LTE waits are bounded, SEN55 recurring string work is removed,
and OPEnS_RTC is restored with checked alarm handling. AS726x sensor-level waits are bounded, while
the official lower-level SAMD21 I2C core still requires hardware fault testing. The historical
eight-hour timing remains useful because it identifies the first LTE/batch boundary, but it is no
longer evidence that one unresolved eight-hour timer exists.

The release question is now empirical: during the three-week Feather M0 soak, `brk`, `frag`,
`holes`, `min_contig`, JSON overflow, and phase pairing must stabilize after each subsystem's first
use. A missing `post_wake` with stable memory still implicates RTC/power/interrupt behavior; a
missing post-operation marker with declining contiguous memory implicates that operation. The new
Dendrometer Serial checkpoints provide the same distinction without changing stored schemas.
