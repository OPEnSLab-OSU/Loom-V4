#pragma once

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

// One monitored pin: its packet label and the latest reading in both supported representations.
struct AnalogMapping {
    int pinNumber;
    char name[5];
    float analog;    // Raw ADC count for a normal pin; volts for the battery pin (legacy format).
    float analog_mv; // Voltage in millivolts for either kind of pin.

    /* Construct a new analog mapping */
    AnalogMapping(int pinNumber, const char *name, float analog, float analog_mv) {
        this->pinNumber = pinNumber;
        strncpy(this->name, name ? name : "", sizeof(this->name) - 1);
        this->name[sizeof(this->name) - 1] = '\0';
        this->analog = analog;
        this->analog_mv = analog_mv;
    }

    AnalogMapping(int pinNumber, float analog, float analog_mv) {
        this->pinNumber = pinNumber;
        snprintf(this->name, sizeof(this->name), "A%i", pinNumber - 14);
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
     * Read one or more analog pins in the supplied order, followed by battery voltage.
     *
     * Example: Loom_Analog analog(manager, A0, A1). The pin table is allocated during setup.
     *
     * @param man Reference to the manager
     * @param firstPin First analog pin we want to read from
     * @param additionalPins Optional additional pin arguments; their count is known at compile
     * time
     */
    template <typename T, typename... Args>
    Loom_Analog(Manager &man, T firstPin, Args... additionalPins)
        : Module("Analog"), manInst(&man) {
        analogReadResolution(adcResolutionBits);
        pinMappings.reserve(sizeof...(additionalPins) + 2);
        const int pins[] = {static_cast<int>(firstPin), static_cast<int>(additionalPins)...};
        for (int pin : pins) {
            pinMappings.emplace_back(pin, 0, 0);
        }
        const float batteryVoltage = readBatteryVoltage();
        pinMappings.emplace_back(batteryPin, "Vbat", batteryVoltage, batteryVoltage * 1000.0f);

        // Register the module with the manager
        manInst->registerModule(this);
    };

    /**
     * Read only battery voltage: Loom_Analog analog(manager).
     * @param man Reference to the
     * manager
     */
    Loom_Analog(Manager &man) : Module("Analog"), manInst(&man) {
        analogReadResolution(adcResolutionBits);
        pinMappings.reserve(1);
        const float batteryVoltage = readBatteryVoltage();
        pinMappings.emplace_back(batteryPin, "Vbat", batteryVoltage, batteryVoltage * 1000.0f);

        // Register the module with the manager
        manInst->registerModule(this);
    };

    /**
     * Read battery voltage in volts, averaging sampleCount ADC readings. The dividerScale
     *
     * accounts for the board's battery-divider circuit. A zero sample count/range returns zero.
 */
    static float getBatteryVoltage(int batteryPin = LOOM_ANALOG_BATTERY_PIN,
                                   uint8_t resolutionBits = LOOM_ANALOG_ADC_RESOLUTION_BITS,
                                   float referenceVoltage = LOOM_ANALOG_ADC_REFERENCE_VOLTAGE,
                                   float dividerScale = LOOM_ANALOG_BATTERY_DIVIDER_SCALE,
                                   uint8_t sampleCount = LOOM_ANALOG_BATTERY_SAMPLE_COUNT,
                                   uint32_t maxReading = LOOM_ANALOG_ADC_MAX_READING);

    /**
     * Get the most recently measured millivolts of a registered pin; an unknown pin returns zero.

     * * @param pin The pin to get the data from eg. A0, A1, ...
     */
    float getMV(int pin);

    /**
     * Get the latest ADC count for a registered pin, or volts for the battery pin.
     * An
     * unknown pin returns zero. Call measure() first when a fresh reading is needed.
     * @param
     * pin The pin to get the data from eg. A0, A1, ...
     */
    float getAnalog(int pin);

  private:
    float analogToMV(int analog); // Convert the analog voltage to mV
    float readBatteryVoltage() const;
    Manager *manInst;                       // Instance of the manager
    std::vector<AnalogMapping> pinMappings; // Contains a struct for each pin we are monitoring
    int batteryPin = LOOM_ANALOG_BATTERY_PIN;
    uint8_t adcResolutionBits = LOOM_ANALOG_ADC_RESOLUTION_BITS;
    float adcReferenceVoltage = LOOM_ANALOG_ADC_REFERENCE_VOLTAGE;
    float batteryDividerScale = LOOM_ANALOG_BATTERY_DIVIDER_SCALE;
    uint8_t batterySampleCount = LOOM_ANALOG_BATTERY_SAMPLE_COUNT;
    uint32_t adcMaxReading = LOOM_ANALOG_ADC_MAX_READING;
};
