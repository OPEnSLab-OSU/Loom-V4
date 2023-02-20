#include "Loom_LTE.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LTE::Loom_LTE(Manager& man, const String apn, const String user, const String pass, const int pin) : Module("LTE"), manInst(&man), modem(SerialAT), client(modem){
    this->APN = apn;
    this->gprsUser = user;
    this->gprsPass = pass;
    this->powerPin = pin;

    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LTE::Loom_LTE(Manager& man) : Module("LTE"), manInst(&man), modem(SerialAT), client(modem){
    manInst->registerModule(this);

    // Not initialized because we don't actually know what to connect to yet
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::initialize(){
    FUNCTION_START;
    // Set the pin to output so we can write to it
    pinMode(powerPin, OUTPUT);

    // Start up the module
    power_up();

    // Get the modem info
    String modemInfo = modem.getModemInfo();

    // If no LTE shield is found we should not initialize the module
    if(modemInfo == NULL){
        LOG("LTE shield not detected! This can also be triggered if there isn't a SIM card in the board");
        moduleInitialized = false;
        return;
    }
    else{
        LOG("Modem Information: " + modemInfo);
    }

    // Connect to the LTE network
    moduleInitialized = connect();

    // If we successfully connected to the LTE network print out some information
    if(moduleInitialized){
        LOG("Connected!");
        LOG("APN: " + APN);
        LOG("Signal State: " + String(modem.getSignalQuality()));
        LOG("IP Address: " + Loom_LTE::IPtoString(modem.localIP()));

        verifyConnection();

        LOG("Module successfully initialized!");
    }
    else{
        LOG("Module failed to initialize");
    }

    firstInit = false;
    FUNCTION_END("void");

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_up(){
    FUNCTION_START;
    // If the batch_sd is initialized and the current batch is one less than the maximum so we turn on the device before the last batch
    if((batch_sd != nullptr && (batch_sd->getCurrentBatch() != batch_sd->getBatchSize()-1)) && !firstInit){
       return;
    }
        
    // If not connected to a network we want to connect
    if(moduleInitialized){
        Watchdog.disable();
        LOG("Powering up GPRS Modem. This should take about 10 seconds...");
        digitalWrite(powerPin, LOW);
        delay(10000);
        SerialAT.begin(9600);
        delay(6000);
        modem.restart();
        LOG("Powering up complete!");
        Watchdog.enable(WATCHDOG_TIMEOUT);
    }

    // If the module isn't initialized we want to try again
    else{
        initialize();
    }

    if(!firstInit && !isConnected() && moduleInitialized)
            connect();
    FUNCTION_END("ret");
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_down(){
    FUNCTION_START;
    if(moduleInitialized){
        LOG("Powering down GPRS Modem. This should take about 5 seconds...");
        modem.poweroff();
        digitalWrite(powerPin, HIGH);
        delay(5000);
        LOG("Powering down complete!");
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::package(){
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["RSSI"] = modem.getSignalQuality();
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::connect(){
    FUNCTION_START;
    uint8_t attemptCount = 1; // Tracks number of attempts, 5 is a fail

    Watchdog.disable();
    do{
        LOG("Waiting for network...");
        if(!modem.waitForNetwork()){
            LOG("No Response from network!");
            FUNCTION_END(false);
            return false;
        }

        if(!modem.isNetworkConnected()){
            LOG("No connection to network!");
            FUNCTION_END(false);
            return false;
        }

        LOG("Connected to network!");

        // Connect to lte network
        LOG("Attempting to connect to LTE Network: " + APN);
        if(modem.gprsConnect(APN.c_str(), gprsUser.c_str(), gprsPass.c_str())){
            LOG("Successfully Connected!");
            Watchdog.enable(WATCHDOG_TIMEOUT);
            FUNCTION_END(true);
            return true;
        }
        else{
            LOG("Connection failed " + String(attemptCount) + "/ 10. Retrying...");
            delay(10000);
            attemptCount++;
        }

        // If the last attempt was the 5th attempt then stop
        if(attemptCount > 5){
            LOG("Connection reattempts exceeded 10 tries. Connection Failed");
            Watchdog.enable(WATCHDOG_TIMEOUT);
            FUNCTION_END(false);
            return false;
        }
    }while(!isConnected());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::disconnect(){
    FUNCTION_START;
    if(moduleInitialized){
        modem.gprsDisconnect();
        delay(200);
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::verifyConnection(){
    bool returnStatus =  false;
    LOG("Attempting to verify internet connection...");
    
    // Connect to TinyGSM's creator's website
    if(!client.connect("vsh.pp.ua", 80)){
        LOG("Failed to contact TinyGSM example your internet connection may not be completely established!");
        client.stop();
    }
    else{

        // Request the logo.txt to display
        client.print(String("GET ") + "/TinyGSM/logo.txt" + " HTTP/1.1\r\n");
        client.print(String("Host: ") + "vsh.pp.ua" + "\r\n");
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
    Watchdog.reset();
    return returnStatus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::loadConfigFromJSON(String json){
    FUNCTION_START;
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        LOG("There was an error reading the sleep interval from SD: " + String(deserialError.c_str()));
    }

    APN = doc["apn"].as<String>();
    gprsUser = doc["user"].as<String>();
    gprsPass = doc["pass"].as<String>();

    // If we are supplying a different power pin then use that one
    if(doc.containsKey("pin"))
        powerPin = doc["pin"].as<int>();

    moduleInitialized = true;
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::isConnected(){ return modem.isGprsConnected(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TinyGsmClient& Loom_LTE::getClient() { return client; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

