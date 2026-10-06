#include <cassert>
#include <cstring>
#include <iostream>
static int traceCalls = 0, serialCalls = 0, cycleCalls = 0;
static int documentReads = 0, batchReads = 0;
static const char *tracePhase = nullptr, *serialPhase = nullptr;
#include "Diagnostics/Loom_DebugSketch.h"
static int &getDocument() { ++documentReads; static int value = 42; return value; }
static int getBatch() { ++batchReads; return 72; }
int main() {
#if LOOM_DEBUG_MEMORY
    Loom_MemoryDiagnostics reporter;
#endif
    // With memory off the absent reporter is legal, and document/batch accessors
    // must not run even if a trace checkpoint is requested.
    LOOM_DEBUG_BEGIN_CYCLE(reporter);
    LOOM_DEBUG_CHECKPOINT(reporter, "After measurement", getDocument(), getBatch());
    assert(cycleCalls == LOOM_DEBUG_MEMORY && serialCalls == LOOM_DEBUG_MEMORY);
    assert(documentReads == LOOM_DEBUG_MEMORY && batchReads == LOOM_DEBUG_MEMORY);
    assert(traceCalls == LOOM_TRACE);
    if (serialPhase) assert(std::strcmp(serialPhase, "After measurement") == 0);
    if (tracePhase) assert(std::strcmp(tracePhase, "After measurement") == 0);
#if !LOOM_DEBUG_MEMORY
    LOOM_DEBUG_CHECKPOINT(undeclaredReporter, "No Serial context", undeclaredDocument, undeclaredBatch);
    assert(traceCalls == 2 * LOOM_TRACE && documentReads == 0 && batchReads == 0);
#endif
    std::cout << "PASS shared checkpoint routing: trace=" << LOOM_TRACE
              << " memory=" << LOOM_DEBUG_MEMORY << '\n';
}
