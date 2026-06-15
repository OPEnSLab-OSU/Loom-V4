#pragma once

/*
 * Loom LTE modem selection
 *
 * A sketch can request the R5 path with:
 *
 *     #define LOOM_LTE_USE_SARA_R5
 *
 * Arduino compiles library .cpp files separately from the .ino, so the package
 * also includes Loom_LTE_Config.h. Keep that file beside this header when the
 * library itself needs to see the same modem selection.
 */

#if __has_include("Loom_LTE_Config.h")
    #include "Loom_LTE_Config.h"
#endif

#if __has_include("arduino_secrets.h")
    #include "arduino_secrets.h"
#endif

#if defined(LOOM_LTE_USE_SARA_R5)
    #undef TINY_GSM_MODEM_UBLOX
    #undef TINY_GSM_MODEM_SARAR4
    #define TINY_GSM_MODEM_SARAR5
#elif defined(LOOM_LTE_USE_SARA_R4)
    #undef TINY_GSM_MODEM_UBLOX
    #undef TINY_GSM_MODEM_SARAR5
    #define TINY_GSM_MODEM_SARAR4
#elif !defined(TINY_GSM_MODEM_UBLOX) && !defined(TINY_GSM_MODEM_SARAR4) && !defined(TINY_GSM_MODEM_SARAR5)
    #define TINY_GSM_MODEM_SARAR4
#endif

// OPEnS/Jolteon control pins drive MOSFET gates. HIGH at the Feather pin
// pulls the SARA input LOW, which is the active state for PWR_ON/RESET_N.
#ifndef LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH
    #define LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH 1
#endif

// R5 power pulses need to be long enough to trigger switch-on but short
// enough that repeated pulses do not wander into switch-off behavior.
#ifndef LOOM_LTE_R5_PWR_PULSE_MS
    #define LOOM_LTE_R5_PWR_PULSE_MS 1100UL
#endif

#ifndef LOOM_LTE_R5_RESET_PULSE_MS
    #define LOOM_LTE_R5_RESET_PULSE_MS 250UL
#endif

#ifndef LOOM_LTE_R5_BOOT_AT_TIMEOUT_MS
    #define LOOM_LTE_R5_BOOT_AT_TIMEOUT_MS 45000UL
#endif

#ifndef LOOM_LTE_R5_POST_PWR_SETTLE_MS
    #define LOOM_LTE_R5_POST_PWR_SETTLE_MS 10000UL
#endif

#ifndef LOOM_LTE_R5_COMPAT_POWER_FIRST
    #define LOOM_LTE_R5_COMPAT_POWER_FIRST 1
#endif

#ifndef LOOM_LTE_R5_ENABLE_RESET_RECOVERY
    #define LOOM_LTE_R5_ENABLE_RESET_RECOVERY 0
#endif

#ifndef LOOM_LTE_R5_SCAN_BAUDS_ON_FAILURE
    #define LOOM_LTE_R5_SCAN_BAUDS_ON_FAILURE 0
#endif

// Optional carrier lock. For AT&T, this can be set to "310410" to avoid
// long operator scans during controlled tests. Empty string keeps automatic
// registration behavior.
#ifndef LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC
    #define LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC ""
#endif

#ifndef LOOM_LTE_R5_FORCE_OPERATOR_ACT
    #define LOOM_LTE_R5_FORCE_OPERATOR_ACT 7
#endif

// Keep rail control off by default because most Loom deployments let the
// manager or Hypnos board own peripheral power. Enable this only for LTE-only
// bench sketches.
#ifndef LOOM_LTE_R5_ENABLE_POWER_RAIL_PINS
    #define LOOM_LTE_R5_ENABLE_POWER_RAIL_PINS 0
#endif

#ifndef LOOM_LTE_R5_3V3_RAIL_PIN
    #define LOOM_LTE_R5_3V3_RAIL_PIN 5
#endif

#ifndef LOOM_LTE_R5_5V_RAIL_PIN
    #define LOOM_LTE_R5_5V_RAIL_PIN 6
#endif

#ifndef LOOM_LTE_R5_3V3_RAIL_ON_LEVEL
    #define LOOM_LTE_R5_3V3_RAIL_ON_LEVEL LOW
#endif

#ifndef LOOM_LTE_R5_5V_RAIL_ON_LEVEL
    #define LOOM_LTE_R5_5V_RAIL_ON_LEVEL HIGH
