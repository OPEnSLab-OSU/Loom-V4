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
     * Templated constructor that uses more than 1 digital pin
     * @param man Reference to the manager
     * @param firstPin First digital pin we want to read from
     * @param additionalPins Variable length argument allowing you to supply multiple pins
     */
    template <typename T, typename... Args>
    Loom_Digital(Manager &man, int pinState, T firstPin, Args... additionalPins)
        : Module("Digital"), manInst(&man) {
        digitalPins.reserve(sizeof...(additionalPins) + 1);
        get_variadic_parameters(firstPin, additionalPins...);
        std::sort(digitalPins.begin(), digitalPins.end());
        digitalPins.erase(std::unique(digitalPins.begin(), digitalPins.end()), digitalPins.end());
        pinData.resize(digitalPins.size());

        // Set pin mode on digital pins
        for (size_t i = 0; i < digitalPins.size(); i++) {
            pinMode(digitalPins[i], pinState);
        }

        // Register the module with the manager
        manInst->registerModule(this);
    };

    /**
     * Templated constructor that uses only 1 digital pin
     * @param man Reference to the manager
     * @param firstPin First digital pin we want to read from
     */
    template <typename T>
    Loom_Digital(Manager &man, int pinState, T firstPin) : Module("Digital"), manInst(&man) {
        digitalPins.reserve(1);
        digitalPins.push_back(firstPin);
        pinData.resize(1);

        for (size_t i = 0; i < digitalPins.size(); i++) {
            pinMode(digitalPins[i], pinState);
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

    /**
     *   The following two functions are some sorcery to get the variadic parameters without the
     * need for passing in a size variable I don't fully understand it so don't touch it just works
     *   Based off: https://eli.thegreenplace.net/2014/variadic-templates-in-c/
     */
    template <typename T> T get_variadic_parameters(T v) {
        digitalPins.push_back(v);
        return v;
    };

    template <typename T, typename... Args> T get_variadic_parameters(T first, Args... args) {
        digitalPins.push_back(first);
        return get_variadic_parameters(args...);
    };
};
