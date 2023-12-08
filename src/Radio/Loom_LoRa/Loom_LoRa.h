#pragma once


#include <RH_RF95.h>
#include <RHReliableDatagram.h>

#include "../Radio.h"
#include "../../Loom_Manager.h"
#include "../../Hardware/Loom_BatchSD/Loom_BatchSD.h"


#define RFM95_CS 8      // Chip select pin
#define RFM95_RST 4     // Reset pin
#define RFM95_INT 3     // Interrupt pin

#define RF95_FREQ 915.0 // LoRa Radio Frequency

#define RECV_DATA_SIZE 256

/**
 * Used to communicate with LoRa type radios
 * 
 * @author Will Richards
 */ 
class Loom_LoRa : public Radio{
    protected:
        /* These aren't used with this module */
        void measure() override {};                               
             

    public:

        /**
         * Construct a new LoRa driver
         * @param man Reference to the manager
         * @param address This device's LoRa address
         * @param powerLevel Transmission power level, low to high
         * @param retryCount Number of attempts to make before failing
         * @param retryTimeout Length of time between retransmissions (ms)
         * @param max_message_len The maximum possible message length we can transmit
         */ 
        Loom_LoRa(
            Manager& man,
            const int address = -1,
            const uint8_t powerLevel = 23,
            const uint8_t retryCount = 3,
            const uint16_t retryTimeout = 200
        );

        /* Destructor for the manager to prevent memory leaks */
        ~Loom_LoRa(){
            delete manager;
        };

        /**
         * Receive a JSON packet from another radio, blocking until the wait time expires or a packet is received
         * @param maxWaitTime The maximum time to wait before continuing execution (Set to 0 for non-blocking)
         */ 
        bool receive(uint maxWaitTime) override;

        /**
         * Send the current JSON data to the specified address
         * @param destinationAddress The address we want to send the data to
         */ 
        bool send(const uint8_t destinationAddress) override;

        /**
         * Send the current JSON data to the specified address overloaded to add support to send generic JSON objects
         * @param destinationAddress The address we want to send the data to
         * @param json Pass in the JSON object to transmit
         */ 
        bool send(const uint8_t destinationAddress, JsonObject json);


        /**
         * Receive multiple batch packets 
         * @param maxWaitTime The maximum time to wait before continuing execution (Set to 0 for non-blocking)
         * @param numberOfPackets Integer pointer so we can control the number of times we loop the function 
         */ 
        bool receiveBatch(uint maxWaitTime, int* numberOfPackets);

         /**
         * Send the current batch of JSON data to the given address
         * @param destinationAddress The address we want to send the data to
         */ 
        bool sendBatch(const uint8_t destinationAddress);

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
         * Set the address of the device
         */ 
        void setAddress(const uint8_t addr);

        /**
         * Set a reference to the batchSD object
         * 
         * @param batch Reference to the BatchSD object being used
        */
        void setBatchSD(Loom_BatchSD& batch){ batchSD = &batch; };

    private:
        Manager* manInst;                                   // Instance of the manager

        RH_RF95 driver;                                     // Underlying radio driver
        RHReliableDatagram* manager;                        // Manager for driver

        Loom_BatchSD* batchSD = nullptr;                    // Create a pointer to the batchSD
        bool poweredUp = true;
        
        bool transmit(JsonObject json, int destination);     // Internal method for sending JSON data over radio
        bool recv(int waitTime);                             // Internal method for reading data in from radio

        char recvData[RECV_DATA_SIZE];

        bool sendFull(const uint8_t destinationAddress, JsonObject json);                                   // Send the full packet with no fragmentation
        bool sendPartial(const uint8_t destinationAddress, JsonObject json);                                // Fragment the packet when needed
        bool sendModules(JsonObject json, int numModules, const uint8_t destinationAddress);                // Send one module to the hub to allow for fragmented sending

        bool receivePartial(uint waitTime);
        
};