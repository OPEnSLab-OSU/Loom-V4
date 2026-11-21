/*
 * Title: Evaporometer SDI-12 Implementation
 *
 * Description: Uses ADS1232 to measure voltage 
 * across a load cell and thermistor temperature.
 * Responds to SDI-12 commands on data bus to provide measurements.
 *
 * Date: May 5, 2025
 */

// Loom manager must be included first
#include <Loom_Manager.h>
#include <Sensors/SPI/Loom_ADS1232/ADS1232_Lib_Fixed.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <SDI12.h>
#include <Wire.h>
#include <algorithm>

// --------------------------------------------------------------------
//                          Pin Definitions
// --------------------------------------------------------------------
constexpr int PIN_PDWN      = A5;
constexpr int PIN_SCLK      = A4;
constexpr int PIN_DOUT      = A3;
constexpr int PIN_VBAT      = A7;
constexpr int PIN_THERM     = A1;
#define SDI12_DATA_PIN      10

// --------------------------------------------------------------------
//                           Constants
// --------------------------------------------------------------------
// Calibration constants
constexpr long  LOAD_CELL_OFFSET    = 9617696; 
constexpr long LOAD_CELL_SCALE     = 2093; 

// Temperature-Correction Parameters
constexpr float TEMP_CORR_M         = -0.1143f;
constexpr float TEMP_CORR_B         = 399.44f;
constexpr float REF_WEIGHT          = 0.0f;

// Battery measurement constants
constexpr float VBAT_SCALE_FACTOR   = (2.0f * 3.3f / 1024.0f);

// Number of samples
constexpr int   NUM_WEIGHT_SAMPLES  = 3;
constexpr int   NUM_READINGS        = 10; // readings per sample
constexpr uint32_t ADC_STARTUP_TIMEOUT_MS = 500;
constexpr uint32_t ADC_CONVERSION_TIMEOUT_MS = 110; // Allow for the ADS1232 10 SPS mode.
constexpr unsigned int MAX_COMMAND_LENGTH = 16;

// --------------------------------------------------------------------
//                      SDI-12 Variables
// --------------------------------------------------------------------
char sensorAddress = '0';
int  state         = 0;
#define WAIT 0
#define INITIATE_MEASUREMENT   1

// Create SDI-12 slave instance
SDI12 sdi(SDI12_DATA_PIN);

// --------------------------------------------------------------------
//                       Global Objects
// --------------------------------------------------------------------
Manager     manager("Device", 1);
ADS1232_Lib_Fixed ads(PIN_PDWN, PIN_SCLK, PIN_DOUT);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true, false);

// --------------------------------------------------------------------
//                    Forward Declarations
// --------------------------------------------------------------------
bool pollSensor(float* measurementValues);
void parseSdi12Cmd(String command, String* dValues);
void formatOutputSDI(float* measurementValues, String* dValues, unsigned int maxChar);
bool measureWeight(float &weight);
float measureBatteryVoltage();
float computeCorrectedWeight(float wMeasured, float temperature);

// --------------------------------------------------------------------
//                         Sensor Polling
// --------------------------------------------------------------------
bool pollSensor(float* measurementValues) {
    float weight;
    if (!measureWeight(weight)) {
        return false;
    }
    int rawTherm = analogRead(PIN_THERM);
    float temperature = static_cast<float>(rawTherm);
    float corrected = computeCorrectedWeight(weight, temperature);
    float vbat = measureBatteryVoltage();

    // The thermistor value is an uncalibrated ADC reading.
    measurementValues[0] = temperature;
    measurementValues[1] = weight;
    measurementValues[2] = corrected;
    measurementValues[3] = vbat;

    return true;
}

// --------------------------------------------------------------------
//                       SDI-12 Command Parser
// --------------------------------------------------------------------
void parseSdi12Cmd(String command, String* dValues) {
    char garb_chars[5] = {'@',' ', '`', '|','~'};
    for (char c : garb_chars) {
        if (command.startsWith(String(c))) {
            command.remove(0, 1);
        }
    }

    if (command.charAt(0) != sensorAddress && command.charAt(0) != '?') {
        return;
    }

    String responseStr = "";

    if (command.length() > 1) {
        switch (command.charAt(1)) {
            case '?':
                responseStr = ""; break;
            case 'I':
                responseStr = "14EVAPRMTR000001.0001"; break;
            case 'M':
                // 005 seconds until data, 04 values to follow
                responseStr = "0054";
                state = INITIATE_MEASUREMENT;
                break;
            case 'D': {
                const char index = command.charAt(2);
                if (command.length() != 3 || index < '0' || index > '9') {
                    return;
                }
                responseStr = dValues[index - '0'];
                break;
            }
            case 'A':
                if (command.length() != 3 || !isAlphaNumeric(command.charAt(2))) {
                    return;
                }
                sensorAddress = command.charAt(2);
                break;
            default:
                responseStr = "UNKN"; break;
        }
    }

    String out = String(sensorAddress) + responseStr + "\r\n";
    sdi.sendResponse(out);
    sdi.forceListen();
}

