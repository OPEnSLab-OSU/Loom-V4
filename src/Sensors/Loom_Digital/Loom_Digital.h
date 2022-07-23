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
class Loom_Digital : public Module{
    protected:
        /* These aren't used by Analog */           
        void power_up() override {};
        void power_down() override {};
        void initialize() override {};  
        void print_measurements() override {};

    public:
        /**
         * Templated constructor that uses more than 1 digital pin
         * @param man Reference to the manager
         * @param firstPin First digital pin we want to read from
         * @param additionalPins Variable length argument allowing you to supply multiple pins
         */ 
        template<typename T, typename... Args>
        Loom_Digital(Manager& man, T firstPin , Args... additionalPins) : Module("Digital"){
           get_variadic_parameters(firstPin, additionalPins...);
           manInst = &man;

           // Register the module with the manager
           manInst->registerModule(this);
        };

        /**
         * Templated constructor that uses only 1 digital pin
         * @param man Reference to the manager
         * @param firstPin First digital pin we want to read from
         */ 
        template<typename T>
        Loom_Digital(Manager& man, T firstPin) : Module("Digital"){
           digitalPins.push_back(firstPin);
           manInst = &man;

           // Register the module with the manager
           manInst->registerModule(this);
        };

        void measure() override;                               
        void package() override;

    private:
        Manager* manInst;                           // Instance of the manager
        std::vector<int> digitalPins;               // Holds a list of the digital pins we want to read
        std::map<int, int> pinToData;          // Map pin number to pin data   

        /** 
         *   The following two functions are some sorcery to get the variadic parameters without the need for passing in a size variable
         *   I don't fully understand it so don't touch it just works
         *   Based off: https://eli.thegreenplace.net/2014/variadic-templates-in-c/
         */
        template<typename T>
        T get_variadic_parameters(T v) {
            digitalPins.push_back(v);
            return v;
        };

        template<typename T, typename... Args>
        T get_variadic_parameters(T first, Args... args) {
           digitalPins.push_back(first);
            return get_variadic_parameters(args...);
        };
       

};