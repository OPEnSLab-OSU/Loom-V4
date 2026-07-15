#include "Loom_MAX31856.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MAX31856::Loom_MAX31856(Manager &man, int chip_select, int samples, int mosi, int miso,
                             int sclk)
    : Module("MAX31856"), manInst(&man), num_samples(samples > 0 ? samples : 1) {
    if (mosi >= 0 && miso >= 0 && sclk >= 0)
        maxthermo = new Adafruit_MAX31856(chip_select, mosi, miso, sclk);
    else
        maxthermo = new Adafruit_MAX31856(chip_select);
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::initialize() {
    if (maxthermo == nullptr || !maxthermo->begin()) {
        ERROR(F("Could not initialize thermocouple."));
        moduleInitialized = false;
        return;
    } else {
        LOG(F("Successfully initialized thermocouple."));
    }
    maxthermo->setThermocoupleType(MAX31856_TCTYPE_K);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::measure() {
    if (moduleInitialized) {
        float temp = 0;
        int successfulSamples = 0;

        // Collect the data however many times as specified
        for (int i = 0; i < num_samples; i++) {
            const float sample = maxthermo->readThermocoupleTemperature();

            // Check and print any faults
            uint8_t fault = maxthermo->readFault();
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
            if (!isnan(sample)) {
                temp += sample;
                successfulSamples++;
            }
        }

        if (successfulSamples > 0)
            temperature = temp / successfulSamples;
        else
            ERROR(F("No valid thermocouple samples collected"));
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
