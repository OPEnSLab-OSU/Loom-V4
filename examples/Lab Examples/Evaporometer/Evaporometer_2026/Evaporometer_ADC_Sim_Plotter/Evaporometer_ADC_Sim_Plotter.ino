/*
 * Evaporometer ADS1232 decoder simulator
 *
 * This sketch does not use Loom, Hypnos, SD, or the real ADS1232 pins.
 * It reverse-calculates fake ADS1232 24-bit words from expected load-cell
 * and thermistor values, feeds those words through the fixed decoder and
 * the original buggy decoder, then prints values for Arduino Serial Plotter.
 *
 * Use this to show why the old decoder can look fine near load-cell zero
 * while breaking badly on the thermistor channel.
 */

#include <Arduino.h>
#include <math.h>

#define SERIAL_BAUD 115200

#define ADC_ZERO_COUNTS 8388608.0f
#define ADC_FULL_COUNTS 16777215UL

#define VREF 3.01f
#define ADS_GAIN 2.0f
#define THERM_PULLUP_OHMS 10000.0f

#define LOAD_OFFSET_COUNTS 8422094L
#define LOAD_SCALE 0.03167f

#define INVALID_TEMP_PLOT -20.0f
#define WEIGHT_PLOT_DIVISOR 20.0f

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

const uint8_t LUT_SIZE = sizeof(resistanceLUT) / sizeof(resistanceLUT[0]);

