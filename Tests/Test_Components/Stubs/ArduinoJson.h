#pragma once

#include <cstddef>

// A super simple stub of ArduinoJson needed for the tests.
// Add to this as your tests require more behavior.

// may need to implement more if tests plan to actually examine any JSON documents

class DynamicJsonDocument {
public:
    void clear() {}
};

class JsonVariantStub {
public:
    template<typename T>
    JsonVariantStub& operator=(const T&) { return *this; }
    operator int() const { return 0; }      // optional conversions
    operator float() const { return 0.0f; }
    operator const char*() const { return ""; }
};

class JsonObject {
public:
    JsonVariantStub operator[](const char*) { return JsonVariantStub(); }
};

class JsonArray {
public:
    void add(int) {}
};