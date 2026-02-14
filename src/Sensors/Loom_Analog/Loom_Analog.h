#pragma once

#include <map>
#include <vector>

#include "Loom_Manager.h"
#include "Module.h"

/* Contain all the information regarding the analog pin that we want to use*/
struct AnalogMapping {
    int pinNumber;
    const char *name;
    float analog;
    float analog_mv;

    /* Construct a new analog mapping */
    AnalogMapping(int pinNumber, const char *name, float analog, float analog_mv) {
        this->pinNumber = pinNumber;
        this->name = name;
        this->analog = analog;
        this->analog_mv = analog_mv;
    }
};

/**
 * Used to read Analog voltages from the analog pins on the feather M0
 *
 * @author Will Richards
 */
class Loom_Analog : public Module {
  protected:
    /* These aren't used by Analog */
    void power_up() override {};
    void power_down() override {};
    void initialize() override {};

  public:
    void measure() override;
    void package() override;

    /**
     * Templated constructor that uses more than 1 analog pin
     * @param man Reference to the manager
     * @param firstPin First analog pin we want to read from
     * @param additionalPins Variable length argument allowing you to supply multiple pins
     */
    template <typename T, typename... Args>
    Loom_Analog(Manager &man, T firstPin, Args... additionalPins) : Module("Analog") {
        get_variadic_parameters(firstPin, additionalPins...);
        pinMappings.push_back(
            new AnalogMapping(A7, "Vbat", getBatteryVoltage(), getBatteryVoltage() * 1000));
        manInst = &man;

        // Set 12-bit analog read resolution
        analogReadResolution(12);

        // Register the module with the manager
        manInst->registerModule(this);
    };

    /**
     * Templated constructor that uses only 1 analog pin
     * @param man Reference to the manager
     * @param firstPin First analog pin we want to read from
     */
    template <typename T> Loom_Analog(Manager &man, T firstPin) : Module("Analog") {
        pinMappings.push_back(new AnalogMapping(firstPin, pinNumberToName(firstPin), 0, 0));
        pinMappings.push_back(
            new AnalogMapping(A7, "Vbat", getBatteryVoltage(), getBatteryVoltage() * 1000));
        manInst = &man;

        // Set 12-bit analog read resolution
        analogReadResolution(12);

        // Register the module with the manager
        manInst->registerModule(this);
    };

    /**
     * Templated constructor that only reads the battery voltage
     * @param man Reference to the manager
     */
    Loom_Analog(Manager &man) : Module("Analog") {
        manInst = &man;
        pinMappings.push_back(
            new AnalogMapping(A7, "Vbat", getBatteryVoltage(), getBatteryVoltage() * 1000));

        // Set 12-bit analog read resolution
        analogReadResolution(12);

        // Register the module with the manager
        manInst->registerModule(this);
    };

    /**
     * Get the current voltage of the battery
     */
    static float getBatteryVoltage();

    /**
     * Get the Millivolts of a specified pin
     * @param pin The pin to get the data from eg. A0, A1, ...
     */
    float getMV(int pin);

    /**
     * Get the analog value from a given pin
     * @param pin The pin to get the data from eg. A0, A1, ...
     */
    float getAnalog(int pin);

  private:
    /**
     *   The following two functions are some sorcery to get the variadic parameters without the
     * need for passing in a size variable I don't fully understand it so don't touch it just works
     *   Based off: https://eli.thegreenplace.net/2014/variadic-templates-in-c/
     */
    template <typename T> T get_variadic_parameters(T v) {
        /* Push the pin number to vector */
        pinMappings.push_back(new AnalogMapping(v, pinNumberToName(v), 0, 0));
        return v;
    };

    template <typename T, typename... Args> T get_variadic_parameters(T first, Args... args) {
        pinMappings.push_back(new AnalogMapping(first, pinNumberToName(first), 0, 0));
        return get_variadic_parameters(args...);
    };

    float analogToMV(int analog);   // Convert the analog voltage to mV
    char *pinNumberToName(int pin); // Convert the given to a name with the style "A0"

    Manager *manInst;                         // Instance of the manager
    std::vector<AnalogMapping *> pinMappings; // Contains a struct for each pin we are monitoring
};