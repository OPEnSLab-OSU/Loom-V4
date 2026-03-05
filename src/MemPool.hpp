#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef LOOM_MEMPOOL_BLOCK_SIZE
#define LOOM_MEMPOOL_BLOCK_SIZE 128
#endif

#ifndef LOOM_MEMPOOL_BLOCK_COUNT
#define LOOM_MEMPOOL_BLOCK_COUNT 64
#endif

#ifndef LOOM_MEMPOOL_MAX_LEASES
#define LOOM_MEMPOOL_MAX_LEASES 32
#endif

#ifndef LOOM_MEMPOOL_ZERO_ON_ALLOC
#define LOOM_MEMPOOL_ZERO_ON_ALLOC 1
#endif

#ifndef LOOM_MEMPOOL_ZERO_ON_FREE
#define LOOM_MEMPOOL_ZERO_ON_FREE 1
#endif

#ifndef LOOM_SYSTEM_RAM_TOTAL_BYTES
#define LOOM_SYSTEM_RAM_TOTAL_BYTES 32768
#endif

class SDManager;

/**
 * @author Reid Pettibone
 * 
 * MemPool uses deterministic memory chunks known as arenas to prevent heap fragmentation due to frequent allocations.
 * It provides a single source single owner memory manager similar to stack frames. 
 * - Macros are set for 8192B (8KB) by default. 
 * - Memory is reserved for the arena at compile time and will not change. 
 * - MemPool should be used for any allocations that are not explicitly known at compile time. 
 *   If you're unsure or need a large buffer use mempool.
 * - After arena allocation the feather is left with 9607B SRAM
 */

class MemPool {
  public:
    struct Handle {
        static constexpr uint16_t INVALID_SLOT = 0xFFFF;
        uint16_t slot;
        uint16_t generation;

        static Handle invalid() {
            Handle h = {INVALID_SLOT, 0};
            return h;
        }
    };

    struct Stats {
        uint16_t freeBlocks;
        uint16_t usedBlocks;
        uint16_t totalBlocks;
        uint16_t highWaterBlocks; // Peak block allocation
        uint16_t failedAllocs;
        uint16_t activeLeases;
        uint16_t bytesTotal;
        uint16_t bytesUsed;
        uint16_t bytesFree;
        uint16_t systemRamTotalBytes;
        uint16_t systemRamFreeBytes;
    };

    MemPool() { init(); }

    bool init() {
        memset(arena_, 0, sizeof(arena_));
        for (size_t i = 0; i < LOOM_MEMPOOL_BLOCK_COUNT; i++) {
            ownerByBlock_[i] = kFreeBlockOwner;
        }

        for (size_t i = 0; i < LOOM_MEMPOOL_MAX_LEASES; i++) {
            leaseActive_[i] = false;
            leaseGeneration_[i] = 0;
            leaseStart_[i] = 0;
            leaseBlocks_[i] = 0;
            leaseSize_[i] = 0;
        }

        freeBlocks_ = LOOM_MEMPOOL_BLOCK_COUNT;
        usedBlocks_ = 0;
        highWaterBlocks_ = 0;
        failedAllocs_ = 0;
        activeLeases_ = 0;
        return true;
    }

