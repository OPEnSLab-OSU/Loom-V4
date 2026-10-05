#include "Loom_Manager.h"
#include "Logger.h"
#include "Utilities/Loom_ResetDiagnostics.h"

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
    for (Module *registered : modules) {
        if (registered == module) {
            WARNING(F("This module is already registered; keeping one measurement cycle."));
            return; // Reusing one object must not rename it or measure it twice.
        }
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
    FUNCTION_START(this);
    if (modulesIdle) {
        WARNING(F("Resume the idle modules before measuring."));
        return;
    }
    if (!hasInitialized) {
        ERROR(F("Unable to collect data as the manager and thus all sensors connected to it have "
                "not been initialized! Call manager.initialize() to fix this."));
    } else {
        LOG(F("** Measuring **"));
        for (Module *module : modules) {
            if (module->moduleInitialized) {
                FUNCTION_START(module, "Module dispatch: measure()");
                module->measure();
                LOOM_FEED_WATCHDOG(); // One module finished; a stalled next driver stays guarded.
            } else {
                WARNINGF("%s Not initialized!", module->getModuleName());
            }
        }
    }
    LOG(F("** Measuring Complete **"));
    FUNCTION_END;
    if (hasInitialized) {
        notifyHealth(HealthEvent::Measured);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::package() {
    FUNCTION_START(this);

    LOG(F("** Packaging **"));

    // Reuse the existing JSON pool; clearing it also invalidates views into the old packet.
    doc.clear();
    doc[F("type")] = F("data");
    doc["id"]["name"] = get_device_name();
    doc["id"]["instance"] = get_instance_num();

    doc.createNestedArray("contents");

    // Add the packet number to the JSON document
    JsonObject json = get_data_object("Packet");
    json["Number"] = packetNumber;

    for (Module *module : modules) {
        if (module->moduleInitialized || module->packageWhenUnavailable()) {
            FUNCTION_START(module, "Module dispatch: package()");
            module->package();
            LOOM_FEED_WATCHDOG(); // Feed after completed work, never during a blocked call.
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
    notifyHealth(HealthEvent::Packaged);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
JsonObject Manager::get_data_object(const char *moduleName) {
    const char *safeModuleName = moduleName ? moduleName : "";

    // getDocument() lets radios/sketches clear or replace the packet. Resolve this view from
    // the CURRENT document each time; a cached array could point into the previous packet.
    JsonArray contents = doc["contents"].as<JsonArray>();
    if (contents.isNull()) {
        contents = doc.createNestedArray("contents");
    }

    for (JsonObject moduleEntry : contents) {
        const char *existingName = moduleEntry["module"].as<const char *>();
        if (existingName != nullptr && strcmp(existingName, safeModuleName) == 0) {
            return moduleEntry["data"];
        }
    }

    JsonObject moduleEntry = contents.createNestedObject();
    moduleEntry["module"] = safeModuleName;
    return moduleEntry.createNestedObject("data");
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_up() { power_up(0); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_up(int wakeWatchdogMs) {
    FUNCTION_START(this);
    modulesIdle = false; // Full power-up supersedes retained-rail idle mode.
    if (wakeWatchdogMs > 0) {
        Watchdog.enable(wakeWatchdogMs);
    } else {
        WD_TIMER_ENABLE;
    }
    for (Module *module : modules) {
        LOOM_FEED_WATCHDOG();
        if (module->moduleInitialized || module->retryPowerUpWhenUninitialized()) {
            // Drivers own any temporary watchdog pause they need. LTE already restores its
            // caller's timer after startup, so the following sensor stays protected too.
            // Module labels are only for people/data; renaming one must not change protection.
            FUNCTION_START(module, "Module dispatch: power_up()");
            module->power_up();
        } else {
            WARNINGF("%s Not initialized!", module->getModuleName());
        }
        LOOM_FEED_WATCHDOG();
    }

    // Preserve the legacy compile-time timer policy. A positive wake guard stays enabled;
    // a sketch's runtime timer is left alone when WATCHDOG_ENABLE is not defined.
    if (wakeWatchdogMs <= 0) {
        WD_TIMER_DISABLE;
    }
    FUNCTION_END;
    notifyHealth(HealthEvent::PoweredUp);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_down() {
    FUNCTION_START(this);
    modulesIdle = false;
    for (Module *module : modules) {
        LOOM_FEED_WATCHDOG();
        if (module->moduleInitialized || !module->canRemovePower()) {
            FUNCTION_START(module, "Module dispatch: power_down()");
            module->power_down();
        } else {
            WARNINGF("%s Not initialized!", module->getModuleName());
        }
        LOOM_FEED_WATCHDOG();
    }
    FUNCTION_END;
    notifyHealth(HealthEvent::PoweredDown);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Manager::canRemovePower() const {
    for (Module *module : modules) {
        if (!module->canRemovePower()) {
            return false;
        }
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::idle() {
    FUNCTION_START(this);
    if (!hasInitialized || modulesIdle) {
        return;
    }
    for (Module *module : modules) {
        if (module->moduleInitialized) {
            FUNCTION_START(module, "Module dispatch: idle()");
            module->idle();
        }
        LOOM_FEED_WATCHDOG();
    }
    modulesIdle = true;
    FUNCTION_END;
    notifyHealth(HealthEvent::Idle);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::resume() {
    FUNCTION_START(this);
    if (!modulesIdle) {
        return;
    }
    for (Module *module : modules) {
        if (module->moduleInitialized) {
            FUNCTION_START(module, "Module dispatch: resume()");
            module->resume();
        }
        LOOM_FEED_WATCHDOG();
    }
    modulesIdle = false;
    FUNCTION_END;
    notifyHealth(HealthEvent::Resumed);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::display_data() {
    FUNCTION_START(this);
    if (!doc.isNull()) {

        // Display data for modules that support it
        for (Module *module : modules) {
            FUNCTION_START(module, "Module dispatch: display_data()");
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
    FUNCTION_START(this);
    notifyHealth(HealthEvent::BeforeInitialize);
    // If you are using a hypnos board that has not been enabled, this needs to occur before
    // initializing sensors
    if (usingHypnos && !hypnosEnabled) {
        LOG(F("Your sketch is set to use a Hypnos board which has not been enabled before "
              "attempting to initialize sensors. \nThis will causing hanging please enable the "
              "board before initialization. Continuing but know this may cause issues!"));
    }

    LOG(F("** Initializing Modules **"));
    read_serial_num();
    const uint8_t resetCause = loomReset::bootCause();
    LOGF("Boot reset cause: %s (raw flags 0x%02X).", loomReset::causeName(resetCause),
         static_cast<unsigned int>(resetCause));
    for (Module *module : modules) {
        module->initialize();
    }
    hasInitialized = true;
    LOG(F("** Setup Complete ** "));
    FUNCTION_END;
    notifyHealth(HealthEvent::Initialized);
    // Optional trace startup runs after module/SD initialization. It neither
    // replaces the health observer nor links a recorder into ordinary sketches.
    Logger::getInstance()->beginConfiguredTrace(*this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::notifyHealth(HealthEvent event) {
    if (healthObserver != nullptr) {
        healthObserver(*this, event, healthContext);
    }
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
