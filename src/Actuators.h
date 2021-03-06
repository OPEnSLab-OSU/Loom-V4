#pragma once

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
class Actuator{
    public:
        Actuator(ACTUATOR_TYPE actType, int instance) { 
            instance_num = instance; 
            type = actType;
        };
    
        /**
         * Called when a packet is received that needs to move the actuator
         * @param json The parameters that can change 
         */ 
        virtual void control(JsonArray json) = 0;
        virtual void initialize() = 0;

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
        void printModuleName() { Serial.print("[" + (typeToString() + String(instance_num)) + "] "); };

    private:
        int instance_num;                   // Instance number of the Actuator
        ACTUATOR_TYPE type;                 // Type of actuator
};