float clampFloat(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

uint32_t clampCountsFloat(float counts) {
  if (counts < 0.0f) return 0UL;
  if (counts > (float)ADC_FULL_COUNTS) return ADC_FULL_COUNTS;
  return (uint32_t)(counts + 0.5f);
}

float temperatureLookUp(float r) {
  if (r <= 0.0f) {
    return -1.0f;
  }

  for (uint8_t i = 0; i < LUT_SIZE - 1; i++) {
    if (r <= resistanceLUT[i] && r >= resistanceLUT[i + 1]) {
      float numer = resistanceLUT[i] - r;
      float denom = resistanceLUT[i] - resistanceLUT[i + 1];
      return i + (numer / denom);
    }
  }

  return -1.0f;
}

float resistanceFromTemperature(float tempC) {
  tempC = clampFloat(tempC, 0.0f, (float)(LUT_SIZE - 1));

  uint8_t lo = (uint8_t)floor(tempC);
  uint8_t hi = lo + 1;

  if (hi >= LUT_SIZE) {
    return resistanceLUT[LUT_SIZE - 1];
  }

  float t = tempC - lo;
  return resistanceLUT[lo] + ((resistanceLUT[hi] - resistanceLUT[lo]) * t);
}

float thermistorVoltsFromResistance(float thermR) {
  return ((THERM_PULLUP_OHMS / (THERM_PULLUP_OHMS + thermR)) - 0.5f) * VREF;
}

uint32_t offsetCountsFromVolts(float volts) {
  float counts = ((volts * (2.0f * ADS_GAIN) / VREF) + 1.0f) * ADC_ZERO_COUNTS;
  return clampCountsFloat(counts);
}

uint32_t rawBitsFromOffsetCounts(uint32_t counts) {
  return counts ^ 0x800000UL;
}

uint32_t offsetCountsFromRawBitsFixed(uint32_t raw) {
  return raw ^ 0x800000UL;
}

uint32_t offsetCountsFromRawBitsOldBug(uint32_t raw) {
  uint8_t msb = (raw >> 16) & 0xFF;
  uint8_t mid = (raw >> 8) & 0xFF;
  uint8_t lsb = raw & 0xFF;
  uint32_t toAdd = 0;

  // Original library bug: B1000000 is bit 6, not the 24-bit sign bit.
  // This folds large parts of the thermistor range into the wrong counts.
  if (msb & B1000000) {
    msb &= B01111111;
    toAdd = 0x0UL;
  }
  else {
    toAdd = 0x7FFFFFUL;
  }

  return (((uint32_t)msb << 16) | ((uint32_t)mid << 8) | lsb) + toAdd;
}

float voltsFromOffsetCounts(uint32_t counts) {
  return ((((float)counts / ADC_ZERO_COUNTS) - 1.0f) * VREF) / (2.0f * ADS_GAIN);
}

float thermistorResistanceFromCounts(uint32_t counts) {
  float volts = voltsFromOffsetCounts(counts);
  float denom = (volts / VREF) + 0.5f;

  if (denom <= 0.0f) {
    return -1.0f;
  }

  return (THERM_PULLUP_OHMS / denom) - THERM_PULLUP_OHMS;
}

float temperatureFromOffsetCounts(uint32_t counts) {
  float resistance = thermistorResistanceFromCounts(counts);
  return temperatureLookUp(resistance);
}

uint32_t offsetCountsFromTemperature(float tempC) {
  float thermR = resistanceFromTemperature(tempC);
  float volts = thermistorVoltsFromResistance(thermR);
  return offsetCountsFromVolts(volts);
}

uint32_t offsetCountsFromWeight(float grams) {
  float counts = (float)LOAD_OFFSET_COUNTS + (grams / LOAD_SCALE);
  return clampCountsFloat(counts);
}

float weightFromOffsetCounts(uint32_t counts) {
  return (((float)counts - (float)LOAD_OFFSET_COUNTS) * LOAD_SCALE);
}

void printPlotRow(float inputTempC, float fixedTempC, float oldTempC, float inputWeight, float fixedWeight, float oldWeight, float roomCountTempC) {
  if (oldTempC < 0.0f) oldTempC = INVALID_TEMP_PLOT;
  if (fixedTempC < 0.0f) fixedTempC = INVALID_TEMP_PLOT;
  if (roomCountTempC < 0.0f) roomCountTempC = INVALID_TEMP_PLOT;

  Serial.print("targetC:");
  Serial.print(inputTempC, 2);
  Serial.print(",fixedC:");
  Serial.print(fixedTempC, 2);
  Serial.print(",oldBugC:");
  Serial.print(oldTempC, 2);
  Serial.print(",targetG_div20:");
  Serial.print(inputWeight / WEIGHT_PLOT_DIVISOR, 2);
  Serial.print(",fixedG_div20:");
  Serial.print(fixedWeight / WEIGHT_PLOT_DIVISOR, 2);
  Serial.print(",oldBugG_div20:");
  Serial.print(oldWeight / WEIGHT_PLOT_DIVISOR, 2);
  Serial.print(",roomLogC:");
  Serial.println(roomCountTempC, 2);
}

void printEdgeCaseHeader() {
  Serial.println();
  Serial.println("Edge case table for Serial Monitor:");
  Serial.println("kind,input,raw_hex,fixed_counts,old_counts,fixed_decoded,old_decoded");
}

void printHex24(uint32_t raw) {
  Serial.print("0x");
  if (raw < 0x100000UL) Serial.print("0");
  if (raw < 0x10000UL) Serial.print("0");
  if (raw < 0x1000UL) Serial.print("0");
  if (raw < 0x100UL) Serial.print("0");
  if (raw < 0x10UL) Serial.print("0");
  Serial.print(raw, HEX);
}

void printTempEdge(float tempC) {
  uint32_t expectedCounts = offsetCountsFromTemperature(tempC);
  uint32_t raw = rawBitsFromOffsetCounts(expectedCounts);
  uint32_t fixedCounts = offsetCountsFromRawBitsFixed(raw);
  uint32_t oldCounts = offsetCountsFromRawBitsOldBug(raw);
  float fixedTemp = temperatureFromOffsetCounts(fixedCounts);
  float oldTemp = temperatureFromOffsetCounts(oldCounts);

  Serial.print("thermistor_C,");
  Serial.print(tempC, 2);
  Serial.print(",");
  printHex24(raw);
  Serial.print(",");
  Serial.print(fixedCounts);
  Serial.print(",");
  Serial.print(oldCounts);
  Serial.print(",");
  Serial.print(fixedTemp, 2);
  Serial.print(",");
  Serial.println(oldTemp, 2);
}

void printWeightEdge(float grams) {
  uint32_t expectedCounts = offsetCountsFromWeight(grams);
  uint32_t raw = rawBitsFromOffsetCounts(expectedCounts);
  uint32_t fixedCounts = offsetCountsFromRawBitsFixed(raw);
  uint32_t oldCounts = offsetCountsFromRawBitsOldBug(raw);
  float fixedWeight = weightFromOffsetCounts(fixedCounts);
  float oldWeight = weightFromOffsetCounts(oldCounts);

  Serial.print("loadcell_g,");
  Serial.print(grams, 2);
  Serial.print(",");
  printHex24(raw);
  Serial.print(",");
  Serial.print(fixedCounts);
  Serial.print(",");
  Serial.print(oldCounts);
  Serial.print(",");
  Serial.print(fixedWeight, 2);
  Serial.print(",");
  Serial.println(oldWeight, 2);
}

void printRoomLogCheck() {
  const uint32_t roomCounts = 9878944UL;
  float volts = voltsFromOffsetCounts(roomCounts);
  float resistance = thermistorResistanceFromCounts(roomCounts);
  float tempC = temperatureFromOffsetCounts(roomCounts);

  Serial.println();
  Serial.println("Room log check:");
  Serial.print("counts,");
  Serial.println(roomCounts);
  Serial.print("volts,");
  Serial.println(volts, 6);
  Serial.print("resistance_ohms,");
  Serial.println(resistance, 2);
  Serial.print("temperature_C,");
  Serial.println(tempC, 2);
}

void printEdgeCases() {
  printRoomLogCheck();
  printEdgeCaseHeader();

  printTempEdge(0.0f);
  printTempEdge(1.0f);
  printTempEdge(2.0f);
  printTempEdge(5.0f);
  printTempEdge(25.0f);
  printTempEdge(35.0f);
  printTempEdge(40.0f);
  printTempEdge(45.0f);
  printTempEdge(50.0f);

  printWeightEdge(-200.0f);
  printWeightEdge(0.0f);
  printWeightEdge(100.0f);
  printWeightEdge(500.0f);
  printWeightEdge(1000.0f);

  Serial.println();
  Serial.println("Open Arduino Serial Plotter now, or leave it running. Plot rows start next.");
  delay(2500);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 3000) {}

  printEdgeCases();
}