    Handle alloc(size_t bytes) {
        if (bytes == 0) {
            bytes = 1;
        }

        const size_t blocksNeeded = (bytes + LOOM_MEMPOOL_BLOCK_SIZE - 1) / LOOM_MEMPOOL_BLOCK_SIZE;
        if (blocksNeeded > LOOM_MEMPOOL_BLOCK_COUNT) {
            return failAllocation();
        }

        size_t runStart = 0;
        size_t runLength = 0;
        bool found = false;
        
        /* Find a run with contigous blocks */
        for (size_t i = 0; i < LOOM_MEMPOOL_BLOCK_COUNT; i++) {
            if (ownerByBlock_[i] == kFreeBlockOwner) {
                if (runLength == 0) {
                    runStart = i;
                }
                runLength++;
                if (runLength >= blocksNeeded) {
                    found = true;
                    break;
                }
            } else {
                runLength = 0;
            }
        }

        if (!found) {
            return failAllocation();
        }

        const uint16_t leaseSlot = findFreeLeaseSlot();
        if (leaseSlot == Handle::INVALID_SLOT) {
            return failAllocation();
        }

        uint16_t generation = (uint16_t)(leaseGeneration_[leaseSlot] + 1);
        if (generation == 0) {
            generation = 1;
        }

        leaseActive_[leaseSlot] = true;
        leaseGeneration_[leaseSlot] = generation;
        leaseStart_[leaseSlot] = (uint16_t)runStart;
        leaseBlocks_[leaseSlot] = (uint16_t)blocksNeeded;
        leaseSize_[leaseSlot] = bytes;

        for (size_t i = runStart; i < runStart + blocksNeeded; i++) {
            ownerByBlock_[i] = leaseSlot;
        }

        usedBlocks_ = (uint16_t)(usedBlocks_ + blocksNeeded);
        freeBlocks_ = (uint16_t)(LOOM_MEMPOOL_BLOCK_COUNT - usedBlocks_);
        if (usedBlocks_ > highWaterBlocks_) {
            highWaterBlocks_ = usedBlocks_;
        }
        activeLeases_++;

#if LOOM_MEMPOOL_ZERO_ON_ALLOC
        memset(arena_ + (runStart * LOOM_MEMPOOL_BLOCK_SIZE), 0,
               blocksNeeded * LOOM_MEMPOOL_BLOCK_SIZE);
#endif

        Handle h = {leaseSlot, generation};
        return h;
    }

    bool release(Handle h) {
        if (!valid(h)) {
            return false;
        }

        const uint16_t slot = h.slot;
        const uint16_t blockStart = leaseStart_[slot];
        const uint16_t blockCount = leaseBlocks_[slot];

        if (blockCount == 0 || (size_t)blockStart + (size_t)blockCount > LOOM_MEMPOOL_BLOCK_COUNT) {
            return false;
        }

        for (size_t i = blockStart; i < (size_t)blockStart + (size_t)blockCount; i++) {
            if (ownerByBlock_[i] != slot) {
                return false;
            }
        }

#if LOOM_MEMPOOL_ZERO_ON_FREE
        memset(arena_ + (blockStart * LOOM_MEMPOOL_BLOCK_SIZE), 0,
               (size_t)blockCount * LOOM_MEMPOOL_BLOCK_SIZE);
#endif

        for (size_t i = blockStart; i < (size_t)blockStart + (size_t)blockCount; i++) {
            ownerByBlock_[i] = kFreeBlockOwner;
        }

        leaseActive_[slot] = false;
        leaseStart_[slot] = 0;
        leaseBlocks_[slot] = 0;
        leaseSize_[slot] = 0;

        if (activeLeases_ > 0) {
            activeLeases_--;
        }

        if (usedBlocks_ >= blockCount) {
            usedBlocks_ = (uint16_t)(usedBlocks_ - blockCount);
        } else {
            usedBlocks_ = 0;
        }
        freeBlocks_ = (uint16_t)(LOOM_MEMPOOL_BLOCK_COUNT - usedBlocks_);
        return true;
    }

    size_t size(Handle h) const {
        if (!valid(h)) {
            return 0;
        }
        return leaseSize_[h.slot];
    }

    bool write(Handle h, size_t offset, const void *src, size_t len) {
        if (!valid(h)) {
            return false;
        }
        if (len == 0) {
            return true;
        }
        if (src == nullptr) {
            return false;
        }

        const size_t leaseBytes = leaseSize_[h.slot];
        if (offset > leaseBytes || len > (leaseBytes - offset)) {
            return false;
        }

        uint8_t *dest = data(h);
        if (dest == nullptr) {
            return false;
        }

        memcpy(dest + offset, src, len);
        return true;
    }

