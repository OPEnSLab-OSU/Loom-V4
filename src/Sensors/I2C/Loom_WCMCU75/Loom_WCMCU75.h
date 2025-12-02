#pragma once

#include <Wire.h>
#include <LM75.h>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

class Loom_WCMCU75 : public I2CDevice{
    protected:
        void initialize() override;
        void measure() override;
        void package() override;

        void power_up() override;
        void power_down() override;

    public:  
        Loom_WCMCU75(
                Manager& man,
                int address = LM75_ADDRESS,
                bool useMux = false
            );

    private:
        Manager* manInst; // instance of manager
        LM75 wcmcu75; // instance of LM75 chip

        float temperature = 0.0f;
        float tos = 0.0f;
        float thyst = 0.0f;
};

