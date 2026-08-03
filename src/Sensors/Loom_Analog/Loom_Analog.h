#pragma once

#include <map>
#include <vector>

#include "Loom_Manager.h"
#include "Module.h"

// Project-wide build flags may override these defaults. The LOOM_ prefix
// avoids collisions with common sketch macros such as VREF and VBATPIN.
#ifndef LOOM_ANALOG_BATTERY_PIN
#define LOOM_ANALOG_BATTERY_PIN A7
#endif
#ifndef LOOM_ANALOG_ADC_RESOLUTION_BITS
#define LOOM_ANALOG_ADC_RESOLUTION_BITS 12
#endif
#ifndef LOOM_ANALOG_ADC_REFERENCE_VOLTAGE
#define LOOM_ANALOG_ADC_REFERENCE_VOLTAGE 3.3f
#endif
#ifndef LOOM_ANALOG_BATTERY_DIVIDER_SCALE
#define LOOM_ANALOG_BATTERY_DIVIDER_SCALE 2.0f
#endif
#ifndef LOOM_ANALOG_BATTERY_SAMPLE_COUNT
#define LOOM_ANALOG_BATTERY_SAMPLE_COUNT 8
#endif
#ifndef LOOM_ANALOG_ADC_MAX_READING
#define LOOM_ANALOG_ADC_MAX_READING ((1UL << LOOM_ANALOG_ADC_RESOLUTION_BITS) - 1UL)
#endif

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
    Loom_Analog(Manager &man, T firstPin, Args... additionalPins)
        : Module("Analog"), manInst(&man) {
        analogReadResolution(adcResolutionBits);
        get_variadic_parameters(firstPin, additionalPins...);
        const float batteryVoltage = readBatteryVoltage();
        pinMappings.push_back(
            new AnalogMapping(batteryPin, "Vbat", batteryVoltage, batteryVoltage * 1000.0f));

        // Register the module with the manager
        manInst->registerModule(this);
    };

    /**
     * Templated constructor that uses only 1 analog pin
     * @param man Reference to the manager
     * @param firstPin First analog pin we want to read from
     */
    template <typename T> Loom_Analog(Manager &man, T firstPin) : Module("Analog"), manInst(&man) {
        analogReadResolution(adcResolutionBits);
        pinMappings.push_back(new AnalogMapping(firstPin, pinNumberToName(firstPin), 0, 0));
        const float batteryVoltage = readBatteryVoltage();
        pinMappings.push_back(
            new AnalogMapping(batteryPin, "Vbat", batteryVoltage, batteryVoltage * 1000.0f));

        // Register the module with the manager
        manInst->registerModule(this);
    };

    /**
     * Templated constructor that only reads the battery voltage
     * @param man Reference to the manager
     */
    Loom_Analog(Manager &man) : Module("Analog"), manInst(&man) {
        analogReadResolution(adcResolutionBits);
        const float batteryVoltage = readBatteryVoltage();
        pinMappings.push_back(
            new AnalogMapping(batteryPin, "Vbat", batteryVoltage, batteryVoltage * 1000.0f));

        // Register the module with the manager
        manInst->registerModule(this);
    };

    /**
     * Get the current voltage of the battery
     */
    static float getBatteryVoltage(int batteryPin = LOOM_ANALOG_BATTERY_PIN,
                                   uint8_t resolutionBits = LOOM_ANALOG_ADC_RESOLUTION_BITS,
                                   float referenceVoltage = LOOM_ANALOG_ADC_REFERENCE_VOLTAGE,
                                   float dividerScale = LOOM_ANALOG_BATTERY_DIVIDER_SCALE,
                                   uint8_t sampleCount = LOOM_ANALOG_BATTERY_SAMPLE_COUNT,
                                   uint32_t maxReading = LOOM_ANALOG_ADC_MAX_READING);

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

    float analogToMV(int analog); // Convert the analog voltage to mV
    float readBatteryVoltage() const;
    char *pinNumberToName(int pin); // Convert the given to a name with the style "A0"

    Manager *manInst;                         // Instance of the manager
    std::vector<AnalogMapping *> pinMappings; // Contains a struct for each pin we are monitoring
    int batteryPin = LOOM_ANALOG_BATTERY_PIN;
    uint8_t adcResolutionBits = LOOM_ANALOG_ADC_RESOLUTION_BITS;
    float adcReferenceVoltage = LOOM_ANALOG_ADC_REFERENCE_VOLTAGE;
    float batteryDividerScale = LOOM_ANALOG_BATTERY_DIVIDER_SCALE;
    uint8_t batterySampleCount = LOOM_ANALOG_BATTERY_SAMPLE_COUNT;
    uint32_t adcMaxReading = LOOM_ANALOG_ADC_MAX_READING;
};
