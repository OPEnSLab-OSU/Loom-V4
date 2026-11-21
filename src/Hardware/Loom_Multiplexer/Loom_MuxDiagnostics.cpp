#include "Loom_Multiplexer.h"
#include "Logger.h"
#include <cstdarg>

void Loom_Multiplexer::setDebug(bool enabled) {
    debugOutput = LOOM_COMPILE_MUX_DEBUG && enabled;

    if (debugOutput) {
        Serial.println(F("[MUX DEBUG] Serial debug enabled"));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setScanDebug(bool enabled) {
    scanDebugOutput = LOOM_COMPILE_MUX_DEBUG && enabled;

    if (debugOutput) {
        Serial.print(F("[MUX DEBUG] Scan miss debug "));
        Serial.println(scanDebugOutput ? F("enabled") : F("disabled"));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::debugScan() {
    FUNCTION_START(this);
    byte previousMuxAddr = activeMuxAddr;
    bool previousInitialized = moduleInitialized;

    debugLog("Starting mux debug scan");
    Wire.begin();

    const byte foundMuxAddr = findMultiplexer();

    if (foundMuxAddr == 0) {
        ERROR(F("Debug scan did not find a TCA9548 mux"));
        debugLog("Debug scan did not find a TCA9548 mux");
        return;
    }

    activeMuxAddr = foundMuxAddr;
    moduleInitialized = true;

    if (known_addresses.empty()) {
        assignKnownAddresses(nullptr, 0);
        debugLogFormatted("Known address list was empty, using default list of %u",
                          (unsigned int)known_addresses.size());
    }

    for (int port = 0; port < numPorts; port++) {
        if (!isPortEnabled(port)) {
            continue;
        }

        debugLogFormatted("Selecting mux port %i for debug scan", port);
        if (!selectPin(port)) {
            ERRORF("Failed to select mux port %i; skipping it.", port);
            continue;
        }
        delay(50);

        bool foundOnPort = false;

        for (byte addr : known_addresses) {
            if (!shouldScanAddress(addr)) {
                continue;
            }

            LOOM_FEED_WATCHDOG();
            debugLogFormatted("Debug scan probing mux port %i at address 0x%02X", port, addr);
            const uint8_t result = probeAddress(addr);

            if (result == 0) {
                foundOnPort = true;
                debugLogFormatted("ACK on mux port %i at I2C address 0x%02X", port, addr);
            } else if (scanDebugOutput) {
                debugLogFormatted("No ACK on mux port %i at I2C address 0x%02X, Wire error %u",
                                  port, addr, result);
            }
        }

        if (!foundOnPort) {
            debugLogFormatted("No known devices found on mux port %i", port);
        }
    }

    disableChannels();

    activeMuxAddr = previousMuxAddr;
    moduleInitialized = previousInitialized;

    debugLog("Finished mux debug scan");
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::debugLog(const char *message) {
#if LOOM_COMPILE_MUX_DEBUG
    if (debugOutput) {
        Serial.print(F("[MUX DEBUG] "));
        Serial.println(message);
    }
#else
    (void)message;
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::debugLogFormatted(const char *format, ...) {
#if LOOM_COMPILE_MUX_DEBUG
    if (!debugOutput) {
        return;
    }

    char output[96];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(output, sizeof(output), format ? format : "", arguments);
    va_end(arguments);
    debugLog(output);
#else
    (void)format;
#endif
}
