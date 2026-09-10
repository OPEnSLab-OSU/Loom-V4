#include "Loom_LTE.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LTE::Loom_LTE(Manager& man, const char* apn, const char* user, const char* pass, const int pin, LTE_VERSION version) : NetworkComponent("LTE"), manInst(&man), modem(SerialAT), client(modem){
    strncpy(this->APN, apn, 100);
    strncpy(this->gprsUser, user, 100);
    strncpy(this->gprsPass, pass, 100);
    this->powerPin = pin;


    lteBoardVersion = version;
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
void Loom_LTE::powerBoardOn(){

    // Handle powering on the parkfun board
    if(lteBoardVersion == SPARKFUN){
        int16_t waitMs = 3000;
        pinMode(powerPin, OUTPUT);
        digitalWrite(powerPin, LOW);
        delay(waitMs);
        pinMode(powerPin, INPUT);
    }
    // Use opens board power on pin mode
    else{
        pinMode(powerPin, OUTPUT);
        digitalWrite(powerPin, HIGH);
        delay(5000);
        pinMode(powerPin, INPUT);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::powerBoardOff(){
    // NOTE: We don't need to power off the sparkfun LTE board we can just use the power off command
    // Handle powering off the parkfun board
    if(lteBoardVersion == OPENS){
         pinMode(powerPin, OUTPUT);
        digitalWrite(powerPin, LOW);
        delay(2500);
        pinMode(powerPin, INPUT);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::initialize(){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    // Set the pin to output so we can write to it
    pinMode(powerPin, INPUT);

    // Start up the module
    power_up();

    moduleInitialized = currState == LTEState::CONNECTED;

    // Get the modem info
    char const* modemInfo = modem.getModemName().c_str();

    // If no LTE shield is found we should not initialize the module
    if(modemInfo == NULL){
        ERROR(F("LTE shield not detected! This can also be triggered if there isn't a SIM card in the board"));
        currState = LTEState::ERROR;
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }

    firstInit = false;
    FUNCTION_END;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_up(){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char ip[16];

    if(currState == LTEState::ERROR){
        reset();
    }
 
    // If the batch_sd is initialized and the current batch is one less than the maximum so we turn on the device before the last batch
    if(batch_sd != nullptr && !firstInit){
        if(batch_sd->getCurrentBatch() != batch_sd->getBatchSize()-1){
            currState = LTEState::SLEEPING;
            FUNCTION_END;
            return;
        }else{
            currState = LTEState::OFF;
        }
    }

    // If not connected to a network we want to connect
    if(currState == LTEState::OFF){
        LOG(F("Powering up GPRS Modem. This should take about 10 seconds..."));
        TIMER_DISABLE;
        // Power on whatever the currently used LTE board is
        powerBoardOn();

        // Delay an additional one second to allow communication to open up
        SerialAT.begin(9600);
        delay(1000);
        bool init = false;
        for(int retries = 1; retries < 6; retries++){
            snprintf(output, OUTPUT_SIZE, "Attempt %d of powering on modem", retries);
            LOG(output);
            if(modem.init()){
                init = true;
                break;
            }
            delay(500);
        }
        if(init){
            LOG(F("Powering up complete!"));
            currState = LTEState::DISCONNECTED;
        }else{
            ERROR(F("Modem did not power on. Attempting recovery."));
            currState = LTEState::ERROR;
            reset();
        }
        TIMER_ENABLE;
    }
    
    if(currState == LTEState::DISCONNECTED){
        connect();
        if(currState == LTEState::CONNECTING){
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

            if(verifyConnection()){
                currState = LTEState::CONNECTED;
                LOG(F("Module successfully connected!"));
            }else{
                ERROR(F("Module could not reach server"));
                currState = LTEState::DISCONNECTED;
            }
        }
    }

    FUNCTION_END;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_down(){
    FUNCTION_START;
    if (currState == LTEState::CONNECTED) {
        LOG(F("Powering down GPRS Modem. This should take about 5 seconds..."));
        TIMER_ENABLE;
        if(disconnect()){
            TIMER_DISABLE;
            LOG(F("Modem succesfully disconnected from the internet"));
            currState = LTEState::DISCONNECTED;
            if(modem.poweroff()){
                LOG(F("Powering down complete!"));
                currState = LTEState::OFF;
            }
        }
        else{
            TIMER_DISABLE;
            LOG(F("Device did not disconnect from the network. "));
        }

    }
    else{
        ERROR(F("Device is not currently connected. Shutting down"));
        TIMER_ENABLE;
        if(modem.poweroff()){
            TIMER_DISABLE;
            LOG(F("Powering down complete!"));
            currState = LTEState::OFF;
        }
    }
    TIMER_DISABLE;
    FUNCTION_END;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::reset(){
    FUNCTION_START;
    TIMER_DISABLE;
    LOG("Attempting LTE recovery. This may take up to 5 seconds.");
    // tells modem to leave data mode and enter command mode
    delay(1100);                  
    SerialAT.print("+++");        
    delay(1100);  
    TIMER_ENABLE;
    if(modem.init()){
        LOG("Successfully recovered");
        currState = LTEState::DISCONNECTED;
        FUNCTION_END;
        return;
    }

    TIMER_DISABLE;
    LOG("Recovery failed. Attempting LTE software reset. This may take up to 20 seconds");

    // Manually force a soft reset command down the line 
    SerialAT.println("AT+CFUN=15"); 
    
    // Give the u-blox operating system 15-20 seconds to log off and reboot cleanly
    delay(20000); 
    
    TIMER_ENABLE;
    // Try initializing one final time
    if (modem.init()) {
        Serial.println("Modem recovered via software reboot!");
        currState = LTEState::DISCONNECTED;
        FUNCTION_END;
        return;
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
    currState = LTEState::REGISTERING;
    TIMER_DISABLE;
    do{
        LOG(F("Waiting for network..."));
        if(!modem.waitForNetwork()){
            ERROR(F("No Response from network!"));
            TIMER_ENABLE;
            FUNCTION_END;
            return false;
        }
        if(!modem.isNetworkConnected()){
            ERROR(F("No connection to network!"));
            TIMER_ENABLE;
            FUNCTION_END;
            return false;
        }

        LOG(F("Registered to carrier!"));
        currState = LTEState::CONNECTING;

        // Connect to lte network
        snprintf(output, OUTPUT_SIZE, "Attempting to connect to LTE Network: %s", APN);
        LOG(output);
        if(modem.gprsConnect(APN, gprsUser, gprsPass)){
            LOG(F("Successfully Connected!"));
            delay(6000);
            TIMER_ENABLE;
            FUNCTION_END;
            return true;
        }
        else{
            snprintf(output, OUTPUT_SIZE, "Connection failed %u / 5. Retrying...", attemptCount);
            WARNING(output);
            delay(10000);
            attemptCount++;
        }

        // If the last attempt was the 5th attempt then stop
        if(attemptCount > 5){
            ERROR(F("Connection reattempts exceeded 5 tries. Connection Failed"));
            TIMER_ENABLE;
            FUNCTION_END;
            return false;
            
        }
    }while(!isConnected());

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::disconnect(){
    FUNCTION_START;
    bool disconnected;

    if(disconnected = modem.gprsDisconnect()){
        delay(200);
    }

    FUNCTION_END;
    return disconnected;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::verifyConnection(){
    FUNCTION_START;
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

        // dont care about what the response is, as long as we got a response.
        uint32_t timeout = millis();
        while (client.connected() && millis() - timeout < 10000) {
            if (client.available()) {
                client.stop();

                LOG(F("Internet connection verified"));
                TIMER_RESET;
                FUNCTION_END;
                return true;
            }
        }
        client.stop();
        LOG(F("Connected to server but received no response"));
        TIMER_RESET;
        FUNCTION_END;
        return false;
    }

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::loadConfigFromJSON(char* json){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, (const char *)json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        snprintf(output, OUTPUT_SIZE, "There was an error reading the LTE credentials from SD: %s", deserialError.c_str());
        ERROR(output);
        return;
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
Client* Loom_LTE::getClient() { return (Client*)&client; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) {
    // Get the timezone that we are in converted to an int
    int tzInt = (int)*tz;

    // Pull the current values from the GSM
    if(modem.getNetworkTime(year, month, day, hour, minute, second, tz)){
        // Create a date time object and then add the TimeZone back to get UTC time
        DateTime time = DateTime(*year, *month, *day, *hour, *minute, *second) + TimeSpan(0,((int)(*tz))*(-1),0,0);
        *year = time.year();
        *month = time.month();
        *day = time.day();
        *hour = time.hour();
        *minute = time.minute();
        *second = time.second();
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

