#pragma once

#include "Module.h"
#include "../Connectivity/Loom_Wifi/Loom_Wifi.h"
#include "Actuators.h"

#include <Udp.h>
#include <vector>
#include <memory>

// Base ports to send and receive on 
#define SEND_BASE_UDP_PORT 8000
#define RECV_BASE_UDP_PORT 9000

/**
 * Class used to handle communication with Max MSP to control devices remotely
 * 
 * @author Will Richards
 */ 
class Loom_Max : public Module{
    protected:
         /* These aren't used with the Max modules */
        void measure() override {};                               
        void print_measurements() override {};  
        void power_up() override {};
        void power_down() override {}; 
        void package() override;

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
         * @param mode How traffic is handled between the feather and the max client (CLIENT = Remote Router, AP = Access point on the feather)
         */ 
        Loom_Max(Manager& man, Loom_WIFI& wifi, CommunicationMode mode = CommunicationMode::CLIENT);

        /**
         * Construct a new instance of the the Max MSP Pub/Sub protocol
         * @param man Reference to the manager
         * @param wifi Reference to the Wifi manager for getting UDP communication streams
         * @param mode How traffic is handled between the feather and the max client (CLIENT = Remote Router, AP = Access point on the feather)
         * @param firstAct The first actuator to add to the list
         */ 
        template<typename T>
        Loom_Max(Manager& man, Loom_WIFI& wifi, CommunicationMode mode, T* firstAct) : Module("Max Pub/Sub"), manInst(&man), wifiInst(&wifi), mode(mode){

            actuators.push_back(firstAct);

            udpSend = UDPPtr(wifiInst->getUDP());
            udpRecv = UDPPtr(wifiInst->getUDP());
            manInst->registerModule(this);
        };

        /**
         * Construct a new instance of the the Max MSP Pub/Sub protocol
         * @param man Reference to the manager
         * @param wifi Reference to the Wifi manager for getting UDP communication streams
         * @param mode How traffic is handled between the feather and the max client (CLIENT = Remote Router, AP = Access point on the feather)
         * @param firstAct The first actuator to add to the list
         * @param additionalActuators This takes any number of actuators
         */ 
         template<typename T, typename... Args>
        Loom_Max(Manager& man, Loom_WIFI& wifi, CommunicationMode mode, T* firstAct, Args*... additionalActuators) : Module("Max Pub/Sub"), manInst(&man), wifiInst(&wifi), mode(mode){
            get_variadic_parameters((Actuator*)firstAct, (Actuator*)additionalActuators...);

            udpSend = UDPPtr(wifiInst->getUDP());
            udpRecv = UDPPtr(wifiInst->getUDP());
            manInst->registerModule(this);
        };

        ~Loom_Max();

    private:
        Manager* manInst;                       // Instance of the manager
        Loom_WIFI* wifiInst;                    // Instance of the WiFi Manager

        CommunicationMode mode;                 // How we want to communicate with the Max client

        UDPPtr udpSend;                         // Instance of the UDP controller for sending
        UDPPtr udpRecv;                         // Instance of the UDP controller for recieving

        uint16_t sendPort;                      // Port to send the UDP packets to
        uint16_t recvPort;                      // Port to receive the packets on

        IPAddress remoteIP;                     // IP Address to send the packets to

        void setUDPPort();                      // Set the UDP port to the correct port number
        void setIP();                           // Set the remote IP to send the packets too

        StaticJsonDocument<1000> messageJson;   // Response packet

        std::vector<Actuator*> actuators;       // List of actuators we want to control with max
        

        /* 
         *   The following two functions are some sorcery to get the variadic parameters without the need for passing in a size variable
         *   I don't fully understand it so don't touch it just works
         *   Based off: https://eli.thegreenplace.net/2014/variadic-templates-in-c/
         */
        template<typename T>
        T* get_variadic_parameters(T* v) {
            actuators.push_back(v);
            return v;
        };

        template<typename T, typename... Args>
        T* get_variadic_parameters(T* first, Args*... args) {
           actuators.push_back(first);
            return get_variadic_parameters(args...);
        };
        

};