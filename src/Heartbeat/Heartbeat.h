#pragma once

#include "../Loom_Manager.h"
#include "../Module.h"
#include "../Hardware/Loom_Hypnos/Loom_Hypnos.h"
#include "Adapter.h"

class Loom_Heartbeat : public Module {
    public:
        /**
         * @brief Construct a new Loom_Heartbeat object
         * 
         *         // PERSONAL_NOTE: Might not need manager reference, but haven't fully commited to taking it out yet.
         * 
         * @param newAddress Destination address for heartbeats
         * @param heartbeatInterval Interval between heartbeats (scalar)
         * @param normalWorkInterval Interval between normal work cycles (scalar)
         * @param managerInstance Manager instance
         * @param adapterInstance Pointer to the communication adapter being used.
         * @param hypnosInstance Pointer to the Hypnos instance being used.
         * 
         * @note heartbeatInterval and normalWorkInterval must be seconds.
         *         // PERSONAL_NOTE: Might not need manager reference, but haven't fully commited to taking it out yet.
         */
        Loom_Heartbeat(const uint8_t newAddress, 
                        const uint32_t pHeartbeatInterval, 
                        const uint32_t pNormalWorkInterval, 
                        Manager* managerInstance, 
                        Adapter* adapterInstance,
                        Loom_Hypnos* hypnosInstance = nullptr);

        /**
         * Convert seconds to a TimeSpan object
         * 
         * @param secondsToConvert The number of seconds to convert
         */ 
        TimeSpan secondsToTimeSpan(const uint32_t secondsToConvert);

        /**
         * when heartbeat has been initialized, find the time until the next wake up / upnause event 
         * to either do heartbeat or normal logic.
         * 
         * @return time until next event in the same unit as heartbeatInit parameters. 
         * 
         * @note enforces a minimum wait time of 5 seconds to ensure stability/safety. This is because
         *      very short sleep times can cause undefined behavior with firmware/hardware systems like 
         *      sleep/interrupt overhead, Serial/LoRa Cleanup, RTC rounding, Power Rails and Peripheral
         *      power down/up times. 
         *      Intervals will still be followed, after the minimum 5 second wait.
         */
        TimeSpan calculateNextEvent();

        /**
         * Set the heartbeat flag bool
         * 
         * @param flag The value to set the heartbeat flag to
         */
        void setHeartbeatFlag(const bool flag) { heartbeatFlag = flag; };

        /**
         * Get the status of the heartbeat flag bool
         */
        bool getHeartbeatFlag() const { return heartbeatFlag; };

        /**
         * Flash the onboard LED at PIN 13 to indicate a heartbeat has been sent
         */
        void flashLight();

        /**
         * Send a heartbeat packet with basic device info
         */
        bool adapterSend();

        /**
         * Ensure that the normal work alarm (1) and heartbeat alarm (2) are both set
         */
        void ensureHeartbeatHypnosAlarmsActive();

        /**
         * Adjust the heartbeat flag based on which alarm triggered
         */
        void adjustHbFlagFromAlarms();
    
    private:
    
        uint32_t heartbeatTimer_s = 0;
        uint32_t heartbeatInterval_s = 0;
        uint32_t normWorkTimer_s = 0;
        uint32_t normWorkInterval_s = 0;

        uint8_t heartbeatDestAddress = 0;

        bool heartbeatFlag = false;

        Loom_Hypnos* hypnosPtr = nullptr;
        Manager* managerPtr = nullptr;
        Adapter* adapPtr = nullptr;
};
