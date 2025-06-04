#pragma once

#include <Arduino.h>
#include <Loom_Manager.h>

enum class magnetStatus
{
    red,
    green,
    yellow,
    error
};

class AS5311
{
public:
    AS5311(uint8_t cs_pin, uint8_t clk_pin, uint8_t do_pin);
    magnetStatus getMagnetStatus();
    uint16_t getFilteredPosition();
    uint16_t getFieldStrength();
    uint32_t getRawData();
    
    void measure(Manager &);
    float measureDisplacement(int);

private:
    const uint8_t CS_PIN;
    const uint8_t CLK_PIN;
    const uint8_t DO_PIN;

    static const int DATA_TIMING_US;
    static const int AVERAGE_MEASUREMENTS;

    int initialPosition = -1; //negative number indicates that initial position has not been measured
    int lastPosition = 0;
    int overflows = 0;

    void recordMagnetStatus(Manager &);

    void initializePins();
    void deinitializePins();
    uint16_t getPosition();
    uint32_t bitbang(bool);
};

// bit definitions - See pages 12 and 13 of the AS5311 datasheet for more information
#define PAR 0
#define MAGDEC 1
#define MAGINC 2
#define LIN 3
#define COF 4
#define OCF 5

#define DATAOFFSET 6
