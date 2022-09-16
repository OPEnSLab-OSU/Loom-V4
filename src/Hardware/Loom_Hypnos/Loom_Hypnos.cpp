#include "Loom_Hypnos.h"

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

    time = get_utc_time();
    localTime = getCurrentTime();

    json["time_utc"] = dateTime_toString(time);
    json["time_local"] = dateTime_toString(localTime);
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Power Rail Control Functionality */

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::enable(){

    // Enable the 3.3v and 5v rails on the Hypnos
    digitalWrite(5, LOW);
    digitalWrite(6, HIGH);
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
void Loom_Hypnos::disable(){
    // Disable the 3.3v and 5v rails on the Hypnos
    digitalWrite(5, HIGH);
    digitalWrite(6, LOW);
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
bool Loom_Hypnos::registerInterrupt(InterruptCallbackFunction isrFunc, int interruptPin, InterruptType interruptType, int triggerState){
    pinMode(interruptPin, INPUT_PULLUP);  //  Set interrupt pin input mode

    if(interruptPin == 12){
        printModuleName(); Serial.println("Registering RTC interrupt...");
    }
    else{
        printModuleName(); Serial.println("Registering interrupt on pin " + String(interruptPin) + "...");
    }

    // If the RTC hasn't already been initialized then do so now if we are trying to schedule an RTC interrupt
    if(!RTC_initialized && interruptPin == 12)
        initializeRTC();

    // Make sure a callback function was supplied
    if(isrFunc != nullptr){
         // If the interrupt we registered is for sleep we should set the interrupt to wake the device from sleep
        if(interruptType == SLEEP){
            LowPower.attachInterruptWakeup(interruptPin, isrFunc, triggerState);
            printModuleName(); Serial.println("Interrupt successfully attached!");
        }
        else{
            attachInterrupt(digitalPinToInterrupt(interruptPin), isrFunc, triggerState);
            attachInterrupt(digitalPinToInterrupt(interruptPin), isrFunc, triggerState);
            printModuleName(); Serial.println("Interrupt successfully attached!");
        }
        // Add the interrupt to the list of pin to interrupts
        pinToInterrupt.insert(std::make_pair(interruptPin, std::make_tuple(isrFunc, triggerState, interruptType)));
        return true;
    } 
    else{
        detachInterrupt(digitalPinToInterrupt(interruptPin));
        printModuleName(); Serial.println("Failed to attach interrupt! Interrupt callback evaluated to a null pointer, it is possible you forgot to supply a callback function");
        return false;
    }

   
        
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::reattachRTCInterrupt(int interruptPin){
    if(std::get<2>(pinToInterrupt[interruptPin]) != SLEEP){

        // If we haven't previously registered the interrupt we need to do this before we can reattach to an interrupt that doesn't exist
        if(pinToInterrupt.count(interruptPin) <= 0){
            printModuleName(); Serial.println("Failed to reattach interrupt! Interrupt has not previously been registered...");
            return false;
        }

        attachInterrupt(digitalPinToInterrupt(interruptPin), std::get<0>(pinToInterrupt[interruptPin]), std::get<1>(pinToInterrupt[interruptPin]));
        attachInterrupt(digitalPinToInterrupt(interruptPin), std::get<0>(pinToInterrupt[interruptPin]), std::get<1>(pinToInterrupt[interruptPin]));
        
    }
    else{
        LowPower.attachInterruptWakeup(interruptPin, std::get<0>(pinToInterrupt[interruptPin]), std::get<1>(pinToInterrupt[interruptPin]));
    }
    printModuleName(); Serial.println("Interrupt successfully reattached!");

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
    
    // If the RTC failed to start inform the user and hang
    if(!RTC_DS.begin()){
        printModuleName(); Serial.println("Couldn't start RTC! Check your connections... Execution will now hang as this is likely a fatal error");
        while(1);
    }
    
    // This may end up causing a problem in practice - what if RTC loses power in field? Shouldn't happen with coin cell batt backup
	if (RTC_DS.lostPower()) {
		printModuleName(); Serial.println("RTC lost power, lets set the time!");

        // If we want to set a custom time
        if(Serial && custom_time){
            set_custom_time();
        }
        else{
            // Set the RTC to the date & time this sketch was compiled
            RTC_DS.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
	}

	// Clear any pending alarms
	RTC_DS.clearAlarm();

    RTC_DS.writeSqwPinMode(DS3231_OFF);

    // We successfully started the RTC 
    printModuleName(); Serial.println("DS3231 Real-Time Clock Initialized Successfully!");
    RTC_initialized = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime Loom_Hypnos::get_utc_time(){
    DateTime now = getCurrentTime();

    // Subtract 30 minutes from this zone
    if(timezone == TIME_ZONE::ACST)
        return now + TimeSpan(0, timezone, -30, 0);

    // If we are in a timezone that observes daylight savings
    else if(timezone == AST || timezone == EST || timezone == CST || timezone == MST || timezone == AST || timezone == PST || timezone == AKST){
        // If we are in the months where daylight savings is not in affect
        if(now.month() >= 3 && now.month() <= 10){

            // If in the months when it changes check if the days are correct
            if( (now.month() == 3 && now.day() >= 13) || (now.month() == 10 && now.day() < 6)){
                return now + TimeSpan(0, (timezone)-1, 0, 0);
            }
            return now + TimeSpan(0, (timezone)-1, 0, 0);
        }
        else{
            return now + TimeSpan(0, (timezone), 0, 0);
        }
    }
    
    else{
        return now + TimeSpan(0, timezone, 0, 0);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
String Loom_Hypnos::dateTime_toString(DateTime time){
     // Formatted as: YYYY-MM-DD HH:MM:SS
    int month = time.month();
    int day = time.day();

    // Adds a trailing 0 to numbers less than 10
    String dayString = (day < 10) ? "0" + String(day) : String(day);
    String monthString = (month < 10) ? "0" + String(month) : String(month);
    
    String timeString =   String(time.year()) 
                        + "-"
                        + monthString
                        + "-"
                        + dayString
                        + " "
                        + String(time.hour())
                        + ":"
                        + String(time.minute())
                        + ":"
                        + String(time.second());

    return timeString;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::set_custom_time(){

   	// initialized variable for user input
	String computer_year = "";
	String computer_month = "";
	String computer_day = "";
	String computer_hour = "";
	String computer_min = "";
	String computer_sec = "";

	// Let the user know that they should enter local time
	printModuleName();
	Serial.println("Please use your local time, not UTC!");

	// Entering the year
	printModuleName();
	Serial.println("Enter the Year (Four digits, e.g. 2020)");
  
	while(computer_year == ""){
		computer_year = Serial.readStringUntil('\n');
	}
	printModuleName();
	Serial.println("Year Entered: " + computer_year);

	// Entering the month
	printModuleName();
	Serial.println("Enter the Month (1 ~ 12)");

	while(computer_month == ""){
		computer_month = Serial.readStringUntil('\n');
	}
	printModuleName();
	Serial.println("Month Entered: " + computer_month);

	// Entering the day
	printModuleName();
	Serial.println("Enter the Day (1 ~ 31)");

	while(computer_day  == ""){
		computer_day = Serial.readStringUntil('\n');
	}
	printModuleName();
	Serial.println("Day Entered: " + computer_day);

	// Entering the hour
	printModuleName();
	Serial.println("Enter the Hour (0 ~ 23)");

	while(computer_hour == ""){
		computer_hour = Serial.readStringUntil('\n');
	}
	printModuleName();
	Serial.println("Hour Entered: "+computer_hour);

	// Entering the minute
	printModuleName();
	Serial.println("Enter the Minute (0 ~ 59)");

	while(computer_min == ""){
		computer_min = Serial.readStringUntil('\n');
	}
	printModuleName();
	Serial.println("Minute Entered: "+computer_min);

	// Entering the second
	printModuleName();
	Serial.println("Enter the Second (0 ~ 59)");
	while(computer_sec == ""){
		computer_sec = Serial.readStringUntil('\n');
	}

    // Set the RTC to the custom time
    RTC_DS.adjust(DateTime(computer_year.toInt(), computer_month.toInt(), computer_day.toInt(), computer_hour.toInt(), computer_min.toInt(), computer_sec.toInt()));

    // Entering the second
	printModuleName();
	Serial.println("Custom time successfully set to: " + String(getCurrentTime().text()));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::setInterruptDuration(const TimeSpan duration){

    // The time in the future that the alarm will be set for
    DateTime future(RTC_DS.now() + duration);
    RTC_DS.setAlarm(future);

    // Print the time that the next interrupt is set to trigger
    printModuleName(); Serial.println("Current Time: " + String(RTC_DS.now().text()));
    printModuleName(); Serial.println("Next Interrupt Alarm Set For: " + String(future.text()));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Sleep Functionality */

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::sleep(bool waitForSerial){
    // Try to power down the active modules
    manInst->power_down();
    
    disable();                      // Disable the power rails before sleeping
    pre_sleep();                    // Pre-sleep cleanup
    LowPower.sleep();               // Go to sleep and hang
    post_sleep(waitForSerial);      // Wake up
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::pre_sleep(){

    // 50ms delay allows this last message to be sent before the bus disconnects
    printModuleName(); Serial.println("Entering Standby Sleep...");
    delay(50);

    // Close the serial connection and detach
    Serial.end();
    USBDevice.detach();

    attachInterrupt(digitalPinToInterrupt(pinToInterrupt.begin()->first), std::get<0>(pinToInterrupt.begin()->second), std::get<1>(pinToInterrupt.begin()->second));

    // Disable the power rails
    disable();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::post_sleep(bool waitForSerial){
    USBDevice.attach();
    Serial.begin(115200);
    enable();

    // Re-init the modules that need it
    manInst->power_up();  
      
    // Clear any pending RTC alarms
    RTC_DS.clearAlarm();

    // We want to wait for the user to re-open the serial monitor before continuing to see readouts
    if(waitForSerial)
        while(!Serial);

    

    printModuleName(); Serial.println("Device has awoken from sleep!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TimeSpan Loom_Hypnos::getSleepIntervalFromSD(String fileName){
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<500> doc;
    DeserializationError deserialError = deserializeJson(doc, sdMan->readFile(fileName));

    // Create json object to easily pull data from
    JsonObject json = doc.as<JsonObject>();

    if(deserialError != DeserializationError::Ok){
        printModuleName(); Serial.println("There was an error reading the sleep interval from SD: " + String(deserialError.c_str()));
        return TimeSpan(0, 0, 20, 0);
    }
    else{
        printModuleName(); Serial.println("Sleep interval successfully loaded from SD!");
        // Return the interval as set in the json
        return TimeSpan(json["days"].as<int>(), json["hours"].as<int>(), json["minutes"].as<int>(), json["seconds"].as<int>());
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::getTimeZoneFromSD(String fileName){
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<500> doc;
    DeserializationError deserialError = deserializeJson(doc, sdMan->readFile(fileName));

    // Create json object to easily pull data from
    JsonObject json = doc.as<JsonObject>();

    if(deserialError != DeserializationError::Ok){
        printModuleName(); Serial.println("There was an error reading the timezone defaulting to programmed timezone");
    }
    else{
        printModuleName(); Serial.println("Timezone successfully loaded!");
        timezone = timezoneMap[json["timezone"].as<String>()];
    }
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
