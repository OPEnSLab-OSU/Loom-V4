# Loom 4.9.1 compatibility contract

This hardening release keeps the established 4.9 storage and transport contracts. Reliability,
bounds checking, ownership, RTC, and RAM behavior may change; valid existing data does not need a
format migration.

## Preserved contracts

- Manager packets retain the top-level `type`, `id`, `contents`, and optional `timestamp` fields.
  The packet counter remains the `Number` field inside the `Packet` module. Module entries remain
  `{"module": ..., "data": ...}` objects.
- Existing supported sensor/module JSON field names are unchanged. Fixes may correct a value that
  was previously stale, uninitialized, or associated with the wrong sensor. New TEROS 21/54 support
  uses explicit model/depth labels instead of the old unsupported generic TEROS interpretation.
  Opt-in ADS channel/voltage selection changes that sensor's selected output columns.
- SD CSV logs retain their existing header/column ordering, timestamp columns, comma placement,
  numbered `.csv` naming, and line termination.
- SD batch files remain newline-delimited compact JSON using the existing numbered `-Batch.txt`
  naming. Streaming and bounds-checking changes do not add an envelope or delimiter.
- MongoDB, ThingSpeak, and RemoteManager MQTT topic strings are unchanged. ThingSpeak query-field
  text and status suffix remain unchanged.
- RemoteManager retained-message JSON keys (`days`, `hours`, `minutes`, and `seconds`) and its
  `setSleepInterval`, `setRTC`, and `status` topics are unchanged.
- LoRa and Freewave continue to use MessagePack and the existing packet/header field names. Fixed
  buffer contents are now initialized deterministically, and LoRa reassembly storage is retained
  after first use, but packet framing is unchanged.
- WISP and Dendrometer memory checkpoints are Serial-only in release sketches; they do not add a
  diagnostic module or columns to stored packets.
- SD configuration keys and supported Hypnos interval layouts remain unchanged. Invalid or missing
  required values now fail closed instead of continuing with partially initialized state.

## Intentional non-format corrections

- ADS1115 default raw/voltage/differential labels and order remain unchanged. Channel mask and
  voltage-column selection are opt-in; failed selected reads use null instead of stale counts.
  Differential-only mode now reads its pairs; conversion polling has a checked 25 ms deadline.
  The vendor gain type/voltage maths remain, but its heap-allocating begin() is no longer called.
- SDI-12 keeps GS3/TER11/TER12 names/order and manual no-argument getters, validates complete
  addressed frames and advertised M! time/count, and parses signed values across data blocks.
  New TER21_i uses Temperature/Matric_Potential (kPa); TER54_i uses Temperature_D1..D4 and
  Volumetric_Water_Content_D1..D4 (m3/m3). Unknown models are not mislabeled. Failed readings use
  NaN/null. TER11/TER12's legacy VWC field still holds calibrated ADC counts; no silent calibration.
  Public response buffers remain 50 bytes; full C/R and CRC variants are not newly supported.
- SEN66 output labels/order remain unchanged. Missing fields use -1; valid zero and negative
  temperature remain valid. Integer sentinels are checked before scaling and each field has its
  own average. PM settling after cold startup adds awake time; retained sensor rails preserve a
  running measurement. Readiness/PM startup do not prove VOC/NOx convergence or sensor accuracy.
- SD text/pretty-JSON debug appends now check byte counts, flush, rollback and close. An uncertain
  append disables further SD debug writes for that boot while Serial and independent sample/batch
  paths remain available. Summaries skip unavailable/quarantined SD instead of delaying startup.
- MQTT closes failed frames and reconnects on later attempts; LTE restores a lost PDP session before
  reopening TCP. Narrow watchdog pauses retain the caller's state around blocking network calls.
  MongoDB connections use the physical Feather serial as their stable client ID (32 characters;
  verify broker length/ACL policy). Payloads/topics and the QoS 1 default are unchanged.
- Ordinary/custom batches retain Manager-based topics. A hub can opt into
  `usePacketIdentityForBatch()` so each saved record keeps its originating id.name/id.instance topic;
  malformed identities remain queued. Unterminated batch records are rejected as possibly torn.
  Debug output distinguishes broker acknowledgement from unverified database persistence.
- The Dendrometer hub now queues packets/heartbeats on SD and retries. Its node uses the intended
  LoRa constructor, bounded alignment and an optional-radio-safe send path. AS5311 field keys/order
  stay unchanged; invalid nonnegative values use -1 and invalid displacement serializes as null.
  Bad reads leave displacement state untouched; averaged positions handle 4095/0 rollover.
- CSV text with commas/quotes/CR/LF and nested JSON now receives valid CSV escaping rather than
  splitting into extra cells. Ordinary previously valid rows retain their layout. A changed
  header/schema selects a new numbered CSV without rotating or clearing its pending batch.
  An uncertain CSV rollback/close preserves that file before subsequent samples use a fresh CSV.
