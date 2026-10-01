# Loom issue tally

Consolidated UTC: 2026-10-01T00:54:43+00:00
Latest card audit UTC: 2026-10-01T00:06:41.427362+00:00
Saved board membership UTC: 2026-09-30T17:11:39.644777+00:00
Issue/discussion refresh UTC: 2026-09-30 23:47:44 UTC
Branch: `4.9-joshfixes`

Task board: [OPEnSLab Loom project 30](https://github.com/orgs/OPEnSLab-OSU/projects/30).

This tally covers the saved 55-card membership. Issue discussions were refreshed for the
latest audit; board membership was not refreshed then. No GitHub cards were changed or closed.
Follow the board link before a future issue-by-issue pass to catch additions and status changes.

## Counts and acceptance limits

| Local disposition | Cards |
| --- | ---: |
| Compilation paused | 1 |
| External input needed | 3 |
| Larger design remaining | 3 |
| Existing behavior retained | 3 |
| Offline mock | 5 |
| Review prepared | 5 |
| Source candidate | 33 |
| Workflow prepared | 2 |
| **Total** | **55** |

**48 cards have local source, mocks, retained behavior, review, or workflow preparation.**
Seven still need larger design (3), external inputs (3), or compilation (1). Preparation is
not release acceptance. Hardware and live-service checks remain wherever listed below;
source candidates and mocks do not establish that a reported field problem is cured.

Compilers are paused at the user's request. Earlier builds passed at earlier revisions;
current firmware and changed C++ cases remain uncompiled. The host inventory has 27 programs:
17 previously passed and ten new programs await compilation; changes since those passes also
need retesting. No hardware uploads, release publication, or new CI dispatch occurred.

Latest pre-cleanup checks passed for source preflight, warning scopes, four quiet/debug pairs,
dependency/core integrity, changed-source formatting, 11 interpreted receipt-mock cases,
the interpreted viewer check, and the release-package fixture. These checks do not validate
C++ compilation, real modem/SD behavior, or live database delivery. Generated reports and
snapshots were intentionally removed; code, test runners, and existing build products remain.

## Card-by-card tally

| Card | Local status | Addressed or prepared | Remaining work |
| --- | --- | --- | --- |
| [#328 — Sleep interval not matching up with actual measuring intervals](https://github.com/OPEnSLab-OSU/Loom-V4/issues/328) | Source candidate | UTC sample-grid deadlines skip missed slots; checked alarms avoid adding active time to every interval. | Full active/sleep bench timing. |
| [#346 — SEN55 degradation](https://github.com/OPEnSLab-OSU/Loom-V4/issues/346) | Source candidate | Driver warm-up, missing-value handling and retained-rail lifecycle reviewed against installed driver inputs. | Physical zero-reading/degradation soak; no claim that indoor hardware degradation is cured. |
| [#314 — Optimizing Data.csv files](https://github.com/OPEnSLab-OSU/Loom-V4/issues/314) | Source candidate | Selectable battery representations, ADS channels/voltages, and new opt-out CSV identity columns. ISO/space-normalized dates retained following the comment; defaults preserved for compatibility. | Compile and SD byte/rotation acceptance; changing default fields requires migration. |
| [#303 — Speed up Loom's compile times. (future enhancement)](https://github.com/OPEnSLab-OSU/Loom-V4/issues/303) | Larger design remaining | Direct include reduction and conservative seven-profile inventories prepared. | Comparable compiler timing and split/slim board package are explicitly outside the current rewrite scope. |
| [#267 — Memory Pools](https://github.com/OPEnSLab-OSU/Loom-V4/issues/267) | Larger design remaining | Optional fixed receive slots and deterministic bounded allocator; selected callback tables avoid allocations. | Whole-framework/public-API/SDK pool migration and full runtime RAM budget are larger work. |
| [#272 — Sensor Dependency Review With Chet](https://github.com/OPEnSLab-OSU/Loom-V4/issues/272) | Review prepared | Dependency profiling tool retained; seven conservative profiles reviewed without stripping required drivers. | Actual Chet/team review and slim board packaging. |
| [#252 — SARAR4 To R5 Conversion](https://github.com/OPEnSLab-OSU/Loom-V4/issues/252) | Source candidate | Explicit R4/R5 families, readiness/recovery and acknowledged shutdown retained. | Exact modem/carrier/power acceptance and published board dependency version. |
| [#354 — Data dashboard explorer - Features](https://github.com/OPEnSLab-OSU/Loom-V4/issues/354) | Offline mock | Wind speed/direction/PM rose, device metadata, import/export and duplicate exclusion in the authorized offline mock. | Production dashboard repository/data/service integration. |
| [#353 — Handshake Look Over (Dendrometer)](https://github.com/OPEnSLab-OSU/Loom-V4/issues/353) | Source candidate | Bounded sender/fragment/batch lifecycle and timeout recovery address dead nodes and fresh packets after abandoned fragments. | Multi-node RF/queue soak; helper tests do not emulate full RadioHead handshake. |
| [#349 — Dendrometer Sketch 4.8.1 compatibility](https://github.com/OPEnSLab-OSU/Loom-V4/issues/349) | Source candidate | SSI parity/state/filtering fixes on the AS5311 example. | Confirm card's AS5111 spelling versus installed part, wiring and physical SSI timing. |
| [#351 — Teros 54 and Teros 21 Loom Integration](https://github.com/OPEnSLab-OSU/Loom-V4/issues/351) | Source candidate | TEROS 21/54 identification, signed replies, advertised wait/count and D0/D1; scan accepts only matching single-address replies. | Excitation/wiring/multi-probe bus bench; recent comments suspect wiring for all-address responses. |
| [#352 — Adafruit Anemometer Loom Support](https://github.com/OPEnSLab-OSU/Loom-V4/issues/352) | Source candidate | Fresh comments specify SparkFun reed kit: gated two-second pulse counter and calibration are present. | Contacts, wiring, calibration and interrupt sharing bench. |
| [#287 — Logger Debug Truncate Bug](https://github.com/OPEnSLab-OSU/Loom-V4/issues/287) | Source candidate | Filename truncation keeps the character immediately after either slash, preserving Loom's L. | No dedicated Logger native fixture; normal/Windows-path serial acceptance remains. |
| [#355 — Git workflows meetings](https://github.com/OPEnSLab-OSU/Loom-V4/issues/355) | External input needed | Workflow demonstration and recording handoff reviewed; actual recording remains external. | Actual Tess meeting recording and Teams storage/index access unavailable. |
| [#268 — SD::Log Overflow Bounds Checks and Removing Arduino Strings](https://github.com/OPEnSLab-OSU/Loom-V4/issues/268) | Source candidate | Bounded escaping/streamed writes, exact schema byte comparison, rollback/sync/close and result reporting; no temporary value Strings. | SD/power-cut bench. Exact header comparison deliberately replaces proposed hashes; whole producer/consumer pipeline is larger work. |
| [#345 — MongoDB/Batch timing mismatch](https://github.com/OPEnSLab-OSU/Loom-V4/issues/345) | Offline mock | Client replay/failure fixes plus separate measured/received/inserted clocks in receipt mock. | Actual broker/database receipts needed to prove delayed arrivals, not local publish logs. |
| [#288 — LoRa + LTE Mongo Upload Issues](https://github.com/OPEnSLab-OSU/Loom-V4/issues/288) | Source candidate | Transport preparation, publish begin/write/end checks and finite recovery keep unsent queues. | Multi-day real carrier/broker/node delivery and QoS timing. |
| [#220 — Add LTE Board Location Data to Device Metadata Packets](https://github.com/OPEnSLab-OSU/Loom-V4/issues/220) | Source candidate | Optional fresh GNSS metadata plus stable Location fields from the first CSV sample; missing/stale fixes stay null. Explicit metadata publish overload exists. | Optional SARA-R510M8S/antenna testing, actual schema and server enrichment acceptance. No coordinates invented for ordinary R4. |
| [#342 — Recharge mode](https://github.com/OPEnSLab-OSU/Loom-V4/issues/342) | Source candidate | Opt-in 3.7/4.2 V hysteresis, hourly external/internal timers, graceful shutdown and calibrated BOD warning. | ADC/BMS/supply margin/current/solar bench; battery and MCU supply thresholds are distinct. |
| [#291 — Various example compile errors.](https://github.com/OPEnSLab-OSU/Loom-V4/issues/291) | Compilation paused | Earlier example builds passed at their recorded revisions; fixes and build outputs retained. Generated narrative build reports removed. | Current example/firmware candidates require compilation when user resumes it. Historical passes cannot accept changed files. |
| [#273 — Loom Manager Class for Idle Standby Modes](https://github.com/OPEnSLab-OSU/Loom-V4/issues/273) | Larger design remaining | Manager idle/resume hooks and mux forwarding; SEN55/SEN66 have retained-rail standby support. | Dendrometer/Evaporometer standby contract inputs and broader 5.0 ownership; unsupported drivers remain no-op. |
| [#343 — Non volatile device health logging](https://github.com/OPEnSLab-OSU/Loom-V4/issues/343) | Source candidate | Two-slot flash health journal independent of SD; Manager before-initialize/completion observer; manual/automatic attempts default off and remain paced. | Physical flash/supply/endurance/reset tests. Durable per-operation crash breadcrumbs require a suitable backend/endurance design; this is only last saved checkpoint. |
| [#348 — 4.8.1 release](https://github.com/OPEnSLab-OSU/Loom-V4/issues/348) | Review prepared | Control/isolated/combined three-week acceptance plan retained below; package audit tooling prepared. | Physical three-week acceptance and actual 4.8 release approval/publishing. 4.9 candidates do not certify 4.8. |
| [#350 — SD End of day/checksum integration](https://github.com/OPEnSLab-OSU/Loom-V4/issues/350) | Source candidate | Opt-in additive-16 streamed byte checksum, next-active-log UTC-day verification and damaged CSV preservation/rotation. | SD/time/power-cut acceptance; per-row close deliberately retained rather than copying persistent-open EOD handles. |
| [#207 — SD Debug File Corruption](https://github.com/OPEnSLab-OSU/Loom-V4/issues/207) | Source candidate | Independent debug handles, rollback/sync/close and stop-after-uncertain debug append; linked checksum card handled. | Physical multi-day FAT/debug corruption and power cuts; latest old-branch discussion is not current proof. |
| [#271 — LoRa Hub Groups & Scheduling](https://github.com/OPEnSLab-OSU/Loom-V4/issues/271) | Source candidate | Nibble group filter, UTC slots, SD-configurable window/device count, early measurement and off-slot sleep example. | RTC/RF acceptance and measured complete-airtime guard; start gate is not hard airtime reservation, 0xFF remains broadcast. |
| [#340 — LTE Low Power mode](https://github.com/OPEnSLab-OSU/Loom-V4/issues/340) | Source candidate | Uses init/readiness probes and disconnect/poweroff acknowledgment before safe rail removal; unknown state is retained. | Measured reconnect/current and repeated hardware cycles; no whole old enum state-machine import. |
| [#347 — Hardware stacks making multiple CSV files](https://github.com/OPEnSLab-OSU/Loom-V4/issues/347) | Source candidate | Watchdog pause/config preservation, guarded wake, reset cause/intent and CSV boot-session retention. | Hardware-specific sleep/reset/flash/RTC investigation remains per latest comments; no universal cured claim. |
| [#318 — Daylight Savings Rework](https://github.com/OPEnSLab-OSU/Loom-V4/issues/318) | Source candidate | Annual North American DST boundaries, leap years, standard offsets and SD-configurable local display separate from UTC. | Actual RTC/modem timezone conventions; future statutory timezone rules outside current North American policy. |
| [#299 — MongoDB Multi-project Support](https://github.com/OPEnSLab-OSU/Loom-V4/issues/299) | Offline mock | Client topics/identity retained; offline multi-route model. Fresh upstream comment reports cached multi-project server routing working. | Production bridge evidence/access and extended tests; no unnecessary replacement of reported server fix. |
| [#341 — MQTT Broker Log files](https://github.com/OPEnSLab-OSU/Loom-V4/issues/341) | Offline mock | SQLite arrivals/client/IP metadata, insertion outcomes/clocks, exact-byte duplicate handling, bounded retention/backpressure and export. | Live broker adapter/auth/ACL/server disk budget/deployment. |
| [#290 — AS7263 Example Typo](https://github.com/OPEnSLab-OSU/Loom-V4/issues/290) | Existing behavior retained | AS7263 example instantiation typo fixed; preserved. | Current affected example compile still held. |
| [#261 — Dendrometer Hub LoRa Batch Packet Receive Failure (Handshake / Scheduling)](https://github.com/OPEnSLab-OSU/Loom-V4/issues/261) | Source candidate | Connected handshake/fragment lifecycle and source identity/timeout protections retained. | Real multi-node handshake and RX timing; legacy merge/Done does not prove current candidates. |
| [#257 — 4.9 Board Profile](https://github.com/OPEnSLab-OSU/Loom-V4/issues/257) | External input needed | Existing 4.9 profile/support retained; current patched RTC dependency is deliberately required. | Published board release/team acceptance is outside this library; no automatic OPEnS_RTC removal or package index change. |
| [#322 — LTE Board Power Issue](https://github.com/OPEnSLab-OSU/Loom-V4/issues/322) | Source candidate | AT readiness before power pulse; uncertain/live externally powered modem can recover after MCU reset. | Externally powered reset/wake bench. |
| [#302 — 4.9 - Smart Rock - Wake up freeze](https://github.com/OPEnSLab-OSU/Loom-V4/issues/302) | Source candidate | Checked RTC/alarm wake path, bounded sensor startup and watchdog ownership retained. | Assembled SmartRock stack bench. |
| [#338 — 4.8 4.9 Git Diff Results](https://github.com/OPEnSLab-OSU/Loom-V4/issues/338) | Review prepared | All 15 saved 4.8-restore heads compared with 4.9; significant differences summarized in the changelog and selectively applied. | No whole legacy-branch import; new changes remain uncompiled. |
| [#300 — Multiplexer power_down() function causing hypnos not to wake from sleep](https://github.com/OPEnSLab-OSU/Loom-V4/issues/300) | Source candidate | Mux disables channels after operations; forwarded shutdown/rail permissions prevent unsafe power removal. | Physical mux/rail/RTC wake integration. |
| [#301 — Network Time Update 15 Hours Behind](https://github.com/OPEnSLab-OSU/Loom-V4/issues/301) | Source candidate | Network UTC and local display separated; checked time writes/readback. | R4/R5 actual +CCLK/UTC/timezone replies. |
| [#213 — LoRa Heartbeat](https://github.com/OPEnSLab-OSU/Loom-V4/issues/213) | Source candidate | Explicit constructor mode, separate status document/numbering, UTC deadlines, battery/custom fields and LED demo. | Radio/wake/server acceptance; one owned checked alarm schedules earliest deadline rather than transplanting legacy dual-alarm ownership. |
| [#277 — Include Type Field in MongoDB](https://github.com/OPEnSLab-OSU/Loom-V4/issues/277) | Offline mock | Offline data/metadata/heartbeat receipts preserve type; upstream reports bridge.py type inclusion. | Actual bridge/source acceptance; no redundant client field introduced. |
| [#295 — Packet Lifecycle Documentation](https://github.com/OPEnSLab-OSU/Loom-V4/issues/295) | Review prepared | Packet lifecycle, ownership, save/send failure and delivery boundaries consolidated into the style guide and source comments. | Remote PDF/wiki publishing not performed. |
| [#275 — VCNL4020 Integration](https://github.com/OPEnSLab-OSU/Loom-V4/issues/275) | Existing behavior retained | VCNL4020 source and example retained. | Physical sensor/new board release acceptance. |
| [#254 — LTE MQTT Stability](https://github.com/OPEnSLab-OSU/Loom-V4/issues/254) | Source candidate | Checked stream frame/write/finish, finite reconnect/transport preparation and queued batch retention. | Actual carrier/TCP/broker/QoS soak. Bad records retained/reported rather than silently skipped. |
| [#209 — Manual Multiplexer Address Overrides](https://github.com/OPEnSLab-OSU/Loom-V4/issues/209) | Existing behavior retained | Manual/SD address mapping and existing sensor-name disambiguation paths retained. | Physical custom mappings/overlap acceptance. |
| [#199 — Local timestamp on receive end of Lora showing completely wrong time](https://github.com/OPEnSLab-OSU/Loom-V4/issues/199) | Source candidate | Checked local diagnostic time and validated complete received documents; UTC/local separation retained. | Radio/RTC integration; upstream says historical symptom has not recurred. |
| [#208 — 4G board sticking on](https://github.com/OPEnSLab-OSU/Loom-V4/issues/208) | Source candidate | Checked shutdown, cached uncertain state and voltage validation before transmission. | Wisp weak-coverage/current/power-reset soak. |
| [#210 — 4G board on Wisp units staying on for long periods of time](https://github.com/OPEnSLab-OSU/Loom-V4/issues/210) | Source candidate | Connected LTE power/readiness/recharge protections retained. | Long-lived carrier sessions/weak RF/solar bench. |
| [#239 — DFRobot Multigas String Usage](https://github.com/OPEnSLab-OSU/Loom-V4/issues/239) | Source candidate | Small allocation-free query/label adapter replaces exercised Arduino String path. | Physical gas/temperature output; required vendor protocol driver retained. |
| [#269 — LoRa Single Packet Transmission Naming Issues](https://github.com/OPEnSLab-OSU/Loom-V4/issues/269) | Source candidate | Proxy identity validated atomically before copy; single/fragment framing retained. | Actual hub-to-server routing. Upstream reports proxy fix working; no new naming scheme. |
| [#249 — Wiki Enhancements](https://github.com/OPEnSLab-OSU/Loom-V4/issues/249) | External input needed | Beginner/lifecycle guidance consolidated into the style guide; source explanations and existing bug template retained. | Actual training recordings/text transcription inputs and remote wiki publication unavailable; not claimed fully complete. |
| [#251 — GitHub Workflow](https://github.com/OPEnSLab-OSU/Loom-V4/issues/251) | Workflow prepared | Existing formatting plus new read-only push/PR/manual source/host/mock CI definition; warning-split audits retained. | Workflow not pushed/dispatched/accepted. SDK-aware clang-tidy requires compiler metadata; compiler pause respected. |
| [#231 — Logical Unit Testing Framework](https://github.com/OPEnSLab-OSU/Loom-V4/issues/231) | Workflow prepared | 27 runnable source programs/fakes, selected local runner and new automatic push/PR workflow definition. | Current programs uncompiled; real initialize register reads and physical behavior not faked. |
| [#259 — MongoDB Heartbeat](https://github.com/OPEnSLab-OSU/Loom-V4/issues/259) | Source candidate | Explicit separate heartbeat publisher and transport-selected example plus authorized service mock. | Firmware compile and actual production bridge/database receipts. |
| [#304 — Remove the .git folder from TinyGSM (do this next release)](https://github.com/OPEnSLab-OSU/Loom-V4/issues/304) | Review prepared | Separate hash-verified source ZIP excludes Git metadata/credentials/products while retaining installed developer history; fresh upstream comment confirms beta fix. | Actual future board package artifact validation/publishing. |

## Retained release and workflow notes

- **#348:** Use `release/4.8-restore` as the paired control, individual
  `release/4.8-restore-feature` candidates, then `release/4.8-restore-allFeats` for combined
  acceptance. Run comparable three-week isolated/combined deployments; 4.9 results do not
  certify a 4.8 release. Save revisions, dependency/core versions, configuration, hardware,
  battery/solar setup, tester, resets, current, UTC cadence, storage and delivery evidence.
  Wisp emphasizes sensor settling/recharge/LTE; SmartRock wake/cadence/rails; Dendrometer
  SSI/radio/batch routing; WeatherChimes sensor waits/wind/network time.
- **#355:** A workflow demonstration should show a focused diff, source checks, then allowed
  host/board checks, warning separation, memory evidence, and review/issue updates. Completion
  still requires the actual meeting recording, dated filename, Teams URL, guide-index link,
  topic offsets, and access/playback verification.
- **#343:** Health observer and optional flash checkpoint support are present. Infrequent
  two-slot checkpoints do not provide durable per-operation crash breadcrumbs; suitable
  backend endurance and interruption behavior remain a separate design/acceptance task.
- **#251/#231:** The read-only source/host/mock CI definition complements the formatting
  workflow. Keep it alongside local preflight and selected host tests. Future extensions
  may check golden outputs, linked RAM/stack budgets and dependency drift; compiler-aware
  analysis awaits compiler metadata. No recurring task or publishing action was scheduled.
- **#304:** `tools/make_release_package.py` makes a separate hash-verified source ZIP,
  excludes Git metadata/credentials/products, and leaves developer checkouts intact.
  It does not build or publish the complete board-manager package.

## Scoped optional trace verification, September 30, 2026

The current trace request authorized selected compilation and native recorder checks. This
does not resume the wider card-by-card compiler audit or change any card's disposition.
Wisp trace-off, call-only, and heap-capture modes compiled for the Feather M0. The latter two
used the deployment sketch's A0/mux settings. Trace-off links no recorder/wrapper symbols;
heap capture now has a 1,872-byte recorder, and disassembly confirms `new/delete` call the
wrapped allocator. These builds reported no Loom-owned warnings; core/dependency warnings
remain in their full logs. The existing manager fixture still has its unused fake reset-cause
warning. Twenty interpreted converter cases, recorder native cases, manager lifecycle,
warning scopes, include discovery, and quiet/debug parity checks passed. The final deployment and mux heap builds include closed SD Chrome JSON, copied baseline/created mux object names and readiness, call-boundary heap totals, and SD power-off buffering. Native fault injection verifies JSON trailer rollback and stopped capture after a companion-file failure; the production JSON and NDJSON serializers reconstruct identical desktop histories. The browser was
verified with explicitly fictional data. No firmware was uploaded; physical SD latency,
power-cut behavior, allocation coverage and soak testing remain unverified. See
[TRACE_DEBUGGING](TRACE_DEBUGGING.md) for scope and use.

Update this file in place after future checks. Keep card URLs, a timestamp, local status,
remaining work, and an honest description of the evidence. Retain broader change history
in the [changelog](../CHANGELOG.md) and coding rules in the [style guide](STYLE_GUIDE.md).
