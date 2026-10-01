// Allocator hook definitions. Include in exactly one selected translation unit.
// Shared by the CLI build and optional IDE-discovered Loom Trace Heap library.
#include "Loom_Trace.h"
#include <cstddef>
#include <cstdint>

namespace {
unsigned int allocatorDepth = 0;
class AllocationScope {
  public:
    AllocationScope() { ++allocatorDepth; }
    ~AllocationScope() { --allocatorDepth; }
    bool outermost() const { return allocatorDepth == 1; }
};
} // namespace

#define LOOM_HEAP_CALLER reinterpret_cast<uintptr_t>(__builtin_return_address(0))

extern "C" {
struct _reent;
bool loomTraceHeapHooksLinked() { return true; }
void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *pointer, size_t size);
void __real_free(void *pointer);
void *__real__malloc_r(_reent *, size_t);
void *__real__calloc_r(_reent *, size_t, size_t);
void *__real__realloc_r(_reent *, void *, size_t);
void __real__free_r(_reent *, void *);

void *__wrap_malloc(size_t size) {
    AllocationScope scope;
    void *pointer = __real_malloc(size);
    if (scope.outermost()) {
        Loom_Trace::heapEvent(pointer != nullptr ? 'A' : 'N', pointer, size, nullptr,
                             LOOM_HEAP_CALLER);
    }
    return pointer;
}

void *__wrap_calloc(size_t count, size_t size) {
    AllocationScope scope;
    void *pointer = __real_calloc(count, size);
    const bool overflow = size != 0 && count > SIZE_MAX / size;
    if (scope.outermost()) {
        Loom_Trace::heapEvent(pointer != nullptr ? 'A' : 'N', pointer,
                             overflow ? SIZE_MAX : count * size, nullptr, LOOM_HEAP_CALLER);
    }
    return pointer;
}

void *__wrap_realloc(void *previous, size_t size) {
    AllocationScope scope;
    const uintptr_t previousAddress = reinterpret_cast<uintptr_t>(previous);
    void *pointer = __real_realloc(previous, size);
    // Failure leaves the previous allocation live. A zero-size call's allocator-dependent
    // outcome is explicitly marked Z; the converter must not claim it definitely freed.
    if (scope.outermost()) {
        Loom_Trace::heapEvent(size == 0 ? 'Z' : (pointer != nullptr ? 'R' : 'N'), pointer, size,
                             reinterpret_cast<void *>(previousAddress), LOOM_HEAP_CALLER);
    }
    return pointer;
}

void __wrap_free(void *pointer) {
    AllocationScope scope;
    if (scope.outermost()) {
        Loom_Trace::heapEvent('F', pointer, 0, nullptr, LOOM_HEAP_CALLER);
    }
    __real_free(pointer);
}

void *__wrap__malloc_r(_reent *state, size_t size) {
    AllocationScope scope;
    void *pointer = __real__malloc_r(state, size);
    if (scope.outermost()) {
        Loom_Trace::heapEvent(pointer != nullptr ? 'A' : 'N', pointer, size, nullptr,
                             LOOM_HEAP_CALLER);
    }
    return pointer;
}

void *__wrap__calloc_r(_reent *state, size_t count, size_t size) {
    AllocationScope scope;
    void *pointer = __real__calloc_r(state, count, size);
    const bool overflow = size != 0 && count > SIZE_MAX / size;
    if (scope.outermost()) {
        Loom_Trace::heapEvent(pointer != nullptr ? 'A' : 'N', pointer,
                             overflow ? SIZE_MAX : count * size, nullptr, LOOM_HEAP_CALLER);
    }
    return pointer;
}

void *__wrap__realloc_r(_reent *state, void *previous, size_t size) {
    AllocationScope scope;
    const uintptr_t previousAddress = reinterpret_cast<uintptr_t>(previous);
    void *pointer = __real__realloc_r(state, previous, size);
    if (scope.outermost()) {
        Loom_Trace::heapEvent(size == 0 ? 'Z' : (pointer != nullptr ? 'R' : 'N'), pointer, size,
                             reinterpret_cast<void *>(previousAddress), LOOM_HEAP_CALLER);
    }
    return pointer;
}

void __wrap__free_r(_reent *state, void *pointer) {
    AllocationScope scope;
    if (scope.outermost()) {
        Loom_Trace::heapEvent('F', pointer, 0, nullptr, LOOM_HEAP_CALLER);
    }
    __real__free_r(state, pointer);
}
} // extern "C"

#undef LOOM_HEAP_CALLER
