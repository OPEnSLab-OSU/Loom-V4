#include "Loom_WiFi.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager& man, String name, String password) : Module("WiFi Manager"), manInst(&man), wifi_name(name), wifi_password(password) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager& man) : Module("WiFi Manager"), manInst(&man) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::initialize() {
    // The pins on the feather M0 WiFi are different than most boards
    WiFi.setPins(8, 7, 4, 2);

    printModuleName(); Serial.println("Initializing WiFi module...");

    if(WiFi.status() == WL_NO_SHIELD){
        printModuleName(); Serial.println("WINC1500 not present, WiFi functionality will be disabled");
        moduleInitialized = false;
    }
    else{

        // Enable low power mode to conserve power
        WiFi.maxLowPowerMode();

        // Call the power up class to connect to the wifi network
        power_up();

        // Give a bit more time to initialize the module
        delay(1000);

        hasInitialized = true;


        // Verify the wifi connection after we have connected
        printModuleName(); Serial.println("Verifying Connection to the Internet...");
        verifyConnection();
        
        printModuleName(); Serial.println("Successfully Initalized Wifi!");
        printModuleName(); Serial.println("Device IP Address: " + Loom_WIFI::IPtoString(getIPAddress()));
        printModuleName(); Serial.println("Device Subnet Address: " + Loom_WIFI::IPtoString(getSubnetMask()));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_up() {
    if(moduleInitialized){
        int retry_count = 0;
        printModuleName(); Serial.println("Attempting to connect to SSID: " + wifi_name);

        // If we are logging into a network with a password
        if(wifi_password.length() > 0){

            // While we are trying to connect to the wifi network
            while(WiFi.begin(wifi_name.c_str(), wifi_password.c_str()) != WL_CONNECTED){
                printModuleName(); Serial.println("Attempting to connect to AP...");
                delay(5000);
                retry_count++;

                // If after 10 attempts we still can't connect to the network we need to stop and break so we don't hang the device
                if(retry_count >= 10){
                    printModuleName(); Serial.println("Failed to connect to the access point after 10 tries! Is the network in range and are your credentials correct?");
                    return;
                }
            }
        }
        else{
            // While we are trying to connect to the wifi network
            while(WiFi.begin(wifi_name.c_str()) != WL_CONNECTED){
                printModuleName(); Serial.println("Attempting to connect to AP...");
                delay(5000);
                retry_count++;

                // If after 10 attempts we still can't connect to the network we need to stop and break so we don't hang the device
                if(retry_count >= 10){
                    printModuleName(); Serial.println("Failed to connect to the access point after 10 tries! Is the network in range and are your credentials correct?");
                    return;
                }
            }
        }

        printModuleName(); Serial.println("Connected to network!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_down(){
    if(moduleInitialized){
        // Disconnect and end the Wifi when we power down the device
        WiFi.disconnect();
        WiFi.end();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::verifyConnection(){
    if(hasInitialized && moduleInitialized){
        int pingLatency = WiFi.ping("www.google.com");
        if(pingLatency >= 0){
            printModuleName(); Serial.println("Successfully Pinged Google! Response Time: " + String(pingLatency) + "ms");
            return true;
        }
        else{
            printModuleName(); Serial.print("Ping Failed! Error Code: ");

            // Parse the error code into a human readable format
            switch (pingLatency)
            {
                case -1:
                    Serial.println("Destination_Unreachable");
                    break;
                case -2:
                    Serial.println("Ping_TimeOut");
                    break;
                case -3:
                    Serial.println("Unknown_Host");
                    break;

                default:
                    Serial.println("General_Error");
                    break;
                
            }
            return false;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::loadConfigFromJSON(JsonObject json){
    wifi_name = json["SSID"].as<String>();
    wifi_password = json["password"].as<String>();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getIPAddress(){
    IPAddress ip = WiFi.localIP();
    return ip;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getSubnetMask(){
    IPAddress subnet = WiFi.subnetMask();
    return subnet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getGateway(){
    IPAddress gateway = WiFi.gatewayIP();
    return gateway;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getBroadcast(){
    IPAddress boradcast = WiFi.gatewayIP();

    // Set the last one to 255 for the netmask
    boradcast[3] = 255;

    return boradcast;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////