void loop() {
  static float tempC = 0.0f;
  static float weightG = -200.0f;
  static int direction = 1;

  uint32_t tempCounts = offsetCountsFromTemperature(tempC);
  uint32_t tempRaw = rawBitsFromOffsetCounts(tempCounts);
  uint32_t fixedTempCounts = offsetCountsFromRawBitsFixed(tempRaw);
  uint32_t oldTempCounts = offsetCountsFromRawBitsOldBug(tempRaw);
  float fixedTemp = temperatureFromOffsetCounts(fixedTempCounts);
  float oldTemp = temperatureFromOffsetCounts(oldTempCounts);

  uint32_t weightCounts = offsetCountsFromWeight(weightG);
  uint32_t weightRaw = rawBitsFromOffsetCounts(weightCounts);
  uint32_t fixedWeightCounts = offsetCountsFromRawBitsFixed(weightRaw);
  uint32_t oldWeightCounts = offsetCountsFromRawBitsOldBug(weightRaw);
  float fixedWeight = weightFromOffsetCounts(fixedWeightCounts);
  float oldWeight = weightFromOffsetCounts(oldWeightCounts);

  float roomLogTemp = temperatureFromOffsetCounts(9878944UL);

  printPlotRow(tempC, fixedTemp, oldTemp, weightG, fixedWeight, oldWeight, roomLogTemp);

  tempC += direction * 0.5f;
  weightG += direction * 15.0f;

  if (tempC >= 50.0f) {
    tempC = 50.0f;
    weightG = 1000.0f;
    direction = -1;
  }
  else if (tempC <= 0.0f) {
    tempC = 0.0f;
    weightG = -200.0f;
    direction = 1;
  }

  delay(100);
}
