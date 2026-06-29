#pragma once

#include "Loom_WarningGuards.h"
#include "Module.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoJson.h>
LOOM_EXTERNAL_INCLUDE_END

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
        
        void power_up() override {};
        void power_down() override {}; 
        void package() override {};

    public:
        Actuator(ACTUATOR_TYPE actType, int instance) : Module("Actuator") { 
            type = actType;
            instance_num = instance;
            snprintf(moduleName, 100, "%s%i", typeToString(), instance_num);
        };

        // Initializer
        virtual void initialize() = 0;
        virtual void package(JsonObject json) = 0;
    
        /**
         * Called when a packet is received that needs to move the actuator
         * @param json The parameters that can change 
         */ 
        virtual void control(JsonArray json) = 0;

        void printModuleName(const char* message) override { 
            Serial.print("[");
            Serial.print(moduleName);
            Serial.print("] ");
            Serial.print(message);
        };

        const char* getModuleName() override { return moduleName; };

        /**
         * Convert the type of actuator to a String
         */ 
        const char* typeToString(){
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
            return "Unknown";
        };

        /**
         * Get the instance number of the actuator
         */ 
        int get_instance_num() { return instance_num; };

    private:
        int instance_num;                   // Instance number of the Actuator
        ACTUATOR_TYPE type;                 // Type of actuator

        char moduleName[100];
};
