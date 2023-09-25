#pragma once

#include "Module.h"
#include "Loom_Manager.h"
#include <SDS011.h>

/**
 * Class for handling the NOVASDS011 Dust Sensor
 * 
 * @author Alexei Burgos-Davila
*/
class Loom_NOVASDS011: public Module{

    protected:
        void power_up() override {};
        void power_down() override {}; 
        
    public:
        void initialize() override;
        void measure() override;
        void package() override;

        Loom_NOVASDS011(Manager& man, HardwareSerial* serial = &Serial1);

    private:
        Manager* manInst;           // Instance of the Manager  
        SDS011 nova;                // Instance of the library
        HardwareSerial* serial;     // Pointer to the interface we are using
        float pm25;                 // PM 2.5 measurement
        float pm10;                 // PM 10 measurement
        











};