// --------------------------------------------------------------------
//                    SDI-12 Data Formatter
// --------------------------------------------------------------------
void formatOutputSDI(float* measurementValues, String* dValues, unsigned int maxChar) {
    dValues[0] = "";
    int j = 0;
    for (int i = 0; i < 4; i++) {
        String valStr = String(measurementValues[i], 2);
        if (valStr.charAt(0) != '-') { valStr = "+" + valStr; }
        if (dValues[j].length() + valStr.length() < maxChar) {
            dValues[j] += valStr;
        } else {
            j++;
            dValues[j] = valStr;
        }
    }
    while (j < 9) { dValues[++j] = ""; }
}

// --------------------------------------------------------------------
//                       Setup Function
// --------------------------------------------------------------------
void setup() {
    pinMode(SDI12_DATA_PIN, INPUT_PULLDOWN);
    manager.beginSerial(false);
    hypnos.enable();
    manager.initialize();
    
    sdi.begin();
    
    ads.set_scale(LOAD_CELL_SCALE);
    ads.set_offset(LOAD_CELL_OFFSET);

    delay(500);
    sdi.forceListen();
}

// --------------------------------------------------------------------
//                       measureWeight
// --------------------------------------------------------------------
bool measureWeight(float &weight) {
    if (!ads.power_up(ADC_STARTUP_TIMEOUT_MS)) {
        ads.power_down();
        return false;
    }
    delay(1000);
    float weights[NUM_WEIGHT_SAMPLES];
    for (float &reading : weights) {
        reading = ads.units_read(NUM_READINGS, ADC_CONVERSION_TIMEOUT_MS);
        if (!ads.last_read_valid()) {
            ads.power_down();
            return false;
        }
    }
    ads.power_down();
    std::sort(weights, weights + NUM_WEIGHT_SAMPLES);
    weight = weights[NUM_WEIGHT_SAMPLES / 2];
    return true;
}

// --------------------------------------------------------------------
//                   computeCorrectedWeight
// --------------------------------------------------------------------
float computeCorrectedWeight(float wMeasured, float temperature) {
    float drift = (TEMP_CORR_M * temperature) + TEMP_CORR_B;
    return (wMeasured - drift - REF_WEIGHT);
}

// --------------------------------------------------------------------
//                 measureBatteryVoltage
// --------------------------------------------------------------------
float measureBatteryVoltage() {
    float raw = static_cast<float>(analogRead(PIN_VBAT));
    return raw * VBAT_SCALE_FACTOR;
}

// --------------------------------------------------------------------
//                          Loop Function
// --------------------------------------------------------------------
void loop() {
    static String dValues[10];
    static String cmd;
    static bool discardCommand = false;

    int avail = sdi.available();
    if (avail < 0) {
        sdi.clearBuffer();
        cmd = "";
        discardCommand = false;
    }
    if (avail > 0) {
        for (int i = 0; i < avail; i++) {
            char c = sdi.read();
            if (c == '!') {
                if (!discardCommand) {
                    parseSdi12Cmd(cmd, dValues);
                }
                cmd = "";
                discardCommand = false;
                sdi.clearBuffer();
                break;
            } else if (cmd.length() >= MAX_COMMAND_LENGTH) {
                discardCommand = true;
            } else if (!discardCommand) {
                cmd += c;
            }
        }
    }

    if (state == INITIATE_MEASUREMENT) {
        float measurementValues[4];
        if (pollSensor(measurementValues)) {
            formatOutputSDI(measurementValues, dValues, 35);
        } else {
            // An address-only data response reports an aborted measurement.
            for (String &value : dValues) {
                value = "";
            }
            Serial.println(F("ADS1232 did not provide a complete weight reading."));
        }
        state = WAIT;
        sdi.clearBuffer();
        sdi.forceListen();
    }
}
