#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef MEMPOOL_BLOCK_SIZE
#define MEMPOOL_BLOCK_SIZE 64
#endif

#ifndef MEMPOOL_BLOCK_COUNT
#define MEMPOOL_BLOCK_COUNT 128
#endif

#ifndef MEMPOOL_MAX_LEASES
#define MEMPOOL_MAX_LEASES 64
#endif

#ifndef MEMPOOL_ZERO_ON_ALLOC
#define MEMPOOL_ZERO_ON_ALLOC 1
#endif

#ifndef MEMPOOL_ZERO_ON_FREE
#define MEMPOOL_ZERO_ON_FREE 1
#endif

#ifndef SYSTEM_RAM_TOTAL_BYTES
#define SYSTEM_RAM_TOTAL_BYTES 32768
#endif

#ifndef MEMPOOL_TAG_MAX_LEN
#define MEMPOOL_TAG_MAX_LEN 16
#endif

#ifndef MEMPOOL_DUMP_LINE_BYTES
#define MEMPOOL_DUMP_LINE_BYTES 96
#endif

class SDManager;

/**
 * MemPool uses deterministic fixed-size blocks to avoid heap fragmentation.
 * Default config reserves 8KB (64 blocks * 128 bytes) at compile time.
 * 
 * The main reason for using this mempool is to help debug and prevent dynamic 
 * and buffer bounds memory issues.
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
    /**
     * Resource Acquisition Is Initialization (RAII)/RRID for lease lifecycles.
     * Moves the burden of resource management from callers to the pool.
     *
     * **Copy method** is defined but results in nothing. We DO NOT want to copy leases...
     * it can lead to bad things (two leases "own" the same memory).
     *
     * **Copy method**
     */
    class Lease {
      public:
        Lease() : pool_(nullptr), handle_(Handle::invalid()) {}
        Lease(MemPool *pool, Handle handle) : pool_(pool), handle_(handle) {}

        /* The Big-5 */
        ~Lease() { release(); } /* Destructor   */

        Lease(const Lease &) = delete;            /* Disabled copy */
        Lease &operator=(const Lease &) = delete; /* Disabled Copy Assignment Operator */

        Lease(Lease &&other) : pool_(other.pool_), handle_(other.handle_) { /* Move */
            other.pool_ = nullptr;
            other.handle_ = Handle::invalid();
        }

        Lease &operator=(Lease &&other) { /* Move Assignment Operator */
            if (this != &other) {
                release();
                pool_ = other.pool_;
                handle_ = other.handle_;
                other.pool_ = nullptr;
                other.handle_ = Handle::invalid();
            }
            return *this;
        }

        bool valid() const { return pool_ != nullptr && pool_->valid(handle_); }
        explicit operator bool() const {
            return valid();
        } /* Lets leases be used as a bool (valid) */

        bool release() {
            if (!valid()) {
                pool_ = nullptr;
                handle_ = Handle::invalid();
                return false;
            }

            bool released = pool_->release(handle_);
            pool_ = nullptr;
            handle_ = Handle::invalid();
            return released;
        }

        Handle handle() const { return handle_; }
        size_t size() const { return valid() ? pool_->size(handle_) : 0; }
        size_t capacity() const { return valid() ? pool_->capacity(handle_) : 0; }
        char *chars() { return valid() ? pool_->chars(handle_) : nullptr; }
        const char *chars() const { return valid() ? pool_->chars(handle_) : nullptr; }
        uint8_t *bytes() { return valid() ? pool_->bytes(handle_) : nullptr; }
        const uint8_t *bytes() const { return valid() ? pool_->bytes(handle_) : nullptr; }

      private:
        MemPool *pool_;
        Handle handle_;
    };

    /**
     * Mempool failure flags
     */
    enum AllocFailureReason : uint8_t {
        ALLOC_FAIL_NONE = 0,
        ALLOC_FAIL_TOO_LARGE = 1,
        ALLOC_FAIL_NO_CONTIGUOUS_RUN = 2,
        ALLOC_FAIL_NO_LEASE_SLOT = 3
    };

    /* Stats function pointer callbacks*/
    typedef int (*FreeRamProviderFn)();
    typedef void (*LeaseDumpCallback)(const char *line, void *userCtx);

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

        uint32_t allocCalls;
        uint32_t releaseCalls;
        uint32_t writeCalls;
        uint32_t readCalls;
        uint32_t clearCalls;

        uint32_t bytesRequested;
        uint32_t bytesGranted;
        uint32_t bytesReleased;

        uint16_t failedTooLarge;
        uint16_t failedNoContiguousRun;
        uint16_t failedNoLeaseSlot;
        uint16_t failedReleaseInvalid;
        uint16_t failedReleaseCorrupt;

        uint16_t maxContiguousFreeBlocks;
        uint8_t lastAllocFailureReason;
    };

    MemPool() { init(); }

    /**
     * Reset pool arena, ownership maps, and all runtime counters.
     */
    bool init() {
        memset(arena_, 0, sizeof(arena_));
        for (size_t i = 0; i < MEMPOOL_BLOCK_COUNT; i++) {
            ownerByBlock_[i] = kFreeBlockOwner;
        }

        for (size_t i = 0; i < MEMPOOL_MAX_LEASES; i++) {
            leaseActive_[i] = false;
            leaseGeneration_[i] = 0;
            leaseStart_[i] = 0;
            leaseBlocks_[i] = 0;
            leaseSize_[i] = 0;
            leaseTag_[i][0] = '\0';
        }

        freeBlocks_ = MEMPOOL_BLOCK_COUNT;
        usedBlocks_ = 0;
        highWaterBlocks_ = 0;
        failedAllocs_ = 0;
        activeLeases_ = 0;

        allocCalls_ = 0;
        releaseCalls_ = 0;
        writeCalls_ = 0;
        readCalls_ = 0;
        clearCalls_ = 0;

        bytesRequested_ = 0;
        bytesGranted_ = 0;
        bytesReleased_ = 0;

        failedTooLarge_ = 0;
        failedNoContiguousRun_ = 0;
        failedNoLeaseSlot_ = 0;
        failedReleaseInvalid_ = 0;
        failedReleaseCorrupt_ = 0;
        lastAllocFailureReason_ = ALLOC_FAIL_NONE;

        freeRamProvider_ = nullptr;
        return true;
    }

    /**
     * Register a callback used by stats() to report system free RAM.
     */
    void setFreeRamProvider(FreeRamProviderFn provider) { freeRamProvider_ = provider; }

    /**
     * Allocate a lease without a debug tag.
     */
    Handle alloc(size_t bytes) { return alloc(bytes, nullptr); }

    Lease allocLease(size_t bytes, const char *tag = nullptr) {
        return Lease(this, alloc(bytes, tag));
    }

    Lease allocBlock(const char *tag = nullptr) { return allocLease(MEMPOOL_BLOCK_SIZE, tag); }

    /**
     * Allocate a lease of at least `bytes` and optionally label it with `tag`.
     * Allocation uses first-fit contiguous blocks and returns Handle::invalid() on failure.
     */
    Handle alloc(size_t bytes, const char *tag) {
        allocCalls_++;

        // alloc cannot allocate 0 bytes, normalize to 1 byte.
        if (bytes == 0) {
            bytes = 1;
        }
        bytesRequested_ += (uint32_t)bytes;
        lastAllocFailureReason_ = ALLOC_FAIL_NONE;

        const size_t blocksNeeded = (bytes + MEMPOOL_BLOCK_SIZE - 1) / MEMPOOL_BLOCK_SIZE;
        if (blocksNeeded > MEMPOOL_BLOCK_COUNT) {
            return failAllocation(ALLOC_FAIL_TOO_LARGE);
        }

        size_t runStart = 0;
        size_t runLength = 0;
        bool found = false;

        // Find a contiguous free run.
        for (size_t i = 0; i < MEMPOOL_BLOCK_COUNT; i++) {
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
            return failAllocation(ALLOC_FAIL_NO_CONTIGUOUS_RUN);
        }

        const uint16_t leaseSlot = findFreeLeaseSlot();
        if (leaseSlot == Handle::INVALID_SLOT) {
            return failAllocation(ALLOC_FAIL_NO_LEASE_SLOT);
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
        setLeaseTag(leaseSlot, tag);

        for (size_t i = runStart; i < runStart + blocksNeeded; i++) {
            ownerByBlock_[i] = leaseSlot;
        }

        usedBlocks_ = (uint16_t)(usedBlocks_ + blocksNeeded);
        freeBlocks_ = (uint16_t)(MEMPOOL_BLOCK_COUNT - usedBlocks_);
        if (usedBlocks_ > highWaterBlocks_) {
            highWaterBlocks_ = usedBlocks_;
        }
        activeLeases_++;
        bytesGranted_ += (uint32_t)(blocksNeeded * MEMPOOL_BLOCK_SIZE);

#if MEMPOOL_ZERO_ON_ALLOC
        memset(arena_ + (runStart * MEMPOOL_BLOCK_SIZE), 0, blocksNeeded * MEMPOOL_BLOCK_SIZE);
#endif

        Handle h = {leaseSlot, generation};
        return h;
    }

    /**
     * Release a valid lease handle back to the pool.
     * Returns false if handle is invalid or ownership metadata is inconsistent.
     */
    bool release(Handle h) {
        releaseCalls_++;

        if (!valid(h)) {
            failedReleaseInvalid_++;
            return false;
        }

        const uint16_t slot = h.slot;
        const uint16_t blockStart = leaseStart_[slot];
        const uint16_t blockCount = leaseBlocks_[slot];

        if (blockCount == 0 || (size_t)blockStart + (size_t)blockCount > MEMPOOL_BLOCK_COUNT) {
            failedReleaseCorrupt_++;
            return false;
        }

        for (size_t i = blockStart; i < (size_t)blockStart + (size_t)blockCount; i++) {
            if (ownerByBlock_[i] != slot) {
                failedReleaseCorrupt_++;
                return false;
            }
        }

#if MEMPOOL_ZERO_ON_FREE
        memset(arena_ + (blockStart * MEMPOOL_BLOCK_SIZE), 0,
               (size_t)blockCount * MEMPOOL_BLOCK_SIZE);
#endif

        for (size_t i = blockStart; i < (size_t)blockStart + (size_t)blockCount; i++) {
            ownerByBlock_[i] = kFreeBlockOwner;
        }

        leaseActive_[slot] = false;
        leaseStart_[slot] = 0;
        leaseBlocks_[slot] = 0;
        leaseSize_[slot] = 0;
        leaseTag_[slot][0] = '\0';

        if (activeLeases_ > 0) {
            activeLeases_--;
        }

        if (usedBlocks_ >= blockCount) {
            usedBlocks_ = (uint16_t)(usedBlocks_ - blockCount);
        } else {
            usedBlocks_ = 0;
        }
        freeBlocks_ = (uint16_t)(MEMPOOL_BLOCK_COUNT - usedBlocks_);
        bytesReleased_ += (uint32_t)(blockCount * MEMPOOL_BLOCK_SIZE);
        return true;
    }

    /**
     * Get the logical byte size requested for a lease.
     */
    size_t size(Handle h) const {
        if (!valid(h)) {
            return 0;
        }
        return leaseSize_[h.slot];
    }

    /**
     * Get the physical byte capacity reserved for a lease.
     */
    size_t capacity(Handle h) const {
        if (!valid(h)) {
            return 0;
        }
        return (size_t)leaseBlocks_[h.slot] * MEMPOOL_BLOCK_SIZE;
    }

    /**
     * Get the debug tag associated with a lease, if present.
     */
    const char *tag(Handle h) const {
        if (!valid(h)) {
            return nullptr;
        }
        return leaseTag_[h.slot];
    }

    /**
     * Copy len bytes from src into lease memory at offset.
     */
    bool write(Handle h, size_t offset, const void *src, size_t len) {
        writeCalls_++;

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

    /**
     * Copy len bytes from lease memory at offset into dst.
     */
    bool read(Handle h, size_t offset, void *dst, size_t len) {
        readCalls_++;

        const uint8_t *src = data(h);
        if (src == nullptr) {
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

        memcpy(dst, src + offset, len);
        return true;
    }

    /**
     * Fill all blocks owned by a lease with a single byte value.
     */
    bool clear(Handle h, uint8_t value = 0) {
        clearCalls_++;

        uint8_t *dst = data(h);
        if (dst == nullptr) {
            return false;
        }
        const size_t byteCount = (size_t)leaseBlocks_[h.slot] * MEMPOOL_BLOCK_SIZE;

        memset(dst, value, byteCount);
        return true;
    }

    /**
     * Returns true only when handle slot is active and generation matches.
     */
    bool valid(Handle h) const {
        if (h.slot >= MEMPOOL_MAX_LEASES || h.generation == 0) {
            return false;
        }
        if (!leaseActive_[h.slot]) {
            return false;
        }
        return leaseGeneration_[h.slot] == h.generation;
    }

    /**
     * Return the active lease handle whose allocation starts at ptr.
     * This is primarily for allocator-style integrations that receive only a raw pointer
     * during deallocation.
     */
    Handle handleFromPointer(const void *ptr) const {
        if (ptr == nullptr) {
            return Handle::invalid();
        }

        const uint8_t *bytePtr = static_cast<const uint8_t *>(ptr);
        const uint8_t *arenaStart = arena_;
        const uint8_t *arenaEnd = arena_ + sizeof(arena_);
        if (bytePtr < arenaStart || bytePtr >= arenaEnd) {
            return Handle::invalid();
        }

        const size_t offset = (size_t)(bytePtr - arenaStart);
        if ((offset % MEMPOOL_BLOCK_SIZE) != 0) {
            return Handle::invalid();
        }

        const uint16_t block = (uint16_t)(offset / MEMPOOL_BLOCK_SIZE);
        const uint16_t slot = ownerByBlock_[block];
        if (slot >= MEMPOOL_MAX_LEASES || leaseStart_[slot] != block) {
            return Handle::invalid();
        }

        Handle h = {slot, leaseGeneration_[slot]};
        return valid(h) ? h : Handle::invalid();
    }

    bool releasePointer(void *ptr) { return release(handleFromPointer(ptr)); }

    /**
     * DANGEROUS: chars() and bytes() return borrowed raw pointers to data within the pool.
     * These pointers allow callers to access any memory position in the pool.
     * You must ensure you do not go beyond the handles lease when using these.
     * Otherwise you risk going out-of-bounds and causing issues.
     * Borrowed pointers must not exceed leases lifetime.
     *
     * Primily used for snprintf and json serialize/deserialize
     */
    uint8_t *bytes(Handle h) { return data(h); }
    const uint8_t *bytes(Handle h) const { return data(h); }

    /**
     * Cast for uint8 leases that need to be char buffers.
     */
    char *chars(Handle h) { return reinterpret_cast<char *>(bytes(h)); }
    const char *chars(Handle h) const { return reinterpret_cast<const char *>(bytes(h)); }

    /**
     * Iterate active leases and emit one formatted line per lease through callback.
     * Returns number of active leases.
     */
    size_t dumpActiveLeases(LeaseDumpCallback cb = nullptr, void *userCtx = nullptr) const {
        size_t count = 0;
        for (uint16_t slot = 0; slot < MEMPOOL_MAX_LEASES; slot++) {
            if (!leaseActive_[slot]) {
                continue;
            }
            count++;
            if (cb != nullptr) {
                char line[MEMPOOL_DUMP_LINE_BYTES];
                snprintf(line, sizeof(line), "slot=%u gen=%u start=%u blocks=%u size=%u tag=%s",
                         (unsigned int)slot, (unsigned int)leaseGeneration_[slot],
                         (unsigned int)leaseStart_[slot], (unsigned int)leaseBlocks_[slot],
                         (unsigned int)leaseSize_[slot],
                         leaseTag_[slot][0] != '\0' ? leaseTag_[slot] : "-");
                cb(line, userCtx);
            }
        }
        return count;
    }

    /**
     * Snapshot pool/system stats and runtime counters.
     */
    Stats stats() const {
        Stats s = {};
        s.freeBlocks = freeBlocks_;
        s.usedBlocks = usedBlocks_;
        s.totalBlocks = MEMPOOL_BLOCK_COUNT;
        s.highWaterBlocks = highWaterBlocks_;
        s.failedAllocs = failedAllocs_;
        s.activeLeases = activeLeases_;
        s.bytesTotal = (uint16_t)(MEMPOOL_BLOCK_SIZE * MEMPOOL_BLOCK_COUNT);
        s.bytesUsed = (uint16_t)(usedBlocks_ * MEMPOOL_BLOCK_SIZE);
        s.bytesFree = (uint16_t)(freeBlocks_ * MEMPOOL_BLOCK_SIZE);
        s.systemRamTotalBytes = (uint16_t)SYSTEM_RAM_TOTAL_BYTES;
        s.systemRamFreeBytes = readSystemFreeRam();

        s.allocCalls = allocCalls_;
        s.releaseCalls = releaseCalls_;
        s.writeCalls = writeCalls_;
        s.readCalls = readCalls_;
        s.clearCalls = clearCalls_;

        s.bytesRequested = bytesRequested_;
        s.bytesGranted = bytesGranted_;
        s.bytesReleased = bytesReleased_;

        s.failedTooLarge = failedTooLarge_;
        s.failedNoContiguousRun = failedNoContiguousRun_;
        s.failedNoLeaseSlot = failedNoLeaseSlot_;
        s.failedReleaseInvalid = failedReleaseInvalid_;
        s.failedReleaseCorrupt = failedReleaseCorrupt_;

        s.maxContiguousFreeBlocks = maxContiguousFreeBlocks();
        s.lastAllocFailureReason = lastAllocFailureReason_;
        return s;
    }

  private:
    friend class SDManager;

    static constexpr uint16_t kFreeBlockOwner = Handle::INVALID_SLOT;

    /**
     * Internal read-write pointer to lease start.
     */
    uint8_t *data(Handle h) {
        if (!valid(h)) {
            return nullptr;
        }
        return arena_ + ((size_t)leaseStart_[h.slot] * MEMPOOL_BLOCK_SIZE);
    }

    /**
     * Internal read-only pointer to lease start.
     */
    const uint8_t *data(Handle h) const {
        if (!valid(h)) {
            return nullptr;
        }
        return arena_ + ((size_t)leaseStart_[h.slot] * MEMPOOL_BLOCK_SIZE);
    }

    /**
     * Internal helper for locating the first available lease slot.
     */
    uint16_t findFreeLeaseSlot() const {
        for (uint16_t i = 0; i < MEMPOOL_MAX_LEASES; i++) {
            if (!leaseActive_[i]) {
                return i;
            }
        }
        return Handle::INVALID_SLOT;
    }

    /**
     * Internal helper to safely set a fixed-size lease tag.
     */
    void setLeaseTag(uint16_t slot, const char *tag) {
        if (slot >= MEMPOOL_MAX_LEASES) {
            return;
        }

        copyCString(leaseTag_[slot], MEMPOOL_TAG_MAX_LEN, tag);
    }

    /**
     * strlcpy is supported in the Loom core and should replace all strcpy and strncpy.
     * anyways this function is a null-ptr safe strcpy.
     */
    static size_t copyCString(char *dst, size_t dstSize, const char *src) {
        if (dstSize == 0) {
            return src != nullptr ? strlen(src) : 0;
        }

        if (src == nullptr) {
            dst[0] = '\0';
            return 0;
        }

        return strlcpy(dst, src, dstSize);
    }

    /**
     *  Allocation failure tracking and invalid-handle return.
     */
    Handle failAllocation(AllocFailureReason reason) {
        failedAllocs_++;
        lastAllocFailureReason_ = (uint8_t)reason;
        switch (reason) {
        case ALLOC_FAIL_TOO_LARGE:
            failedTooLarge_++;
            break;
        case ALLOC_FAIL_NO_CONTIGUOUS_RUN:
            failedNoContiguousRun_++;
            break;
        case ALLOC_FAIL_NO_LEASE_SLOT:
            failedNoLeaseSlot_++;
            break;
        case ALLOC_FAIL_NONE:
        default:
            break;
        }
        return Handle::invalid();
    }

    /**
     * Compute largest contiguous free run in blocks.
     */
    uint16_t maxContiguousFreeBlocks() const {
        uint16_t best = 0;
        uint16_t run = 0;
        for (size_t i = 0; i < MEMPOOL_BLOCK_COUNT; i++) {
            if (ownerByBlock_[i] == kFreeBlockOwner) {
                run++;
                if (run > best) {
                    best = run;
                }
            } else {
                run = 0;
            }
        }
        return best;
    }

    /**
     * Read system free RAM via registered callback, clamped to uint16_t.
     */
    uint16_t readSystemFreeRam() const {
        if (freeRamProvider_ == nullptr) {
            return 0;
        }
        int freeRam = freeRamProvider_();
        if (freeRam < 0) {
            freeRam = 0;
        }
        if (freeRam > 0xFFFF) {
            freeRam = 0xFFFF;
        }
        return (uint16_t)freeRam;
    }

    alignas(max_align_t) uint8_t arena_[MEMPOOL_BLOCK_SIZE * MEMPOOL_BLOCK_COUNT];
    uint16_t ownerByBlock_[MEMPOOL_BLOCK_COUNT];

    bool leaseActive_[MEMPOOL_MAX_LEASES];
    uint16_t leaseGeneration_[MEMPOOL_MAX_LEASES];
    uint16_t leaseStart_[MEMPOOL_MAX_LEASES];
    uint16_t leaseBlocks_[MEMPOOL_MAX_LEASES];
    size_t leaseSize_[MEMPOOL_MAX_LEASES];
    char leaseTag_[MEMPOOL_MAX_LEASES][MEMPOOL_TAG_MAX_LEN];

    uint16_t freeBlocks_;
    uint16_t usedBlocks_;
    uint16_t highWaterBlocks_;
    uint16_t failedAllocs_;
    uint16_t activeLeases_;

    uint32_t allocCalls_;
    uint32_t releaseCalls_;
    uint32_t writeCalls_;
    uint32_t readCalls_;
    uint32_t clearCalls_;

    uint32_t bytesRequested_;
    uint32_t bytesGranted_;
    uint32_t bytesReleased_;

    uint16_t failedTooLarge_;
    uint16_t failedNoContiguousRun_;
    uint16_t failedNoLeaseSlot_;
    uint16_t failedReleaseInvalid_;
    uint16_t failedReleaseCorrupt_;
    uint8_t lastAllocFailureReason_;

    FreeRamProviderFn freeRamProvider_;
};
