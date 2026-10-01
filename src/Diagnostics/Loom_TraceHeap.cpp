// Explicit compiler/linker flags retain compatibility with the CLI comparison helper.
#include "Loom_Trace.h"
#if defined(LOOM_TRACE_LINKER_HEAP_HOOKS) && LOOM_TRACE_LINKER_HEAP_HOOKS
#include "Loom_TraceHeapHooks.inc"
#endif
