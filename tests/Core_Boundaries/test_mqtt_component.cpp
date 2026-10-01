// Compile the actual frame/reconnect/retained-message implementation against explicit SDK returns.
// These cases do not emulate MQTT packets, a real ACK timeout, carrier state or Mongo ingestion.
#include <cassert>
#include <limits>
#include <string>
#include "Arduino.h"
#include "Adafruit_SleepyDog.h"
#include "ArduinoMqttClient.h"
#include "Internet/Logging/MQTTComponent/MQTTComponent.cpp"

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
    bool ready = true;
    int prepareCalls = 0;
    Client client;
    bool prepareConnection() override {
        ++prepareCalls;
        return ready;
    }
    bool isConnected() override { return ready; }
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
class TestBroker : public MQTTComponent {
  public:
    explicit TestBroker(FakeNetwork &network) : MQTTComponent("broker", network) {
        strcpy(address, "test.example");
        port = 1883;
    }
    using MQTTComponent::connectToBroker;
    using MQTTComponent::disconnectFromBroker;
    using MQTTComponent::deleteRetained;
    using MQTTComponent::getCurrentRetained;
    using MQTTComponent::publishDocument;
    using MQTTComponent::publishMessage;
    using MQTTComponent::publishStream;
    bool publish() override { return publishMessage("test", "sample"); }
    void loadConfigFromJSON(char *) override {}
    void measure() override {}
    void package() override {}
    void power_down() override {}
};
class MemoryStream : public Stream {
  public:
    explicit MemoryStream(size_t size) : bytes(size, 'x') {}
    std::string bytes;
    size_t offset = 0;
    bool readsWereGuarded = true;
    size_t readBytes(char *buffer, size_t count) override {
        readsWereGuarded = readsWereGuarded && WDT->CTRL.bit.ENABLE;
        const size_t actual = std::min(count, bytes.size() - offset);
        memcpy(buffer, bytes.data() + offset, actual);
        offset += actual;
        return actual;
    }
};
void resetPlan() {
    mqttPlan = MqttPlan{};
    nowMs = 0;
    watchdogRegisters.CTRL.bit.ENABLE = 1;
    watchdogRegisters.CTRL.bit.WEN = 1;
    watchdogRegisters.CONFIG.bit.PER = 7;
}
void checkGuard() {
    assert(WDT->CTRL.bit.ENABLE == 1);
    assert(WDT->CTRL.bit.WEN == 1 && WDT->CONFIG.bit.PER == 7);
}
int main() {
    FakeNetwork network;
    TestBroker broker(network);
    resetPlan();
    assert(broker.publishMessage("measurements", "sample", false, 1));
    assert(mqttPlan.declaredLength == 6 && mqttPlan.topic == "measurements");
    assert(std::string(mqttPlan.sent.begin(), mqttPlan.sent.end()) == "sample");
    assert(mqttPlan.endCalls == 1 && mqttPlan.stopCalls == 0);
    checkGuard();
    resetPlan();
    assert(broker.deleteRetained("config"));
    assert(mqttPlan.retain && mqttPlan.declaredLength == 0 && mqttPlan.endCalls == 1);
    resetPlan();
    broker.disconnectFromBroker();
    assert(mqttPlan.stopCalls == 1 && !mqttPlan.connected);
    checkGuard();

    // No frame may start for a bad topic/QoS, null payload or a boundary-sized payload.
    resetPlan();
    assert(!broker.publishMessage(nullptr, "x"));
    assert(!broker.publishMessage("", "x"));
    assert(!broker.publishMessage("test", nullptr));
    assert(!broker.publishMessage(std::string(MAX_TOPIC_LENGTH, 't').c_str(), "x"));
    assert(!broker.publishMessage("test", "x", false, -1));
    assert(!broker.publishMessage("test", "x", false, 3));
    assert(!broker.publishMessage("test", std::string(MAX_JSON_SIZE, 'x').c_str()));
    assert(!broker.publishMessage("test/+", "x"));
    assert(!broker.publishMessage("test/#", "x"));
    assert(!broker.publishMessage("test\nmisleading", "x"));
    assert(mqttPlan.beginCalls == 0);
    checkGuard();

    resetPlan();
    mqttPlan.beginResult = 0;
    assert(!broker.publish());
    assert(mqttPlan.stopCalls == 1 && mqttPlan.endCalls == 0 && mqttPlan.sent.empty());
    for (size_t failAt = 0; failAt < 6; ++failAt) {
        resetPlan();
        mqttPlan.writeBudget = failAt;
        assert(!broker.publish());
        assert(mqttPlan.stopCalls == 1 && mqttPlan.endCalls == 0);
        checkGuard();
    }
    resetPlan();
    mqttPlan.disconnectOnPoll = 1;
    assert(!broker.publish());
    assert(mqttPlan.beginCalls == 0 && mqttPlan.stopCalls == 1);
    checkGuard();

    // A failed completion discards the socket. The next attempt must prepare/reconnect it.
    resetPlan();
    mqttPlan.endResult = 0;
    assert(!broker.publish());
    assert(mqttPlan.stopCalls == 1 && !mqttPlan.connected);
    checkGuard();
    mqttPlan.endResult = 1;
    network.prepareCalls = 0;
    assert(broker.connectToBroker());
    assert(network.prepareCalls == 1 && mqttPlan.connectCalls == 1);
    assert(broker.publish());
    checkGuard();

    resetPlan();
    network.prepareCalls = 0;
    mqttPlan.disconnectOnPoll = 1;
    assert(broker.connectToBroker());
    assert(network.prepareCalls == 1 && mqttPlan.connectCalls == 1);
    checkGuard();
    resetPlan();
    network.ready = false;
    mqttPlan.connected = false;
    assert(!broker.connectToBroker());
    assert(mqttPlan.connectCalls == 0);
    network.ready = true;
    checkGuard();
    broker.setMaxRetries(2);
    resetPlan();
    mqttPlan.connected = false;
    mqttPlan.connectResult = 0;
    assert(!broker.connectToBroker());
    assert(mqttPlan.connectCalls == 2 && nowMs == 5000);
    checkGuard();
    resetPlan();
    mqttPlan.disconnectEveryPoll = true;
    assert(!broker.connectToBroker());
    assert(mqttPlan.connectCalls == 2 && nowMs == 5000);
    checkGuard();

    DynamicJsonDocument document(128);
    document["zero"] = 0;
    resetPlan();
    assert(broker.publishDocument("json", document));
    assert(mqttPlan.declaredLength == measureJson(document));
    assert(std::string(mqttPlan.sent.begin(), mqttPlan.sent.end()) == "{\"zero\":0}");
    DynamicJsonDocument overflow(1);
    overflow["cannot_fit"] = 12;
    resetPlan();
    assert(!broker.publishDocument("json", overflow));
    assert(mqttPlan.beginCalls == 0);
    for (size_t failAt = 0; failAt < measureJson(document); ++failAt) {
        resetPlan();
        mqttPlan.writeBudget = failAt;
        assert(!broker.publishDocument("json", document));
        assert(mqttPlan.stopCalls == 1 && mqttPlan.endCalls == 0);
        checkGuard();
    }
    resetPlan();
    MemoryStream complete(130);
    assert(broker.publishStream("stream", complete, 130));
    assert(complete.readsWereGuarded && mqttPlan.sent.size() == 130);
    checkGuard();
    resetPlan();
    MemoryStream truncated(63);
    assert(!broker.publishStream("stream", truncated, 130));
    assert(mqttPlan.stopCalls == 1 && mqttPlan.endCalls == 0 && truncated.readsWereGuarded);
    for (size_t failAt = 0; failAt < 130; ++failAt) {
        resetPlan();
        MemoryStream source(130);
        mqttPlan.writeBudget = failAt;
        assert(!broker.publishStream("stream", source, 130));
        assert(mqttPlan.stopCalls == 1 && mqttPlan.endCalls == 0 && source.readsWereGuarded);
        checkGuard();
    }
    char retained[8];
    resetPlan();
    mqttPlan.incoming = "payload";
    mqttPlan.readyOnParse = 3;
    mqttPlan.checkRetainedGuard = true;
    assert(broker.getCurrentRetained("config", retained, sizeof(retained)));
    assert(strcmp(retained, "payload") == 0 && nowMs == 20 && mqttPlan.unsubscribeCalls == 1);
    checkGuard();
    resetPlan();
    mqttPlan.incoming = "payload";
    mqttPlan.incomingTopic = "another-device/command";
    assert(!broker.getCurrentRetained("config", retained, sizeof(retained)));
    assert(retained[0] == '\0' && mqttPlan.readCalls == 0 && mqttPlan.stopCalls == 1);
    checkGuard();
    resetPlan();
    mqttPlan.checkRetainedGuard = true;
    mqttPlan.subscribeResult = false;
    assert(!broker.getCurrentRetained("config", retained, sizeof(retained)));
    assert(mqttPlan.stopCalls == 1 && mqttPlan.parseCalls == 0);
    checkGuard();
    resetPlan();
    mqttPlan.incoming = "payload";
    mqttPlan.unsubscribeResult = false;
    mqttPlan.checkRetainedGuard = true;
    assert(broker.getCurrentRetained("config", retained, sizeof(retained)));
    assert(mqttPlan.stopCalls == 1 && mqttPlan.unsubscribeCalls == 1);
    checkGuard();
    resetPlan();
    mqttPlan.incoming = "too-long";
    assert(!broker.getCurrentRetained("config", retained, sizeof(retained)));
    assert(mqttPlan.incomingOffset == 0 && mqttPlan.readCalls == 0 && mqttPlan.stopCalls == 1);
    checkGuard();
    resetPlan();
    mqttPlan.incoming = "payload";
    mqttPlan.readLimit = 3;
    assert(!broker.getCurrentRetained("config", retained, sizeof(retained)));
    assert(strcmp(retained, "pay") == 0 && mqttPlan.unsubscribeCalls == 0);
    assert(mqttPlan.stopCalls == 1);
    for (int invalidRead : {-1, 9}) {
        resetPlan();
        mqttPlan.incoming = "payload";
        mqttPlan.forcedReadResult = invalidRead;
        assert(!broker.getCurrentRetained("config", retained, sizeof(retained)));
        assert(retained[0] == '\0' && mqttPlan.stopCalls == 1);
        checkGuard();
    }
    resetPlan();
    nowMs = UINT32_MAX - 500;
    const uint32_t start = nowMs;
    assert(!broker.getCurrentRetained("config", retained, sizeof(retained)));
    assert(static_cast<uint32_t>(nowMs - start) == 2000 && retained[0] == '\0');
    checkGuard();
    resetPlan();
    WDT->CTRL.bit.ENABLE = 0;
    mqttPlan.incoming = "payload";
    assert(broker.getCurrentRetained("config", retained, sizeof(retained)));
    assert(WDT->CTRL.bit.ENABLE == 0); // Never turn on a watchdog the caller left disabled.
}
