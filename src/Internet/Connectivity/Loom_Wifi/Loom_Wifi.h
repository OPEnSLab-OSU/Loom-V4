#pragma once

#include <WiFi101.h>
#include <WiFiUdp.h>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * WiFi 101 library integrated with the manager to allow for easy sleep
 * 
 * @author Will Richards
 */ 
class Loom_WIFI : public Module{
    protected:

        /* These aren't used with the Wifi manager */
        void measure() override {};                               
        void print_measurements() override {};
        void package() override {};        

        // Initialize the device and connect to the network
        void initialize() override;

        // Reconnect to the network
        void power_up() override;

        // Disconnect from the network
        void power_down() override;
    
    public:

        /**
         * Construct a new WiFi Manager
         * @param man Reference to the manager to control all aspects of every module
         * @param name The name of the WiFi access point we are going to connect to
         * @param password The password (if applicable) to connect to the access point 
         */ 
        Loom_WIFI(Manager& man, String name, String password = "");

        /**
         * Construct a new WiFi manager, passing the credentials in as a json document
         * @param man Reference to the manager 
         * @param jsonString JSON string to pull the credentials from
         */ 
        Loom_WIFI(Manager& man);

        /**
         * Load the Wifi credentials from a JSON string, used to pull credentials from a file
         * @param jsonString JSON formatted string containing the SSID and password 
         */
        void loadConfigFromJSON(String json);

        /**
         * Returns a reference to the WifiClient
         * @return wifiClient
         */ 
        WiFiClient& getClient() { return wifiClient; };

        /**
         * Get a reference to the UDP communication handler
         */ 
        WiFiUDP* getUDP() { return new WiFiUDP(); };

        /**
         * Attempt to ping Google, this tests if we are truly connected to the internet
         */ 
        bool verifyConnection();

        /**
         * Get the IP address of the WiFi module
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

        WiFiClient wifiClient;              // Wifi client that can be used with the MQTT client or other additional objects

        bool hasInitialized = false;        // Has the WiFi module run through the initialization process

        String wifi_name;                   // Access point to connect to
        String wifi_password;               // Password to connect to the access point

        IPAddress remoteIP;                 // IP address to send the UDP requests to 

        
};