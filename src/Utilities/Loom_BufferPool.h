#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace loomMemory {
////////////////////////////////////////////////////////////////////////////////////////////////////
// One slot holds one entire buffer. Allocation takes the next free slot in constant time;
// buffers are never split, so the pool cannot develop holes too small for the next request.
// Intended for one loop thread, not interrupts. The pool must outlive all borrowed buffers.
class BufferPool {
  public:
    virtual ~BufferPool() = default;
    virtual void *allocate(size_t bytes) = 0;
    virtual void release(void *buffer) = 0;
    virtual size_t capacity(const void *buffer) const = 0;
};

template <size_t Bytes, size_t Slots> class FixedBufferPool : public BufferPool {
    static_assert(Bytes > 0 && Slots > 0 && Slots < UINT16_MAX, "Pool size must be bounded.");

  public:
    FixedBufferPool() {
        for (uint16_t i = 0; i < Slots; ++i) {
            next[i] = static_cast<uint16_t>(i + 1);
        }
    }
    FixedBufferPool(const FixedBufferPool &) = delete;
    FixedBufferPool &operator=(const FixedBufferPool &) = delete;
    void *allocate(size_t bytes) override {
        if (bytes == 0 || bytes > Bytes || firstFree == Slots) {
            ++failures;
            return nullptr;
        }
        const uint16_t slot = firstFree;
        firstFree = next[slot];
        used[slot] = true;
        ++active;
        if (active > peak) {
            peak = active;
        }
        return blocks[slot].bytes;
    }
    void release(void *buffer) override {
        if (buffer == nullptr) {
            return;
        }
        const uintptr_t base = reinterpret_cast<uintptr_t>(blocks);
        const uintptr_t address = reinterpret_cast<uintptr_t>(buffer);
        if (address < base || address - base >= sizeof(blocks) ||
            (address - base) % sizeof(Block) != 0) {
            return; // Foreign/interior pointers cannot corrupt the free list.
        }
        const uint16_t slot = static_cast<uint16_t>((address - base) / sizeof(Block));
        if (!used[slot]) {
            return; // Reject duplicate release; callers still own lifetime discipline.
        }
        used[slot] = false;
        next[slot] = firstFree;
        firstFree = slot;
        --active;
    }
    uint16_t activeBuffers() const { return active; }
    uint16_t peakBuffers() const { return peak; }
    uint32_t failedAllocations() const { return failures; }
    size_t capacity(const void *buffer) const override {
        const uintptr_t base = reinterpret_cast<uintptr_t>(blocks);
        const uintptr_t address = reinterpret_cast<uintptr_t>(buffer);
        if (address < base || address - base >= sizeof(blocks) ||
            (address - base) % sizeof(Block) != 0) {
            return 0;
        }
        return used[(address - base) / sizeof(Block)] ? Bytes : 0;
    }

  private:
    struct alignas(std::max_align_t) Block {
        uint8_t bytes[Bytes];
    };
    Block blocks[Slots];
    uint16_t next[Slots];
    bool used[Slots] = {};
    uint16_t firstFree = 0, active = 0, peak = 0;
    uint32_t failures = 0;
};

// ArduinoJson allocator adapter. A supplied pool NEVER falls back to the heap on exhaustion.
// No supplied pool preserves the existing heap-backed behavior for ordinary applications.
struct JsonAllocator {
    explicit JsonAllocator(BufferPool *pool = nullptr) : pool(pool) {}
    void *allocate(size_t bytes) { return pool ? pool->allocate(bytes) : std::malloc(bytes); }
    void deallocate(void *buffer) {
        if (pool) {
            pool->release(buffer);
        } else {
            std::free(buffer);
        }
    }
    void *reallocate(void *buffer, size_t bytes) {
        if (pool == nullptr) {
            return std::realloc(buffer, bytes);
        }
        if (buffer == nullptr) {
            return pool->allocate(bytes);
        }
        // ArduinoJson may compact/shrink a document. Keep its slot and address stable.
        // Growing beyond that slot is rejected without losing the caller's old buffer.
        return bytes <= pool->capacity(buffer) ? buffer : nullptr;
    }
    BufferPool *pool;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomMemory
