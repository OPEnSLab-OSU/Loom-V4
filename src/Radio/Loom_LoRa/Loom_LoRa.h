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

private:
    /** TODO
     * @brief Receives a stream of raw bytes over LoRa using the radioManager, 
     *        ultimately storing it into a buffer.
     * 
     * - Input Arguments -
     * @param [in] timeout      uint The maximum time to wait before continuing execution 
     *                          (Set to 0 for non-blocking)
     * @param [in] shouldProxy  bool Whether the device's name and instance number should
     *                          be set to match the received packet.
     * @param [in] fromAddress  uint8_t* The address the packet was received from, used 
     *                          for processing and handler functions.
     * 
     * - Output Arguments - 
     * @param [out] buf          uint8_t* The buffer to store the received bytes in
     *
     * @return FragReceiveStatus enum indicating whether the fragment was successfully 
     *         received and processed, and if so, what type of fragment it was.
     *
     * @note This function will have misaligned packets if a batch header arrives after 
     *       a packet, and will have dropped fragments if a fragment body arrives without 
     *       its header.
    */
    bool receiveFromLoRa(uint8_t *buf, uint8_t buf_size, uint timeout, 
                         uint8_t *fromAddress);

    /** 
     * @brief Receives a single fragment from another device, returning whether the
     * fragment was successfully received and processed.
     *
     * This function processes a stream of raw bytes from a LoRa transmitter into a 
     * JSON document, hence called a fragment. The function then infers 
     * what type of fragment it is (batch header, fragment header, fragment body, 
     * single-fragment packet, or lost fragment) and processes it accordingly with 
     * handler functions. 
     * 
     * - Input Arguments -
     * @param [in] buf_size     uint The size of the buffer to store the received bytes in
     * @param [in] shouldProxy  bool Whether the device's name and instance number should
     *                          be set to match the received packet.
     * @param [in] fromAddress  uint8_t* The address the packet was received from, used 
     *                          for processing and handler functions.
     *
     * @return FragReceiveStatus enum indicating whether the fragment was successfully 
     *         received and processed, and if so, what type of fragment it was.
     *
     * @note This function will have misaligned packets if a batch header arrives after 
     *       a packet, and will have dropped fragments if a fragment body arrives without 
     *       its header.
    */
    FragReceiveStatus receiveFrag(uint timeout, bool shouldProxy, 
                                  uint8_t *fromAddress);

    /** TODO
     * @brief Receives a single fragment from another device, returning whether the
     * fragment was successfully received and processed.
     *
     * This function receives a stream of raw bytes from a LoRa transmitter, and then
     * converts it into a JSON document, hence called a fragment. The function then infers 
     * what type of fragment it is (batch header, fragment header, fragment body, 
     * single-fragment packet, or lost fragment) and processes it accordingly with handler
     * functions. 
     * 
     * - Input Arguments -
     * @param [in] timeout      uint The maximum time to wait before continuing execution 
     *                          (Set to 0 for non-blocking)
     * @param [in] shouldProxy  bool Whether the device's name and instance number should
     *                          be set to match the received packet.
     * @param [in] fromAddress  uint8_t* The address the packet was received from, used 
     *                          for processing and handler functions.
     *
     * @return FragReceiveStatus enum indicating whether the fragment was successfully 
     *         received and processed, and if so, what type of fragment it was.
     *
     * @note This function will have misaligned packets if a batch header arrives after 
     *       a packet, and will have dropped fragments if a fragment body arrives without 
     *       its header.
    */
    bool handleBatchHeader(JsonDocument &workingDoc);
    
    /** TODO
     * @brief Receives a single fragment from another device, returning whether the
     * fragment was successfully received and processed.
     *
     * This function receives a stream of raw bytes from a LoRa transmitter, and then
     * converts it into a JSON document, hence called a fragment. The function then infers 
     * what type of fragment it is (batch header, fragment header, fragment body, 
     * single-fragment packet, or lost fragment) and processes it accordingly with handler
     * functions. 
     * 
     * - Input Arguments -
     * @param [in] timeout      uint The maximum time to wait before continuing execution 
     *                          (Set to 0 for non-blocking)
     * @param [in] shouldProxy  bool Whether the device's name and instance number should
     *                          be set to match the received packet.
     * @param [in] fromAddress  uint8_t* The address the packet was received from, used 
     *                          for processing and handler functions.
     *
     * @return FragReceiveStatus enum indicating whether the fragment was successfully 
     *         received and processed, and if so, what type of fragment it was.
     *
     * @note This function will have misaligned packets if a batch header arrives after 
     *       a packet, and will have dropped fragments if a fragment body arrives without 
     *       its header.
    */
    bool handleFragHeader(JsonDocument &workingDoc, uint8_t fromAddress);
        
    /** TODO
     * @brief Receives a single fragment from another device, returning whether the
     * fragment was successfully received and processed.
     *
     * This function receives a stream of raw bytes from a LoRa transmitter, and then
     * converts it into a JSON document, hence called a fragment. The function then infers 
     * what type of fragment it is (batch header, fragment header, fragment body, 
     * single-fragment packet, or lost fragment) and processes it accordingly with handler
     * functions. 
     * 
     * - Input Arguments -
     * @param [in] timeout      uint The maximum time to wait before continuing execution 
     *                          (Set to 0 for non-blocking)
     * @param [in] shouldProxy  bool Whether the device's name and instance number should
     *                          be set to match the received packet.
     * @param [in] fromAddress  uint8_t* The address the packet was received from, used 
     *                          for processing and handler functions.
     *
     * @return FragReceiveStatus enum indicating whether the fragment was successfully 
     *         received and processed, and if so, what type of fragment it was.
     *
     * @note This function will have misaligned packets if a batch header arrives after 
     *       a packet, and will have dropped fragments if a fragment body arrives without 
     *       its header.
    */
    bool handleFragBody(JsonDocument &workingDoc, uint8_t fromAddress);
    
    /** TODO
     * @brief Receives a single fragment from another device, returning whether the
     * fragment was successfully received and processed.
     *
     * This function receives a stream of raw bytes from a LoRa transmitter, and then
     * converts it into a JSON document, hence called a fragment. The function then infers 
     * what type of fragment it is (batch header, fragment header, fragment body, 
     * single-fragment packet, or lost fragment) and processes it accordingly with handler
     * functions. 
     * 
     * - Input Arguments -
     * @param [in] timeout      uint The maximum time to wait before continuing execution 
     *                          (Set to 0 for non-blocking)
     * @param [in] shouldProxy  bool Whether the device's name and instance number should
     *                          be set to match the received packet.
     * @param [in] fromAddress  uint8_t* The address the packet was received from, used 
     *                          for processing and handler functions.
     *
     * @return FragReceiveStatus enum indicating whether the fragment was successfully 
     *         received and processed, and if so, what type of fragment it was.
     *
     * @note This function will have misaligned packets if a batch header arrives after 
     *       a packet, and will have dropped fragments if a fragment body arrives without 
     *       its header.
    */
    bool handleSingleFrag(JsonDocument &workingDoc);

    /** TODO
     * @brief Receives a single fragment from another device, returning whether the
     * fragment was successfully received and processed.
     *
     * This function receives a stream of raw bytes from a LoRa transmitter, and then
     * converts it into a JSON document, hence called a fragment. The function then infers 
     * what type of fragment it is (batch header, fragment header, fragment body, 
     * single-fragment packet, or lost fragment) and processes it accordingly with handler
     * functions. 
     * 
     * - Input Arguments -
     * @param [in] timeout      uint The maximum time to wait before continuing execution 
     *                          (Set to 0 for non-blocking)
     * @param [in] shouldProxy  bool Whether the device's name and instance number should
     *                          be set to match the received packet.
     * @param [in] fromAddress  uint8_t* The address the packet was received from, used 
     *                          for processing and handler functions.
     *
     * @return FragReceiveStatus enum indicating whether the fragment was successfully 
     *         received and processed, and if so, what type of fragment it was.
     *
     * @note This function will have misaligned packets if a batch header arrives after 
     *       a packet, and will have dropped fragments if a fragment body arrives without 
     *       its header.
    */
    bool handleLostFrag(JsonDocument &workingDoc, uint8_t fromAddress);

    // transmits a json document to over lora
    bool transmitToLoRa(JsonObject json, uint8_t destinationAddress);

    // returns whether sending was successful
    bool sendFullPacket(JsonObject json, uint8_t destinationAddress);
    bool sendFragmentedPacket(JsonObject json, uint8_t destinationAddress);
    bool sendPacketHeader(JsonObject json, uint8_t destinationAddress);

    Manager* manager;                  // Instance of the Loom manager
    RHReliableDatagram* radioManager;  // Radio manager
    RH_RF95 radioDriver;               // Underlying radio driver
    
    Loom_BatchSD *batchSD = nullptr;   // Pointer to the batchSD
    
    bool poweredUp = true;

    uint8_t deviceAddress;      // Device address
    int16_t signalStrength;     // Strength of the signal received

    uint8_t powerLevel;         // The power level we want to transmit at
    uint8_t sendRetryCount;     // Number of transmission retries allowed
    uint8_t receiveRetryCount;  // Number of fragment receive retries allowed
    uint16_t retryTimeout;      // Delay between retries (MS)

    std::unordered_map<uint8_t, PartialPacket> frags; // Partial packets sorted by address
    
    uint expectedOutstandingPackets;   // estimated number of outstanding packets
};

