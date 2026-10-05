#define LOOM_TRACE 0
#define LOOM_TRACE_HEAP 1
#include "Diagnostics/Loom_TraceSketch.h"
#include <cassert>

int main() {
    int evaluated = 0;
    LOOM_TRACE_ATTACH(++evaluated, ++evaluated);
    LOOM_TRACE_ATTACH(managerThatDoesNotExist, hypnosThatDoesNotExist);
    assert(evaluated == 0); // Trace-off never evaluates arguments or requires recorder symbols.
}
