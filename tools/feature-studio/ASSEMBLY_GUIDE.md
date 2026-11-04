# Application assembly style

Start with the smallest relevant example in the current checkout and make the
fewest changes needed for the requested features. Read its API implementation
where behavior is uncertain. Prefer the example's ordinary structure and SDK
helpers over inventing an application framework.

- Include required Manager, board/controller, sensor, storage, initialization,
  measurement/packaging, scheduling and callbacks. Show these components in the
  feature overview and their operations in setup, loop and helper/callback groups.
- Use simple constants and direct calls for ordinary user settings. Do not add
  configuration files, parsers, state machines, retry engines, transport choices,
  generalized wrappers, extra diagnostics, or fallback services unless requested
  or necessary for the verified hardware/API behavior. State necessary assumptions.
- Preserve essential power sequencing, address/bus compatibility, payload identity
  and error behavior from the actual examples. Minimal code must still work.
- Use existing drivers and helpers. Avoid duplicated functionality, speculative
  abstractions, defensive branches for unrequested scenarios, and extra features.
- Keep graph nodes at meaningful feature/operation boundaries. Include real
  control branches and data/callback relationships, not every expression, flag or
  local variable. Group related steps by routine and keep supporting dependencies
  separate from the main workflow. Graph and code must agree.
- Cite the examples and APIs used. Keep credentials as placeholders. Report the
  validation actually performed. Do not install dependencies, flash hardware or
  edit library source just to assemble an application.
- Make explanations traceable on a cold run: every feature, executable step and
  connection should reference its actual declaration, call, branch condition or
  helper body in the final sketch. Supply `sourceLinks` using exact, unique code
  excerpts as anchors; edge target IDs are their zero-based indexes as strings.
  Include repository implementation references for behavior delegated to drivers,
  managers, callbacks or owned helpers, and explain the caller-to-callee relation.
  An API signature or class declaration alone does not establish its behavior.
  Follow inheritance and dispatch only where the current source supports it.
- Keep the sketch minimal when behavior already lives in the library. Link that
  implementation instead of duplicating it in generated code or expanding every
  internal statement into the main flow. Verify anchors after final edits; do not
  guess line numbers, copy stale branch references, cite commented-out alternatives
  as running code, or claim an uncertain match is exact. Clearly label unresolved
  assumptions and external implementations that are not in the selected checkout.

Before returning: remove scaffolding that is not needed by the requested behavior;
check every requested feature and supporting component is represented; check the
routine groups, wires and source anchors against the final sketch. Return the complete `.ino`
and only necessary companion files, plus the organized graph.

## Feasibility review

Return `featureAssessments` for each requested block and the nonempty behavior
brief (`requestId: "$brief"`). Check the selected object and actual board limits,
as well as Loom and compatible Arduino libraries/custom code. A missing Loom
setting alone is not a conflict. Use `supported`, `needs-info`, `partial` or
`unsupported`; unknown wiring, resources or external APIs require information,
not an invented impossibility. For every concern, quote the exact requirement,
give a short reason and the source/constraint checked in `issues`, then suggest
a feasible alternative where possible. Assign `nodeIds` only to the affected
behavior, not all objects that support the same request. Include current source
links for relevant implementation evidence. Keep infeasible requests visible as
non-executable graph nodes outside routines; implement the feasible subset and
document omissions clearly in code and warnings. Never fake a working feature.
If no behavior is feasible, an explicitly nonfunctional review sketch is honest;
do not report it as a complete working application.

## Explain the application like a development tool

Use names that say what the code does: “Read temperature”, “Save reading to SD”,
“Wait 5 minutes”, “Register wake callback”. Prefer the actual API name when a
plain-language summary would hide important behavior. Show the owning object/class
and source location rather than repeating its declaration as another workflow step.
Use “Features”, “Example”, “Code”, “Notes”, “Build” and “Flash” in interface copy;
avoid invented metaphors, motivational copy and vague phrases such as “bring it
to life”, “smart orchestration”, or “magic”. Explanations should answer what runs,
when/under what condition, and what data it reads or writes.

Imported recipes are indexed source, not a C++ compiler. Read their review flags.
Preserve conditions, early returns, compile-time alternatives, explicit state and
callbacks before simplifying. A line following another line in a file does not
prove that both execute. Do not invent a condition for a graph; cite the actual
test or state why the index cannot determine it. Callback registration and callback
execution must remain separate. Check non-Loom includes and companion headers.
If the prepared packet omits a relevant recipe for space, inspect its path from
`additionalRecipePaths` instead of making up an API. Start with a small base and
add only the behavior needed; do not copy a deployment example's diagnostic or
recovery framework just because it also uses the desired sensor.

Treat the sketch and its required helper files as one application. The recipe's
`projectFiles` list distinguishes bundled source from files needing separate
review; an excluded file is not evidence that it is unnecessary. Inspect relevant
helpers and configuration before adapting the example. Return only required
companion files, preserve their includes, and identify files the user must copy
to an SD card separately from flashing firmware. Keep credential placeholders;
do not copy local connection settings, documentation or build scripts into the
application merely because they are beside the example.
