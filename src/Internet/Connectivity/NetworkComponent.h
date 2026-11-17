#pragma once
#include <Client.h>

#include "Module.h"


/**
 * Generic Network Abstraction Layer to provide additional generalized functionality between network components
 * 
 * @author Will Richards
*/
class NetworkComponent : public Module{
    public:
        NetworkComponent(const char* modName) : Module(modName) {}

        /* Request the current network time from the NetworkComponent */
        virtual bool getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) = 0;

        /* Is the current network interface connected */
        virtual bool isConnected() = 0;

        virtual Client* getClient() = 0;

        // structure that mqtt will use to determine if we had a failure due to internet issues
        typedef struct {

            // value is interpreted differently whether we are using LTE or not
            int8_t signalQuality = -1;

            // idnicates we are using LTE vs WiFi
            bool isLte = false;

            // for WiFi, if we are connected to AP
            // for LTE, if we have a data connection to 
            bool connected = false;
            
            // for LTE, if we are registered to our LTE network
            bool registered = false;

        } NetworkDiagnostics;

        // returns structure 
        virtual NetworkDiagnostics logDiagnostics() {}

        virtual bool reconnect() {}


};