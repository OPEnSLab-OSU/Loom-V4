#pragma once

#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Arduino.h>
LOOM_EXTERNAL_INCLUDE_END
#include <stdint.h>

#if defined(__arm__)
extern "C" char *sbrk(int increment);
#else
// Retain MemoryFree's platform-specific behavior outside the ARM deployment target.
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <MemoryFree.h>
LOOM_EXTERNAL_INCLUDE_END
#endif

namespace LoomMemory {
// Like MemoryFree, this reports the current stack-to-heap gap, not allocator capacity or a
// guarantee that an allocation will succeed. No allocation is performed to obtain the value.
inline int freeMemoryBytes() {
#if defined(__arm__)
    char stackMarker = 0;
    const intptr_t stackAddress = reinterpret_cast<intptr_t>(&stackMarker);
    const intptr_t heapAddress = reinterpret_cast<intptr_t>(sbrk(0));
    return static_cast<int>(stackAddress - heapAddress);
#else
    return ::freeMemory();
#endif
}
} // namespace LoomMemory
