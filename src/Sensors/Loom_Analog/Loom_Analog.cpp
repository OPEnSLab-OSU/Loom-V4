#include "Loom_Analog.h"
#include "Loom_Manager.h"
#include "Logger.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Analog::Loom_Analog(Manager &man) : Module("Analog"), manInst(&man) {
    analogReadResolution(adcResolutionBits);
    pinMappings.reserve(1);
    const float batteryVoltage = readBatteryVoltage();
    pinMappings.emplace_back(batteryPin, "Vbat", batteryVoltage, batteryVoltage * 1000.0f);
    registerWithManager();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::registerWithManager() { manInst->registerModule(this); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::measure() {
    FUNCTION_START(this);

    // Read the data from the given analog pin
    for (size_t i = 0; i < pinMappings.size(); i++) {

        // The battery input uses a voltage divider and averaging. Keep its existing volts/MV
        // fields; ordinary analog inputs report the ADC count plus a converted MV field.
        if (pinMappings[i].pinNumber == batteryPin) {
            const float batteryVoltage = readBatteryVoltage();
            pinMappings[i].analog = batteryVoltage;
            pinMappings[i].analog_mv = batteryVoltage * 1000.0f;
        }

        /* If its a normal pin then just read the value and update the previous values */
        else {
            int analogData = analogRead(pinMappings[i].pinNumber);
            pinMappings[i].analog = static_cast<float>(analogData);
            pinMappings[i].analog_mv = analogToMV(analogData);
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::package() {
    FUNCTION_START(this);
    char output[10];
    JsonObject json = manInst->get_data_object(getModuleName());

    /* Loop over the list of pins and pull out the data to formulate the JSON entries*/
    for (const AnalogMapping &mapping : pinMappings) {
        if (outputRaw) {
            json[mapping.name] = mapping.analog;
        }
        if (outputMillivolts) {
            // snprintf writes the terminator; clearing the whole buffer is unnecessary.
            snprintf(output, sizeof(output), "%s_MV", mapping.name);
            json[output] = mapping.analog_mv;
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getBatteryVoltage(int batteryPin, uint8_t resolutionBits, float referenceVoltage,
                                     float dividerScale, uint8_t sampleCount, uint32_t maxReading) {
    if (sampleCount == 0 || maxReading == 0) {
        return 0.0f;
    }

    analogReadResolution(resolutionBits);
    pinMode(batteryPin, INPUT);
    // Discard the first conversion after selecting/configuring this input before averaging.
    (void)analogRead(batteryPin);

    uint32_t readingSum = 0;
    for (uint8_t i = 0; i < sampleCount; ++i) {
        readingSum += analogRead(batteryPin);
        delayMicroseconds(50);
    }

    const float averageReading = static_cast<float>(readingSum) / sampleCount;
    return averageReading * dividerScale * referenceVoltage / maxReading;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::readBatteryVoltage() const {
    FUNCTION_START(this);
    return getBatteryVoltage(batteryPin, adcResolutionBits, adcReferenceVoltage,
                             batteryDividerScale, batterySampleCount, adcMaxReading);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::analogToMV(int analog) {
    if (adcMaxReading == 0) {
        return 0.0f;
    }
    const float voltage = (analog * adcReferenceVoltage) / adcMaxReading;
    return voltage * 1000.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getMV(int pin) {
    for (size_t i = 0; i < pinMappings.size(); i++) {
        if (pinMappings[i].pinNumber == pin) {
            return pinMappings[i].analog_mv;
        }
    }
    return 0.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getAnalog(int pin) {
    for (size_t i = 0; i < pinMappings.size(); i++) {
        if (pinMappings[i].pinNumber == pin) {
            return pinMappings[i].analog;
        }
    }
    return 0.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
