// Exercise the actual publisher, including its base MQTT framing, with explicit SDK return values.
// These tests verify bytes/callbacks/configuration; they do not contact ThingSpeak.
#include <cassert>
#include <climits>
#include <cstdlib>
#include <limits>
#include <string>
#include "Arduino.h"
#include "Adafruit_SleepyDog.h"
#include "ArduinoMqttClient.h"
#include "Internet/Logging/MQTTComponent/MQTTComponent.cpp"
#include "Internet/Logging/Loom_ThingSpeak/Loom_ThingSpeak.cpp"

FakeWatchdogRegisters watchdogRegisters;
FakeWatchdogRegisters *WDT = &watchdogRegisters;
FakeWatchdog Watchdog;
MqttPlan mqttPlan;
uint32_t nowMs = 0;
uint32_t millis() { return nowMs; }
void delay(uint32_t ms) { nowMs += ms; }

class FakeNetwork : public NetworkComponent {
  public:
    FakeNetwork() : NetworkComponent("network") {}
    Client client;
    bool prepareConnection() override { return true; }
    bool isConnected() override { return true; }
    bool getNetworkTime(int *, int *, int *, int *, int *, int *, float *) override {
        return false;
    }
    Client *getClient() override { return &client; }
    void initialize() override {}
    void measure() override {}
    void package() override {}
    void power_up() override {}
    void power_down() override {}
};
int plainCalls = 0, parameterCalls = 0;
float plainValue = 1.25f;
float readPlain() {
    ++plainCalls;
    return plainValue;
}
float readParameter(int value) {
    ++parameterCalls;
    return static_cast<float>(value);
}
std::string payload() { return std::string(mqttPlan.sent.begin(), mqttPlan.sent.end()); }
void resetPlan() {
    mqttPlan = MqttPlan{};
    plainCalls = parameterCalls = 0;
    plainValue = 1.25f;
}
int main() {
    Manager manager;
    FakeNetwork network;
    Loom_ThingSpeak publisher(manager, network, INT_MAX, "id", "user", "pass");
    // Register in the opposite order: the existing payload groups plain callbacks first.
    publisher.addFunction(2, readParameter, 17);
    publisher.addFunction(1, readPlain);
    manager.getDocument()["timestamp"]["time_local"] = "2026-09-30T01:02:03";
    resetPlan();
    assert(publisher.publish());
    assert(mqttPlan.topic == "channels/2147483647/publish");
    assert(payload() ==
           "field1=1.250000&field2=17.000000&created_at=2026-09-30T01:02:03&status=MQTTPUBLISH");
    assert(plainCalls == 1 && parameterCalls == 1);
    assert(mqttPlan.declaredLength == payload().size() && mqttPlan.qos == 0);

    // Never send a silently shortened timestamp or open a partial MQTT frame.
    const std::string longTime(150, 't');
    manager.getDocument()["timestamp"]["time_local"] = longTime;
    resetPlan();
    assert(!publisher.publish() && mqttPlan.beginCalls == 0 && mqttPlan.sent.empty());
    assert(plainCalls == 1 && parameterCalls == 1);

    // All eight maximum finite float values must still fit; a ninth registration is ignored.
    manager.getDocument().clear();
    Loom_ThingSpeak full(manager, network, 1, "id", "user", "pass");
    full.addFunction(0, readPlain);
    full.addFunction(9, readParameter, 9);
    full.addFunction(1, static_cast<FloatReturnFuncDefs>(nullptr));
    for (int field = 1; field <= 8; ++field) {
        full.addFunction(field, readPlain);
    }
    full.addFunction(1, readPlain);
    resetPlan();
    plainValue = -std::numeric_limits<float>::max();
    assert(full.publish() && plainCalls == 8 && parameterCalls == 0);
    assert(payload().find("field8=") != std::string::npos && payload().size() < 1024);

    // Loading only the documented four credentials must retain the default broker and port.
    Loom_ThingSpeak fromSD(manager, network);
    resetPlan();
    assert(!fromSD.publish() && mqttPlan.beginCalls == 0);
    const char config[] =
        "{\"channelID\":42,\"clientID\":\"id\",\"username\":\"user\",\"password\":\"pass\"}";
    char *ownedConfig = static_cast<char *>(malloc(sizeof(config)));
    assert(ownedConfig != nullptr);
    memcpy(ownedConfig, config, sizeof(config));
    fromSD.loadConfigFromJSON(ownedConfig); // Production API frees this caller-owned buffer.
    resetPlan();
    mqttPlan.connected = false;
    assert(fromSD.publish());
    assert(mqttPlan.host == "mqtt3.thingspeak.com" && mqttPlan.port == 1883);
    assert(mqttPlan.topic == "channels/42/publish" && payload() == "status=MQTTPUBLISH");
    // Streaming failures preserve the MQTT failure contract and never resample a callback.
    resetPlan();
    mqttPlan.writeBudget = 4;
    assert(!publisher.publish());
    assert(plainCalls == 1 && parameterCalls == 1 && mqttPlan.stopCalls > 0 &&
           mqttPlan.endCalls == 0);

    // Peeking does not consume text; reading ends at the measured byte length.
    loomThingSpeak::Payload stream;
    assert(stream.addField(1, -0.0f) && stream.prepare(nullptr));
    assert(stream.peek() == 'f' && stream.peek() == 'f');
    const size_t measured = stream.length();
    std::string streamed;
    while (stream.available() > 0) {
        streamed.push_back(static_cast<char>(stream.read()));
    }
    assert(streamed == "field1=-0.000000&status=MQTTPUBLISH" && streamed.size() == measured);
    assert(stream.peek() == -1 && stream.read() == -1 && stream.write('x') == 0);
}
