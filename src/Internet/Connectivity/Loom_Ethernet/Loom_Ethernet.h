#pragma once

#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetUdp.h>

#include "Loom_Manager.h"
#include "../NetworkComponent.h"


/**
 * Ethernet driver for loom devices
 * 
 * @author Will Richards
 */ 
class Loom_Ethernet : public NetworkComponent{

    protected:

        /* These aren't used with the Wifi manager */
        void measure() override {};                               
        
        void package() override {};        

        // Initialize the device and connect to the network
        void initialize() override;

        // Reconnect to the network
        void power_up() override { };

        // Disconnect from the network
        void power_down() override { Ethernet.maintain(); };

        // Get the current time from the network
        bool getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz);

        /* Returns the currently connected state of the interface */
        bool isConnected() override { return ethernetClient.connected(); };
    
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
         * 
         * This frees the input parameter
         * 
         * @param jsonString JSON formatted string containing the mac address and IP
         */
        void loadConfigFromJSON(char* json);

        /**
         * Attempt to connect to the configured network 
         */
        bool connect();

        /**
         * Returns a reference to the Ethernet Client
         * @return Ethernet client
         */ 
        Client* getClient() override { return (Client*)&ethernetClient; };

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
        void ipToString(IPAddress ip, char array[16]) { 
            snprintf(array, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        };

    private:
        Manager* manInst;                   // Pointer to the manager

        EthernetClient ethernetClient;      // Ethernet client that can be used with the MQTT client or other additional objects
        EthernetUDP udp;                    // UDP Client

        /* NTP Syncronization */
        unsigned int localPort = 8888;
        const char* timeServer = "time.nist.gov";
        const int NTP_PACKET_SIZE = 48;
        void sendNTPpacket();

        bool hasInitialized = false;        // Has the Ethernet module run through the initialization process

        uint8_t mac[6];                     // MAC Address of the connected device
        IPAddress ip;                       // The IP we should assign to this device

        IPAddress remoteIP;                 // IP address to send the UDP requests to 

        
};