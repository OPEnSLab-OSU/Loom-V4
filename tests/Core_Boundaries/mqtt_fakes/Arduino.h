#pragma once
#include "../fakes/Arduino.h"
#include <stddef.h>
#include <string.h>
#include <algorithm>
#define F(text) text
using std::min;
uint32_t millis();
void delay(uint32_t ms);
class Stream {
  public:
    virtual ~Stream() = default;
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual size_t write(uint8_t) { return 0; }
    virtual size_t readBytes(char *buffer, size_t count) {
        size_t used = 0;
        while (used < count) {
            const int next = read();
            if (next < 0) {
                break;
            }
            buffer[used++] = static_cast<char>(next);
        }
        return used;
    }
};