- LoRa receive waits cover the whole Loom receive loop and have a frame-count bound. Partial
  packets expire after inactivity; failed blocking batch receives release the outstanding
  estimate. Repeated headers replace counts and only the announcing sender's data decreases
  them. Arbitrary JSON objects/legacy heartbeats and padded MessagePack framing remain supported.
- Hypnos adds opt-in fixed-cadence `setSampleInterval()`; the established relative
  `setInterruptDuration()` API retains its meaning. Only SmartRock examples opt into the new
  schedule in this pass. UTC scheduling and local display stay separate.
- SEN55 failed or unavailable readings use `-1`, including PM fields that could previously look
  like zero or serialize a non-finite value. Valid zero remains zero. PM reads now follow a
  30-second settling window, increasing awake time, and firmware capability governs direct mode
  switching. Field names/order remain unchanged; long-term sensor health still needs bench tests.
- SDI-12 packaging now associates each discovered sensor with its own readings instead of
  repeating the last sensor's values.
- MQTT retained-message retrieval returns the payload instead of copying the topic string.
- Date/time and daylight-saving values follow the corrected RTC rules while retaining the existing
  timestamp keys and textual timestamp layout.
- Failed/truncated writes, malformed configuration, oversized retained messages, and invalid
  actuator commands are rejected rather than emitting partial or corrupt output.
- Batch records are retained after a failed MongoDB or LoRa attempt and are cleared only after the
  complete pending file succeeds. Readiness and radio/network power gating continue retrying above
  the configured threshold. This changes loss/retry behavior, not the numbered `-Batch.txt`
  filename or newline-delimited JSON bytes.
- A batch file whose parsed record count disagrees with its in-RAM counter is retained and reported
  instead of being acknowledged and cleared.
- Tipping-bucket hourly rainfall uses a fixed 60-slot minute ring. Field names and units are
  unchanged; the rolling-hour boundary has at most one minute of quantization instead of an
  unbounded per-measurement history.
- Digital pin keys retain the old sorted, deduplicated order while measurements reuse fixed-size
  storage instead of rebuilding a map.
- Loom AS726x measurement waits and the vendored spectral virtual-register bridges now return
  bounded failures. Official SAMD21 Wire/SERCOM remain unchanged and are not claimed to recover
  from every stuck-bus condition. Successful transaction bytes and sensor field names are unchanged.
- EZO parsing, radio receive workspaces, OLED traversal, Max command parsing, and WiFi flash-write
  checks reject malformed input without changing valid sensor fields, visible screen formats,
  command keys, or credential record layout.
- The allocation-free DFRobot gas adapter preserves the existing `O2`, `CO`, `H2S`, `NO2`, `O3`,
  `CL2`, `NH3`, `H2`, `HCL`, `SO2`, `HF`, `PH3`, `INV_TYPE`, and `NO GAS` field labels.
- ThingSpeak's previously declared but undefined batch overload now returns `false` with a
  diagnostic. It does not synthesize callback fields from unrelated batch JSON or alter the
  established single-packet topic/message format.

The built-in repository output-token audit found no renamed JSON/config key or module label versus
the tracked 4.9 baseline. The only removed bracket literal was OLED's internal `flatObj` scratch
object, which was never serialized or transported. Built-in module labels fit the 31-character
module-name bound; external custom labels longer than that bound must be shortened deliberately.

Any future change to a field name, column order, filename pattern, topic, delimiter, or wire schema
should be treated as a separate versioned migration and called out explicitly in this document.

## 2026-09-30 build follow-through

Analog retains raw-then-millivolt columns for each pin, including Vbat, by default.
`setOutputColumns(raw, millivolts)` is an explicit pre-logging choice; hidden packet columns
remain available through getters. Changing it during a session invokes the existing SD schema
rotation rules. ADS1115's optional channel/voltage output also retains its default schema.
UTC stays ISO 8601 with Z and local time stays ISO 8601 without a claimed UTC suffix; packet
identity remains available for replay/topic routing. No timestamp/identity migration is implicit.

The new ReedAnemometer module adds one new `WindSpeed_mps` field only when the application
registers that module. Its dry-contact calibration is supplied explicitly; zero is valid calm
wind and failed/invalid measurements are null. Two-second counting has limited low-speed
resolution and cannot distinguish calm wind from a disconnected contact. No existing module,
field, timestamp or radio/MQTT framing is renamed.

## 2026-09-30 remote commands, streaming and recharge

ThingSpeak captures each registered callback once and streams the same ordered field/timestamp
text using a small piece buffer. Online/offline remote-manager messages retain their exact JSON
bytes. A retained sleep command is cleared only after the RTC alarm has been armed successfully.
Intervals reject malformed or non-integer components and delays outside 1 second through 27 days.
DS3231 Alarm 1 compares day-of-month, not month/year; longer relative requests could otherwise
wake early. The established `void setInterruptDuration(TimeSpan)` API remains available;
`bool scheduleWake(TimeSpan)` lets callers handle failure explicitly.

