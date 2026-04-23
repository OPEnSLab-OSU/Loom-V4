#include "Loom_MAX31856.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MAX31856::Loom_MAX31856(Manager &man, int samples, int chip_select, int mosi, int miso,
                             int sclk)
    : Module("MAX31856"), manInst(&man), maxthermo(chip_select, mosi, miso, sclk),
      num_samples(samples) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::initialize() {
    if (!maxthermo.begin()) {
        ERROR(F("Could not initialize thermocouple."));
        moduleInitialized = false;
        return;
    } else {
        LOG(F("Successfully initialized thermocouple."));
    }
    maxthermo.setThermocoupleType(MAX31856_TCTYPE_K);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::measure() {
    if (moduleInitialized) {
        float temp = 0;

        // Collect the data however many times as specified
        for (int i = 0; i < num_samples; i++) {
            temp += maxthermo.readThermocoupleTemperature();

            // Check and print any faults
            uint8_t fault = maxthermo.readFault();
            if (fault) {
                if (fault & MAX31856_FAULT_CJRANGE)
                    ERROR(F("Cold Junction Range Fault"));
                if (fault & MAX31856_FAULT_TCRANGE)
                    ERROR(F("Thermocouple Range Fault"));
                if (fault & MAX31856_FAULT_CJHIGH)
                    ERROR(F("Cold Junction High Fault"));
                if (fault & MAX31856_FAULT_CJLOW)
                    ERROR(F("Cold Junction Low Fault"));
                if (fault & MAX31856_FAULT_TCHIGH)
                    ERROR(F("Thermocouple High Fault"));
                if (fault & MAX31856_FAULT_TCLOW)
                    ERROR(F("Thermocouple Low Fault"));
                if (fault & MAX31856_FAULT_OVUV)
                    ERROR(F("Over/Under Voltage Fault"));
                if (fault & MAX31856_FAULT_OPEN)
                    ERROR(F("Thermocouple Open Fault"));
                break;
            }
        }

        // Get the average temperature
        temperature = temp / num_samples;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::package() {
    if (moduleInitialized) {
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Temperature_°C"] = temperature;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////