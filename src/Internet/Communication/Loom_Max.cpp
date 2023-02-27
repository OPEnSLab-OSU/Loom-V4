#include "Loom_Max.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Max::Loom_Max(Manager& man, Loom_WIFI& wifi) : Module("Max Pub/Sub"), manInst(&man), wifiInst(&wifi) {
    wifi.useMax();
    manInst->registerModule(this);
};
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Max::~Loom_Max() {
    if (udpSend) udpSend->stop(); 
    if (udpRecv) udpRecv->stop(); 

    // Clean up the actuator instances
    for(int i = 0; i < actuators.size(); i++){
        delete actuators[i];
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

void Loom_Max::package(){
    JsonArray tmp = manInst->getDocument()["id"].createNestedArray("ip");
    IPAddress ip = wifiInst->getIPAddress();
    tmp.add(ip[0]);
    tmp.add(ip[1]);
    tmp.add(ip[2]);
    tmp.add(ip[3]);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::initialize(){
    LOG("Initializing Max Communication....");

    udpSend = UDPPtr(wifiInst->getUDP());
    udpRecv = UDPPtr(wifiInst->getUDP());

    // Set the IP and port to communicate over
    setIP();
    setUDPPort();

    LOG("Connections Opened!");

    /**
     * Initialize each actuator
     */ 
    if(actuators.size() > 0){
        LOG("Initializing desired actuators...");
        for(int i = 0; i < actuators.size(); i++){
            actuators[i]->initialize();
        }
        LOG("Successfully initialized actuators!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Max::publish(){
    LOG("Sending packet to " + Loom_WIFI::IPtoString(remoteIP) + ":" + String(sendPort));

    if(!udpSend){
        ERROR("Sender UDP instance not set!");
        return false;
    }

    // Attempt to start a new packet
    if(udpSend->beginPacket(remoteIP, sendPort) != 1){
        ERROR("The IP address or port were invalid!");
        return false;
    }

    // Package all actuators
    if(actuators.size() > 0){
        for(int i = 0; i < actuators.size(); i++){
            actuators[i]->package(manInst->get_data_object(actuators[i]->getModuleName()));
        }
    }

    size_t size = serializeJson(manInst->getDocument(), (*udpSend));

    if(size <= 0){
        ERROR("An error occurred when attempting to write the JSON packet to the UDP stream");
        return false;
    }

    if(udpSend->endPacket() != 1){
        ERROR("An error occurred when attempting to close the current packet!");
        return false;
    }

    LOG("Packet successfully sent!");

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Max::subscribe(){
    // If there is a packet available
    if(udpRecv->parsePacket()){

        // Clear the JSON document
        messageJson.clear();

        DeserializationError error = deserializeJson(messageJson, (*udpRecv) );
		if (error != DeserializationError::Ok) {
			ERROR("Failed to parse JSON data from UDP stream, Error: " + String(error.c_str()));
			return false;
		}


        // If there are actuators supplied control those if not just print the packet
        if(actuators.size() > 0){

            // Is the packet actually a command
            if(messageJson["type"].as<String>() == "command"){

                // Loop over each command being sent to the device
                for(int j = 0; j < messageJson["commands"].as<JsonArray>().size(); j++){

                    // Loop over each actuator to find the right one
                    String type = messageJson["commands"][j]["module"].as<String>();
                    int instanceNum = messageJson["commands"][j]["params"][0].as<int>();
                    
                    // Loop over each actuator
                    for(int i = 0; i < actuators.size(); i++){
                        // If the current actuator is the one we want to control
                        if(actuators[i]->typeToString() == type){

                            // If the type we are trying to control doesn't have an instance number
                            if(type.startsWith("Relay") || type.startsWith("Neopixel")){
                                actuators[i]->control(messageJson["commands"][j]["params"].as<JsonArray>());
                            }

                            // Stepper and Servo have an instance number as the 0th index
                            else if(actuators[i]->get_instance_num() == instanceNum){
                                // Pass the parameters field to the actuator
                                actuators[i]->control(messageJson["commands"][j]["params"].as<JsonArray>());
                            }
                        }
                    }
                }
            }
        }
        else{            
            LOG("Packet received from: " + Loom_WIFI::IPtoString(udpRecv->remoteIP()));
            LOG("Message Json: ");
            String jsonStr = "";
            serializeJsonPretty(messageJson, jsonStr);
            LOG(jsonStr + "\n");

            // If we are receiving a command for the MaxSub module
            if(messageJson["commands"][0]["module"].as<String>().startsWith("MaxSub")){

                // Then we are trying to set the WiFi credentials
                if(messageJson["commands"][0]["func"].as<int>() == 99){

                    // Get the name and password out of the parameters and then power cycle the board
                    String name = messageJson["commands"][0]["params"][0].as<String>();
                    String password = messageJson["commands"][0]["params"][1].as<String>();
                    wifiInst->storeNewWiFiCreds(name, password);
                    return true;
                }
            }
        }

       

        return true;
    }

    WARNING("No message received!");

    return false;
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::setUDPPort(){ 
    sendPort = SEND_BASE_UDP_PORT + manInst->get_instance_num();
    recvPort = RECV_BASE_UDP_PORT + manInst->get_instance_num();

    // Open a listen server on the specified port
    udpSend->begin(sendPort);
    udpRecv->begin(recvPort);

    LOG("Listening for UDP Packets on " + String(recvPort));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::setIP(){ remoteIP = wifiInst->getBroadcast(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////