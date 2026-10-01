# Loom style guide

Loom should be easy to follow for someone building their first Arduino sensor project. Keep
the operations visible, explain the hardware decisions, and make memory use predictable. This
guide applies to Loom C/C++, examples, and deployment sketches, especially the Feather M0.

Use this guide alongside the [issue tally](ISSUE_TALLY.md), [changelog](../CHANGELOG.md), and
existing [compatibility contract](COMPATIBILITY_CONTRACT.md). Keep future progress in those
records instead of creating another collection of overlapping reports.

## Let the reader follow the device

A module is one part of the device: a sensor, storage, display, or network connection. Manager
calls registered modules in order. Show the usual cycle plainly:

1. Initialize the modules once.
2. Measure: sensors keep their latest readings.
3. Package: start a fresh JSON packet and add those readings.
4. Save or send: the sketch chooses the storage or network operation.
5. Prepare for sleep, then restore the modules after waking.

Manager does not automatically upload or put the MCU to sleep. Preserve that separation so
readers can see when the device measures, stores, retries, and consumes power.

- Name functions after their action: `readBatteryVoltage`, `restoreDataSession`, `writeHeader`.
- Name values after their meaning; include units where confusion is possible: `timeoutMs`,
  `sampleIntervalSeconds`, `batteryMillivolts`. Explain necessary protocol abbreviations once.
- Prefer a named structure over tuple positions or several unrelated Boolean flags. Give new
  mode choices an enum when that makes their combinations easier to understand.
- Keep one purpose per function. Extract repeated policy, validation, or cleanup into a small
  named helper; leave a short sequence of meaningful operations at the call site.
- Use early returns for invalid input and failed prerequisites. Keep the successful path easy
  to scan, and retain cleanup on every exit.
- Follow nearby naming conventions. Existing public spellings such as `power_down()` and
  `get_data_object()` remain compatible; cosmetic consistency alone does not justify API churn.
- Avoid clever expressions, hidden side effects, or a new abstraction that requires more
  explanation than the original operation. A shorter file is not the goal by itself.

## Comments and visual cues are part of the interface

Keep the paired `////` dividers around existing function/section boundaries. Use plain section
labels such as "Read the sensors" or "Save before uploading." These cues help beginners find
their place. Preserve useful comments and future-work notes; correct them when behavior changes.

A function comment should explain what it does, important inputs/units, its result, and what
happens on failure. Include ownership, lifetime, blocking time, and hardware restrictions when
they matter. Inside a function, explain each meaningful phase and any non-obvious decision.
Comments should teach the reason for a delay, check, or ordering rule rather than merely repeat
an assignment. Keep explanations thorough enough to follow without reading a driver datasheet.

For example, label a bounded polling loop and explain why it exists:

```cpp
////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait for a conversion started by the caller. The explicit deadline also handles a sensor
// that keeps answering over I2C but never becomes ready. Report failure without using old data.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool waitForConversion() {
    FUNCTION_START;
    constexpr uint32_t timeoutMs = 25;
    const uint32_t startedMs = millis();

    while (static_cast<uint32_t>(millis() - startedMs) < timeoutMs) {
        if (conversionIsReady()) {
            return true;
        }
        delay(1);
    }

    FUNCTION_END;
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
```

This is a shape example; `conversionIsReady()` must itself check transfer results and have
bounded behavior. The 25 ms value belongs to the ADS1115 policy, not every sensor. Explain an
actual device delay using its documented minimum/maximum or a measured margin, with a source
reference beside the relevant driver code. A TODO names the missing behavior and issue when known.

## Formatting and ordinary C/C++ rules

The repository's [`.clang-format`](../.clang-format) is authoritative: four spaces, no tabs,
a 100-column limit, and existing include order. Format changed code without erasing dividers
or rearranging unrelated files.

- Use braces for control-flow bodies and one statement per line.
- Initialize every variable. Use `const` for unchanged values and `constexpr` for compile-time
  constants. Prefer named constants over unexplained pin numbers, limits, or timing values.
- Use fixed-width integers at hardware, protocol, storage, and serialized-data boundaries.
  Validate ranges before narrowing; check arithmetic and signed/unsigned conversions.
- Use unsigned elapsed-time subtraction for short `millis()` deadlines so rollover is handled.
  Absolute RTC deadlines use checked UTC and the supported calendar range.
- Give every `switch` a deliberate default; annotate intentional fall-through.
- Avoid recursion, variable-length arrays, and `goto`. A narrowly scoped cleanup exception
  must be explained and reviewed.
