#pragma once

#include "Module.h"

#include <ArduinoJson.h>

enum ACTUATOR_TYPE { SERVO, STEPPER, RELAY, NEOPIXEL };

/**
 * All actuators eg. Servos, Steppers, etc. use this to allow for max control
 *
 * @author Will Richards
 */
class Actuator : public Module {
  protected:
    /* Module methods that are inherited by actuator */
    void measure() override {};

    void power_up() override {};
    void power_down() override {};
    void package() override {};

  public:
    Actuator(ACTUATOR_TYPE actType, int instance)
        : Module("Actuator"), instance_num(instance), type(actType) {
        char name[MODULE_NAME_SIZE];
        snprintf(name, sizeof(name), "%s%i", typeToString(), instance_num);
        setModuleName(name);
    };

    // Initializer
    virtual void initialize() = 0;
    virtual void package(JsonObject json) = 0;

    /**
     * Called when a packet is received that needs to move the actuator
     * @param json The parameters that can change
     */
    virtual void control(JsonArray json) = 0;

    void printModuleName(const char *message) override {
        Serial.print("[");
        Serial.print(getModuleName());
        Serial.print("] ");
        Serial.print(message ? message : "");
    };

    /**
     * Convert the actuator type to its retained text label.
     */
    const char *typeToString() {
        switch (type) {
        case SERVO:
            return "Servo";
        case STEPPER:
            return "Stepper";
        case RELAY:
            return "Relay";
        case NEOPIXEL:
            return "Neopixel";
        }
        return "Unknown";
    };

    /**
     * Get the instance number of the actuator
     */
    int get_instance_num() { return instance_num; };

  private:
    int instance_num;   // Instance number of the Actuator
    ACTUATOR_TYPE type; // Type of actuator
};
