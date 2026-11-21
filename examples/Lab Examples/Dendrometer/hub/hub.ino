#include <Loom_Manager.h> // Loom 4.9
#include <Logger.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

const unsigned long REPORT_INTERVAL = 1 * 60 * 60 * 1000;

Manager manager("HubName", 0);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
Loom_Analog batteryVoltage(manager);
// This awake hub needs one reusable fragment workspace. Reserve it explicitly instead of
// growing the heap when the first multi-part packet arrives. Transmit-only nodes need no pool.
loomMemory::FixedBufferPool<MAX_JSON_SIZE, 1> receivePool;
Loom_LoRa lora(manager); // The borrowed pool above outlives this radio.
Loom_LTE lte(manager, "hologram", "", "", A5);
// Every received node packet goes to SD before MQTT. A size of 1 preserves immediate uploads
// while keeping failed records across later receives and MCU restarts.
Loom_BatchSD batchSD(hypnos, 1);
Loom_MongoDB mqtt(manager, lte); // Load mqtt_creds.json from SD in setup().

const uint32_t UPLOAD_RETRY_INTERVAL_MS = 30000;
const uint32_t STORAGE_RETRY_INTERVAL_MS = 30000;
uint32_t lastUploadAttempt = 0;
uint32_t lastStorageAttempt = 0;
bool storageNeedsRetry = false;

void publishPending();
void saveCurrentPacketToSD();
void setup() {
    Logger::getInstance()->setDebugOutput(false);
    manager.beginSerial(false);

    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.enable();
    if (!lora.setReceivePool(receivePool)) {
        Serial.println(F("LoRa receive pool must be attached before receiving fragments."));
    }
    // Broker client identity is fixed to this Feather even when it forwards a node packet.
    mqtt.setClientID(manager.get_serial_num());
    mqtt.usePacketIdentityForBatch(); // Each queued node retains its own database/device topic.

    setRTC();

    // This hub stays awake to receive LoRa. Keep LTE available instead of cycling its power
    // for every node packet; only the upload queue waits when the broker is unavailable.

    // load MQTT credentials from the SD card, if they exist
    char *credentials = hypnos.readFile("mqtt_creds.json");
    if (credentials != nullptr) {
        mqtt.loadConfigFromJSON(credentials); // Takes ownership and frees the buffer.
    } else {
        Serial.println(F("Missing mqtt_creds.json; packets will stay queued on SD."));
    }

    // Initialize the modules
    manager.initialize();
    lastUploadAttempt = millis() - UPLOAD_RETRY_INTERVAL_MS; // Allow the first queued upload now.
    lastStorageAttempt = millis();
}

void loop() {
    if (millis() - lastStorageAttempt >= STORAGE_RETRY_INTERVAL_MS) {
        lastStorageAttempt = millis();
        DateTime utc;
        if (storageNeedsRetry || !hypnos.getSDManager()->hasSDInitialized() ||
            !hypnos.tryGetCurrentTime(utc)) {
            hypnos.enable();
            storageNeedsRetry = false;
        }
    }
    // Wait 5 seconds for a message
    if (lora.receive(5000, true)) {
        saveCurrentPacketToSD();
        // publishPending() runs below even if no new packet arrives, so an outage can recover.
        // A radio ACK still proves RF delivery only; check the SD log outcome for storage errors.
    }
    static unsigned long timer = millis();
    if (millis() - timer > REPORT_INTERVAL) {
        // receive(..., true) temporarily adopts a node's identity for its packet. Restore the
        // physical hub identity before measuring/packaging the hub's own heartbeat.
        manager.set_device_name("HubName");
        manager.set_instance_num(0);
        manager.measure();
        manager.package();
        saveCurrentPacketToSD(); // Queue the heartbeat through the same checked SD path.

        timer = millis();
    }
    publishPending();
}

/////////////////////////////////
/* SAVE THIS PACKET BEFORE MQTT */
/////////////////////////////////
void saveCurrentPacketToSD() {
    if (hypnos.logToSD()) {
        return;
    }
    SDManager *sd = hypnos.getSDManager();
    // Only retry the safely rolled-back batch part of this same sample. Repeating logToSD()
    // could write a second CSV row after the first row succeeded but its batch append failed.
    if (sd != nullptr && sd->retryBatch()) {
        return;
    }
    storageNeedsRetry = true;
    Serial.println(
        F("SD save incomplete; inspect CSV/batch results. RF ACK does not prove storage."));
}

///////////////////////////////
/* UPLOAD AND RETRY FROM SD  */
///////////////////////////////
void publishPending() {
    if (!batchSD.shouldPublish()) {
        return;
    }
    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastUploadAttempt) < UPLOAD_RETRY_INTERVAL_MS) {
        return;
    }

    if (!mqtt.moduleInitialized) {
        mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
    }
    if (mqtt.moduleInitialized && !mqtt.publish(batchSD)) {
        Serial.println(F("Upload incomplete; the SD batch stays queued for retry."));
    }
    lastUploadAttempt = millis(); // Space attempts after a slow connection, not from its start.
}

void setRTC() {
    if (!Serial) {
        return;
    }

    Serial.println(F("Adjust RTC time? (y/n)"));
    unsigned long timer = millis();
    while (!Serial.available() && (millis() - timer) < 7000) {
        LOOM_FEED_WATCHDOG();
        delay(1);
    }
    if (!Serial.available()) {
        return;
    }
    int val = Serial.read();
    delay(50);
    while (Serial.available()) {
        Serial.read(); // flush the input buffer to avoid invalid input to rtc function
    }

    if (val == 'y') {
        hypnos.set_custom_time();
    }
}