#endif

#include "Loom_Manager.h"
#include "../NetworkComponent.h"
#include <TinyGSM.h>
#include <functional>

#include "../../../Hardware/Loom_BatchSD/Loom_BatchSD.h"

#define SerialAT Serial1

enum LTE_VERSION{
    SPARKFUN,
    OPENS
};

/**
 * Loomified Control for a 4G LTE Board
 *
 * @author Will Richards
 */
class Loom_LTE : public NetworkComponent{
    protected:
        void measure() override {};

        bool isConnected() override { return modem.isGprsConnected(); };

    public:
        /**
         * Construct a new LTE instance
         * @param man Reference to the manager
         * @param apn Name of the LTE network
         * @param user Username to use
         * @param pass Password to use
         * @param powerPin Pin used to power the device
         * @param version Board power-control style
         * @param resetPin Pin used to reset the modem on OPEnS/Jolteon-style boards
         */
        Loom_LTE(
            Manager& man,
            const char* apn,
            const char* user,
            const char* pass,
            const int powerPin = A5,
#if defined(TINY_GSM_MODEM_SARAR5)
            LTE_VERSION version = OPENS,
#else
            LTE_VERSION version = SPARKFUN,
#endif
            const int resetPin = -1
        );

        /**
         * Construct a new LTE instance assuming credentials will be pulled from an SD card
         * @param man Reference to the manager
         */
        Loom_LTE(Manager& man);

        void initialize() override;
        void power_up() override;
        void power_down() override;
        void package() override;
        bool getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) override;

        /**
         * Load the config to connect to the LTE network from a JSON string
         * @param json Json file read, this is freed before returning
         */
        void loadConfigFromJSON(char* json);

        /**
         * Turn on batch upload for the lte which means it will only initialize the module when we need to upload
         * @param batch BatchSD module
         */
        void setBatchSD(Loom_BatchSD& batch) { batch_sd = &batch; };

        /**
         * Connect to the cellular network
         */
        bool connect();

        /**
         * Disconnect from the cellular network
         */
        void disconnect();

        /**
         * Attempt to connect to something remote to see if we actually have an internet connection
         */
        bool verifyConnection();

        /**
         * Bridge USB serial to the LTE UART for manual AT-command debugging.
         * Call this from loop when you want the Serial Monitor to talk directly to the modem.
         */
        void debugPassthrough();

        /**
         * Get the client to supply to publish platforms that need to communicate using this internet framework
         */
        Client* getClient() override;

        void restartModem() {
            TIMER_RESET;
            modem.poweroff();
            delay(5000);
            powered = false;
            power_up();
            TIMER_RESET;
        };

        /**
         * Convert an IP address to a string
         */
        void ipToString(IPAddress ip, char array[16]) {
            snprintf(array, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        };

    private:
        // Copy sketch or SD-card credentials into owned buffers.
        void copyCredential(char* dst, const char* src, size_t dstSize);
        void driveControlPinIdle(int pin);
        void driveControlPinActive(int pin);
        void pulseControlPin(int pin, uint32_t pulseMs, const __FlashStringHelper* label);
        void idlePowerPin();
        void idleResetPin();
        void prepareOptionalPowerRails();
        void powerBoardOn();
        void powerBoardOff();
        bool waitForModemAT(uint32_t timeoutMs);
        bool selectWorkingBaud(uint32_t timeoutMs);
        bool initializeModemFromAT();
        bool bootModemWithRetries();
        bool sendATExpectOK(const char* command, uint32_t timeoutMs = 5000L);
        void applyR5NetworkHints();
        void logBootChecklist();
        void logPlainFailure(const __FlashStringHelper* message);
        void logNetworkDiagnostics();
        void logSignalDiagnostic();
        void logSimDiagnostic();
        void logRegistrationDiagnostic();
        void logRawAT(const char* command, uint32_t timeoutMs = 3000L);

        LTE_VERSION lteBoardVersion = SPARKFUN;

        Manager* manInst;

        char APN[100];
        char gprsUser[100];
        char gprsPass[100];

        int powerPin = A5;
        int resetPin = -1;
        uint32_t selectedBaud = 115200;

        TinyGsm modem;
        TinyGsmClient client;

        bool powerUp = true;
        bool firstInit = true;
        Loom_BatchSD* batch_sd = nullptr;

        bool powered = false;
};
