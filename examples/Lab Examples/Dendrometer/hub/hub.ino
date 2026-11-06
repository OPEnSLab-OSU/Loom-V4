#include <Loom_Manager.h> //4.7

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Radio/Loom_LoRa/DendrometerPacket.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

const unsigned long REPORT_INTERVAL = 1 * 60 * 60 * 1000;
// After a failed upload (e.g. LTE down), keep listening for nodes this long before trying again. A connect attempt can
// block the hub for minutes, and node batches sent meanwhile are lost; the readings wait safely in the kept batch.
const unsigned long UPLOAD_RETRY_WAIT = 30UL * 60 * 1000;
unsigned long lastUploadFailure = 0;
bool uploadFailed = false;
const char *NODE_NAME = "HazelnutDendrometer_"; // must match DEVICE_NAME in node.ino; compact packets carry only the number

Manager manager("Hub", 0);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
Loom_Analog batteryVoltage(manager);
Loom_LoRa lora(manager);
Loom_LTE lte(manager, "hologram", "", "", A5);
Loom_MongoDB mqtt(manager, lte);
// Batch size for uploading; must match the node's TRANSMIT_INTERVAL. `true` keeps a batch whose upload failed (e.g. LTE
// down) and sends it with the next one, instead of emptying it at the next log; it's cleared after each successful upload.
Loom_BatchSD batchSD(hypnos, 16, true);

void setup()
{
    // Write debug logs to the SD card (/debug/output_N.log)
    ENABLE_SD_LOGGING;
    ENABLE_FUNC_SUMMARIES;

    // Start the serial interface
    manager.beginSerial();

    // Enable the power rails on the hypnos
    hypnos.enable();

    setRTC();

    // Sets the LTE board to use batch SD to only start when we actually need to publish data
    lte.setBatchSD(batchSD);


    // load MQTT credentials from the SD card, if they exist
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

    // Set the client ID to something identifiable in the Mosquitto logs
    mqtt.setClientID("HazelnutDendrometerHub0_Test");

    // Each broker connect try can wait up to 75 s for the modem, so cap one attempt at 2 tries (~2.5 min) instead of 4
    mqtt.setMaxRetries(2);

    // Initialize the modules
    manager.initialize();

    // Upload a full batch carried over from before a reboot (e.g. the watchdog fired mid-upload) before the next log clears it.
    // Done before the watchdog is on: a slow connect here ends with MQTT's retry limit instead of a reboot loop.
    uploadBatch();

    // Reset the hub if it freezes (e.g. runs out of memory) instead of hanging until someone presses reset.
    // Enabled after initialize because the LTE power-up and connect there take longer than 16 s.
    Watchdog.enable(16000);
}

void loop()
{
    Watchdog.reset();

    // Wait 5 seconds for a message: a whole batch from a node, or a heartbeat (see DendrometerPacket.h)
    if (lora.receive(5000))
    {
        DendrometerPacket::Heartbeat heartbeat;
        if (lora.rawLength() == 0)
            WARNING(F("Ignoring an old-format JSON packet, reflash that node with the current node.ino"));
        else if (DendrometerPacket::readHeartbeat(lora.rawData(), lora.rawLength(), heartbeat))
            logHeartbeat(heartbeat);
        else
            handlePackedBatch(lora.rawData(), lora.rawLength());
    }
  static unsigned long timer = millis();
  if (millis() - timer > REPORT_INTERVAL)
      {
          manager.set_device_name("Hub");
          manager.set_instance_num(0);

          manager.measure();
          manager.package();
          manager.display_data();
          // Skipped while uploads are backing off; a failed report doesn't start a backoff (it has no reconnect retry,
          // so a stale connection alone can fail it)
          if (uploadAllowed())
              mqtt.publish();

          timer = millis();
      }
}

DateTime toLocalTime(DateTime utc)
{
    return hypnos.getLocalTime(utc);
}

/**
 * Record a node's heartbeat in the debug log only (no LTE upload, so it costs the hub nothing); a script can read the
 * "Heartbeat from" lines to see how often each node is heard and how its battery and signal change between batches
 */
void logHeartbeat(const DendrometerPacket::Heartbeat &hb)
{
    LOGF("Heartbeat from %i: packet %u, %u mV, node heard hub at %i dBm, hub heard node at %i dBm, node time %lu",
         hb.node, hb.packet, hb.batteryMV, hb.rssi, lora.getSignalStrength(), (unsigned long)hb.time);
}

/**
 * Unpack a compact batch (see DendrometerPacket.h) and log and upload each reading exactly as an old-style packet would be
 */
void handlePackedBatch(const uint8_t *data, uint8_t len)
{
    uint8_t count = DendrometerPacket::readingCount(data, len);
    if (count == 0)
    {
        WARNINGF("Dropping invalid compact packet (%i bytes)", len);
        return;
    }
    // No duplicate check needed: RHReliableDatagram already drops resends after a lost ACK
    uint8_t node = DendrometerPacket::nodeNumber(data);

    for (uint8_t i = 0; i < count; i++)
    {
        Watchdog.reset();
        DendrometerPacket::unpack(data, i, manager.getDocument(), NODE_NAME, toLocalTime);
        manager.set_device_name(NODE_NAME);
        manager.set_instance_num(node);
        manager.addData("Node", "Number", node);
        manager.display_data();
        hypnos.logToSD();
    }

    // Upload once per message rather than per reading, so a failed upload costs one attempt, not 16
    uploadBatch();
}

/* Whether to try LTE now: not within UPLOAD_RETRY_WAIT of a failed upload */
bool uploadAllowed()
{
    return !uploadFailed || millis() - lastUploadFailure >= UPLOAD_RETRY_WAIT;
}

void recordUpload(bool ok)
{
    uploadFailed = !ok;
    if (!ok)
    {
        lastUploadFailure = millis();
        WARNINGF("Upload failed; next try in %lu min, readings are kept until then", UPLOAD_RETRY_WAIT / 60000);
    }
}

/**
 * Upload the batch once it holds a full batch (more if an earlier upload failed), then empty it so a reboot doesn't carry
 * it over and upload it again. A failed batch is kept and sent with the next upload.
 */
void uploadBatch()
{
    if (!batchSD.shouldPublish() || !uploadAllowed())
        return;
    // One JSON message per reading. To try the bit-packed draft (one small message per batch), point mqtt_creds.json
    // at the packed broker (port 1884, see packed_bridge.py) and use mqtt.publishPacked(batchSD) instead.
    bool ok = mqtt.publish(batchSD);
    if (ok)
        batchSD.clear();
    recordUpload(ok);
}


void setRTC()
{
    if (!Serial)
        return;

    Serial.println(F("Adjust RTC time? (y/n)"));
    unsigned long timer = millis();
    while (!Serial.available() && (millis() - timer) < 7000)
        ;
    if (!Serial.available())
        return;
    int val = Serial.read();
    delay(50);
    while (Serial.available())
        Serial.read(); // flush the input buffer to avoid invalid input to rtc function

    if (val == 'y')
    {
        hypnos.set_custom_time();
    }
}