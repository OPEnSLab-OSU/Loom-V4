# Feature Studio

A locally runnable feature-to-application canvas. Choose desired capabilities or
describe the application, and let **Codex CLI or your Codex Desktop chat** review the source,
resolve missing dependencies, generate an Arduino application and return an
organized workflow graph. Loom on the Feather M0 is the first source adapter.

Requires Node.js 18+ and a signed-in Codex CLI for button-triggered assembly.
There are **no npm dependencies, build step, hosted service, or API keys** in the
app. The CLI uses its own existing sign-in and model configuration.

Feature Studio runs independently in a normal browser. The ChatGPT desktop app
does not need to be open. Keep this entire folder anywhere you like; no npm install
is needed for Studio itself.

## Run

- Windows: double-click **start.cmd**.
- Mac/Linux: run **sh start.sh**.
- From a terminal in any folder: `node /path/to/FeatureStudio/launch.mjs`.

The launcher opens the browser and finds Loom in your Arduino board packages,
without assuming a package vendor, branch or version. It remembers the selected
Loom folder in `.local/studio-config.json`. With several installed versions, the
newest version is selected initially. Saved selection takes priority afterwards.
To choose another source folder, stop the server and run:

~~~text
start.cmd --loom-root "C:/path/to/Loom"
# Mac/Linux:
sh start.sh --loom-root "/path/to/Loom"
~~~

`LOOM_SOURCE_ROOT` also selects the source. `ARDUINO_DIRECTORIES_DATA` adds a
custom Arduino data directory to discovery. If Loom cannot be found, the launcher
asks for its library folder. The folder must contain library.properties, src and
examples. If a saved folder has moved, discovery runs again.

Open <http://127.0.0.1:4173>. Keep the launcher window open; Ctrl+C stops the server.
Running the launcher again reopens an existing server for this same workspace.
Use `--port 4175` for a different port, `--no-open` to skip opening a browser,
or `--workspace "/path/to/projects"` for a separate local workspace. Relative
workspace paths are relative to your terminal; the default is always `.local`
beside this app. Keep `.local` to preserve projects, history, outputs and settings.
Do not run two servers against the same workspace.

### Codex sign-in

Install Node.js 18+ from <https://nodejs.org>. Install Codex CLI once with
`npm install -g @openai/codex`, then use **More options → Codex sign-in / setup**
or the same button in **Assemble with Codex**. **Sign in with ChatGPT** starts
Codex CLI's browser sign-in. Studio only checks the CLI's sign-in status; it does
not store passwords, API keys or login URLs in your projects.

If the browser does not open, double-click **sign-in.cmd** or run
**sh sign-in.sh**. Add `--device-auth` for Codex's device-code flow, if enabled for
your account. Existing CLI sign-in is reused. Model and reasoning remain selectable
in Assemble; the initial choice is **gpt-6.1-sol / high**, with available models
loaded from the installed CLI. `CODEX_CLI_PATH` or `--codex` selects a custom CLI.
Assembly, compilation, installation and flashing only run when you ask for them.

The server listens only on 127.0.0.1 and rejects unrelated hosts/origins. You can
copy this folder to another laptop; install its own prerequisites and sign in
there. Do not copy Codex credentials. `.local` contains your project data, so omit
it when sharing an empty copy of the app.

## Assemble inside the app

**New design** starts with no blocks or preset behavior. First-time workspaces
also start blank. Choose one sensor, then use the **Behavior brief**, for example:
“Log temperature to SD every 5 minutes.” You can describe the whole application
without selecting every feature. **Assemble with Codex** fills in the supporting
features, setup/loop/callback graph and `.ino` files; the preview remains unreviewed
until that run finishes.

Starting a new design automatically saves the previous design and its current
valid result in `.local/design-history/`. **More options → Restore previous design**
recovers it, including after an app restart. Restoring saves the current design in
turn. Older snapshots remain in that folder. New design is disabled during an
active assembly; finish or cancel it first. The Custom ingredient library persists.

Each request is self-contained for a fresh computer or Codex session. It includes
the current source root/branch/fingerprint, target, selected features, written
behavior, discovered classes, and a ranked shortlist of current recipe paths and
the reasons they match. Both assembly paths instruct Codex to read those sketches,
inspect relevant companion guidance, verify the current implementations, search
further when needed, and cite the actual source paths used. No prior conversation
is needed. Recipe matches are search hints; compatibility still requires review.

