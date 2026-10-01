#pragma once
#include <ArduinoJson.h>
#include "Module.h"

// ThingSpeak reads only the packet timestamp. The real Manager is used by firmware builds.
class Manager {
  public:
    static constexpr size_t DEVICE_NAME_SIZE = 64;
    DynamicJsonDocument &getDocument() { return packet; }
    bool isPacketValid() { return !packet.isNull() && !packet.overflowed(); }
    const char *get_serial_num() { return "00112233445566778899aabbccddeeff"; }
    void registerModule(Module *) { ++registeredModules; }
    const char *get_device_name() { return name.c_str(); }
    int get_instance_num() { return instance; }
    std::string name = "node";
    int instance = 7;
    int registeredModules = 0;

  private:
    DynamicJsonDocument packet{2048};
};
