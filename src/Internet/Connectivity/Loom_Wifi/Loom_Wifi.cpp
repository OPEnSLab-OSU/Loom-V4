
#include "Loom_Wifi.h"
#include "Loom_WifiFlashStorage.h"
#include "Logger.h"
#include <OPEnS_RTC.h>

#if !defined(LOOM_OPENS_RTC_PATCH_LEVEL) || LOOM_OPENS_RTC_PATCH_LEVEL < 1
#error "Loom_WIFI requires the hardened OPEnS_RTC dependency from Loom/dependencies."
#endif

namespace {
constexpr uint32_t AP_CLIENT_WAIT_MS = 30000;

__attribute__((aligned(256)))
static const uint8_t WIFI_CONFIG_FLASH[(sizeof(WifiInfo) + 255) / 256 * 256] = {};

// Function-local construction keeps this object and its guard out of non-WiFi binaries when the
// linker can discard Loom_WIFI. More importantly, the focused wrapper avoids importing the
// FlashStorage library's unrelated 1 KB EEPROM emulation object.
Loom_WifiFlashStorage<WifiInfo> &wifiCredentialStorage() {
    static Loom_WifiFlashStorage<WifiInfo> storage(WIFI_CONFIG_FLASH);
    return storage;
}
} // namespace

WiFiUDP *Loom_WIFI::getUDP() {
    // No Loom call site currently needs this object. Construct one shared instance only when an
    // external caller requests it, avoiding both the old per-call leak and ~1.4 KB in every WiFi
    // object's static footprint.
    static WiFiUDP sharedUdp;
    return &sharedUdp;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager &man, CommunicationMode mode, const char *name, const char *password,
                     int connectionRetries)
    : NetworkComponent("WiFi"), manInst(&man), connectionRetries(max(1, connectionRetries)),
      mode(mode) {
    const char *safeName = name ? name : "";
    const char *safePassword = password ? password : "";
    if (mode == CommunicationMode::AP && safeName[0] == '\0') {
        snprintf(wifi_name, sizeof(wifi_name), "%.*s%lu", 88, manInst->get_device_name(),
                 static_cast<unsigned long>(manInst->get_instance_num()));
    } else {
        strncpy(wifi_name, safeName, sizeof(wifi_name) - 1);
        wifi_name[sizeof(wifi_name) - 1] = '\0';
    }

    strncpy(wifi_password, safePassword, sizeof(wifi_password) - 1);
    wifi_password[sizeof(wifi_password) - 1] = '\0';
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_WIFI::Loom_WIFI(Manager &man)
    : NetworkComponent("WiFi"), manInst(&man), connectionRetries(5),
      mode(CommunicationMode::CLIENT) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::initialize() {
    FUNCTION_START;
    // The pins on the feather M0 WiFi are different than most boards
    WiFi.setPins(8, 7, 4, 2);
    char ip[16];

    LOG(F("Initializing WiFi module..."));

    if (WiFi.status() == WL_NO_SHIELD) {
        ERROR(F("WINC1500 not present, WiFi functionality will be disabled"));
        moduleInitialized = false;
    } else {

        // Enable low power mode to conserve power
        WiFi.maxLowPowerMode();

        powerUp = true;
        // Call the power up class to connect to the wifi network
        power_up();

        // Give a bit more time to initialize the module
        delay(1000);

        // Only try to verify if we have connected to a network
        if (mode != CommunicationMode::AP && !usingMax) {
            // Verify the wifi connection after we have connected
            LOG(F("Verifying Connection to the Internet..."));
            verifyConnection();
        }

        const int wifiStatus = WiFi.status();
        const bool interfaceReady =
            mode == CommunicationMode::AP
                ? (wifiStatus == WL_AP_LISTENING || wifiStatus == WL_AP_CONNECTED)
                : wifiStatus == WL_CONNECTED;
        if (moduleInitialized && interfaceReady) {
            LOG(F("Successfully initialized WiFi!"));

            // Print the device IP
            ipToString(getIPAddress(), ip);
            LOGF("Device IP Address: %s", ip);

            ipToString(getSubnetMask(), ip);
            LOGF("Device Subnet Address: %s", ip);
        } else {
            ERROR(F("Failed to initialize WiFi!"));
        }
    }
    firstInit = false;
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::package() {
    FUNCTION_START;
    if (moduleInitialized) {
        if (powerUp) {
            JsonObject json = manInst->get_data_object(getModuleName());
            json[F("SSID")] = WiFi.SSID();
            json[F("RSSI")] = WiFi.RSSI();
        } else {
            JsonObject json = manInst->get_data_object(getModuleName());
            json[F("SSID")] = wifi_name;
            json[F("RSSI")] = 0;
        }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_up() {
    // Wake for the sample that fills the batch and keep waking while an unsent batch needs retry.
    if (batchSD != nullptr && !firstInit) {
        if (batchSD->getBatchSize() <= 0) {
            ERROR(F("Invalid BatchSD configuration; WIFI will remain off."));
            powerUp = false;
            return;
        }
        if (batchSD->getCurrentBatch() < batchSD->getBatchSize() - 1) {
            WARNING(F("Not ready to publish, WIFI will not be powered up"));
            powerUp = false;
            return;
        } else {
            powerUp = true;
        }
    }

    if (moduleInitialized && powerUp) {

        // Check if we are going through our power up and are using max
        if (usingMax) {
            // If so we want to read the flash memory to see if we have set data yet
            WifiInfo info = wifiCredentialStorage().read();

            // Read the data in if we have set it before
            if (info.is_valid != false) {
                strncpy(wifi_name, info.name, sizeof(wifi_name) - 1);
                wifi_name[sizeof(wifi_name) - 1] = '\0';
                strncpy(wifi_password, info.password, sizeof(wifi_password) - 1);
                wifi_password[sizeof(wifi_password) - 1] = '\0';
            }
        }

        // Initialize the access point mode or connect to a router
        if (mode == CommunicationMode::CLIENT)
            connect_to_network();
        else
            start_ap();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::connect_to_network() {
    FUNCTION_START;
    int retry_count = 0;

    LOGF("Attempting to connect to SSID: %s", wifi_name);
    // TIMER_DISABLE;

    // If we are logging into a network with a password
    if (strlen(wifi_password) > 0) {

        LOG(F("We are authenticating with a password..."));
        // While we are trying to connect to the wifi network
        while (WiFi.begin(wifi_name, wifi_password) != WL_CONNECTED) {
            LOG(F("Attempting to connect to AP..."));
            delay(5000);
            retry_count++;

            // If after 10 attempts we still can't connect to the network we need to stop and break
            // so we don't hang the device
            if (retry_count >= connectionRetries) {
                ERROR(F("Failed to connect to the access point after allotted tries! Is the "
                        "network in range and are your credentials correct?"));

                // Switch over to AP mode if using max
                if (usingMax) {
                    LOG(F("Starting access point as backup!"));
                    mode = CommunicationMode::AP;
                    snprintf(wifi_name, sizeof(wifi_name), "%.*s%lu", 88,
                             manInst->get_device_name(),
                             static_cast<unsigned long>(manInst->get_instance_num()));
                    start_ap();
                }

                // TIMER_ENABLE;
                FUNCTION_END;
                return;
            }
        }
    } else {
        // While we are trying to connect to the wifi network
        while (WiFi.begin(wifi_name) != WL_CONNECTED) {
            LOGF("Attempting to connect to AP (Attempt %i)...", retry_count + 1);
            delay(5000);
            retry_count++;

            // If after 10 attempts we still can't connect to the network we need to stop and break
            // so we don't hang the device
            if (retry_count >= connectionRetries) {
                ERROR(F("Failed to connect to the access point after allotted tries! Is the "
                        "network in range and are your credentials correct?"));

                // Switch over to AP mode if using max
                if (usingMax) {
                    LOG(F("Starting access point as backup!"));
                    mode = CommunicationMode::AP;
                    snprintf(wifi_name, sizeof(wifi_name), "%.*s%lu", 88,
                             manInst->get_device_name(),
                             static_cast<unsigned long>(manInst->get_instance_num()));
                    start_ap();
                }
                // TIMER_ENABLE;
                FUNCTION_END;
                return;
            }
        }
    }

    LOG(F("Connected to network!"));
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::start_ap() {
    FUNCTION_START;
    // TIMER_DISABLE;
    LOGF("Starting access point on: %s", wifi_name);

    auto status = WiFi.beginAP(wifi_name);

    // If the AP is not listening print an error
    if (status != WL_AP_LISTENING) {
        ERROR(F("Access point creation failed!"));
        FUNCTION_END;
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
    if (WiFi.status() == WL_AP_CONNECTED)
        LOG(F("Device connected to AP!"));
    else
        WARNING(F("No AP client connected within 30 seconds; continuing without blocking."));
    // TIMER_ENABLE;
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::power_down() {
    if (powerUp) {
        // Disconnect and end the Wifi when we power down the device
        WiFi.disconnect();
        WiFi.end();
        // WIFI Pins: 8, 7 (Interrupt pin), 4, 2
        // Configure as OUTPUT so they can't possibly trigger an interrupt
        pinMode(7, OUTPUT);
        delay(1000);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::isConnected() {
    if (mode == CommunicationMode::CLIENT)
        return WiFi.status() == WL_CONNECTED;
    else
        return WiFi.status() == WL_AP_CONNECTED;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::verifyConnection() {
    FUNCTION_START;
    if (moduleInitialized && powerUp) {
        int pingLatency = WiFi.ping("www.google.com");
        if (pingLatency >= 0) {
            LOGF("Successfully Pinged Google! Response Time: %ims", pingLatency);
            FUNCTION_END;
            return true;
        } else {
            LOG(F("Ping Failed! Error Code: "));

            // Parse the error code into a human readable format
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
    }
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_WIFI::loadConfigFromJSON(char *json) {
    FUNCTION_START;

    if (json == nullptr) {
        ERROR(F("Cannot load WiFi credentials from a null buffer."));
        moduleInitialized = false;
        FUNCTION_END;
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
        FUNCTION_END;
        return;
    }

    // Only update the wifi creds if the data was not NULL
    if (!doc["SSID"].isNull()) {
        const char *ssid = doc["SSID"].as<const char *>();
        const char *password = doc["password"] | "";
        strncpy(wifi_name, ssid ? ssid : "", sizeof(wifi_name) - 1);
        wifi_name[sizeof(wifi_name) - 1] = '\0';
        strncpy(wifi_password, password, sizeof(wifi_password) - 1);
        wifi_password[sizeof(wifi_password) - 1] = '\0';
    }

    moduleInitialized = wifi_name[0] != '\0';
    if (!moduleInitialized)
        ERROR(F("WiFi configuration requires a non-empty SSID."));

    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
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
        FUNCTION_END;
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
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getIPAddress() {
    IPAddress ip = WiFi.localIP();
    return ip;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getSubnetMask() {
    IPAddress subnet = WiFi.subnetMask();
    return subnet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getGateway() {
    IPAddress gateway = WiFi.gatewayIP();
    return gateway;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
IPAddress Loom_WIFI::getBroadcast() {
    IPAddress broadcast = WiFi.gatewayIP();

    // Set the last one to 255 for the netmask
    broadcast[3] = 255;

    return broadcast;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_WIFI::getNetworkTime(int *year, int *month, int *day, int *hour, int *minute, int *second,
                               float *tz) {
    if (year == nullptr || month == nullptr || day == nullptr || hour == nullptr ||
        minute == nullptr || second == nullptr)
        return false;
    if (tz != nullptr)
        *tz = 0.0f; // WiFi.getTime() returns UTC.

    unsigned long unixtime = WiFi.getTime();
    if (unixtime != 0) {
        DateTime time = DateTime(unixtime);
        *year = time.year();
        *month = time.month();
        *day = time.day();
        *hour = time.hour();
        *minute = time.minute();
        *second = time.second();
        return true;
    } else {
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
