#include "Loom_Ethernet.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Ethernet::Loom_Ethernet(Manager& man, uint8_t mac[6], IPAddress ip) : NetworkComponent("Ethernet"), manInst(&man){
    this->ip = ip;
    for(int i = 0; i < 6; i++)
        this->mac[i] = mac[i];
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Ethernet::Loom_Ethernet(Manager& man) : NetworkComponent("Ethernet"), manInst(&man) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Ethernet::initialize() {
    char output[OUTPUT_SIZE];
    char ip[16];
    LOG(F("Initializing Ethernet module..."));

    // Call the connect class to initiate the connection
    connect();

    // Give a bit more time to initialize the module
    delay(1000);

    moduleInitialized = true;
    
    LOG(F("Successfully Initalized Ethernet!"));
    // Print the device IP
    ipToString(getIPAddress(), ip);
    snprintf(output, OUTPUT_SIZE, "Device IP Address: %s", ip);
    LOG(output);

    ipToString(getSubnetMask(), ip);
    snprintf(output, OUTPUT_SIZE, "Device Subnet Address: %s", ip);
    LOG(output);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Ethernet::connect(){
    if(moduleInitialized){
       pinMode(8, OUTPUT);
       digitalWrite(8, HIGH);

       // Initialize the module
       Ethernet.init(10);

       // No DHCP server
       if(Ethernet.begin(mac) == 0){
        WARNING(F("Failed to configure using DHCP"));

        // Attempt to assign a static IP
        Ethernet.begin(mac, ip);
       }
       else{
        ip = Ethernet.localIP();
       }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Ethernet::loadConfigFromJSON(char* json){
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    char output[OUTPUT_SIZE];
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        snprintf(output, OUTPUT_SIZE, "There was an error reading the Ethernet credentials from SD: %s", deserialError.c_str());
        ERROR(output);
    }

    JsonArray macJson = doc["mac"].as<JsonArray>();
    JsonArray ipJson = doc["ip"].as<JsonArray>();

    // Loop over the loaded mac address
    for(int i = 0; i < 6; i++){
        mac[i] = macJson[i].as<uint8_t>();
    }

    ip = IPAddress(ipJson[0], ipJson[1], ipJson[2], ipJson[3]);

    free(json);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_Ethernet::getIPAddress(){
    IPAddress ip = Ethernet.localIP();
    return ip;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_Ethernet::getSubnetMask(){
    IPAddress subnet = Ethernet.subnetMask();
    return subnet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_Ethernet::getGateway(){
    IPAddress gateway = Ethernet.gatewayIP();
    return gateway;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_Ethernet::getBroadcast(){
    IPAddress broadcast = Ethernet.gatewayIP();

    // Set the last one to 255 for the netmask
    broadcast[3] = 255;

    return broadcast;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////