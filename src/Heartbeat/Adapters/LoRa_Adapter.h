#pragma once

#include "../Adapter.h"
#include "../../Radio/Loom_LoRa/Loom_LoRa.h"

class LoRa_Adapter : public Adapter {
    public:
        LoRa_Adapter(Loom_LoRa* passedLoraModule) {
            this->loraModule = passedLoraModule;
        }

        virtual bool sendHeartbeat(const uint8_t address, const JsonObject& payload) override {
            return loraModule->send(address, payload);
        }

    private:
        Loom_LoRa* loraModule = nullptr;
};