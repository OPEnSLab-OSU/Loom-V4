#include "MQTTComponent.h"
#include "Logger.h"

namespace {
constexpr uint32_t RETAINED_MESSAGE_TIMEOUT_MS = 2000;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTComponent::initialize() {
    if (strlen(address) <= 0 || port == 0) {
        moduleInitialized = false;
        ERROR("Broker address not specified, module will be uninitialized.");
    } else {
        power_up();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::connectToBroker() {
    FUNCTION_START;
    if (moduleInitialized && internetClient.moduleInitialized) {

        // Check if we forgot to supply an address or a port number
        if (strlen(address) <= 0 || port == 0) {
            ERROR("Broker address or port not set!");
            FUNCTION_END;
            return false;
        }

        int retryAttempts = 0;

        // Try to connect multiple times as some may be dropped
        while (!mqttClient.connected()) {
            // If our retry limit has been reached we dont want to try to send data cause it wont
            // work
            if (retryAttempts >= maxRetries) {
                ERROR(F("MQTT Retry limit exceeded!"));
                // TIMER_ENABLE;
                FUNCTION_END;
                return false;
            }

            LOGF("Attempting to connect to broker: %s:%i", address, port);

            // Attempt to Connect to the MQTT client
            if (!mqttClient.connect(address, port)) {
                ERRORF("Failed to connect to broker: %s", getMQTTError());
                delay(5000);
            }

            retryAttempts++;
        }

        LOG(F("Successfully connected to broker!"));

        // Tell the broker we are still here
        mqttClient.poll();
    } else {
        ERROR("Module or NetworkComponent not initialized!");
        FUNCTION_END;
        return false;
    }

    FUNCTION_END;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::publishMessage(const char *topic, const char *message, bool retain, int qos) {
    FUNCTION_START;

    if (topic == nullptr || topic[0] == '\0' || message == nullptr) {
        ERROR(F("Cannot publish an MQTT message with a null/empty topic or null payload."));
        FUNCTION_END;
        return false;
    }

    // Make sure the module is initialized
    if (moduleInitialized && internetClient.moduleInitialized) {
        if (mqttClient.connected()) {
            // Tell the broker we are still here
            mqttClient.poll();

            const size_t messageLength = strlen(message);
            if (messageLength >= MAX_JSON_SIZE) {
                ERROR(F("MQTT message exceeds MAX_JSON_SIZE."));
                FUNCTION_END;
                return false;
            }
            // Supplying the payload length selects ArduinoMqttClient's streaming path. The
            // unknown-length overload allocates and retains a transmit buffer on the heap.
            if (mqttClient.beginMessage(topic, messageLength, retain, qos) != 1) {
                ERROR(F("Failed to begin message!"));
                FUNCTION_END;
                return false;
            }

            if (mqttClient.write(reinterpret_cast<const uint8_t *>(message), messageLength) !=
                messageLength) {
                ERROR(F("Failed to write complete MQTT message!"));
                mqttClient.stop();
                FUNCTION_END;
                return false;
            }

            // Check to see if we are actually closing messages properly
            if (mqttClient.endMessage() != 1) {
                ERROR(F("Failed to close message!"));
                FUNCTION_END;
                return false;
            } else {
                LOG(F("Data has been successfully sent!"));
                FUNCTION_END;
                return true;
            }
        } else {
            ERROR("MQTT Client not connected to broker ");
        }
    } else {
        ERROR("Module or NetworkComponent not initialized!");
    }
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::publishDocument(const char *topic, const DynamicJsonDocument &document,
                                    bool retain, int qos) {
    FUNCTION_START;

    if (!moduleInitialized || !internetClient.moduleInitialized) {
        ERROR(F("Module or NetworkComponent not initialized!"));
        FUNCTION_END;
        return false;
    }
    if (!mqttClient.connected()) {
        ERROR(F("MQTT Client not connected to broker."));
        FUNCTION_END;
        return false;
    }

    mqttClient.poll();
    const size_t payloadLength = measureJson(document);
    if (payloadLength >= MAX_JSON_SIZE) {
        ERROR(F("JSON payload exceeds MAX_JSON_SIZE."));
        FUNCTION_END;
        return false;
    }
    if (mqttClient.beginMessage(topic, payloadLength, retain, qos) != 1) {
        ERROR(F("Failed to begin message!"));
        FUNCTION_END;
        return false;
    }
    if (serializeJson(document, mqttClient) != payloadLength) {
        ERROR(F("Failed to write complete MQTT JSON payload!"));
        mqttClient.stop();
        FUNCTION_END;
        return false;
    }
    if (mqttClient.endMessage() != 1) {
        ERROR(F("Failed to close message!"));
        FUNCTION_END;
        return false;
    }

    LOG(F("Data has been successfully sent!"));
    FUNCTION_END;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::publishStream(const char *topic, Stream &source, size_t length, bool retain,
                                  int qos) {
    FUNCTION_START;

    if (!moduleInitialized || !internetClient.moduleInitialized) {
        ERROR(F("Module or NetworkComponent not initialized!"));
        FUNCTION_END;
        return false;
    }
    if (!mqttClient.connected()) {
        ERROR(F("MQTT Client not connected to broker."));
        FUNCTION_END;
        return false;
    }
    if (length >= MAX_JSON_SIZE) {
        ERROR(F("MQTT stream payload exceeds MAX_JSON_SIZE."));
        FUNCTION_END;
        return false;
    }

    mqttClient.poll();
    if (mqttClient.beginMessage(topic, length, retain, qos) != 1) {
        ERROR(F("Failed to begin message!"));
        FUNCTION_END;
        return false;
    }

    uint8_t buffer[64];
    size_t remaining = length;
    while (remaining > 0) {
        const size_t requested = min(remaining, sizeof(buffer));
        const size_t bytesRead = source.readBytes(reinterpret_cast<char *>(buffer), requested);
        if (bytesRead == 0 || mqttClient.write(buffer, bytesRead) != bytesRead) {
            ERROR(F("Failed while streaming MQTT payload!"));
            mqttClient.stop();
            FUNCTION_END;
            return false;
        }
        remaining -= bytesRead;
    }

    if (mqttClient.endMessage() != 1) {
        ERROR(F("Failed to close message!"));
        FUNCTION_END;
        return false;
    }

    FUNCTION_END;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::getCurrentRetained(const char *topic, char *message, size_t messageCapacity) {
    FUNCTION_START;

    if (topic == nullptr || topic[0] == '\0' || message == nullptr || messageCapacity < 2) {
        ERROR(F("Cannot read a retained MQTT message with an invalid topic or output buffer."));
        FUNCTION_END;
        return false;
    }

    LOG(topic);
    if (mqttClient.connected()) {
        /* Clear the incoming buffer */
        memset(message, '\0', messageCapacity);

        // Subscribe to the given topic we want to read from
        if (!mqttClient.subscribe(topic, 2)) {
            ERRORF("Failed to subscribe to topic: %s.", topic);
            FUNCTION_END;
            return false;
        }
        LOG("Successfully subscribed to topic!");

        // Retained delivery is asynchronous. Give the broker a short bounded window instead of
        // checking once immediately after SUBACK and falsely reporting an empty retained value.
        int messageSize = 0;
        const uint32_t started = millis();
        while (static_cast<uint32_t>(millis() - started) < RETAINED_MESSAGE_TIMEOUT_MS) {
            messageSize = mqttClient.parseMessage();
            if (messageSize > 0)
                break;
            delay(10);
        }

        bool received = false;
        if (messageSize > 0 && static_cast<size_t>(messageSize) < messageCapacity) {
            // parseMessage() reports payload bytes. The previous implementation accidentally
            // copied messageTopic(), so RemoteManager tried to parse its topic as JSON.
            const size_t bytesRead = mqttClient.read(reinterpret_cast<uint8_t *>(message),
                                                     static_cast<size_t>(messageSize));
            message[bytesRead] = '\0';
            received = bytesRead == static_cast<size_t>(messageSize);
            if (!received)
                ERROR(F("Retained MQTT payload ended before the advertised length."));
        } else if (messageSize > 0 && static_cast<size_t>(messageSize) >= messageCapacity) {
            ERRORF("Retained MQTT payload exceeds the %u-byte caller buffer; discarding it.",
                   static_cast<unsigned int>(messageCapacity));

            uint8_t discard[32];
            while (mqttClient.available() > 0) {
                const size_t count = min(static_cast<size_t>(mqttClient.available()),
                                         sizeof(discard));
                if (mqttClient.read(discard, count) == 0)
                    break;
            }
        } else {
            WARNING(F("No retained MQTT payload received before the timeout."));
        }

        if (!mqttClient.unsubscribe(topic))
            WARNINGF("Failed to unsubscribe from topic: %s.", topic);

        if (received) {
            FUNCTION_END;
            return true;
        }

        FUNCTION_END;
        return false;
    } else {
        ERROR("Not connected to MQTT broker.");
        FUNCTION_END;
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::deleteRetained(const char *topic) { return publishMessage(topic, "", true); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
const char *MQTTComponent::getMQTTError() {
    // Convert error codes to actual descriptions
    FUNCTION_START;
    switch (mqttClient.connectError()) {
    case -2:
        FUNCTION_END;
        return "CONNECTION_REFUSED";
    case -1:
        FUNCTION_END;
        return "CONNECTION_TIMEOUT";
    case 1:
        FUNCTION_END;
        return "UNACCEPTABLE_PROTOCOL_VERSION";
    case 2:
        FUNCTION_END;
        return "IDENTIFIER_REJECTED";
    case 3:
        FUNCTION_END;
        return "SERVER_UNAVAILABLE";
    case 4:
        FUNCTION_END;
        return "BAD_USER_NAME_OR_PASSWORD";
    case 5:
        FUNCTION_END;
        return "NOT_AUTHORIZED";
    default:
        FUNCTION_END;
        return "UNKNOWN_ERROR";
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