    bool read(Handle h, size_t offset, void *dst, size_t len) const {
        if (!valid(h)) {
            return false;
        }
        if (len == 0) {
            return true;
        }
        if (dst == nullptr) {
            return false;
        }

        const size_t leaseBytes = leaseSize_[h.slot];
        if (offset > leaseBytes || len > (leaseBytes - offset)) {
            return false;
        }

        const uint8_t *src = data(h);
        if (src == nullptr) {
            return false;
        }

        memcpy(dst, src + offset, len);
        return true;
    }

    bool clear(Handle h, uint8_t value = 0) {
        if (!valid(h)) {
            return false;
        }

        const size_t byteCount = (size_t)leaseBlocks_[h.slot] * LOOM_MEMPOOL_BLOCK_SIZE;
        uint8_t *ptr = data(h);
        if (ptr == nullptr) {
            return false;
        }

        memset(ptr, value, byteCount);
        return true;
    }

    bool valid(Handle h) const {
        if (h.slot >= LOOM_MEMPOOL_MAX_LEASES || h.generation == 0) {
            return false;
        }
        if (!leaseActive_[h.slot]) {
            return false;
        }
        return leaseGeneration_[h.slot] == h.generation;
    }
    
    Stats stats() const {
        Stats s = {};
        s.freeBlocks = freeBlocks_;
        s.usedBlocks = usedBlocks_;
        s.totalBlocks = LOOM_MEMPOOL_BLOCK_COUNT;
        s.highWaterBlocks = highWaterBlocks_;
        s.failedAllocs = failedAllocs_;
        s.activeLeases = activeLeases_;
        s.bytesTotal = (uint16_t)(LOOM_MEMPOOL_BLOCK_SIZE * LOOM_MEMPOOL_BLOCK_COUNT);
        s.bytesUsed = (uint16_t)(usedBlocks_ * LOOM_MEMPOOL_BLOCK_SIZE);
        s.bytesFree = (uint16_t)(freeBlocks_ * LOOM_MEMPOOL_BLOCK_SIZE); 
        s.systemRamTotalBytes = (uint16_t) LOOM_SYSTEM_RAM_TOTAL_BYTES;
        s.systemRamFreeBytes = 0;
        return s;
    }

  private:
    // SDManager is the internal pool owner for SD file/stream paths and needs direct lease pointers.
    friend class SDManager;

    static constexpr uint16_t kFreeBlockOwner = Handle::INVALID_SLOT;

    uint8_t *data(Handle h) {
        if (!valid(h)) {
            return nullptr;
        }
        return arena_ + ((size_t)leaseStart_[h.slot] * LOOM_MEMPOOL_BLOCK_SIZE);
    }

    const uint8_t *data(Handle h) const {
        if (!valid(h)) {
            return nullptr;
        }
        return arena_ + ((size_t)leaseStart_[h.slot] * LOOM_MEMPOOL_BLOCK_SIZE);
    }

    uint16_t findFreeLeaseSlot() const {
        for (uint16_t i = 0; i < LOOM_MEMPOOL_MAX_LEASES; i++) {
            if (!leaseActive_[i]) {
                return i;
            }
        }
        return Handle::INVALID_SLOT;
    }

    Handle failAllocation() {
        failedAllocs_++;
        return Handle::invalid();
    }

    uint8_t arena_[LOOM_MEMPOOL_BLOCK_SIZE * LOOM_MEMPOOL_BLOCK_COUNT];
    uint16_t ownerByBlock_[LOOM_MEMPOOL_BLOCK_COUNT];

    bool leaseActive_[LOOM_MEMPOOL_MAX_LEASES];
    uint16_t leaseGeneration_[LOOM_MEMPOOL_MAX_LEASES];
    uint16_t leaseStart_[LOOM_MEMPOOL_MAX_LEASES];
    uint16_t leaseBlocks_[LOOM_MEMPOOL_MAX_LEASES];
    size_t leaseSize_[LOOM_MEMPOOL_MAX_LEASES];

    uint16_t freeBlocks_;
    uint16_t usedBlocks_;
    uint16_t highWaterBlocks_;
    uint16_t failedAllocs_;
    uint16_t activeLeases_;
};
