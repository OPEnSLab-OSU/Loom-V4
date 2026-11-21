#pragma once

#include "Loom_RF95.h"
#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <RHReliableDatagram.h>
LOOM_EXTERNAL_INCLUDE_END

class Loom_ReliableDatagram : public RHReliableDatagram {
  public:
    explicit Loom_ReliableDatagram(Loom_RF95 &driver, uint8_t address = 0)
        : RHReliableDatagram(driver, address) {}

    void setTimeout(uint16_t timeout);

    // Reuse RadioHead addressing and link ACKs. A matching storage receipt ends the
    // exchange immediately, whether or not the independent link ACK arrived.
    // sendBudgetMs covers one CAD and transmit attempt. All waits use the slot budget.
    bool sendWithReceipt(uint8_t *data, uint8_t length, uint8_t destination,
                         const uint8_t *receipt, uint8_t receiptLength,
                         uint16_t receiptWaitMs, uint32_t slotBudgetMs,
                         uint16_t sendBudgetMs = Loom_RF95::InterruptTimeoutMs);

    uint32_t storageRetransmissions() const { return storageRetries; }

    // Storage protocols deduplicate by record ID after saving to SD. They must see
    // each retry, including the first datagram after a sender reuses an RF sequence.
    bool receiveForStorage(uint8_t *data, uint8_t &length, uint8_t &source, uint8_t &id) {
        uint8_t destination, flags;
        return available() && recvfrom(data, &length, &source, &destination, &id, &flags) &&
               destination == thisAddress() && !(flags & RH_FLAGS_ACK);
    }

    void acknowledgeFrame(uint8_t source, uint8_t id) { acknowledge(id, source); }

  private:
    enum class Reply { Timeout, LinkAck, Stored };
    static constexpr uint8_t MaxReceiptBytes = 10;
    Reply waitForReply(uint8_t destination, const uint8_t *receipt, uint8_t receiptLength,
                       uint16_t timeout, bool acceptLinkAck);

    uint8_t storageSequence = 0;
    uint16_t linkTimeout = RH_DEFAULT_TIMEOUT;
    uint32_t storageRetries = 0;
};
