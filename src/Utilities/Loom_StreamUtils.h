#pragma once
#include <stddef.h>
#include <stdint.h>

namespace loomStream {
////////////////////////////////////////////////////////////////////////////////////////////////////
// A debug bridge must give control back even when bytes arrive continuously. Each direction
// gets a fixed budget, and a busy destination leaves the unforwarded byte in the source queue.
// The caller is the only reader of source; an interrupt may append bytes but must not consume them.
template <typename Input, typename Output>
size_t forwardAvailable(Input &source, Output &destination, size_t byteBudget) {
    size_t forwarded = 0;
    while (forwarded < byteBudget && source.available() > 0) {
        const int next = source.peek();
        if (next < 0 || destination.write(static_cast<uint8_t>(next)) != 1) {
            break;
        }
        ++forwarded;
        if (source.read() != next) {
            break; // Unexpected queue change: stop this pass rather than reading more blindly.
        }
    }
    return forwarded;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomStream
