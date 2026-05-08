/*
 * Title: Evaporometer Rev 02
 * Author: Evan Hockert
 */

#include <Loom_Manager.h>
#include <ADS1232_Lib.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_Hypnos/SDManager.h>
#include <algorithm>

#define ADC_SEL A1
#define DOUT A3
#define SCLK A4
#define PDWN A5
#define VREFEN MOSI

#define VBATPIN A7
#define SDI_EN

#define LOADCELL_CHANNEL 0
#define THERMISTOR_CHANNEL 1

#define MAX_24_BITS 16777215
#define MAX_23_BITS 8388607

// Set calibration params here
#define OFFSET 8402647
#define SCALE 0.032

// 8423296
// 8429402

/*
 * for a calibration weight of size C measured with an 
 * ADC raw output of counts, we have that
 * SCALE = (counts - offset) / C
 */

//#define OFFSET 0
//#define SCALE 1
/*
 * 3/8/2026: 
 * offset 8422422
 * scale 0.0315
 */

// use only raw values
#define CALIBRATION_MODE 1

// Set how many times the ADC Reads. Nominal 100
#define NUM_ADC_READS 40

// Use SD card or sleeping
#define LOGGING_MODE 0

// Output to Serial Monitor Graph
#define DEMO_MODE 0

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

long readCounts(uint8_t channel) {
  // channel = 0 -> Load Cell
  // channel = 1 -> Thermistor   
  digitalWrite(ADC_SEL, channel);
  /* read counts from the adc. this represents the weight
  as an integer between 0 and 2^24 - 1 */
  ads.power_up();
  
  // empty the output buffer
  ads.raw_read(10);
  long counts = ads.raw_read(NUM_ADC_READS);

  return counts;
}

float readWeight(uint8_t use_raw_counts = 0) {
  // turn on the bridge excitation
  digitalWrite(VREFEN, 1);
  manager.pause(100);
  long weight = readCounts(LOADCELL_CHANNEL);
  digitalWrite(VREFEN, 0);

  if (!use_raw_counts && !CALIBRATION_MODE) {
    float calibrated_weight = (weight - OFFSET) * SCALE;
    return calibrated_weight;
  }
  else {
    return weight;
  }
}

void printWeight() {
  float weight = readWeight();
  Serial.println("Weight: ");
  Serial.println(weight);
}

// r is resistance of thermistor. Nominally 10k
float temperatureLookUp(float r) {
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
  // turn on the bridge excitation
  digitalWrite(VREFEN, 1);
  manager.pause(100);
  // read thermistor channel
  long counts = readCounts(THERMISTOR_CHANNEL);
  digitalWrite(VREFEN, 1);
  //Serial.print("counts: ");
  //Serial.println(counts);
  // transform from counts to volts
  float nom = 3 * (counts - MAX_23_BITS);
  float volts = nom / (MAX_24_BITS);
  
  //Serial.print("volts: ");
  //erial.println(volts, 12);
  // transform from volts to resistance of thermistor
  int R = 10000;
  float denom =  (volts / 3) + 0.5; 
  float resistance = (R / denom) - R;
  //Serial.print("resistance: ");
  //Serial.println(resistance);
  // use the lookup table and interpolation to find temperature
  float temperature = temperatureLookUp(resistance);
  return temperature;
}

void printTemperature() {
  float temp = readTemperature();
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
  digitalWrite(VREFEN, 0);
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

  float temp = 0;
  float vbat = 0;

  float weight = readWeight();
  temp = readTemperature();
  vbat = readVbat();


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
