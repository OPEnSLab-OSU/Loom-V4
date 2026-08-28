#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

namespace {
unsigned int failures = 0;

void checkDST(const __FlashStringHelper *label, TIME_ZONE zone, int year, int month, int day,
              int hour, int minute, int second, bool expected) {
    const bool actual = Loom_Hypnos::isDaylightSavingsForDate(
        DateTime(year, month, day, hour, minute, second), zone);
    Serial.print(actual == expected ? F("PASS: ") : F("FAIL: "));
    Serial.println(label);
    if (actual != expected)
        ++failures;
}
} // namespace

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
    }

    // In 2026, North American DST starts March 8 and ends November 1.
    checkDST(F("PST second before spring transition"), PST, 2026, 3, 8, 9, 59, 59, false);
    checkDST(F("PST spring transition"), PST, 2026, 3, 8, 10, 0, 0, true);
    checkDST(F("PST second before fall transition"), PST, 2026, 11, 1, 8, 59, 59, true);
    checkDST(F("PST fall transition"), PST, 2026, 11, 1, 9, 0, 0, false);

    checkDST(F("EST second before spring transition"), EST, 2026, 3, 8, 6, 59, 59, false);
    checkDST(F("EST spring transition"), EST, 2026, 3, 8, 7, 0, 0, true);
    checkDST(F("EST second before fall transition"), EST, 2026, 11, 1, 5, 59, 59, true);
    checkDST(F("EST fall transition"), EST, 2026, 11, 1, 6, 0, 0, false);

    checkDST(F("AKST summer"), AKST, 2026, 7, 1, 12, 0, 0, true);
    checkDST(F("HST never observes DST"), HST, 2026, 7, 1, 12, 0, 0, false);
    checkDST(F("GMT never observes North American DST"), GMT, 2026, 7, 1, 12, 0, 0, false);

    Serial.print(F("DST boundary failures: "));
    Serial.println(failures);
}

void loop() {}
