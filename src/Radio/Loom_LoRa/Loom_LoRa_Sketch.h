#pragma once

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

struct PartialPacket {
    int remainingFragments;
    DynamicJsonDocument working;
};

// wip
class Loom_LoRa_Sketch : public Module {
protected:
    // not used in this module
    void measure() override {};

public:
    Loom_LoRa_Sketch(
        Manager& manager,
        const uint8_t address,
        const uint8_t powerLevel = 23,
        const uint8_t retryCount = 3,
        const uint16_t retryTimeout = 200
    );

    Loom_LoRa_Sketch(
        Manager& manager,
        const uint8_t powerLevel = 23,
        const uint8_t retryCount = 3,
        const uint16_t retryTimeout = 200
    );

    ~Loom_LoRa_Sketch();

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

    /**
     * Receive a JSON packet from another radio, blocking until the wait time expires or a packet is received.
     * Note that this method may block for an arbitrary time to receive a fragmented packet. If this is undesireable then open a PR I guess?
     * @param maxWaitTime The maximum time to wait before continuing execution (Set to 0 for non-blocking)
     */
    bool receive(uint timeout);

    /**
     * Send the current JSON data to the specified address
     * @param destinationAddress The address we want to send the data to
     */ 
    bool send(const uint8_t destinationAddress);

private:
    bool receiveFromLoRa(uint8_t *buf, uint8_t buf_size, uint timeout, uint8_t *fromAddress);
    bool receiveFrag(uint timeout, bool *isReady);

    // returns whether a packet has been loaded into the global document
    bool handleFragHeader(JsonDocument &workingDoc, uint8_t fromAddress);
    bool handleFragBody(JsonDocument &workingDoc, uint8_t fromAddress);
    bool handleSingleFrag(JsonDocument &workingDoc);
    bool handleLostFrag(JsonDocument &workingDoc, uint8_t fromAddress);

    char logOutput[OUTPUT_SIZE] = {};

    Manager* manager;                  // Instance of the Loom manager
    RHReliableDatagram* radioManager;  // Radio manager
    RH_RF95 radioDriver;               // Underlying radio driver
    
    bool poweredUp = true;

    uint8_t deviceAddress;             // Device address
    int16_t signalStrength;            // Strength of the signal received

    uint8_t powerLevel;                // The power level we want to transmit at
    uint8_t sendRetryCount;            // Number transmission retries allowed
    uint8_t receiveRetryCount;         // Number fragment receive retries allowed
    uint16_t retryTimeout;             // Delay between retries (MS)

    // TODO(rwheary): This could result in catastrophic memory leaks in the worst
    // case. Should probably have a mechanism to handle that.
    std::unordered_map<uint8_t, PartialPacket> frags; // Partial packets sorted by address
};

