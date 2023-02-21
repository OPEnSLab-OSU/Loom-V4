#include "Loom_LTE.h"

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

    // Set the pin to output so we can write to it
    pinMode(powerPin, OUTPUT);

    // Start up the module
    power_up();

    // Get the modem info
    String modemInfo = modem.getModemInfo();

    // If no LTE shield is found we should not initialize the module
    if(modemInfo == NULL){
        printModuleName("LTE shield not detected! This can also be triggered if there isn't a SIM card in the board");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName("Modem Information: " + modemInfo);
    }

    // Connect to the LTE network
    moduleInitialized = connect();

    // If we successfully connected to the LTE network print out some information
    if(moduleInitialized){
        printModuleName("Connected!");
        printModuleName("APN: " + APN);
        printModuleName("Signal State: " + String(modem.getSignalQuality()));
        printModuleName("IP Address: " + Loom_LTE::IPtoString(modem.localIP()));

        verifyConnection();

        printModuleName("Module successfully initialized!");
    }
    else{
        printModuleName("Module failed to initialize");
    }

    firstInit = false;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_up(){
    // If the batch_sd is initialized and the current batch is one less than the maximum so we turn on the device before the last batch
    if((batch_sd != nullptr && (batch_sd->getCurrentBatch() != batch_sd->getBatchSize()-1)) && !firstInit){
       return;
    }
        
    // If not connected to a network we want to connect
    if(moduleInitialized){
        TIMER_DISABLE;
        printModuleName("Powering up GPRS Modem. This should take about 10 seconds...");
        digitalWrite(powerPin, LOW);
        delay(10000);
        SerialAT.begin(9600);
        delay(6000);
        modem.restart();
        printModuleName("Powering up complete!");
        TIMER_ENABLE;
    }

    // If the module isn't initialized we want to try again
    else{
        initialize();
    }

    if(!firstInit && !isConnected() && moduleInitialized)
            connect();
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_down(){
    if(moduleInitialized){
        printModuleName("Powering down GPRS Modem. This should take about 5 seconds...");
        modem.poweroff();
        digitalWrite(powerPin, HIGH);
        delay(5000);
        printModuleName("Powering down complete!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::package(){
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["RSSI"] = modem.getSignalQuality();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::connect(){
    uint8_t attemptCount = 1; // Tracks number of attempts, 5 is a fail

    TIMER_DISABLE;
    do{
        printModuleName("Waiting for network...");
        if(!modem.waitForNetwork()){
            printModuleName("No Response from network!");
            return false;
        }

        if(!modem.isNetworkConnected()){
            printModuleName("No connection to network!");
            return false;
        }

        printModuleName("Connected to network!");

        // Connect to lte network
        printModuleName("Attempting to connect to LTE Network: " + APN);
        if(modem.gprsConnect(APN.c_str(), gprsUser.c_str(), gprsPass.c_str())){
            printModuleName("Successfully Connected!");
            TIMER_ENABLE;
            return true;
        }
        else{
            printModuleName("Connection failed " + String(attemptCount) + "/ 10. Retrying...");
            delay(10000);
            attemptCount++;
        }

        // If the last attempt was the 5th attempt then stop
        if(attemptCount > 5){
            printModuleName("Connection reattempts exceeded 10 tries. Connection Failed");
            TIMER_ENABLE;
            return false;
        }
    }while(!isConnected());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::disconnect(){
    if(moduleInitialized){
        modem.gprsDisconnect();
        delay(200);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::verifyConnection(){
    printModuleName("Attempting to verify internet connection...");
    
    // Connect to TinyGSM's creator's website
    if(!client.connect("vsh.pp.ua", 80)){
        printModuleName("Failed to contact TinyGSM example your internet connection may not be completely established!");
        client.stop();
        return false;
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
    }
    return true;
    TIMER_RESET;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::loadConfigFromJSON(String json){
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        printModuleName("There was an error reading the sleep interval from SD: " + String(deserialError.c_str()));
    }

    APN = doc["apn"].as<String>();
    gprsUser = doc["user"].as<String>();
    gprsPass = doc["pass"].as<String>();

    // If we are supplying a different power pin then use that one
    if(doc.containsKey("pin"))
        powerPin = doc["pin"].as<int>();

    moduleInitialized = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::isConnected(){ return modem.isGprsConnected(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TinyGsmClient& Loom_LTE::getClient() { return client; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

