#include <cassert>
#include <limits>
#include "Utilities/Loom_RechargePolicy.h"

float volts = 4.2f;
int reads = 0;
float readVolts() {
    ++reads;
    return volts;
}
int main() {
    loomPower::RechargePolicy policy;
    assert(!policy.isConfigured() && !policy.shouldRecharge(0.0f));
    assert(!policy.configure(4.2f, 3.7f));
    assert(!policy.configure(3.7f, 3.7f));
    assert(!policy.configure(-1.0f, 4.2f));
    assert(!policy.configure(3.7f, std::numeric_limits<float>::infinity()));
    assert(policy.configure(3.7f, 4.2f));
    assert(!policy.shouldRecharge(4.0f));
    assert(policy.shouldRecharge(3.7f) && policy.isCharging());
    assert(policy.shouldRecharge(3.9f)); // Removing load is not the same as recharging.
    assert(policy.shouldRecharge(4.199f));
    assert(!policy.shouldRecharge(4.2f) && !policy.isCharging());
    assert(policy.shouldRecharge(std::numeric_limits<float>::quiet_NaN()));
    assert(policy.shouldRecharge(0.0f));
    assert(policy.shouldRecharge(-1.0f));
    assert(!policy.shouldRecharge(4.2f));
    assert(!policy.configure(5.0f, 4.0f)); // A rejected reconfiguration retains the prior settings.
    assert(policy.shouldRecharge(3.7f));

    assert(loomPower::allowModuleWake(true, nullptr, nullptr));
    assert(!loomPower::allowModuleWake(false, &policy, readVolts) && reads == 0);
    assert(!loomPower::allowModuleWake(true, &policy, nullptr));
    volts = 4.1f;
    assert(!loomPower::allowModuleWake(true, &policy, readVolts) && reads == 1);
    volts = 4.2f;
    assert(loomPower::allowModuleWake(true, &policy, readVolts) && reads == 2);
    volts = 3.7f;
    assert(!loomPower::allowModuleWake(true, &policy, readVolts) && reads == 3);
}
