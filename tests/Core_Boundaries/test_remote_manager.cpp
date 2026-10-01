// Execute the production retained-command parser and acknowledgement decisions with fake RTC
// outcomes. Real alarm writes/readback and network time synchronization still require hardware.
#include <cassert>
#include <string>
#include "Arduino.h"
#include "Adafruit_SleepyDog.h"
#include "ArduinoMqttClient.h"
#include "Internet/Logging/MQTTComponent/MQTTComponent.cpp"
#include "Internet/Communication/Loom_RemoteManager/Loom_RemoteManager.cpp"

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
bool intervalAcknowledged() {
    return std::find(mqttPlan.startedTopics.begin(), mqttPlan.startedTopics.end(),
                     "RemoteManager/node7/Hypnos/setSleepInterval") != mqttPlan.startedTopics.end();
}
int main() {
    Manager manager;
    FakeNetwork network;
    Loom_Hypnos hypnos;
    Loom_RemoteManager remote(manager, network, "test.example", 1883);
    remote.setHypnosInstance(hypnos);
    mqttPlan.incoming = "{\"minutes\":5}";
    assert(remote.publish() && hypnos.scheduleCalls == 1 && hypnos.lastInterval == 300);
    assert(intervalAcknowledged());
    assert(std::string(mqttPlan.sent.begin(), mqttPlan.sent.end()) ==
           "{\"online\":true}{\"online\":false}");

    // A failed verified alarm must not consume the command, even if status publication succeeds.
    mqttPlan = MqttPlan{};
    mqttPlan.incoming = "{\"seconds\":10}";
    hypnos.alarmReady = false;
    assert(remote.publish() && hypnos.scheduleCalls == 2 && !intervalAcknowledged());
    hypnos.alarmReady = true;

    const char *invalid[] = {"{}",
                             "[]",
                             "{\"seconds\":null}",
                             "{\"seconds\":true}",
                             "{\"seconds\":\"10\"}",
                             "{\"seconds\":1.5}",
                             "{\"seconds\":-1}",
                             "{\"hours\":24}",
                             "{\"minutes\":60}",
                             "{\"days\":24855}",
                             "{\"days\":28}",
                             "{\"days\":27,\"seconds\":1}",
                             "{\"seconds\":4294967297}",
                             "{\"seconds\":9223372036854775807}"};
    for (const char *text : invalid) {
        mqttPlan = MqttPlan{};
        mqttPlan.incoming = text;
        const int before = hypnos.scheduleCalls;
        assert(remote.publish());
        assert(hypnos.scheduleCalls == before && !intervalAcknowledged());
    }
    mqttPlan = MqttPlan{};
    mqttPlan.incoming = "{\"days\":27}";
    assert(remote.publish() && intervalAcknowledged() && hypnos.lastInterval == 2332800);

    // Failed parsing must not partially overwrite an existing interval.
    StaticJsonDocument<256> json;
    assert(!deserializeJson(json, "{\"days\":1,\"seconds\":-1}"));
    int32_t seconds = 123;
    assert(!loomRemote::parseSleepInterval(json.as<JsonObjectConst>(), seconds) && seconds == 123);
    Loom_RemoteManager unconfigured(manager, network);
    mqttPlan = MqttPlan{};
    assert(!unconfigured.publish() && mqttPlan.beginCalls == 0);
}
