#include "Loom_Ethernet.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Ethernet::Loom_Ethernet(Manager &man, uint8_t mac[6], IPAddress ip)
    : NetworkComponent("Ethernet"), manInst(&man) {
    this->ip = ip;
    for (int i = 0; i < 6; i++)
        this->mac[i] = mac[i];
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Ethernet::Loom_Ethernet(Manager &man) : NetworkComponent("Ethernet"), manInst(&man) {
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
bool Loom_Ethernet::connect() {
    if (moduleInitialized) {
        pinMode(8, OUTPUT);
        digitalWrite(8, HIGH);

        // Initialize the module
        Ethernet.init(10);

        // No DHCP server
        if (Ethernet.begin(mac) == 0) {
            ERROR(F("Failed to configure using DHCP"));

            // Module didn't get an IP address attempting to figure out why...
            moduleInitialized = false;
            if (Ethernet.hardwareStatus() == EthernetNoHardware) {
                ERROR("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
            } else if (Ethernet.linkStatus() == LinkOFF) {
                ERROR("Ethernet cable is not connected.");
            }

        } else {
            ip = Ethernet.localIP();
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Ethernet::loadConfigFromJSON(char *json) {
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    char output[OUTPUT_SIZE];
    DeserializationError deserialError = deserializeJson(doc, (const char *)json);

    // Check if an error occurred and if so print it
    if (deserialError != DeserializationError::Ok) {
        snprintf(output, OUTPUT_SIZE,
                 "There was an error reading the Ethernet credentials from SD: %s",
                 deserialError.c_str());
        ERROR(output);
    }

    JsonArray macJson = doc["mac"].as<JsonArray>();
    JsonArray ipJson = doc["ip"].as<JsonArray>();

    // Loop over the loaded mac address
    for (int i = 0; i < 6; i++) {
        mac[i] = macJson[i].as<uint8_t>();
    }

    ip = IPAddress(ipJson[0], ipJson[1], ipJson[2], ipJson[3]);

    free(json);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Ethernet::getNetworkTime(int *year, int *month, int *day, int *hour, int *minute,
                                   int *second, float *tz) {
    byte packetBuffer[NTP_PACKET_SIZE];              // Buffer to read in packet
    const unsigned long seventyYears = 2208988800UL; // Unix time start

    // Start UDP listener
    udp.begin(localPort);

    // Send off the NTP request to the time server
    sendNTPpacket();

    // Wait 1 seconds for data to come back
    delay(1000);

    /* Receive the packet from the timeserver*/
    if (udp.parsePacket()) {
        udp.read(packetBuffer, NTP_PACKET_SIZE);

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        // combine the four bytes (two words) into a long integer
        // this is NTP time (seconds since Jan 1 1900):
        unsigned long secsSince1900 = highWord << 16 | lowWord;

        // Convert seconds since 1900 into unixtime
        unsigned long unixtime = secsSince1900 - seventyYears;

        // Set the integer pointers to the corresponding time
        DateTime currentTime = DateTime(unixtime);
        *year = currentTime.year();
        *month = currentTime.month();
        *day = currentTime.day();
        *hour = currentTime.hour();
        *minute = currentTime.minute();
        *second = currentTime.minute();

        return true;
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Ethernet::sendNTPpacket() {
    byte packetBuffer[NTP_PACKET_SIZE]; // Buffer to read in packet
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);

    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011; // LI, Version, Mode
    packetBuffer[1] = 0;          // Stratum, or type of clock
    packetBuffer[2] = 6;          // Polling Interval
    packetBuffer[3] = 0xEC;       // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(timeServer, 123); // NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_Ethernet::getIPAddress() {
    IPAddress ip = Ethernet.localIP();
    return ip;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_Ethernet::getSubnetMask() {
    IPAddress subnet = Ethernet.subnetMask();
    return subnet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_Ethernet::getGateway() {
    IPAddress gateway = Ethernet.gatewayIP();
    return gateway;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_Ethernet::getBroadcast() {
    IPAddress broadcast = Ethernet.gatewayIP();

    // Set the last one to 255 for the netmask
    broadcast[3] = 255;

    return broadcast;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
