#pragma once

#include "Module.h"
#include "Loom_Wifi.h"
#include <Udp.h>
#include <memory>

// Base ports to send and receive on 
#define SEND_BASE_UDP_PORT 8000
#define RECV_BASE_UDP_PORT 9000

/**
 * Class used to handle communication with Max MSP to control devices remotely
 */ 
class Loom_Max : public Module{
    protected:
         /* These aren't used with the Max modules */
        void measure() override {};                               
        void print_measurements() override {};  
        void power_up() override {};
        void power_down() override {}; 
        void package() override {};

    public:
        /// Close the socket and delete the UDP object when the unique ptr dissapears
        struct UDPDeletor {
            void operator() (UDP* p) {
                if (p != nullptr) {
                    p->stop();
                    delete p;
                }
            }
        };

        using UDPPtr = std::unique_ptr<UDP, UDPDeletor>;

        /* Initialize from the manager */
        void initialize() override;

        /**
         * Send the UDP packet from the device to the server
         */ 
        bool publish();

        /**
         * Retrieve the response on the UDP stream if one is available
         */ 
        bool subscribe();

        /**
         * Construct a new instance of the the Max MSP Pub/Sub protocol
         * @param man Reference to the manager
         * @param wifi Reference to the Wifi manager for getting UDP communication streams
         */ 
        Loom_Max(Manager& man, Loom_WIFI& wifi);

        ~Loom_Max();

    private:
        Manager* manInst;       // Instance of the manager
        Loom_WIFI* wifiInst;    // Instance of the WiFi Manager

        UDPPtr udpSend;         // Instance of the UDP controller for sending
        UDPPtr udpRecv;         // Instance of the UDP controller for recieving

        uint16_t sendPort;       // Port to send the UDP packets to
        uint16_t recvPort;       // Port to receive the packets on

        IPAddress remoteIP;     // IP Address to send the packets to

        void setUDPPort();      // Set the UDP port to the correct port number
        void setIP();           // Set the remote IP to send the packets too

        StaticJsonDocument<1000> messageJson; // Response packet
        

};