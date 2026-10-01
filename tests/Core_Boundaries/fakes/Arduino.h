#pragma once
#include <stdint.h>

// Minimal SAMD21 watchdog surface for portable pause/restore tests; no board or timing model.
#define ARDUINO_ARCH_SAMD 1
struct FakeWatchdogRegisters {
    struct {
        struct {
            uint8_t ENABLE = 0;
            uint8_t WEN = 0;
        } bit;
    } CTRL;
    struct {
        struct {
            uint8_t SYNCBUSY = 0;
        } bit;
    } STATUS;
    struct {
        struct {
            uint8_t PER = 0;
            uint8_t WINDOW = 0;
        } bit;
    } CONFIG;
};
extern FakeWatchdogRegisters *WDT;
