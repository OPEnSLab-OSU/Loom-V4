#pragma once

#include <Loom_Manager.h>
#include "BenchRadio.h"
#ifndef BENCH_ENABLE_SWEEP
#define BENCH_ENABLE_SWEEP 0
#endif
#ifndef BENCH_ENABLE_DENDROMETER
#define BENCH_ENABLE_DENDROMETER (!BENCH_ENABLE_LTE)
#endif
#if BENCH_ENABLE_DENDROMETER && BENCH_ENABLE_LTE
#error "Dendrometer AS5311 clock and benchmark LTE power control both use A5."
#endif
#if BENCH_ENABLE_DENDROMETER
#include "BenchDendrometer.h"
#endif
#ifndef BENCH_RTC_BUTTON_PIN
#define BENCH_RTC_BUTTON_PIN 12
#endif
#if defined(ARDUINO_ARCH_SAMD) && !defined(__SAMD51__) && BENCH_RTC_BUTTON_PIN >= 0
#define BENCH_HAS_RTC_BUTTON 1
#include <ArduinoLowPower.h>
#include <Utilities/Loom_Watchdog.h>
#else
#define BENCH_HAS_RTC_BUTTON 0
#endif
#if BENCH_ENABLE_LTE
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#endif

namespace bench {

enum class Role : uint8_t { Hub, Node };

struct BootSettings {
    Role role;
    uint8_t nodeId, hubId, preferredHub;
    bool enrollment;
    const char *deviceName;
    uint8_t retries;

    constexpr BootSettings(Role role, uint8_t nodeId, uint8_t hubId, uint8_t preferredHub,
                           bool enrollment = false, const char *deviceName = nullptr,
                           uint8_t retries = MaxRetries)
        : role(role), nodeId(nodeId), hubId(hubId), preferredHub(preferredHub),
          enrollment(enrollment), deviceName(deviceName), retries(retries) {}

    constexpr bool valid() const {
        return nodeId <= MaxNodes && hubId >= 1 && hubId <= MaxHubs &&
               preferredHub >= 1 && preferredHub <= MaxHubs && retries <= MaxRetries;
    }
};

RH_RF95 radio(RFM95_CS, RFM95_INT);
CommandLine console;
bool ready = false;
Role role = Role::Node;
void (*prepareSensors)() = nullptr;

} // namespace bench

#include "BenchHub.h"
#include "BenchNode.h"
#if BENCH_ENABLE_SWEEP
#include "BenchSweep.h"
#endif
#if BENCH_HAS_RTC_BUTTON
#include "BenchSleep.h"
#endif

namespace bench {

uint8_t roleAddress() {
    return role == Role::Hub ? hubAddress(hub::hubId) : node::nodeId;
}

void printRole() {
    Serial.println(role == Role::Hub ? F("role=hub") : F("role=node"));
}

void activateRole() {
    printRole();
    if (role == Role::Hub) {
        hub::activate();
    } else {
        node::activate(prepareSensors);
    }
}

void setup(const BootSettings &boot, void (*sensorSetup)() = nullptr) {
    node::manager.beginSerial(false);
    if (boot.deviceName && boot.deviceName[0]) {
        node::manager.set_device_name(boot.deviceName);
        hub::manager.set_device_name(boot.deviceName);
    }
    const int deviceNumber = boot.nodeId ? boot.nodeId : node::manager.get_instance_num();
    node::preferredId = deviceNumber >= 1 && deviceNumber <= MaxNodes ?
                            static_cast<uint8_t>(deviceNumber) : 1;
    node::preferredHub = boot.preferredHub;
    hub::hubId = boot.hubId;
    hub::enrollment = boot.enrollment;
    hub::config.retries = boot.retries;
    if (!node::setEnrollment(boot.enrollment)) {
        node::setEnrollment(false);
    }
    prepareSensors = sensorSetup;
    role = boot.role;

#if BENCH_ENABLE_SWEEP
    digitalWrite(BENCH_SD_CS_PIN, HIGH);
    pinMode(BENCH_SD_CS_PIN, OUTPUT);
#endif
    ready = beginRadio(radio, roleAddress());
    Serial.println(ready ? F("RFM95 ready. Type help.") : F("RFM95 init failed. Check wiring."));
    activateRole();
#if BENCH_HAS_RTC_BUTTON
    setupSleepButton();
#endif
}

bool handleRoleCommand(char *line) {
    char *verb = line + strspn(line, " \t");
    if (strncmp(verb, "role", 4) || (verb[4] && verb[4] != ' ' && verb[4] != '\t')) {
        return false;
    }

    const char *arg = strtok(verb + 4, " \t");
    if (!arg) {
        printRole();
        return true;
    }
    if ((strcmp(arg, "hub") && strcmp(arg, "node")) || strtok(nullptr, " \t")) {
        Serial.println(F("ERR use role hub or role node"));
        return true;
    }

    const Role next = strcmp(arg, "hub") == 0 ? Role::Hub : Role::Node;
    if (next == role) {
        printRole();
        return true;
    }

#if BENCH_ENABLE_SWEEP
    sweep::stop("role_change");
#endif
    if (role == Role::Hub) {
        hub::deactivate();
    } else {
        node::deactivate();
    }
    role = next;
    if (ready) {
        // Discard a pending frame accepted for the previous address before switching.
        radio.recv(nullptr, nullptr);
        radio.setModeIdle();
        setRadioAddress(radio, roleAddress());
    }
    activateRole();
    return true;
}

void loop() {
#if BENCH_HAS_RTC_BUTTON
    if (serviceSleepButton()) {
        return;
    }
#endif
    if (char *line = console.read()) {
#if BENCH_ENABLE_SWEEP
        if (sweep::handleCommand(line)) return;
#endif
        if (!handleRoleCommand(line)) {
            if (role == Role::Hub) {
                hub::handleCommand(line);
            } else {
                node::handleCommand(line);
            }
        }
        return;
    }
    if (role == Role::Hub) {
        hub::loop();
#if BENCH_ENABLE_SWEEP
        sweep::service();
#endif
    } else {
        node::loop();
    }
}

} // namespace bench
