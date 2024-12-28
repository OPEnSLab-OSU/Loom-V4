#pragma once

#include <DFRobot_MultiGasSensor.h>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

/**
 * DFRobot MultiGasSensor
 *
 * @author Soren Emmons
 */
class Loom_MultiGasSensor : public I2CDevice{
    protected:

       // Manager controlled functions
        void measure() override;
        void initialize() override;
        void power_up() override {};
        void power_down() override {};
        void package() override;

    public:
        /**
         * Constructs a new sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         */
        Loom_MultiGasSensor(
                      Manager& man,
                      int address = 0x77,    
                      bool useMux = false
                );

        /**
         * Get gas type that is currently being recorded 
         */
        //uint16_t getGasType() {return ; };

        /**
         * Get the gas concentration that is currently being recorded
         */
        //uint16_t getGasConcentration() { return ; };



    private:
    Manager* manInst;
    DFRobot_GAS_I2C gas;
    bool moduleInitialized = false;
    bool needsReinit = false;
    String gasType;
    float Concentration;
};