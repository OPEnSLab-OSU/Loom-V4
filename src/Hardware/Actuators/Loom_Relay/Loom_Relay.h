#pragma once

#include "Actuators.h"
#include "Loom_Manager.h"

/**
 * Relay controls for integration with Max
 *
 * @author Will Richards
 */
class Loom_Relay : public Actuator {
  public:
    Loom_Relay(const byte controlPin = 10);

    /**
     * Construct a new object to manually control the relay
     *
     * @param man Instance of the Manager
     */
    Loom_Relay(Manager &man, const byte controlPin = 10);

    void control(JsonArray json) override;
    void initialize() override {};
    void package(JsonObject json) override;

    void printModuleName(const char *message) override {
        char output[OUTPUT_SIZE];
        snprintf(output, OUTPUT_SIZE, "[%s] %s", moduleName, message);
        Serial.print(output);
    };

    const char *getModuleName() override { return moduleName; };

    /**
     * Set the state of the relay
     *
     * @param state New state of the relay
     */
    void setState(bool state);

  private:
    Manager *manInst;
    byte pin;
    bool state = false;

    char moduleName[100];
};