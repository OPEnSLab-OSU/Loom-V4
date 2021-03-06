#include "Loom_Relay.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Relay::Loom_Relay(const byte controlPin) : Actuator(ACTUATOR_TYPE::RELAY, 0), pin(controlPin) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Relay::control(JsonArray json){
    digitalWrite(pin , (json[0].as<bool>()) ? HIGH : LOW);
    printModuleName(); Serial.println("Relay pin is set to: " + String((json[0].as<bool>()) ? "HIGH" : "LOW"));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////