# Loom SAMD21 C/C++ coding profile

## Status and scope

This is the project coding profile for Loom code and deployment sketches targeting the Adafruit
Feather M0 / ATSAMD21G18A. It is a review and release checklist, not a claim of NASA, JPL, MIT,
MISRA, or safety certification.

NASA-STD-8739.8B is a software-assurance and software-safety standard. NASA guidance calls for a
project to select, define, document, and follow coding standards and to use repeatable verification
such as static analysis and unit tests. It does not supply one Arduino-specific C style. MIT
publishes useful course and communication style guides, but there is no single institution-wide
"MIT C standard" that can be applied as a certification. This profile turns those principles into
concrete rules for Loom's constrained embedded target.

References:

- [NASA-STD-8739.8B](https://standards.nasa.gov/standard/nasa/nasa-std-87398)
- [NASA Software Engineering and Assurance Handbook guidance](https://swehb.nasa.gov/spaces/SITE/pages/119242809/NASA-STD-8739.8B)
- [MIT C style guide example](https://stuff.mit.edu/afs/sipb/project/iap/Archive/2001/ccc/handouts.pdf)
- [MIT Communication Lab coding and comment guidance](https://mitcommlab.mit.edu/meche/commkit/coding-and-comment-style/)

## Non-negotiable target constraints

- Treat SRAM as a 32,768-byte shared resource for static data, heap, stack, library buffers, and
  interrupt context. An IDE "free RAM" value is not a runtime guarantee.
- Build release evidence with the exact Feather M0 FQBN:
  `loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off`.
- Preserve established JSON keys and module labels, CSV headers and column order, filenames, MQTT
  topics, delimiters, and wire framing. A format change requires an explicit schema version,
  migration plan, and golden-output update.
- Use the official Loom board-core Wire and SERCOM implementation. Core experiments may be retained
  under `dependencies` as inactive engineering notes, but they are not release inputs.

## Deterministic resource rules

1. Normal recurring paths shall not allocate dynamically after setup. Exceptions in third-party
   libraries must be identified, bounded, and measured across first use and reconnect.
2. Do not introduce Arduino `String`, `std::string`, unbounded containers, or repeated `new`/`delete`
   in measurement, packaging, logging, sleep/wake, or retry paths.
3. Prefer fixed-capacity storage, caller-owned buffers, direct streaming, and capacity reservation
   before one-time setup growth. Every copy or formatted write must carry the destination size.
4. Do not use variable-length arrays or recursion. Large local arrays require a measured stack
   justification; normal Loom-owned field frames should remain below 512 bytes where practical.
5. Every wait or retry loop must have a documented bound or be covered by an enabled watchdog.
   Feeding the watchdog is allowed only after verified forward progress, never inside a loop that
   can repeat forever without changing state.
6. The watchdog is production fault containment, not debug instrumentation. It is disabled only
   around known operations whose valid duration exceeds the SAMD21 maximum interval and before
   standby when the configured watchdog continues running.
7. Interrupt handlers shall not allocate, log, access SD, perform I2C/SPI transactions, or block.
   They set or clear minimal `volatile` state and return.

## C/C++ construction and control-flow rules

- Use fixed-width integer types at register, protocol, storage, and serialized-data boundaries.
- Initialize every variable before use. Make immutable values `const`; use `constexpr` for compile-
  time constants. Avoid implicit narrowing and signed/unsigned comparisons.
- Check hardware, filesystem, parser, serializer, and transport results. Invalid input fails closed
  without emitting a partial output record.
- Use braces for every `if`, `else`, `for`, `while`, and `do` body. Put one statement on each line.
- Every `switch` has a `default`. Intentional fall-through is explicitly annotated. Do not use
  `goto`; a narrowly scoped cleanup exception requires review and a comment.
- Keep functions single-purpose and make ownership/lifetime visible. Avoid hidden heap allocation
  in convenience return types. Pass large objects by reference.
- Use macros only for include guards, platform/compiler adaptation, and clearly bounded compile-time
  instrumentation. Parenthesize macro arguments where the expansion permits it.
- Comments explain constraints, invariants, units, hardware behavior, or a non-obvious decision.
  Remove narration that merely restates the next line.

## Beta diagnostic boundary

Temporary deployment telemetry in a sketch uses exactly these markers:

```cpp
// BEGIN LOOM_BETA_DIAGNOSTICS
// ... compile switch, include, and adapter definitions ...
// END LOOM_BETA_DIAGNOSTICS

WISP_DIAGNOSTIC_CHECKPOINT("phase"); // LOOM_BETA_DIAGNOSTIC
```

`LOOM_WISP_BETA_DIAGNOSTICS=1` is the soak-test build. Setting it to `0` must compile out the
diagnostic object, strings, and calls while leaving watchdog and recovery behavior in place. The
final canonical cleanup removes every marked block and tagged call, then rebuilds and reruns golden
output tests. Diagnostics remain Serial-only and may not add or rename stored JSON/CSV fields.

## Required review and release evidence

- Exact-FQBN clean build with warnings separated into Loom-owned and external findings.
- Linked `.data` + `.bss` measurement and compiler stack-usage review for changed recurring paths.
- Diagnostic-enabled and diagnostic-disabled WISP builds.
- Automated verification of canonical/mirrored examples, diagnostic boundaries, dependency hashes,
  official core hashes, and output-contract tokens.
- Golden comparisons of representative JSON, CSV, filenames, MQTT topics, and wire payloads.
- Hardware tests for RTC/DST boundaries, repeated standby/wake, SDA/SCL stuck-low faults, sensor-rail
  recovery, SD removal/partial writes, LTE outage/reconnect, brownout, and controlled watchdog reset.
- A three-week soak spanning first-use and repeated network uploads. Preserve complete memory,
  reset-cause, wake, and fault-injection logs as release artifacts.

Exceptions must name the rule, affected target/path, measured cost, failure containment, reviewer,
and removal or reevaluation condition. An undocumented exception is a failed review item.
