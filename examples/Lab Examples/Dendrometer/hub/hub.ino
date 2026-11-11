#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Radio/Loom_LoRa/DendrometerPacket.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SETTINGS: change these
//////////////////////////////////////////////////////////////////////////////////////////////////////
const char *NODE_NAME = "HazelnutDendrometer_";      // must match DEVICE_NAME in node.ino
const char *MQTT_CLIENT_ID = "HazelnutDendrometerHub0_Test"; // how the hub shows up in the broker logs
const char *LTE_APN = "hologram";                     // the SIM provider's APN
const TIME_ZONE LOCAL_TIME_ZONE = TIME_ZONE::PST;     // for time_local in the logs; readings are stored in UTC

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ADVANCED: the defaults work, change only if you know you need to
//////////////////////////////////////////////////////////////////////////////////////////////////////
// Time slots: node N sends N * SLOT_SECONDS after every CYCLE_SECONDS mark in UTC, e.g. node 3 at 00:03, 04:03, 08:03.
// The nodes take these from the hub, so changing them needs no node reflash.
// shortcut: a node sends at most its newest 64 readings (MAX_UNSENT in node.ino) per cycle, so its measurement interval
// must be at least CYCLE_SECONDS / 64 (3.75 min); shorten the cycle if nodes ever measure more often
const uint32_t CYCLE_SECONDS = 4UL * 60 * 60;            // how often each node sends: 16 readings at 15 min
const uint32_t SLOT_SECONDS = 60;                        // per node: a send and its upload take about 10 s
const uint8_t MAX_NODES = 64;                            // highest node number + 1 that the hub checks for repeats
const unsigned long REPORT_INTERVAL = 60UL * 60 * 1000;  // the hub's own status upload: hourly
const unsigned long UPLOAD_RETRY_WAIT = 30UL * 60 * 1000; // after a failed upload, wait 30 min before trying LTE again

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Nothing below this line needs changing
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Messages one node may send in its slot: up to 64 unsent readings (MAX_UNSENT in node.ino) plus a split
const uint8_t MAX_MESSAGES = 5;
uint8_t messages[MAX_MESSAGES][RH_RF95_MAX_MESSAGE_LEN];
uint8_t lengths[MAX_MESSAGES];
uint8_t newReadings[MAX_MESSAGES];

// Newest reading logged from each node (by node number), to skip readings a node sends again because it missed the
// hub's ACK. Forgotten on reboot and not kept for nodes past MAX_NODES; MongoDB's unique index catches those instead.
uint32_t lastTime[MAX_NODES];
uint32_t lastPacket[MAX_NODES];
uint8_t highestNode = 0; // highest node number heard, to know when the nodes' slots are over

unsigned long lastUploadFailure = 0;
bool uploadFailed = false;

Manager manager("Hub", 0);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, LOCAL_TIME_ZONE);
Loom_Analog batteryVoltage(manager);
Loom_LoRa lora(manager);
Loom_LTE lte(manager, LTE_APN, "", "", A5);
Loom_MongoDB mqtt(manager, lte);
// Readings waiting to be uploaded. `true` keeps readings whose upload failed (e.g. LTE down) instead of emptying the file
// at the next log; it's cleared after each successful upload. 16: a backlog is sent once it holds at least that many.
Loom_BatchSD batchSD(hypnos, 16, true);

void setup()
{
    // Write debug logs to the SD card (/debug/output_N.log)
    ENABLE_SD_LOGGING;
    ENABLE_FUNC_SUMMARIES;

    manager.beginSerial();
    hypnos.enable();
    // The hub's clock sets every node's clock (see sendSchedule), so it must be right
    setRTC();

    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
    mqtt.setClientID(MQTT_CLIENT_ID);
    // Each broker connect try can wait up to 75 s for the modem, so cap one attempt at 2 tries (~2.5 min) instead of 4
    mqtt.setMaxRetries(2);

    manager.initialize();

    // Upload readings carried over from before a reboot (e.g. the watchdog fired mid-upload).
    // Done before the watchdog is on: a slow connect here ends with MQTT's retry limit instead of a reboot loop.
    uploadBatch(0, 0);

    // Reset the hub if it freezes instead of hanging until someone presses reset.
    // Enabled after initialize because the LTE power-up and connect there take longer than 16 s.
    Watchdog.enable(16000);
}

void loop()
{
    Watchdog.reset();

    // Wait 5 seconds for a batch from a node (see DendrometerPacket.h)
    if (lora.receive(5000))
    {
        if (lora.rawLength() == 0)
            WARNING(F("Ignoring an old-format JSON packet, reflash that node with the current node.ino"));
        else
            handleNodeSend();
    }

    // The hub's own report waits until the nodes' slots are over, so it never uploads while a node is sending
    static unsigned long lastReport = millis();
    if (millis() - lastReport > REPORT_INTERVAL && !slotsRunning())
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
        lastReport = millis();
    }
}

/* Whether it's the part of the cycle where nodes have their slots */
bool slotsRunning()
{
    return hypnos.getCurrentTime().unixtime() % CYCLE_SECONDS < (highestNode + 1UL) * SLOT_SECONDS;
}

/**
 * Handle one node's send: collect all its messages, reply with its next slot, then log and upload the readings.
 * Replying and uploading wait for the node's last message, so the hub is never busy while the node is still sending.
 */
