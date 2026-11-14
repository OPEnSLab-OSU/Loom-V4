#pragma once
#include "config.h"
// Open-drain software I2C with a 2.5 ms clock-stretch timeout. No HIGH drives.
// This keeps a stuck mux branch from trapping a core Wire polling loop.
class BenchI2c {
  void low(uint8_t p) { digitalWrite(p,LOW); pinMode(p,OUTPUT); }
  void release(uint8_t p) { pinMode(p,INPUT); }
  bool high() {
    release(SCL); uint32_t t=micros();
    while (!digitalRead(SCL)) if (uint32_t(micros()-t)>2500) { fault=true; return false; }
    delayMicroseconds(5); return true;
  }
  bool start() {
    release(SDA); if (!high() || !digitalRead(SDA)) { fault=true; return false; }
    low(SDA); delayMicroseconds(5); low(SCL); return true;
  }
  void stop() { low(SDA); high(); release(SDA); delayMicroseconds(5); }
  bool put(uint8_t v) {
    for (uint8_t b=128;b;b>>=1) { if (v&b) release(SDA); else low(SDA);
      delayMicroseconds(5); if (!high()) return false; low(SCL); }
    release(SDA); delayMicroseconds(5); if (!high()) return false;
    bool ack=!digitalRead(SDA); low(SCL); return ack;
  }
  uint8_t get(bool ack) {
    uint8_t v=0; release(SDA);
    for (uint8_t i=0;i<8;++i) { delayMicroseconds(5); if (!high()) return 255;
      v=uint8_t((v<<1)|digitalRead(SDA)); low(SCL); }
    if (ack) low(SDA); else release(SDA); delayMicroseconds(5);
    high(); low(SCL); release(SDA); return v;
  }
public:
  bool fault=false;
  bool transfer(uint8_t addr,const uint8_t *tx,size_t nt,uint8_t *rx,size_t nr) {
    fault=false; bool ok=addr>=8 && addr<=0x77 && start();
    if (ok && (nt || !nr)) { ok=put(addr<<1); for (size_t i=0;ok && i<nt;++i) ok=put(tx[i]); }
    if (ok && nr) { if (nt) ok=start(); if (ok) ok=put(uint8_t((addr<<1)|1));
      for (size_t i=0;ok && i<nr;++i) { rx[i]=get(i+1<nr); ok=!fault; } }
    stop(); return ok && !fault;
  }
  bool recover() {
    fault=false; release(SDA); release(SCL);
    for (uint8_t i=0;i<9 && !digitalRead(SDA);++i) { low(SCL); delayMicroseconds(5); if (!high()) break; }
    stop(); return !fault && digitalRead(SDA) && digitalRead(SCL);
  }
};
