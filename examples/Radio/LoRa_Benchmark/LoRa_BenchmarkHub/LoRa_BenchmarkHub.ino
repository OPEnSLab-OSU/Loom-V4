#include <Loom_Manager.h>

#ifndef BENCH_ENABLE_LTE
#define BENCH_ENABLE_LTE 0
#endif
#if BENCH_ENABLE_LTE
#include "arduino_secrets.h"
#endif
#define BENCH_ENABLE_SWEEP 1
#define BENCH_SD_CS_PIN 11 // Hypnos V3.3. Use 10 for Hypnos V3.2.
#include <Radio/Loom_LoRa/Benchmark/Loom_LoRaBenchmark.h>

// Role, preferred node ID (0 uses Manager), hub ID, preferred hub.
// Optional arguments: enrollment enabled, Manager name, retry limit (0–10, default 10).
constexpr bench::BootSettings Boot{bench::Role::Hub, 1, 1, 1};
constexpr uint8_t SweepHoursOnBoot = 0; // Set to 8 for automatic overnight tests on power-up.
static_assert(Boot.valid(), "Use node IDs 1-20 (0 uses Manager), hub IDs 1-16, and retries 0-10");

// Register optional sensors with bench::node::manager.

void setup() {
    bench::setup(Boot);
    if (SweepHoursOnBoot) bench::sweep::start(SweepHoursOnBoot);
}

void loop() {
    bench::loop();
}
