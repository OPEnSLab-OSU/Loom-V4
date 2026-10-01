#include <cassert>
#include "Utilities/Loom_BufferPool.h"
#include "ArduinoJson.h"
int main() {
    loomMemory::FixedBufferPool<1024, 2> pool;
    assert(pool.allocate(0) == nullptr && pool.allocate(1025) == nullptr);
    void *first = pool.allocate(1024), *second = pool.allocate(1);
    assert(first && second && first != second && pool.activeBuffers() == 2);
    assert(pool.allocate(1) == nullptr && pool.peakBuffers() == 2);
    pool.release(static_cast<uint8_t *>(first) + 1);
    int foreign = 0;
    pool.release(&foreign);
    assert(pool.activeBuffers() == 2);
    pool.release(first);
    pool.release(first);
    assert(pool.activeBuffers() == 1);
    assert(pool.allocate(10) == first);
    pool.release(first);
    pool.release(second);
    {
        BasicJsonDocument<loomMemory::JsonAllocator> document(1024,
                                                              loomMemory::JsonAllocator(&pool));
        document["id"]["name"] = "node";
        document["id"]["instance"] = 1;
        document["value"] = 42;
        assert(!document.overflowed() && pool.activeBuffers() == 1);
        document.shrinkToFit(); // Compaction must not drop or free the pool slot.
        assert(document["value"] == 42 && pool.activeBuffers() == 1);
        BasicJsonDocument<loomMemory::JsonAllocator> other(1024, loomMemory::JsonAllocator(&pool));
        BasicJsonDocument<loomMemory::JsonAllocator> exhausted(1024,
                                                               loomMemory::JsonAllocator(&pool));
        assert(other.capacity() > 0 && exhausted.capacity() == 0); // No heap fallback.
    }
    assert(pool.activeBuffers() == 0 && pool.failedAllocations() >= 4);
}
