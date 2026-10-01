#pragma once
#include "Arduino.h"
class Module {
  public:
    explicit Module(const char *name) : name(name) {}
    virtual ~Module() = default;
    virtual void initialize() = 0;
    virtual void measure() = 0;
    virtual void package() = 0;
    virtual void power_up() = 0;
    virtual void power_down() = 0;
    const char *getModuleName() const { return name; }

  protected:
    bool moduleInitialized = true;
    int module_address = -1;

  private:
    const char *name;
};
