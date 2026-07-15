#include <Arduino.h>


#define PORT_A 0
#define PA21 PORT_A, 21


#define BLINK_PORT_PIN PA21


static inline void pinOutput(uint8_t port, uint8_t pin) {
  PORT->Group[port].DIRSET.reg = 1ul << pin;
}


static inline void pinLow(uint8_t port, uint8_t pin) {
  PORT->Group[port].OUTCLR.reg = 1ul << pin;
}


static inline void pinToggle(uint8_t port, uint8_t pin) {
  PORT->Group[port].OUTTGL.reg = 1ul << pin;
}


void setup() {
  pinOutput(BLINK_PORT_PIN);
  pinLow(BLINK_PORT_PIN);
}


void loop() {
  pinToggle(BLINK_PORT_PIN);
  delay(500);
}

