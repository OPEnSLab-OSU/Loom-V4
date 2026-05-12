#include "Heartbeat.h"

Heartbeat::Heartbeat(uint32_t normalWorkInterval)
    : normalWorkInterval(normalWorkInterval) {
}

bool Heartbeat::shouldPackageData() {
    if (normalWorkInterval <= 1 || currentInterval == 0 ||
        currentInterval >= normalWorkInterval) {
        currentInterval = 1;
        return true;
    }

    currentInterval++;
    return false;
}

void Heartbeat::package(
    DynamicJsonDocument &doc,
    const char *deviceName,
    uint32_t instanceNumber
) {
    doc.clear();
    doc[F("type")] = F("heartbeat");
    doc["id"]["name"] = deviceName;
    doc["id"]["instance"] = instanceNumber;
}
