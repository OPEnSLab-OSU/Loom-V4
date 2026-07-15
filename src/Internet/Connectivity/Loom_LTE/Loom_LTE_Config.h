#pragma once

// Select exactly one modem family for the complete Loom library build. This
// 4.9-compatible builds default to R4. Select R5 with a project-wide build
// flag or by changing this library configuration before compiling.
// A define placed only in the .ino will not reach Loom_LTE.cpp.
#if !defined(LOOM_LTE_USE_SARA_R4) && !defined(LOOM_LTE_USE_SARA_R5)
#define LOOM_LTE_USE_SARA_R4
#endif

#if defined(LOOM_LTE_USE_SARA_R4) && defined(LOOM_LTE_USE_SARA_R5)
#error "Select only one Loom LTE modem profile"
#endif

// OPEnS/Jolteon PWR_ON and RESET_N are driven through MOSFETs.
// HIGH at the Feather pin turns the MOSFET on, which pulls the SARA pin LOW.
#ifndef LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH
#define LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH 1
#endif

// R5 PWR_ON low time for switch-on is 1 to 2 seconds.
// Keep this below the normal switch-off window.
#ifndef LOOM_LTE_R5_PWR_PULSE_MS
#define LOOM_LTE_R5_PWR_PULSE_MS 1200UL
#endif
#ifndef LOOM_LTE_R5_RESET_PULSE_MS
#define LOOM_LTE_R5_RESET_PULSE_MS 250UL
#endif

// Optional: only enable these if this sketch, not the Loom manager, controls the Hypnos rails.
#ifndef LOOM_LTE_R5_ENABLE_POWER_RAIL_PINS
#define LOOM_LTE_R5_ENABLE_POWER_RAIL_PINS 0
#endif
#ifndef LOOM_LTE_R5_3V3_RAIL_PIN
#define LOOM_LTE_R5_3V3_RAIL_PIN 5
#endif
#ifndef LOOM_LTE_R5_5V_RAIL_PIN
#define LOOM_LTE_R5_5V_RAIL_PIN 6
#endif
#ifndef LOOM_LTE_R5_3V3_RAIL_ON_LEVEL
#define LOOM_LTE_R5_3V3_RAIL_ON_LEVEL LOW
#endif
#ifndef LOOM_LTE_R5_5V_RAIL_ON_LEVEL
#define LOOM_LTE_R5_5V_RAIL_ON_LEVEL HIGH
#endif

// Optional: set to "310410" for AT&T if you want to skip broad operator search.
#ifndef LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC
#define LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC ""
#endif
#ifndef LOOM_LTE_R5_FORCE_OPERATOR_ACT
#define LOOM_LTE_R5_FORCE_OPERATOR_ACT 7
#endif

// Primary UART baud for SARA-R5. The host must choose a baud before it can send AT.
#ifndef LOOM_LTE_R5_UART_BAUD
#define LOOM_LTE_R5_UART_BAUD 115200UL
#endif

// Maximum time to wait for AT after the OPEnS-compatible power pulse and boot settle.
#ifndef LOOM_LTE_R5_BOOT_AT_TIMEOUT_MS
#define LOOM_LTE_R5_BOOT_AT_TIMEOUT_MS 45000UL
#endif
