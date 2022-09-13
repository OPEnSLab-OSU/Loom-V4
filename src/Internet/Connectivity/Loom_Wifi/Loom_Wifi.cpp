#include "Loom_Wifi.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager& man, CommunicationMode mode, String name, String password) : Module("WiFi"), manInst(&man), mode(mode) {
    if(mode == CommunicationMode::AP && name.length() <= 0){
        wifi_name = manInst->get_device_name() + String(manInst->get_instance_num());
        
    }else{
        wifi_name = name;
    }

    wifi_password = password;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager& man) : Module("WiFi"), manInst(&man) {
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

        // Only try to verify if we have connected to a network
        if(mode != CommunicationMode::AP && !usingMax){
            // Verify the wifi connection after we have connected
            printModuleName(); Serial.println("Verifying Connection to the Internet...");
            verifyConnection();
        }
        
        printModuleName(); Serial.println("Successfully Initalized Wifi!");
        printModuleName(); Serial.println("Device IP Address: " + Loom_WIFI::IPtoString(getIPAddress()));
        printModuleName(); Serial.println("Device Subnet Address: " + Loom_WIFI::IPtoString(getSubnetMask()));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::package(){
    JsonObject json = manInst->get_data_object(getModuleName());
    json["SSID"] = WiFi.SSID();
    json["RSSI"] = WiFi.RSSI();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_up() {
    if(moduleInitialized){
        if(mode == CommunicationMode::CLIENT)
            connect_to_network();
        else
            start_ap();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::connect_to_network(){
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
            printModuleName(); Serial.println("Attempting to connect to AP (Attempt " + String(retry_count+1) + " of 10)...");
            delay(5000);
            retry_count++;

            // If after 10 attempts we still can't connect to the network we need to stop and break so we don't hang the device
            if(retry_count >= 10){
                printModuleName(); Serial.println("Failed to connect to the access point after 10 tries! Is the network in range and are your credentials correct?");

                // Switch over to AP mode if using max
                if(usingMax){
                    printModuleName(); Serial.println("Starting access point as backup!");
                    mode = CommunicationMode::AP;
                    wifi_name = manInst->get_device_name() + String(manInst->get_instance_num());
                    start_ap();
                }
                return;
            }
        }
    }

    printModuleName(); Serial.println("Connected to network!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::start_ap(){
    printModuleName(); Serial.println("Starting access point on: " + wifi_name);

    auto status = WiFi.beginAP(wifi_name.c_str());

    // If the AP is not listening print an error
    if(status != WL_AP_LISTENING){
        printModuleName(); Serial.println("Access point creation failed!");
        return;
    }

    // Wait 10 seconds for the AP to start up
    printModuleName(); Serial.println("Waiting for a device to connect to the access point...");
    while(WiFi.status() != WL_AP_CONNECTED);
    printModuleName(); Serial.println("Device connected to AP!");
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
void Loom_WIFI::loadConfigFromJSON(String json){

    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        printModuleName(); Serial.println("There was an error reading the WIFI credentials from SD: " + String(deserialError.c_str()));
    }
    
    wifi_name = doc["SSID"].as<String>();
    wifi_password = doc["password"].as<String>();
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
    IPAddress broadcast;
    if(mode == CommunicationMode::CLIENT)
        broadcast = getGateway();
    else
        broadcast = getIPAddress();

    // Set the last one to 255 for the netmask
    broadcast[3] = 255;

    return broadcast;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////