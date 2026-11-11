#include <Loom_Manager.h>

#ifndef BENCH_ENABLE_LTE
#define BENCH_ENABLE_LTE 0
#endif
#if BENCH_ENABLE_LTE
#include "arduino_secrets.h"
#endif
#include <Radio/Loom_LoRa/Benchmark/Loom_LoRaBenchmark.h>

// Role, preferred node ID (0 uses Manager), hub ID, preferred hub.
// Optional fifth and sixth arguments: enrollment enabled, Manager name.
constexpr bench::BootSettings Boot{bench::Role::Node, 2, 1, 1};
static_assert(Boot.valid(), "Use node IDs 1-20 (0 uses Manager) and hub IDs 1-16");

// Register optional sensors with bench::node::manager.

void setup() {
    bench::setup(Boot);
}

void loop() {
    bench::loop();
}
