#include "Loom_Manager.h"
#include "Logger.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
Manager::Manager(const char *devName, uint32_t instanceNum)
    : instanceNumber(instanceNum), doc(MAX_JSON_SIZE) {
    strncpy(deviceName, devName ? devName : "", sizeof(deviceName) - 1);
    deviceName[sizeof(deviceName) - 1] = '\0';
    // The three WISP variants register five to eight modules. Allocate the pointer table once
    // before other global constructors allocate long-lived objects, avoiding 4/8/16-byte holes.
    modules.reserve(8);
    Logger::getInstance();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::registerModule(Module *module) {
    if (module == nullptr) {
        ERROR(F("Cannot register a null module."));
        return;
    }

    // The packet needs distinct labels for modules of the same type. Match the base name even
    // when a previously registered module already has an address suffix, then label both.
    for (Module *registered : modules) {
        // Substring matching preserves the existing handling of earlier suffixed names.
        const char *location = strstr(registered->getModuleName(), module->getModuleName());

        if (location != nullptr) {
            // Append the address to the name
            char modifiedName[MODULE_NAME_SIZE];

            // Format first module name
            snprintf_P(modifiedName, sizeof(modifiedName), PSTR("%s_%i"),
                       registered->getModuleName(), registered->module_address);
            registered->setModuleName(modifiedName);

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
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
DynamicJsonDocument &Manager::getDocument() { return doc; }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::beginSerial(bool waitForSerial) {
    const uint32_t startMillis = millis();

    Serial.begin(BAUD_RATE);
    // USB Serial may never open on a deployed device, so waiting has a time limit.
    while (!Serial && waitForSerial) {

        if (static_cast<uint32_t>(millis() - startMillis) >= WAIT_TIME_MS) {
            break;
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::measure() {
    FUNCTION_START;
    if (!hasInitialized) {
        ERROR(F("Unable to collect data as the manager and thus all sensors connected to it have "
                "not been initialized! Call manager.initialize() to fix this."));
    } else {
        LOG(F("** Measuring **"));
        for (Module *module : modules) {
            if (module->moduleInitialized) {
                module->measure();
            } else {
                WARNINGF("%s Not initialized!", module->getModuleName());
            }
        }
    }
    LOG(F("** Measuring Complete **"));
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::package() {
    FUNCTION_START;

    LOG(F("** Packaging **"));

    // Reuse the existing JSON pool; clearing it also invalidates views into the old packet.
    doc.clear();
    doc[F("type")] = F("data");
    doc["id"]["name"] = get_device_name();
    doc["id"]["instance"] = get_instance_num();

    contentsArray = doc.createNestedArray("contents");

    // Add the packet number to the JSON document
    JsonObject json = get_data_object("Packet");
    json["Number"] = packetNumber;

    for (Module *module : modules) {
        if (module->moduleInitialized) {
            module->package();
        } else {
            WARNINGF("%s Not initialized!", module->getModuleName());
        }
    }

    if (doc.overflowed()) {
        ERRORF("JSON document overflowed its %u-byte capacity; this packet is incomplete.",
               (unsigned int)doc.capacity());
    }
    packetNumber++;

    LOG(F("** Packaging Complete **"));
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
JsonObject Manager::get_data_object(const char *moduleName) {
    const char *safeModuleName = moduleName ? moduleName : "";

    for (JsonObject moduleEntry : contentsArray) {
        const char *existingName = moduleEntry["module"].as<const char *>();
        if (existingName != nullptr && strcmp(existingName, safeModuleName) == 0) {
            return moduleEntry["data"];
        }
    }

    JsonObject moduleEntry = contentsArray.createNestedObject();
    moduleEntry["module"] = safeModuleName;
    return moduleEntry.createNestedObject("data");
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_up() { power_up(0); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_up(int wakeWatchdogMs) {
    FUNCTION_START;
    if (wakeWatchdogMs > 0) {
        Watchdog.enable(wakeWatchdogMs);
    } else {
        WD_TIMER_ENABLE;
    }
    for (Module *module : modules) {
        loomResetWatchdogIfEnabled();
        if (module->moduleInitialized || module->retryPowerUpWhenUninitialized()) {
            // LTE startup may legitimately take minutes. Limit the exception to that call,
            // then restore protection before the following sensor/SD module is touched.
            const bool isLTE = strcmp(module->getModuleName(), "LTE") == 0;
            if (isLTE) {
                if (wakeWatchdogMs > 0) {
                    Watchdog.disable();
                } else {
                    WD_TIMER_DISABLE;
                }
            }
            module->power_up();
            if (isLTE && wakeWatchdogMs > 0) {
                Watchdog.enable(wakeWatchdogMs);
            }
        } else {
            WARNINGF("%s Not initialized!", module->getModuleName());
        }
        loomResetWatchdogIfEnabled();
    }

    // If we didn't already disable the timer from finding the LTE we should disable it now
    if (wakeWatchdogMs <= 0) {
        WD_TIMER_DISABLE;
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_down() {
    FUNCTION_START;
    for (Module *module : modules) {
        if (module->moduleInitialized) {
            module->power_down();
        } else {
            WARNINGF("%s Not initialized!", module->getModuleName());
        }
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::display_data() {
    FUNCTION_START;
    if (!doc.isNull()) {

        // Display data for modules that support it
        for (Module *module : modules) {
            module->display_data();
        }

        LOG(F("Data Json: \n"));
        Logger::getInstance()->logDocument(doc);
    } else {
        LOG(F("JSON Document is Null there is no data to display"));
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
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
    for (Module *module : modules) {
        module->initialize();
    }
    hasInitialized = true;
    LOG(F("** Setup Complete ** "));
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::getJSONString(char array[MAX_JSON_SIZE]) { serializeJson(doc, array, MAX_JSON_SIZE); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::read_serial_num() {
    // Serial numbers are made up of four words located at these specific registers (see datasheet)
    const uint32_t serialWords[] = {
        *reinterpret_cast<volatile const uint32_t *>(0x0080A00C),
        *reinterpret_cast<volatile const uint32_t *>(0x0080A040),
        *reinterpret_cast<volatile const uint32_t *>(0x0080A044),
        *reinterpret_cast<volatile const uint32_t *>(0x0080A048),
    };

    // Each word becomes eight uppercase hex digits, most-significant byte first. Write into the
    // member buffer directly so no second 33-byte text buffer is needed on the stack.
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            const size_t offset = static_cast<size_t>(i * 8 + j * 2);
            snprintf_P(serial_num + offset, sizeof(serial_num) - offset, PSTR("%02X"),
                       static_cast<uint8_t>(serialWords[i] >> ((3 - j) * 8)));
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::pause(const uint32_t ms) const {
    const uint32_t startTime = millis();
    while ((uint32_t)(millis() - startTime) < ms) {
        // Preserve the busy wait; delay() would change scheduler/USB behavior.
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