In **Recipes**, **For this design** shows that shortlist. Matches include behavior
from the brief, such as logging and timing, as well as selected sensor classes.
Indexing ignores commented alternatives, include-only names and code-like text
inside diagnostic strings. Imported source stays intact, including custom logic.

Start with just the capabilities you want: for example, one sensor and **SD card
logging**. You do not need to add a Manager, manually select every controller or
wire the operation calls. Codex reviews this checkout and supplies the required
objects, initialization, measurement/packaging and log calls. Automatically added
components and operations appear in the returned graph with reasons; the original
requested features remain your editable inputs. Unsupported hardware choices or
combinations are explained in warnings.

Both the CLI and Desktop handoff use this completion policy. Their request includes
the selected features, discovered constructor/intent candidates and a compact index
of all available classes so supporting components outside your picks remain visible.
These are source-review hints, not a fixed dependency recipe or compatibility claim.

1. Add and configure blocks, or write the application brief in plain language.
2. Select **Assemble with Codex**. The default is **GPT-6.1 Sol, high reasoning**.
   Choose another model reported by your local CLI, or enter its exact ID. The
   reasoning choices update for the selected model.
3. Choose project permissions. The default **Review source & generate application
   files** uses read-only tools to inspect the source and returns generated file
   contents. **Allow project writes for validation** permits repository writes
   with the workspace-write policy; the task instructions still prohibit changes
   to Loom library source, this app or hardware.
4. Press **Send to Codex**. The local server starts codex exec with the selected
   repository as its working directory, the request and adapter guidance on
   stdin, and the assembly JSON schema as its output contract.
5. Wait in the app. It shows progress, supports cancellation, validates the
   returned revision/source/graph/files and displays the result automatically.

Codex receives repository access and tools according to its own configuration
and the selected permission mode. This does not inherit another Desktop chat's
conversation or its connected apps. It is a new CLI run in the selected project
folder. The server captures its returned session ID in the current job state.
Calls use your Codex account/model access and can consume its usage allowance.
The model selector reads model/list from a short-lived local Codex app-server
connection. It does not start an inference turn. The CLI's reported model catalog
is used for the menu; a completed generation verifies access to the chosen model.

## Playing with the canvas

The assembled flow groups source operations inside collapsible **setup()**,
**loop()**, and named **callback/helper** sections. Initialization, configuration
and callback registration calls appear alongside measurement and reporting steps.
The graph is wired automatically: blue arrows show step order, green wires show
data/results, purple dashed wires link callbacks, and gold wires show repetition.
Flow advances horizontally, with extra rows for branches. Nested
argument reads appear before their consumers, with a result wire. Imported recipes
show structural source order with conditions retained in source details; generated
assemblies request explicit branch/condition wiring from Codex. Existing routine
graphs gain visible wiring without changing the selected feature requirements.
Numbers show source order, not a claim that every conditional branch runs.
Callback badges link to the actual registration/call step. A block's Flow details
also lists its related routines and callbacks, with links to inspect them.
**Step order** lists incoming and outgoing routes near the top of Flow details.
Select a connected step to bring it into view, or use the file-and-line button
to inspect that connection in code. A shared execution/data route appears once,
with both explanations and source links retained. Data, calls/callbacks and
repetition have separate sections; supporting connections stay collapsed.
Different branch conditions remain separate routes. Imported examples still show
source order only, with their actual conditions in the linked sketch.
Shared component declarations remain on **Your features**. **Fit** shows the whole
hierarchy; the initial view starts at a readable zoom near setup(). SVG exports
keep the visible grouping and collapse state.

Both Codex assembly paths request explicit routine membership and callback
bindings that agree with the generated sketch. Older saved assemblies still open
using their original graph; load a recipe again or reassemble to get this richer
hierarchy. Example indexing remains conservative: arbitrary C++ control flow,
event timing, macros and nested call evaluation require source review.

The habitat supports wheel zoom around the pointer and background dragging to pan.
Fit shows the complete graph; the application drawer and behavior brief tuck away
when arranging features. Hover or focus a control for its description. Hints keep
their text still while surfaces and icons stretch; reduced-motion preferences are
respected. Optional click sounds start off.

