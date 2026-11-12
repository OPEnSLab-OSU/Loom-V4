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
constexpr uint8_t IndividualPoll = 0x3f, IndividualControl = 0x40;
constexpr uint8_t Compact = 0x41, FragmentAck = 0x42;
constexpr uint8_t RecoveryPoll = 0x43, PacedRecoveryPoll = 0x44;
constexpr uint8_t SensorSample = 0, SyntheticSample = 1, ProbeSample = 2;
constexpr uint8_t HeaderBytes = 6, BeaconBytes = 13;
constexpr uint8_t OfferBytes = 2, PingBytes = 4;
constexpr uint8_t JoinAddress = 0;
constexpr uint8_t JoinOfferBytes = 4, JoinFrameBytes = JoinOfferBytes + 1 + BoardKeyBytes;
constexpr uint8_t PacedQuietOffset = 4 + SlotMaskBytes, PacedControlBytes = PacedQuietOffset + 2;
constexpr uint8_t LinkBytes = 6;
constexpr uint8_t MaxBeaconBytes = BeaconBytes + SlotMaskBytes + MaxNodes * BoardKeyBytes + LinkBytes;
constexpr uint8_t PollMaskOffset = BeaconBytes, PollAttemptOffset = PollMaskOffset + 1;
constexpr uint8_t IndividualPollBytes = PollAttemptOffset + 1;
constexpr uint8_t MaxIndividualPollBytes = IndividualPollBytes + BoardKeyBytes;
constexpr uint8_t MaxRecoveryPollBytes = MaxIndividualPollBytes + LinkBytes;
static_assert(MaxBeaconBytes <= RH_RF95_MAX_MESSAGE_LEN, "Enrollment roster must fit in one beacon");
static_assert(MaxNodes < 32 && MaxFragments <= 8, "Retry masks must cover all configured slots");
constexpr uint16_t PingTurnaroundMs = 20;
constexpr uint8_t MaxChunk = RH_RF95_MAX_MESSAGE_LEN - HeaderBytes;
constexpr uint16_t GuardMs = 12, FrameMarginMs = 10;
constexpr uint16_t MaxPadding = 1800;
constexpr float FrequencyMHz = RF95_FREQ;
#ifndef BENCH_TX_POWER_DBM
#define BENCH_TX_POWER_DBM 23
#endif
constexpr int8_t TxPowerDbm = BENCH_TX_POWER_DBM;
static_assert(BENCH_TX_POWER_DBM >= 5 && BENCH_TX_POWER_DBM <= 23, "RFM95 PA_BOOST setting must be 5-23 dBm");
constexpr uint16_t DefaultAckTimeoutMs = 500;
constexpr bool WideBandwidth = false; // Match on every radio. SF7, CR4/5 in both profiles.

enum class Transport : uint8_t { Basic, Ack, RadioHead };
struct LinkSettings {
    Transport transport;
    uint8_t retries;
    uint16_t cadMs, ackMs;

    constexpr bool enabled() const { return transport != Transport::Basic || cadMs; }
    constexpr bool valid() const {
        return transport <= Transport::RadioHead && retries <= MaxRetries && cadMs <= 10000 &&
               (transport == Transport::Basic || (ackMs >= 60 && ackMs <= 1000));
    }
};

struct Schedule {
    uint8_t nodes;
    uint8_t chunk;
    uint8_t fragments;
    uint16_t leadMs;
    LinkSettings link;

    constexpr bool valid() const {
        return nodes >= 1 && nodes <= MaxNodes && chunk >= 1 && chunk <= MaxChunk &&
               fragments >= 1 && fragments <= MaxFragments && leadMs >= GuardMs && link.valid();
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

struct SampleRequest {
    Schedule plan;
    uint16_t round, padBytes;
    uint8_t format, source;

    bool valid() const {
        return plan.valid() && (format == MsgPack || format == Json || format == Compact) &&
               source <= ProbeSample && padBytes <= MaxPadding;
    }
};

inline SampleRequest readSampleRequest(const uint8_t *frame) {
    return {{frame[4], frame[5], frame[6], read16(frame + 7), {}}, read16(frame + 2),
            read16(frame + 11), frame[9], frame[10]};
}

inline LinkSettings readLinkSettings(const uint8_t *p) {
    return {static_cast<Transport>(p[0]), p[1], read16(p + 2), read16(p + 4)};
}

inline void writeLinkSettings(uint8_t *p, const LinkSettings &link) {
    p[0] = static_cast<uint8_t>(link.transport);
    p[1] = link.retries;
    write16(p + 2, link.cadMs);
    write16(p + 4, link.ackMs);
}

inline void writeSampleRequest(uint8_t *frame, const SampleRequest &request) {
    write16(frame + 2, request.round);
    frame[4] = request.plan.nodes;
    frame[5] = request.plan.chunk;
    frame[6] = request.plan.fragments;
    write16(frame + 7, request.plan.leadMs);
    frame[9] = request.format;
    frame[10] = request.source;
    write16(frame + 11, request.padBytes);
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

inline uint32_t linkFrameMs(uint8_t length, const LinkSettings &link) {
    uint32_t attempt = airtimeMs(length) + FrameMarginMs;
    if (link.cadMs) attempt += link.cadMs + FrameMarginMs;
    if (link.transport != Transport::Basic) {
        // RadioHead randomizes each ACK timeout up to twice its configured value.
        const uint32_t wait = link.transport == Transport::RadioHead ? 2UL * link.ackMs : link.ackMs;
        attempt += wait + airtimeMs(HeaderBytes) + FrameMarginMs;
        attempt *= 1 + link.retries;
    }
    return attempt;
}

inline uint32_t slotMs(const Schedule &s) {
    return 2 * GuardMs + s.fragments * linkFrameMs(HeaderBytes + s.chunk, s.link);
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
    radio.setHeaderFlags(0, RH_FLAGS_ACK | RH_FLAGS_RETRY);
    if (radio.send(frame, len) && radio.waitPacketSent(airtimeMs(len) + FrameMarginMs)) {
        return true;
    }
    radio.setModeIdle();
    return false;
}

// Use the SX127x CAD primitive with a short bounded backoff. RadioHead's built-in CAD wait
// can back off for another 900 ms after its deadline, which would invalidate a short slot.
class BenchRadio : public RH_RF95 {
  public:
    BenchRadio(uint8_t cs, uint8_t irq) : RH_RF95(cs, irq) {}

    bool send(const uint8_t *data, uint8_t length) override {
        if (cadMs) {
            if (!waitPacketSent(airtimeMs(RH_RF95_MAX_MESSAGE_LEN) + FrameMarginMs)) return false;
            setModeIdle();
            const uint32_t started = millis();
            while (isChannelActive()) {
                const uint32_t elapsed = millis() - started;
                if (elapsed >= cadMs) return false;
                delay(min(uint32_t(cadMs) - elapsed, static_cast<uint32_t>(random(2, 8))));
            }
            if (millis() - started > cadMs) return false;
        }
        return RH_RF95::send(data, length);
    }

    uint16_t cadMs = 0;
};

} // namespace bench
