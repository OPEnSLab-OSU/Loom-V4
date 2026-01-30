#pragma once

class Adapter {
    public:
        virtual bool sendHeartbeat(const uint8_t address, const JsonObject& payload) = 0;

        // virtual bool sendHeartbeat(const char* metadata) = 0;
};