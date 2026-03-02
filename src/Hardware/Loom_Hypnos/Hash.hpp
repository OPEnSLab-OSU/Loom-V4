#pragma once

#include <string.h>
#include <stdint.h>


// Lightweight FNV-1a 64-bit (tableless)
static inline uint64_t fnv1a64_init() {
    return 14695981039346656037ULL; // offset basis
}

static inline uint64_t fnv1a64_update(uint64_t h, const uint8_t* data, size_t len) {
    const uint64_t prime = 1099511628211ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)data[i];
        h *= prime;
    }
    return h;
}

static inline uint64_t fnv1a64_update_cstr(uint64_t h, const char* s) {
    if (!s) return fnv1a64_update(h, (const uint8_t*)"", 0);
    return fnv1a64_update(h, (const uint8_t*)s, strlen(s));
}
