#pragma once

#include "Actuators.h"

/**
 * Relay controls for integration with Max
 * 
 * @author Will Richards
 */ 
class Loom_Relay : public Actuator{
    public:
        Loom_Relay(const byte controlPin = 10);

        void control(JsonArray json) override;
        void initialize() override {};

        void printModuleName() override { Serial.print("[" + (typeToString() + String(pin)) + "] "); };

    private:
        byte pin;
};