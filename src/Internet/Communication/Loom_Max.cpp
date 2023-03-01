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
    LOG(F("Initializing Max Communication...."));

    udpSend = UDPPtr(wifiInst->getUDP());
    udpRecv = UDPPtr(wifiInst->getUDP());

    // Set the IP and port to communicate over
    setIP();
    setUDPPort();

    LOG(F("Connections Opened!"));

    /**
     * Initialize each actuator
     */ 
    if(actuators.size() > 0){
        LOG(F("Initializing desired actuators..."));
        for(int i = 0; i < actuators.size(); i++){
            actuators[i]->initialize();
        }
        LOG(F("Successfully initialized actuators!"));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Max::publish(){
    char output[100];

    // Print the device IP
    char* ip = wifiInst->ipToString(remoteIP);
    snprintf(output, 100, "Sending packet to %s:%u", ip, sendPort);
    free(ip);
    LOG(output);

    if(!udpSend){
        ERROR(F("Sender UDP instance not set!"));
        return false;
    }

    // Attempt to start a new packet
    if(udpSend->beginPacket(remoteIP, sendPort) != 1){
        ERROR(F("The IP address or port were invalid!"));
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
        ERROR(F("An error occurred when attempting to write the JSON packet to the UDP stream"));
        return false;
    }

    if(udpSend->endPacket() != 1){
        ERROR(F("An error occurred when attempting to close the current packet!"));
        return false;
    }

    LOG(F("Packet successfully sent!"));

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Max::subscribe(){
    char output[100];
    // If there is a packet available
    if(udpRecv->parsePacket()){

        // Clear the JSON document
        messageJson.clear();

        DeserializationError error = deserializeJson(messageJson, (*udpRecv) );
		if (error != DeserializationError::Ok) {
            snprintf(output, 100, "Failed to parse JSON data from UDP stream, Error: %s", error.c_str());
			ERROR(output);
			return false;
		}


        // If there are actuators supplied control those if not just print the packet
        if(actuators.size() > 0){

            // Is the packet actually a command
            if(strcmp(messageJson["type"].as<const char*>(), "command") == 0){

                // Loop over each command being sent to the device
                for(int j = 0; j < messageJson["commands"].as<JsonArray>().size(); j++){

                    // Loop over each actuator to find the right one
                    const char* type = messageJson["commands"][j]["module"].as<const char*>();
                    int instanceNum = messageJson["commands"][j]["params"][0].as<int>();
                    
                    // Loop over each actuator
                    for(int i = 0; i < actuators.size(); i++){

                        // If the current actuator is the one we want to control
                        if(strcmp(actuators[i]->typeToString(), type) == 0){

                            // If the type we are trying to control doesn't have an instance number
                            if(strstr(type, "Relay") != NULL || strstr(type, "Neopixel") != NULL){
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

            // Print out where the packet came from
            char* ip = wifiInst->ipToString(udpRecv->remoteIP());
            snprintf(output, 100, "Packet received from: %s", ip) ;
            free(ip);
            LOG(output);

            LOG(F("Message Json: "));
            size_t jsonSize = measureJsonPretty(messageJson)+1;
            char* jsonStr = (char *) malloc(jsonSize);
            serializeJsonPretty(messageJson, jsonStr, jsonSize);
            LOG(jsonStr);
            free(jsonStr);

            // If we are receiving a command for the MaxSub module
            if(strstr(messageJson["commands"][0]["module"].as<const char*>(),"MaxSub") != NULL){

                // Then we are trying to set the WiFi credentials
                if(messageJson["commands"][0]["func"].as<int>() == 99){

                    // Get the name and password out of the parameters and then power cycle the board
                    const char* name = messageJson["commands"][0]["params"][0].as<const char*>();
                    const char* password = messageJson["commands"][0]["params"][1].as<const char*>();
                    wifiInst->storeNewWiFiCreds(name, password);
                    return true;
                }
            }
        }

       

        return true;
    }

    WARNING(F("No message received!"));

    return false;
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::setUDPPort(){ 
    char output[100];
    sendPort = SEND_BASE_UDP_PORT + manInst->get_instance_num();
    recvPort = RECV_BASE_UDP_PORT + manInst->get_instance_num();

    // Open a listen server on the specified port
    udpSend->begin(sendPort);
    udpRecv->begin(recvPort);

    snprintf(output, 100, "Listening for UDP Packets on %u", recvPort);
    LOG(output);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::setIP(){ remoteIP = wifiInst->getBroadcast(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////