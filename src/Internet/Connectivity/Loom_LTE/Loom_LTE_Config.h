#pragma once

// Select exactly one modem family for the Loom LTE library build.
#define LOOM_LTE_USE_SARA_R5
// #define LOOM_LTE_USE_SARA_R4

// OPEnS/Jolteon PWR_ON and RESET_N are driven through MOSFETs.
// HIGH at the Feather pin turns the MOSFET on, which pulls the SARA pin LOW.
#define LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH 1

// R5 PWR_ON low time for switch-on is 1 to 2 seconds.
// Keep this below the normal switch-off window.
#define LOOM_LTE_R5_PWR_PULSE_MS 1100UL
#define LOOM_LTE_R5_RESET_PULSE_MS 250UL

// Optional: only enable these if this sketch, not the Loom manager, controls the Hypnos rails.
#define LOOM_LTE_R5_ENABLE_POWER_RAIL_PINS 0
#define LOOM_LTE_R5_3V3_RAIL_PIN 5
#define LOOM_LTE_R5_5V_RAIL_PIN 6
#define LOOM_LTE_R5_3V3_RAIL_ON_LEVEL LOW
#define LOOM_LTE_R5_5V_RAIL_ON_LEVEL HIGH

// Optional: set to "310410" for AT&T if you want to skip broad operator search.
#define LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC ""
#define LOOM_LTE_R5_FORCE_OPERATOR_ACT 7

// Primary UART baud for SARA-R5. The host must choose a baud before it can send AT.
#define LOOM_LTE_R5_UART_BAUD 115200UL

// Maximum time to wait for AT after the OPEnS-compatible power pulse and boot settle.
#define LOOM_LTE_R5_BOOT_AT_TIMEOUT_MS 45000UL
