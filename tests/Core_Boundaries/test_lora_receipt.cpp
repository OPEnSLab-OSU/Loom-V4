#include <cassert>
#include "Radio/Loom_LoRa/Loom_ReliableDatagram.cpp"

uint32_t now = 0;
constexpr uint8_t Node = 0x21, Hub = 0x20;
const uint8_t Receipt[] = {0xb7, 0x52, 1, 2, 3, 4};
const std::vector<uint8_t> Bytes(Receipt, Receipt + sizeof(Receipt));

struct Exchange {
    Loom_RF95 radio;
    Loom_ReliableDatagram link{radio, Node};

    explicit Exchange(uint32_t clock = 0) {
        now = clock;
        link.setTimeout(10);
    }

    bool send(uint32_t budget = 40, uint16_t sendBudget = 3) {
        uint8_t message[] = {0xb7, 0x51, 1, 2, 3, 4, 0xff};
        return link.sendWithReceipt(message, sizeof(message), Hub, Receipt, sizeof(Receipt), 15,
                                    budget, sendBudget);
    }
};

int main() {
    // A receipt alone proves storage and cancels the remaining link-ACK retries.
    {
        Exchange test;
        test.radio.incoming = {{5, Hub, Node, 0, Bytes}};
        assert(test.send() && now == 5 && test.radio.sent.size() == 1);
        assert(test.link.storageRetransmissions() == 0);
    }
    {
        Exchange test;
        test.radio.incoming = {{1, Hub, Node, RH_FLAGS_ACK, {'!'}, 1},
                               {15, Hub, Node, 0, Bytes}};
        assert(test.send() && now == 15 && test.radio.sent.size() == 1);
    }
    {
        Exchange test;
        test.radio.incoming = {{10, Hub, Node, 0, Bytes}};
        assert(test.send() && now == 10 && test.radio.sent.size() == 1);
    }

    // A stale link sequence cannot suppress a retry. Retries preserve the sequence.
    {
        Exchange test;
        test.radio.incoming = {{2, Hub, Node, RH_FLAGS_ACK, {'!'}, 0},
                               {13, Hub, Node, 0, Bytes}};
        assert(test.send() && now == 13 && test.radio.sent.size() == 2);
        const auto first = test.radio.sent[0], second = test.radio.sent[1];
        assert(first.flags == 0 && second.flags == RH_FLAGS_RETRY && first.id == second.id);
        assert(first.to == Hub && second.to == Hub && second.startedAt == 10);
        assert(test.link.storageRetransmissions() == 1);
    }

    // A link ACK never confirms storage. Reject other nodes, data, and overlong frames.
    {
        Exchange test;
        test.radio.incoming = {{1, Hub, Node, RH_FLAGS_ACK, Bytes, 1},
                               {2, Hub + 1, Node, 0, Bytes},
                               {3, Hub, Node + 1, 0, Bytes},
                               {4, Hub, Node, 0, {0xb7, 0x52, 1, 2, 3, 9}},
                               {5, Hub, Node, 0, {0xb7, 0x52, 1, 2, 3, 4, 5}}};
        assert(!test.send() && now == 16 && test.radio.sent.size() == 1);
    }

    // The deadline caps every wait. No retry may start without room for its TX.
    {
        Exchange test;
        test.radio.incoming = {{13, Hub, Node, 0, Bytes}};
        assert(!test.send(13, 4) && now == 13 && test.radio.sent.size() == 1);
        assert(test.radio.recv(nullptr, nullptr)); // The next caller can still read this frame.
    }
    {
        Exchange test;
        assert(!test.send(2) && now == 0 && test.radio.sent.empty());
    }
    {
        Exchange test;
        test.radio.txDelay = 80;
        assert(!test.send(6, 5) && now == 6 && test.radio.sent.size() == 1);
    }
    {
        Exchange test(UINT32_MAX - 2);
        test.radio.incoming = {{1, Hub, Node, 0, Bytes}};
        assert(test.send(7) && now == 1 && test.radio.sent.size() == 1);
    }
}
