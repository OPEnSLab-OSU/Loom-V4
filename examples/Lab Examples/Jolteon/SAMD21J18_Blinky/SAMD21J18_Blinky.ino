#include <Arduino.h>

#define PORT_A 0
#define PORT_B 1

#define PA00 PORT_A, 0
#define PA01 PORT_A, 1
#define PA02 PORT_A, 2
#define PA03 PORT_A, 3
#define PA04 PORT_A, 4
#define PA05 PORT_A, 5
#define PA06 PORT_A, 6
#define PA07 PORT_A, 7
#define PA08 PORT_A, 8
#define PA09 PORT_A, 9
#define PA10 PORT_A, 10
#define PA11 PORT_A, 11
#define PA12 PORT_A, 12
#define PA13 PORT_A, 13
#define PA14 PORT_A, 14
#define PA15 PORT_A, 15
#define PA16 PORT_A, 16
#define PA17 PORT_A, 17
#define PA18 PORT_A, 18
#define PA19 PORT_A, 19
#define PA20 PORT_A, 20
#define PA21 PORT_A, 21
#define PA22 PORT_A, 22
#define PA23 PORT_A, 23
#define PA24 PORT_A, 24
#define PA25 PORT_A, 25
#define PA27 PORT_A, 27
#define PA28 PORT_A, 28
#define PA30 PORT_A, 30
#define PA31 PORT_A, 31

#define PB00 PORT_B, 0
#define PB01 PORT_B, 1
#define PB02 PORT_B, 2
#define PB03 PORT_B, 3
#define PB04 PORT_B, 4
#define PB05 PORT_B, 5
#define PB06 PORT_B, 6
#define PB07 PORT_B, 7
#define PB08 PORT_B, 8
#define PB09 PORT_B, 9
#define PB10 PORT_B, 10
#define PB11 PORT_B, 11
#define PB12 PORT_B, 12
#define PB13 PORT_B, 13
#define PB14 PORT_B, 14
#define PB15 PORT_B, 15
#define PB16 PORT_B, 16
#define PB17 PORT_B, 17
#define PB22 PORT_B, 22
#define PB23 PORT_B, 23
#define PB30 PORT_B, 30
#define PB31 PORT_B, 31

#define BLINK_PORT_PIN PA15

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
