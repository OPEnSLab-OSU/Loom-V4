/*
 * Title: Evaporometer Rev 02
 * By: Evan and Forrest
 */

#include <Loom_Manager.h>
#include <ADS1232_Lib.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_Hypnos/SDManager.h>
#include <algorithm>
#include <math.h>

#define ADC_SEL A1
#define DOUT A3
#define SCLK A4
#define PDWN A5
#define VREFEN MOSI

#define VBATPIN A7
#define SDI_EN

#define CHANNEL_LOW 0
#define CHANNEL_HIGH 1

// Set to 1 if the ADS1232 channel select wiring/polarity is reversed.
// This version is set to swapped because the logged traces look like the
// load cell and thermistor paths may be crossed at the ADC input or SEL pin.
#define SWAP_ADC_CHANNELS 1

#if SWAP_ADC_CHANNELS
  #define LOADCELL_CHANNEL CHANNEL_HIGH
  #define THERMISTOR_CHANNEL CHANNEL_LOW
#else
  #define LOADCELL_CHANNEL CHANNEL_LOW
  #define THERMISTOR_CHANNEL CHANNEL_HIGH
#endif

#define MAX_24_BITS 16777215
#define MAX_23_BITS 8388607
#define ADC_ZERO_COUNTS 8388608.0f

// Set calibration params here
#define OFFSET 8422094
#define SCALE 0.03167
#define COEFFICIENT -0.7221
#define BIAS 16.684

// 8423296
// 8429402

/*
 * for a calibration weight of size C measured with an
 * ADC raw output of counts, we have that
 * SCALE = C / (counts - offset)
 */

//#define OFFSET 0
//#define SCALE 1
/*
 * 3/8/2026:
 * offset 8422422
 * scale 0.0315
 */

// use only raw values
#define CALIBRATION_MODE 0

// Set how many times the ADC Reads. Nominal 50
#define NUM_ADC_READS 40

// Use SD card or sleeping
#define LOGGING_MODE 1

// Output to Serial Monitor Graph
#define DEMO_MODE 0

// Print raw channel diagnostics to Serial without changing the SD columns.
#define ADC_DEBUG_MODE 1

// ADS1232 / analog front-end settling. These are intentionally conservative
// for debugging because VREF/excitation, the thermistor divider, and channel
// select settling can all look like channel flipping when they are marginal.
#define VREF_SETTLE_MS 1000
#define ADC_CHANNEL_SETTLE_MS 250
#define ADC_DISCARD_READS 10

Manager     manager("Device", 1);
ADS1232_Lib ads(PDWN, SCLK, DOUT);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true, true);
SDManager   sd(&manager, 11);

const float resistanceLUT[] = {
  32739.8, 31109.2, 29569.5, 28115.0,
  26740.6, 25441.4, 24212.9, 23050.9,
  21951.4, 20910.8, 19925.5, 18992.3,
  18108.2, 17270.4, 16476.1, 15722.9,
  15008.5, 14330.6, 13687.1, 13076.3,
  12496.1, 11945.0, 11421.3, 10923.4,
  10450.1, 10000.0, 9571.77, 9164.26,
  8776.38, 8407.07, 8055.35, 7720.30,
  7401.03, 7096.72, 6806.60, 6529.94,
  6266.03, 6014.23, 5773.93, 5544.53,
  5325.50, 5116.30, 4916.46, 4725.49,
  4542.98, 4368.49, 4201.64, 4042.05,
  3898.38, 3743.29, 3603.46
};

const int LUT_SIZE = sizeof(resistanceLUT) / sizeof(resistanceLUT[0]);

long lastWeightCounts = 0;
long lastTempCounts = 0;

void analogFrontendOn() {
  digitalWrite(VREFEN, HIGH);
  ads.power_up();
  manager.pause(VREF_SETTLE_MS);
}

void analogFrontendOff() {
  digitalWrite(VREFEN, LOW);
}

long readCounts(uint8_t channel) {
  // channel = LOADCELL_CHANNEL -> load cell
  // channel = THERMISTOR_CHANNEL -> thermistor
  digitalWrite(ADC_SEL, channel ? HIGH : LOW);
  manager.pause(ADC_CHANNEL_SETTLE_MS);

  // Flush stale data after channel select. The first conversion after a mux
  // or excitation change can belong to the old channel or still be settling.
  ads.raw_read(ADC_DISCARD_READS);
  return ads.raw_read(NUM_ADC_READS);
}

float readWeight(uint8_t use_raw_counts = 0) {
  long weight = readCounts(LOADCELL_CHANNEL);
  lastWeightCounts = weight;

  if (!use_raw_counts && !CALIBRATION_MODE) {
    return ((weight - OFFSET) * SCALE);
  }

  return weight;
}

void printWeight() {
  analogFrontendOn();
  float weight = readWeight();
  analogFrontendOff();

  Serial.println("Weight: ");
  Serial.println(weight);
}

