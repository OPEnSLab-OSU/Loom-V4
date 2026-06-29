#include "Loom_Hypnos.h"
#include "Logger.h"


static uint8_t hypnosMonthFromCompileString(const char* month) {
    if(strncmp(month, "Jan", 3) == 0) return 1;
    if(strncmp(month, "Feb", 3) == 0) return 2;
    if(strncmp(month, "Mar", 3) == 0) return 3;
    if(strncmp(month, "Apr", 3) == 0) return 4;
    if(strncmp(month, "May", 3) == 0) return 5;
    if(strncmp(month, "Jun", 3) == 0) return 6;
    if(strncmp(month, "Jul", 3) == 0) return 7;
    if(strncmp(month, "Aug", 3) == 0) return 8;
    if(strncmp(month, "Sep", 3) == 0) return 9;
    if(strncmp(month, "Oct", 3) == 0) return 10;
    if(strncmp(month, "Nov", 3) == 0) return 11;
    if(strncmp(month, "Dec", 3) == 0) return 12;
    return 1;
}

static bool hypnosTimezoneUsesDST(TIME_ZONE timezone) {
    return timezone == AST || timezone == EST || timezone == CST || timezone == MST || timezone == PST || timezone == AKST;
}

static DateTime hypnosCompileLocalTime(const char* buildDate, const char* buildTime) {
    const char* dateStr = (buildDate != nullptr && buildDate[0] != '\0') ? buildDate : __DATE__;
    const char* timeStr = (buildTime != nullptr && buildTime[0] != '\0') ? buildTime : __TIME__;

    char monthStr[4] = {};
    int day = 1;
    int year = 2000;
    int hour = 0;
    int minute = 0;
    int second = 0;

    sscanf(dateStr, "%3s %d %d", monthStr, &day, &year);
    sscanf(timeStr, "%d:%d:%d", &hour, &minute, &second);

    return DateTime(year, hypnosMonthFromCompileString(monthStr), day, hour, minute, second);
}

