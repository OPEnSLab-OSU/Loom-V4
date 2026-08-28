# Loom 4.9.1 compatibility contract

This hardening release keeps the established 4.9 storage and transport contracts. Reliability,
bounds checking, ownership, RTC, and RAM behavior may change; valid existing data does not need a
format migration.

## Preserved contracts

- Manager packets retain the top-level `type`, `id`, `contents`, and optional `timestamp` fields.
  The packet counter remains the `Number` field inside the `Packet` module. Module entries remain
  `{"module": ..., "data": ...}` objects.
- Existing sensor/module JSON field names are unchanged. Fixes may correct a value that was
  previously stale, uninitialized, or associated with the wrong sensor.
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
- AS726x and SAMD21 Wire waits now return bounded failures instead of blocking forever. Successful
  transaction bytes and sensor field names are unchanged.
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
