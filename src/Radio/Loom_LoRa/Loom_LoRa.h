#pragma once

#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoJson.h>
LOOM_EXTERNAL_INCLUDE_END
#include <Module.h>
#include "Utilities/Loom_LoRaReceiveState.h"
#include "Utilities/Loom_LoRaSchedule.h"
#include "Utilities/Loom_BufferPool.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <RHReliableDatagram.h>
#include <RH_RF95.h>
LOOM_EXTERNAL_INCLUDE_END
#include <cstdint>
#include <cstdio>
#include <cstring>

#define MAX_MESSAGE_LENGTH RH_RF95_MAX_MESSAGE_LEN

#define RFM95_CS 8  // Chip select pin
#define RFM95_RST 4 // Reset pin
#define RFM95_INT 3 // Interrupt pin

#define RF95_FREQ 915.0 // LoRa Radio Frequency

#define RECV_DATA_SIZE 256

class Manager;
class Loom_BatchSD;

enum class FragReceiveStatus {
    Incomplete, // no packet has been completed
    Complete,   // packet has been loaded into the global document
    Error       // could not receive fragment
};

class Loom_LoRa : public Module {
  protected:
    // not used in this module
    void measure() override {};

  public:
    // DataAndHeartbeat preserves existing sketches. HeartbeatOnly never sends measurements
    // or drains an SD batch; it sends compact, single-frame status packets instead.
    enum class Mode : uint8_t { DataAndHeartbeat, HeartbeatOnly };
    /**
     * Construct a new LoRa driver.
     *
     * @param manager Reference to the manager
     * @param address This device's LoRa address
     * @param powerLevel Transmission power level, low to high
     * @param sendMaxRetries The number of transmission attempts to make before failing
     * @param receiveMaxRetries The number of reception attempts to make before failing
     * @param retryTimeout Length of time between retransmissions (ms)
     * @param mode Optional heartbeat-only transmission; default preserves ordinary sending
     */
    Loom_LoRa(Manager &manager, const uint8_t address, const uint8_t powerLevel,
              const uint8_t sendMaxRetries, const uint8_t receiveMaxRetries,
              const uint16_t retryTimeout, Mode mode = Mode::DataAndHeartbeat);

    /**
     * Construct a new LoRa driver, using the manager instance number as the
     * address.
     *
     * @param manager Reference to the manager
     * @param powerLevel Transmission power level, low to high
     * @param retryCount Number of attempts to make before failing
     * @param retryTimeout Length of time between retransmissions (ms)
     * @param mode Optional heartbeat-only transmission; default preserves ordinary sending
     */
    Loom_LoRa(Manager &manager, const uint8_t powerLevel = 23, const uint8_t retryCount = 3,
              const uint16_t retryTimeout = 200, Mode mode = Mode::DataAndHeartbeat);

    ~Loom_LoRa() override = default;

    Loom_LoRa(const Loom_LoRa &) = delete;
    Loom_LoRa &operator=(const Loom_LoRa &) = delete;

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

    /** Optional group isolation. Both sender and receiver must use matching address groups.
     * RadioHead ACK is still a link ACK; filtering here does not prove application storage. */
    void restrictToGroup(bool enabled = true) { groupRestricted = enabled; }
    uint8_t getHubGroup() const { return loomLoRa::Schedule::group(deviceAddress); }
    uint8_t getTimeSlot() const { return loomLoRa::Schedule::device(deviceAddress); }
    /** Borrow a validated schedule; it must outlive this radio. Defaults remain unscheduled. */
    void setSchedule(const loomLoRa::Schedule &value) { schedule = &value; }
    /** SD settings are borrowed for this call. Caller frees readFile()'s text afterwards.
     * Invalid settings preserve the old schedule. All window values are seconds. */
    bool loadScheduleFromJSON(const char *json, loomLoRa::Schedule &value);
    bool isTransmitSlot(uint32_t utc) const {
        return schedule == nullptr || schedule->mayTransmit(deviceAddress, utc);
    }
    uint32_t secondsUntilTransmitWake(uint32_t utc) const {
        return schedule == nullptr ? 0 : schedule->secondsUntilWake(deviceAddress, utc);
    }
    /** Explicit UTC gate; existing send() retains its ordinary behavior. */
    bool sendScheduled(uint8_t destination, uint32_t utc) {
        return isTransmitSlot(utc) && send(destination);
    }

    /**
     * Set a reference to the batchSD object
     *
     * @param batch Reference to the BatchSD object being used
     */
    void setBatchSD(Loom_BatchSD &batch) { batchSD = &batch; };

    /** Attach before the first fragmented receive. Pool must supply MAX_JSON_SIZE bytes and
     * outlive this radio. A failed pool allocation rejects the packet; no heap fallback. */
    bool setReceivePool(loomMemory::BufferPool &pool) {
        if (fragmentWorking.capacity() != 0) {
            return false;
        }
        receivePool = &pool;
        fragmentWorking.allocator() = loomMemory::JsonAllocator(receivePool);
        return true;
    }

    /**
     * Get the current signal strength of the radio
     */
    int16_t getSignalStrength() const { return signalStrength; };

