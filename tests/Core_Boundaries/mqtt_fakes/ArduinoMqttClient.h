#pragma once
#include "Client.h"
#include <limits>
#include <string>
#include <vector>
#include <cassert>

// Model the SDK's observable returns only. No sockets, broker, deadlines or packet parser.
struct MqttPlan {
    bool connected = true;
    bool disconnectEveryPoll = false;
    int disconnectOnPoll = -1;
    int connectResult = 1;
    int beginResult = 1;
    int endResult = 1;
    size_t writeBudget = std::numeric_limits<size_t>::max();
    std::vector<uint8_t> sent;
    std::string topic;
    std::vector<std::string> startedTopics;
    std::vector<std::string> payloads;
    std::vector<int> endResults;
    std::string host;
    int port = 0;
    unsigned long declaredLength = 0;
    bool retain = false;
    int qos = -1;
    int pollCalls = 0, connectCalls = 0, stopCalls = 0, beginCalls = 0, endCalls = 0;
    std::string incoming;
    std::string incomingTopic;
    std::string subscribedTopic;
    size_t incomingOffset = 0;
    size_t readLimit = std::numeric_limits<size_t>::max();
    int forcedReadResult = -2; // -2 uses normal bytes; -1 models a signed read error.
    int readCalls = 0;
    int advertisedLength = -1;
    int readyOnParse = 1, parseCalls = 0, unsubscribeCalls = 0;
    bool subscribeResult = true;
    bool unsubscribeResult = true;
    bool checkRetainedGuard = false;
};
extern MqttPlan mqttPlan;

class MqttClient {
  public:
    explicit MqttClient(Client *) {}
    void poll() {
        ++mqttPlan.pollCalls;
        if (mqttPlan.disconnectEveryPoll || mqttPlan.pollCalls == mqttPlan.disconnectOnPoll) {
            mqttPlan.connected = false;
        }
    }
    bool connected() { return mqttPlan.connected; }
    void stop() {
        ++mqttPlan.stopCalls;
        mqttPlan.connected = false;
    }
    int connect(const char *host, int port) {
        mqttPlan.host = host;
        mqttPlan.port = port;
        ++mqttPlan.connectCalls;
        mqttPlan.connected = mqttPlan.connectResult == 1;
        return mqttPlan.connectResult;
    }
    int beginMessage(const char *topic, unsigned long length, bool retain, int qos) {
        ++mqttPlan.beginCalls;
        mqttPlan.topic = topic;
        mqttPlan.startedTopics.push_back(topic);
        mqttPlan.payloads.emplace_back();
        mqttPlan.declaredLength = length;
        mqttPlan.retain = retain;
        mqttPlan.qos = qos;
        return mqttPlan.beginResult;
    }
    size_t write(const uint8_t *data, size_t count) {
        const size_t written = std::min(count, mqttPlan.writeBudget);
        mqttPlan.sent.insert(mqttPlan.sent.end(), data, data + written);
        if (!mqttPlan.payloads.empty()) {
            mqttPlan.payloads.back().append(reinterpret_cast<const char *>(data), written);
        }
        mqttPlan.writeBudget -= written;
        return written;
    }
    size_t write(uint8_t value) { return write(&value, 1); }
    int endMessage() {
        ++mqttPlan.endCalls;
        return static_cast<size_t>(mqttPlan.endCalls) <= mqttPlan.endResults.size()
                   ? mqttPlan.endResults[mqttPlan.endCalls - 1] : mqttPlan.endResult;
    }
    void setKeepAliveInterval(unsigned long) {}
    void setId(const char *) {}
    void setUsernamePassword(const char *, const char *) {}
    void setCleanSession(bool) {}
    int connectError() { return -1; }
    bool subscribe(const char *topic, int) {
        if (mqttPlan.checkRetainedGuard) { assert(!WDT->CTRL.bit.ENABLE); }
        mqttPlan.subscribedTopic = topic;
        return mqttPlan.subscribeResult;
    }
    bool unsubscribe(const char *) {
        ++mqttPlan.unsubscribeCalls;
        if (mqttPlan.checkRetainedGuard) { assert(!WDT->CTRL.bit.ENABLE); }
        return mqttPlan.unsubscribeResult;
    }
    std::string messageTopic() {
        return mqttPlan.incomingTopic.empty() ? mqttPlan.subscribedTopic : mqttPlan.incomingTopic;
    }
    int parseMessage() {
        if (mqttPlan.checkRetainedGuard) { assert(!WDT->CTRL.bit.ENABLE); }
        ++mqttPlan.parseCalls;
        if (!mqttPlan.incoming.empty() && mqttPlan.incomingOffset >= mqttPlan.incoming.size()) {
            return 0; // One queued frame: a later subscription cannot redeliver consumed bytes.
        }
        if (mqttPlan.parseCalls < mqttPlan.readyOnParse) {
            return 0;
        }
        return mqttPlan.advertisedLength >= 0 ? mqttPlan.advertisedLength
                                              : static_cast<int>(mqttPlan.incoming.size());
    }
    int available() { return static_cast<int>(mqttPlan.incoming.size() - mqttPlan.incomingOffset); }
    int read(uint8_t *data, size_t count) {
        if (mqttPlan.checkRetainedGuard) { assert(!WDT->CTRL.bit.ENABLE); }
        ++mqttPlan.readCalls;
        if (mqttPlan.forcedReadResult != -2) {
            return mqttPlan.forcedReadResult;
        }
        const size_t actual =
            std::min(std::min(count, mqttPlan.readLimit), static_cast<size_t>(available()));
        memcpy(data, mqttPlan.incoming.data() + mqttPlan.incomingOffset, actual);
        mqttPlan.incomingOffset += actual;
        return static_cast<int>(actual);
    }
};
