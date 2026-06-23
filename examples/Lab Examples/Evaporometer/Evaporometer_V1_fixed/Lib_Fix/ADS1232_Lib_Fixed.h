/*
  ADS1232_Lib.h - Library for TI ADS1232
  24-Bit Analog-to-Digital Converter For Bridge Sensors
  Created by Sorin Ciorceri, 2017.
  Released into the public domain.
*/

#ifndef ADS1232_LIB_h
#define ADS1232_LIB_h

#include "Arduino.h"

class ADS1232_Lib
{
  public:
    byte PDWN;         // power down pin
    byte SCLK;         // serial clock pin
    byte DOUT;         // serial data out pin
    long OFFSET = 0;   // used for tare
    float SCALE = 1.0; // used to scale weight to grams/kg/ounces

    ADS1232_Lib(byte pdwn, byte sclk, byte dout);
    ~ADS1232_Lib();
    bool is_ready();
    void power_up();
    void power_down();
    void set_offset(long offset = 0);
    void set_scale(float scale = 1.0f);
    long _raw_read();
    long raw_read(byte times = 1);
    float units_read(byte times = 1);
};

#endif
