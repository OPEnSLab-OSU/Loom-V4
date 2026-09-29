# Loom readability update — 2026-09-29

The cleanup preserves the public module APIs and keeps the existing storage and network
contracts. The main changes are:

- Retain the familiar `FUNCTION_START` / `FUNCTION_END` spelling for debug summaries. Entry
  creates the existing scoped instrumentor; its destructor closes summaries on every return.
  The end marker emits no duplicate summary. `INSTRUMENT()` remains available.
- Retain paired slash dividers around functions and add plain-language section labels at the
  main entry points. Format implementation files consistently and add braces around control
  flow. Review comments for accuracy and retain hardware explanations and future-work notes.
- Traverse registered modules directly instead of repeatedly indexing the pointer table.
- Name mux sensor fields (`address`, `module`, `port`) instead of using tuple positions.
  Centralize mux constructor setup, default address selection, and verified mux discovery.
- Flatten CSV logging and MQTT connection/publishing guards so the successful path is
  easier to follow. Preserve CSV field order, rollback, batch replay, record counting,
  cursor restoration, and file retention on failure.
- Build MongoDB MQTT topics in one helper, using the same optional project prefix and
  device/instance format as before.
- Express rail selection with one state switch per rail and direct configuration checks,
  preserving the enabled-rail fallback for invalid states/configurations. Return directly
  after successful network-time synchronization while retaining the two-attempt limit.
- Add an opt-in logger switch for quiet sketches. The default remains debug-enabled;
  warnings/errors are not filtered. It does not remove the logging feature from Loom.

The existing uncommitted Hypnos interrupt/wake and mux recovery edits were preserved.
Third-party dependency code and active board-core code were not edited.

## Sketch layout

The three library Wisp examples and the active deployment each have quiet/debug pairs.
Debug variants use matching `_debug` folder and `.ino` names. The duplicate nested
`examples/Lab Examples/Wisp/examples` folder was removed. See the Wisp README for exact
paths and diagnostic behavior.

## Validation status

The data-safety helper regression and installed dependency/core hash verification passed
before the user's request to defer compilation. No further compiler checks are authorized
at this point. A baseline board compile was started and then interrupted; no successful
board-build or memory/stack-size result is claimed for this refactor.

Clean/debug source parity and diagnostic-boundary checks pass, including the active
deployment pair. These checks verify matching operational code/configuration and retained
watchdog/retry calls; they do not prove runtime behavior on a Feather M0.
The first formatting pass checked code-token preservation in 54 implementation files.
Further source comparisons for subsequent simplifications are recorded in the
[embedded design review](EMBEDDED_DESIGN_REVIEW.md). The mux sensor loader
retains its address cases and constructor options. Original include order was preserved.

Remaining validation, when authorized: build all six library variants and both active
variants with the exact Feather M0 FQBN, compare linked SRAM/flash and changed stack frames,
run the full existing example audit, and repeat SD/network/standby fault acceptance checks.

## Suggested workflow automation

1. **A source-only preflight.** Run warning-scope verification, pair parity, diagnostic boundaries, dependency/core hash
   checks, and formatting checks before a manual audit. Reject stale nested examples and
   pair configuration drift early. `tests/verify_source_preflight.ps1` now combines these checks;
   formatting is enabled with its optional `-FormatterPath`. Keep this separate from compiler execution.
2. **Compare each audit with an approved baseline.** Report only new Loom-owned warnings,
   changed failures, and meaningful static RAM/flash growth. Keep external diagnostics
   visible in their own report. Compare warning identity after normalizing changing source
   line numbers; comparing total warning counts alone can miss a new warning replacing an old one.
3. **Select affected sketches for a quick audit, then use a full release audit.** Include both
   quiet/debug variants whenever their source or shared dependencies change. Run an R4/R5
   and SD/mux smoke set for shared infrastructure changes. Selection is an optimization;
   it must not replace the release matrix.
4. **Archive reproducible evidence.** Save source revision plus dirty-worktree patch, exact
   FQBN, compiler/package/dependency versions and hashes, requested sketch list, raw logs,
   warning split, firmware checksums, and memory/stack measurements together.
5. **Summarize soak logs automatically.** Flag changed reset causes, declining minimum free
   RAM, missed wakes, repeated SD rollbacks, and stalled batch delivery. Alert on a new or
   worsening condition, using thresholds tied to this device's five-minute sample and
   six-hour publish settings.

The existing repository already has a formatting workflow and retry-failed audit launcher.
Extend those mechanisms instead of introducing a second formatter or retry system.
Pin the formatter version so local formatting and CI use the same supported configuration.
Keep compiler automation manual until the user authorizes running it.

Arduino CLI supports warning levels, explicit library inputs, build properties, and build
directories for a reproducible matrix ([compile reference](https://docs.arduino.cc/arduino-cli/commands-reference/arduino-cli_compile)).
If these reports are later hosted in GitHub Actions, keep build logs and release evidence
as [workflow artifacts](https://docs.github.com/en/actions/concepts/workflows-and-actions/workflow-artifacts),
and use [dependency caching](https://docs.github.com/en/actions/concepts/workflows-and-actions/dependency-caching)
to accelerate repeated runs. These are suggestions; no new scheduled automation was created.
