#include "Loom_Manager.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Manager::Manager(const char *devName, uint32_t instanceNum)
    : instanceNumber(instanceNum), doc(MAX_JSON_SIZE) {
    strncpy(deviceName, devName ? devName : "", sizeof(deviceName) - 1);
    deviceName[sizeof(deviceName) - 1] = '\0';
    // The three WISP variants register five to eight modules. Allocate the pointer table once
    // before other global constructors allocate long-lived objects, avoiding 4/8/16-byte holes.
    modules.reserve(8);
    Logger::getInstance();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::registerModule(Module *module) {
    if (module == nullptr) {
        ERROR(F("Cannot register a null module."));
        return;
    }

    char *location;
    // If there are no duplicates proceed as normal
    for (size_t i = 0; i < modules.size(); i++) {
        // Find the pointer to the module name
        location = strstr(modules[i]->getModuleName(), module->getModuleName());

        // Check if the module name contains the base string to make sure this works past 2 modules
        // of the same type
        if (location != NULL) {
            // Append the address to the name
            char modifiedName[MODULE_NAME_SIZE];

            // Format first module name
            snprintf_P(modifiedName, sizeof(modifiedName), PSTR("%s_%i"),
                       modules[i]->getModuleName(), modules[i]->module_address);
            modules[i]->setModuleName(modifiedName);

            // Format second string using the same array
            snprintf_P(modifiedName, sizeof(modifiedName), PSTR("%s_%i"), module->getModuleName(),
                       module->module_address);
            module->setModuleName(modifiedName);

            // Once we find a module of this type we want to break out to avoid redundant name
            // changes
            break;
        }
    }

    modules.push_back(module);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
DynamicJsonDocument &Manager::getDocument() { return doc; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::beginSerial(bool waitForSerial) {
    uint32_t startMillis = millis();

    Serial.begin(BAUD_RATE);
    // Pause if the Serial is not open and we want to wait
    while (!Serial && waitForSerial) {

        // If it has been 20 seconds break out of the loop
        if ((uint32_t)(millis() - startMillis) >= WAIT_TIME_MS) {
            break;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::measure() {
    FUNCTION_START;

    if (hasInitialized) {
        LOG(F("** Measuring **"));
        for (size_t i = 0; i < modules.size(); i++) {
            if (modules[i]->moduleInitialized)
                modules[i]->measure();
            else
                WARNINGF("%s Not initialized!", modules[i]->getModuleName());
            // TIMER_RESET;
        }
    } else {
        ERROR(F("Unable to collect data as the manager and thus all sensors connected to it have "
                "not been initialized! Call manager.initialize() to fix this."));
    }
    LOG(F("** Measuring Complete **"));
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::package() {
    FUNCTION_START;

    LOG(F("** Packaging **"));

    // Clear the document so that we don't get null characters after too many updates
    doc.clear();
    doc[F("type")] = F("data");
    doc["id"]["name"] = get_device_name();
    doc["id"]["instance"] = get_instance_num();

    // Get the contents of the JSON document
    contentsArray = doc["contents"];
    if (contentsArray.isNull())
        contentsArray = doc.createNestedArray("contents");

    // Add the packet number to the JSON document
    JsonObject json = get_data_object("Packet");
    json["Number"] = packetNumber;

    for (size_t i = 0; i < modules.size(); i++) {
        if (modules[i]->moduleInitialized) {
            modules[i]->package();
        } else {
            WARNINGF("%s Not initialized!", modules[i]->getModuleName());
        }
        // TIMER_RESET;
    }

    if (doc.overflowed()) {
        ERRORF("JSON document overflowed its %u-byte capacity; this packet is incomplete.",
               (unsigned int)doc.capacity());
    }
    packetNumber++;

    LOG(F("** Packaging Complete **"));
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
JsonObject Manager::get_data_object(const char *moduleName) {
    const char *safeModuleName = moduleName ? moduleName : "";

    // Check if the key already exists in the array
    for (JsonVariant value : contentsArray) {

        // If the data already exists
        const char *existingName = value.as<JsonObject>()["module"].as<const char *>();
        if (existingName != nullptr && strcmp(existingName, safeModuleName) == 0) {
            return value.as<JsonObject>()["data"];
        }
    }

    // If it doesn't already exist create a new object
    JsonObject json = contentsArray.createNestedObject();
    json["module"] = safeModuleName;
    return json.createNestedObject("data");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_up() {
    FUNCTION_START;
    WD_TIMER_ENABLE;
    for (size_t i = 0; i < modules.size(); i++) {
        WD_TIMER_RESET;
        if (modules[i]->moduleInitialized || modules[i]->retryPowerUpWhenUninitialized()) {
            // If we are about to power up the LTE we should turn off the watchdog
            if (strcmp(modules[i]->getModuleName(), "LTE") == 0) {
                WD_TIMER_DISABLE;
            }
            modules[i]->power_up();
        } else {
            WARNINGF("%s Not initialized!", modules[i]->getModuleName());
        }
        WD_TIMER_RESET;
    }

    // If we didn't already disable the timer from finding the LTE we should disable it now
    WD_TIMER_DISABLE;
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_down() {
    FUNCTION_START;
    for (size_t i = 0; i < modules.size(); i++) {
        if (modules[i]->moduleInitialized)
            modules[i]->power_down();
        else
            WARNINGF("%s Not initialized!", modules[i]->getModuleName());
        // TIMER_RESET;
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::display_data() {
    FUNCTION_START;
    if (!doc.isNull()) {

        // Display data for modules that support it
        for (size_t i = 0; i < modules.size(); i++) {
            modules[i]->display_data();
        }

        LOG(F("Data Json:"));
        // ArduinoJson can serialize to Print directly. Avoid a 2 KB stack array and avoid
        // duplicating the full payload into the optional SD debug log.
        serializeJsonPretty(doc, Serial);
        Serial.println();
    } else {
        LOG(F("JSON Document is Null there is no data to display"));
    }

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::initialize() {
    FUNCTION_START;
    // If you are using a hypnos board that has not been enabled, this needs to occur before
    // initializing sensors
    if (usingHypnos && !hypnosEnabled) {
        LOG(F("Your sketch is set to use a Hypnos board which has not been enabled before "
              "attempting to initialize sensors. \nThis will causing hanging please enable the "
              "board before initialization. Continuing but know this may cause issues!"));
    }

    LOG(F("** Initializing Modules **"));
    read_serial_num();
    for (size_t i = 0; i < modules.size(); i++) {
        modules[i]->initialize();
    }
    hasInitialized = true;
    LOG(F("** Setup Complete ** "));

    // TIMER_ENABLE;
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::getJSONString(char array[MAX_JSON_SIZE]) {
    // size_t jsonSize = measureJson(doc) + 1; // Retained for callers that need a measured size.
    serializeJson(doc, array, MAX_JSON_SIZE);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::read_serial_num() {
    char serial_no[33];
    // Serial numbers are made up of four words located at these specific registers (see datasheet)
    uint32_t sn_words[4];
    sn_words[0] = *(volatile uint32_t *)(0x0080A00C);
    sn_words[1] = *(volatile uint32_t *)(0x0080A040);
    sn_words[2] = *(volatile uint32_t *)(0x0080A044);
    sn_words[3] = *(volatile uint32_t *)(0x0080A048);

    // Take these raw values and convert them into a string of hex characters
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            const size_t offset = static_cast<size_t>(i * 8 + j * 2);
            snprintf_P(serial_no + offset, sizeof(serial_no) - offset, PSTR("%02X"),
                       (uint8_t)(sn_words[i] >> ((3 - j) * 8)));
        }
    }

    // Copy the contents of the calculated char array into the member variable
    strncpy(serial_num, serial_no, 33);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::pause(const uint32_t ms) const {
    // TIMER_DISABLE;
    const uint32_t startTime = millis();
    while ((uint32_t)(millis() - startTime) < ms)
        ;
    // TIMER_ENABLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
