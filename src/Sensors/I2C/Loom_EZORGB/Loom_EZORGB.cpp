#include "Loom_EZORGB.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_EZORGB::Loom_EZORGB(Manager &man, byte address, bool useMux)
    : EZOSensor("EZO-RGB"), manInst(&man) {
    module_address = address;

    if (!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZORGB::initialize() { Wire.begin(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZORGB::measure() {
    if (moduleInitialized) {

        // Attempt to read data from the sensor
        if (!readSensor(400)) {
            ERROR(F("Failed to read sensor!"));
            return;
        }

        // Parse the constructed string
        if (!parseData(getSensorData())) {
            ERROR(F("Malformed RGB response"));
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZORGB::package() {
    if (moduleInitialized) {
        JsonObject json = manInst->get_data_object(getModuleName());

        // these are rgb intensity values (0-255). These are unitless.
        json["Red"] = rgb[0];
        json["Green"] = rgb[1];
        json["Blue"] = rgb[2];
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZORGB::power_down() {
    if (moduleInitialized) {
        if (!sendTransmission("sleep")) {
            ERROR(F("Failed to send 'sleep' command to device"));
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_EZORGB::parseData(const char *sensorData) {
    if (!sensorData) {
        return false;
    }

    // Parse out the comma separated strings without committing a partial sample.
    char response[33] = {};
    strncpy(response, sensorData, sizeof(response) - 1);

    char *splitPointer = strtok(response, ",");
    if (!splitPointer) {
        return false;
    }
    const int red = atoi(splitPointer);

    splitPointer = strtok(NULL, ",");
    if (!splitPointer) {
        return false;
    }
    const int green = atoi(splitPointer);

    splitPointer = strtok(NULL, ",");
    if (!splitPointer) {
        return false;
    }

    rgb[0] = static_cast<uint8_t>(red);
    rgb[1] = static_cast<uint8_t>(green);
    rgb[2] = static_cast<uint8_t>(atoi(splitPointer));
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
