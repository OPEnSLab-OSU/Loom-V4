#include "Loom_Stepper.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Stepper::Loom_Stepper(Manager &man, int instance_num)
    : Actuator(ACTUATOR_TYPE::STEPPER, instance_num), manInst(&man), instance(instance_num) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Stepper::Loom_Stepper(int instance_num)
    : Actuator(ACTUATOR_TYPE::STEPPER, instance_num), instance(instance_num) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Stepper::~Loom_Stepper() { delete AFMS; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Stepper::initialize() {
    FUNCTION_START;

    if (AFMS == nullptr)
        AFMS = new Adafruit_MotorShield();
    if (AFMS == nullptr) {
        ERROR(F("Failed to allocate the stepper motor shield."));
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }

    // Get references to each motor
    motor = AFMS->getStepper(200, instance + 1);
    if (motor == nullptr) {
        ERROR(F("Invalid stepper motor port; expected instance 0 or 1."));
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }

    // Start the motor controller
    AFMS->begin();

    // Wait for init move
    yield();

    LOG(F("Stepper Initialized!"));
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Stepper::package(JsonObject json) {
    FUNCTION_START;
    json["Position"] = currentSteps;
    json["RPM"] = rpm;
    json["Direction"] = (clockwise ? "Counterclockwise" : "Clockwise");
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Stepper::control(JsonArray json) {
    FUNCTION_START;
    if (json.size() < 4) {
        ERROR(F("Stepper command requires instance, steps, speed, and direction."));
        FUNCTION_END;
        return;
    }
    moveSteps(json[1].as<uint16_t>(), json[2].as<uint8_t>(), json[3].as<bool>());
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Stepper::moveSteps(const uint16_t steps, const uint8_t speed, const bool clockwise) {
    FUNCTION_START;
    rpm = speed;
    this->clockwise = clockwise;

    if (motor == nullptr || !moduleInitialized) {
        ERROR(F("Cannot move an uninitialized stepper motor."));
        FUNCTION_END;
        return;
    }

    motor->setSpeed(speed);
    motor->step(steps, (clockwise) ? BACKWARD : FORWARD, SINGLE);

    // Wait for move to finish
    yield();

    // Tracks the current state of the motor
    if (clockwise)
        currentSteps = currentSteps - steps;
    else
        currentSteps = currentSteps + steps;

    LOGF("Stepper set to move %u steps at speed %u going %s", steps, speed,
         clockwise ? "counterclockwise" : "clockwise");
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
