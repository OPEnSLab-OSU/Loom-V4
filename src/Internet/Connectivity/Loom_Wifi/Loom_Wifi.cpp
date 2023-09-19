#include "Loom_Wifi.h"
#include "Logger.h"

// Reserve a section of memory called WiFi config
FlashStorage(WiFiConfig, WifiInfo);

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager& man, CommunicationMode mode, const char* name, const char* password, int connectionRetries) : NetworkComponent("WiFi"), manInst(&man), mode(mode), connectionRetries(connectionRetries){
    if(mode == CommunicationMode::AP && strlen(name) <= 0){
        snprintf(wifi_name, 100, "%s%i", manInst->get_device_name(), manInst->get_instance_num());
    }else{
        strncpy(wifi_name, name, 100);
    }

    strncpy(wifi_password, password, 100);
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager& man) : NetworkComponent("WiFi"), manInst(&man) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::initialize() {
    FUNCTION_START;
    // The pins on the feather M0 WiFi are different than most boards
    WiFi.setPins(8, 7, 4, 2);
    char output[OUTPUT_SIZE];
    char ip[16];

    LOG(F("Initializing WiFi module..."));

    if(WiFi.status() == WL_NO_SHIELD){
        ERROR(F("WINC1500 not present, WiFi functionality will be disabled"));
        moduleInitialized = false;
    }
    else{

        // Enable low power mode to conserve power
        WiFi.maxLowPowerMode();

        powerUp = true;
        // Call the power up class to connect to the wifi network
        power_up();

        // Give a bit more time to initialize the module
        delay(1000);


        // Only try to verify if we have connected to a network
        if(mode != CommunicationMode::AP && !usingMax){
            // Verify the wifi connection after we have connected
            LOG(F("Verifying Connection to the Internet..."));
            verifyConnection();
        }

        if(moduleInitialized){
            LOG(F("Successfully Initalized Wifi!"));


            // Print the device IP
            ipToString(getIPAddress(), ip);
            snprintf(output, OUTPUT_SIZE, "Device IP Address: %s", ip);
            LOG(output);

            ipToString(getSubnetMask(), ip);
            snprintf(output, OUTPUT_SIZE, "Device Subnet Address: %s", ip);
            LOG(output);
        }else{
            ERROR(F("Failed to initialize Wifi!"));
        }      
    }
    firstInit = false;
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::package(){
    FUNCTION_START;
    if(moduleInitialized){
        if(powerUp){
            JsonObject json = manInst->get_data_object(getModuleName());
            json[F("SSID")] = WiFi.SSID();
            json[F("RSSI")] = WiFi.RSSI();
        }else{
            JsonObject json = manInst->get_data_object(getModuleName());
            json[F("SSID")] = wifi_name;
            json[F("RSSI")] = 0;
        }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_up() {
    // If batchSD is defined and our current batch is not equal to one less than the needed for publishing dont power up
    if(batchSD != nullptr && !firstInit){
        if(batchSD->getCurrentBatch() != batchSD->getBatchSize()-1){ 
            WARNING(F("Not ready to publish, WIFI will not be powered up"));
            powerUp = false;
            return; 
        }else{
            powerUp = true;
        }
    }

    if(moduleInitialized && powerUp){
        

        // Check if we are going through our power up and are using max
        if(usingMax){
            // If so we want to read the flash memory to see if we have set data yet
            WifiInfo info = WiFiConfig.read();

            // Read the data in if we have set it before
            if(info.is_valid != false){
                strncpy(wifi_name, info.name, 100);
                strncpy(wifi_password, info.password, 100);
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
    FUNCTION_START;
    int retry_count = 0;
    char output[OUTPUT_SIZE];

    snprintf(output, OUTPUT_SIZE, "Attempting to connect to SSID: %s", wifi_name);
    LOG(output);
    TIMER_DISABLE;

    // If we are logging into a network with a password
    if(strlen(wifi_password) > 0){

        // While we are trying to connect to the wifi network
        while(WiFi.begin(wifi_name, wifi_password) != WL_CONNECTED){
            LOG(F("Attempting to connect to AP..."));
            delay(5000);
            retry_count++;

            // If after 10 attempts we still can't connect to the network we need to stop and break so we don't hang the device
            if(retry_count >= connectionRetries){
                ERROR(F("Failed to connect to the access point after allotted tries! Is the network in range and are your credentials correct?"));
                
                // Switch over to AP mode if using max
                if(usingMax){
                    LOG(F("Starting access point as backup!"));
                    mode = CommunicationMode::AP;
                    snprintf(wifi_name, 100, "%s%i", manInst->get_device_name(), manInst->get_instance_num());
                    start_ap();
                }
                TIMER_ENABLE;
                FUNCTION_END;
                return;
            }
        }
    }
    else{
        // While we are trying to connect to the wifi network
        while(WiFi.begin(wifi_name) != WL_CONNECTED){
            snprintf(output, OUTPUT_SIZE, "Attempting to connect to AP (Attempt %i)...", retry_count+1);
            LOG(output);
            delay(5000);
            retry_count++;

            // If after 10 attempts we still can't connect to the network we need to stop and break so we don't hang the device
            if(retry_count >= connectionRetries){
                ERROR(F("Failed to connect to the access point after allotted tries! Is the network in range and are your credentials correct?"));

                // Switch over to AP mode if using max
                if(usingMax){
                    LOG(F("Starting access point as backup!"));
                    mode = CommunicationMode::AP;
                    snprintf(wifi_name, 100, "%s%i", manInst->get_device_name(), manInst->get_instance_num());
                    start_ap();
                }
                TIMER_ENABLE;
                FUNCTION_END;
                return;
            }
        }
    }

    LOG(F("Connected to network!"));
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::start_ap(){
    FUNCTION_START;
    TIMER_DISABLE;
    char output[OUTPUT_SIZE];
    snprintf(output, OUTPUT_SIZE, "Starting access point on: %s", wifi_name);
    LOG(output);

    auto status = WiFi.beginAP(wifi_name);

    // If the AP is not listening print an error
    if(status != WL_AP_LISTENING){
        ERROR(F("Access point creation failed!"));
        FUNCTION_END;
        return;
    }

    // Wait 10 seconds for the AP to start up
    LOG(F("Waiting for a device to connect to the access point..."));
    while(WiFi.status() != WL_AP_CONNECTED);
    LOG(F("Device connected to AP!"));
    TIMER_ENABLE;
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_down(){
    if(powerUp){
        // Disconnect and end the Wifi when we power down the device
        WiFi.disconnect();
        WiFi.end();
        delay(1000);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::isConnected() { 
    if(mode == CommunicationMode::CLIENT)
        return WiFi.status() == WL_CONNECTED;
    else
        return WiFi.status() == WL_AP_CONNECTED;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::verifyConnection(){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    if(moduleInitialized && powerUp){
        int pingLatency = WiFi.ping("www.google.com");
        if(pingLatency >= 0){
            snprintf(output, OUTPUT_SIZE, "Successfully Pinged Google! Response Time: %ims", pingLatency);
            LOG(output);
            FUNCTION_END;
            return true;
        }
        else{
            LOG(F("Ping Failed! Error Code: "));

            // Parse the error code into a human readable format
            switch (pingLatency)
            {
                case -1:
                    LOG(F("Ping Failed! Error Code: Destination_Unreachable"));
                    break;
                case -2:
                    LOG(F("Ping Failed! Error Code: Ping_TimeOut"));
                    break;
                case -3:
                    LOG(F("Ping Failed! Error Code: Unknown_Host"));
                    break;

                default:
                    LOG(F("Ping Failed! Error Code: General_Error"));
                    break;
                
            }
            FUNCTION_END;
            return false;
        }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::loadConfigFromJSON(char* json){
    FUNCTION_START;

    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    char output[OUTPUT_SIZE];
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        snprintf(output, OUTPUT_SIZE, "There was an error reading the WIFI credentials from SD: %s", deserialError.c_str());
        ERROR(output);
    }

    // Only update the wifi creds if the data was not NULL
    if(!doc["SSID"].isNull()){
        strncpy(wifi_name, doc["SSID"].as<const char*>(), 100);
        strncpy(wifi_password, doc["password"].as<const char*>(), 100);
    }

    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::storeNewWiFiCreds(const char* name, const char* password){
    FUNCTION_START;
    // Write the new info to the flash memory
    LOG(F("Writing new WiFi credentials to flash..."));
    WifiInfo info;
    info.is_valid = true;
    strncpy(info.name, name, 100);
    strncpy(info.password, password, 100);
    WiFiConfig.write(info);
    LOG(F("Information written to flash!"));

    // Power cycle the board
    LOG(F("Power cycling the WiFi chip..."));
    mode = CLIENT;
    power_down();
    delay(1000);
    power_up();
    FUNCTION_END;
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

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) {
    unsigned long unixtime = WiFi.getTime();
    if(unixtime != 0){
        DateTime time = DateTime(unixtime);
        Serial.println(unixtime);
        *year = time.year();
        *month = time.month();
        *day = time.day();
        *hour = time.hour();
        *minute = time.minute();
        *second = time.second();
        return true;
    }else{
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
