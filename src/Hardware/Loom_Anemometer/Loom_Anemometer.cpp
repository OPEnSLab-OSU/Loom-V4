#include "Loom_Anemometer.h"
#include "Logger.h"

Loom_Anemometer::Loom_Anemometer(Manager& man, int windPin, int speedPin, int rainfallPin = -1): Module("Loom_Anemometer"), manInst(&man){
    
    wind_pin = windPin;
    speed_pin = speedPin;
    if(rainfallPin != -1){
        rainfall_pin = rainfallPin;
        }
    else{   
        measure_rainfall = false;
        }

    anemometer = SFEWeatherMeterKit(wind_pin, speed_pin, rainfall_pin);
    // ADC values for 16 angles
    // calibration_params.vaneADCValues = ;
    // 2 second windspeed measurement window
    calibration_params.windSpeedMeasurementPeriodMillis = 2000;
    // rainfall bucket calibration
    // calibration_params.mmPerRainfallCount = ;
    // calibration_params.minMillisPerRainfall =
    anemometer.setCalibrationParams(calibration_params);
    manInst->registerModule(this);

}

Loom_Anemometer::Loom_Anemometer(Manager& man) : Module("Loom_Anemometer"), manInst(&man){
    this->manInst = manInst;

    moduleInitialized = false;
}

void Loom_Anemometer::initialize(){
    LOG(F("Initializing Loom Anemometer"));
    anemometer.setADCResolutionBits(12);

    anemometer.begin();

    LOG(F("Sensor successfully initialized!"));
}

void Loom_Anemometer::measure(){
    // according to sparkfun library, the first cycle will not output values for windspeed. The cycle after will. 
    LOG(F("Measuring wind speed. This will take a few seconds..."));
    windspeed_kph = anemometer.getWindSpeed();
    windspeed_mph = kph_to_mph(windspeed_kph);

    LOG(F("Measuring wind direction"));
    wind_direction = anemometer.getWindDirection();

    if(measure_rainfall){
        rainfall_mm = anemometer.getTotalRainfall();
        rainfall_inches = mm_to_inches(rainfall_mm);
    }
}

void Loom_Anemometer::package(){

    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["windSpeed_kmh"] = windspeed_kph;
        json["windspeed_mph"] = windspeed_mph;
        json["rainfall_mm"] = rainfall_mm;
        json["rainfall_inches"] = rainfall_inches;
        json["wind_direction_degrees"] = wind_direction; 
    }
    FUNCTION_END;

}

float Loom_Anemometer::kph_to_mph(float kmh){
    return kmh * MPH_CONVERSION;
}

float Loom_Anemometer::mm_to_inches(float mm){
    return mm/INCHES_CONVERSION;
}