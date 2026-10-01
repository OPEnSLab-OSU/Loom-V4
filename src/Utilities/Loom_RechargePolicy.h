#pragma once
#include <cmath>

namespace loomPower {
////////////////////////////////////////////////////////////////////////////////////////////////////
// A low reading starts charging; voltage must reach the higher threshold before work resumes.
// The gap prevents repeatedly starting LTE when the battery briefly rebounds after shutdown.
// This decides policy only. The sketch/Hypnos controls rails, shutdown and the next wake alarm.
class RechargePolicy {
  public:
    bool configure(float enterVolts, float resumeVolts) {
        if (!std::isfinite(enterVolts) || !std::isfinite(resumeVolts) || enterVolts <= 0.0f ||
            resumeVolts <= enterVolts) {
            return false;
        }
        lowThreshold = enterVolts;
        highThreshold = resumeVolts;
        configured = true;
        return true;
    }

    bool isConfigured() const { return configured; }
    bool isCharging() const { return charging; }
    void requestRecharge() { charging = true; } // Call in loop after an ISR signals low supply.

    bool shouldRecharge(float batteryVolts) {
        if (!configured) {
            return false;
        }
        if (!std::isfinite(batteryVolts) || batteryVolts <= 0.0f) {
            charging = true; // A missing ADC reading must not restart the modem.
        } else if (charging) {
            charging = batteryVolts < highThreshold;
        } else {
            charging = batteryVolts <= lowThreshold;
        }
        return charging;
    }

  private:
    float lowThreshold = 0.0f, highThreshold = 0.0f;
    bool configured = false, charging = false;
};

// Preserve ordinary wakes when no policy is attached. An attached policy checks voltage before
// the first sensor/modem power_up(); a bad reader/configuration keeps those modules asleep.
inline bool allowModuleWake(bool requested, RechargePolicy *policy, float (*readVolts)()) {
    if (!requested) {
        return false;
    }
    if (policy == nullptr) {
        return true;
    }
    return policy->isConfigured() && readVolts != nullptr && !policy->shouldRecharge(readVolts());
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomPower
