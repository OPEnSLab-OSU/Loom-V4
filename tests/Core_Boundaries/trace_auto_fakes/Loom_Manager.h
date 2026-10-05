#pragma once

// Startup fixtures model only readiness. Firmware builds exercise the real
// Manager::initialize() hook; native tests cannot read SAMD serial-number registers.
class Manager {
  public:
    bool initialized = false;
    bool isInitialized() const { return initialized; }
};
