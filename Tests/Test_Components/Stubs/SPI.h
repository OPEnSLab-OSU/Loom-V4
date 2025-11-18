#pragma once

#define SPI_CLOCK_DIV2 2
#define SPI_MODE0 0

class SPIClass {
public:
    void begin() {}
    void transfer(unsigned char) {}
};

extern SPIClass SPI;