// r is resistance of thermistor. Nominally 10k
float temperatureLookUp(float r) {
  if (r <= 0.0f) {
    return -1;
  }

  // check every adjacent pair in the LUT
  for (uint8_t i = 0; i < LUT_SIZE - 1; i++){
    // if the resistance is within a pair
    if (r <= resistanceLUT[i] && r >= resistanceLUT[i+1]) {
      // interpolate the temperature
      float numer = resistanceLUT[i] - r;
      float denom = resistanceLUT[i] - resistanceLUT[i+1];

      // in our LUT the index is temp in degrees C
      return i + (numer / denom);
    }
  }

  // if the input was out of range return -1
  return -1;
}

float readTemperature() {
  // read thermistor channel
  long counts = readCounts(THERMISTOR_CHANNEL);
  lastTempCounts = counts;

  // The ads1232_lib output is centered near 2^23 for 0 V differential.
  // Convert centered counts back to the thermistor divider signal voltage.
  const float gain = 2.0f;
  const float v_ref = 3.01f;
  float volts = (((float)counts / ADC_ZERO_COUNTS) - 1.0f) * v_ref / (2.0f * gain);

  // transform from volts to resistance of thermistor
  const float R = 10000.0f;
  float denom = (volts / v_ref) + 0.5f;

  if (denom <= 0.0f) {
    return -1;
  }

  // v_sig = (R / (R + R_t) - 0.5) * v_ref
  // R / (v_sig / v_ref + 0.5) - R = R_t
  float resistance = (R / denom) - R;

#if ADC_DEBUG_MODE
  Serial.print("temp_counts: ");
  Serial.println(counts);
  Serial.print("temp_volts: ");
  Serial.println(volts, 6);
  Serial.print("thermistor_ohms: ");
  Serial.println(resistance, 2);
#endif

  // use the lookup table and interpolation to find temperature
  return temperatureLookUp(resistance);
}

void printTemperature() {
  analogFrontendOn();
  float temp = readTemperature();
  analogFrontendOff();

  Serial.print("Temperature: ");
  Serial.println(temp, 8);
}

float readVbat(){
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  return measuredvbat;
}

/*
 *    Function: log_date
 *    Description: Logs weight and temp to SD card
 *    Logs by default to "Device0.csv"
 *
 */
void logData(float weight, float temp, float vbat){
  //Get the current time from hypnos and save it as a c str
  char buf1[32] = {};
  DateTime now = hypnos.getCurrentTime();

  sprintf(buf1, "%02d:%02d:%02d %02d/%02d/%02d",
          now.hour(), now.minute(), now.second(),
          now.day(), now.month(), now.year());

  char buf2[32] = {};
  sprintf(buf2, "%.2f", weight);

  char buf3[32] = {};
  sprintf(buf3, "%.2f", temp);

  char buf4[32] = {};
  sprintf(buf4, "%f", vbat);

  //Combine the time and measurments strings. Include "," to separate columns
  char date_and_data[128] = {};
  strcat(date_and_data, buf1);
  strcat(date_and_data, ",");
  strcat(date_and_data, buf2);
  strcat(date_and_data,",");
  strcat(date_and_data, buf3);
  strcat(date_and_data, ",");
  strcat(date_and_data, buf4);

  sd.writeLineToFile("Device0.csv", date_and_data);
}

void isr_Trigger(){
  hypnos.wakeup();
}

void sleep(uint8_t seconds=0, uint8_t minutes = 1) {
  analogFrontendOff();
  hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 30));
  hypnos.reattachRTCInterrupt();
  hypnos.sleep();
}

void setup() {
  manager.beginSerial();
  Serial.println("Starting Setup");

  if (LOGGING_MODE) {hypnos.enable(true, false);}

  manager.initialize();

  pinMode(ADC_SEL, OUTPUT);
  pinMode(VREFEN, OUTPUT);
  digitalWrite(ADC_SEL, LOADCELL_CHANNEL ? HIGH : LOW);
  analogFrontendOff();

  // ADC library setup
  ads.set_offset(0);
  ads.set_scale(1);

  // set SD card write pins
  if (LOGGING_MODE) {
    pinMode(23, OUTPUT);
    pinMode(24, OUTPUT);
    pinMode(11, OUTPUT);
    sd.begin();
    hypnos.registerInterrupt(isr_Trigger);
  }

  Serial.println("End of Setup");
}

void loop() {
  analogFrontendOn();

  float temp = readTemperature();
  float weight = 0;

  // Removing the effect of temperature on the load cell.
  if (CALIBRATION_MODE) {
    weight = readWeight();
  }
  else {
    weight = readWeight() - ((COEFFICIENT * temp) + BIAS);
  }

  analogFrontendOff();

  float vbat = readVbat();

#if ADC_DEBUG_MODE
  Serial.print("load_channel: ");
  Serial.println(LOADCELL_CHANNEL);
  Serial.print("therm_channel: ");
  Serial.println(THERMISTOR_CHANNEL);
  Serial.print("weight_counts: ");
  Serial.println(lastWeightCounts);
#endif

  if (DEMO_MODE) {
    Serial.println(weight);
  } else {
    Serial.print("weight: ");
    Serial.println(weight);
    Serial.print("temperature: ");
    Serial.println(temp);
  }

  if (LOGGING_MODE) {
    logData(weight, temp, vbat);
    sleep();
  }
}
