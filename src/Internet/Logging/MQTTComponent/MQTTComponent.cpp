#include "MQTTComponent.h"
#include "Logger.h"
#include "Utilities/Loom_JsonUtils.h"

namespace {
constexpr uint32_t RETAINED_MESSAGE_TIMEOUT_MS = 2000;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTComponent::initialize() {
    if (strlen(address) <= 0 || port == 0) {
        moduleInitialized = false;
        ERROR("Broker address not specified, module will be uninitialized.");
    } else {
        power_up();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::connectToBroker() {
    FUNCTION_START(this);
    if (!moduleInitialized) {
        ERROR("MQTT module not initialized!");
        return false;
    }
    if (address[0] == '\0' || port == 0) {
        ERROR("Broker address or port not set!");
        return false;
    }

    // poll() can discover a dead connection. Recheck afterwards instead of returning a stale
    // connected result, and close the old TCP stream before each fresh MQTT handshake.
    {
        LoomWatchdogPause watchdogPause;
        mqttClient.poll();
        if (mqttClient.connected() && internetClient.moduleInitialized) {
            return true;
        }
        mqttClient.stop(); // Release the old socket before the transport repairs its data session.
    }
    if (!internetClient.prepareConnection()) {
        ERROR(F("Network data session unavailable; retaining data for the next upload attempt."));
        return false;
    }
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        LOGF("Attempting to connect to broker: %s:%i (%i/%i)", address, port, attempt + 1,
             maxRetries);
        bool connected = false;
        {
            // ArduinoMqttClient's handshake timeout is 30 seconds, longer than the Feather's
            // watchdog. Restore the caller's watchdog before diagnostics or the next operation.
            LoomWatchdogPause watchdogPause;
            mqttClient.stop();
            connected = mqttClient.connect(address, port) == 1;
            if (connected) {
                mqttClient.poll();
                connected = mqttClient.connected();
            }
            if (!connected) {
                mqttClient.stop();
            }
        }
        if (connected) {
            LOG(F("Successfully connected to broker!"));
            return true;
        }
        ERRORF("Failed to connect to broker: %s", getMQTTError());
        if (attempt + 1 < maxRetries) {
            // A fixed backoff is expected idle time, even with a sketch's short watchdog period.
            LoomWatchdogPause watchdogPause;
            delay(5000);
        }
    }
    ERROR(F("MQTT Retry limit exceeded; the next publish can retry a fresh connection."));
    FUNCTION_END;
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::publishMessage(const char *topic, const char *message, bool retain, int qos) {
    FUNCTION_START(this);

    if (topic == nullptr || topic[0] == '\0' || message == nullptr) {
        ERROR(F("Cannot publish an MQTT message with a null/empty topic or null payload."));
        return false;
    }

    const size_t messageLength = strlen(message);
    if (messageLength >= MAX_JSON_SIZE) {
        ERROR(F("MQTT message exceeds MAX_JSON_SIZE."));
        return false;
    }
    // Supplying the payload length selects ArduinoMqttClient's streaming path. The
    // unknown-length overload allocates and retains a transmit buffer on the heap.
    if (!beginPublish(topic, messageLength, retain, qos)) {
        return false;
    }

    bool wroteAll = false;
    {
        LoomWatchdogPause watchdogPause;
        wroteAll = mqttClient.write(reinterpret_cast<const uint8_t *>(message), messageLength) ==
                   messageLength;
        if (!wroteAll) {
            mqttClient.stop();
        }
    }
    if (!wroteAll) {
        ERROR(F("Failed to write complete MQTT message; connection closed!"));
        return false;
    }

    // Check to see if we are actually closing messages properly
    if (!finishPublish()) {
        return false;
    }
    FUNCTION_END;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::publishDocument(const char *topic, const JsonDocument &document, bool retain,
                                    int qos) {
    FUNCTION_START(this);

    if (!loomJsonIsComplete(document)) {
        ERROR(F("Refusing to publish an empty or overflowed JSON document."));
        return false;
    }

    const size_t payloadLength = measureJson(document);
    if (payloadLength >= MAX_JSON_SIZE) {
        ERROR(F("JSON payload exceeds MAX_JSON_SIZE."));
        return false;
    }
    if (!beginPublish(topic, payloadLength, retain, qos)) {
        return false;
    }
    bool wroteAll = false;
    {
        LoomWatchdogPause watchdogPause;
        wroteAll = serializeJson(document, mqttClient) == payloadLength;
        if (!wroteAll) {
            mqttClient.stop();
        }
    }
    if (!wroteAll) {
        ERROR(F("Failed to write complete MQTT JSON payload; connection closed!"));
        return false;
    }
    if (!finishPublish()) {
        return false;
    }

    FUNCTION_END;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::publishStream(const char *topic, Stream &source, size_t length, bool retain,
                                  int qos) {
    FUNCTION_START(this);

    if (length >= MAX_JSON_SIZE) {
        ERROR(F("MQTT stream payload exceeds MAX_JSON_SIZE."));
        return false;
    }

    if (!beginPublish(topic, length, retain, qos)) {
        return false;
    }

    uint8_t buffer[64];
    size_t remaining = length;
    while (remaining > 0) {
        const size_t requested = min(remaining, sizeof(buffer));
        const size_t bytesRead = source.readBytes(reinterpret_cast<char *>(buffer), requested);
        bool wroteAll = false;
        {
            // Keep the watchdog active while reading SD; pause only the modem write/close.
            LoomWatchdogPause watchdogPause;
            wroteAll = bytesRead > 0 && mqttClient.write(buffer, bytesRead) == bytesRead;
            if (!wroteAll) {
                mqttClient.stop();
            }
        }
        if (!wroteAll) {
            ERROR(F("Failed while streaming MQTT payload; connection closed!"));
            return false;
        }
        remaining -= bytesRead;
    }

    if (!finishPublish()) {
        return false;
    }
    FUNCTION_END;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::beginPublish(const char *topic, size_t length, bool retain, int qos) {
    FUNCTION_START(this);
    if (topic == nullptr || topic[0] == '\0' || strlen(topic) >= MAX_TOPIC_LENGTH ||
        length >= MAX_JSON_SIZE || qos < 0 || qos > 2) {
        ERROR(F("Invalid MQTT topic, payload length, or QoS."));
        return false;
    }
    if (!moduleInitialized || !internetClient.moduleInitialized) {
        ERROR(F("Module or NetworkComponent not initialized!"));
        return false;
    }
    bool started = false;
    {
        LoomWatchdogPause watchdogPause;
        mqttClient.poll();
        // A failed heartbeat can close the socket during poll(). Never start a frame on it.
        started =
            mqttClient.connected() &&
            mqttClient.beginMessage(topic, static_cast<unsigned long>(length), retain, qos) == 1;
        if (!started) {
            mqttClient.stop();
        }
    }
    if (!started) {
        ERROR(F("MQTT begin failed/disconnected; closed the stream for the next reconnect."));
    }
    FUNCTION_END;
    return started;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::finishPublish() {
    FUNCTION_START(this);
    bool completed = false;
    {
        // QoS 1 waits for PUBACK; QoS 2 can wait for two responses. A timeout must discard the
        // socket, otherwise a late acknowledgement can contaminate a later publish attempt.
        LoomWatchdogPause watchdogPause;
        completed = mqttClient.endMessage() == 1;
        if (!completed) {
            mqttClient.stop();
        }
    }
    if (!completed) {
        ERROR(F("MQTT finish/acknowledgement failed; closed the stream for the next reconnect."));
    }
    FUNCTION_END;
    return completed;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::getCurrentRetained(const char *topic, char *message, size_t messageCapacity) {
    FUNCTION_START(this);

    if (topic == nullptr || topic[0] == '\0' || strlen(topic) >= MAX_TOPIC_LENGTH ||
        message == nullptr || messageCapacity < 2) {
        ERROR(F("Cannot read a retained MQTT message with an invalid topic or output buffer."));
        return false;
    }

    LOG(topic);
    if (mqttClient.connected()) {
        /* Clear the incoming buffer */
        memset(message, '\0', messageCapacity);

        // Subscribe to the given topic we want to read from
        if (!mqttClient.subscribe(topic, 2)) {
            ERRORF("Failed to subscribe to topic: %s.", topic);
            return false;
        }
        LOG("Successfully subscribed to topic!");

        // Retained delivery is asynchronous. Give the broker a short bounded window instead of
        // checking once immediately after SUBACK and falsely reporting an empty retained value.
        int messageSize = 0;
        const uint32_t started = millis();
        while (static_cast<uint32_t>(millis() - started) < RETAINED_MESSAGE_TIMEOUT_MS) {
            messageSize = mqttClient.parseMessage();
            if (messageSize > 0) {
                break;
            }
            delay(10);
        }

        bool received = false;
        if (messageSize > 0 && static_cast<size_t>(messageSize) < messageCapacity) {
            // parseMessage() reports payload bytes. The previous implementation accidentally
            // copied messageTopic(), so RemoteManager tried to parse its topic as JSON.
            // The SDK returns a signed count. Validate it before using it as a buffer index.
            const int bytesRead = mqttClient.read(reinterpret_cast<uint8_t *>(message),
                                                  static_cast<size_t>(messageSize));
            const bool countValid = bytesRead >= 0 && bytesRead <= messageSize;
            if (countValid) {
                message[static_cast<size_t>(bytesRead)] = '\0';
            } else {
                message[0] = '\0';
            }
            received = countValid && bytesRead == messageSize;
            if (!received) {
                ERROR(F("Retained MQTT payload ended before the advertised length."));
                mqttClient.stop(); // Discard the incomplete frame, not the broker's retained value.
            }
        } else if (messageSize > 0 && static_cast<size_t>(messageSize) >= messageCapacity) {
            ERRORF("Retained MQTT payload exceeds the %u-byte caller buffer; discarding it.",
                   static_cast<unsigned int>(messageCapacity));

            // Do not drain an arbitrarily large/continuous payload during command handling.
            // Closing this socket leaves the broker's retained value available for diagnosis.
            mqttClient.stop();
        } else {
            WARNING(F("No retained MQTT payload received before the timeout."));
        }

        if (mqttClient.connected() && !mqttClient.unsubscribe(topic)) {
            WARNINGF("Failed to unsubscribe from topic: %s.", topic);
        }

        if (received) {
            return true;
        }
        return false;
    } else {
        ERROR("Not connected to MQTT broker.");
        return false;
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::deleteRetained(const char *topic) { return publishMessage(topic, "", true); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
const char *MQTTComponent::getMQTTError() {
    // Convert error codes to actual descriptions
    FUNCTION_START(this);
    switch (mqttClient.connectError()) {
    case -2:
        return "CONNECTION_REFUSED";
    case -1:
        return "CONNECTION_TIMEOUT";
    case 1:
        return "UNACCEPTABLE_PROTOCOL_VERSION";
    case 2:
        return "IDENTIFIER_REJECTED";
    case 3:
        return "SERVER_UNAVAILABLE";
    case 4:
        return "BAD_USER_NAME_OR_PASSWORD";
    case 5:
        return "NOT_AUTHORIZED";
    default:
        return "UNKNOWN_ERROR";
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
