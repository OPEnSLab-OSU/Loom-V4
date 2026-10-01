/**
 * Optional SARA-R510M8S bench example. Leave USE_GNSS false on ordinary R4/R5 boards.
 * A receiver needs an appropriate antenna and a view of the sky. Starting it is not a fix.
 * This prints a separate metadata document. Optional SD logging adds stable location columns.
 * The LTE constructor below targets R5. Select SARA_R4 there when using an R4 board.
 */
#include <Loom_Manager.h> // Include first so Arduino discovers Loom.
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Utilities/Loom_GnssUtils.h>
#include <Utilities/Loom_GnssMetadata.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#ifndef LOOM_GNSS_EXAMPLE_ENABLED
#define LOOM_GNSS_EXAMPLE_ENABLED 0 // Change to 1 only on an assembled SARA-R510M8S GNSS stack.
#endif
#ifndef LOOM_GNSS_EXAMPLE_SD
#define LOOM_GNSS_EXAMPLE_SD 0 // Optional CSV: location columns begin with the FIRST sample.
#endif
Manager manager("GNSSDemo", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, LOOM_GNSS_EXAMPLE_SD != 0);
Loom_LTE lte(manager, "hologram", "", "", A5, OPENS, -1, LTE_MODEM::SARA_R5);
const bool USE_GNSS = LOOM_GNSS_EXAMPLE_ENABLED != 0;
bool receiverStarted = false;
bool haveFix = false;
loomGnss::Fix latestFix;

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
    manager.beginSerial(false);
    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.enable();
    manager.initialize();
    if (USE_GNSS) {
        hypnos.setNetworkInterface(&lte);
        if (!hypnos.networkTimeUpdate()) {
            Serial.println(F("Network UTC sync failed; verify RTC UTC before accepting metadata."));
        }
        receiverStarted = lte.startGNSS();
        if (!receiverStarted) {
            Serial.println(F("GNSS start failed; ordinary LTE remains available."));
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
    if (receiverStarted) {
        loomGnss::Fix fix;
        if (lte.readGNSS(fix)) {
            latestFix = fix;
            haveFix = true; // Timestamp freshness is checked again at every output boundary.
            Serial.print(F("Latitude: "));
            Serial.println(fix.latitude, 7);
            Serial.print(F("Longitude: "));
            Serial.println(fix.longitude, 7);
            DateTime now;
            StaticJsonDocument<512> metadata;
            metadata["type"] = "metadata";
            metadata["id"]["name"] = manager.get_device_name();
            metadata["id"]["instance"] = manager.get_instance_num();
            if (hypnos.tryGetCurrentTime(now) &&
                loomGnss::addMetadata(metadata.as<JsonObject>(), fix, now.unixtime()) &&
                loomJsonIsComplete(metadata)) {
                // RTC UTC must already be synchronized. Stale/future fixes are rejected.
                // To upload explicitly, pass this document to mqtt.publishMetadata(metadata).
                serializeJsonPretty(metadata, Serial);
                Serial.println();
            } else {
                Serial.println(
                    F("Metadata skipped: no checked RTC time, fresh fix, or JSON space."));
            }
        } else {
            Serial.println(F("No valid GNSS snapshot yet. LTE/RTC time stays unchanged."));
        }
    }
    if (LOOM_GNSS_EXAMPLE_SD) {
        manager.measure();
        manager.package();
        DateTime now;
        const bool haveUtc = hypnos.tryGetCurrentTime(now);
        // Keep the same columns from the first row onwards. No fix/expired fix becomes blank,
        // including when a plain R4/R5 is used with GNSS disabled. No header rotation per fix.
        loomGnss::addCsvLocation(manager.get_data_object("Location"),
                                 haveUtc && haveFix ? &latestFix : nullptr,
                                 haveUtc ? now.unixtime() : 0);
        if (!hypnos.logToSD()) {
            Serial.println(F("CSV save failed; inspect RTC, SD and packet-space diagnostics."));
        }
    }
    manager.pause(5000); // Try another cached snapshot later; never wait here for a satellite fix.
    // Call lte.stopGNSS() when no more fixes are needed. LTE power_down() also stops owned GNSS.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
