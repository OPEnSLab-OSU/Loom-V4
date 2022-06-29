#include "../../Loom_Hypnos.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::Loom_Hypnos(HYPNOS_VERSION version, bool use_custom_time, bool useSD) : Module("Hypnos"), custom_time(use_custom_time), sd_chip_select(version), enableSD(useSD){
    // Set the pins to write mode
    pinMode(5, OUTPUT);                     // 3.3v power rail
    pinMode(6, OUTPUT);                     // 5v power rail
    pinMode(LED_BUILTIN, OUTPUT);           // Status LED
    pinMode(12, INPUT_PULLUP);              // RTC Interrupt
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Hypnos::Loom_Hypnos(Manager& man, HYPNOS_VERSION version, bool use_custom_time, bool useSD) : Module("Hypnos"), custom_time(use_custom_time), sd_chip_select(version), enableSD(useSD){
    manInst = &man;

    // Set the pins to write mode
    pinMode(5, OUTPUT);                     // 3.3v power rail
    pinMode(6, OUTPUT);                     // 5v power rail
    pinMode(LED_BUILTIN, OUTPUT);           // Status LED
    pinMode(12, INPUT_PULLUP);              // RTC Interrupt

    // Create the SD Manager if we want to use SD
    if(useSD)
        sdMan = new SDManager(manInst, sd_chip_select);

    // Add the Hypnos to the module register s
    manInst->registerModule(this);

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
    // Formatted as: YYYY-MM-DD HH:MM:SS
    int month = getCurrentTime().month();
    int day = getCurrentTime().day();

    // Adds a trailing 0 to numbers less than 10
    String dayString = (day < 10) ? "0" + String(day) : String(day);
    String monthString = (month < 10) ? "0" + String(month) : String(month);
    
    String timeString =   String(getCurrentTime().year()) 
                        + "-"
                        + monthString
                        + "-"
                        + dayString
                        + " "
                        + String(getCurrentTime().hour())
                        + ":"
                        + String(getCurrentTime().minute())
                        + ":"
                        + String(getCurrentTime().second());

    manInst->getDocument()["Timestamp"]["time"] = timeString;
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
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


/* Interrupt Functionality */

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::registerInterrupt(InterruptCallbackFunction isrFunc){

    printModuleName(); Serial.println("Registering Real-Time Clock Interrupt...");

    // If the RTC hasn't already been initialized then do so now
    if(!RTC_initialized)
        initializeRTC();

    // Make sure a callback function was supplied
    if(isrFunc != nullptr){
        
        attachInterrupt(digitalPinToInterrupt(12), isrFunc, LOW);
        attachInterrupt(digitalPinToInterrupt(12), isrFunc, LOW);
        printModuleName(); Serial.println("Interrupt successfully attached!");
        

        // We have registered this interrupt previously
        hasInterruptBeenRegistered = true;
        callbackFunc = isrFunc;
        return true;
    } 
    else{
        detachInterrupt(digitalPinToInterrupt(12));
        printModuleName(); Serial.println("Failed to attach interrupt! Interrupt callback evaluated to a null pointer, it is possible you forgot to supply a callback function");
        return false;
    }

   
        
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Hypnos::reattachRTCInterrupt(){
    // If we haven't previously registered the interrupt we need to do this before we can reattach to an interrupt that doesn't exist
    if(!hasInterruptBeenRegistered){
        printModuleName(); Serial.println("Failed to reattach interrupt! Interrupt has not previously been registered...");
        return false;
    }

    attachInterrupt(digitalPinToInterrupt(12), callbackFunc, LOW);
    attachInterrupt(digitalPinToInterrupt(12), callbackFunc, LOW);
    printModuleName(); Serial.println("Interrupt successfully reattached!");

    return true;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::wakeup(){
    detachInterrupt(digitalPinToInterrupt(12));     // Detach the interrupt so it doesn't trigger again
    enable();                                       // Re-enable the Hypnos power rails       
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::initializeRTC(){
    

    // If the RTC failed to start inform the user and hang
    if(!RTC_DS.begin()){
        printModuleName(); Serial.println("Couldn't start RTC! Check your connections... Execution will now hang as this is likely a fatal error");
        while(1);
    }

    // Set the time to the last compile time
    RTC_DS.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // This may end up causing a problem in practice - what if RTC loses power in field? Shouldn't happen with coin cell batt backup
	if (RTC_DS.lostPower()) {
		printModuleName(); Serial.println("RTC lost power, lets set the time!");
		// Set the RTC to the date & time this sketch was compiled
		RTC_DS.adjust(DateTime(F(__DATE__), F(__TIME__)));
	}

	// Clear any pending alarms
	RTC_DS.clearAlarm();

    RTC_DS.writeSqwPinMode(DS3231_OFF);

    // If we want to set a custom time
    if(custom_time){
        set_custom_time();
    }


    // We successfully started the RTC 
    printModuleName(); Serial.println("DS3231 Real-Time Clock Initialized Successfully!");
    RTC_initialized = true;
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
    LowPower.standby();             // Go to sleep and hang
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

   

    attachInterrupt(digitalPinToInterrupt(12), callbackFunc, LOW);
    attachInterrupt(digitalPinToInterrupt(12), callbackFunc, LOW);

    // Disable the power rails
    disable();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Hypnos::post_sleep(bool waitForSerial){
    USBDevice.attach();
    Serial.begin(115200);

    manInst->power_up();    // Re-init the modules that need it

    // Clear any pending RTC alarms
    RTC_DS.clearAlarm();


    // We want to wait for the user to re-open the serial monitor before continuing to see readouts
    if(waitForSerial)
        while(!Serial);

    printModuleName(); Serial.println("Device has awoken from sleep!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////