#include "Loom_SDI12.h"
#include "Logger.h"
#include "Loom_Manager.h"

#include "Utilities/Loom_Watchdog.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SDI12::Loom_SDI12(Manager &man, const int pinNumber)
    : Module("SDI12"), sdiInterface(pinNumber) {
    manInst = &man;
    manInst->registerModule(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SDI12::Loom_SDI12(const int pinNumber) : Module("SDI12"), sdiInterface(pinNumber) {}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::initialize() {
    LOG(F("Initializing SDI-12 Sensors..."));

    // On init we set the SDI pin to OUTPUT so we can request data
    pinMode(sdiInterface.getDataPin(), OUTPUT);

    // METER's power-up DDI message can occupy the data wire before SDI-12 is ready.
    // Wait after the rail comes up, then discard startup bytes before scanning addresses.
    sdiInterface.begin();
    {
        LoomWatchdogPause pause;
        delay(powerUpDelayMs);
    }
    sdiInterface.clearBuffer();

    // Create a list of addresses that have a sensor connected to them
    inUseAddresses = scanAddressSpace();

    // Store all persistent sensor state in one vector allocation. The former map plus two
    // mallocs per sensor fragmented the 32 KB SAMD21 heap and leaked on repeated initialize().
    sensors.clear();
    sensors.reserve(inUseAddresses.size());
    for (char address : inUseAddresses) {
        SensorRecord sensor;
        requestSensorInfo(sensor.type, address);
        if (sensor.type[0] != address) {
            sensor.type[0] = '\0';
        }
        if (loomSDI12::identifyModel(sensor.type) == loomSDI12::Model::Unknown) {
            WARNINGF("SDI-12 address %c has an unsupported or unreadable sensor model: %s", address,
                     sensor.type);
        }
        sensors.push_back(sensor);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::measure() {

    // On measure we also want to reset the mode to output in case the 4G board has messed with it
    pinMode(sdiInterface.getDataPin(), OUTPUT);
    delay(30);

    // Populate the variables that will be used to package data
    for (char address : inUseAddresses) {
        getData(address);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::package() {
    if (manInst == nullptr) {
        return; // The manual constructor does not register with a Manager.
    }

    for (size_t i = 0; i < inUseAddresses.size() && i < sensors.size(); i++) {
        SensorRecord &sensor = sensors[i];
        const loomSDI12::Model model = loomSDI12::identifyModel(sensor.type);
        if (model == loomSDI12::Model::TER21) {
            snprintf(sensor.name, sizeof(sensor.name), "TER21_%u", static_cast<unsigned int>(i));
            JsonObject json = manInst->get_data_object(sensor.name);
            json["Temperature"] = sensor.data[0];
            json["Matric_Potential"] = sensor.data[1]; // kPa; this is not water content.
        } else if (model == loomSDI12::Model::TER54) {
            snprintf(sensor.name, sizeof(sensor.name), "TER54_%u", static_cast<unsigned int>(i));
            JsonObject json = manInst->get_data_object(sensor.name);
            for (uint8_t depth = 0; depth < 4; ++depth) {
                char key[40];
                snprintf(key, sizeof(key), "Temperature_D%u", static_cast<unsigned>(depth + 1));
                json[key] = sensor.data[depth * 2];
                snprintf(key, sizeof(key), "Volumetric_Water_Content_D%u",
                         static_cast<unsigned>(depth + 1));
                json[key] = sensor.data[depth * 2 + 1]; // m3/m3, using the probe's M! calibration.
            }
        } else if (model == loomSDI12::Model::GS3) {
            if (sensor.name[0] == '\0') {
                snprintf(sensor.name, sizeof(sensor.name), "GS3_%u", static_cast<unsigned int>(i));
            }
            JsonObject json = manInst->get_data_object(sensor.name);
            json["Temperature"] = sensor.data[0];
            json["Dielectric_Permittivity"] = sensor.data[1];
            json["Conductivity"] = sensor.data[2];
        } else if (model == loomSDI12::Model::TER11 || model == loomSDI12::Model::TER12) {
            if (sensor.name[0] == '\0') {
                snprintf(sensor.name, sizeof(sensor.name), "TER_%u", static_cast<unsigned int>(i));
            }
            JsonObject json = manInst->get_data_object(sensor.name);
            json["Temperature"] = sensor.data[0];
            // Preserve the legacy label/value: M! returns calibrated ADC counts on TER11/12.
            // Converting these counts to m3/m3 requires a separate, explicit calibration policy.
            json["Volumetric_Water_Content"] = sensor.data[1];
            if (model == loomSDI12::Model::TER12) {
                json["Conductivity"] = sensor.data[2];
            }
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::power_up() {
    pinMode(sdiInterface.getDataPin(), OUTPUT);
    sdiInterface.begin();
    {
        LoomWatchdogPause pause;
        delay(powerUpDelayMs);
    }
    sdiInterface.clearBuffer();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::power_down() { sdiInterface.end(); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<char> Loom_SDI12::scanAddressSpace() {
    std::vector<char> activeSensors;

    // Print the module name followed by the message saying please wait
    LOG(F("Scanning SDI-12 Address Space; this may take a little while..."));

    // Preserve discovery order: it determines the numbered module names in packaged data.
    constexpr char addresses[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (size_t i = 0; i < sizeof(addresses) - 1; ++i) {
        const char address = addresses[i];
        if (checkActive(address)) {
            activeSensors.push_back(address);
        }
        LOOM_FEED_WATCHDOG(); // One bounded address probe has finished.
    }

    // Check if we actually found any connected devices
    if (!activeSensors.empty()) {
        // Print the module name followed by the message saying please wait
        LOG(F("== We found the following active Addresses =="));
        for (char address : activeSensors) {
            LOGF("    Address: %c", address);
        }
    } else {
        LOG(F("== No SDI-12 Devices Were Discovered == "));
    }
    return activeSensors;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_SDI12::checkActive(char addr) {
    // Attempt to contact the sensor 3 times
    char response[RESPONSE_SIZE];
    for (int i = 0; i < 3; i++) {
        memset(response, '\0', RESPONSE_SIZE);
        sendCommand(response, addr, "!");
        if (response[0] == addr && response[1] == '\0') {
            return true;
        }
    }

    sdiInterface.clearBuffer();
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<char> Loom_SDI12::getInUseAddresses() { return inUseAddresses; }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
const char *Loom_SDI12::getSensorInfo(char addr) {
    const int index = findSensorIndex(addr);
    return index >= 0 ? sensors[static_cast<size_t>(index)].type : "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
int Loom_SDI12::findSensorIndex(char addr) const {
    for (size_t i = 0; i < inUseAddresses.size() && i < sensors.size(); ++i) {
        if (inUseAddresses[i] == addr) {
            return static_cast<int>(i);
        }
    }
    return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::sendCommand(char response[RESPONSE_SIZE], char addr, const char *command) {
    if (response == nullptr) {
        return;
    }
    if (command == nullptr) {
        response[0] = '\0';
        return;
    }

    char output[25] = {};
    const int length = snprintf(output, sizeof(output), "%c%s", addr, command);
    response[0] = '\0'; // Build first so manual callers can reuse a buffer for command/response.
    if (length < 0 || static_cast<size_t>(length) >= sizeof(output)) {
        ERROR(F("SDI-12 command is too long; nothing sent."));
        return;
    }
    // An old service request or power-up DDI message must not become this command's response.
    sdiInterface.clearBuffer();
    sdiInterface.sendCommand(output);
    readResponse(response);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::readResponse(char response[RESPONSE_SIZE]) {
    if (response == nullptr) {
        return;
    }
    response[0] = '\0';
    size_t length = 0;
    bool carriageReturnSeen = false;
    const uint32_t started = millis();
    // At 1200 baud each byte takes about 8.33 ms. Poll the library's interrupt-fed buffer;
    // sleeping 20 ms after every character can fill it faster than we empty it.
    // The 100 ms first-byte and 1 s full-frame limits allow margin over the SDI-12 timings.
    while (static_cast<uint32_t>(millis() - started) < 1000) {
        const int available = sdiInterface.available();
        if (available < 0) {
            break; // The driver reports buffer overflow as -1, not as more available bytes.
        }
        if (available == 0) {
            if (length == 0 && !carriageReturnSeen &&
                static_cast<uint32_t>(millis() - started) >= 100) {
                break;
            }
            delay(1);
            continue;
        }
        const int character = sdiInterface.read();
        if (carriageReturnSeen) {
            if (character == '\n') {
                response[length] = '\0';
                // SDI-12 allows the sensor 7.5 ms (+0.4 ms tolerance) to release the wire.
                // Leave 10 ms before another command can drive it, including ttt=000 and D1!.
                delay(10);
                return; // Only a complete CR/LF-terminated response is accepted.
            }
            break;
        }
        if (character == '\r') {
            carriageReturnSeen = true;
        } else if (character < 32 || character > 126 || length >= RESPONSE_SIZE - 1) {
            break;
        } else {
            response[length++] = static_cast<char>(character);
        }
    }
    response[0] = '\0'; // Reject partial, oversized and corrupt frames instead of parsing a prefix.
    sdiInterface.clearBuffer();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::requestSensorInfo(char response[RESPONSE_SIZE], char addr) {
    sendCommand(response, addr, "I!");
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::getData(char addr) {
    const int sensorIndex = findSensorIndex(addr);
    for (float &value : sensorData) {
        value = NAN;
    }
    if (sensorIndex < 0) {
        ERROR(F("SDI-12 address has not been discovered; initialize before reading."));
        return;
    }
    SensorRecord &sensor = sensors[static_cast<size_t>(sensorIndex)];
    sensor.data.fill(NAN); // A failed read must not put yesterday's value beside today's timestamp.
    const loomSDI12::Model model = loomSDI12::identifyModel(sensor.type);
    const uint8_t expectedValues = loomSDI12::valueCount(model);
    if (expectedValues == 0) {
        return; // Do not label an unknown TEROS model as water content.
    }

    char response[RESPONSE_SIZE];
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        sendCommand(response, addr, "M!");
        uint16_t waitSeconds = 0;
        uint8_t reportedValues = 0;
        if (!loomSDI12::parseMeasurementReply(response, addr, waitSeconds, reportedValues) ||
            reportedValues != expectedValues || waitSeconds > maximumMeasurementWaitSeconds) {
            continue;
        }

        // M! starts a measurement; D0! does not start one. Sending D0! too soon can abort it.
        // SDI-12 permits waiting the full advertised ttt after the response's final LF, even
        // when an early service request arrives. Keep this wait bounded by the configured limit.
        {
            LoomWatchdogPause pause;
            delay(static_cast<uint32_t>(waitSeconds) * 1000UL);
        }

        std::array<float, loomSDI12::MAX_VALUES> readings;
        readings.fill(NAN);
        size_t count = 0;
        bool valid = true;
        for (uint8_t block = 0; block < 10 && count < reportedValues; ++block) {
            char command[] = "D0!";
            command[1] = static_cast<char>('0' + block);
            sendCommand(response, addr, command);
            if (!loomSDI12::appendDataReply(response, addr, readings.data(), reportedValues,
                                            count)) {
                valid = false;
                break;
            }
        }
        if (!valid || count != reportedValues) {
            continue;
        }

        // METER replies put water content/potential before temperature. Keep Loom's manual
        // getters and existing GS3/TER11/TER12 package order: temperature first, then soil value.
        const uint8_t depths = model == loomSDI12::Model::TER54 ? 4 : 1;
        for (uint8_t depth = 0; depth < depths; ++depth) {
            const float soilValue = readings[depth * 2];
            readings[depth * 2] = readings[depth * 2 + 1];
            readings[depth * 2 + 1] = soilValue;
        }
        sensor.data = readings;
        sensorData[0] = readings[0];
        sensorData[1] = readings[1];
        if (model == loomSDI12::Model::GS3 || model == loomSDI12::Model::TER12) {
            sensorData[2] = readings[2];
        }
        LOOM_FEED_WATCHDOG(); // A complete, validated measurement was collected.
        return;
    }
    ERRORF("SDI-12 address %c failed after three attempts; missing readings remain NaN/null.",
           addr);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_SDI12::setPowerUpDelay(uint32_t milliseconds) {
    if (milliseconds < 1000 || milliseconds > 60000) {
        return false;
    }
    powerUpDelayMs = milliseconds;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_SDI12::setMaximumMeasurementWait(uint16_t seconds) {
    if (seconds == 0 || seconds > 999) {
        return false;
    }
    maximumMeasurementWaitSeconds = seconds;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_SDI12::getTemperature(char address, uint8_t depth) const {
    const int index = findSensorIndex(address);
    if (index < 0 || depth == 0 || depth > 4) {
        return NAN;
    }
    const SensorRecord &sensor = sensors[static_cast<size_t>(index)];
    if (depth != 1 && loomSDI12::identifyModel(sensor.type) != loomSDI12::Model::TER54) {
        return NAN;
    }
    return sensor.data[(depth - 1) * 2];
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_SDI12::getWaterContent(char address, uint8_t depth) const {
    const int index = findSensorIndex(address);
    if (index < 0 || depth == 0 || depth > 4) {
        return NAN;
    }
    const SensorRecord &sensor = sensors[static_cast<size_t>(index)];
    const loomSDI12::Model model = loomSDI12::identifyModel(sensor.type);
    if (model == loomSDI12::Model::TER54) {
        return sensor.data[(depth - 1) * 2 + 1];
    }
    return NAN;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_SDI12::getMatricPotential(char address) const {
    const int index = findSensorIndex(address);
    if (index < 0) {
        return NAN;
    }
    const SensorRecord &sensor = sensors[static_cast<size_t>(index)];
    return loomSDI12::identifyModel(sensor.type) == loomSDI12::Model::TER21 ? sensor.data[1] : NAN;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
