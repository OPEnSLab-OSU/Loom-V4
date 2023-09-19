#include "Loom_LTE.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LTE::Loom_LTE(Manager& man, const char* apn, const char* user, const char* pass, const int pin) : NetworkComponent("LTE"), manInst(&man), modem(SerialAT), client(modem){
    strncpy(this->APN, apn, 100);
    strncpy(this->gprsUser, user, 100);
    strncpy(this->gprsPass, pass, 100);
    this->powerPin = pin;

    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LTE::Loom_LTE(Manager& man) : NetworkComponent("LTE"), manInst(&man), modem(SerialAT), client(modem){
    manInst->registerModule(this);

    // Not initialized because we don't actually know what to connect to yet
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::initialize(){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char ip[16];
    // Set the pin to output so we can write to it
    pinMode(powerPin, OUTPUT);

    // Start up the module
    power_up();

    // Get the modem info
    char const* modemInfo = modem.getModemInfo().c_str();

    // If no LTE shield is found we should not initialize the module
    if(modemInfo == NULL){
        ERROR(F("LTE shield not detected! This can also be triggered if there isn't a SIM card in the board"));
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }
    else{
        snprintf(output, OUTPUT_SIZE, "Modem Information: %s", modemInfo);
    }

    // Connect to the LTE network
    moduleInitialized = connect();

    // If we successfully connected to the LTE network print out some information
    if(moduleInitialized){
        LOG(F("Connected!"));

        // Print APN
        snprintf(output, OUTPUT_SIZE, "APN: %s", APN);
        LOG(output);

        // Signal Quality
        snprintf(output, OUTPUT_SIZE, "Signal State: %i", modem.getSignalQuality());
        LOG(output);

        // Log IP address
        ipToString(modem.localIP(), ip);
        snprintf(output, OUTPUT_SIZE, "Device IP Address: %s", ip);
        LOG(output);

        //verifyConnection();
        LOG(F("Module successfully initialized!"));
    }
    else{
        ERROR(F("Module failed to initialize"));
    }

    firstInit = false;
    FUNCTION_END;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_up(){
    FUNCTION_START;
    // If the batch_sd is initialized and the current batch is one less than the maximum so we turn on the device before the last batch
    if(batch_sd != nullptr && !firstInit){
        if(batch_sd->getCurrentBatch() != batch_sd->getBatchSize()-1){
            powerUp = false;
            FUNCTION_END;
            return;
        }else{
            powerUp = true;
        }
    }
        
    // If not connected to a network we want to connect
    if(moduleInitialized){
        LOG(F("Powering up GPRS Modem. This should take about 10 seconds..."));
        TIMER_DISABLE;
        digitalWrite(powerPin, LOW);
        delay(10000);
        SerialAT.begin(9600);
        delay(6000);
        modem.restart();
        LOG(F("Powering up complete!"));
        TIMER_ENABLE;
    }
    // If the module isn't initialized we want to try again
    else{
        initialize();
    }
    
    if(!firstInit && !isConnected() && moduleInitialized)
            connect();
    
    FUNCTION_END;
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_down(){
    FUNCTION_START;
    if(moduleInitialized && powerUp){
        LOG(F("Powering down GPRS Modem. This should take about 5 seconds..."));
        modem.poweroff();
        digitalWrite(powerPin, HIGH);
        delay(5000);
        LOG(F("Powering down complete!"));
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::package(){
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["RSSI"] = modem.getSignalQuality();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::connect(){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    uint8_t attemptCount = 1; // Tracks number of attempts, 5 is a fail

    TIMER_DISABLE;
    do{
        LOG(F("Waiting for network..."));
        if(!modem.waitForNetwork()){
            ERROR(F("No Response from network!"));
            FUNCTION_END;
            return false;
        }

        if(!modem.isNetworkConnected()){
            ERROR(F("No connection to network!"));
            FUNCTION_END;
            return false;
        }

        LOG(F("Connected to network!"));

        // Connect to lte network
        snprintf(output, OUTPUT_SIZE, "Attempting to connect to LTE Network: %s", APN);
        LOG(output);
        if(modem.gprsConnect(APN, gprsUser, gprsPass)){
            LOG(F("Successfully Connected!"));
            FUNCTION_END;
            TIMER_ENABLE;
            return true;
        }
        else{
            snprintf(output, OUTPUT_SIZE, "Connection failed %u / 10. Retrying...", attemptCount);
            WARNING(output);
            delay(10000);
            attemptCount++;
        }

        // If the last attempt was the 5th attempt then stop
        if(attemptCount > 5){
            ERROR(F("Connection reattempts exceeded 10 tries. Connection Failed"));
            FUNCTION_END;
            TIMER_ENABLE;
            return false;
        }
    }while(!isConnected());
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::disconnect(){
    FUNCTION_START;
    if(moduleInitialized){
        modem.gprsDisconnect();
        delay(200);
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::verifyConnection(){
    FUNCTION_START;
    bool returnStatus =  false;
    LOG(F("Attempting to verify internet connection..."));
    
    // Connect to TinyGSM's creator's website
    if(!client.connect("vsh.pp.ua", 80)){
        ERROR(F("Failed to contact TinyGSM example your internet connection may not be completely established!"));
        client.stop();
        FUNCTION_END;
        return false;
    }
    else{

        // Request the logo.txt to display
        client.print("GET /TinyGSM/logo.txt HTTP/1.1\r\n");
        client.print("Host: vsh.pp.ua\r\n");
        client.print("Connection: close\r\n\r\n");
        client.println();

        // Print logo to screen
        uint32_t timeout = millis();
        while (client.connected() && millis() - timeout < 10000L) {
            // Print available data
            while (client.available()) {
                char c = client.read();
                Serial.print(c);
                timeout = millis();
            }
        }
        Serial.println();
        client.stop();
        returnStatus = true;
    }
    TIMER_RESET;
    FUNCTION_END;
    return true;
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::loadConfigFromJSON(char* json){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        snprintf(output, OUTPUT_SIZE, "There was an error reading the WIFI credentials from SD: %s", deserialError.c_str());
        ERROR(output);
    }

    // Check if apn is null
    if(!doc["apn"].isNull()){
        strncpy(APN, doc["apn"].as<const char*>(), 100);
        strncpy(gprsUser, doc["user"].as<const char*>(), 100);
        strncpy(gprsPass, doc["pass"].as<const char*>(), 100);
    }

    // If we are supplying a different power pin then use that one
    if(doc.containsKey("pin"))
        powerPin = doc["pin"].as<int>();

    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TinyGsmClient& Loom_LTE::getClient() { return client; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) {
    return modem.getNetworkTime(year, month, day, hour, minute, second, tz);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

