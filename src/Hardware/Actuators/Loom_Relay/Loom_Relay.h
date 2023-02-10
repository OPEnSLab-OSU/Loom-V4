#pragma once

#include "Actuators.h"
#include "Loom_Manager.h"

/**
 * Relay controls for integration with Max
 * 
 * @author Will Richards
 */ 
class Loom_Relay : public Actuator{
    public:
        Loom_Relay(const byte controlPin = 10);

        /**
         * Construct a new object to manually control the relay
         * 
         * @param man Instance of the Manager
         */ 
        Loom_Relay(Manager& man, const byte controlPin = 10);

        void control(JsonArray json) override;
        void initialize() override {printModuleName("Initialized Module!"); };
        void package(JsonObject json) override;

        void printModuleName(String message) override { Serial.print("[" + (typeToString() + String(pin)) + "] "); };

        String getModuleName() override { return (typeToString() + String(pin)); };

        /**
         * Set the state of the relay 
         * 
         * @param state New state of the relay
         */
        void setState(bool state);

    private:

        Manager* manInst;
        byte pin;
        bool state = false;
};