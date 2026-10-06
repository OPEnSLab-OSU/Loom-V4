#pragma once

// Shared sketch helpers: keep optional Serial memory reports and trace RAM
// snapshots under one phase label without application-specific globals/macros.
// Define LOOM_DEBUG_MEMORY and the trace flags before including this header.
#ifndef LOOM_DEBUG_MEMORY
#define LOOM_DEBUG_MEMORY 0
#endif

#include <Diagnostics/Loom_TraceSketch.h>

#if LOOM_DEBUG_MEMORY
#include <Diagnostics/Loom_MemoryDiagnostics.h>
#define LOOM_DEBUG_BEGIN_CYCLE(reporter) do { (reporter).beginCycle(); } while (false)
#define LOOM_DEBUG_SERIAL_CHECKPOINT(reporter, phase, document, batchCount) \
    do { (reporter).checkpoint(F(phase), document, batchCount); } while (false)
#else
// Disabled reports do not evaluate the reporter/document/batch arguments. The
// sketch can omit its Loom_MemoryDiagnostics object entirely in this mode.
#define LOOM_DEBUG_BEGIN_CYCLE(reporter) do {} while (false)
#define LOOM_DEBUG_SERIAL_CHECKPOINT(reporter, phase, document, batchCount) do {} while (false)
#endif

// phase must be a static string literal: Serial uses F(), and trace retains the
// label's address. Document and batch count are evaluated once, only for Serial.
// Before trace startup, Serial reports can still describe initialization phases.
#define LOOM_DEBUG_CHECKPOINT(reporter, phase, document, batchCount) \
    do { \
        LOOM_DEBUG_SERIAL_CHECKPOINT(reporter, phase, document, batchCount); \
        LOOM_TRACE_CHECKPOINT(phase); \
    } while (false)
