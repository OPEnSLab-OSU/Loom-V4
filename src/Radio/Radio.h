#pragma once

#include "../Module.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <SPI.h>

/**
 * Slightly abstracted Radio class to allow for all radio modules to inherit from one spot
 *
 * WARNING: this class was previously used by both LoRa and Freewave, but after refactoring
 * is now only used by Freewave. At some point this class should probably be merged into
 * Freewave and removed.
 *
 * @author Will Richards
 */
class Radio : public Module {
  protected:
    uint8_t deviceAddress;     // Device address
    uint16_t maxMessageLength; // Maximum length a packet can be
    int16_t signalStrength;    // Strength of the signal received

    uint8_t powerLevel;    // The power level we want to transmit at
    uint8_t retryCount;    // Number transmission retries allowed
    uint16_t retryTimeout; // Delay between retries (MS)

    /**
     * Get this device's address
     */
    uint8_t getAddress() const { return deviceAddress; };

    /**
     * Get the current signal strength of the radio
     */
    int16_t getSignalStrength() { return signalStrength; };

    /**
     * Receive a JSON packet from another radio, blocking until the wait time expires or a packet is
     * received
     * @param maxWaitTime The maximum time to wait before continuing execution (Set to 0 for
     * non-blocking)
     */
    virtual bool receive(uint maxWaitTime) = 0;

    /**
     * Send the current JSON data to the specified address
     * @param destinationAddress The address we want to send the data to
     */
    virtual bool send(const uint8_t destinationAddress) = 0;

    /**
     * Convert the message pack to json
     */
    bool bufferToJson(JsonDocument &destination, const uint8_t *buffer, size_t length) {
        DeserializationError error = deserializeMsgPack(destination, buffer, length);

        // Check if an error occurred
        if (error != DeserializationError::Ok) {
            ERRORF("Error occurred parsing MsgPack: %s", error.c_str());
            destination.clear();
            return false;
        }

        if (destination.overflowed()) {
            ERROR(F("Received MsgPack exceeds the Manager JSON capacity"));
            destination.clear();
            return false;
        }

        return true;
    };

    /**
     * Convert the json to a message pack
     */
    bool jsonToBuffer(uint8_t *buffer, JsonObjectConst json) {
        return serializeMsgPack(json, buffer, maxMessageLength) > 0;
    };

  public:
    /**
     * Construct a new Radio module
     * @param moduleName Name of the module
     * @param maxLength The maximum length a packet can be
     */
    Radio(const char *moduleName)
        : Module(moduleName), deviceAddress(0), maxMessageLength(0), signalStrength(0),
          powerLevel(0), retryCount(0), retryTimeout(0) {};
};
