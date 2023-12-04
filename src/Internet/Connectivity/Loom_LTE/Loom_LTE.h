#pragma once

// GSM Model Number
//#define TINY_GSM_MODEM_UBLOX 
#define TINY_GSM_MODEM_SARAR4

#include "Loom_Manager.h"   
#include "../NetworkComponent.h"
#include <TinyGsmClient.h>

#include "../../../Hardware/Loom_BatchSD/Loom_BatchSD.h"

// Specify what serial interface we want to use
#define SerialAT Serial1

/**
 * Loomified Control for a 4G LTE Board
 * 
 * @author Will Richards
 */ 
class Loom_LTE : public NetworkComponent{
    protected:
        /* These aren't used with the Wifi manager */
        void measure() override {};    

        bool isConnected() override { return modem.isGprsConnected(); };          
           
    public:

        /**
         * Construct a new LTE instance
         * @param man Reference to the manager
         * @param apn Name of the LTE network
         * @param user Username to use 
         * @param pass Password to use
         * @param powerPin Pin used to power the device
         */ 
        Loom_LTE(
            Manager& man, 
            const char* apn, 
            const char* user, 
            const char* pass, 
            const int powerPin = A5
        );

        /**
         * Construct a new LTE instance assuming credentials will be pulled from an SD card
         * @param man Reference to the manager
         */ 
        Loom_LTE(Manager& man);

        // Initialize the device and connect to the network
        void initialize() override;

        // Reconnect to the network
        void power_up() override;

        // Disconnect from the network
        void power_down() override;

        // Signal Strength
        void package() override;

        // Get the current time from the network
        bool getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) override;  

        /**
         * Load the config to connect to the LTE network from a JSON string
         * @param json Json file read, this is freed before returning
         */ 
        void loadConfigFromJSON(char* json);

        /**
         * Turn on batch upload for the lte which means it will only initialize the module when we need to upload
         * @param batch BatchSD module
         */ 
        void setBatchSD(Loom_BatchSD& batch) { batch_sd = &batch; };

        /**
         * Connect to the cellular network
         */ 
        bool connect();

        /**
         * Disconnect from the cellular network
         */ 
        void disconnect();

        /**
         * Attempt to connect to something remote to see if we actually have an internet connection
         */ 
        bool verifyConnection();

        /**
         * Get the client to supply to publish platforms that need to communicate using this internet framework
         */ 
        TinyGsmClient& getClient();

        /* Restart the modem */
        void restartModem() { 
            TIMER_RESET;
            modem.poweroff();
            delay(3000);
            modem.restart(); 
            delay(1000);
            TIMER_RESET;
        };

        /**
         * Convert an IP address to a string
         */ 
        void ipToString(IPAddress ip, char array[16]) { 
            snprintf(array, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        };

    private:
        Manager* manInst;                   // Instance of the manager

        char APN[100];                         // LTE Network Name
        char gprsUser[100];                    // GPRS Username
        char gprsPass[100];                    // GPRS Password

        int powerPin = A5;                  // Analog pin to power the LTE board

        TinyGsm modem;                      // LTE Modem
        TinyGsmClient client;               // LTE Client

        bool powerUp = true;
        bool firstInit = true;              // First time it was initialized
        Loom_BatchSD* batch_sd = nullptr;   // If we are using batch publish

};