#pragma once

#include "RH_RF95.h"

constexpr uint16_t RH_DEFAULT_TIMEOUT = 200;

class RHReliableDatagram {
  public:
    RHReliableDatagram(RHGenericDriver &driver, uint8_t address)
        : _driver(driver), address(address) {}

    uint8_t thisAddress() const { return address; }
    bool available() { return _driver.available(); }
    void setTimeout(uint16_t) {}
    uint8_t retries() const { return allowedRetries; }
    void setRetries(uint8_t value) { allowedRetries = value; }
    void setHeaderId(uint8_t id) { sendId = id; }
    void setHeaderFlags(uint8_t set, uint8_t clear) { sendFlags = (sendFlags & ~clear) | set; }

    bool waitAvailableTimeout(uint16_t timeout) {
        const uint32_t started = millis();
        while (millis() - started < timeout) {
            if (available()) {
                return true;
            }
            ++now;
        }
        return false;
    }

    bool sendto(uint8_t *, uint8_t, uint8_t destination) {
        auto &rf95 = static_cast<RH_RF95 &>(_driver);
        if (!rf95.sendWorks) {
            return false;
        }
        rf95.sent.push_back({millis(), destination, sendId, sendFlags});
        return true;
    }

    bool waitPacketSent(uint16_t timeout) {
        return static_cast<RH_RF95 &>(_driver).waitPacketSent(timeout);
    }

    bool recvfrom(uint8_t *data, uint8_t *size, uint8_t *from, uint8_t *to, uint8_t *id,
                  uint8_t *flags) {
        auto &radio = static_cast<RH_RF95 &>(_driver);
        if (!radio.recv(data, size)) {
            return false;
        }
        *from = radio.headerFrom();
        *to = radio.headerTo();
        *id = radio.headerId();
        *flags = radio.headerFlags();
        return true;
    }

  protected:
    RHGenericDriver &_driver;
    void acknowledge(uint8_t, uint8_t) {}

  private:
    uint8_t address;
    uint8_t allowedRetries = 3, sendId = 0, sendFlags = 0;
};
