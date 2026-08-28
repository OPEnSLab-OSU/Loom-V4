#include <OPEnS_RTC.h>

// DateTime is stored persistently in Loom_Hypnos. Keep it a six-byte value type
// on the 32 KB SAMD21 instead of restoring the legacy per-instance text buffer.
static_assert(sizeof(DateTime) == 6, "OPEnS DateTime grew; review Feather M0 SRAM usage");

RTC_DS3231 rtc;

void exerciseCompatibilitySurface(const DateTime &wakeTime) {
    char timestamp[24];
    wakeTime.text(timestamp, sizeof(timestamp));

    // Legacy OPEnS calls used by older team sketches.
    rtc.setAlarm(wakeTime);
    rtc.setAlarm(ALM1_MATCH_DATE, wakeTime.second(), wakeTime.minute(), wakeTime.hour(),
                 wakeTime.day());
    rtc.armAlarm(1, true);
    rtc.clearAlarm();

    // RTClib-style calls used by the guarded Loom 4.9 Hypnos path.
    rtc.setAlarm1(wakeTime, DS3231_A1_Date);
    rtc.setAlarm2(wakeTime, DS3231_A2_Date);
    rtc.getAlarm1();
    rtc.getAlarm2();
    rtc.getAlarm1Mode();
    rtc.getAlarm2Mode();
    rtc.alarmFired(1);
    rtc.disableAlarm(1);
}

void setup() {
    rtc.begin();
    const DateTime now = rtc.now();
    exerciseCompatibilitySurface(now + TimeSpan(0, 0, 5, 0));
}

void loop() {}
