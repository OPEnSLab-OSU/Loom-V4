#include "Loom_Stepper.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Stepper::Loom_Stepper(Manager& man, int instance_num) : Actuator(ACTUATOR_TYPE::STEPPER, instance_num), manInst(&man), instance(instance_num) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Stepper::Loom_Stepper(int instance_num) : Actuator(ACTUATOR_TYPE::STEPPER, instance_num), instance(instance_num) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Stepper::~Loom_Stepper(){
    delete AFMS;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Stepper::initialize(){
    FUNCTION_START;

    // Get references to each motor
    AFMS = new Adafruit_MotorShield();
    motor = AFMS->getStepper(200, instance+1);

    // Start the motor controller
    AFMS->begin();

    // Wait for init move
    yield();

    LOG("Stepper Initialized!");
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
void Loom_Stepper::control(JsonArray json){
    FUNCTION_START;
    moveSteps(json[1].as<uint16_t>(), json[2].as<uint8_t>(), json[3].as<bool>());
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Stepper::moveSteps(const uint16_t steps, const uint8_t speed, const bool clockwise){
    FUNCTION_START;
    rpm = speed;
    this->clockwise = clockwise;

    motor->setSpeed(speed); 
    motor->step(steps, (clockwise) ? BACKWARD : FORWARD, SINGLE);

    // Wait for move to finish
    yield();

    // Tracks the current state of the motor
    if(clockwise)
        currentSteps =  currentSteps - steps;
    else
        currentSteps =  currentSteps + steps;

    LOG("Stepper set to move " + String(steps) + " steps at speed " + String(speed) + " going " + (clockwise) ? "counterclockwise" : "clockwise"); 
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////