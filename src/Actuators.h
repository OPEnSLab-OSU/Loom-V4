#pragma once

#include "Module.h"

#include <ArduinoJson.h>

enum ACTUATOR_TYPE{
    SERVO,
    STEPPER,
    RELAY,
    NEOPIXEL
};

/**
 * All actuators eg. Servos, Steppers, etc. use this to allow for max control
 * 
 * @author Will Richards
 */ 
class Actuator : public Module{
    protected:
        /* Module methods that are inherited by actuator */
        void measure() override {};
        void print_measurements() override {};  
        void power_up() override {};
        void power_down() override {}; 
        void package() override {};

    public:
        Actuator(ACTUATOR_TYPE actType, int instance) : Module("Actuator") { 
            type = actType;
            instance_num = instance;
        };

        // Initializer
        virtual void initialize() = 0;
        virtual void package(JsonObject json) = 0;
    
        /**
         * Called when a packet is received that needs to move the actuator
         * @param json The parameters that can change 
         */ 
        virtual void control(JsonArray json) = 0;

        void printModuleName() override { Serial.print("[" + (typeToString() + String(instance_num)) + "] "); };

        String getModuleName() override { return (typeToString() + String(instance_num)); };

        /**
         * Convert the type of actuator to a String
         */ 
        String typeToString(){
            switch(type){
                case SERVO:
                    return "Servo";
                case STEPPER:
                    return "Stepper";
                case RELAY:
                    return "Relay";
                case  NEOPIXEL:
                    return "Neopixel";
            }
        };

        /**
         * Get the instance number of the actuator
         */ 
        int get_instance_num() { return instance_num; };

    private:
        int instance_num;                   // Instance number of the Actuator
        ACTUATOR_TYPE type;                 // Type of actuator
};