#pragma once

#include "../Loom_Manager.h"
#include "../Module.h"
#include "../Loom_Hypnos/Loom_Hypnos.h"

class Adapter {
    public:
        virtual bool sendHeartbeat(const uint8_t address, const JsonObject& payload) = 0;
};