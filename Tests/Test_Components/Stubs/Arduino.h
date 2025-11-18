#pragma once

#include <cstdint> // Use standard C++ types if available

#define PSTR(s) s
inline int snprintf_P(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, size, format, args);
    va_end(args);
    return ret;
}

typedef bool boolean; // Arduino typedefs not in std

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1

struct SerialClass {
    void begin(int) {}
    void println(const char*) {}
    void println(int) {}
    void print(const char*) {}
    void print(int) {}
};
extern SerialClass Serial;