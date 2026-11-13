#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Actuators/Loom_Neopixel/Loom_Neopixel.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Radio/Loom_LoRa/DendrometerPacket.h>

#include "AS5311.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SETTINGS: change these
//////////////////////////////////////////////////////////////////////////////////////////////////////
static const uint8_t NODE_NUMBER = 13;                 // different for every node, 1 to 78 (one slot each in the hub's cycle)
static const char *DEVICE_NAME = "HazelnutDendrometer_"; // must match NODE_NAME in hub.ino
static const uint16_t MEASURE_MINUTES = 5;            // time between measurements; 0 uses HypnosConfig.json instead
//#define LB_VERSION                                  // uncomment for the LB board
#define LED_COLOR_ORDER NEO_RGB                       // NEO_RGB for through-hole LEDs, NEO_GRB otherwise
#define DENDROMETER_LORA                              // send readings to the hub (comment out to only log to SD)
//#define DENDROMETER_TEROS10                         // also measure a TEROS 10 soil moisture sensor on A0

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ADVANCED: the defaults work, change only if you know you need to
//////////////////////////////////////////////////////////////////////////////////////////////////////
static const TIME_ZONE LOCAL_TIME_ZONE = TIME_ZONE::PST; // for time_local; a timezone in HypnosConfig.json wins
static const uint8_t TRANSMIT_INTERVAL = 16; // before the hub assigns a slot: send after this many measurements
static const uint8_t RETRY_INTERVAL = 4;     // before the hub assigns a slot: retry a failed send every X measurements
static const uint8_t MAX_UNSENT = 64;        // send at most the newest X unsent readings; older ones stay in the CSV

//////////////////////////////////////////////////////////////////////////////////////////////////////
// You shouldn't need to change anything below this line 
//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef LB_VERSION
#define AS5311_CS 9
#define BUTTON_PIN 11
static const HYPNOS_VERSION HYPNOS_BOARD = HYPNOS_VERSION::V3_2;
#else
#define AS5311_CS A3
#define BUTTON_PIN A1
static const HYPNOS_VERSION HYPNOS_BOARD = HYPNOS_VERSION::V3_3;
#endif
#define AS5311_CLK A5
#define AS5311_DO A4

Manager manager(DEVICE_NAME, NODE_NUMBER);
Loom_Hypnos hypnos(manager, HYPNOS_BOARD, LOCAL_TIME_ZONE);
Loom_Analog analog(manager);
#ifdef DENDROMETER_TEROS10
#include <Sensors/Analog/Loom_Teros10/Loom_Teros10.h>
Loom_Teros10 teros(manager, A0);
#endif
Loom_SHT31 sht(manager);
Loom_Neopixel statusLight(manager, false, false, true, LED_COLOR_ORDER); // status LED on channel 2 (pin A2)
AS5311 magnetSensor(AS5311_CS, AS5311_CLK, AS5311_DO);

#if defined DENDROMETER_LORA
// 23 dBm, 10 retries, 500 ms retry timeout. Address comes from the manager's instance number.
Loom_LoRa lora(manager, 23, 10, 500);
// `true` keeps readings the hub didn't acknowledge and sends them with the next batch
Loom_BatchSD batchSD(hypnos, TRANSMIT_INTERVAL, true);

// This node's send slot from the hub (see hub.ino): send at nextSend, then every slotCycle seconds, so nodes never
// transmit at the same time. 0 until the hub first replies; until then the node sends once it has a full batch.
uint32_t nextSend = 0;
uint32_t slotCycle = 0;
static const uint16_t REPLY_WAIT_MS = 3000; // how long to listen for the hub's reply after sending
void waitForSchedule();
#else
#warning Wireless communication disabled!
#endif

TimeSpan sleepInterval;              // time between measurements: MEASURE_MINUTES, or HypnosConfig.json if that's 0
static const unsigned long ALIGN_WAIT_MS = 10UL * 60 * 1000; // how long setup waits for a green magnet
uint32_t nextMeasure = 0;            // unix time (UTC) of the next measurement
volatile bool buttonPressed = false; // woken by the button

void sleepCycle();
void ISR_RTC();
void ISR_BUTTON();
void measure();
void measureVPD();
void transmit();
void setRTC(bool);
void checkMagnetSensor();
void alignMagnetSensor();
bool checkStableAlignment();
void displayMagnetStatus(magnetStatus);
void flashColor(uint8_t r, uint8_t g, uint8_t b);

