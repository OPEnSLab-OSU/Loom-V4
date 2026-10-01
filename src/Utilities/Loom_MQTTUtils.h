#pragma once

#include <stddef.h>
#include <string.h>

namespace loomMQTT {
// Publish topics cannot contain subscription wildcards. Controls also make routing/logs ambiguous.
inline bool validTopic(const char *text, size_t capacity, bool component = false) {
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i < capacity; ++i) {
        const unsigned char value = static_cast<unsigned char>(text[i]);
        if (value == 0) {
            return true;
        }
        if (value < 0x20 || value == 0x7f || value == '+' || value == '#' ||
            (component && value == '/')) {
            return false;
        }
    }
    return false; // Never truncate a topic or a routing component into a different destination.
}
} // namespace loomMQTT
