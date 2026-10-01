#include <Loom_Manager.h> // Loom 4.9
#include <Diagnostics/Loom_MemoryDiagnostics.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Actuators/Loom_Neopixel/Loom_Neopixel.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>

#include "AS5311.h"

//////////////////////////
/* DEVICE CONFIGURATION */
//////////////////////////
static const uint8_t NODE_NUMBER = 1;
static const char *DEVICE_NAME = "NodeName_";
////Select one wireless communication option
#define DENDROMETER_LORA

TimeSpan sleepInterval;
static const uint8_t TRANSMIT_INTERVAL = 4; // to save power, only transmit every X measurements
////Use teros 10?
// #define DENDROMETER_TEROS10
#ifdef DENDROMETER_TEROS10
#include <Sensors/Analog/Loom_Teros10/Loom_Teros10.h>
#endif
//////////////////////////
//////////////////////////

// Pins
#define AS5311_CS A3 // 9 for LB version, A3 otherwise
#define AS5311_CLK A5
#define AS5311_DO A4
#define BUTTON_PIN A1 // 11 for LB, A1 otherwise

// Loom
Manager manager(DEVICE_NAME, NODE_NUMBER);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST); // 3_2 for LB, 3_3 otherwise
// Loom Sensors
Loom_Analog analog(manager);
#ifdef DENDROMETER_TEROS10
Loom_Teros10 teros(manager, A0);
#endif
Loom_SHT31 sht(manager);
Loom_Neopixel statusLight(manager, false, false, true,
                          NEO_GRB); // using channel 2 (physical pin A2). use RGB for through-hole
                                    // LED devices. GRB otherwise.

// magnet sensor
AS5311 magnetSensor(AS5311_CS, AS5311_CLK, AS5311_DO);

// wireless
#if defined DENDROMETER_LORA
static_assert(NODE_NUMBER > 0 && NODE_NUMBER < 255,
              "Node address must avoid hub 0 and broadcast 255.");
// The short constructor takes transmit power, not a radio address. The Manager's instance
// supplies NODE_NUMBER automatically; keep the default 23 dBm power/retry settings.
Loom_LoRa lora(manager);
#else
#warning Wireless communication disabled!
#endif

Loom_MemoryDiagnostics memoryDiagnostics;

// Global Variables
volatile bool buttonPressed = false; // Check to see if button was pressed

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

/**
 * Program setup
 */
void setup() {

    pinMode(BUTTON_PIN, INPUT_PULLUP); // Enable pullup on button pin. this is necessary for the
                                       // interrupt (and the button check on the next line)
    delay(10);                         // hold down device button to reset time
    bool userInput = !digitalRead(
        BUTTON_PIN); // wait for serial connection ONLY if button is pressed (low reading)
    manager.beginSerial(userInput); // wait for serial connection ONLY if button is pressed
    memoryDiagnostics.checkpoint(F("post_global_ctor"), manager.getDocument(), -1);

    // Build the SD base from the configured node instead of always calling every node "1".
    char logName[Manager::DEVICE_NAME_SIZE];
    snprintf(logName, sizeof(logName), "%s%u", DEVICE_NAME, static_cast<unsigned int>(NODE_NUMBER));
    hypnos.setLogName(logName); // SD card CSV file name
    hypnos.enable();
    sleepInterval = hypnos.getConfigFromSD("HypnosConfig.json");

    memoryDiagnostics.checkpoint(F("pre_initialize"), manager.getDocument(), -1);
    manager.initialize();
    memoryDiagnostics.checkpoint(F("post_initialize"), manager.getDocument(), -1);
    setRTC(userInput);

    checkMagnetSensor();
    alignMagnetSensor();

    hypnos.registerInterrupt(ISR_RTC);
}

/**
 * Main loop
 */
void loop() {
    memoryDiagnostics.beginCycle();
    memoryDiagnostics.checkpoint(F("loop_start"), manager.getDocument(), -1);
    measure();
    memoryDiagnostics.checkpoint(F("post_measure_sd"), manager.getDocument(), -1);
    if (buttonPressed) // if interrupt button was pressed, display staus of magnet sensor
    {
        displayMagnetStatus(magnetSensor.getMagnetStatus());
        delay(3000);
        statusLight.set_color(2, 0, 0, 0, 0); // LED Off
        buttonPressed = false;
    }
    transmit();
    memoryDiagnostics.checkpoint(F("post_transmit"), manager.getDocument(), -1);
    memoryDiagnostics.checkpoint(F("pre_sleep"), manager.getDocument(), -1);
    sleepCycle();
    memoryDiagnostics.checkpoint(F("post_wake"), manager.getDocument(), -1);
}

/**
 * Perform all measurements for the dendrometer and put them into a packet.
 * Log to SD card.
 */
void measure() {
    manager.measure();
    manager.package();

    measureVPD();
    magnetSensor.measure(manager);
    // Log whether system woke up from button or not
    manager.addData("Button", "Pressed?", buttonPressed);

    hypnos.logToSD();

    // delay(5000);
    // manager.display_data();
}

/**
 * Log readings from the SHT31 sensor. Also calculate and log VPD.
 */