void setup()
{
    // Write debug logs to the SD card (/debug/output_N.log)
    ENABLE_SD_LOGGING;
    ENABLE_FUNC_SUMMARIES;

    // Holding the button at power-up waits for a serial connection and offers to set the clock
    pinMode(BUTTON_PIN, INPUT_PULLUP); // also needed for the button interrupt
    delay(10);
    bool userInput = !digitalRead(BUTTON_PIN);
    manager.beginSerial(userInput);

    char logName[40];
    snprintf(logName, sizeof(logName), "%s%idata", DEVICE_NAME, NODE_NUMBER);
    hypnos.setLogName(logName); // SD card CSV file name, e.g. HazelnutDendrometer_3data
    hypnos.enable();
    sleepInterval = hypnos.getConfigFromSD("HypnosConfig.json"); // also reads the timezone, so read it either way
    if (MEASURE_MINUTES > 0)
        sleepInterval = TimeSpan(MEASURE_MINUTES * 60L); // in seconds: the minutes argument only holds up to 127

    manager.initialize();
    setRTC(userInput);
    nextMeasure = hypnos.getCurrentTime().unixtime(); // phase = boot time, so nodes flashed together don't all send together before they have slots

    checkMagnetSensor();
    alignMagnetSensor();

    hypnos.registerInterrupt(ISR_RTC);
}

void loop()
{
    // A wake is for a measurement, the button or this node's send slot (sleepCycle wakes for whichever is first)
    uint32_t now = hypnos.getCurrentTime().unixtime();
    if (buttonPressed || now + 10 >= nextMeasure)
    {
        measure();
        // Keep the phase: readings exactly an interval apart, the next one after now + 10.
        // A button wake isn't due, so it doesn't shift the schedule.
        uint32_t interval = sleepInterval.totalseconds();
        if (nextMeasure <= now + 10)
            nextMeasure += (now + 10 - nextMeasure) / interval * interval + interval;
        if (buttonPressed) // show the magnet sensor status on the LED
        {
            displayMagnetStatus(magnetSensor.getMagnetStatus());
            delay(3000);
            statusLight.set_color(2, 0, 0, 0, 0); // LED off
            buttonPressed = false;
        }
    }
    transmit();
    sleepCycle();
}

/* Take all measurements and log them to the SD card */
void measure()
{
    manager.measure();
    manager.package();
    measureVPD();
    magnetSensor.measure(manager);
    manager.addData("Button", "Pressed?", buttonPressed); // whether the button woke the node

    hypnos.logToSD();
    hypnos.display_data();
}

/* Vapor pressure deficit from the SHT31 (Tetens equation); DendrometerPacket and the forwarder recompute it the same way */
void measureVPD()
{
    float temperature = sht.getTemperature();
    float humidity = sht.getHumidity();
    float svp = 0.61078 * pow(2.71828, (17.2694 * temperature) / (temperature + 237.3));
    manager.addData("SHT31", "VPD", svp * (1 - humidity / 100));
}

/* Send the unsent readings to the hub when it's this node's turn */
void transmit()
{
#if defined DENDROMETER_LORA
    // Unsent readings stay in the batch file until the hub acknowledges them, so after a failure there are more than 16
    int waiting = batchSD.getCurrentBatch();
    if (nextSend)
    {
        // On the hub's schedule: send in this node's slot, once per cycle
        uint32_t now = hypnos.getCurrentTime().unixtime();
        if (now + 10 < nextSend)
            return;
        // Next cycle's slot unless the hub's reply moves it; a failed send retries then. Also skips slots missed while off.
        while (nextSend <= now + 10)
            nextSend += slotCycle;
        if (waiting == 0)
            return;
    }
    // No schedule yet: send once a batch is full, and after a failure retry every RETRY_INTERVAL measurements
    else if (!batchSD.shouldPublish() || (waiting - TRANSMIT_INTERVAL) % RETRY_INTERVAL != 0)
        return;
    // shortcut: skipped lines stay in the file until the next successful send clears it, fine for days of hub downtime
    int skip = waiting > MAX_UNSENT ? waiting - MAX_UNSENT : 0;

    DendrometerPacket::Writer packet;
    StaticJsonDocument<1500> reading;
    char line[1000];
    bool sent = true;
    File batch = batchSD.getBatch(); // a copy, so logging while sending can't close it
    while (sent && batch.available())
    {
        int len = batch.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';
        if (skip > 0)
        {
            skip--;
            continue;
        }
        if (len < 2 || deserializeJson(reading, line) != DeserializationError::Ok)
            continue; // skip a blank or corrupt line rather than lose the whole batch

        // Full, or packet numbers restarted after a reboot: send what we have and start a new message
        if (!packet.add(reading, NODE_NUMBER))
        {
            packet.setMoreFollows(true); // the hub waits for the rest before replying and uploading
            sent = lora.sendRaw(packet.data(), packet.length(), 0); // stop at the first failure; the hub isn't listening
            packet.clear();
            packet.add(reading, NODE_NUMBER);
        }
    }
    batch.close();

    if (sent && packet.length() > 0)
        sent = lora.sendRaw(packet.data(), packet.length(), 0);
    // Only forget the readings once the hub has acknowledged every message. If only some got through they're sent
    // again next time; the hub and MongoDB's unique index drop the repeats.
    if (sent)
    {
        batchSD.clear();
        waitForSchedule();
    }
#endif
}

#if defined DENDROMETER_LORA
/**
 * Listen briefly for the hub's reply to a send: set the clock to the hub's and take the slot it gives this node.
 * An older hub doesn't reply; the node then keeps its current schedule (or sends on a full batch if it has none).
 */
