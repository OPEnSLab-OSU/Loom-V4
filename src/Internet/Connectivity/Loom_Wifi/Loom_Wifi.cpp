#include "Loom_Wifi.h"
#include "Logger.h"

// Reserve a section of memory called WiFi config
FlashStorage(WiFiConfig, WifiInfo);

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

    LOG("Initializing WiFi module...");

    if(WiFi.status() == WL_NO_SHIELD){
        ERROR("WINC1500 not present, WiFi functionality will be disabled");
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
            LOG("Verifying Connection to the Internet...");
            moduleInitialized = verifyConnection();
        }
        
        if(moduleInitialized){
            LOG("Successfully Initalized Wifi!");
            LOG("Device IP Address: " + Loom_WIFI::IPtoString(getIPAddress()));
            LOG("Device Subnet Address: " + Loom_WIFI::IPtoString(getSubnetMask()));
        }else{
            ERROR("Failed to initialize Wifi!");
        }      
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::package(){
    if(moduleInitialized && powerUp){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["SSID"] = WiFi.SSID();
        json["RSSI"] = WiFi.RSSI();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_up() {
    // If batchSD is defined and our current batch is not equal to one less than the needed for publishing dont power up
    if(hasInitialized && batchSD != nullptr && batchSD->getCurrentBatch() != batchSD->getBatchSize()-1){ 
        WARNING("Not ready to publish, WIFI will not be powered up");
        powerUp = false;
        return; 
    }else{
        powerUp = true;
    }

    if(moduleInitialized && powerUp){
        

        // Check if we are going through our power up and are using max
        if(usingMax){
            // If so we want to read the flash memory to see if we have set data yet
            WifiInfo info = WiFiConfig.read();

            // Read the data in if we have set it before
            if(info.is_valid != false){
                wifi_name = String(info.name);
                wifi_password = String(info.password);
            }
        }

        // Initialize the access point mode or connect to a router 
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
    LOG("Attempting to connect to SSID: " + wifi_name);
    TIMER_DISABLE;

    // If we are logging into a network with a password
    if(wifi_password.length() > 0){

        // While we are trying to connect to the wifi network
        while(WiFi.begin(wifi_name.c_str(), wifi_password.c_str()) != WL_CONNECTED){
            LOG("Attempting to connect to AP...");
            delay(5000);
            retry_count++;

            // If after 10 attempts we still can't connect to the network we need to stop and break so we don't hang the device
            if(retry_count >= 10){
                ERROR("Failed to connect to the access point after 10 tries! Is the network in range and are your credentials correct?");
                
                // Switch over to AP mode if using max
                if(usingMax){
                    LOG("Starting access point as backup!");
                    mode = CommunicationMode::AP;
                    wifi_name = manInst->get_device_name() + String(manInst->get_instance_num());
                    start_ap();
                }
                TIMER_ENABLE;
                return;
            }
        }
    }
    else{
        // While we are trying to connect to the wifi network
        while(WiFi.begin(wifi_name.c_str()) != WL_CONNECTED){
            LOG("Attempting to connect to AP (Attempt " + String(retry_count+1) + " of 10)...");
            delay(5000);
            retry_count++;

            // If after 10 attempts we still can't connect to the network we need to stop and break so we don't hang the device
            if(retry_count >= 10){
                ERROR("Failed to connect to the access point after 10 tries! Is the network in range and are your credentials correct?");

                // Switch over to AP mode if using max
                if(usingMax){
                    LOG("Starting access point as backup!");
                    mode = CommunicationMode::AP;
                    wifi_name = manInst->get_device_name() + String(manInst->get_instance_num());
                    start_ap();
                }
                TIMER_ENABLE;
                return;
            }
        }
    }

    LOG("Connected to network!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::start_ap(){
    TIMER_DISABLE;
    LOG("Starting access point on: " + wifi_name);

    auto status = WiFi.beginAP(wifi_name.c_str());

    // If the AP is not listening print an error
    if(status != WL_AP_LISTENING){
        ERROR("Access point creation failed!");
        return;
    }

    // Wait 10 seconds for the AP to start up
    LOG("Waiting for a device to connect to the access point...");
    while(WiFi.status() != WL_AP_CONNECTED);
    LOG("Device connected to AP!");
    TIMER_ENABLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_down(){
    if(powerUp){
        // Disconnect and end the Wifi when we power down the device
        WiFi.disconnect();
        WiFi.end();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::verifyConnection(){
    if(hasInitialized && moduleInitialized && powerUp){
        int pingLatency = WiFi.ping("www.google.com");
        if(pingLatency >= 0){
            LOG("Successfully Pinged Google! Response Time: " + String(pingLatency) + "ms");
            return true;
        }
        else{
            LOG("Ping Failed! Error Code: ");

            // Parse the error code into a human readable format
            switch (pingLatency)
            {
                case -1:
                    LOG("Ping Failed! Error Code: Destination_Unreachable");
                    break;
                case -2:
                    LOG("Ping Failed! Error Code: Ping_TimeOut");
                    break;
                case -3:
                    LOG("Ping Failed! Error Code: Unknown_Host");
                    break;

                default:
                    LOG("Ping Failed! Error Code: General_Error");
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
        ERROR("There was an error reading the WIFI credentials from SD: " + String(deserialError.c_str()));
    }

    // Only update the wifi creds if the data was not NULL
    if(!doc["SSID"].isNull()){
        wifi_name = doc["SSID"].as<String>();
        wifi_password = doc["password"].as<String>();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::storeNewWiFiCreds(String name, String password){
    // Write the new info to the flash memory
    LOG("Writing new WiFi credentials to flash...");
    WifiInfo info;
    info.is_valid = true;
    name.toCharArray(info.name, 100);
    password.toCharArray(info.password, 100);
    WiFiConfig.write(info);
    LOG("Information written to flash!");

    // Power cycle the board
    LOG("Power cycling the WiFi chip...");
    mode = CLIENT;
    power_down();
    delay(1000);
    power_up();
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
    IPAddress broadcast = WiFi.gatewayIP();

    // Set the last one to 255 for the netmask
    broadcast[3] = 255;

    return broadcast;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
