#pragma once


#include "../Radio.h"
#include "../../Loom_Manager.h"

#include <HardwareSerial.h>
#include <RH_Serial.h>
#include <RHReliableDatagram.h>

/**
 * Used to communicate with LoRa type radios
 */ 
class Loom_Freewave : public Radio{
    protected:
        /* These aren't used with this module */
        void measure() override {};                               
        void print_measurements() override {};  
        void package() override {};        

    public:

        /**
         * Construct a new LoRa driver
         * @param man Reference to the manager
         * @param max_message_len The maximum possible message length we can transmit
         * @param address This device's LoRa address
         * @param powerLevel Transmission power level, low to high
         * @param retryCount Number of attempts to make before failing
         * @param retryTimeout Length of time between retransmissions (ms)
         */ 
        Loom_Freewave(
            Manager& man,
            const uint8_t address = 0,
            const uint16_t max_message_len = RH_SERIAL_MAX_MESSAGE_LEN,
            const uint8_t retryCount = 3,
            const uint16_t retryTimeout = 200
        );

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
         * Set the address of the device
         */ 
        void setAddress(const uint8_t addr);

    private:
        Manager* manInst;                       // Instance of the manager

        HardwareSerial& serial1;                // Serial reference
        RH_Serial driver;                       // Freewave Driver
        RHReliableDatagram manager;             // Manager for driver

        
};