#pragma once

struct TwoWire {
    void begin() {}
    void beginTransmission(int) {}
    void endTransmission() {}
    int requestFrom(int, int) { return 0; }
    int read() { return 0; }
    void write(int) {}
};

extern TwoWire Wire;