The dock icons open ingredient **categories**. Choose a sensor, storage behavior or
other feature from the expanded list; category buttons never add arbitrary classes.
**Describe a feature** is a direct shortcut for a new plain-language block.
Use **Save to Custom** in that block's settings to keep its description, notes and
instance count in the local **Custom** library. Saved ingredients can be reused
without changing existing blocks; saving the same label updates that saved entry.
The library lives in .local/custom-features.json beside the design.
The tray stays below the workspace toolbar, and its ingredient list scrolls while
the heading and search stay visible. Recipe icons use the same hop and wiggle as
ingredient icons.

The canvas has generous panning margins, including outside return wires and
their full labels. Fit leaves room for the ingredient dock. Block settings fill
the side of the workspace with larger behavior fields.

The default flow view separates workflow steps, setup calls, other routines and
supporting components. Constructor/ownership lines stay in **All connections**
and in the inspector. This presentation keeps every node and verified payload
route; it does not invent runtime order. SVG export follows the selected view.
**Graph ↓** can also export an explicitly unreviewed intent preview before a
generation. VS Code and File actions become available when the current design
has an application result; editing features requires a new assembly.

The source bar shows the current local Git branch. Select a local branch and
press **Switch** to change it, then rescan and reassemble for that source. Git's
normal conflict checks protect local work: the app never forces, resets or
stashes edits, and it prevents switching during an assembly run.

**More options → Preview water sounds** enables and previews locally generated
liquid bloops. Sounds use Web Audio with a rounded rising bubble, faint glassy
attack, soft airy texture and short echoes. Close/remove buttons use a separate
quiet descending puff; no sound files are downloaded. The app reports blocked
browser audio. System/app volume and mute settings still apply.
Categories, nodes, panels, tabs, zoom controls, source actions, recipes and assembly
have distinct sound profiles with small repeat variations. The behavior brief has
its own close button. File previews have independent horizontal/vertical scrolling;
before assembly, the drawer explicitly says that no application file exists yet.

Click a connection name to inspect its explanation and linked code. A block's
source badge and class-definition buttons can open VS Code, which must be installed
locally. The Files drawer has syntax colors, line numbers, and a VS Code button.

Node and connection explanations put **Jump to sketch code** first. It opens the
matching file in the in-app Files drawer, scrolls to the source lines, and highlights
the referenced range. Additional sketch references can jump to their own ranges.
Library implementations are grouped under a collapsed **Under the hood** section;
their excerpts and VS Code links remain available without crowding the sketch view.

Flow steps show the actual owning object and class, derived from sketch declarations
and linked calls rather than the ingredient category. For example, Start serial
can belong to `manager · Manager`. Connection bubbles include their linked source
file and line range as part of the comment. Double-click, or Shift+Enter, jumps to
the linked sketch lines in Studio; repository implementation references open in
VS Code. A single click still opens the explanation. Inferred/context locations
are labeled accordingly. Graph exports include the same source captions and class
associations. Source maps are rechecked when the checkout changes.

Requested features have a remove button. The assembled flow and added supporting
components are read-only; change the requested features or behavior and reassemble.

## Projects and complete exports

New designs, checkout recipes, and imported projects open in separate project tabs.
Each project retains its feature board, generated files, graph, and source references.
Renaming a project or moving its blocks keeps its built result. Changes to behavior
mark it as **Previous assembly** until reassembly; the previous graph and code remain
reviewable, survive tab switches, and are included in project exports.
**Changes** beside the project tabs compares current requests with the snapshot
used for that assembly: added/removed features, behavior text, instance counts,
constructor parameters and other settings. Review links open the affected feature
or brief. Moving blocks and renaming the project do not count as request changes.
The comparison preserves the original requests while you edit; it does not rewrite
the displayed code. Restoring the exact original requests reuses the matching
saved result after rechecking its source fingerprint, including after tab switches.
Tabs and the selected project survive a server restart. Closing a tab keeps the
project under **Examples → My projects**. Reopen it there, or use its × button and
confirm to delete that saved project. Checkout recipes are never deleted this way.
Project switching is paused while Codex is assembling so its response stays with
the project that requested it.

