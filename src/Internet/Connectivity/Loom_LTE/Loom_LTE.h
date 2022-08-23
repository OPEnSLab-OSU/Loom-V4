#pragma once

// GSM Model Number
//#define TINY_GSM_MODEM_UBLOX 
#define TINY_GSM_MODEM_SARAR4

#include "Module.h"
#include "Loom_Manager.h"   

#include <TinyGsmClient.h>

// Specify what serial interface we want to use
#define SerialAT Serial1

/**
 * Loomified Control for a 4G LTE Board
 * 
 * @author Will Richards
 */ 
class Loom_LTE : public Module{
    protected:
        /* These aren't used with the Wifi manager */
        void measure() override {};                               
        void print_measurements() override {};
        void package() override {};        

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
            const String apn, 
            const String user, 
            const String pass, 
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

        /**
         * Load the config to connect to the LTE network from a JSON string
         */ 
        void loadConfigFromJSON(String json);

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

        /**
         * Checks if the modem is connected to a cellular network
         */ 
        bool isConnected();

        /**
         * Convert an IP address to a string
         */ 
        static String IPtoString(IPAddress ip) { return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]); };

    private:
        Manager* manInst;                   // Instance of the manager

        String APN;                         // LTE Network Name
        String gprsUser;                    // GPRS Username
        String gprsPass;                    // GPRS Password

        int powerPin = A5;                  // Analog pin to power the LTE board

        TinyGsm modem;                      // LTE Modem
        TinyGsmClient client;               // LTE Client

        bool firstInit = true;              // First time it was initialized

};