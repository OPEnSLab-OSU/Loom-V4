#pragma once

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
};