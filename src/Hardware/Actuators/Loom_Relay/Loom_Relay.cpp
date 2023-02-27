#include "Loom_Relay.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Relay::Loom_Relay(const byte controlPin) : Actuator(ACTUATOR_TYPE::RELAY, 0), pin(controlPin) {
    pinMode(controlPin, OUTPUT);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Relay::Loom_Relay(Manager& man, const byte controlPin) : Actuator(ACTUATOR_TYPE::RELAY, 0), pin(controlPin), manInst(&man) {
    pinMode(controlPin, OUTPUT);
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Relay::control(JsonArray json){
    FUNCTION_START;
    // Update the state
    state = json[0].as<bool>();
    digitalWrite(pin , state ? HIGH : LOW);
    LOG("Relay pin is set to: " + String(state ? "HIGH" : "LOW"));
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Relay::package(JsonObject json) {
    FUNCTION_START;
    json["State"] = String(state ? "HIGH" : "LOW");
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Relay::setState(bool state){
    FUNCTION_START;
    digitalWrite(pin , state ? HIGH : LOW);
    LOG("Relay pin is set to: " + String(state ? "HIGH" : "LOW"));
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////