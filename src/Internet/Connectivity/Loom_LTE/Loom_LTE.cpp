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
        printModuleName(); Serial.println("LTE Shield not present!");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName(); Serial.println("Modem Information: " + modemInfo);
    }

    // Connect to the LTE network
    moduleInitialized = connect();

    // If we successfully connected to the LTE network print out some information
    if(moduleInitialized){
        printModuleName(); Serial.println("Connected!");
        printModuleName(); Serial.println("APN: " + APN);
        printModuleName(); Serial.print("Signal State: ");
        Serial.println(modem.getSignalQuality());
        printModuleName(); Serial.println("IP Address: " + Loom_LTE::IPtoString(modem.localIP()));

        verifyConnection();

        printModuleName(); Serial.println("Module successfully initialized!");
    }
    else{
        printModuleName(); Serial.println("Module failed to initialize");
    }

    firstInit = false;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_up(){
    // If not connected to a network we want to connect
    if(moduleInitialized){
        printModuleName(); Serial.println("Powering up GPRS Modem. This should take about 10 seconds...");
        digitalWrite(powerPin, LOW);
        delay(10000);
        SerialAT.begin(9600);
        delay(6000);
        modem.restart();
        printModuleName(); Serial.println("Powering up complete!");

        if(!firstInit && !isConnected())
            connect();
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_down(){
    if(moduleInitialized){
        printModuleName(); Serial.println("Powering down GPRS Modem. This should take about 5 seconds...");
        modem.poweroff();
        digitalWrite(powerPin, HIGH);
        delay(5000);
        printModuleName(); Serial.println("Powering down complete!");
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
    uint8_t attemptCount = 1; // Tracks number of attempts, 10 is a fail

    do{
        printModuleName(); Serial.println("Waiting for network...");
        if(!modem.waitForNetwork()){
            printModuleName(); Serial.println("No Response from network!");
            return false;
        }

        if(!modem.isNetworkConnected()){
            printModuleName(); Serial.println("No connection to network!");
            return false;
        }

        printModuleName(); Serial.println("Connected to network!");

        // Connect to lte network
        printModuleName(); Serial.println("Attempting to connect to LTE Network: " + APN);
        if(modem.gprsConnect(APN.c_str(), gprsUser.c_str(), gprsPass.c_str())){
            printModuleName(); Serial.println("Successfully Connected!");
            return true;
        }
        else{
            printModuleName(); Serial.println("Connection failed " + String(attemptCount) + "/ 10. Retrying...");
            delay(10000);
            attemptCount++;
        }

        // If the last attempt was the 10th attempt then stop
        if(attemptCount > 10){
            printModuleName(); Serial.println("Connection reattempts exceeded 10 tries. Connection Failed");
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
    printModuleName(); Serial.println("Attempting to verify internet connection...");
    
    // Connect to TinyGSM's creator's website
    if(!client.connect("vsh.pp.ua", 80)){
        printModuleName(); Serial.println("Failed to contact TinyGSM example your internet connection may not be completely established!");
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
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::loadConfigFromJSON(String json){
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        printModuleName(); Serial.println("There was an error reading the sleep interval from SD: " + String(deserialError.c_str()));
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

