#include "Loom_Hypnos.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time, bool useSD) : Module("Hypnos"), custom_time(use_custom_time), sd_chip_select(version), enableSD(useSD), timezone(zone){
    manInst = &man;

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

    dateTime_toString(localTime, localStr);
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
void Loom_Hypnos::initializeRTC(){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    LOG("Initializing DS3231....");

    // If the RTC failed to start inform the user and hang
    if(!RTC_DS.begin()){
        ERROR(F("Couldn't start RTC! Check your connections... Execution will now hang as this is likely a fatal error"));
        return;
    }

    // This may end up causing a problem in practice - what if RTC loses power in field? Shouldn't happen with coin cell batt backup
	if (RTC_DS.lostPower()) {
		WARNING(F("RTC lost power, let's set the time!"));

        // If we want to set a custom time
        if(Serial){
            set_custom_time();
        }
	}

	// Clear any pending alarms
	RTC_DS.clearAlarm();

    RTC_DS.writeSqwPinMode(DS3231_OFF);

    // We successfully started the RTC
    LOG(F("DS3231 Real-Time Clock Initialized Successfully!"));
    RTC_initialized = true;
    snprintf(output, OUTPUT_SIZE, "Custom time successfully set to: %s", getCurrentTime().text());
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
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::isDaylightSavings(){
    // Timezones that observe daylight savings
    if(timezone == AST || timezone == EST || timezone == CST || timezone == AST || timezone == PST || timezone == AKST){
        int currMonth = getCurrentTime().month();
        Serial.println(currMonth);

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
    if(networkComponent != nullptr && networkComponent->isConnected()){
        char output[OUTPUT_SIZE];
        int year, month, day, hour, minute, second = 0;
        float tz = timezone;

        /* Try twice to set the time if it works break out if not we just og again*/
        for(int i = 0; i < 2; i++){
            LOG("Attempting to set RTC time to the current network time...");

            // Attempt to retrieve the current time from our network component
            if(networkComponent->getNetworkTime(&year, &month, &day, &hour, &minute, &second, &tz)){
                RTC_DS.adjust(DateTime(year, month, day, hour, minute, second));
                snprintf(output, OUTPUT_SIZE, "Network time successfully set to: %s", getCurrentTime().text());
                LOG(output);
                break;
            }else{
                ERROR("Failed to get network time! Time has not been set. Retrying...");
            }
        }
    }else{
        ERROR("Network component not set in hypnos or component wasn't connected to the internet.");
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::dateTime_toString(DateTime time, char array[21]){

    // Formatted as: YYYY-MM-DDTHH:MM:SSZ
    snprintf_P(array, 21, PSTR("%u-%02u-%02uT%u:%u:%uZ"), time.year(), time.month(), time.day(), time.hour(), time.minute(), time.second());
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
void Loom_Hypnos::sleep(bool waitForSerial, bool disable33, bool disable5){
    // Try to power down the active modules

    // If the alarm set time is less than the current time we missed our next alarm so we need to set a new one, we need to check if we have powered on already so we dont use the RTC that isn't enabled
    bool hasAlarmTriggered = false;

    // Try to power down the active modules
    if(shouldPowerUp){
        manInst->power_down();

        // 50ms delay allows this last message to be sent before the bus disconnects
        LOG("Entering Standby Sleep...");
        delay(50);

        // After powering down the devices check if the alarmed time is less than the current time, this means that the alarm may have already triggered
        uint32_t alarmedTime = RTC_DS.getAlarm(1).unixtime();
        uint32_t currentTime = RTC_DS.now().unixtime();
        hasAlarmTriggered = alarmedTime <= currentTime;
    }

    // If it hasn't we should preform our sleep as before
    if(!hasAlarmTriggered){
        pre_sleep(disable33, disable5);                         // Pre-sleep cleanup
        shouldPowerUp = true;
        LowPower.sleep();                                       // Go to sleep and hang
    }
    // If it has we want to trigger a resample which requires powering the sensors back up
    else{
        WARNING("Alarm triggered during sample, specified sample duration was too short! Resampling...");
        reattachRTCInterrupt();
        if(shouldPowerUp){
            manInst->power_up();
        }
    }

    // If the alarm hadn't triggered last time we want to wake up like normal
    if(!hasAlarmTriggered)
        post_sleep(waitForSerial, disable33, disable5);         // Wake up
   
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::pre_sleep(bool disable33, bool disable5){
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
void Loom_Hypnos::post_sleep(bool waitForSerial, bool disable33, bool disable5){
    // Enable the Watchdog timer when waking up
    TIMER_ENABLE;
    Serial.println(shouldPowerUp);
    if(shouldPowerUp){
        USBDevice.attach();
        Serial.begin(115200);
        Serial.println("Before enable");

        enable(disable33, disable5); // Checks if the 3.3v or 5v are disabled and re-enables them

        Serial.println("After  enable");

        // Clear any pending RTC alarms
        RTC_DS.clearAlarm();

        // Re-init the modules that need it
        manInst->power_up();

        // We want to wait for the user to re-open the serial monitor before continuing to see readouts
        if(waitForSerial){
            TIMER_DISABLE;
            while(!Serial);
            TIMER_ENABLE;
        }

        LOG(F("Device has awoken from sleep!"));
    }
    Serial.println(shouldPowerUp);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TimeSpan Loom_Hypnos::getSleepIntervalFromSD(const char* fileName){
    FUNCTION_START;
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<255> doc;
    char output[OUTPUT_SIZE];
    char* fileRead = sdMan->readFile(fileName);
    DeserializationError deserialError = deserializeJson(doc, fileRead);
    free(fileRead);

    // Create json object to easily pull data from
    JsonObject json = doc.as<JsonObject>();

    if(deserialError != DeserializationError::Ok){
        snprintf(output, OUTPUT_SIZE, "There was an error reading the sleep interval from SD: %s, defaulting sampling interval to 20 minutes.", deserialError.c_str());
        ERROR(output);
        return TimeSpan(0, 0, 20, 0);
    }
    else{
        LOG(F("Sleep interval successfully loaded from SD!"));
        // Return the interval as set in the json
        return TimeSpan(json["days"].as<int>(), json["hours"].as<int>(), json["minutes"].as<int>(), json["seconds"].as<int>());
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::getTimeZoneFromSD(const char* fileName){
    FUNCTION_START;
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<255> doc;
    char* fileRead = sdMan->readFile(fileName);
    DeserializationError deserialError = deserializeJson(doc, fileRead);
    free(fileRead);

    // Create json object to easily pull data from
    JsonObject json = doc.as<JsonObject>();

    if(deserialError != DeserializationError::Ok){
        ERROR(F("There was an error reading the timezone defaulting to programmed timezone"));
    }
    else{
        if(!json["timezone"].isNull())
            timezone = timezoneMap[json["timezone"].as<const char*>()];
        LOG(F("Timezone successfully loaded!"));

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
    sdMan->log(getCurrentTime());
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
