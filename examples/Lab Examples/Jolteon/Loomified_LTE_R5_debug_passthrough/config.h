#pragma once
#include <Arduino.h>
#if !defined(ADAFRUIT_FEATHER_M0) || !defined(__SAMD21G18A__)
#error This rewire sketch needs Loomified Feather M0, not the Jolteon J18 or Zero pin map. Use python build.py in this sketch folder.
#endif
// Jolteon_Rewire.pdf, exported 2026-10-30 09:32; Feather Arduino labels.
constexpr uint8_t RAIL3 = 5, RAIL5 = 6, RTC_INT = 12, SD_CS = 11;
constexpr uint8_t SDI_DATA = 9, SDI_DIR = 10, SENSOR12_EN = A3;
constexpr uint8_t MODEM_PWR = A5, MODEM_RESET = A4;
constexpr uint8_t MUX_ADDR_DEFAULT = 0x71, RTC_ADDR = 0x68;
constexpr uint8_t MS5803_MUX_CHANNEL = 4;
constexpr uint32_t MODEM_BAUD = 115200, MODEM_PULSE_MS = 1200;
// Build PC timezone is Pacific; the Hypnos constructor handles PST/PDT compile-time conversion.
constexpr char DEVICE_NAME[] = "Jolteon";
constexpr uint32_t DEVICE_INSTANCE = 1;
constexpr char HYPNOS_CONFIG_FILE[] = "SD_config.json";
constexpr char MQTT_CONFIG_FILE[] = "mqtt_creds.json";
constexpr uint32_t DEFAULT_SLEEP_SECONDS = 10, MAX_SLEEP_SECONDS = 86400;
constexpr uint16_t NEOPIXEL_COUNT = 1;
constexpr uint8_t NEOPIXEL_PIN = A2, NEOPIXEL_BRIGHTNESS = 255;
// Do not heartbeat D13: its schematic net "13" has no assigned load.
// Mux RESET is pull-up only; Feather EN is the power regulator shutdown input.
// No radio/nRF controls exist on this carrier.
