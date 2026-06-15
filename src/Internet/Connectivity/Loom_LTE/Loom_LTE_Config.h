#pragma once

// Select exactly one modem family for the Loom LTE library build.
#define LOOM_LTE_USE_SARA_R5
// #define LOOM_LTE_USE_SARA_R4

// OPEnS/Jolteon PWR_ON and RESET_N are driven through MOSFETs.
// HIGH at the Feather pin turns the MOSFET on, which pulls the SARA pin LOW.
#define LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH 1

// R5 PWR_ON low time for switch-on is 1 to 2 seconds.
// Keep this below the normal switch-off window.
#define LOOM_LTE_R5_PWR_PULSE_MS 1000UL
#define LOOM_LTE_R5_RESET_PULSE_MS 250UL

// Match the old working OPEnS path: pulse PWR_ON first, wait for the modem OS, then send AT.
#define LOOM_LTE_R5_COMPAT_POWER_FIRST 1
#define LOOM_LTE_R5_EXACT_OPENS_POWER_PATH 1
#define LOOM_LTE_R5_POST_PWR_SETTLE_MS 10000UL

// Reset recovery is off by default because the old working path never drove A4.
// Enable only after verifying A4 polarity on the board.
#define LOOM_LTE_R5_ENABLE_RESET_RECOVERY 0

// Keep SARA-R5 normal/debug UART fixed at 115200. Set to 1 only if you want
// fallback probing after 115200 fails.
#define LOOM_LTE_R5_SCAN_BAUDS_ON_FAILURE 0

// Optional: only enable these if this sketch, not the Loom manager, controls the Hypnos rails.
#define LOOM_LTE_R5_ENABLE_POWER_RAIL_PINS 0
#define LOOM_LTE_R5_3V3_RAIL_PIN 5
#define LOOM_LTE_R5_5V_RAIL_PIN 6
#define LOOM_LTE_R5_3V3_RAIL_ON_LEVEL LOW
#define LOOM_LTE_R5_5V_RAIL_ON_LEVEL HIGH

// Optional: set to "310410" for AT&T if you want to skip broad operator search.
#define LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC ""
#define LOOM_LTE_R5_FORCE_OPERATOR_ACT 7