    /**
     * Receive a JSON packet from another radio, blocking until the wait time
     * expires or a packet is received. The timeout covers the whole call, including
     * fragment headers and bodies. A partial packet can continue on the next call;
     * it is discarded after ten seconds without fragment progress.
     *
     * @param maxWaitTime The maximum time to wait before continuing execution
     *                    (Set to 0 for non-blocking)
     * @param shouldProxy Whether the device's name and instance number should
     *                    be set to match the received packet.
     */
    bool receive(uint timeout, bool shouldProxy = false);

    /**
     * Receive a JSON packet from another radio, blocking until the wait time
     * expires or a packet is received. The timeout covers the whole call, including
     * fragment headers and bodies. A partial packet can continue on the next call;
     * it is discarded after ten seconds without fragment progress.
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
    /** Send a basic alive/identity packet without packaging sensors or changing Manager's
     * document. It has no clock/battery value. Use Loom_Heartbeat for fresh optional fields. */
    bool sendHeartbeat(const uint8_t destinationAddress);
    /** Send a caller-owned, complete heartbeat document, including optional health fields. */
    bool sendHeartbeat(const uint8_t destinationAddress, const JsonDocument &heartbeat);

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
     *        This is an estimate for the most recently announced sender's batch.
     *        A blocking receive that fails clears the estimate so a dead node
     *        cannot keep a caller's batch loop running. A non-blocking poll keeps
     *        the estimate until ten seconds pass without a completed batch packet.
     */
    bool receiveBatch(uint timeout, int *numberOfPackets);

    /**
     * Receive multiple batch packets
     *
     * @param maxWaitTime The maximum time to wait before continuing execution
     *        (Set to 0 for non-blocking)
     * @param numberOfPackets Integer pointer so we can control the number of
     *        times we loop the function.
     *        This is an estimate for the most recently announced sender's batch.
     *        A blocking receive that fails clears the estimate so a dead node
     *        cannot keep a caller's batch loop running. A non-blocking poll keeps
     *        the estimate until ten seconds pass without a completed batch packet.
     * @param fromAddress out The address the packet was received from
     */
    bool receiveBatch(uint timeout, int *numberOfPackets, uint8_t *fromAddress);

  private:
    // receives some data from lora
    bool receiveFromLoRa(uint8_t *buf, uint8_t bufferCapacity, uint8_t &receivedLength,
                         uint timeout, uint8_t *fromAddress);

    // receives a single fragment from some device
    FragReceiveStatus receiveFrag(uint timeout, bool shouldProxy, uint8_t *fromAddress);

    // returns whether a packet has been loaded into the global document
    bool handleBatchHeader(JsonDocument &workingDoc, uint8_t fromAddress);
    bool handleFragHeader(JsonDocument &workingDoc, uint8_t fromAddress);
    bool handleFragBody(JsonDocument &workingDoc, uint8_t fromAddress);
    bool handleSingleFrag(JsonDocument &workingDoc);
    bool handleLostFrag(JsonDocument &workingDoc, uint8_t fromAddress);
    void resetFragmentState();
    void expireReceiveState(); // Discard abandoned state even while other nodes remain active.

    // transmits a json document to over lora
    bool transmitToLoRa(JsonObjectConst json, uint8_t destinationAddress);

    // returns whether sending was successful
    bool sendFullPacket(JsonObject json, uint8_t destinationAddress);
    bool sendFragmentedPacket(JsonObject json, uint8_t destinationAddress);
    bool sendPacketHeader(JsonObject json, uint8_t destinationAddress);

    Manager *manager = nullptr;      // Instance of the Loom manager
    RH_RF95 radioDriver;             // Underlying radio driver
    RHReliableDatagram radioManager; // RadioHead reliability manager, owned in-place

    Loom_BatchSD *batchSD = nullptr; // Pointer to the batchSD

    bool poweredUp = true;
    bool groupRestricted = false;
    const loomLoRa::Schedule *schedule = nullptr;

    uint8_t deviceAddress;      // Device address
    int16_t signalStrength = 0; // Strength of the signal received

    uint8_t powerLevel;        // The power level we want to transmit at
    uint8_t sendRetryCount;    // Number of transmission retries allowed
    uint8_t receiveRetryCount; // Number of fragment receive retries allowed
    uint16_t retryTimeout;     // Delay between retries (MS)
    Mode transmissionMode;

    // Lazily allocate one fragment workspace on the first fragmented receive, then retain and
    // reuse it. Transmit-only nodes pay no 2 KB penalty, while hubs avoid per-packet heap churn.
    static constexpr int MAX_FRAGMENT_COUNT = 64;
    static constexpr uint32_t RECEIVE_IDLE_LIMIT_MS = 10000;
    static constexpr int MAX_DATAGRAMS_PER_RECEIVE = MAX_FRAGMENT_COUNT + 2;
    using FragmentDocument = BasicJsonDocument<loomMemory::JsonAllocator>;
    FragmentDocument fragmentWorking{0};
    loomMemory::BufferPool *receivePool = nullptr;
    int remainingFragments = 0;
    uint8_t fragmentSender = 0;
    bool fragmentActive = false;
    uint32_t lastFragmentProgress = 0;

    loomLoRa::BatchReceiveState batchReceive;
};
