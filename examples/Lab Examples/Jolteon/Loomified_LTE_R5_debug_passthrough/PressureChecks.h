#pragma once
#include <stdint.h>

// MS5803 PROM CRC4, polynomial 0x3000. The low byte of word 7 is
// excluded from the calculation; only its low nibble stores the CRC.
inline uint8_t pressurePromCrc(const uint16_t *prom) {
  uint16_t remainder=0;
  for (uint8_t i=0;i<16;++i) {
    uint16_t word=prom[i/2];
    if (i/2==7) word&=0xff00;
    remainder^=uint16_t(i&1 ? word&255 : word>>8);
    for (uint8_t bit=0;bit<8;++bit)
      remainder=uint16_t((remainder<<1)^((remainder&0x8000)?0x3000:0));
  }
  return uint8_t((remainder>>12)&15);
}
inline bool pressurePromValid(const uint16_t *prom) {
  uint8_t coefficients=0;
  for (uint8_t i=1;i<=6;++i) if (prom[i]!=0 && prom[i]!=0xffff) ++coefficients;
  return coefficients>=4 && pressurePromCrc(prom)==(prom[7]&15);
}
inline bool pressureAdcValid(uint32_t value) { return value>0 && value<0xffffff; }
