#include "../../Loom_Max.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Max::Loom_Max(Manager& man, Loom_WIFI& wifi) : Module("Max Pub/Sub"), manInst(&man), wifiInst(&wifi) {
    udpSend = UDPPtr(wifiInst->getUDP());
    udpRecv = UDPPtr(wifiInst->getUDP());
    manInst->registerModule(this);
};
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Max::~Loom_Max() {
    if (udpSend) udpSend->stop(); 
    if (udpRecv) udpRecv->stop(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::initialize(){

    // Set the IP and port to communicate over
    setIP();
    setUDPPort();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Max::publish(){
    printModuleName(); Serial.println("Sending packet to " + Loom_WIFI::IPtoString(remoteIP) + ":" + String(sendPort));

    if(!udpSend){
        printModuleName(); Serial.println("Sender UDP instance not set!");
        return false;
    }

    // Attempt to start a new packet
    if(udpSend->beginPacket(remoteIP, sendPort) != 1){
        printModuleName(); Serial.println("The IP address or port were invalid!");
        return false;
    }

    size_t size = serializeJson(manInst->getDocument(), (*udpSend));
    Serial.println(size);

    if(size <= 0){
        printModuleName(); Serial.println("An error occurred when attempting to write the JSON packet to the UDP stream");
        return false;
    }

    if(udpSend->endPacket() != 1){
        printModuleName(); Serial.println("An error occurred when attempting to close the current packet!");
        return false;
    }

    printModuleName(); Serial.println("Packet successfully sent!");

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
			printModuleName(); Serial.println("Failed to parse JSON data from UDP stream, Error: " + String(error.c_str()));
			return false;
		}

        printModuleName(); Serial.println("Packet received from: " + Loom_WIFI::IPtoString(udpRecv->remoteIP()));
        printModuleName(); Serial.println("Message Json: ");
        serializeJsonPretty(messageJson, Serial);
        Serial.println("\n");
        
        // DISPATCH HERE

        return true;
    }

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

    printModuleName(); Serial.println("Listening for UDP Packets on " + String(recvPort));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::setIP(){ remoteIP = wifiInst->getBroadcast(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////