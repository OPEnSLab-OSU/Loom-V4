#pragma once

#include "Actuators.h"
#include "Loom_Manager.h"

#include <Adafruit_PWMServoDriver.h>

#define SERVO_MIN 150 // Minimum pulse width
#define SERVO_MAX 600 // Maximum pulse width

/**
 * Servo control with Max
 *
 * @author Will Richards
 */
class Loom_Servo : public Actuator {
  public:
    Loom_Servo(int instance_num);

    Loom_Servo(Manager &man, int instance_num);

    void control(JsonArray json) override;
    void initialize() override;
    void package(JsonObject json) override;

    /**
     * Set the degrees of the servo manually
     * @param degrees The angle to set the servo to
     */
    void setDegrees(const int degrees);

  private:
    Manager *manInst; // Instance of the manager

    Adafruit_PWMServoDriver servo; // Instance of the Servo driver
    int instance;                  // Instance number of the servo

    int degrees; // The expected current degrees of the servo
};