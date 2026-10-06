#pragma once

#include "Loom_Manager.h"
#include "SparkFun_Weather_Meter_Kit_Arduino_Library.h"
#include "../../Sensors/Loom_Analog/Loom_Analog.h"

#define MPH_CONVERSION 0.621371
#define INCHES_CONVERSION 25.4

class Loom_Anemometer: public Module {
    public:
        // constructor 
        Loom_Anemometer(Manager& manInst, int windPin, int speedPin, int rainfallPin);
        // default constructor, leaves module uninitialized
        Loom_Anemometer(Manager& manInst);

        void initialize() override;
        void measure() override;
        void package() override;
        void power_up() override {};
        void power_down() override {};

    private:

        float kph_to_mph(float kmh);
        float mm_to_inches(float mm);

        Manager* manInst;
        // assigned data pins for each sensor on the weather meter kit
        int wind_pin;
        int speed_pin;
        int rainfall_pin;
        // weaether meter kit values
        float windspeed_mph = 0;
        float windspeed_kph = 0;
        // wind direction in degrees
        float wind_direction = 0;
        // rainfall in mm and inches
        float rainfall_mm = 0;
        float rainfall_inches = 0;
        // if we want to use the tipping bucket on the weather meter kit. False
        // implies we are going to use the Loom_TippingBucket
        bool measure_rainfall = false;
        // SFE weather kit object. Need to pass alues, so all zeros. 
        // doesnt matter what pins until we call begin()
        SFEWeatherMeterKit anemometer = SFEWeatherMeterKit(0,0,0);
        // parameters to pass to external Weather Kit library. Allows for 
        // custom configuration
        SFEWeatherMeterKitCalibrationParams calibration_params;

};

