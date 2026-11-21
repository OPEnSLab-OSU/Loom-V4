#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

extern uint32_t now;
inline uint32_t millis() { return now; }
inline long random(long minimum, long) { return minimum; }

constexpr uint8_t RH_FLAGS_ACK = 0x80, RH_FLAGS_RETRY = 0x40, RH_BROADCAST_ADDRESS = 0xff;
constexpr uint8_t RH_RF95_REG_01_OP_MODE = 1, RH_RF95_REG_40_DIO_MAPPING1 = 0x40,
                  RH_RF95_MODE_CAD = 7;
enum { RHModeIdle, RHModeCad };
#define YIELD (++now)

class RHGenericDriver {
  public:
    virtual bool recv(uint8_t *data, uint8_t *length) = 0;
    virtual bool available() = 0;
};

class RH_RF95 : public RHGenericDriver {
  public:
    struct Frame {
        uint32_t readyAt;
        uint8_t from;
        uint8_t to;
        uint8_t flags;
        std::vector<uint8_t> payload;
        uint8_t id = 0;
    };
    struct Transmission {
        uint32_t startedAt;
        uint8_t to, id, flags;
    };

    std::deque<Frame> incoming;
    std::vector<Transmission> sent;
    bool sendWorks = true;
    uint16_t txDelay = 0;

    bool available() override {
        return !incoming.empty() && static_cast<int32_t>(now - incoming.front().readyAt) >= 0;
    }

    bool recv(uint8_t *data, uint8_t *length) override {
        if (!available()) {
            return false;
        }
        last = incoming.front();
        incoming.pop_front();
        if (data && length) {
            *length = std::min(*length, static_cast<uint8_t>(last.payload.size()));
            memcpy(data, last.payload.data(), *length);
        }
        return true;
    }

    uint8_t headerFrom() const { return last.from; }
    uint8_t headerTo() const { return last.to; }
    uint8_t headerId() const { return last.id; }
    uint8_t headerFlags() const { return last.flags; }
    virtual bool waitPacketSent() { return true; }
    virtual bool waitPacketSent(uint16_t timeout) {
        now += std::min(txDelay, timeout);
        return txDelay <= timeout;
    }
    virtual bool isChannelActive() { return false; }

  protected:
    int _mode = RHModeIdle;
    bool _cad = false;
    void spiWrite(uint8_t, uint8_t) {}
    void setModeIdle() { _mode = RHModeIdle; }

  private:
    Frame last{};
};
