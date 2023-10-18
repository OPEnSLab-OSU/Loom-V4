#ifndef LOOM_SEN5X_H
#define LOOM_SEN5X_H

#include <SensirionI2CSen5x.h>
#include "Loom_Manager.h"
#include "../I2CDevice.h"
#include <Wire.h>

#define MAXBUF_REQUIREMENT 48 // Test this

class Loom_Sen5x : public I2CDevice {
    protected:
        void initialize() override;

        void measure() override;

        void package() override;

        void power_up() override;
        void power_down() override;

    public:
        Loom_Sen5x(Manager&, float = 0.0, int = 0x69);

    private:
        SensirionI2CSen5x sen5xInstance;    //Sen5x obj
        Manager *manInst;

        float tempOffset;
        float massConcentrationPm1p0;
        float massConcentrationPm2p5;
        float massConcentrationPm4p0;
        float massConcentrationPm10p0;

        float t_massConcentrationPm1p0;
        float t_massConcentrationPm2p5;
        float t_massConcentrationPm4p0;
        float t_massConcentrationPm10p0;
        float t_numberConcentrationPm0p5;
        float t_numberConcentrationPm1p0;
        float t_numberConcentrationPm2p5;
        float t_numberConcentrationPm4p0;
        float t_numberConcentrationPm10p0;
        float t_typicalParticleSize;

        float ambientHumidity;
        float ambientTemperature;
        float vocIndex;
        float noxIndex;



};

#endif