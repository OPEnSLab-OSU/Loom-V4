#pragma once

/*
 * Loom LTE modem selection and board configuration
 *
 * Loom_LTE_Config.h is the project-level configuration file for R4/R5 selection,
 * R5 startup timing, UART baud, optional reset recovery, optional rail control,
 * and optional carrier locking.
 *
 * Arduino compiles library .cpp files separately from the .ino, so sketch-local
 * defines do not reliably reach Loom_LTE.cpp. Including Loom_LTE_Config.h here
 * keeps the library translation unit on the same modem and board configuration
 * as the sketch.
 */

#if __has_include("Loom_LTE_Config.h")
    #include "Loom_LTE_Config.h"
#endif

#if __has_include("arduino_secrets.h")
    #include "arduino_secrets.h"
#endif

/*
 * TinyGSM modem profile
 *
 * TinyGSM needs exactly one modem profile selected before TinyGSM.h is included.
 * The Loom-facing defines are translated here so examples can use:
 *
 *     #define LOOM_LTE_USE_SARA_R5
 *
 * instead of editing TinyGSM internals or this library header per project.
 */
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

/*
 * R5 control polarity
 *
 * OPEnS/Jolteon control pins drive MOSFET gates. HIGH at the Feather pin turns
 * the control MOSFET on, which pulls the SARA input LOW. For SARA PWR_ON and
 * RESET_N, that LOW state is the asserted state.
 */
#ifndef LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH
    #define LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH 1
#endif

/*
 * R5 power timing
 *
 * The OPEnS/Jolteon R5 startup sequence drives A5 HIGH, waits for the configured pulse
 * width, releases A5 LOW, and then waits for the modem OS before sending AT.
 */
#ifndef LOOM_LTE_R5_PWR_PULSE_MS
    #define LOOM_LTE_R5_PWR_PULSE_MS 1200UL
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

/*
 * R5 UART timing
 *
 * The host UART must open at one baud before it can send the first AT command.
 * Keep the primary baud configurable so board startup does not require editing
 * Loom_LTE.cpp. Normal SARA-R5 bring-up uses 115200.
 */
#ifndef LOOM_LTE_R5_UART_BAUD
    #define LOOM_LTE_R5_UART_BAUD 115200UL
#endif

/*
 * R5 boot mode switches
 *
 * The R5 startup sequence sends the A5 power pulse before probing AT.
 * Reset recovery and fallback baud probing are opt-in diagnostics.
 */
#ifndef LOOM_LTE_R5_COMPAT_POWER_FIRST
    #define LOOM_LTE_R5_COMPAT_POWER_FIRST 1
#endif

#ifndef LOOM_LTE_R5_EXACT_OPENS_POWER_PATH
    #define LOOM_LTE_R5_EXACT_OPENS_POWER_PATH 1
#endif

#ifndef LOOM_LTE_R5_ENABLE_RESET_RECOVERY
    #define LOOM_LTE_R5_ENABLE_RESET_RECOVERY 0
#endif

#ifndef LOOM_LTE_R5_SCAN_BAUDS_ON_FAILURE
    #define LOOM_LTE_R5_SCAN_BAUDS_ON_FAILURE 0
#endif

/*
 * Optional carrier lock
 *
 * Empty string keeps automatic registration behavior. A numeric operator such
 * as "310410" can be configured for controlled AT&T tests.
 */
#ifndef LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC
    #define LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC ""
#endif

#ifndef LOOM_LTE_R5_FORCE_OPERATOR_ACT
    #define LOOM_LTE_R5_FORCE_OPERATOR_ACT 7
#endif

/*
 * Optional Hypnos rail control
 *
 * Normal Loom deployments usually let the manager/Hypnos own peripheral rails.
 * LTE-only bench sketches can enable these pins when they need the LTE library
 * to power the 3.3 V and 5 V rails directly.
 */
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

// The LTE modem is connected to the board's hardware Serial1 port.
#define SerialAT Serial1

enum LTE_VERSION{
    SPARKFUN,
    OPENS
};

/**
 * Loomified control for a u-blox SARA LTE board.
 *
 * The class owns power sequencing, low-level AT readiness checks, cellular
 * registration, APN/PDP activation, socket verification, network diagnostics,
 * and direct UART passthrough for field debugging.
 */