- Check parser, filesystem, transport, and hardware results. Reject a malformed input before
  modifying valid state. Never present a partial packet or stale sensor value as a fresh success.
- Limit macros to guards, platform adaptation, and existing compile-time instrumentation.
  Prefer typed functions and constants for ordinary behavior.

## Keep RAM, stack, and ownership visible

The Feather M0 has 32,768 bytes of shared SRAM. Static storage, heap, stack, driver buffers, and
interrupt work all compete for it. A stack-to-heap gap reading is an estimate, not a guarantee
that an allocation will succeed or a measurement of peak stack use.

- Normal recurring Loom paths should reuse storage after setup. Do not introduce recurring
  `String`, `std::string`, `new`/`delete`, growing containers, or hidden allocating return values
  in measurement, packaging, logging, sleep/wake, and retry paths.
- Existing third-party allocation exceptions need a bounded policy and measurements across
  first use and reconnect. Reserve known setup capacity; do not claim all SDK allocation is gone.
- Prefer fixed-capacity or caller-owned buffers, streaming, and reused JSON storage. Every copy
  and formatted write carries capacity and checks truncation/overflow. Budget the terminating NUL.
- Pass large objects by reference. Large local arrays need a stack justification; aim for normal
  Loom-owned frames below 512 bytes where practical. Moving an array to static storage still uses
  SRAM and changes reentrancy, so it needs an ownership decision.
- Manager borrows registered modules: they must outlive it. The mux owns discovered sensor
  objects. State explicitly who deletes any other owned object; preserve existing ownership APIs.
- Manager JSON objects are views into its shared packet. Reacquire them after `package()`, clear,
  or replacement. Borrowed strings and callback contexts must outlive their uses.
- An optional pool needs a clear owner, fixed capacity, and visible release path. Do not migrate
  unrelated subsystems into a pool merely to make the source look uniform.
- Explain savings using linked `.data`/`.bss`, stack reports, and runtime observations. Source
  size, fewer includes, and fewer lines do not establish speed or memory improvements.

## Bound work and preserve fault containment

Every polling/retry loop needs a count, deadline, progress limit, or an explicitly documented
watchdog containment policy for unavoidable blocking SDK calls. Continuously available input
must not bypass the bound. Distinguish an AT acknowledgment from a usable network or GNSS fix.

Feed the watchdog only after verified progress. Preserve its previous enabled state, period,
and window when temporarily pausing it for a known operation or standby. Restore that state
on success and every failure/early return. Quiet builds keep the watchdog and recovery behavior.

Interrupt handlers only record minimal `volatile` state and return. No allocation, logger,
SD access, modem commands, I2C/SPI transaction, or blocking work belongs in an ISR. Consume the
flag in the normal loop, and protect shared multi-step updates appropriately.

State whether an operation powers down a rail, idles a supported sensor, or sleeps the MCU.
`manager.idle()` keeps shared rails on and pauses measurement; unsupported drivers remain
powered. It is not a promise that every sensor has an efficient standby mode. Module shutdown
must succeed before removing a rail that the module says must remain powered.

## Includes, utilities, and debug headers

Headers include what their public types require. Forward-declare pointer/reference-only
dependencies when valid; put implementation-only drivers in `.cpp` files. Sketches explicitly
include the classes they construct. Remove genuinely unused includes.

A small bounded utility may replace an isolated library helper or simple data structure if
its behavior and boundary cases remain clear. Keep real hardware/protocol libraries when they
provide required behavior; reproducing a driver stack is not a readability improvement.

Keep [`Loom_WarningGuards.h`](../src/Loom_WarningGuards.h) present. Wrap only external Arduino
or vendor include groups with `LOOM_EXTERNAL_INCLUDE_BEGIN` / `LOOM_EXTERNAL_INCLUDE_END`.
Leave Loom headers, standard/runtime headers, sketches, and function bodies outside those
scopes so Loom warnings remain visible. See the existing [warning-scope usage](../tests/WARNING_SCOPE.md).

Retain `FUNCTION_START` / `FUNCTION_END` as the familiar debug spelling. Start creates a scoped
instrumentor whose destructor records exit on every return. End is the explicit normal-exit
source marker and does not log again. Do not replace them with a less descriptive call name.

Keep the canonical logger/memory debug headers in the `_debug` sketches. Each Arduino folder
matches its main `.ino` name. A quiet sketch and its `_debug` sibling retain the same operational
configuration and recovery behavior. Temporary diagnostics use the existing boundary markers:

