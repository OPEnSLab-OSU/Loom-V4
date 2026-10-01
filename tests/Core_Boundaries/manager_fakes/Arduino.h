#pragma once
#include "../fakes/Arduino.h"
#include <cstdio>
#include <cstring>

// Manager tests exercise lifecycle dispatch, not USB or the board's serial-number registers.
class __FlashStringHelper;
#define F(text) text
#define PSTR(text) text
#define snprintf_P snprintf

#define PM_RCAUSE_POR 1
#define PM_RCAUSE_BOD12 2
#define PM_RCAUSE_BOD33 4
#define PM_RCAUSE_EXT 16
#define PM_RCAUSE_WDT 32
#define PM_RCAUSE_SYST 64

class FakeSerial {
  public:
    void begin(unsigned long) {}
    explicit operator bool() const { return true; }
    template <typename T> void print(const T &) {}
    template <typename T> void println(const T &) {}
};
extern FakeSerial Serial;
uint32_t millis();
