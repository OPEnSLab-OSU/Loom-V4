#pragma once

#include "ArduinoJson.hpp"
#include "ArduinoJson/Object/JsonObject.hpp"
#include "Hardware/Loom_BatchSD/Loom_BatchSD.h"
#include <ArduinoJson.h>
#include <Logger.h>
#include <RHReliableDatagram.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <Module.h>
#include <RH_RF95.h>

#define MAX_MESSAGE_LENGTH RH_RF95_MAX_MESSAGE_LEN

#define RFM95_CS 8            // Chip select pin
#define RFM95_RST 4         // Reset pin
#define RFM95_INT 3         // Interrupt pin

#define RF95_FREQ 915.0 // LoRa Radio Frequency

#define RECV_DATA_SIZE 256

enum class FragReceiveStatus {
    Incomplete,  // no packet has been completed
    Complete,    // packet has been loaded into the global document
    Error        // could not receive fragment
};

struct PartialPacket {
    int remainingFragments;
    DynamicJsonDocument working;
};

class Loom_LoRa : public Module {
protected:
    // not used in this module
    void measure() override {};

public:
    /**
     * Construct a new LoRa driver.
     *
     * @param manager Reference to the manager
     * @param address This device's LoRa address
     * @param powerLevel Transmission power level, low to high
     * @param retryCount Number of attempts to make before failing
     * @param retryTimeout Length of time between retransmissions (ms)
     */ 
    Loom_LoRa(
        Manager& manager,
        const uint8_t address,
        const uint8_t powerLevel,
        const uint8_t retryCount,
        const uint16_t retryTimeout
    );

    /**
     * Construct a new LoRa driver, using the manager instance number as the
     * address.
     *
     * @param manager Reference to the manager
     * @param address This device's LoRa address
     * @param powerLevel Transmission power level, low to high
     * @param retryCount Number of attempts to make before failing
     * @param retryTimeout Length of time between retransmissions (ms)
     */ 
    Loom_LoRa(
        Manager& manager,
        const uint8_t powerLevel = 23,
        const uint8_t retryCount = 3,
        const uint16_t retryTimeout = 200
    );

    ~Loom_LoRa();

    /**
     * Initialize the module
     */ 
    void initialize() override;

    /**
     * Power up the module
     */ 
    void power_up() override;

    /**
     * Power down the module
     */ 
    void power_down() override; 

    /**
     * Package basic data about the device
     */ 
    void package() override;

    // TODO(rwheary): maybe this should be fixed
    /**
     * Receive a JSON packet from another radio, blocking until the wait time 
     * expires or a packet is received. Note that this method may block for an
     * arbitrary time to receive a fragmented packet. If this is undesireable
     * then open a PR I guess?
     *
     * @param maxWaitTime The maximum time to wait before continuing execution 
     *                    (Set to 0 for non-blocking)
     */
    bool receive(uint timeout);

    bool receiveBatch(uint timeout);

    /**
     * Send the current JSON data to the specified address.
     *
     * @param destinationAddress The address to send the data to.
     */ 
    bool send(const uint8_t destinationAddress);

    /**
     * Send an arbitrary JSON object to the specified address.
     *
     * @param destinationAddress The address to send the data to.
     * @param json The JSON object to transmit.
     */ 
    bool send(const uint8_t destinationAddress, JsonObject json);

    bool sendBatch(const uint8_t destinationAddress);

private:
    // receives some data from lora
    bool receiveFromLoRa(uint8_t *buf, uint8_t buf_size, uint timeout, 
                         uint8_t *fromAddress);
    FragReceiveStatus receiveFrag(uint timeout);

    // TODO(rwheary): consider changing JsonDocument& to JsonVariant
    // returns whether a packet has been loaded into the global document
    bool handleFragHeader(JsonDocument &workingDoc, uint8_t fromAddress);
    bool handleFragBody(JsonDocument &workingDoc, uint8_t fromAddress);
    bool handleSingleFrag(JsonDocument &workingDoc);
    bool handleLostFrag(JsonDocument &workingDoc, uint8_t fromAddress);

    // transmits a json document to over lora
    bool transmitToLoRa(JsonObject json, uint8_t destinationAddress);

    // returns whether sending was successful
    bool sendFullPacket(JsonObject json, uint8_t destinationAddress);
    bool sendFragmentedPacket(JsonObject json, uint8_t destinationAddress);
    bool sendPacketHeader(JsonObject json, uint8_t destinationAddress);

    char logOutput[OUTPUT_SIZE] = {};

    Manager* manager;                  // Instance of the Loom manager
    RHReliableDatagram* radioManager;  // Radio manager
    RH_RF95 radioDriver;               // Underlying radio driver
    
    Loom_BatchSD *batchSD = nullptr;   // Pointer to the batchSD
    
    bool poweredUp = true;

    uint8_t deviceAddress;      // Device address
    int16_t signalStrength;     // Strength of the signal received

    uint8_t powerLevel;         // The power level we want to transmit at
    uint8_t sendRetryCount;     // Number transmission retries allowed
    uint8_t receiveRetryCount;  // Number fragment receive retries allowed
    uint16_t retryTimeout;      // Delay between retries (MS)

    // TODO(rwheary): This could result in catastrophic memory leaks in the 
    // worst case. Should probably have a mechanism to handle that.
    std::unordered_map<uint8_t, PartialPacket> frags; // Partial packets sorted by address
};

