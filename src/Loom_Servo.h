#pragma once

#include "Actuators.h"
#include <Adafruit_PWMServoDriver.h> 

#define SERVO_MIN 150   // Minimum pulse width
#define SERVO_MAX 600   // Maximum pulse width

class Loom_Servo : public Actuator{
    public:
        Loom_Servo(int instance_num);

        void control(JsonArray json) override;
        void initialize() override;

    private:
        Adafruit_PWMServoDriver servo; // Instance of the Servo driver
        int instance;                   // Instance number of the servo

        void setDegrees(const int degrees); // Set the actual position of the servo
};