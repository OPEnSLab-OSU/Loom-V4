#pragma once

#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include "BenchEnrollment.h"
#include "BenchRetry.h"
#include "BenchCadence.h"

namespace bench {

constexpr uint8_t NetworkId = 0xB7;
constexpr uint8_t MaxHubs = 16;
constexpr uint8_t HubAddressBase = 0x80; // Hub IDs occupy a separate range from node IDs.
constexpr uint8_t MaxFragments = 8;
// All frames start with network and kind. Bench frames then carry a round number.
// Offers need only the prefix. Ping and pong append an echoed 16-bit token.
constexpr uint8_t Beacon = 0x31, MsgPack = 0x32, Json = 0x33, Offer = 0x34, Ping = 0x35, Pong = 0x36;
constexpr uint8_t JoinOffer = 0x37, JoinRequest = 0x38, JoinGrant = 0x39, EnrolledBeacon = 0x3a;
constexpr uint8_t RetryRequest = 0x3b;
constexpr uint8_t PacedBeacon = 0x3c, PacedEnrolledBeacon = 0x3d, PacedControl = 0x3e;
constexpr uint8_t SensorSample = 0, SyntheticSample = 1, ProbeSample = 2;
constexpr uint8_t HeaderBytes = 6, BeaconBytes = 13;
constexpr uint8_t OfferBytes = 2, PingBytes = 4;
constexpr uint8_t JoinAddress = 0;
constexpr uint8_t JoinOfferBytes = 4, JoinFrameBytes = JoinOfferBytes + 1 + BoardKeyBytes;
constexpr uint8_t PacedQuietOffset = 4 + SlotMaskBytes, PacedControlBytes = PacedQuietOffset + 2;
constexpr uint8_t MaxBeaconBytes = BeaconBytes + SlotMaskBytes + MaxNodes * BoardKeyBytes;
static_assert(MaxBeaconBytes <= RH_RF95_MAX_MESSAGE_LEN, "Enrollment roster must fit in one beacon");
static_assert(MaxNodes < 32 && MaxFragments <= 8, "Retry masks must cover all configured slots");
constexpr uint16_t PingTurnaroundMs = 20;
constexpr uint8_t MaxChunk = RH_RF95_MAX_MESSAGE_LEN - HeaderBytes;
constexpr uint16_t GuardMs = 12, FrameMarginMs = 10;
constexpr uint16_t MaxPadding = 1800;
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

constexpr uint8_t hubAddress(uint8_t id) {
    return HubAddressBase + id - 1;
}

constexpr uint8_t hubIdFromAddress(uint8_t address) {
    return address >= HubAddressBase && address < HubAddressBase + MaxHubs
        ? address - HubAddressBase + 1 : 0;
}

inline uint16_t read16(const uint8_t *p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

inline void write16(uint8_t *p, uint16_t value) {
    p[0] = uint8_t(value);
    p[1] = uint8_t(value >> 8);
}

inline bool parseNumber(const char *text, uint32_t minValue, uint32_t maxValue, uint32_t &value) {
    if (!text || !*text) {
        return false;
    }
    uint32_t number = 0;
    while (*text) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        const uint8_t digit = *text++ - '0';
        if (number > maxValue / 10 || (number == maxValue / 10 && digit > maxValue % 10)) {
            return false;
        }
        number = 10 * number + digit;
    }
    if (number < minValue) {
        return false;
    }
    value = number;
    return true;
}

struct CommandLine {
    char buffer[64];
    size_t length = 0;
    bool overflow = false;

    char *read() {
        while (Serial.available() > 0) {
            const char c = static_cast<char>(Serial.read());
            if (c == '\n' || c == '\r') {
                if (overflow) {
                    Serial.println(F("ERR command too long"));
                    overflow = false;
                    length = 0;
                } else if (length > 0) {
                    buffer[length] = '\0';
                    length = 0;
                    return buffer;
                }
            } else if (length < sizeof(buffer) - 1) {
                buffer[length++] = c;
            } else {
                overflow = true;
            }
        }
        return nullptr;
    }
};

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

inline uint32_t joinSlotMs() {
    return 2 * (airtimeMs(JoinFrameBytes) + GuardMs + FrameMarginMs) + PingTurnaroundMs;
}

inline uint32_t slotStartMs(const Schedule &s, uint8_t node) {
    return s.leadMs + (node - 1) * slotMs(s);
}

inline void setRadioAddress(RH_RF95 &radio, uint8_t address) {
    radio.setThisAddress(address);
    radio.setHeaderFrom(address);
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
    setRadioAddress(radio, address);
    return true;
}

inline bool sendFrame(RH_RF95 &radio, uint8_t destination, const uint8_t *frame, uint8_t len) {
    radio.setHeaderTo(destination);
    if (radio.send(frame, len) && radio.waitPacketSent(airtimeMs(len) + FrameMarginMs)) {
        return true;
    }
    radio.setModeIdle();
    return false;
}

} // namespace bench
