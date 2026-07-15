#pragma once

#include "Loom_LTE_Modem.h"

template <typename ModemType, typename ClientType>
class Loom_LTE_TinyGsmAdapter : public Loom_LTE_Modem {
    public:
        explicit Loom_LTE_TinyGsmAdapter(Stream& stream)
            : modem(stream), client(modem) {}

        bool testAT(uint32_t timeoutMs) override { return modem.testAT(timeoutMs); }
        bool init() override { return modem.init(); }
        void sendAT(const __FlashStringHelper* command) override { modem.sendAT(command); }
        void setPdpContext(const char* apn) override {
            modem.sendAT(GF("+CGDCONT=1,\"IP\",\""), apn, GF("\""));
        }
        int8_t waitResponse(uint32_t timeoutMs) override { return modem.waitResponse(timeoutMs); }

        String getModemInfo() override { return modem.getModemInfo(); }
        int16_t getSignalQuality() override { return modem.getSignalQuality(); }
        IPAddress localIP() override { return modem.localIP(); }
        bool isGprsConnected() override { return modem.isGprsConnected(); }
        void poweroff() override { (void)modem.poweroff(); }
        int getSimStatus() override { return static_cast<int>(modem.getSimStatus()); }
        int getRegistrationStatus() override {
            return static_cast<int>(modem.getRegistrationStatus());
        }
        bool waitForNetwork(uint32_t timeoutMs) override {
            return modem.waitForNetwork(timeoutMs);
        }
        bool isNetworkConnected() override { return modem.isNetworkConnected(); }
        bool gprsDisconnect() override { return modem.gprsDisconnect(); }
        bool gprsConnect(const char* apn, const char* user, const char* pass) override {
            return modem.gprsConnect(apn, user, pass);
        }
        bool getNetworkTime(int* year, int* month, int* day, int* hour,
                            int* minute, int* second, float* timezone) override {
            return modem.getNetworkTime(year, month, day, hour, minute, second, timezone);
        }
        Client* getClient() override { return &client; }

    private:
        ModemType modem;
        ClientType client;
};

