#include "Loom_WarningGuards.h"

#include "Loom_Hypnos.h"
#include "Utilities/Loom_RechargePolicy.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoLowPower.h>
#include <Wire.h>
LOOM_EXTERNAL_INCLUDE_END
#include "Logger.h"
#if defined(LOOM_ENABLE_TRACE) && LOOM_ENABLE_TRACE
#include "Diagnostics/Loom_Trace.h"
#endif
#include "Internet/Connectivity/NetworkComponent.h"
#include "Sensors/Loom_Analog/Loom_Analog.h"
#include "Utilities/Loom_TimeUtils.h"
#include "Utilities/Loom_ClockRead.h"

namespace {
struct TimezoneEntry {
    const char *name;
    TIME_ZONE zone;
};

// A small linear table lives in flash. The former std::map allocated 23 tree nodes on the heap
// during every Hypnos construction even though configuration is normally parsed only once.
const TimezoneEntry TIMEZONE_ENTRIES[] = {
    {"WAT", WAT}, {"AT", AT},     {"AST", AST},   {"EST", EST},   {"CST", CST},   {"MST", MST},
    {"PST", PST}, {"AKST", AKST}, {"HST", HST},   {"SST", SST},   {"GMT", GMT},   {"BST", BST},
    {"CET", CET}, {"EET", EET},   {"EEST", EEST}, {"BRT", BRT},   {"ZP4", ZP4},   {"ZP5", ZP5},
    {"ZP6", ZP6}, {"ZP7", ZP7},   {"AWST", AWST}, {"ACST", ACST}, {"AEST", AEST},
};

bool timezoneFromName(const char *name, TIME_ZONE &zone) {
    if (name == nullptr) {
        return false;
    }
    for (const TimezoneEntry &entry : TIMEZONE_ENTRIES) {
        if (strcmp(name, entry.name) == 0) {
            zone = entry.zone;
            return true;
        }
    }
    return false;
}

void clearPendingExternalInterrupt(int interruptPin) {
#if defined(ARDUINO_ARCH_SAMD)
#if ARDUINO_SAMD_VARIANT_COMPLIANCE >= 10606
    EExt_Interrupts externalInterrupt = g_APinDescription[interruptPin].ulExtInt;
#else
    EExt_Interrupts externalInterrupt = digitalPinToInterrupt(interruptPin);
#endif
    if (externalInterrupt != NOT_AN_INTERRUPT && externalInterrupt != EXTERNAL_INT_NMI) {
        EIC->INTFLAG.reg = (1ul << externalInterrupt);
        NVIC_ClearPendingIRQ(EIC_IRQn);
    }
#else
    (void)interruptPin;
#endif
}

uint8_t compileMonth(const char *month) {
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (uint8_t i = 0; i < 12; ++i) {
        if (strncmp(month, months[i], 3) == 0) {
            return i + 1;
        }
    }
    return 1;
}

DateTime compileLocalTime(const char *buildDate, const char *buildTime) {
    const char *date = buildDate ? buildDate : "";
    const char *time = buildTime ? buildTime : "";
    char month[4] = {};
    int day = 1, year = 2000, hour = 0, minute = 0, second = 0;
    sscanf(date, "%3s %d %d", month, &day, &year);
    sscanf(time, "%d:%d:%d", &hour, &minute, &second);
    return DateTime(year, compileMonth(month), day, hour, minute, second);
}

int16_t timezoneOffsetMinutes(TIME_ZONE zone) {
    if (zone == ACST) {
        return 9 * 60 + 30;
    }
    return static_cast<int16_t>(zone) * 60;
}

bool timezoneUsesDST(TIME_ZONE zone) {
    return zone == AST || zone == EST || zone == CST || zone == MST || zone == PST || zone == AKST;
}

uint32_t localToUtcSeconds(uint32_t localSeconds, int16_t utcOffsetMinutes) {
    const int32_t offsetSeconds = static_cast<int32_t>(utcOffsetMinutes) * 60;
    return offsetSeconds < 0 ? localSeconds + static_cast<uint32_t>(-offsetSeconds)
                             : localSeconds - static_cast<uint32_t>(offsetSeconds);
}

bool isDaylightSavingsForLocalWallTime(const DateTime &localTime, TIME_ZONE zone) {
    if (!timezoneUsesDST(zone)) {
        return false;
    }

    const DateTime start = Loom_Hypnos::nthWeekdayOfMonth(localTime.year(), 3, 0, 2, 2);
    const DateTime end = Loom_Hypnos::nthWeekdayOfMonth(localTime.year(), 11, 0, 1, 2);
    const uint32_t now = localTime.unixtime();
    return now >= start.unixtime() && now < end.unixtime();
}

DateTime compileUtcTime(TIME_ZONE zone, const char *buildDate, const char *buildTime) {
    const DateTime local = compileLocalTime(buildDate, buildTime);
    int16_t offsetMinutes = timezoneOffsetMinutes(zone);
    if (isDaylightSavingsForLocalWallTime(local, zone)) {
        offsetMinutes += 60;
    }
    return DateTime(localToUtcSeconds(local.unixtime(), offsetMinutes));
}

int readSerialInteger(const __FlashStringHelper *prompt, const char *label, int minimum,
                      int maximum) {
    LOG(prompt);

    while (true) {
        // Avoid Arduino String here: this path runs precisely when the RTC has lost power, and six
        // separate String allocations used to fragment the small SAMD21 heap during recovery.
        char input[12] = {};
        size_t length = 0;
        bool lineComplete = false;

        while (!lineComplete) {
            if (!Serial.available()) {
                delay(1);
                continue;
            }

            const int next = Serial.read();
            if (next == '\n') {
                lineComplete = true;
            } else if (next != '\r' && length < sizeof(input) - 1) {
                input[length++] = static_cast<char>(next);
            }
        }

        char *end = nullptr;
        const long value = strtol(input, &end, 10);
        if (length > 0 && end != input && *end == '\0' && value >= minimum && value <= maximum) {
            LOGF("%s entered: %ld", label, value);
            return static_cast<int>(value);
        }

        WARNINGF("Invalid %s; enter a value from %d to %d.", label, minimum, maximum);
    }
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::Loom_Hypnos(Manager &man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time,
                         bool useSD)
    : Module("Hypnos"), manInst(&man), sd_chip_select(version), enableSD(useSD), batch_size(0),
      custom_time(use_custom_time), timezone(zone) {

    // Establish both rails OFF before exposing the control pins as outputs.
    digitalWrite(5, HIGH);
    digitalWrite(6, LOW);
    digitalWrite(LED_BUILTIN, LOW);

    pinMode(5, OUTPUT);
    pinMode(6, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    // Create the SD Manager if we want to use SD
    if (useSD) {
        sdMan = new SDManager(manInst, sd_chip_select);
        Logger::getInstance()->setHypnos(this);
    }

    // Add the Hypnos to the module register
    manInst->registerModule(this);
    manInst->useHypnos(); // Enable the use of the hypnos
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::~Loom_Hypnos() {
    if (sdMan != nullptr) {
        delete sdMan;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::package() {
    JsonObject json = manInst->getDocument().createNestedObject("timestamp");
    if (!tryGetCurrentTime(time)) {
        // Keep both keys/schema, but never label a failed I2C read as a real default date.
        json["time_utc"] = nullptr;
        json["time_local"] = nullptr;
        WARNING(F("RTC read failed; this packet's timestamps are unavailable."));
        return;
    }
    localTime = getLocalTime(time);
    // ArduinoJson copies mutable text on assignment. Reuse one 21-byte buffer for both values.
    char text[21];
    dateTime_toString(time, text);
    json["time_utc"] = text;
    dateTime_toString(localTime, text, true);
    json["time_local"] = text;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

/* Power Rail Control Functionality */

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::setPowerRails(bool enable33, bool enable5) {
    digitalWrite(5, enable33 ? LOW : HIGH);
    digitalWrite(6, enable5 ? HIGH : LOW);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::enable() {
    const bool enable33 = !is3VDisabled(DEVICE_STATE::EXITING_SLEEP);
    const bool enable5 = !is5VDisabled(DEVICE_STATE::EXITING_SLEEP);

    enable(enable33, enable5);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::enable(bool enable33, bool enable5) {
    FUNCTION_START(this);

    // Enable the configured 3.3v and 5v rails on the Hypnos
    setPowerRails(enable33, enable5);
    digitalWrite(LED_BUILTIN, HIGH);

    if (enableSD) {
        // Enable SPI pins
        pinMode(23, OUTPUT);
        pinMode(24, OUTPUT);
        pinMode(sd_chip_select, OUTPUT);

        sdMan->begin();
#if defined(LOOM_ENABLE_TRACE) && LOOM_ENABLE_TRACE
        if (Loom_Trace *trace = Loom_Trace::current()) {
            trace->setStorageAvailable(sdMan->hasSDInitialized());
        }
#endif
    }

    // If the RTC hasn't already been initialized then do so now
    if (!RTC_initialized) {
        initializeRTC();
    }

    manInst->setEnableState(true);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::setWakeConfiguration(POWERRAIL_CONFIG config) {
    wakeModePowerConfig = config;
    applyWakeConfiguration();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::applyWakeConfiguration() {
    const bool enable33 = !is3VDisabled(DEVICE_STATE::EXITING_SLEEP);
    const bool enable5 = !is5VDisabled(DEVICE_STATE::EXITING_SLEEP);

    setPowerRails(enable33, enable5);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::disable(bool disable33, bool disable5) {
#if defined(LOOM_ENABLE_TRACE) && LOOM_ENABLE_TRACE
    if (enableSD) {
        if (Loom_Trace *trace = Loom_Trace::current()) {
            trace->setStorageAvailable(false);
        }
    }
#endif
    // Disable the configured 3.3v and 5v rails on the Hypnos
    setPowerRails(!disable33, !disable5);
    digitalWrite(LED_BUILTIN, LOW);

    if (enableSD) {
        // Disable SPI pins/SD chip select to save power
        pinMode(23, INPUT);
        pinMode(24, INPUT);
        pinMode(sd_chip_select, INPUT);
    }

    manInst->setEnableState(false);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::is3VDisabled(DEVICE_STATE deviceState) {
    POWERRAIL_CONFIG config;
    switch (deviceState) {
    case ENTERING_SLEEP:
        config = sleepModePowerConfig;
        break;
    case EXITING_SLEEP:
        config = wakeModePowerConfig;
        break;
    default:
        return false; // Preserve the fallback: an unknown state leaves the rail enabled.
    }
    return config == PR_3V_OFF_5V_ON || config == PR_3V_OFF_5V_OFF;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::is5VDisabled(DEVICE_STATE deviceState) {
    POWERRAIL_CONFIG config;
    switch (deviceState) {
    case ENTERING_SLEEP:
        config = sleepModePowerConfig;
        break;
    case EXITING_SLEEP:
        config = wakeModePowerConfig;
        break;
    default:
        return false; // Preserve the fallback: an unknown state leaves the rail enabled.
    }
    return config == PR_3V_ON_5V_OFF || config == PR_3V_OFF_5V_OFF;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

/* Interrupt Functionality */

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::InterruptRegistration *Loom_Hypnos::findInterruptRegistration(int pin) {
    for (InterruptRegistration &registration : interruptRegistrations) {
        if (registration.callback != nullptr && registration.pin == pin) {
            return &registration;
        }
    }
    return nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::registerInterrupt(InterruptCallbackFunction isrFunc, int interruptPin,
                                    HypnosInterruptType interruptType, int triggerState) {
    FUNCTION_START(this);
    pinMode(interruptPin, INPUT_PULLUP); //  Set interrupt pin input mode
    LOG(F("Registering interrupt..."));

    // If the RTC hasn't already been initialized then do so now if we are trying to schedule an RTC
    // interrupt
    if (!RTC_initialized && interruptPin == 12) {
        initializeRTC();
    }

    // Make sure a callback function was supplied
    if (isrFunc != nullptr) {
        InterruptRegistration *registration = findInterruptRegistration(interruptPin);
        if (registration == nullptr) {
            for (InterruptRegistration &candidate : interruptRegistrations) {
                if (candidate.callback == nullptr) {
                    registration = &candidate;
                    break;
                }
            }
        }
        if (registration == nullptr) {
            ERROR(F("Failed to attach interrupt: Hypnos supports two registered interrupt sources "
                    "on the Feather M0."));
            return false;
        }

        // If the interrupt we registered is for sleep we should set the interrupt to wake the
        // device from sleep
        if (interruptType == SLEEP) {
            sleepInterruptPin = interruptPin;
            LowPower.attachInterruptWakeup(interruptPin, isrFunc, triggerState);
            LOG(F("Interrupt successfully attached!"));
        } else {
            attachInterrupt(digitalPinToInterrupt(interruptPin), isrFunc, triggerState);
            LOG(F("Interrupt successfully attached!"));
        }
        // Assignment intentionally replaces an older registration for this pin.
        registration->callback = isrFunc;
        registration->pin = static_cast<int16_t>(interruptPin);
        registration->triggerState = static_cast<int8_t>(triggerState);
        registration->type = interruptType;
        return true;
    } else {
        detachInterrupt(digitalPinToInterrupt(interruptPin));
        ERROR(F("Failed to attach interrupt! Interrupt callback evaluated to a null pointer, it is "
                "possible you forgot to supply a callback function"));
        return false;
    }
    FUNCTION_END;
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::reattachRTCInterrupt(int interruptPin) {
    FUNCTION_START(this);
    InterruptRegistration *registered = findInterruptRegistration(interruptPin);
    if (registered == nullptr) {
        ERROR(F("Failed to reattach interrupt! Interrupt has not previously been registered..."));
        return false;
    }

    const HypnosInterruptType interruptType = registered->type;
    // LOW is level-sensitive on the SAMD21. Keep an old pending level from
    // calling the sketch ISR while we inspect and acknowledge the RTC.
    detachInterrupt(digitalPinToInterrupt(interruptPin));
    if (interruptType == SLEEP && interruptPin == 12 && digitalRead(interruptPin) == LOW) {
        // DS3231 alarms are active-low level interrupts. If the scheduled alarm
        // has genuinely elapsed, sleep() will deliver the overrun callback. If
        // it has not elapsed, recover a stale alarm flag before attaching. The
        // previous behavior returned success without attaching anything, which
        // allowed sleep() to enter standby with no usable wake source.
        DateTime now;
        if (RTC_initialized && alarmScheduled && !tryGetCurrentTime(now)) {
            ERROR(F("Cannot verify the active RTC alarm; wake interrupt was not attached."));
            return false;
        }
        const bool alarmElapsed =
            RTC_initialized && alarmScheduled && alarmTime.unixtime() <= now.unixtime();
        if (alarmElapsed) {
            LOG(F("RTC alarm is already active; deferring callback to the sleep overrun handler."));
            return true;
        }

        WARNING(F("RTC INT was LOW before its scheduled time; clearing stale alarm state."));
        if (!RTC_DS.clearAlarm()) {
            ERRORF("Could not clear DS3231 alarm flags (I2C error %u); wake interrupt was not "
                   "attached.",
                   static_cast<unsigned int>(RTC_DS.lastI2CError()));
            return false;
        }
        clearPendingExternalInterrupt(interruptPin);
        delay(2);
        if (digitalRead(interruptPin) == LOW) {
            ERROR(F("RTC INT remained LOW after clearing alarm state; wake interrupt was not "
                    "attached."));
            return false;
        }
    }

    clearPendingExternalInterrupt(interruptPin);
    if (interruptType != SLEEP) {

        attachInterrupt(digitalPinToInterrupt(interruptPin), registered->callback,
                        registered->triggerState);
    } else {
        LowPower.attachInterruptWakeup(interruptPin, registered->callback,
                                       registered->triggerState);
    }
    LOG(F("Interrupt successfully reattached!"));
    FUNCTION_END;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::wakeup() {
    if (sleepInterruptPin >= 0) {
        detachInterrupt(digitalPinToInterrupt(sleepInterruptPin));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::setCompileTime(const char *buildDate, const char *buildTime) {
    strncpy(sketchCompileDate, buildDate ? buildDate : "", sizeof(sketchCompileDate) - 1);
    strncpy(sketchCompileTime, buildTime ? buildTime : "", sizeof(sketchCompileTime) - 1);
    sketchCompileDate[sizeof(sketchCompileDate) - 1] = '\0';
    sketchCompileTime[sizeof(sketchCompileTime) - 1] = '\0';
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::initializeRTC() {
    FUNCTION_START(this);
    RTC_initialized = false; // A failed reinitialization must not retain an earlier success.
    alarmScheduled = false;  // An old alarm is not evidence for this initialization attempt.
    LOG("Initializing DS3231....");

    // Failed initialization returns without arming sleep; the sketch can report/retry it.
    if (!RTC_DS.begin()) {
        ERROR(F("Couldn't start RTC! Check connections; RTC sleep remains unavailable."));
        return;
    }

    // This may end up causing a problem in practice - what if RTC loses power in field? Shouldn't
    // happen with coin cell batt backup
    const bool hasSketchCompileTime = sketchCompileDate[0] != '\0' && sketchCompileTime[0] != '\0';
    const bool rtcLostPower = RTC_DS.lostPower();
    if (!RTC_DS.lastOperationSucceeded()) {
        ERRORF("Could not read DS3231 status register (I2C error %u).",
               static_cast<unsigned int>(RTC_DS.lastI2CError()));
        return;
    }
    if (rtcLostPower) {
        WARNING(F("RTC lost power."));

        if (hasSketchCompileTime) {
            if (!writeRtcUtc(compileUtcTime(timezone, sketchCompileDate, sketchCompileTime))) {
                ERRORF("Could not verify DS3231 compile time (I2C status %u; 0 can mean a time "
                       "mismatch).",
                       static_cast<unsigned int>(RTC_DS.lastI2CError()));
                return;
            }
        } else if (custom_time && Serial) {
            set_custom_time();
            RTC_initialized = false; // This outer initialization still has to configure alarms.
        } else {
            WARNING(F("RTC was not adjusted because no explicit time source was provided. Call "
                      "setCompileTime(__DATE__, __TIME__) before enable(), enable custom time, or "
                      "perform a network time update."));
        }
    } else if (hasSketchCompileTime) {
        const DateTime compileUTC = compileUtcTime(timezone, sketchCompileDate, sketchCompileTime);
        const DateTime rtcTime = RTC_DS.now();
        if (!RTC_DS.lastOperationSucceeded()) {
            ERRORF("Could not read DS3231 time (I2C error %u).",
                   static_cast<unsigned int>(RTC_DS.lastI2CError()));
            return;
        }
        if (rtcTime.unixtime() < compileUTC.unixtime() && !writeRtcUtc(compileUTC)) {
            ERRORF(
                "Could not verify DS3231 compile time (I2C status %u; 0 can mean a time mismatch).",
                static_cast<unsigned int>(RTC_DS.lastI2CError()));
            return;
        }
    }

    // Establish a verified, inactive alarm output before allowing standby.
    if (!RTC_DS.disableAlarm(1) || !RTC_DS.disableAlarm(2) || !RTC_DS.clearAlarm(1) ||
        !RTC_DS.clearAlarm(2) || !RTC_DS.writeSqwPinMode(DS3231_OFF)) {
        ERRORF("Could not initialize DS3231 alarm output (I2C error %u).",
               static_cast<unsigned int>(RTC_DS.lastI2CError()));
        return;
    }

    // We successfully started the RTC
    LOG(F("DS3231 Real-Time Clock Initialized Successfully!"));
    RTC_initialized = true;
    DateTime t;
    if (!tryGetCurrentTime(t)) {
        WARNING(F("RTC is configured, but UTC is unavailable; establish time before logging."));
        return;
    }
    char tbuf[21];
    dateTime_toString(t, tbuf);
    LOGF("DS3231 current time: %s", tbuf);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime Loom_Hypnos::getLocalTime(DateTime time) {
    int16_t offsetMinutes = timezoneOffsetMinutes(timezone);
    if (isDaylightSavingsForDate(time, timezone)) {
        offsetMinutes += 60;
    }
    return time + TimeSpan(static_cast<int32_t>(offsetMinutes) * 60);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::releaseRTCInterrupt() {
    // An Alarm 1 date match can set A1F again on a future matching date. A
    // consumed one-shot wake must disable both alarm enables, not just clear
    // their flags. In particular, a LOW-triggered SAMD21 EIC can immediately
    // re-enter the callback while INT/SQW remains asserted.
    if (sleepInterruptPin == 12) {
        detachInterrupt(digitalPinToInterrupt(12));
    }
    const bool alarm1Disabled = RTC_DS.disableAlarm(1);
    const uint8_t alarm1Error = RTC_DS.lastI2CError();
    const bool alarm2Disabled = RTC_DS.disableAlarm(2);
    const uint8_t alarm2Error = RTC_DS.lastI2CError();
    if (!alarm1Disabled || !alarm2Disabled) {
        ERRORF("Could not release DS3231 INT (Alarm 1 I2C error %u, Alarm 2 I2C error %u).",
               static_cast<unsigned int>(alarm1Error), static_cast<unsigned int>(alarm2Error));
        return false;
    }

    if (sleepInterruptPin == 12) {
        delay(2); // Allow the open-drain line to rise through its pullup.
        if (digitalRead(12) == LOW) {
            ERROR(F("DS3231 alarms are disabled but RTC INT remains LOW; check the RTC pullup, "
                    "shared interrupt wiring, and power rails."));
            return false;
        }
        clearPendingExternalInterrupt(12);
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime Loom_Hypnos::nthWeekdayOfMonth(int year, int month, int dow, int week, int hour) {
    const DateTime firstOfMonth(year, month, 1, 0, 0, 0);
    int day = 1 + ((dow - firstOfMonth.dayOfTheWeek() + 7) % 7);

    if (week == 0) {
        static const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int finalDay = daysInMonth[month - 1];
        if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
            finalDay = 29;
        }
        while (day + 7 <= finalDay) {
            day += 7;
        }
    } else {
        day += (week - 1) * 7;
    }
    return DateTime(year, month, day, hour, 0, 0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::isDaylightSavingsForDate(const DateTime &utcTime, TIME_ZONE zone) {
    if (!timezoneUsesDST(zone)) {
        return false;
    }

    const int16_t standardOffsetMinutes = timezoneOffsetMinutes(zone);
    const DateTime startLocalStandard = nthWeekdayOfMonth(utcTime.year(), 3, 0, 2, 2);
    const DateTime endLocalDaylight = nthWeekdayOfMonth(utcTime.year(), 11, 0, 1, 2);
    const uint32_t startUtc =
        localToUtcSeconds(startLocalStandard.unixtime(), standardOffsetMinutes);
    const uint32_t endUtc =
        localToUtcSeconds(endLocalDaylight.unixtime(), standardOffsetMinutes + 60);
    const uint32_t nowUtc = utcTime.unixtime();
    return nowUtc >= startUtc && nowUtc < endUtc;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::isDaylightSavings() {
    return isDaylightSavingsForDate(getCurrentTime(), timezone);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime Loom_Hypnos::getCurrentTime() {
    if (RTC_initialized) {
        return RTC_DS.now();
    } else {
        LOG(F("Attempted to pull time when RTC was not previously initialized! Returned default "
              "datetime"));
        return DateTime();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::tryGetCurrentTime(DateTime &utc) {
    if (!RTC_initialized || rtcWriteUnverified) {
        return false;
    }
    // Recheck status rather than caching it: the RTC can lose oscillator power between wakes.
    return loomTime::readEstablishedUtc(RTC_DS, utc);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::writeRtcUtc(const DateTime &utc) {
    const bool verified = loomTime::writeVerifiedUtc(RTC_DS, utc, millis);
    // A failed write can leave plausible but mixed date bytes while OSF is already clear.
    // Remember that uncertainty for this session; only a verified explicit set clears it.
    rtcWriteUnverified = !verified;
    if (!verified) {
        alarmScheduled = false;
    }
    return verified;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::networkTimeUpdate() {
    FUNCTION_START(this);
    if (!RTC_initialized) {
        ERROR(F("Initialize Hypnos/RTC before requesting a network time update."));
        return false;
    }
    if (networkComponent == nullptr) {
        ERROR("Network component not set in Hypnos; RTC time was not updated.");
        return false;
    }

    // Batch deployments intentionally leave LTE disconnected between upload
    // windows. That is a normal power-saving state, not a network-time error.
    if (!networkComponent->isConnected()) {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    float tz = timezoneOffsetMinutes(timezone) / 60.0f;

    // Retry network retrieval or a failed RTC write at most twice.
    for (int i = 0; i < 2; i++) {
        LOG("Attempting to set RTC time to the current network time...");

        // Attempt to retrieve the current time from our network component
        if (networkComponent->getNetworkTime(&year, &month, &day, &hour, &minute, &second, &tz)) {
            // Validate the original integers before DateTime narrows its fields to bytes.
            // Otherwise a corrupt modem response (for example month=257) can become January.
            if (!loomTime::validUtcFields(year, month, day, hour, minute, second)) {
                ERROR(F("Network returned an invalid UTC date; RTC was not changed."));
                continue;
            }
            if (!writeRtcUtc(DateTime(year, month, day, hour, minute, second))) {
                ERRORF("Could not verify DS3231 network time (I2C status %u; 0 can mean a time "
                       "mismatch).",
                       static_cast<unsigned int>(RTC_DS.lastI2CError()));
                continue;
            }
            DateTime t;
            if (!tryGetCurrentTime(t)) {
                ERROR(F("RTC read failed after network time update; success not confirmed."));
                continue;
            }
            char tbuf[21];
            dateTime_toString(t, tbuf);
            LOGF("Network time successfully set to: %s", tbuf);
            return true;
        } else {
            ERROR("Failed to get network time! Time has not been set. Retrying...");
        }
    }
    FUNCTION_END;
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::dateTime_toString(DateTime time, char array[21], bool isLocal) {

    // Formatted as: YYYY-MM-DDTHH:MM:SSZ
    const unsigned int year = static_cast<unsigned int>(time.year() % 10000);
    const unsigned int month = static_cast<unsigned int>(time.month() % 100);
    const unsigned int day = static_cast<unsigned int>(time.day() % 100);
    const unsigned int hour = static_cast<unsigned int>(time.hour() % 100);
    const unsigned int minute = static_cast<unsigned int>(time.minute() % 100);
    const unsigned int second = static_cast<unsigned int>(time.second() % 100);

    if (isLocal) {
        snprintf_P(array, 21, PSTR("%04u-%02u-%02uT%02u:%02u:%02u"), year, month, day, hour, minute,
                   second);
    } else {
        snprintf_P(array, 21, PSTR("%04u-%02u-%02uT%02u:%02u:%02uZ"), year, month, day, hour,
                   minute, second);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::set_custom_time() {
    FUNCTION_START(this);

    // Let the user know that they should enter local time
    LOG(F("Please use UTC time, not local!"));

    const int year =
        readSerialInteger(F("Enter the Year (Four digits, e.g. 2020)"), "year", 2000, 2099);
    const int month = readSerialInteger(F("Enter the Month (1 ~ 12)"), "month", 1, 12);
    const int day = readSerialInteger(F("Enter the Day (1 ~ 31)"), "day", 1, 31);
    const int hour = readSerialInteger(F("Enter the Hour (0 ~ 23)"), "hour", 0, 23);
    const int minute = readSerialInteger(F("Enter the Minute (0 ~ 59)"), "minute", 0, 59);
    const int second = readSerialInteger(F("Enter the Second (0 ~ 59)"), "second", 0, 59);

    // Set the RTC to the custom time
    if (!writeRtcUtc(DateTime(year, month, day, hour, minute, second))) {
        ERRORF("Could not verify custom DS3231 time (I2C status %u; 0 can mean a time mismatch).",
               static_cast<unsigned int>(RTC_DS.lastI2CError()));
        return;
    }
    RTC_initialized = true;

    // Output
    DateTime t;
    if (!tryGetCurrentTime(t)) {
        ERROR(F("RTC read failed after setting custom time; success not confirmed."));
        return;
    }
    char tbuf[21];
    dateTime_toString(t, tbuf);
    LOGF("Custom time successfully set to: %s", tbuf);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::setInterruptDuration(const TimeSpan duration) {
    FUNCTION_START(this);
    (void)scheduleWake(duration);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::scheduleWake(const TimeSpan duration) {
    FUNCTION_START(this);

    sampleIntervalSeconds = 0; // Explicitly return to the established relative sleep API.
    nextSampleUtc = 0;

    if (!RTC_initialized) {
        ERROR(F("Cannot set an RTC alarm before the RTC is initialized."));
        alarmScheduled = false;
        return false;
    }

    if (!loomTime::validRtcWakeDelay(duration.totalseconds())) {
        ERROR(F("RTC alarm duration must be 1 second through 27 days."));
        alarmScheduled = false;
        return false;
    }

    DateTime now;
    if (!tryGetCurrentTime(now)) {
        ERROR(F("Cannot read the RTC to schedule a relative wake alarm."));
        alarmScheduled = false;
        return false;
    }
    const uint32_t lastSupportedUtc = DateTime(2099, 12, 31, 23, 59, 59).unixtime();
    if (now.unixtime() > lastSupportedUtc - static_cast<uint32_t>(duration.totalseconds())) {
        ERROR(F("Relative wake time is outside the supported RTC calendar."));
        alarmScheduled = false;
        return false;
    }
    const bool scheduled = armRTCAlarm(now + duration);
    FUNCTION_END;
    return scheduled;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::setSampleInterval(const TimeSpan interval) {
    FUNCTION_START(this);
    const int32_t seconds = interval.totalseconds();
    // DS3231 Alarm 1 compares day-of-month, not month/year. A short interval prevents an
    // earlier monthly match. Longer schedules require periodic wake/rearm, a separate feature.
    if (!RTC_initialized || !loomTime::validRtcWakeDelay(seconds)) {
        ERROR(F("Sample interval requires an initialized RTC and 1 second through 27 days."));
        alarmScheduled = false;
        sampleIntervalSeconds = 0;
        nextSampleUtc = 0;
        return false;
    }
    DateTime now;
    if (!tryGetCurrentTime(now)) {
        ERROR(F("Cannot read the RTC to schedule the next sample."));
        alarmScheduled = false;
        return false;
    }
    const uint32_t nowUtc = now.unixtime();
    if (sampleIntervalSeconds != static_cast<uint32_t>(seconds) || nowUtc < lastSampleClockUtc) {
        nextSampleUtc = 0; // Changed period or backwards clock adjustment: use a fresh anchor.
    }
    uint32_t targetUtc = 0;
    if (!loomTime::nextSampleTime(nowUtc, static_cast<uint32_t>(seconds), nextSampleUtc,
                                  targetUtc) ||
        targetUtc > DateTime(2099, 12, 31, 23, 59, 59).unixtime()) {
        ERROR(F("Next sample time is outside the supported RTC calendar."));
        alarmScheduled = false;
        return false;
    }
    sampleIntervalSeconds = static_cast<uint32_t>(seconds);
    nextSampleUtc = targetUtc;
    lastSampleClockUtc = nowUtc;
    return armRTCAlarm(DateTime(targetUtc));
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::armRTCAlarm(const DateTime target) {
    FUNCTION_START(this);
    alarmScheduled = false;

    // Reset both alarm sources before replacement so neither can hold the shared
    // active-low INT/SQW line asserted. The hardened OPEnS driver repeats this
    // invariant inside its contiguous control/status arm transaction.
    if (!RTC_DS.disableAlarm(1) || !RTC_DS.disableAlarm(2) || !RTC_DS.clearAlarm(1) ||
        !RTC_DS.clearAlarm(2)) {
        ERRORF("Could not reset DS3231 alarms (I2C error %u); sleep will be aborted.",
               static_cast<unsigned int>(RTC_DS.lastI2CError()));
        alarmScheduled = false;
        return false;
    }
    if (sleepInterruptPin >= 0) {
        clearPendingExternalInterrupt(sleepInterruptPin);
    }

    alarmTime = target;
    alarmScheduled = RTC_DS.setAlarm1(alarmTime, DS3231_A1_Date);
    if (!alarmScheduled) {
        ERRORF("Failed to set RTC alarm 1 (I2C error %u).",
               static_cast<unsigned int>(RTC_DS.lastI2CError()));
        return false;
    }

    // Independently read the alarm registers back so an interrupted/failed I2C
    // write cannot lead to an indefinite sleep with the wrong Alarm 1 value.
    const DateTime programmedAlarm = RTC_DS.getAlarm1();
    const bool alarmReadOk = RTC_DS.lastOperationSucceeded();
    const Ds3231Alarm1Mode programmedMode = RTC_DS.getAlarm1Mode();
    const bool modeReadOk = RTC_DS.lastOperationSucceeded();
    const bool alarmMatches = alarmReadOk && modeReadOk && programmedMode == DS3231_A1_Date &&
                              programmedAlarm.day() == alarmTime.day() &&
                              programmedAlarm.hour() == alarmTime.hour() &&
                              programmedAlarm.minute() == alarmTime.minute() &&
                              programmedAlarm.second() == alarmTime.second();
    if (!alarmMatches) {
        releaseRTCInterrupt();
        alarmScheduled = false;
        ERROR(
            F("RTC alarm readback did not match the requested wake time; sleep will be aborted."));
        return false;
    }

    // Clear a match flag that may have become pending while the alarm registers
    // were being replaced. This mirrors the proven pre-RTClib Hypnos sequence.
    if (!RTC_DS.clearAlarm(1)) {
        RTC_DS.disableAlarm(1);
        alarmScheduled = false;
        ERRORF("Could not clear the programmed DS3231 alarm flag (I2C error %u); sleep will be "
               "aborted.",
               static_cast<unsigned int>(RTC_DS.lastI2CError()));
        return false;
    }
    if (sleepInterruptPin >= 0) {
        clearPendingExternalInterrupt(sleepInterruptPin);
    }

    // A clock fault during alarm preparation must not be reported as a successful schedule.
    DateTime utc;
    if (!tryGetCurrentTime(utc)) {
        alarmScheduled = false;
        (void)RTC_DS.disableAlarm(1); // Best effort; the sketch remains awake even if I2C fails.
        ERROR(F("RTC UTC became unavailable during alarm preparation; sleep aborted."));
        return false;
    }
    DateTime t = getLocalTime(utc);
    char tbuf[21];
    dateTime_toString(t, tbuf);
    LOGF("Current Time (Local): %s", tbuf);
    t = getLocalTime(alarmTime);
    dateTime_toString(t, tbuf);
    LOGF("Next interrupt alarm set for: %s", tbuf);
    return true;
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

/* Sleep Functionality */

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::sleep(bool waitForSerial) { (void)sleepImpl(waitForSerial, true); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::sleepForRecharge(const TimeSpan checkInterval) {
    FUNCTION_START(this);
    if (sleepInterruptPin != 12) {
        ERROR(F("Recharge sleep requires the registered Hypnos RTC wake on pin 12."));
        return false;
    }
    // Restore the RTC/SD control path, but do not call Manager::power_up() here. Sensors and
    // LTE must stay logically asleep until the sketch has measured a recovered battery.
    enable();
    if (!scheduleWake(checkInterval)) {
        manInst->power_down();
        disable();
        return false;
    }
    const bool slept = sleepImpl(false, false);
    if (!slept) {
        disable();
    }
    FUNCTION_END;
    return slept;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::sleepForRechargeInternal(const TimeSpan checkInterval) {
    FUNCTION_START(this);
    const int32_t seconds = checkInterval.totalseconds();
    if (!loomTime::validRtcWakeDelay(seconds)) {
        return false;
    }
    manInst->power_down();
    if (!manInst->canRemovePower()) {
        ERROR(F("Recharge aborted: a module has not acknowledged shutdown."));
        return false;
    }
    if (RTC_initialized && alarmScheduled) {
        if (!releaseRTCInterrupt()) {
            return false;
        }
        alarmScheduled = false;
    }
    if (sleepInterruptPin >= 0) {
        detachInterrupt(digitalPinToInterrupt(sleepInterruptPin));
    }
    if (!pre_sleep(true, false)) {
        return false;
    }
    shouldPowerUp = true;
    {
        LoomWatchdogPause watchdogPause;
        LowPower.sleep(static_cast<uint32_t>(seconds) * 1000UL);
    }
    // Restore control/USB only. The next voltage check determines when work can resume.
    post_sleep(false, false);
    FUNCTION_END;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::setRechargePolicy(loomPower::RechargePolicy &policy, float (*reader)()) {
    if (!policy.isConfigured() || reader == nullptr) {
        ERROR(F("Recharge policy needs valid thresholds and a battery-voltage reader."));
        return false;
    }
    rechargePolicy = &policy;
    readBatteryVolts = reader;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::sleepImpl(bool waitForSerial, bool wakeModules) {
    FUNCTION_START(this);

    if (sleepInterruptPin < 0) {
        ERROR(F("Sleep aborted because no SLEEP interrupt is registered."));
        return false;
    }

    const bool rtcWakeSource = sleepInterruptPin == 12;
    if (rtcWakeSource && (!RTC_initialized || !alarmScheduled)) {
        ERROR(F("Sleep aborted because no valid RTC alarm is scheduled."));
        return false;
    }

    // If the alarm set time is less than the current time we missed our next alarm so we need to
    // set a new one, we need to check if we have powered on already so we dont use the RTC that
    // isn't enabled
    bool hasAlarmTriggered = false;

    // Try to power down the active modules
    if (shouldPowerUp) {
        manInst->power_down();

        // Compare against the exact DateTime captured when the alarm was set.
        // Reconstructing getAlarm1() with the current month breaks across month/year boundaries.
        if (rtcWakeSource) {
            DateTime now;
            if (!tryGetCurrentTime(now)) {
                ERROR(F("Sleep aborted: cannot check the RTC wake deadline."));
                if (wakeModules) {
                    restoreModulesAfterSleep(wakeModules);
                }
                return false;
            }
            hasAlarmTriggered = alarmTime.unixtime() <= now.unixtime();
        }

        // 50ms delay allows this last message to be sent before the bus disconnects
        LOG("Entering Standby Sleep...");
        delay(50);
    }

    if (rtcWakeSource && hasAlarmTriggered && sampleIntervalSeconds > 0) {
        // Work took longer than this period. Skip missed grid slots before sleeping instead
        // of immediately collecting a burst of catch-up samples. Relative-delay users retain
        // the established resample behavior below.
        WARNING(F("Sample interval overrun; skipping missed slots and keeping the UTC phase."));
        if (!setSampleInterval(TimeSpan(static_cast<int32_t>(sampleIntervalSeconds)))) {
            if (shouldPowerUp && wakeModules) {
                restoreModulesAfterSleep(wakeModules);
            }
            return false;
        }
        hasAlarmTriggered = false;
    }

    // Prepare the wake source before entering standby. If the RTC alarm becomes
    // active during that preparation, handle it through the normal overrun path
    // instead of treating the asserted line as an attachment failure.
    if (!hasAlarmTriggered) {
        if (!pre_sleep(!wakeModules)) {
            DateTime now;
            const bool missedRtcAlarm = rtcWakeSource && tryGetCurrentTime(now) && alarmScheduled &&
                                        alarmTime.unixtime() <= now.unixtime();
            if (missedRtcAlarm && sampleIntervalSeconds > 0) {
                // Preparation itself can cross the deadline. Retry preparation once after
                // advancing the grid; tiny periods must not create an unbounded sleep loop.
                if (!setSampleInterval(TimeSpan(static_cast<int32_t>(sampleIntervalSeconds))) ||
                    !pre_sleep(!wakeModules)) {
                    ERROR(F("Sleep preparation exceeded the sample interval; staying awake."));
                    if (shouldPowerUp && wakeModules) {
                        restoreModulesAfterSleep(wakeModules);
                    }
                    return false;
                }
            } else if (missedRtcAlarm) {
                hasAlarmTriggered = true;
            } else {
                ERROR(F("Sleep aborted because the registered wake source was not ready."));
                if (shouldPowerUp && wakeModules) {
                    restoreModulesAfterSleep(wakeModules);
                }
                return false;
            }
        }
    }

    if (!hasAlarmTriggered) {
        shouldPowerUp = true;
        {
            // SAMD WDT keeps running in standby. Suspend even a sketch-enabled watchdog, then
            // restore its exact configuration before wake I/O. A configured wake guard below
            // deliberately takes precedence over that restored timeout.
            LoomWatchdogPause watchdogPause;
            LowPower.sleep();
        }
        enableWakeWatchdog();
    }
    // If it has we want to trigger a resample which requires powering the sensors back up
    else {
        enableWakeWatchdog();
        WARNING("Alarm triggered during sample, specified sample duration was too short! "
                "Resampling...");
        releaseRTCInterrupt();
        alarmScheduled = false;
        if (sleepInterruptPin >= 0) {
            clearPendingExternalInterrupt(sleepInterruptPin);
        }
        if (shouldPowerUp && wakeModules) {
            restoreModulesAfterSleep(wakeModules);
        }
        InterruptRegistration *registered = findInterruptRegistration(sleepInterruptPin);
        if (registered != nullptr && registered->callback != nullptr) {
            registered->callback();
        }
    }
    WD_TIMER_RESET;

    // If the alarm hadn't triggered last time we want to wake up like normal
    if (!hasAlarmTriggered) {
        post_sleep(waitForSerial, wakeModules); // Wake up
    }
    return !hasAlarmTriggered;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::pre_sleep(bool forceRailsOff, bool externalWake) {
    FUNCTION_START(this);
    const bool disable5 = forceRailsOff || is5VDisabled(DEVICE_STATE::ENTERING_SLEEP);
    const bool disable33 = forceRailsOff || is3VDisabled(DEVICE_STATE::ENTERING_SLEEP);
    delay(1000);

    // Validate and attach the wake source while Serial and the sensor rails are
    // still available. A failure is now visible to the user and leaves the
    // device awake instead of silently entering unbounded standby.
    if (externalWake && (sleepInterruptPin < 0 || !reattachRTCInterrupt(sleepInterruptPin))) {
        ERROR(F("Could not attach the registered wake interrupt before standby."));
        return false;
    }

    if (externalWake && sleepInterruptPin == 12 && digitalRead(sleepInterruptPin) == LOW) {
        DateTime now;
        if (RTC_initialized && alarmScheduled && tryGetCurrentTime(now) &&
            alarmTime.unixtime() <= now.unixtime()) {
            WARNING(F("RTC alarm became active during pre-sleep preparation; skipping standby."));
        } else {
            ERROR(F("RTC INT is LOW before its scheduled time; refusing to enter standby."));
        }
        return false;
    }

    if (!manInst->canRemovePower()) {
        ERROR(F("A module has not acknowledged shutdown; peripheral rails stay on."));
        return false;
    }

    // Close the serial connection and detach
    Serial.end();
    USBDevice.detach();

    // Disable the power rails
    disable(disable33, disable5);
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::post_sleep(bool waitForSerial, bool wakeModules) {
    FUNCTION_START(this);
    // Enable the Watchdog timer when waking up
    enableWakeWatchdog();
    LOOM_FEED_WATCHDOG();

    if (shouldPowerUp) {
        USBDevice.attach();
        LOOM_FEED_WATCHDOG();
        Serial.begin(115200);
        LOOM_FEED_WATCHDOG();

        enable();
        LOOM_FEED_WATCHDOG();

        // Acknowledge immediately after restoring the RTC bus, before the
        // ordinary wake delay and module reinitialization. The pin is a level,
        // not a pulse; merely detaching the Feather interrupt is insufficient.
        if (RTC_initialized && alarmScheduled) {
            if (sleepInterruptPin == 12) {
                const bool alarm1Fired = RTC_DS.alarmFired(1);
                if (!RTC_DS.lastOperationSucceeded()) {
                    ERRORF("Could not read the DS3231 Alarm 1 wake flag (I2C error %u).",
                           static_cast<unsigned int>(RTC_DS.lastI2CError()));
                } else if (!alarm1Fired) {
                    DateTime wakeTime;
                    if (tryGetCurrentTime(wakeTime) && wakeTime.unixtime() < alarmTime.unixtime()) {
                        WARNINGF(
                            "Woke %lu s before the DS3231 Alarm 1 match; INT pin is %s. "
                            "Check for another wake source or noise on the RTC interrupt line.",
                            static_cast<unsigned long>(alarmTime.unixtime() - wakeTime.unixtime()),
                            digitalRead(12) == LOW ? "LOW" : "HIGH");
                    }
                }
            }
            releaseRTCInterrupt();
            alarmScheduled = false;
        }
        LOOM_FEED_WATCHDOG();
        delay(1000);
        LOOM_FEED_WATCHDOG();

        LOG(F("Device has awoken from sleep!"));
        LOOM_FEED_WATCHDOG();

        // Ordinary wakes reinitialize modules. A recharge check restores only the control
        // path; starting the modem/fans now would spend energy before checking the battery.
        const bool resumeModules = restoreModulesAfterSleep(wakeModules);

        // We want to wait for the user to re-open the serial monitor before continuing to see
        // readouts
        if (waitForSerial && resumeModules) {
            const uint32_t serialWaitStarted = millis();
            while (!Serial && static_cast<uint32_t>(millis() - serialWaitStarted) < WAIT_TIME_MS) {
                LOOM_FEED_WATCHDOG();
                delay(1);
            }
        }
    } else {
        if (wakeWatchdogMs > 0) {
            Watchdog.disable();
        }
        WD_TIMER_DISABLE;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::restoreModulesAfterSleep(bool wakeModules) {
    FUNCTION_START(this);
    // Use the same voltage gate after standby, an alarm overrun, or an aborted sleep.
    // An error path must not accidentally restart LTE while the battery is charging.
    if (!loomPower::allowModuleWake(wakeModules, rechargePolicy, readBatteryVolts)) {
        disable();
        return false;
    }
    manInst->power_up(wakeWatchdogMs);
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::enableWakeWatchdog() {
    if (wakeWatchdogMs > 0) {
        Watchdog.enable(wakeWatchdogMs);
    } else {
        WD_TIMER_ENABLE;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
TimeSpan Loom_Hypnos::getConfigFromSD(const char *fileName) {
    FUNCTION_START(this);
    // Maximum supported layout is a two-member outer object (timezone + nested interval) and a
    // four-member interval object. Mutable input keeps the strings zero-copy.
    StaticJsonDocument<JSON_OBJECT_SIZE(6)> doc;

    const TimeSpan fallback(0, 0, 20, 0);
    if (sdMan == nullptr) {
        ERROR(F("Attempted to read Hypnos configuration without an SD manager; using 20 minutes."));
        return fallback;
    }

    char *fileRead = sdMan->readFile(fileName);
    if (fileRead == nullptr) {
        ERROR(F("Failed to read Hypnos configuration from SD; using 20 minutes."));
        return fallback;
    }

    char *jsonStart = fileRead;
    const size_t fileLength = strlen(fileRead);
    if (fileLength >= 3 && (uint8_t)fileRead[0] == 0xEF && (uint8_t)fileRead[1] == 0xBB &&
        (uint8_t)fileRead[2] == 0xBF) {
        jsonStart += 3;
    }

    // Mutable input enables zero-copy parsing, so fileRead remains alive until
    // all configuration values have been copied out below.
    DeserializationError deserialError = deserializeJson(doc, jsonStart);

    if (deserialError != DeserializationError::Ok) {
        free(fileRead);
        ERRORF("There was an error reading the config from SD: %s; using 20 minutes.",
               deserialError.c_str());
        return fallback;
    }

    JsonObject json = doc.as<JsonObject>();
    LOG(F("Config successfully loaded from SD!"));

    if (!json["timezone"].isNull()) {
        const char *timezoneStr = json["timezone"].as<const char *>();
        TIME_ZONE configuredZone;
        if (timezoneFromName(timezoneStr, configuredZone)) {
            timezone = configuredZone;
            LOGF("Selected timezone: %s, UTC offset: %i minutes", timezoneStr,
                 timezoneOffsetMinutes(timezone));
        } else {
            WARNINGF("Unknown timezone '%s'; retaining configured timezone.",
                     timezoneStr ? timezoneStr : "(not a string)");
        }
    }

    JsonObject intervalJson = json;
    const char *intervalLayout = "top-level";
    bool intervalFound = json.containsKey("days") || json.containsKey("hours") ||
                         json.containsKey("minutes") || json.containsKey("seconds");
    if (!intervalFound) {
        const char *keys[] = {"SleepInterval", "sleepInterval", "sleep_interval"};
        for (const char *key : keys) {
            if (json[key].is<JsonObject>()) {
                intervalJson = json[key].as<JsonObject>();
                intervalLayout = key;
                intervalFound = true;
                break;
            }
        }
    }

    // Missing individual fields mean zero; supplied fields must be non-negative integers.
    // Accumulate seconds with overflow checks rather than passing wide JSON integers to
    // TimeSpan's narrow days/hours/minutes constructor (hours=256 used to become zero).
    const char *fields[] = {"days", "hours", "minutes", "seconds"};
    const int32_t scales[] = {86400, 3600, 60, 1};
    int32_t totalSeconds = 0;
    bool validInterval = intervalFound;
    for (size_t index = 0; index < 4 && validInterval; ++index) {
        JsonVariant value = intervalJson[fields[index]];
        if (!intervalJson.containsKey(fields[index])) {
            continue;
        }
        validInterval = value.is<int32_t>() &&
                        loomTime::addIntervalPart(value.as<int32_t>(), scales[index], totalSeconds);
    }

    LOGF("Sampling interval layout: %s", intervalFound ? intervalLayout : "missing");
    free(fileRead);

    if (!validInterval || totalSeconds <= 0) {
        ERROR(F("Sampling interval is missing, invalid, zero, or too large; using 20 minutes."));
        return fallback;
    }

    LOGF("Sampling interval loaded from SD: %ld seconds.", (long)totalSeconds);
    FUNCTION_END;
    return TimeSpan(totalSeconds);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::requestReset(const char *reason) {
#if defined(ARDUINO_ARCH_SAMD)
    if (reason == nullptr || reason[0] == '\0') {
        ERROR(F("Reset canceled: supply a reason."));
        return false;
    }
    size_t length = 0;
    while (length < 64 && reason[length] != '\0') {
        ++length;
    }
    if (length == 64) {
        ERROR(F("Reset canceled: reason must fit in 63 bytes."));
        return false;
    }
    LoomWatchdogPause watchdogPause;
    if (enableSD && (sdMan == nullptr || !sdMan->prepareForReset(reason))) {
        return false;
    }
    // Print directly: Logger could issue another SD write after the committed intent.
    Serial.print(F("[MCU RESET REQUEST] "));
    Serial.println(reason);
    NVIC_SystemReset();
#else
    (void)reason;
    ERROR(F("Recorded MCU reset requests are supported only on SAMD."));
#endif
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

/*** SD Stuff ****/

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::logToSD() {
    FUNCTION_START(this);
    if (sdMan == nullptr) {
        ERROR(F("Cannot log to SD because Hypnos SD support is disabled."));
        return false;
    }
    DateTime utc;
    if (!tryGetCurrentTime(utc)) {
        ERROR(F("Cannot log a timed sample because the RTC read failed."));
        return false;
    }
    const bool logged = sdMan->log(utc);
    FUNCTION_END;
    return logged;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

/* Voltage Checks */

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::checkVoltage(float vmin, int analogPin, float scale, bool mv, int num_samples) {
    FUNCTION_START(this);
    if (num_samples <= 0) {
        ERROR(F("Voltage check requires at least one sample."));
        voltage_flags = 0;
        return false;
    }
    analogReadResolution(LOOM_ANALOG_ADC_RESOLUTION_BITS);

    float voltage_sum = 0.0f;

    // Multiple samples for the average voltage
    for (int i = 0; i < num_samples; i++) {
        float voltage = 0.0f;

        if (analogPin == LOOM_ANALOG_BATTERY_PIN) {
            voltage = Loom_Analog::getBatteryVoltage(
                analogPin, LOOM_ANALOG_ADC_RESOLUTION_BITS, LOOM_ANALOG_ADC_REFERENCE_VOLTAGE,
                scale, LOOM_ANALOG_BATTERY_SAMPLE_COUNT, LOOM_ANALOG_ADC_MAX_READING);
        } else {
            float pin_reading = analogRead(analogPin);
            pin_reading *= scale;
            pin_reading *= LOOM_ANALOG_ADC_REFERENCE_VOLTAGE;
            pin_reading /= LOOM_ANALOG_ADC_MAX_READING;
            voltage = pin_reading;
        }

        voltage_sum += voltage;
    }

    float voltage = voltage_sum / num_samples;
    LOGF("Average Voltage: %.2fV", voltage);

    const float comparedVoltage = mv ? voltage * 1000.0f : voltage;
    if (comparedVoltage < vmin) {
        LOGF("Voltage lower than vmin!");
    }

    uint8_t new_flags = VF_CHECKED;

    if (voltage < V_CRITICAL) {
        new_flags |= VF_CRITICAL;
        LOGF("WARNING: Critical voltage (%.2fV < %.2fV) - device will NOT function properly!",
             voltage, V_CRITICAL);
    } else if (voltage < V_DEGRADED) {
        new_flags |= VF_DEGRADED;
        LOGF("WARNING: Degraded voltage (%.2fV < %.2fV) - device may not function properly!",
             voltage, V_DEGRADED);
    } else if (voltage < V_ACCEPTABLE) {
        new_flags |= VF_DEGRADED;
        LOGF("WARNING: Voltage is below the acceptable operating range.");
    } else if (voltage < V_LTE_MIN) {
        new_flags |= VF_ACCEPTABLE;
        LOGF("WARNING: Voltage acceptable for normal operation but may experience issues "
             "transmitting.");
    } else if (voltage < V_OPTIMAL) {
        new_flags |= VF_LTE_READY;
        LOGF("Voltage acceptable for LTE transmission but remains suboptimal.");
    } else {
        new_flags |= VF_OPTIMAL;
        LOGF("Voltage is optimal");
    }

    voltage_flags = new_flags;
    FUNCTION_END;
    return comparedVoltage >= vmin;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