In **Examples**, search across class names, API calls, operations and behaviors;
multiple words can match different parts of the same example (for example,
`WiFi SD` or `logToSD()`). **Preview** reads the selected checkout's prepared index
without opening or changing a project. It lists referenced classes, setup/loop,
callbacks/helpers and their registration/call sites, custom-logic review flags,
and companion files. Choose **Open example** to create a new project tab with
the original feature board, wired graph and sketch. Enter in search previews the
first matching example. **My projects** marks the current tab and lists saved
projects by recent save, with the current project first. These previews describe
source structure; compatibility still needs review when adapting the sketch.

**Undo**, **Redo**, and **History** sit beside the project tabs. Each project keeps
up to 200 saved design edits, including block addition/removal, settings, title,
behavior brief, and layout. Ctrl+Z and Ctrl+Y/Ctrl+Shift+Z work outside text fields;
text fields retain their ordinary text undo. The history list can restore an earlier
design directly. Editing after undo starts a new history branch. History survives
restarts; it changes design inputs while retaining the most recent built result.

**More options → Export project** downloads a `.studio.json` bundle containing the
design, complete assembly (nodes, connections, routines, generated source files,
notes, validation, and source links), example provenance, and checkout fingerprint.
**Import project** restores this bundle in a new tab; older design-only JSON files
also remain supported. Completed results are checked against the selected checkout
on import. If Loom has changed, your feature requests still open, including classes
no longer present in that checkout. The previous code and flow remain available as
**Previous assembly · Loom checkout changed** with their original source fingerprint.
Assemble again when adapting it to the current implementations. Build can compile
the saved assembled code against the current checkout; opening an older project
never silently marks its feature requests as assembled. Library implementations stay in the repository and are referenced by
their source anchors; the bundle includes generated application files, not a copy
of Loom or its dependencies. **File ↓** exports one source file and **Graph ↓**
exports the currently selected diagram view; use Export project for reconstruction.

## Arduino device tools

Use the top workflow: **1 Assemble with Codex → 2 Build → 3 Flash**. Review
the generated sketch in the full-size **Code** tab and explanations in **Notes**.
Build and Flash open separate menus. The tool detects Arduino CLI from PATH,
`ARDUINO_CLI_PATH`, an existing Arduino IDE installation, or a saved explicit path.
It uses an existing Arduino IDE CLI configuration when available. On a fresh
machine it creates a local configuration and discovers Boards Manager URLs from
the selected checkout's README. No compile, upload, or package install runs on
startup, assembly completion, project switching, or opening the panel.

The board is preselected as **Loomified Feather M0**. **Build** has a project
picker containing open tabs with assembled code. It compiles that project's saved
files without switching the active tab, including while its feature requests are
being edited. A connected board or COM port is not required, and Build does not
scan serial ports. **Review code** opens the selected project's Code tab.
**Open binaries folder** opens its matching successful build in the file manager.
**Flash** has its own project and COM-port controls.

**Build ready** shows the build time and expandable output-file list with sizes.
**Open Flash** carries that same project into the Flash menu without uploading;
**Open Build** returns an unbuilt project to Build without changing the selection.
The saved files are checked before a cached result is offered for flashing.
Missing or differently sized recorded output requires a new build. Earlier
compiler output remains readable without labeling those missing binaries ready.

Build compiles a separate copy of those files with the selected Loom checkout explicitly supplied as a
library. It shows the complete command output and any missing headers. **Flash**
is enabled only after the exact source and board pass Build, and uploads those
saved binaries without compiling again. Changing the code, board, checkout, or
tool setup requires a new build. No hardware flashing is part of automated tests.

Compiler errors and warnings appear as readable
cards with file, line and column; notes, full file paths and the complete tool log
remain expandable. **View file:line** opens a verified line in the in-app Code tab.
These links are checked against the build's project, source fingerprint and exact
generated files when clicked. A link can open another open project tab, including
one of its helper headers, and highlight the reported line. The app saves the
current draft, opens that project's code and rechecks the build reference before
jumping. **Build messages** returns to that project's compiler output. Earlier
messages stay readable, but cannot jump into a different version of the code. Library and
generated compiler files retain their reported locations without guessed sketch
line mappings. Missing-header searches open the library setup controls directly.

