#pragma once
#include <stdint.h>
constexpr uint8_t HIGH = 1, LOW = 0, INPUT = 0, OUTPUT = 1;
void pinMode(uint8_t, uint8_t);
void digitalWrite(uint8_t, uint8_t);
int digitalRead(uint8_t);
void delayMicroseconds(unsigned int);