```cpp
// BEGIN LOOM_BETA_DIAGNOSTICS
// Diagnostic switch, includes, and adapters belong here.
// END LOOM_BETA_DIAGNOSTICS
WISP_DIAGNOSTIC_CHECKPOINT("after package"); // LOOM_BETA_DIAGNOSTIC
```

`LOOM_WISP_BETA_DIAGNOSTICS=0` compiles out the temporary instrumentation. Quiet siblings omit
those blocks and routine SD/JSON debug output. Temporary telemetry stays on Serial and never
silently changes the measurement schema.

## Preserve feature contracts while simplifying

- Preserve default JSON keys, CSV ordering, filenames, topics, and radio framing. New output
  options are explicit opt-ins; a breaking format needs versioning, migration, and golden outputs.
- Keep measurement UTC as ground truth. Local timezone/DST is a separate display/configuration
  concern. Invalid clocks yield unavailable data or refuse an alarm; receipt/insert time must
  never replace missing measurement time.
- SD appends check write, sync, rollback, and close outcomes. Preserve uncertain files and pending
  upload batches on failure. A broker ACK proves broker acceptance, not final MongoDB insertion.
- GNSS remains optional for supported R5 hardware; the SARA-R510M8S/antenna is not assumed present.
  One owner reads the modem UART. Validate checksum, coordinates, UTC, and freshness. CSV location
  keys are added after every package, starting at the first row; unavailable fixes stay null.
  Location UTC does not adjust the measurement clock. The optional example defaults GNSS and SD off.
- CSV identity suppression changes CSV only; sensor column-selection options also affect packaged
  samples. Explain that distinction. Header changes rotate CSV without clearing pending batches.
- Recharge is optional. The current example's 3.7 V enter / 4.2 V resume hysteresis is a user-selected
  battery policy, not a measured MCU supply or substitute for battery/BMS limits. Internal-timer
  sleep must have one RTC alarm owner; brownout handlers do minimal work before loop-side recovery.
- Manager health observers borrow callback/context and report call completion, not driver success.
  Callbacks must be bounded: no Manager reentry, watchdog-policy changes, or modem queries.
  Optional flash writes and automatic checkpoints default off. The example uses two independent
  erase slots, an infrequent policy (one day by default), and a verified supply decision. Saved
  health is a past checkpoint, not evidence of every operation before a crash. Programming time,
  interruption behavior, endurance, and firmware-upload erasure need hardware acceptance.
- Offline bridge/dashboard mocks are integration examples. Keep their fictional data explicit;
  distinguish device measurement, broker receipt, and database insertion, and preserve failed work.

## Review and automation

Keep checks proportional to the change. Source-only preflight checks warning headers, includes,
quiet/debug parity, diagnostic boundaries, and dependency/core integrity. Interpreted mock tests
check local server/viewer contracts. Their commands remain in the existing [test README](../tests/README.md).

The prepared `core-boundaries.yml` workflow adds host/source/mock checks beside the existing
formatting workflow. Neither workflow has been newly dispatched by this cleanup. Compiler work
is currently paused by the user; preparing source or checks does not authorize executing them.
When compilation resumes, use the exact target
`loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off`, separate Loom/external warnings,
review RAM/stack, and validate both quiet and debug paths. Hardware timing, power cuts, real
transport delivery, and soak behavior require their own acceptance evidence.

Record an exception's reason, affected path, bounded cost, failure handling, and review condition.
Update the issue tally when evidence changes; distinguish source candidates, mocks, historical
passes, current builds, and physical acceptance. Do not call an open card finished solely because
its local implementation exists.

## Reference basis

These are Loom project rules informed by embedded practice and selected NASA/MIT guidance,
not a claim of NASA, MIT, MISRA, or safety certification. NASA's standard defines software
assurance and safety requirements. MIT's communication guidance supports consistent code that
communicates its logic through names, structure, context, and meaningful comments.

- [NASA-STD-8739.8B: Software Assurance and Software Safety Standard](https://standards.nasa.gov/standard/nasa/nasa-std-87398)
- [MIT Mechanical Engineering Communication Lab: Coding and Comment Style](https://mitcommlab.mit.edu/meche/commkit/coding-and-comment-style/)
- [MIT course C handouts and style examples](https://stuff.mit.edu/afs/sipb/project/iap/Archive/2001/ccc/handouts.pdf)
