#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
using byte = uint8_t;
#define INPUT 0
#define A7 21
#define F(text) text
uint32_t millis();
void delay(uint32_t ms);
void delayMicroseconds(unsigned int us);
void analogReadResolution(uint8_t bits);
void pinMode(int pin, int mode);
int analogRead(int pin);

#define PINS_COUNT 32
#define INPUT_PULLUP 2
#define FALLING 3
#define NOT_AN_INTERRUPT (-1)
int digitalPinToInterrupt(uint8_t pin);
void attachInterrupt(int number, void (*callback)(), int mode);
void detachInterrupt(int number);
#define LOOM_FEED_WATCHDOG() ((void)0)
