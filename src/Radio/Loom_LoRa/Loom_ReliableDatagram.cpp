#include "Loom_ReliableDatagram.h"
#include <cstring>

void Loom_ReliableDatagram::setTimeout(uint16_t timeout) {
    RHReliableDatagram::setTimeout(timeout);
    linkTimeout = timeout > UINT16_MAX / 2 ? UINT16_MAX / 2 : timeout;
}

Loom_ReliableDatagram::Reply Loom_ReliableDatagram::waitForReply(
    uint8_t destination, const uint8_t *receipt, uint8_t receiptLength, uint16_t timeout,
    bool acceptLinkAck) {
    const uint32_t start = millis();
    while (true) {
        const uint32_t elapsed = millis() - start;
        const bool ready = timeout == 0 ? available()
                         : elapsed < timeout &&
                               waitAvailableTimeout(static_cast<uint16_t>(timeout - elapsed));
        if (!ready) {
            return Reply::Timeout;
        }
        uint8_t frame[MaxReceiptBytes + 1];
        uint8_t size = sizeof(frame), from, to, id, flags;
        if (!recvfrom(frame, &size, &from, &to, &id, &flags) ||
            from != destination || to != thisAddress()) {
            continue;
        }
        if (flags & RH_FLAGS_ACK) {
            if (acceptLinkAck && id == storageSequence) {
                return Reply::LinkAck;
            }
        } else if (size == receiptLength && memcmp(frame, receipt, size) == 0) {
            return Reply::Stored;
        }
    }
}

bool Loom_ReliableDatagram::sendWithReceipt(uint8_t *data, uint8_t length, uint8_t destination,
                                           const uint8_t *receipt, uint8_t receiptLength,
                                           uint16_t receiptWaitMs, uint32_t slotBudgetMs,
                                           uint16_t sendBudgetMs) {
    if (!receipt || receiptLength == 0 || receiptLength > MaxReceiptBytes ||
        destination == RH_BROADCAST_ADDRESS || sendBudgetMs == 0) {
        return false;
    }

    const uint32_t start = millis();
    const auto remaining = [start, slotBudgetMs]() -> uint32_t {
        const uint32_t elapsed = millis() - start;
        return elapsed < slotBudgetMs ? slotBudgetMs - elapsed : 0;
    };
    const auto wait = [&](uint16_t limit, bool acceptLinkAck) {
        const uint32_t left = remaining();
        if (left == 0) {
            return Reply::Timeout;
        }
        return waitForReply(destination, receipt, receiptLength,
                            static_cast<uint16_t>(left < limit ? left : limit), acceptLinkAck);
    };

    ++storageSequence;
    bool sent = false;
    for (uint16_t attempt = 0; attempt <= retries() && remaining() >= sendBudgetMs; ++attempt) {
        if (sent) {
            // A reply can become ready as the ACK wait expires. Read it before starting TX.
            const Reply pending = wait(0, true);
            if (pending == Reply::Stored) {
                return true;
            }
            if (pending == Reply::LinkAck) {
                break;
            }
        }
        if (remaining() < sendBudgetMs) {
            break;
        }
        setHeaderId(storageSequence);
        setHeaderFlags(attempt ? RH_FLAGS_RETRY : 0, RH_FLAGS_ACK | RH_FLAGS_RETRY);
        if (sendto(data, length, destination)) {
            if (sent) {
                ++storageRetries;
            }
            sent = true;
            const uint32_t left = remaining();
            waitPacketSent(static_cast<uint16_t>(left < sendBudgetMs ? left : sendBudgetMs));
        }

        // RadioHead adds a random delay of up to one ACK timeout before retrying.
        const uint16_t jitter = static_cast<uint16_t>((uint32_t(linkTimeout) * random(0, 256)) / 256);
        const Reply reply = wait(linkTimeout + jitter, true);
        if (sent && reply == Reply::Stored) {
            return true;
        }
        if (sent && reply == Reply::LinkAck) {
            break;
        }
    }
    return sent && wait(receiptWaitMs, false) == Reply::Stored;
}
