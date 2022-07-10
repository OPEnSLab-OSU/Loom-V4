#pragma once

#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetUdp.h>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * WiFi 101 library integrated with the manager to allow for easy sleep
 */ 
class Loom_Ethernet : public Module{
    protected:

        /* These aren't used with the Wifi manager */
        void measure() override {};                               
        void print_measurements() override {};
        void package() override {};        

        // Initialize the device and connect to the network
        void initialize() override;

        // Reconnect to the network
        void power_up() override {};

        // Disconnect from the network
        void power_down() override {};
    
    public:

        /**
         * Construct a new WiFi Manager
         * @param man Reference to the manager to control all aspects of every module
         * @param mac The mac address of our device
         * @param ip The IP address we want to use if DHCP isn't active
         */ 
        Loom_Ethernet(Manager& man, uint8_t mac[6], IPAddress ip);

        /**
         * Construct a new WiFi manager, passing the credentials in as a json document
         * @param man Reference to the manager 
         * @param jsonString JSON string to pull the credentials from
         */ 
        Loom_Ethernet(Manager& man);

        /**
         * Load the Ethernet connection information from JSON
         * @param jsonString JSON formatted string containing the mac address and IP
         */
        void loadConfigFromJSON(JsonObject json);

        /**
         * Attempt to connect to the configured network 
         */
        void connect();

        /**
         * Returns a reference to the Ethernet Client
         * @return Ethernet client
         */ 
        EthernetClient& getClient() { return ethernetClient; };

        /**
         * Get a reference to the UDP communication handler
         */ 
        EthernetUDP* getUDP() { return new EthernetUDP(); };

        /**
         * Get the IP address of the Ethernet module
         */ 
        IPAddress getIPAddress();

        /**
         * Get the subnet mask of the connected network
         */ 
        IPAddress getSubnetMask();

        /**
         * Get the gateway IP of the network
         */ 
        IPAddress getGateway();

        /**
         *  Get the broadcast IP of the network
         */ 
        IPAddress getBroadcast();

        /**
         * Convert an IP address to a string
         */ 
        static String IPtoString(IPAddress ip) { return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]); };

    private:
        Manager* manInst;                   // Pointer to the manager

        EthernetClient ethernetClient;      // Ethernet client that can be used with the MQTT client or other additional objects

        bool hasInitialized = false;        // Has the Ethernet module run through the initialization process

        uint8_t mac[6];                     // MAC Address of the connected device
        IPAddress ip;                       // The IP we should assign to this device

        IPAddress remoteIP;                 // IP address to send the UDP requests to 

        
};