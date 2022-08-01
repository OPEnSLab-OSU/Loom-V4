#pragma once

#include <vector>
#include <map>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * Used to read Analog voltages from the analog pins on the feather M0
 * 
 * @author Will Richards
 */ 
class Loom_Analog : public Module{
    protected:
        /* These aren't used by Analog */           
        void power_up() override {};
        void power_down() override {};
        void initialize() override {};  
        void print_measurements() override {};

    public:
        /**
         * Templated constructor that uses more than 1 analog pin
         * @param man Reference to the manager
         * @param firstPin First analog pin we want to read from
         * @param additionalPins Variable length argument allowing you to supply multiple pins
         */ 
        template<typename T, typename... Args>
        Loom_Analog(Manager& man, T firstPin , Args... additionalPins) : Module("Analog"){
           get_variadic_parameters(firstPin, additionalPins...);
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
        template<typename T>
        Loom_Analog(Manager& man, T firstPin) : Module("Analog"){
           analogPins.push_back(firstPin);
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
        Loom_Analog(Manager& man) : Module("Analog"){
           manInst = &man;

           // Set 12-bit analog read resolution
           analogReadResolution(12);

           // Register the module with the manager
           manInst->registerModule(this);
        };

        void measure() override;                               
        void package() override;

    private:

        /** 
         *   The following two functions are some sorcery to get the variadic parameters without the need for passing in a size variable
         *   I don't fully understand it so don't touch it just works
         *   Based off: https://eli.thegreenplace.net/2014/variadic-templates-in-c/
         */
        template<typename T>
        T get_variadic_parameters(T v) {
            analogPins.push_back(v);
            return v;
        };

        template<typename T, typename... Args>
        T get_variadic_parameters(T first, Args... args) {
           analogPins.push_back(first);
            return get_variadic_parameters(args...);
        };

        float get_battery_voltage();                // Get the current voltage of the battery
        String pin_number_to_name(int pin);         // Convert the given to a name with the style "A0"


        Manager* manInst;                           // Instance of the manager

        std::vector<int> analogPins;                // Holds a list of the analog pins we want to read
        std::map<String, float> pinToData;          // Map mapping analog pins to the data read from them
        

};