Under **Setup**, **Get Arduino CLI** downloads the official Windows x64 release
into this workspace and verifies its published SHA-256 checksum. Other operating
systems can use an existing CLI or the linked official installer instructions.
Find board packages or libraries in Arduino's catalog, then explicitly choose
**Install**. Library installation lets Arduino CLI resolve its declared dependencies.
Missing-header diagnostics offer a library search. **Refresh package catalogs**
updates the board and library indexes; additional third-party Boards Manager URLs
and CLI/configuration paths can be saved in the expanded settings. None of these
setup controls run automatically. Tool output and failures stay visible for review.

During assembly, Codex returns a feasibility review for each requested block and
the behavior brief. Verified conflicts appear red with the affected requirement,
reason, evidence and a suggested alternative; requests needing hardware/API details
appear amber. Missing Loom functionality alone is not a conflict: compatible custom
Arduino code and external libraries are allowed. This is source-based review, not
proof of physical hardware behavior. Older recipes without assessments stay neutral;
changed requests are reviewed again when assembled.

## Examples as recipes

Select **Recipes** to browse every .ino discovered in the current checkout.
Search or filter by its repository group, then load a recipe into the canvas.
It opens in its own project tab and keeps the existing creation available.

Each example is reconstructed from its source declarations, constructor
arguments and recognizable measurement, packaging, storage, radio, reporting
and scheduling calls. The original .ino appears alongside the graph. Source
dependencies and routes are explicit, and the graph is labeled **Source
reconstruction**, not a reviewed Codex assembly.

Callbacks, helper functions, non-Loom includes, conditional behavior and other
custom implementation are denoted with **Describe a feature** blocks and
per-block behavior text. Select any block to edit that behavior in plain
language, including sensor options not exposed by the library. Such behavior
appears as a small note on its ingredient bubble.

The lightweight source index does not evaluate preprocessing, callbacks,
aliasing or general C++ control flow. A graph can therefore be a coarse summary
of a complex example. Opening an example includes its adjacent helper source
(`.h`, `.hpp`, `.cpp`, `.c`) without changing those files. They appear in Code
and remain in saved projects and complete project exports. Configuration headers,
credential-related files and runtime configuration need a separate review;
the preview and Notes list additional discovered files that are not bundled.
Subfolder helpers are not flattened into the sketch folder.
Use the assembly button to ask Codex to review and adapt the example against
the current implementation before treating it as deployable firmware.

Source tracking includes example helper code and JSON configuration, so changes
to them invalidate saved results just like changes to an `.ino`. Documentation
edits refresh the prepared recipe packet without marking the sketch outdated.
The assembly guide asks Codex to inspect required companion files and distinguish
SD-card configuration from files compiled into firmware.

Install/sign in to the CLI separately if needed. The server discovers codex on
PATH, including the standard Windows npm installation. You can select a CLI
executable or the package's bin/codex.js at launch:

~~~text
node server.mjs --codex "C:/path/to/codex.exe"
~~~

CODEX_CLI_PATH provides the same setting. Commands use argument arrays with no
shell interpolation. Interactive approval requests are disabled for this
bounded noninteractive run; tools needing permissions outside the selected
policy fail rather than waiting for an invisible approval prompt. The app does
not bypass the CLI's protection settings.

Run snapshots, result and status are stored under .local/runs/. The server owns
the final assembly.json write after validation. Do not run multiple app servers
against the same workspace.

Select **Activity** in the assembly status bar to see the model, elapsed time,
current stage and recent updates. These are public Codex messages and counts of
repository commands; hidden reasoning and raw command output are not shown.
The latest run status stays with each project across tabs and app restarts.
If Studio stops before completion is recorded, the run is marked **Interrupted**.
It is not resumed automatically. Review the requests and any previous code,
then use **Review and send again** to open the model and access settings before
explicitly sending another run. **Copy details** copies a compact run report.

Assembly checks the returned file and graph structure. Firmware compilation,
package installation and upload remain separate manual Build and Flash actions.

