#pragma once

#include <algorithm>
#include <vector>

#include "Loom_Manager.h"
#include "Module.h"

/**
 * Used to read digital states from pins on the Feather M0
 *
 * @author Will Richards
 */
class Loom_Digital : public Module {
  protected:
    /* These aren't used by Digital */
    void power_up() override {};
    void power_down() override {};
    void initialize() override {};

  public:
    /**
     * Read one or more digital pins, sorted and deduplicated for stable JSON field order.
     *
     * Example: Loom_Digital digital(manager, INPUT_PULLUP, 5, 6).
     *
     * @param man Reference to the manager
     * @param pinState Arduino input mode, such as INPUT
     * or INPUT_PULLUP
     * @param firstPin First digital pin we want to read from
     * @param additionalPins Optional additional pin arguments; their count is known at compile
     * time
     */
    template <typename T, typename... Args>
    Loom_Digital(Manager &man, int pinState, T firstPin, Args... additionalPins)
        : Module("Digital"), manInst(&man) {
        digitalPins.reserve(sizeof...(additionalPins) + 1);
        const int pins[] = {static_cast<int>(firstPin), static_cast<int>(additionalPins)...};
        for (int pin : pins) {
            digitalPins.push_back(pin);
        }
        std::sort(digitalPins.begin(), digitalPins.end());
        digitalPins.erase(std::unique(digitalPins.begin(), digitalPins.end()), digitalPins.end());
        // Allocate one stored reading per retained pin during setup; measure() only updates it.
        pinData.resize(digitalPins.size());

        // Set pin mode on digital pins
        for (int pin : digitalPins) {
            pinMode(pin, pinState);
        }

        // Register the module with the manager
        manInst->registerModule(this);
    };

    void measure() override;
    void package() override;

  private:
    Manager *manInst;             // Instance of the manager
    std::vector<int> digitalPins; // Holds a list of the digital pins we want to read
    std::vector<int> pinData;     // Values aligned one-to-one with digitalPins
};