void measureVPD() {
    float SVP, VPD, temperature, humidity;

    temperature = sht.getTemperature();
    humidity = sht.getHumidity();

    // Tetens equation
    SVP = (0.61078 * expf((17.2694f * temperature) / (temperature + 237.3f)));
    VPD = SVP * (1 - (humidity / 100));

    manager.addData("SHT31", "VPD", VPD);
}

/**
 * transmit the batch data packet over LoRa
 */
void transmit() {
#ifdef DENDROMETER_LORA
    static_assert(TRANSMIT_INTERVAL >= 1, "TRANSMIT_INTERVAL must be at least 1 measurement.");
    static uint8_t loopCounter = TRANSMIT_INTERVAL > 1 ? TRANSMIT_INTERVAL - 2 : 0;
    loopCounter++;
    if (loopCounter >= TRANSMIT_INTERVAL) {
        if (!lora.send(0)) {
            Serial.println(F("LoRa delivery failed; this sample remains in the node CSV."));
        }
        loopCounter = 0;
    }
#endif
}

/**
 * Shut down the device for a specified time period to save power.
 */
void sleepCycle() {
    hypnos.setInterruptDuration(sleepInterval);
    // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
    hypnos.reattachRTCInterrupt();
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), ISR_BUTTON, FALLING);

    // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
    hypnos.sleep();
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
}

// Interrupt routines
void ISR_RTC() { hypnos.wakeup(); }

void ISR_BUTTON() {
    buttonPressed = true;
    hypnos.wakeup();
}

/**
 * Magnet alignment procedure. Displays magnet sensor to user until
 * the magnet is determined to be properly aligned and maintains that alignment
 * for a certain amount of time
 */
void alignMagnetSensor() {
    magnetStatus status;

    Serial.println(F("<Dendrometer> Waiting for magnet alignment"));
    const uint32_t started = millis();
    const uint32_t ALIGNMENT_TIMEOUT_MS = 60000;
    while (static_cast<uint32_t>(millis() - started) < ALIGNMENT_TIMEOUT_MS) {
        LOOM_FEED_WATCHDOG();
        status = magnetSensor.getMagnetStatus();
        displayMagnetStatus(status);
        delay(100);
        if (status == magnetStatus::green && checkStableAlignment()) {
            flashColor(0, 255, 0);
            return;
        }
    }
    Serial.println(F("Magnet alignment timed out; logging continues with validity flags."));
    flashColor(255, 0, 0);
}

/**
 * Check the magnet sensor alignment status and display it on the multi-color LED
 * @param   status  the magnetStatus to display
 */
void displayMagnetStatus(magnetStatus status) {
    switch (status) {
    case magnetStatus::yellow:
        statusLight.set_color(2, 0, 255, 100, 0); // yellow
        break;
    case magnetStatus::green:
        statusLight.set_color(2, 0, 0, 255, 0); // green
        break;
    case magnetStatus::error: // Fall through
    case magnetStatus::red:   // Fall through
    default:
        statusLight.set_color(2, 0, 255, 0, 0); // red
        break;                                  // do nothing
    }
}

/**
 * Flashes status light
 * @param   r   red color value (unsigned 8 bit number)
 * @param   g   green color value (unsigned 8 bit number)
 * @param   b   blue color value (unsigned 8 bit number)
 */
void flashColor(uint8_t r, uint8_t g, uint8_t b) {
    for (auto _ = 6; _--;) {
        LOOM_FEED_WATCHDOG();
        statusLight.set_color(2, 0, r, g, b);
        delay(250);
        statusLight.set_color(2, 0, 0, 0, 0); // off
        delay(250);
    }
}

/**
 * Make sure calibration is stable before proceeding
 * Returns true if the sensor remains aligned for the next three seconds
 */
bool checkStableAlignment() {
    const unsigned int CHECK_TIME = 3000;
    magnetStatus status;
    bool aligned = true;

    for (unsigned int i = 0; i < (CHECK_TIME / 100); i++) {
        LOOM_FEED_WATCHDOG();
        status = magnetSensor.getMagnetStatus();
        if (status != magnetStatus::green) {
            aligned = false;
            break;
        }
        delay(100);
    }

    return aligned;
}

/**
 * Checks to see if a magnet sensor is connected and functioning.
 */
void checkMagnetSensor() {
    uint32_t data = magnetSensor.getRawData();
    if (AS5311::isValidReading(data)) { // Completed conversion, no overflow, even parity.
        return;
    }
    for (auto _ = 6; _--;) {
        flashColor(255, 100, 0); // if the check didn't pass, alert the user by flashing the LED
    }
}

/**
 * Ask the user to set a custom time
 */
void setRTC(bool wait) {
    if (!wait || !Serial) {
        return;
    }

    Serial.println(F("<Dendrometer> Adjust RTC time? (y/n)"));
    const uint32_t started = millis();
    while (!Serial.available() && static_cast<uint32_t>(millis() - started) < 7000) {
        LOOM_FEED_WATCHDOG();
        delay(1);
    }
    if (!Serial.available()) {
        return; // A disconnected terminal must not prevent the first sensor sample.
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
