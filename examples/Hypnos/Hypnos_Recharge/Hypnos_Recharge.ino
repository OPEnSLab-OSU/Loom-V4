/**
 * Opt-in software recharge mode for a Feather M0 + Hypnos stack.
 * Low voltage pauses work; hourly checks continue until the battery reaches the resume voltage.
 * These deployment thresholds were requested by the user, not inferred from a battery datasheet.
 * Verify the ADC calibration, battery limits and actual sleep current on your assembled stack.
 * Choose external RTC or internal timer below. Brownout warning is separately opt-in.
 */
#include <Loom_Manager.h> // Include Manager first so Arduino discovers the Loom library.
#include <Logger.h>       // This sketch uses LOGF and ERROR explicitly.
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Utilities/Loom_RechargePolicy.h>
#include <Diagnostics/Loom_BrownoutGuard.h>

Manager manager("RechargeDemo", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, false);
loomPower::RechargePolicy recharge;

const float ENTER_RECHARGE_VOLTS = 3.7f;
const float RESUME_WORK_VOLTS = 4.2f;
const int32_t BATTERY_CHECK_SECONDS = 3600;
const int32_t SAMPLE_SECONDS = 300;
bool rechargeConfigured = false;
bool managerReady = false;
const bool USE_INTERNAL_RECHARGE_TIMER = false;

// Set both macros only after measuring the MCU supply and the available shutdown margin.
// This raw BOD33 LEVEL is NOT the battery's 3.7/4.2 V threshold. No guessed default is enabled.
#ifndef LOOM_RECHARGE_BOD_WARNING_ENABLED
#define LOOM_RECHARGE_BOD_WARNING_ENABLED 0
#endif
#if LOOM_RECHARGE_BOD_WARNING_ENABLED
#ifndef LOOM_RECHARGE_BOD_WARNING_LEVEL
#error "Set the calibrated SAMD21 BOD33 warning LEVEL (0..63), above the existing reset LEVEL."
#endif
Loom_BrownoutGuard brownout;
void SYSCTRL_Handler() { brownout.handleInterrupt(); }
#endif

float readBatteryVolts() { return Loom_Analog::getBatteryVoltage(); }
void rtcWake() { hypnos.wakeup(); } // ISR: flag the wake only; no logging, SD or sensor reads.

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
    manager.beginSerial(false);
    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.enable();
    hypnos.registerInterrupt(rtcWake);
    rechargeConfigured = recharge.configure(ENTER_RECHARGE_VOLTS, RESUME_WORK_VOLTS) &&
                         hypnos.setRechargePolicy(recharge, readBatteryVolts);
    if (!rechargeConfigured) {
        ERROR(F("Recharge configuration failed; sensor work will stay paused."));
    }
#if LOOM_RECHARGE_BOD_WARNING_ENABLED
    if (!brownout.begin(LOOM_RECHARGE_BOD_WARNING_LEVEL)) {
        ERROR(F("Brownout warning could not be armed; existing reset protection is retained."));
    }
#endif
    // Add sensor/network objects for your stack. At boot, initialize those modules only after
    // checking the battery; this example has no LTE object and does not send network data.
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
    if (!rechargeConfigured) {
        manager.pause(5000);
        return;
    }
    const bool wasCharging = recharge.isCharging();
    bool supplyWarning = false;
#if LOOM_RECHARGE_BOD_WARNING_ENABLED
    supplyWarning = brownout.takeSignal();
    if (supplyWarning) {
        recharge.requestRecharge(); // Handle the signal in the loop, never inside the ISR.
    }
#endif
    const float batteryVolts = readBatteryVolts();
    if (supplyWarning || recharge.shouldRecharge(batteryVolts)) {
        LOGF("Recharge check: battery %.3f V; work resumes at %.3f V", batteryVolts,
             RESUME_WORK_VOLTS);
        const bool slept = USE_INTERNAL_RECHARGE_TIMER
                               ? hypnos.sleepForRechargeInternal(TimeSpan(BATTERY_CHECK_SECONDS))
                               : hypnos.sleepForRecharge(TimeSpan(BATTERY_CHECK_SECONDS));
        if (!slept) {
            ERROR(F("Recharge sleep failed; check RTC/wake wiring. No sensor work will run."));
            manager.pause(5000); // Bounded retry; never enter standby without a verified alarm.
        }
        return; // Keep measurements/uploads paused throughout charging.
    }

    if (!managerReady) {
        hypnos.enable();
        manager.initialize();
        managerReady = true;
    } else if (wasCharging) {
        hypnos.enable();
        manager.power_up(); // The first LTE/sensor restart happens after recovered voltage.
    }
#if LOOM_RECHARGE_BOD_WARNING_ENABLED
    if (!brownout.isArmed()) {
        brownout.begin(LOOM_RECHARGE_BOD_WARNING_LEVEL); // Re-arm only after voltage recovered.
    }
#endif
    manager.measure();
    manager.package();
    manager.display_data();
    if (hypnos.scheduleWake(TimeSpan(SAMPLE_SECONDS))) {
        hypnos.sleep(false);
    } else {
        manager.pause(5000);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
