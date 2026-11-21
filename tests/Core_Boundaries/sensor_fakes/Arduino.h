#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#if defined(LOOM_SENSOR_FAKE_SAMD)
#include "../fakes/Arduino.h"
#endif
using byte = uint8_t;
#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 1
#define MSBFIRST 1
#define A0 14
#define A1 15
#define A2 16
#define A7 21
#define F(text) text
uint32_t millis();
void delay(uint32_t ms);
void delayMicroseconds(unsigned int us);
void analogReadResolution(uint8_t bits);
void pinMode(int pin, int mode);
int analogRead(int pin);
int digitalRead(int pin);
void digitalWrite(int pin, int value);
uint8_t shiftIn(int dataPin, int clockPin, int bitOrder);

#define PINS_COUNT 32
#define INPUT_PULLUP 2
#define FALLING 3
#define NOT_AN_INTERRUPT (-1)
int digitalPinToInterrupt(uint8_t pin);
void attachInterrupt(int number, void (*callback)(), int mode);
void detachInterrupt(int number);
#if !defined(LOOM_SENSOR_FAKE_SAMD)
#define LOOM_FEED_WATCHDOG() ((void)0)
#endif
