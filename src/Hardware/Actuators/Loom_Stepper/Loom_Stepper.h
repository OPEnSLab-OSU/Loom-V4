#pragma once

#include "Actuators.h"
#include "Loom_Manager.h"

#include <Adafruit_MotorShield.h>
#include <Adafruit_PWMServoDriver.h>

#define SERVO_MIN 150   // Minimum pulse width
#define SERVO_MAX 600   // Maximum pulse width

/**
 * Stepper Motor Control for Max
 * 
 * @author Will Richards
 */ 
class Loom_Stepper : public Actuator{
    public:

        /**
         * Construct a new 
         */ 
        Loom_Stepper(Manager& man, int instance_num = 0);

        Loom_Stepper(int instance_num = 0);

        /**
         * Deconstructor to clean up motor controller pointers
         */  
        ~Loom_Stepper();

        void control(JsonArray json) override;
        void initialize() override;
        void package(JsonObject json) override;

        /**
         * Move the motor a set number of steps forward
         * @param steps Number of steps to move
         * @param speed The speed to move steps
         * @param clockwise If we want to rotate clockwise or counter clockwise
         */ 
        void moveSteps(const uint16_t steps, const uint8_t speed, const bool clockwise = true);

    private:
        Manager* manInst = nullptr;     // Manager instance

        Adafruit_MotorShield*	AFMS;   // Motor Shield controller
        Adafruit_StepperMotor* motor;   // Stepper controller

        int instance;                   // Instance number of the servo

        int currentSteps;               // Running step count
        uint8_t rpm;                    // Current RPM of the motor
        bool clockwise;                 // If it is spinning clockwise

       
};