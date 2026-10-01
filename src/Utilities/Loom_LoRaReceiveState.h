#pragma once

#include <stdint.h>

// Protocol bookkeeping only: no radio, JSON buffer, allocation, or Arduino dependency.
namespace loomLoRa {
class BatchReceiveState {
  public:
    void start(uint8_t sender, uint8_t count, uint32_t now) {
        // A header describes one batch. Repeated headers replace the estimate rather than
        // adding another copy of the same batch to the hub's outstanding count.
        senderAddress = sender;
        outstanding = count;
        lastProgress = now;
    }

    void complete(uint8_t sender, uint32_t now) {
        // Traffic from another node must not acknowledge this node's missing records.
        if (outstanding > 0 && sender == senderAddress) {
            --outstanding;
            lastProgress = now;
        }
    }

    bool expire(uint32_t now, uint32_t idleLimit) {
        if (outstanding > 0 && static_cast<uint32_t>(now - lastProgress) >= idleLimit) {
            cancel();
            return true;
        }
        return false;
    }

    void cancel() { outstanding = 0; }
    uint8_t remaining() const { return outstanding; }

  private:
    uint32_t lastProgress = 0;
    uint8_t senderAddress = 0;
    uint8_t outstanding = 0;
};
} // namespace loomLoRa
