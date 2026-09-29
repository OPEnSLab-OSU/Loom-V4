#include "Loom_WarningGuards.h"

#include "Loom_Wifi.h"
#include "../../../Hardware/Loom_BatchSD/Loom_BatchSD.h"
#include "Loom_WifiFlashStorage.h"
#include "Logger.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <OPEnS_RTC.h>
LOOM_EXTERNAL_INCLUDE_END

#if !defined(LOOM_OPENS_RTC_PATCH_LEVEL) || LOOM_OPENS_RTC_PATCH_LEVEL < 1
#error "Loom_WIFI requires the hardened OPEnS_RTC dependency from Loom/dependencies."
#endif

namespace {
constexpr uint32_t AP_CLIENT_WAIT_MS = 30000;
constexpr size_t FLASH_ROW_BYTES = 256;
constexpr size_t WIFI_CONFIG_BYTES =
    (sizeof(WifiInfo) + FLASH_ROW_BYTES - 1) / FLASH_ROW_BYTES * FLASH_ROW_BYTES;
alignas(FLASH_ROW_BYTES) const uint8_t WIFI_CONFIG_FLASH[WIFI_CONFIG_BYTES] = {};

// Function-local construction keeps this object and its guard out of non-WiFi binaries when the
// linker can discard Loom_WIFI. More importantly, the focused wrapper avoids importing the
// FlashStorage library's unrelated 1 KB EEPROM emulation object.
Loom_WifiFlashStorage<WifiInfo> &wifiCredentialStorage() {
    static Loom_WifiFlashStorage<WifiInfo> storage(WIFI_CONFIG_FLASH);
    return storage;
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
WiFiUDP *Loom_WIFI::getUDP() {
    // No Loom call site currently needs this object. Construct one shared instance only when an
    // external caller requests it, avoiding both the old per-call leak and ~1.4 KB in every WiFi
    // object's static footprint.
    static WiFiUDP sharedUdp;
    return &sharedUdp;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager &man, CommunicationMode mode, const char *name, const char *password,
                     int connectionRetries)
    : NetworkComponent("WiFi"), manager(&man), connectionRetries(max(1, connectionRetries)),
      mode(mode) {
    const char *safeName = name ? name : "";
    const char *safePassword = password ? password : "";
    if (mode == CommunicationMode::AP && safeName[0] == '\0') {
        setDefaultAccessPointName();
    } else {
        strncpy(networkName, safeName, sizeof(networkName) - 1);
        networkName[sizeof(networkName) - 1] = '\0';
    }

    strncpy(networkPassword, safePassword, sizeof(networkPassword) - 1);
    networkPassword[sizeof(networkPassword) - 1] = '\0';
    manager->registerModule(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager &man)
    : NetworkComponent("WiFi"), manager(&man), connectionRetries(5),
      mode(CommunicationMode::CLIENT) {
    manager->registerModule(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::initialize() {
    FUNCTION_START;
    // The Feather M0 WiFi uses different pins from the other supported boards.
    WiFi.setPins(8, 7, 4, 2);
    LOG(F("Initializing WiFi module..."));

    if (WiFi.status() == WL_NO_SHIELD) {
        ERROR(F("WINC1500 not present, WiFi functionality will be disabled"));
        moduleInitialized = false;
        firstInitialization = false;
        return;
    }

    WiFi.maxLowPowerMode();
    shouldPowerUp = true;
    power_up();
    delay(1000);

    if (mode != CommunicationMode::AP && !usedByMax) {
        LOG(F("Verifying Connection to the Internet..."));
        verifyConnection();
    }

    const int wifiStatus = WiFi.status();
    const bool interfaceReady = mode == CommunicationMode::AP ? (wifiStatus == WL_AP_LISTENING ||
                                                                 wifiStatus == WL_AP_CONNECTED)
                                                              : wifiStatus == WL_CONNECTED;
    if (moduleInitialized && interfaceReady) {
        LOG(F("Successfully initialized WiFi!"));
        char ip[16] = {};
        ipToString(getIPAddress(), ip);
        LOGF("Device IP Address: %s", ip);
        ipToString(getSubnetMask(), ip);
        LOGF("Device Subnet Address: %s", ip);
    } else {
        ERROR(F("Failed to initialize WiFi!"));
    }
    firstInitialization = false;
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::package() {
    FUNCTION_START;
    if (!moduleInitialized) {
        return;
    }

    JsonObject json = manager->get_data_object(getModuleName());
    if (shouldPowerUp) {
        json[F("SSID")] = WiFi.SSID();
        json[F("RSSI")] = WiFi.RSSI();
    } else {
        json[F("SSID")] = networkName;
        json[F("RSSI")] = 0;
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_up() {
    // Wake for the sample that fills the batch and while an unsent batch needs retry.
    if (batchSD != nullptr && !firstInitialization) {
        if (batchSD->getBatchSize() <= 0) {
            ERROR(F("Invalid BatchSD configuration; WIFI will remain off."));
            shouldPowerUp = false;
            return;
        }
        if (!batchSD->shouldPowerModem()) {
            WARNING(F("Not ready to publish, WIFI will not be powered up"));
            shouldPowerUp = false;
            return;
        }
        shouldPowerUp = true;
    }
    if (!moduleInitialized || !shouldPowerUp) {
        return;
    }

    if (usedByMax) {
        const WifiInfo info = wifiCredentialStorage().read();
        if (info.is_valid) {
            strncpy(networkName, info.name, sizeof(networkName) - 1);
            networkName[sizeof(networkName) - 1] = '\0';
            strncpy(networkPassword, info.password, sizeof(networkPassword) - 1);
            networkPassword[sizeof(networkPassword) - 1] = '\0';
        }
    }

    if (mode == CommunicationMode::CLIENT) {
        connect_to_network();
    } else {
        start_ap();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::setDefaultAccessPointName() {
    // Reserve room for the instance number and terminator in the 100-byte SSID buffer.
    snprintf(networkName, sizeof(networkName), "%.*s%lu", 88, manager->get_device_name(),
             static_cast<unsigned long>(manager->get_instance_num()));
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::connect_to_network() {
    FUNCTION_START;
    const bool hasPassword = networkPassword[0] != '\0';
    LOGF("Attempting to connect to SSID: %s", networkName);
    if (hasPassword) {
        LOG(F("We are authenticating with a password..."));
    }

    // Constructors and setMaxRetries() enforce at least one attempt.
    for (int attempt = 0; attempt < connectionRetries; ++attempt) {
        const int status =
            hasPassword ? WiFi.begin(networkName, networkPassword) : WiFi.begin(networkName);
        if (status == WL_CONNECTED) {
            LOG(F("Connected to network!"));
            return;
        }

        if (hasPassword) {
            LOG(F("Attempting to connect to AP..."));
        } else {
            LOGF("Attempting to connect to AP (Attempt %i)...", attempt + 1);
        }
        delay(5000);
    }

    ERROR(F("Failed to connect to the access point after allotted tries! Is the "
            "network in range and are your credentials correct?"));
    if (usedByMax) {
        LOG(F("Starting access point as backup!"));
        mode = CommunicationMode::AP;
        setDefaultAccessPointName();
        start_ap();
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::start_ap() {
    FUNCTION_START;

    LOGF("Starting access point on: %s", networkName);

    auto status = WiFi.beginAP(networkName);

    // If the AP is not listening print an error
    if (status != WL_AP_LISTENING) {
        ERROR(F("Access point creation failed!"));
        return;
    }

    // Wait for a client, but do not hang setup indefinitely when the controller is offline.
    LOG(F("Waiting for a device to connect to the access point..."));
    const uint32_t started = millis();
    while (WiFi.status() != WL_AP_CONNECTED &&
           static_cast<uint32_t>(millis() - started) < AP_CLIENT_WAIT_MS) {
        WD_TIMER_RESET;
        delay(10);
    }
    if (WiFi.status() == WL_AP_CONNECTED) {
        LOG(F("Device connected to AP!"));
    } else {
        WARNING(F("No AP client connected within 30 seconds; continuing without blocking."));
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_down() {
    if (shouldPowerUp) {
        // Disconnect and end the Wifi when we power down the device
        WiFi.disconnect();
        WiFi.end();
        // WIFI Pins: 8, 7 (Interrupt pin), 4, 2
        // Configure as OUTPUT so they can't possibly trigger an interrupt
        pinMode(7, OUTPUT);
        delay(1000);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::isConnected() {
    if (mode == CommunicationMode::CLIENT) {
        return WiFi.status() == WL_CONNECTED;
    } else {
        return WiFi.status() == WL_AP_CONNECTED;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::verifyConnection() {
    FUNCTION_START;
    if (!moduleInitialized || !shouldPowerUp) {
        return false;
    }

    const int pingLatency = WiFi.ping("www.google.com");
    if (pingLatency >= 0) {
        LOGF("Successfully Pinged Google! Response Time: %ims", pingLatency);
        return true;
    }

    LOG(F("Ping Failed! Error Code: "));
    switch (pingLatency) {
    case -1:
        LOG(F("Ping Failed! Error Code: Destination_Unreachable"));
        break;
    case -2:
        LOG(F("Ping Failed! Error Code: Ping_TimeOut"));
        break;
    case -3:
        LOG(F("Ping Failed! Error Code: Unknown_Host"));
        break;
    default:
        LOG(F("Ping Failed! Error Code: General_Error"));
        break;
    }
    FUNCTION_END;
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::loadConfigFromJSON(char *json) {
    FUNCTION_START;

    if (json == nullptr) {
        ERROR(F("Cannot load WiFi credentials from a null buffer."));
        moduleInitialized = false;
        return;
    }

    // Mutable input enables zero-copy strings; only the two object slots occupy document RAM.
    StaticJsonDocument<JSON_OBJECT_SIZE(2)> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if (deserialError != DeserializationError::Ok) {
        ERRORF("There was an error reading the WIFI credentials from SD: %s",
               deserialError.c_str());
        free(json);
        moduleInitialized = false;
        return;
    }

    // Only update the wifi creds if the data was not NULL
    if (!doc["SSID"].isNull()) {
        const char *ssid = doc["SSID"].as<const char *>();
        const char *password = doc["password"] | "";
        strncpy(networkName, ssid ? ssid : "", sizeof(networkName) - 1);
        networkName[sizeof(networkName) - 1] = '\0';
        strncpy(networkPassword, password, sizeof(networkPassword) - 1);
        networkPassword[sizeof(networkPassword) - 1] = '\0';
    }

    moduleInitialized = networkName[0] != '\0';
    if (!moduleInitialized) {
        ERROR(F("WiFi configuration requires a non-empty SSID."));
    }

    free(json);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::storeNewWiFiCreds(const char *name, const char *password) {
    FUNCTION_START;
    // Write the new info to the flash memory
    LOG(F("Writing new WiFi credentials to flash..."));
    WifiInfo info = {};
    info.is_valid = true;
    strncpy(info.name, name ? name : "", sizeof(info.name) - 1);
    info.name[sizeof(info.name) - 1] = '\0';
    strncpy(info.password, password ? password : "", sizeof(info.password) - 1);
    info.password[sizeof(info.password) - 1] = '\0';
    if (!wifiCredentialStorage().write(info)) {
        ERROR(F("Failed to write WiFi credentials to flash"));
        return;
    }
    LOG(F("Information written to flash!"));

    // Power cycle the board
    LOG(F("Power cycling the WiFi chip..."));
    mode = CLIENT;
    power_down();
    delay(1000);
    power_up();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getIPAddress() { return WiFi.localIP(); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getSubnetMask() { return WiFi.subnetMask(); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getGateway() { return WiFi.gatewayIP(); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getBroadcast() {
    IPAddress broadcast = WiFi.gatewayIP();

    // Set the last one to 255 for the netmask
    broadcast[3] = 255;
    return broadcast;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::getNetworkTime(int *year, int *month, int *day, int *hour, int *minute, int *second,
                               float *tz) {
    if (year == nullptr || month == nullptr || day == nullptr || hour == nullptr ||
        minute == nullptr || second == nullptr) {
        return false;
    }
    if (tz != nullptr) {
        *tz = 0.0f; // WiFi.getTime() returns UTC.
    }

    const unsigned long unixtime = WiFi.getTime();
    if (unixtime == 0) {
        return false;
    }

    const DateTime time(unixtime);
    *year = time.year();
    *month = time.month();
    *day = time.day();
    *hour = time.hour();
    *minute = time.minute();
    *second = time.second();
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