Recharge mode is an explicit sketch choice. An attached policy checks voltage before ordinary
module restart, and `sleepForRecharge()` uses the existing verified external RTC wake without
restarting sensors or LTE. The example enters recharge at 3.7 V and resumes at 4.2 V, as requested;
thresholds remain configurable. Existing sketches without a policy retain their wake behavior.
No CPU brownout interrupt, internal-RTC replacement or automatic deployment change is implied.
Policies and voltage readers must outlive the Hypnos object that borrows them.

Optional SARA-R510M8S GNSS has explicit start/read/stop APIs. Existing LTE packaging stays RSSI
only; GNSS is not started automatically and adds no default metadata or CSV fields. The fix's
UTC remains separate from RTC/network ground truth and local timezone display. See
[optional-feature contracts in the style guide](STYLE_GUIDE.md#preserve-feature-contracts-while-simplifying) for bounds, cache freshness and acceptance limits.

`restartModem()` keeps its existing void signature but uses checked shutdown and aborts on an
unacknowledged power-off. LTE's header now forward-declares Manager; applications constructing
a Manager include `Loom_Manager.h` directly, as all current examples already do.

## 2026-09-30 optional status and failed-clock handling

The final LoRa constructor mode defaults to `DataAndHeartbeat`; existing calls and padded
framing remain. `HeartbeatOnly` sends a compact single-frame identity/status payload and refuses
measurement/batch sends. `sendHeartbeat()` can use a separate caller-owned status document.
No default measurement fields, packet numbers or SD queues are changed by heartbeat creation.

On a failed RTC read, Hypnos retains the `time_utc`/`time_local` keys with null values instead
of presenting a default date as ground truth. `logToSD()` returns false rather than logging
with that false date; callers must handle the failure before overwriting the in-memory sample.
Diagnostic log prefixes use their existing untimed form when the RTC cannot be read. Successful
UTC/local timestamp formatting is unchanged. The formatter reuses one 21-byte text buffer;
ArduinoJson copies each mutable string into the document before that buffer is reused.

Optional flash health checkpoints are separate from normal CSV/Mongo/LoRa schemas. Their format,
wear policy and SDK dependency are described in [the style guide](STYLE_GUIDE.md#preserve-feature-contracts-while-simplifying).

## Continued boundary review, 2026-09-30

Manager resolves its contents-array view from the current document when fields are added, so
receiving/replacing a packet cannot leave additions attached to an older JSON view. No key or
ordering migration is intended. A proxied LoRa packet that contains an identity must provide both
a non-empty name of at most 63 bytes without embedded NULs and a non-negative signed instance.
Both fields are checked before either Manager identity field changes. Identity-free custom
radio objects retain their existing acceptance; this does not add identity to those messages.

LoRa batch replay requires each bounded JSON line to end with a delimiter, rejects non-whitespace
tails/crossed row boundaries, stops at the first failed send and preserves the queue after any
read/close/count failure. Valid record bytes and default wire framing stay unchanged. Previously
sent records may be retried after a later failure, as in the existing whole-batch retry policy.

Checked RTC reads now also reject oscillator-stop status. Compile/manual/network time writes are
read back and compared with the requested UTC, allowing elapsed whole seconds plus one tick.
Scheduling, sleep-deadline checks and timestamp logging use checked UTC.
A failed/partial explicit time write also blocks checked UTC for
this session until a new explicit set is verified; plausible mixed date bytes cannot silently
become ground truth. This RAM flag is not persistent across MCU reset. The legacy DateTime-only
getter remains for compatibility; callers needing failure information must use tryGetCurrentTime.
One additional status-register read per checked query trades I2C work for detecting a clock stop
between wakes; no speed improvement is claimed for this path.

Retained MQTT reads close the local connection after oversized/truncated/invalid-count payloads
instead of draining unlimited input. They do not clear the broker's retained message. A subsequent
connection attempt uses the existing reconnect path. LTE raw-AT setup requires a complete OK line;
letters inside an echoed command or unrelated message cannot acknowledge it.

## All-card audit opt-ins, 2026-09-30

`Manager::setHealthObserver()` adds a borrowed callback/context with no heap allocation or
diagnostic driver dependency. It is null by default. Pre-initialize and completed-operation
events let the sketch print the previous checkpoint and attempt paced saves; callbacks must
not re-enter Manager or change watchdog policy. No durable per-operation crash record is promised.

`SDManager::setCsvIdentityColumns(false)` hides repeated name/instance CSV columns explicitly.
Default headers/rows and JSON/upload identity remain unchanged. Exact header comparison rotates
CSV when this setting changes; pending upload batches stay independent.

Optional `loomGnss::addCsvLocation()` adds four stable fields to the caller's Location module
after every package, starting with the first row. Missing/expired fixes become null while keys
stay present, so CSV schema does not rotate whenever reception changes. No GNSS query, clock
adjustment or default schema change occurs. The example's GNSS and CSV switches remain off.
