#pragma once

#include "Actuators.h"

#include <Adafruit_MotorShield.h>
#include <Adafruit_PWMServoDriver.h>

#define SERVO_MIN 150   // Minimum pulse width
#define SERVO_MAX 600   // Maximum pulse width

class Loom_Stepper : public Actuator{
    public:
        Loom_Stepper(int instance_num);

        /**
         * Deconstructor to clean up motor controller pointers
         */  
        ~Loom_Stepper();

        void control(JsonArray json) override;
        void initialize() override;

        /**
         * Move the motor a set number of steps forward
         * @param steps Number of steps to move
         * @param speed The speed to move steps
         * @param clockwise If we want to rotate clockwise or counter clockwise
         */ 
        void moveSteps(const uint16_t steps, const uint8_t speed, const bool clockwise);

    private:
        Adafruit_MotorShield*	AFMS;   // Motor Shield controller
        Adafruit_StepperMotor* motor;   // Stepper controller

        int instance;                   // Instance number of the servo

       
};