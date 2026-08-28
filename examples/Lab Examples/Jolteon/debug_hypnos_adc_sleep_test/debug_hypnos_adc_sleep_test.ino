/**
 * Loom 4.9 Analog + Hypnos functional test v2
 *
 * Verifies:
 *  1. Loom_Analog reads the battery correctly even after another module changes ADC resolution.
 *  2. A fired DS3231 Alarm 1 can be cleared and replaced without immediately retriggering.
 *  3. Hypnos repeatedly enters standby and wakes from a new RTC alarm.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

static constexpr uint8_t RTC_INTERRUPT_PIN = 12;
static constexpr uint32_t FIRST_ALARM_SECONDS = 5;
static constexpr uint32_t REPLACEMENT_ALARM_SECONDS = 10;
static constexpr uint32_t SLEEP_SECONDS = 10;
static constexpr float MIN_EXPECTED_BATTERY_V = 3.30f;
static constexpr float MAX_ANALOG_SPREAD_V = 0.20f;

Manager manager("HypnosAnalogTest", 1);
Loom_Analog analog(manager);
Loom_Hypnos hypnos(
    manager,
    HYPNOS_VERSION::V3_3,
    TIME_ZONE::PST,
    false,
    false
);

volatile uint32_t rtcInterruptCount = 0;
uint32_t sleepCycle = 0;
bool analogTestPassed = false;
bool alarmReplacementPassed = false;

void rtcInterruptHandler() {
    rtcInterruptCount++;
    hypnos.wakeup();
}

void printVoltage(const __FlashStringHelper *label, float voltage) {
    Serial.print(label);
    Serial.print(voltage, 3);
    Serial.println(F(" V"));
}

void printUTC(const __FlashStringHelper *label, const DateTime &time) {
    char timestamp[21];
    hypnos.dateTime_toString(time, timestamp);
    Serial.print(label);
    Serial.println(timestamp);
}

void waitForSerialReconnect(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (!Serial && millis() - start < timeoutMs) {
        delay(10);
    }
}

void blinkWakeIndicator(uint8_t count) {
    pinMode(LED_BUILTIN, OUTPUT);

    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(150);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(150);
    }
}

bool waitForInterruptCount(uint32_t targetCount, uint32_t timeoutMs) {
    const uint32_t start = millis();

    while (rtcInterruptCount < targetCount) {
        if (millis() - start >= timeoutMs) {
            return false;
        }
        delay(10);
    }

    return true;
}

void runAnalogTest() {
    Serial.println();
    Serial.println(F("[1] Analog test"));

    const float constructorVoltage = analog.getAnalog(A7);
    printVoltage(F("Constructor Vbat:               "), constructorVoltage);

    analogReadResolution(10);
    const float staticVoltage = Loom_Analog::getBatteryVoltage();
    printVoltage(F("Static read after forced 10-bit: "), staticVoltage);

    analogReadResolution(10);
    manager.measure();
    const float measuredVoltage = analog.getAnalog(A7);
    printVoltage(F("Manager measured Vbat:           "), measuredVoltage);

    float minimum = constructorVoltage;
    float maximum = constructorVoltage;

    if (staticVoltage < minimum) minimum = staticVoltage;
    if (measuredVoltage < minimum) minimum = measuredVoltage;
    if (staticVoltage > maximum) maximum = staticVoltage;
    if (measuredVoltage > maximum) maximum = measuredVoltage;

    const float spread = maximum - minimum;
    analogTestPassed =
        constructorVoltage >= MIN_EXPECTED_BATTERY_V &&
        staticVoltage >= MIN_EXPECTED_BATTERY_V &&
        measuredVoltage >= MIN_EXPECTED_BATTERY_V &&
        spread <= MAX_ANALOG_SPREAD_V;

    Serial.print(F("Analog spread:                   "));
    Serial.print(spread, 3);
    Serial.println(F(" V"));
    Serial.print(F("Analog result:                   "));
    Serial.println(analogTestPassed ? F("PASS") : F("FAIL"));
}

void runAlarmReplacementTest() {
    Serial.println();
    Serial.println(F("[2] Alarm clear and replacement test"));

    const uint32_t initialCount = rtcInterruptCount;

    hypnos.setInterruptDuration(TimeSpan(0, 0, 0, FIRST_ALARM_SECONDS));
    hypnos.reattachRTCInterrupt(RTC_INTERRUPT_PIN);

    Serial.println(F("Waiting awake for the first alarm to fire..."));
    const bool firstAlarmObserved = waitForInterruptCount(initialCount + 1, 8000);

    Serial.print(F("First alarm observed:            "));
    Serial.println(firstAlarmObserved ? F("YES") : F("NO"));

    if (!firstAlarmObserved) {
        alarmReplacementPassed = false;
        Serial.println(F("Alarm replacement result:       FAIL"));
        return;
    }

    const uint32_t countBeforeReplacement = rtcInterruptCount;
    const DateTime replacementStart = hypnos.getCurrentTime();

    hypnos.setInterruptDuration(TimeSpan(0, 0, 0, REPLACEMENT_ALARM_SECONDS));
    Serial.print(F("RTC INT pin after replacement:   "));
    Serial.println(digitalRead(RTC_INTERRUPT_PIN) == HIGH ? F("HIGH") : F("LOW"));
    hypnos.reattachRTCInterrupt(RTC_INTERRUPT_PIN);

    delay(2000);
    const bool immediateRetrigger = rtcInterruptCount != countBeforeReplacement;

    Serial.print(F("Immediate stale retrigger:       "));
    Serial.println(immediateRetrigger ? F("YES") : F("NO"));

    const bool replacementObserved = immediateRetrigger
        ? false
        : waitForInterruptCount(countBeforeReplacement + 1, 12000);

    const DateTime replacementEnd = hypnos.getCurrentTime();
    const uint32_t elapsed = replacementEnd.unixtime() - replacementStart.unixtime();

    Serial.print(F("Replacement alarm elapsed:       "));
    Serial.print(elapsed);
    Serial.println(F(" s"));

    alarmReplacementPassed =
        !immediateRetrigger &&
        replacementObserved &&
        elapsed >= REPLACEMENT_ALARM_SECONDS - 2 &&
        elapsed <= REPLACEMENT_ALARM_SECONDS + 3;

    Serial.print(F("Alarm replacement result:        "));
    Serial.println(alarmReplacementPassed ? F("PASS") : F("FAIL"));
}

void setup() {
    manager.beginSerial();

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("Loom Analog + Hypnos functional test"));
    Serial.println(F("========================================"));

    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_OFF_5V_OFF);
    hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);
    hypnos.enable();
    manager.initialize();

    if (!hypnos.registerInterrupt(
            rtcInterruptHandler,
            RTC_INTERRUPT_PIN,
            HypnosInterruptType::SLEEP,
            LOW)) {
        Serial.println(F("RTC interrupt registration failed."));
        while (true) {
            delay(1000);
        }
    }

    runAnalogTest();
    runAlarmReplacementTest();

    Serial.println();
    Serial.print(F("Initial combined result:          "));
    Serial.println(
        analogTestPassed && alarmReplacementPassed
            ? F("PASS")
            : F("FAIL")
    );

    Serial.println();
    Serial.println(F("[3] Repeating standby cycles"));
    Serial.println(F("The Feather status LED goes LOW during standby."));
    Serial.println(F("It flashes twice after every successful wake."));
}

void loop() {
    sleepCycle++;

    analogReadResolution(10);
    manager.measure();
    const float beforeSleepVoltage = analog.getAnalog(A7);

    Serial.println();
    Serial.print(F("Sleep cycle:                     "));
    Serial.println(sleepCycle);
    printVoltage(F("Vbat before sleep:               "), beforeSleepVoltage);

    const uint32_t interruptCountBeforeSleep = rtcInterruptCount;

    hypnos.setInterruptDuration(TimeSpan(0, 0, 0, SLEEP_SECONDS));
    hypnos.reattachRTCInterrupt(RTC_INTERRUPT_PIN);

    const DateTime sleepStart = hypnos.getCurrentTime();
    printUTC(F("Sleep start UTC:                 "), sleepStart);
    Serial.println(F("Entering standby..."));
    Serial.flush();

    hypnos.sleep(false);

    waitForSerialReconnect(3000);
    blinkWakeIndicator(2);

    const DateTime wakeTime = hypnos.getCurrentTime();
    const uint32_t elapsed = wakeTime.unixtime() - sleepStart.unixtime();
    const uint32_t wakeInterruptDelta = rtcInterruptCount - interruptCountBeforeSleep;
    const bool wakeInterruptObserved = wakeInterruptDelta == 1;

    analogReadResolution(10);
    manager.measure();
    const float afterSleepVoltage = analog.getAnalog(A7);

    Serial.println();
    Serial.println(F("WOKE FROM STANDBY"));
    printUTC(F("Wake time UTC:                   "), wakeTime);
    Serial.print(F("RTC elapsed:                     "));
    Serial.print(elapsed);
    Serial.println(F(" s"));
    Serial.print(F("Wake interrupt count delta:      "));
    Serial.println(wakeInterruptDelta);
    Serial.print(F("Exactly one wake interrupt:      "));
    Serial.println(wakeInterruptObserved ? F("YES") : F("NO"));
    printVoltage(F("Vbat after wake:                 "), afterSleepVoltage);

    const bool cyclePassed =
        wakeInterruptObserved &&
        elapsed >= SLEEP_SECONDS - 2 &&
        elapsed <= SLEEP_SECONDS + 4 &&
        afterSleepVoltage >= MIN_EXPECTED_BATTERY_V;

    Serial.print(F("Standby cycle result:            "));
    Serial.println(cyclePassed ? F("PASS") : F("FAIL"));

    delay(2000);
}