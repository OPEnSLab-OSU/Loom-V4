
#include "Loom_Ethernet.h"
#include "Logger.h"
#include <OPEnS_RTC.h>

#if !defined(LOOM_OPENS_RTC_PATCH_LEVEL) || LOOM_OPENS_RTC_PATCH_LEVEL < 1
#error "Loom_Ethernet requires the hardened OPEnS_RTC dependency from Loom/dependencies."
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Ethernet::Loom_Ethernet(Manager &man, uint8_t mac[6], IPAddress ip)
    : NetworkComponent("Ethernet"), manInst(&man) {
    this->ip = ip;
    if (mac != nullptr) {
        for (int i = 0; i < 6; i++)
            this->mac[i] = mac[i];
    } else {
        moduleInitialized = false;
        ERROR(F("Ethernet constructor received a null MAC address."));
    }
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
    char ip[16];
    LOG(F("Initializing Ethernet module..."));

    if (!connect()) {
        ERROR(F("Failed to initialize Ethernet."));
        return;
    }

    // Give a bit more time to initialize the module
    delay(1000);

    LOG(F("Successfully initialized Ethernet!"));
    // Print the device IP
    ipToString(getIPAddress(), ip);
    LOGF("Device IP Address: %s", ip);

    ipToString(getSubnetMask(), ip);
    LOGF("Device Subnet Address: %s", ip);
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
    return moduleInitialized;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Ethernet::loadConfigFromJSON(char *json) {
    if (json == nullptr) {
        ERROR(F("Cannot load Ethernet configuration from a null buffer."));
        moduleInitialized = false;
        return;
    }

    // Mutable input enables zero-copy parsing; capacity covers the two arrays and outer object.
    StaticJsonDocument<JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(6) + JSON_ARRAY_SIZE(4)> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if (deserialError != DeserializationError::Ok) {
        ERRORF("There was an error reading the Ethernet credentials from SD: %s",
               deserialError.c_str());
        free(json);
        moduleInitialized = false;
        return;
    }

    JsonArray macJson = doc["mac"].as<JsonArray>();
    JsonArray ipJson = doc["ip"].as<JsonArray>();
    if (macJson.size() != 6 || ipJson.size() != 4) {
        ERROR(F("Ethernet configuration requires six MAC bytes and four IP bytes."));
        free(json);
        moduleInitialized = false;
        return;
    }

    // Loop over the loaded mac address
    for (int i = 0; i < 6; i++) {
        mac[i] = macJson[i].as<uint8_t>();
    }

    ip = IPAddress(ipJson[0], ipJson[1], ipJson[2], ipJson[3]);

    moduleInitialized = true;
    free(json);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Ethernet::getNetworkTime(int *year, int *month, int *day, int *hour, int *minute,
                                   int *second, float *tz) {
    if (year == nullptr || month == nullptr || day == nullptr || hour == nullptr ||
        minute == nullptr || second == nullptr)
        return false;
    if (tz != nullptr)
        *tz = 0.0f; // NTP is UTC.

    byte packetBuffer[NTP_PACKET_SIZE];              // Buffer to read in packet
    const unsigned long seventyYears = 2208988800UL; // Unix time start

    // Start UDP listener
    udp.begin(localPort);

    // Send off the NTP request to the time server
    sendNTPpacket();

    // Wait 1 seconds for data to come back
    delay(1000);

    /* Receive the packet from the timeserver*/
    if (udp.parsePacket() >= static_cast<int>(NTP_PACKET_SIZE) &&
        udp.read(packetBuffer, NTP_PACKET_SIZE) == static_cast<int>(NTP_PACKET_SIZE)) {

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        // combine the four bytes (two words) into a long integer
        // this is NTP time (seconds since Jan 1 1900):
        const unsigned long secsSince1900 = highWord << 16 | lowWord;
        if (secsSince1900 < seventyYears)
            return false;

        // Convert seconds since 1900 into unixtime
        unsigned long unixtime = secsSince1900 - seventyYears;

        // Set the integer pointers to the corresponding time
        DateTime currentTime = DateTime(unixtime);
        *year = currentTime.year();
        *month = currentTime.month();
        *day = currentTime.day();
        *hour = currentTime.hour();
        *minute = currentTime.minute();
        *second = currentTime.second();

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
