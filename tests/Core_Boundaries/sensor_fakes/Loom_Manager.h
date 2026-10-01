#pragma once
#include <ArduinoJson.h>
class Manager {
  public:
    StaticJsonDocument<2048> document;
    template <typename ModuleType> void registerModule(ModuleType *) {}
    JsonObject get_data_object(const char *name) {
        JsonObject object = document[name].as<JsonObject>();
        return object.isNull() ? document.createNestedObject(name) : object;
    }
};