void handleNodeSend()
{
    uint8_t n = 0;
    bool more;
    do
    {
        lengths[n] = lora.rawLength();
        memcpy(messages[n], lora.rawData(), lengths[n]);
        more = DendrometerPacket::readingCount(messages[n], lengths[n]) && DendrometerPacket::moreFollows(messages[n]);
        n++;
    } while (more && n < MAX_MESSAGES && lora.receiveRaw(6000));

    // Reply right away, while the node is still listening (normally one node; a message from another one is rare)
    for (uint8_t i = 0; i < n; i++)
        if (DendrometerPacket::readingCount(messages[i], lengths[i]) && !DendrometerPacket::moreFollows(messages[i]))
            sendSchedule(DendrometerPacket::nodeNumber(messages[i]));

    uint8_t logged = 0;
    for (uint8_t i = 0; i < n; i++)
        logged += newReadings[i] = logPackedBatch(messages[i], lengths[i]);
    uploadBatch(n, logged);
}

/* Tell a node the hub's time and its next slot */
void sendSchedule(uint8_t node)
{
    uint32_t now = hypnos.getCurrentTime().unixtime();
    uint32_t next = now - now % CYCLE_SECONDS + node % (CYCLE_SECONDS / SLOT_SECONDS) * SLOT_SECONDS;
    while (next < now + SLOT_SECONDS) // never the slot that's running now
        next += CYCLE_SECONDS;
    uint8_t reply[DendrometerPacket::SCHEDULE_SIZE];
    DendrometerPacket::makeSchedule(reply, now, next, CYCLE_SECONDS);
    if (lora.sendRaw(reply, sizeof(reply), node))
        LOGF("Node %i sends next at %lu (in %lu s)", node, (unsigned long)next, (unsigned long)(next - now));
    else
        WARNINGF("Node %i didn't get its schedule; it keeps its old one", node);
}

DateTime toLocalTime(DateTime utc)
{
    return hypnos.getLocalTime(utc);
}

/**
 * Unpack a compact batch (see DendrometerPacket.h) and log each new reading exactly as an old-style packet would be.
 * Returns how many readings were new.
 */
uint8_t logPackedBatch(const uint8_t *data, uint8_t len)
{
    uint8_t count = DendrometerPacket::readingCount(data, len);
    if (count == 0)
    {
        WARNINGF("Dropping invalid compact packet (%i bytes)", len);
        return 0;
    }
    uint8_t node = DendrometerPacket::nodeNumber(data);
    if (node > highestNode)
        highestNode = node;

    uint8_t logged = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        Watchdog.reset();
        uint32_t time = DendrometerPacket::readingTime(data, i);
        uint32_t packet = DendrometerPacket::readingPacket(data, i);
        if (node < MAX_NODES)
        {
            // A repeat is no newer in both time and packet number, and within the 64 packets a node resends at most
            // (MAX_UNSENT in node.ino). Time or packet alone can go back after a clock change or a node reboot.
            // shortcut: a node rebooted <64 packets ago whose clock is also set back before its last reading has those
            // readings skipped (they stay in its CSV); fine while clocks are only set forward, revisit with time sync
            if (time <= lastTime[node] && packet <= lastPacket[node] && lastPacket[node] - packet < 64)
                continue;
            lastTime[node] = time;
            lastPacket[node] = packet;
        }
        logged++;
        DendrometerPacket::unpack(data, i, manager.getDocument(), NODE_NAME, toLocalTime);
        manager.set_device_name(NODE_NAME);
        manager.set_instance_num(node);
        manager.addData("Node", "Number", node);
        manager.display_data();
        hypnos.logToSD();
    }

    if (logged < count)
        LOGF("Skipped %i repeated reading(s) from node %i", count - logged, node);
    return logged;
}

/**
 * Upload the readings waiting in the batch, then empty it so a reboot doesn't carry them over and upload them again.
 * Normally only the `n` messages just received are waiting (`logged` new readings): send each one's raw bytes as one
 * message (dendro_forwarder.py turns them back into JSON on the server). If an earlier upload failed, the batch also
 * holds those readings, so send them all the old way, one JSON message per reading, once there are at least 16; that can
 * take long enough to run into the next node's slot, which then retries a cycle later. A message with some repeated
 * readings is still sent whole; MongoDB's unique index drops the repeats.
 */
void uploadBatch(uint8_t n, uint8_t logged)
{
    if (!uploadAllowed())
        return;
    bool ok = true;
    if (n > 0 && batchSD.getCurrentBatch() == logged)
    {
        for (uint8_t i = 0; i < n && ok; i++)
            if (newReadings[i] > 0)
                ok = mqtt.publishRaw(NODE_NAME, messages[i], lengths[i]);
    }
    else if (batchSD.shouldPublish())
        ok = mqtt.publish(batchSD);
    else
        return;

    if (ok)
        batchSD.clear();
    uploadFailed = !ok;
    if (!ok)
    {
        lastUploadFailure = millis();
        WARNINGF("Upload failed; next try in %lu min, readings are kept until then", UPLOAD_RETRY_WAIT / 60000);
    }
}

/* Whether to try LTE now: not within UPLOAD_RETRY_WAIT of a failed upload */
bool uploadAllowed()
{
    return !uploadFailed || millis() - lastUploadFailure >= UPLOAD_RETRY_WAIT;
}

/* Ask over serial (if connected) whether to set the RTC; gives up after 7 s so an unattended hub keeps running */
void setRTC()
{
    if (!Serial)
        return;

    Serial.println(F("Adjust RTC time? (y/n)"));
    unsigned long start = millis();
    while (!Serial.available() && millis() - start < 7000)
        ;
    if (!Serial.available())
        return;
    int val = Serial.read();
    delay(50);
    while (Serial.available())
        Serial.read(); // flush the input buffer to avoid invalid input to rtc function

    if (val == 'y')
        hypnos.set_custom_time();
}
