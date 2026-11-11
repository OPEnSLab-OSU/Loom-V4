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

// Handshake before each send()/sendRaw(): off for now, since a compact batch is one ACKed message and the handshake only
// adds two extra transmits. Uncomment on both the hub and the nodes to try it (see requestHandshake).
// #define LOOM_LORA_HANDSHAKE

#define RFM95_CS 8            // Chip select pin
#define RFM95_RST 4         // Reset pin
#define RFM95_INT 3         // Interrupt pin

#define RF95_FREQ 915.0 // LoRa Radio Frequency

#define RECV_DATA_SIZE 256

enum class FragReceiveStatus {
    Incomplete,  // no packet has been completed
    Complete,    // packet has been loaded into the global document
    Error,       // could not receive fragment
    HandshakeAccepted  // hub answered a node's handshake request and now waits for its payload
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
     * @param sendMaxRetries The number of transmission attempts to make before failing
     * @param receiveMaxRetries The number of reception attempts to make before failing
     * @param retryTimeout Length of time between retransmissions (ms)
     */ 
    Loom_LoRa(
        Manager& manager,
        const uint8_t address,
        const uint8_t powerLevel,
        const uint8_t sendMaxRetries,
        const uint8_t receiveMaxRetries,
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

    /**
     * Get this device's address
     */ 
    uint8_t getAddress() const { return deviceAddress; };

    /**
     * Set this device's address
     */
    void setAddress(const uint8_t newAddress);

    /**
     * Set a reference to the batchSD object
     * 
     * @param batch Reference to the BatchSD object being used
     */
    void setBatchSD(Loom_BatchSD& batch) { batchSD = &batch; };

    /**
     * Get the current signal strength of the radio
     */ 
    int16_t getSignalStrength() const { return signalStrength; };

    /**
     * Receive a JSON packet from another radio, blocking until the wait time 
     * expires or a packet is received. Note that this method may block for an
     * arbitrary time to receive a fragmented packet.
     *
     * @param maxWaitTime The maximum time to wait before continuing execution 
     *                    (Set to 0 for non-blocking)
     * @param shouldProxy Whether the device's name and instance number should
     *                    be set to match the received packet.
     */
    bool receive(uint timeout, bool shouldProxy = false);

    /**
     * Receive a JSON packet from another radio, blocking until the wait time 
     * expires or a packet is received. Note that this method may block for an
     * arbitrary time to receive a fragmented packet.
     *
     * @param maxWaitTime The maximum time to wait before continuing execution 
     *                    (Set to 0 for non-blocking)
     * @param shouldProxy Whether the device's name and instance number should
     *                    be set to match the received packet.
     * @param senderAddr out param, the address of the sending device.
     */
    bool receive(uint timeout, uint8_t *fromAddress, bool shouldProxy = false);

    /**
     * Send the current JSON data to the specified address (after a handshake if LOOM_LORA_HANDSHAKE is on).
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

    /**
     * Send the current batch of JSON data to the given address
     *
     * @param destinationAddress The address we want to send the data to
     */ 
    bool sendBatch(const uint8_t destinationAddress);

    /**
     * Receive multiple batch packets 
     *
     * @param maxWaitTime The maximum time to wait before continuing execution
     *        (Set to 0 for non-blocking)
     * @param numberOfPackets Integer pointer so we can control the number of
     *        times we loop the function.
     *        IMPORTANT: numberOfPackets represents the number of outstanding
     *        packets that receiveBatch expects: this number may increase,
     *        or not reflect the actual number.
     */ 
    bool receiveBatch(uint timeout, int *numberOfPackets);

    /**
     * Receive multiple batch packets 
     *
     * @param maxWaitTime The maximum time to wait before continuing execution
     *        (Set to 0 for non-blocking)
     * @param numberOfPackets Integer pointer so we can control the number of
     *        times we loop the function.
     *        IMPORTANT: numberOfPackets represents the number of outstanding
     *        packets that receiveBatch expects: this number may increase,
     *        or not reflect the actual number.
     * @param fromAddress out The address the packet was received from
     */ 
    bool receiveBatch(uint timeout, int *numberOfPackets, uint8_t *fromAddress);

    /**
     * Send bytes as-is in one LoRa message, with the same acknowledgement and retries as send().
     * For compact formats such as DendrometerPacket; the first byte must be below 0x80 so the
     * receiver can tell it apart from Loom's MsgPack packets.
     *
     * @param data The bytes to send
     * @param len Number of bytes, at most RH_RF95_MAX_MESSAGE_LEN (251)
     * @param destinationAddress The address to send the data to
     */
    bool sendRaw(const uint8_t *data, uint8_t len, const uint8_t destinationAddress);

    /**
     * Length of the raw message (see sendRaw) the last receive() / receiveBatch() returned, or 0 if it was a
     * normal Loom packet (which is loaded into the manager's document as usual). Read it with rawData().
     */
    uint8_t rawLength() const { return rawMessageLength; };

    /**
     * Wait once, up to `timeout` ms (must be > 0), for a raw message; unlike receive() there are no retries, so a
     * short wait for a reply stays short. Returns whether a raw message arrived (read it with rawData()).
     */
    bool receiveRaw(uint timeout);
    const uint8_t *rawData() const { return rawMessage; };

private:
    // receives some data from lora; buf_size is the buffer size in, the received length out
    bool receiveFromLoRa(uint8_t *buf, uint8_t *buf_size, uint timeout,
                         uint8_t *fromAddress);

    // receives a single fragment from some device
    FragReceiveStatus receiveFrag(uint timeout, bool shouldProxy, 
                                  uint8_t *fromAddress);

    // returns whether a packet has been loaded into the global document
    bool handleBatchHeader(JsonDocument &workingDoc);
    bool handleFragHeader(JsonDocument &workingDoc, uint8_t fromAddress);
    bool handleFragBody(JsonDocument &workingDoc, uint8_t fromAddress);
    bool handleSingleFrag(JsonDocument &workingDoc);
    bool handleLostFrag(JsonDocument &workingDoc, uint8_t fromAddress);

    // transmits a json document to over lora
    bool transmitToLoRa(JsonObject json, uint8_t destinationAddress);

#ifdef LOOM_LORA_HANDSHAKE
    // Handshake: before a payload the node sends {"handshake":"Request"} and waits for "Accept"; the hub answers one
    // node at a time (until its receive() returns or HANDSHAKE_TIMEOUT_MS passes) so payloads don't interleave
    bool requestHandshake(uint8_t destinationAddress);  // node: request + wait for accept, with retries
    bool sendHandshakeRequest(uint8_t destinationAddress);
    bool sendHandshakeResponse(uint8_t destinationAddress);
    bool handshakeReceive(uint8_t destinationAddress);  // node: wait for the hub's accept
    bool handleHandshakeReceive(JsonDocument &tempDoc, uint8_t *fromAddress);  // hub: answer a request
    void beginHandshake(uint8_t peerAddress);
    void clearHandshake();
    bool clearExpiredHandshake();  // true if a handshake had timed out and was cleared
#endif

    // returns whether sending was successful
    bool sendFullPacket(JsonObject json, uint8_t destinationAddress);
    bool sendFragmentedPacket(JsonObject json, uint8_t destinationAddress);
    bool sendPacketHeader(JsonObject json, uint8_t destinationAddress);

    Manager* manager;                  // Instance of the Loom manager
    RHReliableDatagram* radioManager;  // Radio manager
    RH_RF95 radioDriver;               // Underlying radio driver
    
    Loom_BatchSD *batchSD = nullptr;   // Pointer to the batchSD
    uint8_t rawMessage[MAX_MESSAGE_LENGTH];  // Last raw message received, see rawLength()
    uint8_t rawMessageLength = 0;

    bool poweredUp = true;

#ifdef LOOM_LORA_HANDSHAKE
    bool handshakeEstablished = false;
    uint8_t handshakePeerAddress = 0;
    uint32_t handshakeEstablishedAt = 0;
    static constexpr uint32_t HANDSHAKE_TIMEOUT_MS = 15000;
#endif

    uint8_t deviceAddress;      // Device address
    int16_t signalStrength;     // Strength of the signal received

    uint8_t powerLevel;         // The power level we want to transmit at
    uint8_t sendRetryCount;     // Number of transmission retries allowed
    uint8_t receiveRetryCount;  // Number of fragment receive retries allowed
    uint16_t retryTimeout;      // Delay between retries (MS)

    std::unordered_map<uint8_t, PartialPacket> frags; // Partial packets sorted by address
    
    uint expectedOutstandingPackets;   // estimated number of outstanding packets
};