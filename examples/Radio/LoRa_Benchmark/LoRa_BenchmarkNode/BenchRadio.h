#pragma once

#include <Radio/Loom_LoRa/Loom_LoRa.h>

namespace bench {

constexpr uint8_t NetworkId = 0xB7;
constexpr uint8_t HubAddress = 0;
constexpr uint8_t MaxNodes = 16;
constexpr uint8_t MaxFragments = 8;
// All frames start with network, kind and a little-endian round number.
// Data then carries fragment index/count. Beacons carry nodes, chunk, count and lead time.
constexpr uint8_t Beacon = 0x11, MsgPack = 0x12, Json = 0x13;
constexpr uint8_t HeaderBytes = 6, BeaconBytes = 9;
constexpr uint8_t MaxChunk = RH_RF95_MAX_MESSAGE_LEN - HeaderBytes;
constexpr uint16_t GuardMs = 12, FrameMarginMs = 10;
constexpr float FrequencyMHz = RF95_FREQ;
constexpr int8_t TxPowerDbm = 5;
constexpr bool WideBandwidth = false; // Match on every radio. SF7, CR4/5 in both profiles.

struct Schedule {
    uint8_t nodes;
    uint8_t chunk;
    uint8_t fragments;
    uint16_t leadMs;

    constexpr bool valid() const {
        return nodes >= 1 && nodes <= MaxNodes && chunk >= 1 && chunk <= MaxChunk &&
               fragments >= 1 && fragments <= MaxFragments && leadMs >= GuardMs;
    }
};

inline uint16_t read16(const uint8_t *p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

inline void write16(uint8_t *p, uint16_t value) {
    p[0] = uint8_t(value);
    p[1] = uint8_t(value >> 8);
}

inline uint16_t airtimeMs(uint8_t messageBytes) {
    // SF7, CR4/5, explicit header, CRC, 8-symbol preamble, no low-data-rate optimization.
    const uint16_t phyBytes = messageBytes + RH_RF95_HEADER_LEN;
    const uint16_t blocks = (8 * phyBytes + 16 + 27) / 28;
    const uint32_t quarterSymbols = 81 + 20 * blocks;
    const uint16_t symbolUs = WideBandwidth ? 256 : 1024;
    return (quarterSymbols * symbolUs + 3999) / 4000;
}

inline uint32_t slotMs(const Schedule &s) {
    return 2 * GuardMs + s.fragments * (airtimeMs(HeaderBytes + s.chunk) + FrameMarginMs);
}

inline uint32_t slotStartMs(const Schedule &s, uint8_t node) {
    return s.leadMs + (node - 1) * slotMs(s);
}

inline bool beginRadio(RH_RF95 &radio, uint8_t address) {
    pinMode(RFM95_CS, INPUT_PULLUP);
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);

    if (!radio.init() || !radio.setFrequency(FrequencyMHz) ||
        !radio.setModemConfig(WideBandwidth ? RH_RF95::Bw500Cr45Sf128 : RH_RF95::Bw125Cr45Sf128)) {
        return false;
    }
    radio.setPreambleLength(8);
    radio.setTxPower(TxPowerDbm, false);
    radio.setCADTimeout(0); // Hub slots own transmit timing.
    radio.setThisAddress(address);
    radio.setHeaderFrom(address);
    radio.setHeaderTo(address == HubAddress ? RH_BROADCAST_ADDRESS : HubAddress);
    return true;
}

inline bool sendFrame(RH_RF95 &radio, const uint8_t *frame, uint8_t len) {
    if (radio.send(frame, len) && radio.waitPacketSent(airtimeMs(len) + FrameMarginMs)) {
        return true;
    }
    radio.setModeIdle();
    return false;
}

} // namespace bench
