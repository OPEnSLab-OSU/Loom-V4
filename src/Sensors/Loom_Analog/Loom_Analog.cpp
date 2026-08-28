#include "Loom_Analog.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::measure() {

    // Read the data from the given analog pin
    for (size_t i = 0; i < pinMappings.size(); i++) {

        /* If we are measuring the Vbat pin we want a little different behavior */
        if (pinMappings[i].pinNumber == batteryPin) {
            const float batteryVoltage = readBatteryVoltage();
            pinMappings[i].analog = batteryVoltage;
            pinMappings[i].analog_mv = batteryVoltage * 1000.0f;
        }

        /* If its a normal pin then just read the value and update the previous values */
        else {
            int analogData = analogRead(pinMappings[i].pinNumber);
            pinMappings[i].analog = analogData;
            pinMappings[i].analog_mv = analogToMV(analogData);
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::package() {
    char output[10];
    JsonObject json = manInst->get_data_object(getModuleName());

    /* Loop over the list of pins and pull out the data to formulate the JSON entries*/
    for (size_t i = 0; i < pinMappings.size(); i++) {
        memset(output, '\0', 10);
        json[pinMappings[i].name] = pinMappings[i].analog;

        /* Append MV to the name to differentiate between normal analog and the millivolt
         * representation */
        snprintf(output, sizeof(output), "%s_MV", pinMappings[i].name);
        json[output] = pinMappings[i].analog_mv;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getBatteryVoltage(int batteryPin, uint8_t resolutionBits, float referenceVoltage,
                                     float dividerScale, uint8_t sampleCount, uint32_t maxReading) {
    if (sampleCount == 0 || maxReading == 0) {
        return 0.0f;
    }

    analogReadResolution(resolutionBits);
    pinMode(batteryPin, INPUT);
    (void)analogRead(batteryPin);

    uint32_t readingSum = 0;
    for (uint8_t i = 0; i < sampleCount; ++i) {
        readingSum += analogRead(batteryPin);
        delayMicroseconds(50);
    }

    const float averageReading = static_cast<float>(readingSum) / sampleCount;
    return averageReading * dividerScale * referenceVoltage / maxReading;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

float Loom_Analog::readBatteryVoltage() const {
    return getBatteryVoltage(batteryPin, adcResolutionBits, adcReferenceVoltage,
                             batteryDividerScale, batterySampleCount, adcMaxReading);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::analogToMV(int analog) {
    if (adcMaxReading == 0) {
        return 0.0f;
    }
    const float voltage = (analog * adcReferenceVoltage) / adcMaxReading;
    return voltage * 1000.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getMV(int pin) {
    for (size_t i = 0; i < pinMappings.size(); i++) {
        if (pinMappings[i].pinNumber == pin) {
            return pinMappings[i].analog_mv;
        }
    }
    return 0.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getAnalog(int pin) {
    for (size_t i = 0; i < pinMappings.size(); i++) {
        if (pinMappings[i].pinNumber == pin) {
            return pinMappings[i].analog;
        }
    }
    return 0.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
