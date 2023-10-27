#pragma once

#include <SensirionI2CSEN55.h>
#include "Loom_Manager.h"
#include "../I2CDevice.h"
#include <Wire.h>

/**
 * SEN55 Particulate Matter (1, 2.5, 4, 10), NoX, Vox, Humidity, Temperature
 * 
 * @author Zak Watts, Will Richards
*/
class Loom_SEN55 : public I2CDevice {
    protected:
        void initialize() override;
        void measure() override;
        void package() override;
        void power_up() override {};
        void power_down() override {};

    public:
        /**
         * Constructor for the SEN55 Sensor
         * 
         * @param manager Reference to the manager object
         * @param tempOffset Temperature offset 
         * @param addr I2C address of the SEN55
        */
        Loom_SEN55(Manager& manager, float tempOffset = 0.0, int addr = 0x69);

    private:
        SensirionI2CSEN55 SEN55Instance;    //SEN55 obj
        Manager *manInst;

        /* 
            Array of mass concentrations
            0 - PM 1.0
            1 - PM 2.5
            2 - PM 4.0
            4 - PM 10.0
        */
        float massConcentration[4] = {0}

        float tempOffset;
        float ambientHumidity;
        float ambientTemperature;
        float vocIndex;
        float noxIndex;



};

#endif