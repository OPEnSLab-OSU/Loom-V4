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

#define BLINK_PORT_PIN PA21
#define NRF_ENABLE_PORT_PIN PA13
#define NRF_RESET_PORT_PIN PA14

// nRF P1.04 UART_TX -> SAMD PA23 RX
// nRF P1.06 UART_RX -> SAMD PA22 TX
#define NRF_UART_BAUD 115200

#define NRF_UART_SERCOM SERCOM3
#define NRF_UART_GCLK_ID SERCOM3_GCLK_ID_CORE
#define NRF_UART_PM_APBCMASK PM_APBCMASK_SERCOM3

#define NRF_UART_TX_PORT PORT_A
#define NRF_UART_TX_PIN 22
#define NRF_UART_RX_PORT PORT_A
#define NRF_UART_RX_PIN 23

#define PORT_PMUX_FUNC_C 2

static inline void pinOutput(uint8_t port, uint8_t pin) {
  PORT->Group[port].DIRSET.reg = 1ul << pin;
}

static inline void pinLow(uint8_t port, uint8_t pin) {
  PORT->Group[port].OUTCLR.reg = 1ul << pin;
}

static inline void pinHigh(uint8_t port, uint8_t pin) {
  PORT->Group[port].OUTSET.reg = 1ul << pin;
}

static inline void pinToggle(uint8_t port, uint8_t pin) {
  PORT->Group[port].OUTTGL.reg = 1ul << pin;
}

static inline void pinMux(uint8_t port, uint8_t pin, uint8_t function) {
  PORT->Group[port].PINCFG[pin].bit.PMUXEN = 1;

  if (pin & 1) {
    PORT->Group[port].PMUX[pin >> 1].bit.PMUXO = function;
  } else {
    PORT->Group[port].PMUX[pin >> 1].bit.PMUXE = function;
  }
}

static void enableNrf() {
  pinOutput(NRF_ENABLE_PORT_PIN);
  pinHigh(NRF_ENABLE_PORT_PIN);

  pinOutput(NRF_RESET_PORT_PIN);
  pinHigh(NRF_RESET_PORT_PIN);
}

static void nrfUartWaitSync() {
  while (NRF_UART_SERCOM->USART.SYNCBUSY.reg) {
  }
}

static void nrfUartBegin(uint32_t baud) {
  PM->APBCMASK.reg |= NRF_UART_PM_APBCMASK;

  GCLK->CLKCTRL.reg =
    GCLK_CLKCTRL_ID(NRF_UART_GCLK_ID) |
    GCLK_CLKCTRL_GEN_GCLK0 |
    GCLK_CLKCTRL_CLKEN;

  while (GCLK->STATUS.bit.SYNCBUSY) {
  }

  pinMux(NRF_UART_TX_PORT, NRF_UART_TX_PIN, PORT_PMUX_FUNC_C);
  pinMux(NRF_UART_RX_PORT, NRF_UART_RX_PIN, PORT_PMUX_FUNC_C);

  NRF_UART_SERCOM->USART.CTRLA.bit.ENABLE = 0;
  nrfUartWaitSync();

  NRF_UART_SERCOM->USART.CTRLA.bit.SWRST = 1;
  nrfUartWaitSync();

  NRF_UART_SERCOM->USART.CTRLA.reg =
    SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
    SERCOM_USART_CTRLA_DORD |
    SERCOM_USART_CTRLA_RXPO(1) |
    SERCOM_USART_CTRLA_TXPO(0);

  NRF_UART_SERCOM->USART.CTRLB.reg =
    SERCOM_USART_CTRLB_RXEN |
    SERCOM_USART_CTRLB_TXEN |
    SERCOM_USART_CTRLB_CHSIZE(0);

  nrfUartWaitSync();

  NRF_UART_SERCOM->USART.BAUD.reg =
    (uint16_t)(65536ul - ((uint64_t)65536ul * 16ul * baud) / SystemCoreClock);

  NRF_UART_SERCOM->USART.CTRLA.bit.ENABLE = 1;
  nrfUartWaitSync();
}

static void nrfUartWriteByte(uint8_t value) {
  while (!NRF_UART_SERCOM->USART.INTFLAG.bit.DRE) {
  }

  NRF_UART_SERCOM->USART.DATA.reg = value;
}

static void nrfUartWrite(const char *text) {
  while (*text) {
    nrfUartWriteByte((uint8_t)*text++);
  }
}

static void nrfUartWriteLine(const char *text) {
  nrfUartWrite(text);
  nrfUartWrite("\r\n");
}

static bool nrfUartAvailable() {
  return NRF_UART_SERCOM->USART.INTFLAG.bit.RXC;
}

static int nrfUartRead() {
  if (!nrfUartAvailable()) {
    return -1;
  }

  return NRF_UART_SERCOM->USART.DATA.reg & 0xff;
}

static void serviceNrfUartRx() {
  while (nrfUartAvailable()) {
    int c = nrfUartRead();

    if (c >= 0 && Serial) {
      Serial.write((uint8_t)c);
    }
  }
}

void setup() {
  pinOutput(BLINK_PORT_PIN);
  pinLow(BLINK_PORT_PIN);

  enableNrf();
  delay(500);

  Serial.begin(115200);
  nrfUartBegin(NRF_UART_BAUD);

  delay(250);

  nrfUartWriteLine("tx ble j18_uart_ready");
}

void loop() {
  pinToggle(BLINK_PORT_PIN);

  nrfUartWriteLine("tx ble j18_blink");

  unsigned long startMs = millis();
  while (millis() - startMs < 500) {
    serviceNrfUartRx();
    delay(1);
  }
}

