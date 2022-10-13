#pragma once

#include <WiFi101.h>
#include <WiFiUdp.h>
#include <FlashStorage.h>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * Communication mode for routing traffic between the feather and Max client
 */ 
enum CommunicationMode{
    CLIENT,         // Connect to a remote router to handle traffic
    AP              // Set the feather itself as an access point
};

/* WiFi info struct that will be used to store the WiFi data on flash */
typedef struct {
    bool is_valid;
    char name[100];
    char password[100];
} WifiInfo;

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
             

        // Initialize the device and connect to the network
        void initialize() override;
        void package() override;   

        // Reconnect to the network
        void power_up() override;

        // Disconnect from the network
        void power_down() override;
    
    public:

        /**
         * Construct a new WiFi Manager
         * @param man Reference to the manager to control all aspects of every module
         * @param mode Whether or not to try to connect to an access point or create our own
         * @param name The name of the WiFi access point we are going to connect to
         * @param password The password (if applicable) to connect to the access point 
         */ 
        Loom_WIFI(Manager& man, CommunicationMode mode, String name = "", String password = "");

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
         * Connect to an already existing access point
         */ 
        void connect_to_network();

        /**
         * Create our own access point
         */ 
        void start_ap();

        /* Take in new WiFi information store it to flash and restart the WiFi chip to put the new credentials into effect*/
        void storeNewWiFiCreds(String name, String password);

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
         * Return the current connection state of the WiFi module
         */ 
        bool isConnected(){ return WiFi.status() == WL_CONNECTED; };

        /**
         * Called by max to ignore WiFi verification requests
         */ 
        void useMax() {usingMax = true; };

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

        bool usingMax = false;              // If we are using max
        CommunicationMode mode;             // Current WiFi mode we are in

        IPAddress remoteIP;                 // IP address to send the UDP requests to 

        
};