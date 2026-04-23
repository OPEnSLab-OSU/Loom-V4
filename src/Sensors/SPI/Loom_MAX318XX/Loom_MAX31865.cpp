#include "Loom_MAX31865.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MAX31865::Loom_MAX31865(Manager &man, int samples, int chip_select)
    : Module("MAX31865"), manInst(&man), max(chip_select), num_samples(samples) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31865::initialize() { max.begin(MAX31865_2WIRE); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31865::measure() {
    float temp = 0;

    // Collect the data however many times as specified
    for (int i = 0; i < num_samples; i++) {
        temp += max.temperature(RNOMINAL, RREF);

        // Check and print any faults
        uint8_t fault = max.readFault();
        if (fault) {
            if (fault & MAX31865_FAULT_HIGHTHRESH)
                ERROR(F("RTD High Threshold"));
            if (fault & MAX31865_FAULT_LOWTHRESH)
                ERROR(F("RTD Low Threshold"));
            if (fault & MAX31865_FAULT_REFINLOW)
                ERROR(F("REFIN- > 0.85 x Bias"));
            if (fault & MAX31865_FAULT_REFINHIGH)
                ERROR(F("REFIN- < 0.85 x Bias - FORCE- open"));
            if (fault & MAX31865_FAULT_RTDINLOW)
                ERROR(F("RTDIN- < 0.85 x Bias - FORCE- open"));
            if (fault & MAX31865_FAULT_OVUV)
                ERROR(F("Under/Over voltage"));
            break;
        }
    }

    // Get the average temperature
    temperature = temp / num_samples;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31865::package() {
    JsonObject json = manInst->get_data_object(getModuleName());
    json["Temperature_°C"] = temperature;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////