#pragma once
#include <ArduinoJson.h>
#include "Module.h"

// ThingSpeak reads only the packet timestamp. The real Manager is used by firmware builds.
class Manager {
  public:
    DynamicJsonDocument &getDocument() { return packet; }
    void registerModule(Module *) { ++registeredModules; }
    const char *get_device_name() { return "node"; }
    int get_instance_num() { return 7; }
    int registeredModules = 0;

  private:
    DynamicJsonDocument packet{2048};
};
