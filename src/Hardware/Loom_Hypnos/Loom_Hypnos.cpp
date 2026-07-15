#include "Loom_Hypnos.h"
#include "Logger.h"

namespace {
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
    for (uint8_t i = 0; i < 12; ++i)
        if (strncmp(month, months[i], 3) == 0)
            return i + 1;
    return 1;
}

DateTime compileLocalTime(const char *buildDate, const char *buildTime) {
    const char *date = buildDate && buildDate[0] ? buildDate : __DATE__;
    const char *time = buildTime && buildTime[0] ? buildTime : __TIME__;
    char month[4] = {};
    int day = 1, year = 2000, hour = 0, minute = 0, second = 0;
    sscanf(date, "%3s %d %d", month, &day, &year);
    sscanf(time, "%d:%d:%d", &hour, &minute, &second);
    return DateTime(year, compileMonth(month), day, hour, minute, second);
}

bool timezoneUsesDST(TIME_ZONE zone) {
    return zone == AST || zone == EST || zone == CST || zone == MST || zone == PST ||
           zone == AKST;
}

DateTime compileUtcTime(TIME_ZONE zone, const char *buildDate, const char *buildTime) {
    DateTime local = compileLocalTime(buildDate, buildTime);
    int hours = static_cast<int>(zone);
    int minutes = zone == ACST ? 30 : 0;
    if (timezoneUsesDST(zone) && local.month() >= 3 && local.month() < 11)
        hours++;
    return local + TimeSpan(0, -hours, -minutes, 0);
}
} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::Loom_Hypnos(Manager &man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time,
                         bool useSD)
    : Module("Hypnos"), manInst(&man), sd_chip_select(version), enableSD(useSD), batch_size(0),
      custom_time(use_custom_time), timezone(zone) {

    // Set the pins to write mode
    pinMode(5, OUTPUT);           // 3.3v power rail
    pinMode(6, OUTPUT);           // 5v power rail
    pinMode(LED_BUILTIN, OUTPUT); // Status LED

    // Create the SD Manager if we want to use SD
    if (useSD) {
        sdMan = new SDManager(manInst, sd_chip_select);
        Logger::getInstance()->setHypnos(this);
    }

    // Create the map of timezone strings to actual timezones
    createTimezoneMap();

    // Add the Hypnos to the module register
    manInst->registerModule(this);
    manInst->useHypnos(); // Enable the use of the hypnos
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::~Loom_Hypnos() {
    if (sdMan != nullptr)
        delete sdMan;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::package() {
    JsonObject json = manInst->getDocument().createNestedObject("timestamp");
    char timeStr[21];
    char localStr[21];

    time = getCurrentTime();
    localTime = getLocalTime(time);

    dateTime_toString(time, timeStr);
    json["time_utc"] = timeStr;

    dateTime_toString(localTime, localStr, true);
    json["time_local"] = localStr;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Power Rail Control Functionality */

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::enable(bool enable33, bool enable5) {

    // Enable the 3.3v and 5v rails on the Hypnos
    digitalWrite(5, (enable33) ? LOW : HIGH);
    digitalWrite(6, (enable5) ? HIGH : LOW);
    digitalWrite(LED_BUILTIN, HIGH);

    if (enableSD) {
        // Enable SPI pins
        pinMode(23, OUTPUT);
        pinMode(24, OUTPUT);
        pinMode(sd_chip_select, OUTPUT);

        sdMan->begin();
    }

    // If the RTC hasn't already been initialized then do so now
    if (!RTC_initialized)
        initializeRTC();

    manInst->setEnableState(true);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::disable(bool disable33, bool disable5) {
    // Disable the 3.3v and 5v rails on the Hypnos
    digitalWrite(5, (disable33) ? HIGH : LOW);
    digitalWrite(6, (disable5) ? LOW : HIGH);
    digitalWrite(LED_BUILTIN, LOW);

    if (enableSD) {
        // Disable SPI pins/SD chip select to save power
        pinMode(23, INPUT);
        pinMode(24, INPUT);
        pinMode(sd_chip_select, INPUT);
    }

    manInst->setEnableState(false);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::is3VDisabled(DEVICE_STATE deviceState) {

    switch (deviceState) {
    case ENTERING_SLEEP:
        switch (sleepModePowerConfig) {
        case PR_3V_ON_5V_ON:
            return false;
        case PR_3V_ON_5V_OFF:
            return false;
        case PR_3V_OFF_5V_ON:
            return true;
        case PR_3V_OFF_5V_OFF:
            return true;
        }
        break;
    case EXITING_SLEEP:
        switch (wakeModePowerConfig) {
        case PR_3V_ON_5V_ON:
            return false;
        case PR_3V_ON_5V_OFF:
            return false;
        case PR_3V_OFF_5V_ON:
            return true;
        case PR_3V_OFF_5V_OFF:
            return true;
        }
        break;
    }

    // We should never make it here but enable the rail if we do
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::is5VDisabled(DEVICE_STATE deviceState) {
    switch (deviceState) {
    case ENTERING_SLEEP:
        switch (sleepModePowerConfig) {
        case PR_3V_ON_5V_ON:
            return false;
        case PR_3V_OFF_5V_ON:
            return false;
        case PR_3V_ON_5V_OFF:
            return true;
        case PR_3V_OFF_5V_OFF:
            return true;
        }
        break;
    case EXITING_SLEEP:
        switch (wakeModePowerConfig) {
        case PR_3V_ON_5V_ON:
            return false;
        case PR_3V_OFF_5V_ON:
            return false;
        case PR_3V_ON_5V_OFF:
            return true;
        case PR_3V_OFF_5V_OFF:
            return true;
        }
        break;
    }

    // We should never make it here but enable the rail if we do
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Interrupt Functionality */

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::registerInterrupt(InterruptCallbackFunction isrFunc, int interruptPin,
                                    HypnosInterruptType interruptType, int triggerState) {
    FUNCTION_START;
    pinMode(interruptPin, INPUT_PULLUP); //  Set interrupt pin input mode
    LOG(F("Registering interrupt..."));

    // If the RTC hasn't already been initialized then do so now if we are trying to schedule an RTC
    // interrupt
    if (!RTC_initialized && interruptPin == 12)
        initializeRTC();

    // Make sure a callback function was supplied
    if (isrFunc != nullptr) {

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
        // Add the interrupt to the list of pin to interrupts
        // Assignment intentionally replaces an older registration for this pin.
        pinToInterrupt[interruptPin] = std::make_tuple(isrFunc, triggerState, interruptType);
        FUNCTION_END;
        return true;
    } else {
        detachInterrupt(digitalPinToInterrupt(interruptPin));
        ERROR(F("Failed to attach interrupt! Interrupt callback evaluated to a null pointer, it is "
                "possible you forgot to supply a callback function"));
        FUNCTION_END;
        return false;
    }
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::reattachRTCInterrupt(int interruptPin) {
    FUNCTION_START;
    auto registered = pinToInterrupt.find(interruptPin);
    if (registered == pinToInterrupt.end()) {
        ERROR(F("Failed to reattach interrupt! Interrupt has not previously been registered..."));
        FUNCTION_END;
        return false;
    }

    const HypnosInterruptType interruptType = std::get<2>(registered->second);
    if (interruptType == SLEEP && interruptPin == 12 && digitalRead(interruptPin) == LOW) {
        // DS3231 alarms are active-low level interrupts. Attaching while INT is
        // already low immediately invokes the callback and can turn a stale RTC
        // flag into a duplicate wake event. sleep() handles a genuinely elapsed
        // alarm through its overrun path instead.
        LOG(F("RTC alarm is already active; deferring callback to the sleep overrun handler."));
        FUNCTION_END;
        return true;
    }

    clearPendingExternalInterrupt(interruptPin);
    if (interruptType != SLEEP) {

        attachInterrupt(digitalPinToInterrupt(interruptPin),
                        std::get<0>(registered->second), std::get<1>(registered->second));
    } else {
        LowPower.attachInterruptWakeup(interruptPin, std::get<0>(registered->second),
                                       std::get<1>(registered->second));
    }
    LOG(F("Interrupt successfully reattached!"));
    FUNCTION_END;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::wakeup() {
    if (sleepInterruptPin >= 0)
        detachInterrupt(digitalPinToInterrupt(sleepInterruptPin));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

void Loom_Hypnos::setCompileTime(const char *buildDate, const char *buildTime) {
    strncpy(sketchCompileDate, buildDate ? buildDate : "", sizeof(sketchCompileDate) - 1);
    strncpy(sketchCompileTime, buildTime ? buildTime : "", sizeof(sketchCompileTime) - 1);
    sketchCompileDate[sizeof(sketchCompileDate) - 1] = '\0';
    sketchCompileTime[sizeof(sketchCompileTime) - 1] = '\0';
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::initializeRTC() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    LOG("Initializing DS3231....");

    // If the RTC failed to start inform the user and hang
    if (!RTC_DS.begin()) {
        ERROR(F("Couldn't start RTC! Check your connections... Execution will now hang as this is "
                "likely a fatal error"));
        return;
    }

    // This may end up causing a problem in practice - what if RTC loses power in field? Shouldn't
    // happen with coin cell batt backup
    const DateTime compileUTC = compileUtcTime(timezone, sketchCompileDate, sketchCompileTime);
    if (RTC_DS.lostPower()) {
        WARNING(F("RTC lost power, let's set the time!"));

        if (sketchCompileDate[0] != '\0') {
            RTC_DS.adjust(compileUTC);
        } else if (custom_time && Serial) {
            set_custom_time();
        } else {
            RTC_DS.adjust(compileUTC);
            if (sketchCompileDate[0] == '\0')
                WARNING(F("RTC used the library compile timestamp; call setCompileTime(__DATE__, "
                          "__TIME__) before enable() to avoid cached timestamps."));
        }
    } else if (sketchCompileDate[0] != '\0' &&
               RTC_DS.now().unixtime() < compileUTC.unixtime()) {
        RTC_DS.adjust(compileUTC);
    }

    // Clear any pending alarms
    RTC_DS.clearAlarm(1);
    RTC_DS.clearAlarm(2);

    RTC_DS.writeSqwPinMode(DS3231_OFF);

    // We successfully started the RTC
    LOG(F("DS3231 Real-Time Clock Initialized Successfully!"));
    RTC_initialized = true;
    DateTime t = getCurrentTime();
    char tbuf[21];
    dateTime_toString(t, tbuf);
    snprintf(output, OUTPUT_SIZE, "Custom time successfully set to: %s", tbuf);
    LOG(output);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime Loom_Hypnos::getLocalTime(DateTime time) {
    // Add 30 minutes from this zone
    if (timezone == TIME_ZONE::ACST)
        return time + TimeSpan(0, timezone, 30, 0);
    if (isDaylightSavings()) {
        return time + TimeSpan(0, (timezone) + 1, 0, 0);
    } else {
        return time + TimeSpan(0, (timezone), 0, 0);
    }
}
///////////////////////////////////////
//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::isDaylightSavings() {
    // Timezones that observe daylight savings
    if (timezone == AST || timezone == EST || timezone == CST || timezone == MST ||
        timezone == PST || timezone == AKST) {
        int currMonth = getCurrentTime().month();

        // If we are in the months where daylight savings is in affect
        return (currMonth >= 3 && currMonth < 11);
    }
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime Loom_Hypnos::getCurrentTime() {
    if (RTC_initialized)
        return RTC_DS.now();
    else {
        LOG(F("Attempted to pull time when RTC was not previously initialized! Returned default "
              "datetime"));
        return DateTime();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::networkTimeUpdate() {
    FUNCTION_START;
    bool updated = false;
    if (networkComponent != nullptr && networkComponent->isConnected()) {
        char output[OUTPUT_SIZE];
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        float tz = timezone;

        /* Try twice to set the time if it works break out if not we just og again*/
        for (int i = 0; i < 2; i++) {
            LOG("Attempting to set RTC time to the current network time...");

            // Attempt to retrieve the current time from our network component
            if (networkComponent->getNetworkTime(&year, &month, &day, &hour, &minute, &second,
                                                 &tz)) {
                RTC_DS.adjust(DateTime(year, month, day, hour, minute, second));
                DateTime t = getCurrentTime();
                char tbuf[21];
                dateTime_toString(t, tbuf);
                snprintf(output, OUTPUT_SIZE, "Network time successfully set to: %s", tbuf);
                LOG(output);
                updated = true;
                break;
            } else {
                ERROR("Failed to get network time! Time has not been set. Retrying...");
            }
        }
    } else {
        ERROR("Network component not set in hypnos or component wasn't connected to the internet.");
    }
    FUNCTION_END;
    return updated;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::dateTime_toString(DateTime time, char array[21], bool isLocal) {

    // Formatted as: YYYY-MM-DDTHH:MM:SSZ
    const unsigned int year = static_cast<unsigned int>(time.year() % 10000);
    const unsigned int month = static_cast<unsigned int>(time.month() % 100);
    const unsigned int day = static_cast<unsigned int>(time.day() % 100);
    const unsigned int hour = static_cast<unsigned int>(time.hour() % 100);
    const unsigned int minute = static_cast<unsigned int>(time.minute() % 100);
    const unsigned int second = static_cast<unsigned int>(time.second() % 100);

    if (isLocal) {
        snprintf_P(array, 21, PSTR("%04u-%02u-%02uT%02u:%02u:%02u"), year, month, day, hour,
                   minute, second);
    } else {
        snprintf_P(array, 21, PSTR("%04u-%02u-%02uT%02u:%02u:%02uZ"), year, month, day, hour,
                   minute, second);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::set_custom_time() {
    FUNCTION_START;

    // initialized variable for user input
    String computer_year = "";
    String computer_month = "";
    String computer_day = "";
    String computer_hour = "";
    String computer_min = "";
    String computer_sec = "";
    char output[OUTPUT_SIZE];

    // Let the user know that they should enter local time
    LOG(F("Please use UTC time, not local!"));

    // Entering the year
    LOG(F("Enter the Year (Four digits, e.g. 2020)"));

    while (computer_year == "") {
        computer_year = Serial.readStringUntil('\n');
    }

    snprintf(output, OUTPUT_SIZE, "Year Entered: %s", computer_year.c_str());
    LOG(output);

    // Entering the month
    LOG(F("Enter the Month (1 ~ 12)"));

    while (computer_month == "") {
        computer_month = Serial.readStringUntil('\n');
    }
    snprintf(output, OUTPUT_SIZE, "Month Entered: %s", computer_month.c_str());
    LOG(output);

    // Entering the day
    LOG(F("Enter the Day (1 ~ 31)"));

    while (computer_day == "") {
        computer_day = Serial.readStringUntil('\n');
    }
    snprintf(output, OUTPUT_SIZE, "Day Entered: %s", computer_day.c_str());
    LOG(output);

    // Entering the hour
    LOG(F("Enter the Hour (0 ~ 23)"));

    while (computer_hour == "") {
        computer_hour = Serial.readStringUntil('\n');
    }

    snprintf(output, OUTPUT_SIZE, "Hour Entered: %s", computer_hour.c_str());
    LOG(output);

    // Entering the minute
    LOG(F("Enter the Minute (0 ~ 59)"));

    while (computer_min == "") {
        computer_min = Serial.readStringUntil('\n');
    }
    snprintf(output, OUTPUT_SIZE, "Minute Entered: %s", computer_min.c_str());
    LOG(output);

    // Entering the second
    LOG(F("Enter the Second (0 ~ 59)"));
    while (computer_sec == "") {
        computer_sec = Serial.readStringUntil('\n');
    }

    // Set the RTC to the custom time
    RTC_DS.adjust(DateTime(computer_year.toInt(), computer_month.toInt(), computer_day.toInt(),
                           computer_hour.toInt(), computer_min.toInt(), computer_sec.toInt()));
    RTC_initialized = true;

    // Output
    DateTime t = getCurrentTime();
    char tbuf[21];
    dateTime_toString(t, tbuf);
    snprintf(output, OUTPUT_SIZE, "Custom time successfully set to: %s", tbuf);
    LOG(output);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::setInterruptDuration(const TimeSpan duration) {
    FUNCTION_START;

    if (!RTC_initialized) {
        ERROR(F("Cannot set an RTC alarm before the RTC is initialized."));
        alarmScheduled = false;
        FUNCTION_END;
        return;
    }

    if (duration.totalseconds() <= 0) {
        ERROR(F("RTC alarm duration must be greater than zero."));
        alarmScheduled = false;
        FUNCTION_END;
        return;
    }

    // RTClib's setAlarm1() programs and enables Alarm 1, but it does not clear
    // a previously fired A1F status bit. If A1F remains set, the DS3231 INT pin
    // stays LOW and reattaching the level interrupt immediately retriggers the
    // old alarm. Disable and clear both alarm sources before replacement so
    // neither can hold the shared INT/SQW line low.
    RTC_DS.disableAlarm(1);
    RTC_DS.disableAlarm(2);
    RTC_DS.clearAlarm(1);
    RTC_DS.clearAlarm(2);
    if (sleepInterruptPin >= 0)
        clearPendingExternalInterrupt(sleepInterruptPin);

    // The time in the future that the alarm will be set for
    alarmTime = RTC_DS.now() + duration;
    alarmScheduled = RTC_DS.setAlarm1(alarmTime, DS3231_A1_Date);
    if (!alarmScheduled) {
        ERROR(F("Failed to set RTC alarm 1."));
        FUNCTION_END;
        return;
    }

    // Clear a match flag that may have become pending while the alarm registers
    // were being replaced. This mirrors the proven pre-RTClib Hypnos sequence.
    RTC_DS.clearAlarm(1);
    if (sleepInterruptPin >= 0)
        clearPendingExternalInterrupt(sleepInterruptPin);

    // Print the time that the next interrupt is set to trigger
    DateTime t = getLocalTime(RTC_DS.now());
    char tbuf[21];
    dateTime_toString(t, tbuf);
    LOGF("Current Time (Local): %s", tbuf, true);
    t = getLocalTime(alarmTime);
    dateTime_toString(t, tbuf);
    LOGF("Next interrupt alarm set for: %s", tbuf, true);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Sleep Functionality */

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::sleep(bool waitForSerial) {

    if (sleepInterruptPin < 0) {
        ERROR(F("Sleep aborted because no SLEEP interrupt is registered."));
        return;
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
        DateTime now = RTC_DS.now();
        hasAlarmTriggered = alarmScheduled && alarmTime.unixtime() <= now.unixtime();

        // 50ms delay allows this last message to be sent before the bus disconnects
        LOG("Entering Standby Sleep...");
        delay(50);
    }

    // If it hasn't we should preform our sleep as before
    if (!hasAlarmTriggered) {
        pre_sleep(); // Pre-sleep cleanup
        shouldPowerUp = true;
        LowPower.sleep(); // Go to sleep and hang
        WD_TIMER_ENABLE;
    }
    // If it has we want to trigger a resample which requires powering the sensors back up
    else {
        WARNING("Alarm triggered during sample, specified sample duration was too short! "
                "Resampling...");
            RTC_DS.clearAlarm(1);
            alarmScheduled = false;
        if (sleepInterruptPin >= 0)
            clearPendingExternalInterrupt(sleepInterruptPin);
        if (shouldPowerUp) {
            manInst->power_up();
        }
        auto registered = pinToInterrupt.find(sleepInterruptPin);
        if (registered != pinToInterrupt.end()) {
            InterruptCallbackFunction callback = std::get<0>(registered->second);
            if (callback != nullptr)
                callback();
        }
    }
    WD_TIMER_RESET;

    // If the alarm hadn't triggered last time we want to wake up like normal
    if (!hasAlarmTriggered) {
        post_sleep(waitForSerial); // Wake up
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::pre_sleep() {
    bool disable5 = is5VDisabled(DEVICE_STATE::ENTERING_SLEEP);
    bool disable33 = is3VDisabled(DEVICE_STATE::ENTERING_SLEEP);
    delay(1000);

    // Close the serial connection and detach
    Serial.end();
    USBDevice.detach();

    if (sleepInterruptPin >= 0)
        reattachRTCInterrupt(sleepInterruptPin);
    else
        WARNING(F("Entering sleep without a registered SLEEP interrupt."));

    // Disable the power rails
    disable(disable33, disable5);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::post_sleep(bool waitForSerial) {
    // Enable the Watchdog timer when waking up
    WD_TIMER_ENABLE;
    WD_TIMER_RESET;

    if (shouldPowerUp) {
        USBDevice.attach();
        WD_TIMER_RESET;
        Serial.begin(115200);
        WD_TIMER_RESET;

        // Check if they are not disabled to see if they should be enabled
        bool enable5 = !is5VDisabled(DEVICE_STATE::EXITING_SLEEP);
        WD_TIMER_RESET;
        bool enable33 = !is3VDisabled(DEVICE_STATE::EXITING_SLEEP);
        WD_TIMER_RESET;

        enable(enable33, enable5); // Checks if the 3.3v or 5v are disabled and re-enables them
        WD_TIMER_RESET;
        delay(1000);
        WD_TIMER_RESET;

        LOG(F("Device has awoken from sleep!"));
        WD_TIMER_RESET;

        // Clear any pending RTC alarms
        RTC_DS.clearAlarm(1);
        RTC_DS.clearAlarm(2);
        alarmScheduled = false;
        WD_TIMER_RESET;

        // Re-init the modules that need it
        manInst->power_up();

        // We want to wait for the user to re-open the serial monitor before continuing to see
        // readouts
        if (waitForSerial) {
            WD_TIMER_DISABLE;
            while (!Serial)
                ;
            WD_TIMER_ENABLE;
        }
    } else {
        WD_TIMER_DISABLE;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TimeSpan Loom_Hypnos::getConfigFromSD(const char *fileName) {
    FUNCTION_START;
    StaticJsonDocument<OUTPUT_SIZE> doc;
    char output[OUTPUT_SIZE];

    const TimeSpan fallback(0, 0, 20, 0);
    if (sdMan == nullptr) {
        ERROR(F("Attempted to read Hypnos configuration without an SD manager; using 20 minutes."));
        FUNCTION_END;
        return fallback;
    }

    char *fileRead = sdMan->readFile(fileName);
    if (fileRead == nullptr) {
        ERROR(F("Failed to read Hypnos configuration from SD; using 20 minutes."));
        FUNCTION_END;
        return fallback;
    }

    char *jsonStart = fileRead;
    const size_t fileLength = strlen(fileRead);
    if (fileLength >= 3 && (uint8_t)fileRead[0] == 0xEF &&
        (uint8_t)fileRead[1] == 0xBB && (uint8_t)fileRead[2] == 0xBF)
        jsonStart += 3;

    // Mutable input enables zero-copy parsing, so fileRead remains alive until
    // all configuration values have been copied out below.
    DeserializationError deserialError = deserializeJson(doc, jsonStart);

    if (deserialError != DeserializationError::Ok) {
        free(fileRead);
        snprintf(output, OUTPUT_SIZE,
                 "There was an error reading the config from SD: %s; using 20 minutes.",
                 deserialError.c_str());
        ERROR(output);
        FUNCTION_END;
        return fallback;
    }

    JsonObject json = doc.as<JsonObject>();
    LOG(F("Config successfully loaded from SD!"));

    if (!json["timezone"].isNull()) {
        const char *timezoneStr = json["timezone"].as<const char *>();
        auto entry = timezoneMap.find(timezoneStr);
        if (entry != timezoneMap.end()) {
            timezone = entry->second;
            LOGF("Selected timezone: %s, UTC offset: %i", timezoneStr, (int)timezone);
        } else {
            WARNINGF("Unknown timezone '%s'; retaining configured timezone.", timezoneStr);
        }
    }

    JsonObject intervalJson = json;
    bool intervalFound = json.containsKey("days") || json.containsKey("hours") ||
                         json.containsKey("minutes") || json.containsKey("seconds");
    if (!intervalFound) {
        const char *keys[] = {"SleepInterval", "sleepInterval", "sleep_interval"};
        for (const char *key : keys) {
            if (json[key].is<JsonObject>()) {
                intervalJson = json[key].as<JsonObject>();
                intervalFound = true;
                break;
            }
        }
    }

    const int days = intervalFound ? intervalJson["days"].as<int>() : 0;
    const int hours = intervalFound ? intervalJson["hours"].as<int>() : 0;
    const int minutes = intervalFound ? intervalJson["minutes"].as<int>() : 0;
    const int seconds = intervalFound ? intervalJson["seconds"].as<int>() : 0;
    const TimeSpan interval(days, hours, minutes, seconds);
    free(fileRead);

    if (!intervalFound || interval.totalseconds() <= 0) {
        ERROR(F("Sampling interval is missing or zero; using 20 minutes."));
        FUNCTION_END;
        return fallback;
    }

    LOGF("Sampling interval loaded from SD: %ld seconds.", (long)interval.totalseconds());
    FUNCTION_END;
    return interval;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::createTimezoneMap() {
    timezoneMap.insert(std::make_pair("WAT", TIME_ZONE::WAT));
    timezoneMap.insert(std::make_pair("AT", TIME_ZONE::AT));
    timezoneMap.insert(std::make_pair("AST", TIME_ZONE::AST));
    timezoneMap.insert(std::make_pair("EST", TIME_ZONE::EST));
    timezoneMap.insert(std::make_pair("CST", TIME_ZONE::CST));
    timezoneMap.insert(std::make_pair("MST", TIME_ZONE::MST));
    timezoneMap.insert(std::make_pair("PST", TIME_ZONE::PST));
    timezoneMap.insert(std::make_pair("AKST", TIME_ZONE::AKST));
    timezoneMap.insert(std::make_pair("HST", TIME_ZONE::HST));
    timezoneMap.insert(std::make_pair("SST", TIME_ZONE::SST));
    timezoneMap.insert(std::make_pair("GMT", TIME_ZONE::GMT));
    timezoneMap.insert(std::make_pair("BST", TIME_ZONE::BST));
    timezoneMap.insert(std::make_pair("CET", TIME_ZONE::CET));
    timezoneMap.insert(std::make_pair("EET", TIME_ZONE::EET));
    timezoneMap.insert(std::make_pair("EEST", TIME_ZONE::EEST));
    timezoneMap.insert(std::make_pair("BRT", TIME_ZONE::BRT));
    timezoneMap.insert(std::make_pair("ZP4", TIME_ZONE::ZP4));
    timezoneMap.insert(std::make_pair("ZP5", TIME_ZONE::ZP5));
    timezoneMap.insert(std::make_pair("ZP6", TIME_ZONE::ZP6));
    timezoneMap.insert(std::make_pair("ZP7", TIME_ZONE::ZP7));
    timezoneMap.insert(std::make_pair("AWST", TIME_ZONE::AWST));
    timezoneMap.insert(std::make_pair("ACST", TIME_ZONE::ACST));
    timezoneMap.insert(std::make_pair("AEST", TIME_ZONE::AEST));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

/*** SD Stuff ****/

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::logToSD() {
    FUNCTION_START;
    if (sdMan == nullptr) {
        ERROR(F("Cannot log to SD because Hypnos SD support is disabled."));
        FUNCTION_END;
        return false;
    }
    bool logged = sdMan->log(getCurrentTime());
    FUNCTION_END;
    return logged;
}

/* Voltage Checks */

bool Loom_Hypnos::checkVoltage(float vmin, int analogPin, float scale, bool mv, int num_samples) {
    INSTRUMENT();
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
                analogPin, LOOM_ANALOG_ADC_RESOLUTION_BITS,
                LOOM_ANALOG_ADC_REFERENCE_VOLTAGE, scale,
                LOOM_ANALOG_BATTERY_SAMPLE_COUNT, LOOM_ANALOG_ADC_MAX_READING);
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
    return comparedVoltage >= vmin;
}
