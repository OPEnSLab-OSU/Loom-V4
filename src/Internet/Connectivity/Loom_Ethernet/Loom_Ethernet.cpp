#include "Loom_Ethernet.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Ethernet::Loom_Ethernet(Manager& man, uint8_t mac[6], IPAddress ip) : Module("Ethernet"), manInst(&man){
    this->ip = ip;
    for(int i = 0; i < 6; i++)
        this->mac[i] = mac[i];
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Ethernet::Loom_Ethernet(Manager& man) : Module("Ethernet"), manInst(&man) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Ethernet::initialize() {

    printModuleName(); Serial.println("Initializing Ethernet module...");

    // Call the connect class to initiate the connection
    connect();

    // Give a bit more time to initialize the module
    delay(1000);

    hasInitialized = true;
    
    printModuleName(); Serial.println("Successfully Initalized Ethernet!");
    printModuleName(); Serial.println("Device IP Address: " + Loom_Ethernet::IPtoString(getIPAddress()));
    printModuleName(); Serial.println("Device Subnet Address: " + Loom_Ethernet::IPtoString(getSubnetMask()));
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
        printModuleName(); Serial.println("Failed to configure using DHCP");

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
void Loom_Ethernet::loadConfigFromJSON(JsonObject json){
    JsonArray macJson = json["mac"].as<JsonArray>();
    JsonArray ipJson = json["ip"].as<JsonArray>();

    // Loop over the loaded mac address
    for(int i = 0; i < 6; i++){
        mac[i] = macJson[i].as<uint8_t>();
    }

    ip = IPAddress(ipJson[0], ipJson[1], ipJson[2], ipJson[3]);
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