void waitForSchedule()
{
    uint32_t hubTime, next, cycle;
    if (!lora.receiveRaw(REPLY_WAIT_MS) ||
        !DendrometerPacket::readSchedule(lora.rawData(), lora.rawLength(), hubTime, next, cycle) || cycle < 60)
        return;
    // A hub whose RTC lost power restarts at 2000-01-01; don't copy that clock (or slots based on it) to every node
    if (hubTime < 1735689600) // 2025-01-01
    {
        WARNING(F("Ignoring the hub's schedule: its clock isn't set"));
        return;
    }
    uint32_t now = hypnos.getCurrentTime().unixtime();
    if (hubTime > now + 2 || now > hubTime + 2)
    {
        LOGF("Clock was %li s off the hub's, setting it", (long)(now - hubTime));
        hypnos.setTime(DateTime(hubTime));
        nextMeasure += hubTime - now; // keep the same time to the next measurement
        now = hubTime;
    }
    nextSend = next;
    slotCycle = cycle;
    // Measure in the send slot so one wake does both. Only when the cycle is a whole number of intervals: otherwise
    // re-phasing every cycle would make the reading spacing uneven.
    uint32_t interval = sleepInterval.totalseconds();
    if (cycle % interval == 0 && next > now + 11)
        nextMeasure = next - ((next - (now + 11)) / interval) * interval;
    LOGF("Hub's schedule: next send in %lu s, then every %lu s", (unsigned long)(next - hubTime), (unsigned long)cycle);
}
#endif

/* Sleep until the next measurement or this node's send slot, whichever comes first (or the button) */
void sleepCycle()
{
    uint32_t now = hypnos.getCurrentTime().unixtime();
    uint32_t wake = nextMeasure;
#if defined DENDROMETER_LORA
    if (nextSend && nextSend < wake)
        wake = nextSend;
#endif
    // At least 10 s: an alarm time that passes before the board is asleep would never fire
    hypnos.setInterruptDuration(TimeSpan(wake > now + 10 ? (int32_t)(wake - now) : 10));
    // Reattach after setting the alarm so it can trigger again
    hypnos.reattachRTCInterrupt();
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), ISR_BUTTON, FALLING);

    hypnos.sleep(); // execution stops here until an interrupt
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
}

void ISR_RTC()
{
    hypnos.wakeup();
}

void ISR_BUTTON()
{
    buttonPressed = true;
    hypnos.wakeup();
}

/* Show the magnet status on the LED until it's green and stays green, then flash green */
void alignMagnetSensor()
{
    Serial.println(F("<Dendrometer> Waiting for magnet alignment"));
    // Gives up after ALIGN_WAIT_MS so a reset in the field (brown-out, power blip) with the magnet off green still goes
    // back to measuring instead of waiting here until someone realigns it
    unsigned long start = millis();
    while (millis() - start < ALIGN_WAIT_MS)
    {
        magnetStatus status = magnetSensor.getMagnetStatus();
        displayMagnetStatus(status);
        delay(100);
        if (status == magnetStatus::green && checkStableAlignment())
        {
            flashColor(0, 255, 0);
            return;
        }
    }
    WARNING(F("Magnet not aligned in time, measuring anyway"));
    statusLight.set_color(2, 0, 0, 0, 0); // LED off
}

/* Show a magnet status on the LED: green, yellow, or red for red and error */
void displayMagnetStatus(magnetStatus status)
{
    switch (status)
    {
    case magnetStatus::green:
        statusLight.set_color(2, 0, 0, 255, 0);
        break;
    case magnetStatus::yellow:
        statusLight.set_color(2, 0, 255, 100, 0);
        break;
    default:
        statusLight.set_color(2, 0, 255, 0, 0);
        break;
    }
}

/* Flash the LED in a color six times */
void flashColor(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < 6; i++)
    {
        statusLight.set_color(2, 0, r, g, b);
        delay(250);
        statusLight.set_color(2, 0, 0, 0, 0);
        delay(250);
    }
}

/* Whether the magnet stays aligned (green) for the next 3 seconds */
bool checkStableAlignment()
{
    for (uint8_t i = 0; i < 30; i++)
    {
        if (magnetSensor.getMagnetStatus() != magnetStatus::green)
            return false;
        delay(100);
    }
    return true;
}

/* Flash orange if the magnet sensor isn't connected or working (its data must be nonzero with even parity) */
void checkMagnetSensor()
{
    uint32_t data = magnetSensor.getRawData();
    if (__builtin_parity(data) == 0 && data != 0)
        return;
    for (uint8_t i = 0; i < 6; i++)
        flashColor(255, 100, 0);
}

/* Ask over serial whether to set the clock; only when the button was held at power-up */
void setRTC(bool wait)
{
    if (!wait || !Serial)
        return;

    Serial.println(F("<Dendrometer> Adjust RTC time? (y/n)"));
    while (!Serial.available())
        ;
    int val = Serial.read();
    delay(50);
    while (Serial.available())
        Serial.read(); // flush the input buffer to avoid invalid input to rtc function

    if (val == 'y')
        hypnos.set_custom_time();
}
