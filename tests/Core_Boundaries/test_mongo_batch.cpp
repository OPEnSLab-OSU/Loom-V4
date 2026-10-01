// Compile actual Mongo routing/configuration/batch and MQTT framing against SD/SDK outcomes.
// This does not emulate a broker, database, modem or on-card crash recovery.
#include <cassert>
#include <cstdlib>
#include <limits>
#include "Arduino.h"
#include "Adafruit_SleepyDog.h"
#include "ArduinoMqttClient.h"
#include "Internet/Logging/MQTTComponent/MQTTComponent.cpp"
#include "Internet/Logging/Loom_MongoDB/Loom_MongoDB.cpp"

FakeWatchdogRegisters watchdogRegisters;
FakeWatchdogRegisters *WDT = &watchdogRegisters;
FakeWatchdog Watchdog;
MqttPlan mqttPlan;
FakeBatchPlan batchPlan;
float testBatteryVoltage = 4.0f;
uint32_t nowMs = 0;
uint32_t millis() { return nowMs; }
void delay(uint32_t ms) { nowMs += ms; }

class Network : public NetworkComponent {
  public:
    Network() : NetworkComponent("test") {}
    Client client;
    bool ready = true;
    bool prepareConnection() override { return ready; }
    bool isConnected() override { return ready; }
    bool getNetworkTime(int *, int *, int *, int *, int *, int *, float *) override { return false; }
    Client *getClient() override { return &client; }
    void initialize() override {}
    void measure() override {}
    void package() override {}
    void power_up() override {}
    void power_down() override {}
};
const std::string first = "{\"id\":{\"name\":\"alpha\",\"instance\":1},\"time_utc\":\"2026-10-01T09:00:00Z\",\"value\":0}";
const std::string second = "{\"id\":{\"name\":\"beta\",\"instance\":2},\"value\":12}";
void reset() {
    mqttPlan = MqttPlan{};
    batchPlan = FakeBatchPlan{};
    batchPlan.bytes = first + "\r\n" + second + "\n";
    testBatteryVoltage = 4.0f;
    watchdogRegisters.CTRL.bit.ENABLE = 1;
}
void load(Loom_MongoDB &mongo, const std::string &json) {
    char *owned = static_cast<char *>(malloc(json.size() + 1));
    assert(owned);
    memcpy(owned, json.c_str(), json.size() + 1);
    mongo.loadConfigFromJSON(owned); // Production owns/frees this mutable input.
}
int main() {
    Network network;
    Manager manager;
    Loom_BatchSD batch;
    Loom_MongoDB mongo(manager, network, "broker.example", 1883, "readings", "", "", "project");
    reset();
    assert(mongo.publish(batch));
    assert(mqttPlan.startedTopics == std::vector<std::string>({"project/readings/node7", "project/readings/node7"}));
    assert(mqttPlan.payloads == std::vector<std::string>({first, second}));
    assert(batchPlan.clearCalls == 1 && batchPlan.count == 0 && batchPlan.bytes.empty());
    assert(WDT->CTRL.bit.ENABLE);

    // Hub packets use their own identity, while ordinary legacy batches use the Manager topic.
    mongo.usePacketIdentityForBatch();
    reset();
    assert(mongo.publish(batch));
    assert(mqttPlan.startedTopics == std::vector<std::string>({"project/readings/alpha1", "project/readings/beta2"}));
    assert(mqttPlan.payloads == std::vector<std::string>({first, second}));
    Loom_MongoDB noProject(manager, network, "broker.example", 1883, "readings");
    reset();
    manager.getDocument()["value"] = 0;
    assert(noProject.publish());
    assert(mqttPlan.topic == "readings/node7");
    reset();
    DynamicJsonDocument heartbeat(512);
    assert(loomHeartbeat::buildStatusPayload(heartbeat, "node", 7));
    const size_t measurementBytes = measureJson(manager.getDocument());
    assert(mongo.publishHeartbeat(heartbeat));
    assert(mqttPlan.topic == "project/readings/node7" && mqttPlan.qos == 1);
    assert(mqttPlan.payloads[0].find("\"type\":\"heartbeat\"") != std::string::npos);
    assert(measureJson(manager.getDocument()) == measurementBytes);
    reset();
    mqttPlan.endResult = 0;
    assert(!mongo.publishMetadata(heartbeat) && mqttPlan.stopCalls == 1);
    heartbeat["id"]["name"] = "other";
    reset();
    assert(!mongo.publishHeartbeat(heartbeat) && mqttPlan.beginCalls == 0);
    heartbeat["id"]["name"] = "node";
    heartbeat["contents"].as<JsonArray>().add(12);
    assert(!mongo.publishHeartbeat(heartbeat) && mqttPlan.beginCalls == 0);

    // Losing the second ACK preserves the complete queue. Retry repeats the first record too:
    // QoS 1 is at-least-once, so the receiving service must handle duplicates.
    reset();
    const std::string original = batchPlan.bytes;
    mqttPlan.endResults = {1, 0};
    assert(!mongo.publish(batch));
    assert(batchPlan.clearCalls == 0 && batchPlan.bytes == original && batchPlan.count == 2);
    mqttPlan = MqttPlan{};
    assert(mongo.publish(batch));
    assert(mqttPlan.payloads == std::vector<std::string>({first, second}));
    assert(batchPlan.clearCalls == 1);

    for (int fault = 0; fault < 8; ++fault) {
        reset();
        if (fault == 0) { mqttPlan.beginResult = 0; }
        if (fault == 1) { mqttPlan.writeBudget = 10; }
        if (fault == 2) { batchPlan.openOK = false; }
        if (fault == 3) { batchPlan.seekOK = false; }
        if (fault == 4) { batchPlan.readError = 1; }
        if (fault == 5) { batchPlan.closeOK = false; }
        if (fault == 6) { batchPlan.count = 3; }
        if (fault == 7) { batchPlan.clearOK = false; }
        assert(!mongo.publish(batch));
        assert(batchPlan.bytes == original && batchPlan.count >= 2);
        assert(batchPlan.clearCalls == (fault == 7 ? 1 : 0));
        assert(WDT->CTRL.bit.ENABLE);
    }
    for (const std::string &bad : {
             std::string("{\"id\":{\"name\":\"alpha\",\"instance\":1}} garbage\n"),
             std::string("{\"id\":{\"name\":\"other/node\",\"instance\":1}}\n"),
             std::string("{\"id\":{\"name\":\"alpha\\u0000other\",\"instance\":1}}\n"),
             std::string("{\"id\":{\"name\":\"alpha\",\"instance\":\"1\"}}\n"),
             std::string("{\"id\":{\"name\":\"alpha\",\"instance\":-1}}\n"),
             std::string("{\"id\":{\"name\":\"alpha\",\"instance\":1}\n") + second + "\n",
             first, // No line terminator: possible torn SD append.
             std::string(MAX_JSON_SIZE, 'x') + "\n", std::string("\r\n\n")}) {
        reset();
        batchPlan.bytes = bad;
        assert(!mongo.publish(batch));
        assert(batchPlan.bytes == bad && batchPlan.clearCalls == 0 && mqttPlan.beginCalls == 0);
    }
    for (float voltage : {0.0f, 3.39f, std::numeric_limits<float>::quiet_NaN()}) {
        reset();
        testBatteryVoltage = voltage;
        assert(!mongo.publish(batch));
        assert(batchPlan.openCalls == 0 && mqttPlan.beginCalls == 0 && batchPlan.clearCalls == 0);
    }
    reset();
    batchPlan.count = 1;
    assert(!mongo.publish(batch));
    assert(batchPlan.openCalls == 0 && mqttPlan.connectCalls == 0);

    // Constructor and SD settings must fail rather than silently route to a truncated name.
    for (const std::string &database : {std::string(""), std::string(64, 'd'), std::string("db/other"), std::string("db+"), std::string("db#")}) {
        reset();
        Loom_MongoDB bad(manager, network, "broker.example", 1883, database.c_str());
        assert(!bad.moduleInitialized && !bad.publish());
        assert(mqttPlan.beginCalls == 0);
    }
    for (int port : {-1, 0, 65536}) {
        Loom_MongoDB bad(manager, network, "broker.example", port, "readings");
        assert(!bad.moduleInitialized);
    }
    Loom_MongoDB configured(manager, network);
    const std::string good = "{\"broker\":\"broker.example\",\"database\":\"readings\",\"port\":1883,\"project\":\"project\"}";
    load(configured, good);
    reset();
    assert(configured.moduleInitialized && configured.publish());
    assert(mqttPlan.topic == "project/readings/node7");
    for (const std::string &bad : {
             std::string("{\"broker\":\"broker.example\",\"database\":\"readings/other\",\"port\":1883}"),
             std::string("{\"broker\":\"broker.example\",\"database\":\"readings\\u0000other\",\"port\":1883}"),
             std::string("{\"broker\":\"broker.example\",\"database\":\"readings\",\"port\":1883,\"project\":12}"),
             std::string("{\"broker\":\"broker.example\",\"database\":\"readings\",\"port\":1883,\"username\":\"a\\u0000b\"}"),
             std::string("{\"broker\":\"broker.example\",\"database\":\"readings\",\"port\":\"1883\"}")}) {
        reset();
        load(configured, bad);
        assert(!configured.moduleInitialized && !configured.publish() && mqttPlan.beginCalls == 0);
    }
    load(configured, good);
    // Legacy custom records need no id, but malformed JSON must not be ACKed and then cleared.
    reset();
    batchPlan.bytes = "{\"zero\":0}\n{\"value\":12}  \r\n";
    assert(configured.publish(batch));
    for (const std::string &bad : {std::string("{\"value\":12} garbage\n"),
                                  std::string("{\"value\":12\n"), std::string("not JSON\n")}) {
        reset();
        batchPlan.bytes = bad;
        assert(!configured.publish(batch));
        assert(batchPlan.bytes == bad && batchPlan.clearCalls == 0 && mqttPlan.beginCalls == 0);
    }
    manager.name = "other/node";
    reset();
    assert(!configured.publish() && mqttPlan.beginCalls == 0);
}