class Loom_LTE : public NetworkComponent{
    protected:
        void measure() override {};

        bool isConnected() override { return modem.isGprsConnected(); };

    public:
        /**
         * Construct a configured LTE instance from sketch-supplied credentials.
         *
         * @param man Reference to the Loom manager.
         * @param apn Cellular APN. For Hologram this is usually "hologram".
         * @param user APN username, usually empty for Hologram.
         * @param pass APN password, usually empty for Hologram.
         * @param powerPin Board-level pin that controls LTE PWR_ON.
         * @param version Board power-control style.
         * @param resetPin Optional board-level pin that controls LTE RESET_N. Use -1 to leave reset unused.
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
         * Construct an LTE instance whose credentials will be loaded later from
         * SD-card JSON.
         */
        Loom_LTE(Manager& man);

        /**
         * Boot the modem, verify AT/TinyGSM readiness, read modem identity, and
         * open the cellular data session.
         */
        void initialize() override;

        /**
         * Apply board power sequencing and bring the modem to an AT-ready state.
         * Carrier registration and APN/PDP activation are handled by connect().
         */
        void power_up() override;

        /**
         * Request modem power-down when the module is initialized and active.
         */
        void power_down() override;

        /**
         * Add LTE signal quality to the Loom data package.
         */
        void package() override;

        /**
         * Read network time from the modem and convert the modem-reported local
         * time back to UTC using the supplied timezone offset.
         */
        bool getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) override;

        /**
         * Load APN credentials and optional pin configuration from JSON.
         *
         * Expected credential keys:
         * - apn
         * - user
         * - pass
         *
         * Optional pin keys:
         * - pin
         * - reset_pin
         */
        void loadConfigFromJSON(char* json);

        /**
         * Attach a BatchSD module so LTE can remain off until the configured
         * batch window requires upload.
         */
        void setBatchSD(Loom_BatchSD& batch) { batch_sd = &batch; };

        /**
         * Register on the cellular network and activate the APN/PDP data session.
         */
        bool connect();

        /**
         * Disconnect the APN/PDP data session.
         */
        void disconnect();

        /**
         * Open a real TCP socket to verify that the data session can route
         * internet traffic.
         */
        bool verifyConnection();

        /**
         * Bridge USB serial to the LTE UART for manual AT-command debugging.
         * Call this from loop when the Serial Monitor should talk directly to the modem.
         */
        void debugPassthrough();

        /**
         * Return the TinyGSM client used by MQTT and other internet modules.
         */
        Client* getClient() override;

        /**
         * Request a modem poweroff, then run the normal power-up sequence again.
         */
        void restartModem() {
            TIMER_RESET;
            modem.poweroff();
            delay(5000);
            powered = false;
            power_up();
            TIMER_RESET;
        };

        /**
         * Convert an IPAddress into a dotted IPv4 string.
         */
        void ipToString(IPAddress ip, char array[16]) {
            snprintf(array, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        };

    private:
        // Copy sketch or SD-card credentials into owned, null-terminated buffers.
        void copyCredential(char* dst, const char* src, size_t dstSize);

        // Board-level control pin helpers.
        void driveControlPinIdle(int pin);
        void driveControlPinActive(int pin);
        void pulseControlPin(int pin, uint32_t pulseMs, const __FlashStringHelper* label);
        void idlePowerPin();
        void idleResetPin();

        // Board and modem bring-up.
        void prepareOptionalPowerRails();
        void powerBoardOn();
        void powerBoardOff();
        bool waitForModemAT(uint32_t timeoutMs);
        bool selectWorkingBaud(uint32_t timeoutMs);
        bool initializeModemFromAT();
        bool bootModemWithRetries();

        // Raw AT helpers and R5 setup hints.
        bool sendATExpectOK(const char* command, uint32_t timeoutMs = 5000L);
        void applyR5NetworkHints();

        // Human-readable diagnostics for field logs.
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
        uint32_t selectedBaud = LOOM_LTE_R5_UART_BAUD;

        TinyGsm modem;
        TinyGsmClient client;

        bool powerUp = true;
        bool firstInit = true;
        Loom_BatchSD* batch_sd = nullptr;

        bool powered = false;
};
