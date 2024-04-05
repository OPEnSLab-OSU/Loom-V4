#pragma once

#include <WiFi101.h>
#include <WiFiUdp.h>
#include <FlashStorage.h>


#include "Loom_Manager.h"
#include "../NetworkComponent.h"
#include "../../../Hardware/Loom_BatchSD/Loom_BatchSD.h"

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
class Loom_WIFI : public NetworkComponent{
    protected:

        /* These aren't used with the Wifi manager */
        void measure() override {};

        bool getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) override;

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
        Loom_WIFI(Manager& man, CommunicationMode mode, const char* name = "", const char* password = "", int connectionRetries = 5);

        /**
         * Construct a new WiFi manager, passing the credentials in as a json document
         * @param man Reference to the manager
         * @param jsonString JSON string to pull the credentials from
         */
        Loom_WIFI(Manager& man);

        // Overridden isConnected for the NetworkComponent
        bool isConnected() override;

        /**
         * Load the Wifi credentials from a JSON string, used to pull credentials from a file
         *
         * This automatically frees the input json
         *
         * @param jsonString JSON formatted string containing the SSID and password
         */
        void loadConfigFromJSON(char* json);

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
        void storeNewWiFiCreds(const char* name, const char* password);

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
         * Called by max to ignore WiFi verification requests
         */
        void useMax() {usingMax = true; };

        /**
         * Set an instance of BatchSD to check if we need to power up
        */
        void setBatchSD(Loom_BatchSD& batch) { batchSD = &batch; };

        /**
         * Set the number of connection retries
        */
        void setMaxRetries(int retries) { connectionRetries = retries; };

        /**
         * Convert an IP address to a string
         */
        void ipToString(IPAddress ip, char array[16]) {
            snprintf(array, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        };

    private:
        Manager* manInst;                   // Pointer to the manager

        WiFiClient wifiClient;              // Wifi client that can be used with the MQTT client or other additional objects
        Loom_BatchSD* batchSD = nullptr;

        bool powerUp = true;                // Whether or not the WiFi should power up (used with batch uploads)

        char wifi_name[100];                // Access point to connect to
        char wifi_password[100];          // Password to connect to the access point
        int connectionRetries;

        bool usingMax = false;              // If we are using max
        bool firstInit = true;
        CommunicationMode mode;             // Current WiFi mode we are in

        IPAddress remoteIP;                 // IP address to send the UDP requests to


};