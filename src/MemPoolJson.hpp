#pragma once

#include "MemPool.hpp"
#include <ArduinoJson.h>
#include <string.h>

struct MemPoolJsonAllocator {
    MemPool *pool;

    explicit MemPoolJsonAllocator(MemPool *memPool = nullptr) : pool(memPool) {}

    void *allocate(size_t size) {
        if (pool == nullptr || size == 0) {
            return nullptr;
        }

        MemPool::Handle handle = pool->alloc(size, "json_doc");
        return pool->bytes(handle);
    }

    void deallocate(void *ptr) {
        if (pool == nullptr || ptr == nullptr) {
            return;
        }

        pool->releasePointer(ptr);
    }

    void *reallocate(void *ptr, size_t new_size) {
        if (pool == nullptr) {
            return nullptr;
        }

        if (ptr == nullptr) {
            return allocate(new_size);
        }

        if (new_size == 0) {
            deallocate(ptr);
            return nullptr;
        }

        MemPool::Handle oldHandle = pool->handleFromPointer(ptr);
        if (!pool->valid(oldHandle)) {
            return nullptr;
        }

        const size_t oldSize = pool->size(oldHandle);
        const size_t oldCapacity = pool->capacity(oldHandle);
        if (new_size <= oldCapacity) {
            return pool->resize(oldHandle, new_size) ? ptr : nullptr;
        }

        MemPool::Handle newHandle = pool->alloc(new_size, "json_doc");
        uint8_t *newPtr = pool->bytes(newHandle);
        if (newPtr == nullptr) {
            return nullptr;
        }

        memcpy(newPtr, ptr, oldSize < new_size ? oldSize : new_size);
        pool->release(oldHandle);
        return newPtr;
    }
};

// Set the basicjsondoc to allocate from MemPoolJsonAllocator rather than TAllocator
using LoomJsonDocument = BasicJsonDocument<MemPoolJsonAllocator>;
