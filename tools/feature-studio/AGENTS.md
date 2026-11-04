# Feature Studio maintenance

This is a standalone Node.js app with no npm dependencies. Run `start.cmd` on
Windows or `sh start.sh` on macOS/Linux. `launch.mjs` discovers the external Loom
library; never assume this app lives inside a Loom checkout. `--loom-root` or
`LOOM_SOURCE_ROOT` selects a specific checkout. Keep `.local` as user data.

Use `npm test` for the test suite. Tests use isolated workspaces and injected
compiler/download fixtures. Do not compile, install board packages, generate with
Codex, or flash a real device merely to validate the interface. Setup and each
assembly/build/flash require a user action. Preserve project tabs and saved files
when restarting; check that the local jobs are idle first.

For application generation, follow ASSEMBLY_GUIDE.md. Send its current instructions
in the prepared request so a fresh Codex CLI session has complete context. The
selected Loom source and examples establish APIs and behavior. Prefer minimal
example adaptations with exact source anchors and honest compatibility reviews.

Manual code edits are a supported output. Save them explicitly, use those exact
files for compilation, keep the original graphs, and label their possible
divergence. Never report that edited code was synchronized back to its graph.

Use browser UI checks for presentation changes. Retain selectable code and text
fields while preventing selection during canvas dragging. Keep popup dismissal,
keyboard access, stable hit targets, reduced motion, and high contrast in both
themes. Do not copy Codex authentication data into the app or project exports.
