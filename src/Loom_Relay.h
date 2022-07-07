#pragma once

#include "Actuators.h"

class Loom_Relay : public Actuator{
    public:
        Loom_Relay(const byte controlPin = 10);

        void control(JsonArray json) override;
        void initialize() override {};

    private:
        byte pin;
};