#include "../../../Loom_Servo.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Servo::Loom_Servo(int instance_num) : Actuator(ACTUATOR_TYPE::SERVO, instance_num), instance(instance_num) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Servo::initialize(){

    /* Initialize the servo driver*/
    servo.begin();
    servo.setPWMFreq(60);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Servo::control(JsonArray json){
    setDegrees(json[1].as<int>());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Servo::setDegrees(const int degrees){
    servo.setPWM(instance, 0, map(degrees, 0, 180, SERVO_MIN, SERVO_MAX));
    printModuleName(); Serial.println("Servo set to: " + String(degrees));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////