static DateTime hypnosCompileTimeUTC(TIME_ZONE timezone, const char* buildDate, const char* buildTime) {
    DateTime compileLocal = hypnosCompileLocalTime(buildDate, buildTime);

    int offsetHours = (int)timezone;
    int offsetMinutes = 0;

    if(hypnosTimezoneUsesDST(timezone) && compileLocal.month() >= 3 && compileLocal.month() < 11){
        offsetHours += 1;
    }

    if(timezone == TIME_ZONE::ACST){
        offsetMinutes = 30;
    }

    // __DATE__ and __TIME__ must be supplied by the sketch before enable().
    // The fallback macros in this .cpp can be stale because Arduino may cache library builds.
    // Store the DS3231 in UTC because getLocalTime() applies the timezone later.
    return compileLocal + TimeSpan(0, -offsetHours, -offsetMinutes, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time, bool useSD) : Module("Hypnos"), manInst(&man), sd_chip_select(version), enableSD(useSD), batch_size(0), custom_time(use_custom_time), timezone(zone){
    memset(sketchCompileDate, '\0', sizeof(sketchCompileDate));
    memset(sketchCompileTime, '\0', sizeof(sketchCompileTime));

    // Set the pins to write mode
    pinMode(5, OUTPUT);                     // 3.3v power rail
    pinMode(6, OUTPUT);                     // 5v power rail
    pinMode(LED_BUILTIN, OUTPUT);           // Status LED

    // Create the SD Manager if we want to use SD
    if(useSD){
        sdMan = new SDManager(manInst, sd_chip_select);
        Logger::getInstance()->setHypnos(this);
    }

    // Create the map of timezone strings to actual timezones
    createTimezoneMap();

    // Add the Hypnos to the module register
    manInst->registerModule(this);
    manInst->useHypnos();   // Enable the use of the hypnos
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::~Loom_Hypnos(){
    if(sdMan != nullptr)
        delete sdMan;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::package(){
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
void Loom_Hypnos::enable(bool enable33, bool enable5){

    // Enable the 3.3v and 5v rails on the Hypnos
    digitalWrite(5, (enable33) ? LOW : HIGH);
    digitalWrite(6, (enable5) ? HIGH : LOW);
    digitalWrite(LED_BUILTIN, HIGH);

    if(enableSD){
        // Enable SPI pins
        pinMode(23, OUTPUT);
        pinMode(24, OUTPUT);
        pinMode(sd_chip_select, OUTPUT);

        sdMan->begin();
    }

    // If the RTC hasn't already been initialized then do so now
    if(!RTC_initialized)
        initializeRTC();

    manInst->setEnableState(true);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::disable(bool disable33, bool disable5){
    // Disable the 3.3v and 5v rails on the Hypnos
    digitalWrite(5, (disable33) ? HIGH : LOW);
    digitalWrite(6, (disable5) ? LOW : HIGH);
    digitalWrite(LED_BUILTIN, LOW);

    if(enableSD){
        // Disable SPI pins/SD chip select to save power
        pinMode(23, INPUT);
        pinMode(24, INPUT);
        pinMode(sd_chip_select, INPUT);
    }

    manInst->setEnableState(false);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::is3VDisabled(DEVICE_STATE deviceState){
   
    switch (deviceState)
    {
        case ENTERING_SLEEP:
            switch (sleepModePowerConfig)
            {
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
            switch (wakeModePowerConfig)
                {
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
bool Loom_Hypnos::is5VDisabled(DEVICE_STATE deviceState){
    switch (deviceState)
    {
        case ENTERING_SLEEP:
            switch (sleepModePowerConfig)
            {
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
            switch (wakeModePowerConfig)
                {
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
bool Loom_Hypnos::registerInterrupt(InterruptCallbackFunction isrFunc, int interruptPin, HypnosInterruptType interruptType, int triggerState){
    FUNCTION_START;
    pinMode(interruptPin, INPUT_PULLUP);  //  Set interrupt pin input mode
    LOG(F("Registering interrupt..."));

    // If the RTC hasn't already been initialized then do so now if we are trying to schedule an RTC interrupt
    if(!RTC_initialized && interruptPin == 12)
        initializeRTC();

    // Make sure a callback function was supplied
    if(isrFunc != nullptr){

         // If the interrupt we registered is for sleep we should set the interrupt to wake the device from sleep
        if(interruptType == SLEEP){
            LowPower.attachInterruptWakeup(interruptPin, isrFunc, triggerState);
            LOG(F("Interrupt successfully attached!"));
        }
        else{
            attachInterrupt(digitalPinToInterrupt(interruptPin), isrFunc, triggerState);
            attachInterrupt(digitalPinToInterrupt(interruptPin), isrFunc, triggerState);
            LOG(F("Interrupt successfully attached!"));
        }
        // Add the interrupt to the list of pin to interrupts
        pinToInterrupt.insert(std::make_pair(interruptPin, std::make_tuple(isrFunc, triggerState, interruptType)));
        FUNCTION_END;
        return true;
    }
    else{
        detachInterrupt(digitalPinToInterrupt(interruptPin));
        ERROR(F("Failed to attach interrupt! Interrupt callback evaluated to a null pointer, it is possible you forgot to supply a callback function"));
        FUNCTION_END;
        return false;
    }
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::reattachRTCInterrupt(int interruptPin){
    FUNCTION_START;
    if(std::get<2>(pinToInterrupt[interruptPin]) != SLEEP){

        // If we haven't previously registered the interrupt we need to do this before we can reattach to an interrupt that doesn't exist
        if(pinToInterrupt.count(interruptPin) <= 0){
            ERROR(F("Failed to reattach interrupt! Interrupt has not previously been registered..."));
            FUNCTION_END;
            return false;
        }

        attachInterrupt(digitalPinToInterrupt(interruptPin), std::get<0>(pinToInterrupt[interruptPin]), std::get<1>(pinToInterrupt[interruptPin]));
        attachInterrupt(digitalPinToInterrupt(interruptPin), std::get<0>(pinToInterrupt[interruptPin]), std::get<1>(pinToInterrupt[interruptPin]));
    }
    else{
        LowPower.attachInterruptWakeup(interruptPin, std::get<0>(pinToInterrupt[interruptPin]), std::get<1>(pinToInterrupt[interruptPin]));
    }
    LOG(F("Interrupt successfully reattached!"));
    FUNCTION_END;
    return true;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::wakeup(){
    detachInterrupt(pinToInterrupt.begin()->first);     // Detach the interrupt so it doesn't trigger again
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::setCompileTime(const char* buildDate, const char* buildTime){
    memset(sketchCompileDate, '\0', sizeof(sketchCompileDate));
    memset(sketchCompileTime, '\0', sizeof(sketchCompileTime));

    if(buildDate != nullptr){
        strncpy(sketchCompileDate, buildDate, sizeof(sketchCompileDate) - 1);
    }

    if(buildTime != nullptr){
        strncpy(sketchCompileTime, buildTime, sizeof(sketchCompileTime) - 1);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::initializeRTC(){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    LOG("Initializing DS3231....");

    // If the RTC failed to start inform the user and hang
    if(!RTC_DS.begin()){
        ERROR(F("Couldn't start RTC! Check your connections... Execution will now hang as this is likely a fatal error"));
        return;
    }

    RTC_initialized = true;

    bool rtcLostPower = RTC_DS.lostPower();

    // If the coin cell died or the DS3231 lost state, keep the existing manual UTC prompt.
    // This preserves field recovery behavior instead of silently accepting a reset RTC time.
    if(rtcLostPower){
        WARNING(F("RTC lost power, let's set the time!"));

        if(Serial){
            set_custom_time();
        }
        else{
            DateTime compileUTC = hypnosCompileTimeUTC(timezone, sketchCompileDate, sketchCompileTime);
            RTC_DS.adjust(compileUTC);
            snprintf(output, OUTPUT_SIZE, "Serial unavailable. RTC set from local compile time as UTC: %s", getCurrentTime().text());
            LOG(output);
        }
    }
    else if(custom_time){
        DateTime compileLocal = hypnosCompileLocalTime(sketchCompileDate, sketchCompileTime);
        DateTime compileUTC = hypnosCompileTimeUTC(timezone, sketchCompileDate, sketchCompileTime);
        DateTime rtcTime = RTC_DS.now();

        snprintf(output, OUTPUT_SIZE, "Compile time local: %s", compileLocal.text());
        LOG(output);

        if(rtcTime.unixtime() < compileUTC.unixtime()){
            RTC_DS.adjust(compileUTC);
            snprintf(output, OUTPUT_SIZE, "RTC set from sketch compile time as UTC: %s", getCurrentTime().text());
            LOG(output);
        }
        else{
            snprintf(output, OUTPUT_SIZE, "RTC already newer than sketch compile time: %s", rtcTime.text());
            LOG(output);
        }
    }

	// Clear any pending alarms
	RTC_DS.clearAlarm();

    RTC_DS.writeSqwPinMode(DS3231_OFF);

    // We successfully started the RTC
    LOG(F("DS3231 Real-Time Clock Initialized Successfully!"));

    DateTime currentUTC = getCurrentTime();
    DateTime currentLocal = getLocalTime(currentUTC);

    snprintf(output, OUTPUT_SIZE, "Current RTC time UTC: %s", currentUTC.text());
    LOG(output);
    snprintf(output, OUTPUT_SIZE, "Current RTC time Local: %s", currentLocal.text());
    LOG(output);
    FUNCTION_END;


}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime Loom_Hypnos::getLocalTime(DateTime time){
    // Add 30 minutes from this zone
    if(timezone == TIME_ZONE::ACST)
        return time + TimeSpan(0, timezone, 30, 0);
    if(isDaylightSavings()){
        return time + TimeSpan(0, (timezone)+1, 0, 0);
    }else{
        return time + TimeSpan(0, (timezone), 0, 0);
    }
}
/////////////////////////////////////// ///////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::isDaylightSavings(){
    // Timezones that observe daylight savings
    if(timezone == AST || timezone == EST || timezone == CST || timezone == AST || timezone == PST || timezone == AKST){
        int currMonth = getCurrentTime().month();

        // If we are in the months where daylight savings is in affect
        return (currMonth >= 3 && currMonth < 11);
    }
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime Loom_Hypnos::getCurrentTime(){
    if(RTC_initialized)
        return RTC_DS.now();
    else{
        LOG(F("Attempted to pull time when RTC was not previously initialized! Returned default datetime"));
        return DateTime();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::networkTimeUpdate(){
    FUNCTION_START;
    bool timeUpdated = false;
    if(networkComponent != nullptr && networkComponent->isConnected()){
        char output[OUTPUT_SIZE];
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        float tz = timezone;

        /* Try twice to set the time if it works break out if not we just og again*/
        for(int i = 0; i < 2; i++){
            LOG("Attempting to set RTC time to the current network time...");

            // Attempt to retrieve the current time from our network component
            if(networkComponent->getNetworkTime(&year, &month, &day, &hour, &minute, &second, &tz)){
                RTC_DS.adjust(DateTime(year, month, day, hour, minute, second));
                snprintf(output, OUTPUT_SIZE, "Network time successfully set to: %s", getCurrentTime().text());
                LOG(output);
                timeUpdated = true;
                break;
            }else{
                ERROR("Failed to get network time! Time has not been set. Retrying...");
            }
        }
    }else{
        ERROR("Network component not set in hypnos or component wasn't connected to the internet.");
    }
    FUNCTION_END;
    return timeUpdated;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::dateTime_toString(DateTime time, char array[21], bool isLocal){

    // Formatted as: YYYY-MM-DDTHH:MM:SSZ
    const unsigned int year = (unsigned int)(time.year() % 10000);
    const unsigned int month = (unsigned int)(time.month() % 100);
    const unsigned int day = (unsigned int)(time.day() % 100);
    const unsigned int hour = (unsigned int)(time.hour() % 100);
    const unsigned int minute = (unsigned int)(time.minute() % 100);
    const unsigned int second = (unsigned int)(time.second() % 100);

    if(isLocal){
        snprintf_P(array, 21, PSTR("%04u-%02u-%02uT%02u:%02u:%02u"), year, month, day, hour, minute, second);
    }else{
        snprintf_P(array, 21, PSTR("%04u-%02u-%02uT%02u:%02u:%02uZ"), year, month, day, hour, minute, second);
    }
   
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::set_custom_time(){
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

	while(computer_year == ""){
		computer_year = Serial.readStringUntil('\n');
	}

    snprintf(output, OUTPUT_SIZE, "Year Entered: %s", computer_year.c_str());
	LOG(output);

	// Entering the month
	LOG(F("Enter the Month (1 ~ 12)"));

	while(computer_month == ""){
		computer_month = Serial.readStringUntil('\n');
	}
    snprintf(output, OUTPUT_SIZE, "Month Entered: %s", computer_month.c_str());
	LOG(output);

	// Entering the day
	LOG(F("Enter the Day (1 ~ 31)"));

	while(computer_day  == ""){
		computer_day = Serial.readStringUntil('\n');
	}
    snprintf(output, OUTPUT_SIZE, "Day Entered: %s", computer_day.c_str());
	LOG(output);


	// Entering the hour
	LOG(F("Enter the Hour (0 ~ 23)"));

	while(computer_hour == ""){
		computer_hour = Serial.readStringUntil('\n');
	}

    snprintf(output, OUTPUT_SIZE, "Hour Entered: %s", computer_hour.c_str());
	LOG(output);

	// Entering the minute
	LOG(F("Enter the Minute (0 ~ 59)"));

	while(computer_min == ""){
		computer_min = Serial.readStringUntil('\n');
	}
    snprintf(output, OUTPUT_SIZE, "Minute Entered: %s", computer_min.c_str());
	LOG(output);

	// Entering the second
	LOG(F("Enter the Second (0 ~ 59)"));
	while(computer_sec == ""){
		computer_sec = Serial.readStringUntil('\n');
	}

    // Set the RTC to the custom time
    RTC_DS.adjust(DateTime(computer_year.toInt(), computer_month.toInt(), computer_day.toInt(), computer_hour.toInt(), computer_min.toInt(), computer_sec.toInt()));
    RTC_initialized = true;

    // Output
    snprintf(output, OUTPUT_SIZE, "Custom time successfully set to: %s", getCurrentTime().text());
	LOG(output);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::setInterruptDuration(const TimeSpan duration){
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    // The time in the future that the alarm will be set for
    DateTime future(RTC_DS.now() + duration);
    RTC_DS.setAlarm(future);

    // Print the time that the next interrupt is set to trigger
    snprintf(output, OUTPUT_SIZE, PSTR("Current Time (Local): %s"), getLocalTime(RTC_DS.now()).text());
    LOG(output);

    snprintf(output, OUTPUT_SIZE, PSTR("Next Interrupt Alarm Set For: %s"), getLocalTime(future).text());
    LOG(output);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Sleep Functionality */

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::sleep(bool waitForSerial){
    // Try to power down the active modules

    // If the alarm set time is less than the current time we missed our next alarm so we need to set a new one, we need to check if we have powered on already so we dont use the RTC that isn't enabled
    bool hasAlarmTriggered = false;

    // Try to power down the active modules
    if(shouldPowerUp){
        manInst->power_down();

        // After powering down the devices check if the alarmed time is less than the current time, this means that the alarm may have already triggered
        uint32_t alarmedTime = RTC_DS.getAlarm(1).unixtime();
        uint32_t currentTime = RTC_DS.now().unixtime();
        hasAlarmTriggered = alarmedTime <= currentTime;
        
        // 50ms delay allows this last message to be sent before the bus disconnects
        LOG("Entering Standby Sleep...");
        delay(50);
    }

    // If it hasn't we should preform our sleep as before
    if(!hasAlarmTriggered){
        pre_sleep();                                            // Pre-sleep cleanup
        shouldPowerUp = true;
        LowPower.sleep();                                       // Go to sleep and hang
        Watchdog.enable(WATCHDOG_TIMEOUT);
    }
    // If it has we want to trigger a resample which requires powering the sensors back up
    else{
        WARNING("Alarm triggered during sample, specified sample duration was too short! Resampling...");
        reattachRTCInterrupt();
        if(shouldPowerUp){
            manInst->power_up();
        }
    }
    Watchdog.reset();

    // If the alarm hadn't triggered last time we want to wake up like normal
    if(!hasAlarmTriggered)
        post_sleep(waitForSerial);         // Wake up
   
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::pre_sleep(){
    bool disable5 = is5VDisabled(DEVICE_STATE::ENTERING_SLEEP);
    bool disable33 = is3VDisabled(DEVICE_STATE::ENTERING_SLEEP);
    delay(1000);

    // Close the serial connection and detach
    Serial.end();
    USBDevice.detach();

    // Reattach the interrupt to the RTC interrupt pin
    attachInterrupt(digitalPinToInterrupt(pinToInterrupt.begin()->first), std::get<0>(pinToInterrupt.begin()->second), std::get<1>(pinToInterrupt.begin()->second));

    // Disable the power rails
    disable(disable33, disable5);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::post_sleep(bool waitForSerial){
    // Enable the Watchdog timer when waking up
    TIMER_ENABLE;
    Watchdog.reset();
    
    if(shouldPowerUp){
        USBDevice.attach();
        Watchdog.reset();
        Serial.begin(115200);
        Watchdog.reset();

        // Check if they are not disabled to see if they should be enabled
        bool enable5 = !is5VDisabled(DEVICE_STATE::EXITING_SLEEP);
        Watchdog.reset();
        bool enable33 = !is3VDisabled(DEVICE_STATE::EXITING_SLEEP);
        Watchdog.reset();

        enable(enable33, enable5); // Checks if the 3.3v or 5v are disabled and re-enables them
        Watchdog.reset();
        delay(1000);
        Watchdog.reset();

        LOG(F("Device has awoken from sleep!"));
        Watchdog.reset();

        // Clear any pending RTC alarms
        RTC_DS.clearAlarm();
        Watchdog.reset();

        // Re-init the modules that need it
        manInst->power_up();

        // We want to wait for the user to re-open the serial monitor before continuing to see readouts
        if(waitForSerial){
            TIMER_DISABLE;
            while(!Serial);
            TIMER_ENABLE;
        }        
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TimeSpan Loom_Hypnos::getConfigFromSD(const char* fileName){
    FUNCTION_START;
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<OUTPUT_SIZE> doc;
    char output[OUTPUT_SIZE];
    char* fileRead = sdMan->readFile(fileName);
    DeserializationError deserialError = deserializeJson(doc, fileRead);
    free(fileRead);

    // Create json object to easily pull data from
    JsonObject json = doc.as<JsonObject>();

    if(deserialError != DeserializationError::Ok){
        snprintf(output, OUTPUT_SIZE, "There was an error reading the config from SD: %s, defaulting sampling interval to 20 minutes.", deserialError.c_str());
        ERROR(output);
        return TimeSpan(0, 0, 20, 0);
    }
    else{
        LOG(F("Config successfully loaded from SD!"));
        if(!json["timezone"].isNull()){
            const char* timezoneStr = json["timezone"].as<const char*>();
            snprintf(output, OUTPUT_SIZE, "Selected timezone: %s, UTC offset: %i", timezoneStr, (int)timezoneMap[(const char*)timezoneStr]);
            LOG(output);
            timezone = timezoneMap[(const char*)timezoneStr];
        }

        // If the sleep interval key is not supplied we want to set some default
        if(!json["SleepInterval"].isNull()){
            // Return the interval as set in the json
            return TimeSpan(json["SleepInterval"]["days"].as<int>(), json["SleepInterval"]["hours"].as<int>(), json["SleepInterval"]["minutes"].as<int>(), json["SleepInterval"]["seconds"].as<int>());
        }
        else{
            snprintf(output, OUTPUT_SIZE, "There was an error reading the sampling interval from SD, defaulting sampling interval to 20 minutes.");
            ERROR(output);
            return TimeSpan(0, 0, 20, 0);
        }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::createTimezoneMap(){
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
    bool logStatus = false;
    if(sdMan != nullptr){
        logStatus = sdMan->log(getCurrentTime());
    }
    else{
        ERROR(F("Attempted to log to SD without an SD manager."));
    }
    FUNCTION_END;
    return logStatus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
