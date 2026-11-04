# Arduino application assembly

When assembling a sketch or adapting an example, follow
`tools/feature-studio/ASSEMBLY_GUIDE.md` when present. Prefer the smallest working
implementation demonstrated by this checkout's examples and implementations.
Add complexity only when the user's requirements or verified hardware/API needs
require it. Feature Studio's prepared recipe packets are discovery aids; the
selected checkout remains the source of truth.

For any application/diagram reconstruction, keep each logic explanation traceable
to the actual generated code and to relevant current repository implementations.
Include exact source anchors for declarations, calls, conditions and delegated
behavior; distinguish verified references from inferred candidates or context.
Do not duplicate library internals merely to make them visible in a sketch.
Keep these instructions and the assembly request self-contained for a fresh run
without prior chat history. Re-check source references after branch or code changes.
