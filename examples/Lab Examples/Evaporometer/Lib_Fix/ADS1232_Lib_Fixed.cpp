/*
  ADS1232_Lib.cpp - Library for TI ADS1232
  24-Bit Analog-to-Digital Converter For Bridge Sensors
  Created by Sorin Ciorceri, 2017.
  Released into the public domain.
*/

#include "Arduino.h"
#include "ADS1232_Lib.h"

ADS1232_Lib::ADS1232_Lib(byte pdwn, byte sclk, byte dout) {
  PDWN = pdwn;
  SCLK = sclk;
  DOUT = dout;

  pinMode(PDWN, OUTPUT);
  pinMode(SCLK, OUTPUT);
  pinMode(DOUT, INPUT);

  digitalWrite(SCLK, LOW);
  digitalWrite(PDWN, LOW);
}

ADS1232_Lib::~ADS1232_Lib() {
}

bool ADS1232_Lib::is_ready() {
  return digitalRead(DOUT) == LOW;
}
  
void ADS1232_Lib::power_up() {
  digitalWrite(PDWN, HIGH);
  while (!is_ready()) {};
}

void ADS1232_Lib::power_down() {
  digitalWrite(PDWN, LOW);
}

void ADS1232_Lib::set_offset(long offset) {
  OFFSET = offset;
}

void ADS1232_Lib::set_scale(float scale) {
  SCALE = scale;
}

long ADS1232_Lib::_raw_read() {
  // From the ADS1232 datasheet: DOUT low means a conversion is ready.
  // Wait for the current ready state to clear, then wait for the next sample.
  while (is_ready()) {};
  while (!is_ready()) {};

  uint32_t raw = 0;

  // Pulse the clock pin 24 times to read the conversion result, MSB first.
  raw |= (uint32_t)shiftIn(DOUT, SCLK, MSBFIRST) << 16;
  raw |= (uint32_t)shiftIn(DOUT, SCLK, MSBFIRST) << 8;
  raw |= (uint32_t)shiftIn(DOUT, SCLK, MSBFIRST);

  // ADS1232 data are 24-bit two's complement. XORing the sign bit converts
  // the ADC code to offset-binary counts centered at 2^23, so sketches can
  // treat 8388608 counts as 0 V differential.
  long value = (long)(raw ^ 0x800000UL);
  value -= OFFSET;

  return value;
}

long ADS1232_Lib::raw_read(byte times) {
  if (times == 0) {
    return _raw_read();
  }

  int64_t sum = 0;

  for (byte i = 0; i < times; i++) {
    sum += _raw_read();
  }

  return (long)(sum / times);
}

float ADS1232_Lib::units_read(byte times) {
  return raw_read(times) / SCALE;
}
