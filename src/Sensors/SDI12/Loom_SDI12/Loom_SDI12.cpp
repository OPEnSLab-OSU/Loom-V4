#include "Loom_SDI12.h"
#include "Logger.h"

namespace {
bool parseFloatToken(char *&context, float &value) {
    char *token = strtok_r(nullptr, "+", &context);
    if (token == nullptr)
        return false;

    char *end = nullptr;
    const float parsed = strtof(token, &end);
    if (end == token)
        return false;
    while (*end == ' ' || *end == '\t' || *end == '\r')
        ++end;
    if (*end != '\0')
        return false;

    value = parsed;
    return true;
}
} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SDI12::Loom_SDI12(Manager &man, const int pinNumber)
    : Module("SDI12"), sdiInterface(pinNumber) {
    manInst = &man;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SDI12::Loom_SDI12(const int pinNumber) : Module("SDI12"), sdiInterface(pinNumber) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::initialize() {
    LOG(F("Initializing SDI-12 Sensors..."));

    // On init we set the SDI pin to OUTPUT so we can request data
    pinMode(sdiInterface.getDataPin(), OUTPUT);

    // Start the interface and then wait for 100ms to allow things to settle and startup correctly
    sdiInterface.begin();
    delay(100);

    // Create a list of addresses that have a sensor connected to them
    inUseAddresses = scanAddressSpace();

    // Store all persistent sensor state in one vector allocation. The former map plus two
    // mallocs per sensor fragmented the 32 KB SAMD21 heap and leaked on repeated initialize().
    sensors.clear();
    sensors.reserve(inUseAddresses.size());
    for (char address : inUseAddresses) {
        SensorRecord sensor;
        requestSensorInfo(sensor.type, address);
        sensors.push_back(sensor);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::measure() {

    // On measure we also want to reset the mode to output in case the 4G board has messed with it
    pinMode(sdiInterface.getDataPin(), OUTPUT);
    delay(30);

    // Populate the variables that will be used to package data
    for (size_t i = 0; i < inUseAddresses.size(); i++) {
        getData(inUseAddresses[i]);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::package() {

    for (size_t i = 0; i < inUseAddresses.size() && i < sensors.size(); i++) {
        SensorRecord &sensor = sensors[i];
        if (strstr(sensor.type, "GS3") != nullptr) {
            if (sensor.name[0] == '\0') {
                snprintf(sensor.name, sizeof(sensor.name), "GS3_%u",
                         static_cast<unsigned int>(i));
            }
            JsonObject json = manInst->get_data_object(sensor.name);
            json["Temperature"] = sensor.data[0];
            json["Dielectric_Permittivity"] = sensor.data[1];
            json["Conductivity"] = sensor.data[2];
        } else if (strstr(sensor.type, "TER") != nullptr) {
            if (sensor.name[0] == '\0') {
                snprintf(sensor.name, sizeof(sensor.name), "TER_%u",
                         static_cast<unsigned int>(i));
            }
            JsonObject json = manInst->get_data_object(sensor.name);
            json["Temperature"] = sensor.data[0];
            json["Volumetric_Water_Content"] = sensor.data[1];
            if (strstr(sensor.type, "TER12") != nullptr)
                json["Conductivity"] = sensor.data[2];
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::power_up() {
    pinMode(sdiInterface.getDataPin(), OUTPUT);
    sdiInterface.begin();
    delay(100);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::power_down() { sdiInterface.end(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<char> Loom_SDI12::scanAddressSpace() {
    std::vector<char> activeSensors;

    // Print the module name followed by the message saying please wait
    LOG(F("Scanning SDI-12 Address Space this make take a little while..."));

    // Scan over the characters that can be used as addresses for referencing the sensors
    for (char i = '0'; i <= '9'; i++) {
        if (checkActive(i)) {
            activeSensors.push_back(i);
        }
    }

    for (char i = 'a'; i <= 'z'; i++) {
        if (checkActive(i)) {
            activeSensors.push_back(i);
        }
    }

    for (char i = 'A'; i <= 'Z'; i++) {
        if (checkActive(i)) {
            activeSensors.push_back(i);
        }
    }

    // Check if we actually found any connected devices
    if (activeSensors.size() > 0) {
        // Print the module name followed by the message saying please wait
        LOG(F("== We found the following active Addresses =="));
        for (size_t i = 0; i < activeSensors.size(); i++) {
            LOGF("    Address: %c", activeSensors[i]);
        }
    } else {
        LOG(F("== No SDI-12 Devices Were Discovered == "));
    }

    return activeSensors;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_SDI12::checkActive(char addr) {
    // Attempt to contact the sensor 3 times
    char response[RESPONSE_SIZE];
    for (int i = 0; i < 3; i++) {
        memset(response, '\0', RESPONSE_SIZE);
        sendCommand(response, addr, "!");
        if (strlen(response) > 0)
            return true;
    }

    sdiInterface.clearBuffer();
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<char> Loom_SDI12::getInUseAddresses() { return inUseAddresses; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
const char *Loom_SDI12::getSensorInfo(char addr) {
    const int index = findSensorIndex(addr);
    return index >= 0 ? sensors[static_cast<size_t>(index)].type : "";
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
int Loom_SDI12::findSensorIndex(char addr) const {
    for (size_t i = 0; i < inUseAddresses.size() && i < sensors.size(); ++i)
        if (inUseAddresses[i] == addr)
            return static_cast<int>(i);
    return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::sendCommand(char response[RESPONSE_SIZE], char addr, const char *command) {
    if (response == nullptr || command == nullptr)
        return;

    // Send a request to the sensor at the given address and then wait 30ms before continuing
    char output[25];
    memset(output, '\0', 25);
    snprintf(output, 25, "%c%s", addr, command);
    sdiInterface.sendCommand(output);
    delay(30);
    readResponse(response);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::readResponse(char response[RESPONSE_SIZE]) {
    if (response == nullptr)
        return;

    size_t index = 0;
    memset(response, '\0', RESPONSE_SIZE);
    // While data is available to be read read until an end line character appears.
    while (sdiInterface.available()) {
        char c = sdiInterface.read();

        // Command responses terminate with an endline so we should stop when we see this
        if (c == '\n') {
            break;
        }

        if (index < RESPONSE_SIZE - 1)
            response[index++] = c;
        delay(20); // SDI-12 is slow so we need to wait after each character
    }
    response[index] = '\0';

    // Replace the carriage return with a null-byte
    char *pch = strstr(response, "\r");
    if (pch != NULL) {
        response[pch - response] = '\0';
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::requestSensorInfo(char response[RESPONSE_SIZE], char addr) {
    sendCommand(response, addr, "I!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::getData(char addr) {
    // char buf[20]; // Reserved for the former manual response parser.
    // char *p;      // The first token only advances strtok state; it is not otherwise needed.
    char response[RESPONSE_SIZE];

    // Request a measurement from the sensor at the given address
    sendCommand(response, addr, "M!");
    sendCommand(response, addr, "D0!");

    // If the value returned was 0 we want to re-request data
    if (strlen(response) == 1) {
        WARNING(F("Invalid data received! Retrying..."));
        delay(3000);

        // Request a measurement from the sensor at the given address
        sendCommand(response, addr, "M!");
        sendCommand(response, addr, "D0!");

        // TIMER_RESET;
        if (strlen(response) == 1) {
            WARNING(F("Retrying for a second time..."));
            delay(3000);

            // Request a measurement from the sensor at the given address
            sendCommand(response, addr, "M!");
            sendCommand(response, addr, "D0!");
            // TIMER_RESET;
        }
    }

    const int sensorIndex = findSensorIndex(addr);
    if (strlen(response) <= 1 || sensorIndex < 0) {
        ERROR(F("Failed to record new data! Using previous valid information!"));
        return;
    }

    SensorRecord &sensor = sensors[static_cast<size_t>(sensorIndex)];
    std::array<float, 3> parsed = sensor.data;
    char *context = nullptr;
    if (strtok_r(response, "+", &context) == nullptr) {
        ERROR(F("Malformed SDI-12 response; using previous valid information."));
        return;
    }

    bool valid = false;
    if (strstr(sensor.type, "GS3") != nullptr) {
        valid = parseFloatToken(context, parsed[1]) && parseFloatToken(context, parsed[0]) &&
                parseFloatToken(context, parsed[2]);
    } else if (strstr(sensor.type, "TER") != nullptr) {
        valid = parseFloatToken(context, parsed[1]) && parseFloatToken(context, parsed[0]);
        if (valid && strstr(sensor.type, "12") != nullptr)
            valid = parseFloatToken(context, parsed[2]);
    }

    if (!valid) {
        ERROR(F("Malformed SDI-12 numeric data; using previous valid information."));
        return;
    }

    sensor.data = parsed;
    for (size_t i = 0; i < parsed.size(); ++i)
        sensorData[i] = parsed[i];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
