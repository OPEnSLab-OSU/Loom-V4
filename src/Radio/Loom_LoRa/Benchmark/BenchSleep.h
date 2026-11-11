#pragma once

namespace bench {

constexpr uint8_t SleepButtonPin = BENCH_RTC_BUTTON_PIN;
constexpr uint32_t ButtonDebounceMs = 35;
volatile bool sleepRequested = false, buttonWoke = false;
bool sleepButtonReady = false, buttonArmed = false;
uint32_t lastButtonLow = 0;

void requestSleepISR() {
    sleepRequested = true;
}

void wakeButtonISR() {
    buttonWoke = true;
    EIC->INTENCLR.reg = 1UL << g_APinDescription[SleepButtonPin].ulExtInt;
}

void armAwakeButton() {
    sleepRequested = false;
    buttonArmed = false;
    lastButtonLow = millis();
    attachInterrupt(digitalPinToInterrupt(SleepButtonPin), requestSleepISR, FALLING);
}

void setupSleepButton() {
    if (SleepButtonPin >= PINS_COUNT ||
        g_APinDescription[SleepButtonPin].ulExtInt == NOT_AN_INTERRUPT ||
        g_APinDescription[SleepButtonPin].ulExtInt == EXTERNAL_INT_NMI ||
        g_APinDescription[SleepButtonPin].ulExtInt == g_APinDescription[RFM95_INT].ulExtInt) {
        Serial.println(F("ERR RTC sleep button requires an external interrupt separate from RFM95 DIO0"));
        return;
    }
    pinMode(SleepButtonPin, INPUT_PULLUP);
    sleepButtonReady = true;
    armAwakeButton();
}

void standbyUntilButton() {
    detachInterrupt(digitalPinToInterrupt(SleepButtonPin));
    buttonWoke = false;
    noInterrupts();
    LowPower.attachInterruptWakeup(SleepButtonPin, wakeButtonISR, LOW);
    delayMicroseconds(200);
    if (digitalRead(SleepButtonPin) == HIGH) {
        EIC->INTFLAG.reg = 1UL << g_APinDescription[SleepButtonPin].ulExtInt;
    }
    interrupts();

    Serial.end();
    USBDevice.detach();
    {
        LoomWatchdogPause watchdogPause;
        // Mask IRQ dispatch through the check and WFI so a press cannot be consumed just
        // before standby. A pending IRQ still wakes the Cortex-M0+ and runs on unmasking.
        const uint32_t savedTick = SysTick->CTRL;
        const uint32_t savedScr = SCB->SCR;
        SysTick->CTRL = savedTick & ~SysTick_CTRL_TICKINT_Msk;
        SCB->SCR = savedScr | SCB_SCR_SLEEPDEEP_Msk;
        do {
            noInterrupts();
            if (!buttonWoke && digitalRead(SleepButtonPin) == HIGH) {
                __DSB();
                __WFI();
            } else {
                buttonWoke = true;
            }
            interrupts();
        } while (!buttonWoke);
        SCB->SCR = savedScr;
        SysTick->CTRL = savedTick;
    }
    detachInterrupt(digitalPinToInterrupt(SleepButtonPin));
    USBDevice.attach();
    Serial.begin(BAUD_RATE);
    armAwakeButton();
}

void sleepBench() {
    const bool sensorsStarted = node::manager.isInitialized();
    if (sensorsStarted) {
        node::manager.power_down();
        if (!node::manager.canRemovePower()) {
            node::manager.power_up();
            Serial.println(F("ERR sleep cancelled: a node module did not confirm shutdown"));
            return;
        }
    }

#if BENCH_ENABLE_LTE
    if (hub::modemStarted) {
        hub::lte.power_down();
        if (!hub::lte.canRemovePower()) {
            if (sensorsStarted) {
                node::manager.power_up();
            }
            Serial.println(F("ERR sleep cancelled: LTE modem still powered"));
            return;
        }
        if (BENCH_LTE_SUPPLY_PIN >= 0) {
            digitalWrite(BENCH_LTE_SUPPLY_PIN, LOW);
        }
        hub::modemStarted = false;
        hub::config.upload = false;
    }
#endif

#if BENCH_ENABLE_SWEEP
    sweep::stop("sleep");
#endif
    hub::deactivate();
    node::deactivate();
    if (ready) {
        radio.recv(nullptr, nullptr);
        radio.sleep();
    }
#if BENCH_ENABLE_DENDROMETER
    if (sensorsStarted) {
        node::dendrometer.setPower(false);
    }
#endif

    Serial.println(F("Standby. Press the RTC button again to wake."));
    delay(ButtonDebounceMs);
    standbyUntilButton();

#if BENCH_ENABLE_DENDROMETER
    if (sensorsStarted) {
        node::dendrometer.setPower(true);
    }
#endif
    if (ready) {
        radio.setModeIdle();
    }
    if (role == Role::Hub) {
        hub::activate();
    }
    if (sensorsStarted) {
        node::manager.power_up();
    }
    Serial.println(F("Awake. Type status."));
}

bool serviceSleepButton() {
    if (!sleepButtonReady) {
        return false;
    }
    if (!buttonArmed) {
        if (digitalRead(SleepButtonPin) == LOW) {
            lastButtonLow = millis();
        } else if (millis() - lastButtonLow >= ButtonDebounceMs) {
            sleepRequested = false;
            buttonArmed = true;
        }
        return false;
    }
    if (!sleepRequested || digitalRead(SleepButtonPin) == LOW) {
        return false;
    }

    delay(ButtonDebounceMs);
    if (digitalRead(SleepButtonPin) != HIGH) {
        return false;
    }
    sleepRequested = false;
    sleepBench();
    return true;
}

} // namespace bench