See the official [Codex noninteractive documentation](https://developers.openai.com/codex/noninteractive).

## Alternatively, use an existing Codex Desktop chat

1. Drag feature cards onto **Your features**, or click a card to add it.
2. Select a block to configure physical instance counts, behavior, per-set
   requirements and optional constructor arguments. Use the application brief
   for a design described entirely in natural language. Blocks are requirements,
   not a circuit editor.
3. Select **Assemble with Codex → Use an existing Desktop chat instead**, copy the
   handoff and paste it into your Codex Desktop chat with access to the selected
   source checkout and this workspace.
4. Codex reads the saved request and actual source/examples, then writes
   .local/assembly.json. The app polls for that result and shows the organized
   graph, reasons for added components, application files, warnings and validation.
5. Review the files and validation. Download individual files or the SVG diagram.
   All returned files are also materialized together under the output directory
   shown in **Assembly notes**. Open FeatureApplication.ino from its same-named
   folder in the Arduino IDE.

The alternative Desktop connection is an explicit **two-way file bridge**, not
an automatic chat-message sender or a private Desktop API. After saving, you
can also tell an existing project chat “assemble the current Feature Studio
design” and give it the workspace path.

**Assembled flow** before a Codex result is an explicitly unreviewed *intent
preview*. It uses capability categories to visualize the request; it does not
generate C++ or establish compatibility. Actual assembly and dependency decisions
belong to Codex. The graph describes software workflow and dependencies, not
verified physical wiring.

## Branch/version independence

adapters/loom.mjs scans the chosen checkout's src headers and implementations
and its example sketches. It discovers class names, inheritance,
constructors, parameter defaults and related examples. It rescans on startup,
on request and periodically while open. It does not depend on Git branch names,
release numbers, a hardcoded list of sensors, or a fixed Loom API.

The intent cards (sleep, local logging, batching, reporting, gateway, diagnostics
and custom requirements) express stable *goals*. Keyword matches to discovered
classes are hints only. They do not dictate a class, add a Hypnos instance,
construct a dependency graph or generate a sketch. Codex must decide those things
from the current implementation and explain its choices. Constructor extraction
is intentionally a lightweight discovery index, not a full C++ parser; inspect
source before relying on an extracted signature.

Source fingerprints include source and sketch contents, not merely commit
identifiers, so local edits and branch changes invalidate old assemblies.
Every behavioral design edit changes its revision; moving a block does not.
Only a result matching the current design revision **and** source fingerprint is
accepted. Unknown blocks survive imports and source changes so Codex can report
and resolve them explicitly. Existing credentials are excluded from discovery.
Do not enter real credentials in the design.

## Prepared recipes and assembly style

Startup prepares the current checkout once: example reconstructions, setup/loop/
callback graphs, API signatures, and companion-file locations. Later starts reuse
that cache. Source edits, example guidance changes, branch changes with different
contents, and updates to the extraction or assembly rules refresh it automatically.
This works from a fresh clone without prior chat history.

Recipe graphs are reconstructed automatically by the local source parser, without
an LLM call. The existing `.ino` stays intact. GPT handles new/custom assemblies
and their corresponding graphs when **Assemble with Codex** is requested.
Reconstruction is structural: custom expressions and runtime conditions can still
need source review.

The branch selector automatically rescans both library code and example sketches
after switching. Changed, added, and removed examples update the recipe library;
supporting example files and guidance also invalidate preparation. Switching back
to identical checkout contents reuses their cache. External checkout edits are
detected while the app is open (within the normal scan interval), or immediately
with **Rescan source**. Results made against different source/sketch contents are
marked stale; loading a recipe reconstructs it for the selected checkout.

To prepare a checkout ahead of time, run `npm run prepare`. For another checkout,
use `npm run prepare -- --loom-root "C:/path/to/Loom"`. Add `--force` to rebuild.
The local cache lives under `.local/prepared`; it is disposable and not committed.

Each assembly request includes a small complementary set of matching sketches,
routine/wire blueprints, relevant API references, and the portable
[assembly guide](ASSEMBLY_GUIDE.md). Codex should start there, inspect unresolved
details in the actual source, and use the smallest working implementation that
satisfies the requested behavior. Preparation saves repeated source discovery;
model response time and correctness still depend on the request and validation.

The feature overview shows requested blocks plus supporting components added by
Codex. The default workflow shows setup, loop, and callbacks without repeating
component cards. Supporting links in step details open the component on **Your
features**; **All connections** remains available as an explicit dependency view.
Branches occupy separate
rows; repeated step/data routes share a wire. Sequences continue horizontally with space reserved for connection
labels; pan across long routines instead of following artificial folded rows.
Select a block to highlight its
connections, collapse a routine to reduce clutter, or use **All connections** to
inspect component input links and supporting dependencies. **Go to routine** jumps to a readable view of
setup, loop, or a named callback/helper. Imported examples show source order; runtime
conditions still require review of their code.

Assembled nodes are read-only. Edit or remove requested ingredients in **Your
features**, then reassemble. Click empty canvas or press **Escape** to clear a
selected block and restore all connections; Escape closes an open dialog first.

## File contract

Logic explanations include clickable code evidence. Select a block for its sketch
call and relevant library implementations, or click a connection bubble to inspect
its condition/payload relationship and delegated behavior. Each code excerpt has
a file/range and an **Open this code in VS Code** link. Older results use symbol
matching where possible; suggestions and routine context are labeled explicitly.

New assemblies return `sourceLinks`: `{targetKind, targetId, scope, file, anchor,
explanation}`. Targets are node/routine IDs or zero-based edge indexes as strings;
scope is `application` or `repository`. Anchors are exact, unique code excerpts,
checked against the current generated files or selected checkout. Library links
explain internals without duplicating that code or expanding the workflow. Missing,
ambiguous, or stale anchors appear as unresolved references, never as an exact
line-1 link. The portable guide and both generation prompts carry this contract.

The server owns:

- design.json: requested features, settings, counts, positions and brief.
- request.json: design, selected source metadata, revision and fingerprint.
- assembly.schema.json: the structured return contract.
- codex-prompt.txt: complete Desktop handoff instructions.

Codex owns assembly.json, written atomically using a temporary file plus rename.
See the generated schema. The required fields are:

~~~text
schemaVersion: 1
designRevision, sourceFingerprint: copied verbatim from request.json
summary: what the resulting application does
nodes: [{ id, label, category, added, reason, requestIds }]
edges: [{ from, to, kind: "flow" | "callback" | "data" | "dependency" | "cycle", label }]
routines: [{ id, name, kind: "setup" | "loop" | "callback" | "helper", nodeIds, bindings, reason }]
sourceLinks: [{ targetKind: "node" | "edge" | "routine", targetId, scope: "application" | "repository", file, anchor, explanation }]
files: [{ name, content }]
decisions: [plain-language choices and hardware assignments]
warnings: [incompatibilities, unresolved choices, practical limits]
validation: honest checks run and checks still needed
~~~

Every requested block ID must appear in at least one node's requestIds.
Added components require a reason. An unsupported request must be represented
and explained rather than silently dropped. Files must have flat, safe names
and include an .ino; the primary sketch should be FeatureApplication.ino.
Assembly never automatically flashes hardware, installs dependencies or modifies
Loom library source. Build, Flash and package installation require their separate
explicit controls. Output validation checks the envelope, references and revision;
it does **not** prove that generated C++ is correct or safe on a particular board.

## Extending beyond Loom

The design document, node/edge/file return contract, canvas and Desktop bridge
are application-neutral. A source adapter supplies discovery metadata,
categories, intent suggestions, a target and assembly guidance. The server
currently selects the Loom adapter; adding another integration means supplying
an adapter and selecting it there. This is a first prototype, not a universal
source parser or a visual programming language.

## Checks

The canvas menu exports the selected view: **Export features (SVG)** preserves
feature cards, instance counts, settings summaries and their layout;
**Export flow (SVG)** preserves routine groups, wires and source references.
Last-build flow exports are labeled as such. **Export project** in the top menu
still saves the full `.studio.json` reconstruction bundle, including all code,
graph data, notes and provenance; an SVG is a standalone visual summary.

In **Code**, **Find in code** (or **Ctrl+F**) searches literal text in the current
file without changing it. Enter and Shift+Enter move between matches; Escape
closes search. Search is case-insensitive and shows up to 1,000 matches; a `+`
indicates more matches, so narrow the query. **Copy code** copies the original
current file without display line numbers or search markup, including when
reviewing a clearly labeled previous assembly. Building and flashing remain separate
manual actions.

**Find** beside the project tabs (or **Ctrl+K**) searches the current project's
features and workflow steps, including descriptions, class names and API calls.
Results identify their routine and source step. Enter opens the first result;
arrow keys move through results. Opening a result expands its routine and brings
the block into view without changing the design. The feature-library search
continues to search available ingredients separately.

Flow steps have a **Line: #** button that jumps directly to the linked sketch
line. **Back to flow** restores the selected step, zoom and canvas position.
References that only resolve to library implementations use **Library ↗**;
approximate sketch matches are marked with **≈**. Selecting a step still opens
its explanation, class association and implementation references in the inspector.

**Outline** in Assembled flow lists setup, loop, callbacks and helpers without
requiring a zoomed-out graph. Select a numbered step to open its section and reveal
it clear of the settings panel. Expand or collapse routine groups independently;
**Routine code** opens the function definition, and caller/registration links
reveal the corresponding call site. Registration is kept separate from callback
execution. Steps stay in source order; inspect Code for branch and event timing.

The reverse lookup is available in **Code**: click a source line or enter its
number, then choose **Show in flow**. Select a linked step to reveal it, including
steps inside collapsed routines. Expand **Connections** to trace an individual
wire; it is highlighted until you select another step or press Escape. Exact
anchors, suggested matches and routine context keep their distinct labels.
Includes and comments may have no corresponding flow block. This navigation
does not change the sketch, requested features, or assembly status.

The sun/moon button in the top bar switches between deep-blue dark mode and light
mode. It remembers the local preference independently of projects. Sounds remain
optional under More options; the synthesized droplets use soft attacks, filtered
high frequencies and a low playback level.

Prepared recipe packets include review flags for custom logic, control statements,
helper functions, non-Loom includes and compile-time alternatives. Their graph
nodes retain source lines in the original repository file. Additional relevant
recipe paths stay available when the excerpt budget cannot include every sketch.

~~~text
npm test
~~~

Tests cover procedural discovery in a synthetic future checkout, constructor
parsing, stale results, block coverage, file-path rejection, the HTTP
save/handoff/return loop, CLI launch arguments, model selection, progress,
stale CLI results, process cancellation, dynamic model discovery and reconstruction
of every example in the selected checkout. A local CLI protocol fixture is
used; tests do not invoke an LLM or flash a board.

Feature notes: Describe a feature opens as an editable sticky note on Your features. Its text saves as the block requirement; the open/collapsed state is remembered in this browser for each project and block. Collapse it from its corner button. Connection labels isolate their wire on the first click and show the explanation on the next click; double-click jumps to linked code.

In Code, choose **Edit code**, change the file, then **Save code** (Ctrl+S). Build and Flash use saved edits. Cancel discards the current draft. Unsaved drafts stay available while switching tabs in this session; a reload warns before losing them. Customized code is clearly labeled: graphs/features remain the original assembly. Previous file contents are backed up under `.local/code-history/`. Saving rejects files changed since editing began. Build tool checks warm in the background at launch and are reused for five minutes; Refresh tools forces another check. COM ports are checked separately when opening Flash.

### Missing Loom or board packages

If Loom is not found, the launcher offers **Install and open Studio** or lets you
choose an existing Loom folder. For explicit first-run installation, run
`start.cmd --setup` or `sh start.sh --setup`. It gets Arduino CLI if missing
(official release and verified checksum), refreshes the package indexes, then
installs missing **arduino:samd → adafruit:samd → loom4:samd** in that order.
Installed core versions are kept. It verifies the Loomified Feather M0 board and
finds the Loom library included in that package before starting Studio. There is
no compile or upload during setup. Allow several minutes and an internet
connection for a fresh machine. Close a running Studio before using --setup.

For setup without opening Studio, use `setup.cmd --yes` or `sh setup.sh --yes`.
Windows x64, macOS x64/ARM64 and Linux x64/ARM64/ARMv7 use their respective official
Arduino CLI archives. Installed CLI executables are reused. Studio adds the two
board index URLs from the Loom checkout's documented setup to its own CLI flags,
without replacing your Arduino IDE configuration. Setup can be rerun after an
interrupted download; installed boards are skipped.

Package sources: [Loom setup](https://github.com/OPEnSLab-OSU/Loom-V4#install-board-profiles)
and [Adafruit SAMD setup](https://learn.adafruit.com/adafruit-feather-m0-adalogger/using-with-arduino-ide).

Recipes includes checkout examples and My projects. Empty Untitled application tabs stay out of My projects. Closing an untouched starter removes its recipe entry; one fresh blank workspace remains if it was the last tab. Features, a behavior brief, generated code or a custom name make it a saved project. Saved projects remain in Recipes after closing their tabs. Old empty snapshot files remain locally for recovery.
