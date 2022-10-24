#pragma once


#include "../Radio.h"
#include "../../Loom_Manager.h"

#include <RH_RF95.h>
#include <RHReliableDatagram.h>

#define RFM95_CS 8      // Chip select pin
#define RFM95_RST 4     // Reset pin
#define RFM95_INT 3     // Interrupt pin

#define RF95_FREQ 915.0 // LoRa Radio Frequency

/**
 * Used to communicate with LoRa type radios
 * 
 * @author Will Richards
 */ 
class Loom_LoRa : public Radio{
    protected:
        /* These aren't used with this module */
        void measure() override {};                               
        void print_measurements() override {};  
             

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
            const uint8_t address,
            const uint8_t powerLevel = 23,
            const uint8_t retryCount = 3,
            const uint16_t retryTimeout = 200,
            const uint16_t max_message_len = RH_RF95_MAX_MESSAGE_LEN
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

    private:
        Manager* manInst;                       // Instance of the manager

        RH_RF95 driver;                         // Underlying radio driver
        RHReliableDatagram* manager;            // Manager for driver

        
};