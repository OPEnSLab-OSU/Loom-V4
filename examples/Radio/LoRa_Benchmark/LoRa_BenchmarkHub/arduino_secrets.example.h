#pragma once

// Copy this file to arduino_secrets.h and enter your modem and broker settings.
#define BENCH_APN "your-apn"
#define BENCH_APN_USER ""
#define BENCH_APN_PASS ""
#define BENCH_BROKER "your-broker"
#define BENCH_PORT 1883
#define BENCH_DATABASE "lora-bench"
#define BENCH_MQTT_USER ""
#define BENCH_MQTT_PASS ""

#define BENCH_LTE_BOARD OPENS
#define BENCH_LTE_MODEM LTE_MODEM::SARA_R5
#define BENCH_LTE_SUPPLY_PIN 6 // Jolteon R5 example. Use -1 if the modem is separately powered.
