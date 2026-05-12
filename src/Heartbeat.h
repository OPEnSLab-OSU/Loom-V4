#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint>

class Heartbeat {
  public:
    Heartbeat(uint32_t normalWorkInterval);

    bool shouldPackageData();

    void package(
        DynamicJsonDocument &doc,
        const char *deviceName,
        uint32_t instanceNumber
    );

  private:
    uint32_t normalWorkInterval = 0;
    uint32_t currentInterval = 0;
};
