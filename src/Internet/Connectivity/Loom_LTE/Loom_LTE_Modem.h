#pragma once

#include <Arduino.h>
#include <Client.h>

/**
 * Small runtime interface over TinyGSM's separate SARA-R4 and SARA-R5 types.
 * Keeping the concrete drivers behind this interface lets an Arduino sketch
 * select its modem in the Loom_LTE constructor without global build flags.
 */
class Loom_LTE_Modem {
  public:
    virtual ~Loom_LTE_Modem() = default;

    virtual bool testAT(uint32_t timeoutMs) = 0;
    virtual bool init() = 0;
    virtual void sendAT(const __FlashStringHelper *command) = 0;
    virtual void setPdpContext(const char *apn) = 0;
    virtual int8_t waitResponse(uint32_t timeoutMs) = 0;

    virtual int16_t getSignalQuality() = 0;
    virtual IPAddress localIP() = 0;
    virtual bool isGprsConnected() = 0;
    virtual void poweroff() = 0;
    virtual int getSimStatus() = 0;
    virtual int getRegistrationStatus() = 0;
    virtual bool waitForNetwork(uint32_t timeoutMs) = 0;
    virtual bool isNetworkConnected() = 0;
    virtual bool gprsDisconnect() = 0;
    virtual bool gprsConnect(const char *apn, const char *user, const char *pass) = 0;
    virtual bool getNetworkTime(int *year, int *month, int *day, int *hour, int *minute,
                                int *second, float *timezone) = 0;
    virtual Client *getClient() = 0;
};

Loom_LTE_Modem *createLteModem(bool saraR5, Stream &stream);
