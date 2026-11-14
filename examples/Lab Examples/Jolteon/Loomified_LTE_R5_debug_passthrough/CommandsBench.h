#pragma once
static void sleepCommand(const char *action,const char *value,const char *extra) {
  if (action && !strcmp(action,"status")) { sleepStatus(); return; }
  if (action && !strcmp(action,"load")) { loadSleepConfig(value); return; }
  if (action && (!strcmp(action,"set") || !strcmp(action,"save"))) {
    if (!strcmp(action,"save") && !sleepConfigTried) loadSleepConfig();
    uint32_t seconds=sleepSeconds;
    if (value && (!benchDuration(value,extra,seconds) || seconds<3)) { Serial.println(F("Use sleep set/save 10s or 1 min (3 s..24 h).")); return; }
    sleepSeconds=seconds; sleepOverride=true;
    if (!strcmp(action,"save")) { if (!saveSleepConfig()) Serial.println(F("Sleep config save failed; current session setting retained.")); }
    else sleepStatus();
    return;
  }
  uint32_t seconds=sleepSeconds,cycles=1;
  if (!action) {
    if (!sleepConfigTried && !sleepOverride) loadSleepConfig();
    seconds=sleepSeconds;
  } else {
    if (benchDuration(action,value,seconds)) {
      if (extra && (!unsignedNumber(extra,cycles,5) || !cycles)) { Serial.println(F("Sleep cycles must be 1..5.")); return; }
    } else if (benchDuration(action,nullptr,seconds) && value && unsignedNumber(value,cycles,5) && cycles && !extra) {
      // e.g. sleep 10s 3
    } else { Serial.println(F("sleep [10s | 1 min] [cycles] | sleep set 10s | sleep load [file.json] | sleep save 1min | sleep status")); return; }
    sleepSeconds=seconds; sleepOverride=true;
  }
  sleepStatus(); sleepTest(seconds,uint8_t(cycles));
}
static void help() {
  Serial.println(F("\n============================================================"));
  Serial.println(F("JOLTEON BENCH  |  Feather M0  |  Serial 115200"));
  Serial.print(F("Compiled (Pacific): ")); Serial.print(__DATE__); Serial.print(' '); Serial.println(__TIME__);
  Serial.print(F("Device: ")); Serial.print(manager.get_device_name()); Serial.print(F("  instance: ")); Serial.println(manager.get_instance_num());
  Serial.println(F("Protocol commands enable Hypnos 3V/5V; SDI data also enables 12V."));
  Serial.println(F("\nQUICK START"));
  Serial.println(F("  help                             Show this command list"));
  Serial.println(F("  baseline [nosd]                  Run board checks, optionally skip SD"));
  Serial.println(F("  summary                          Show the last test results"));
  Serial.println(F("\nPOWER"));
  Serial.println(F("  rails on                         Enable Hypnos 3V/5V and initialize the RTC"));
  Serial.println(F("  rails off                        Disable switched rails"));
  Serial.println(F("  rails 3                          Enable only the switched 3V rail"));
  Serial.println(F("  rails 5                          Enable only the switched 5V rail"));
  Serial.println(F("  rails 12 on|off                  Control the sensor 12V boost"));
  Serial.println(F("  rails status                     Show rail GPIO states"));
  Serial.println(F("  hypnos enable                    Enable Hypnos (same as rails on)"));
  Serial.println(F("  hypnos disable                   Disable Hypnos (same as rails off)"));
  Serial.println(F("  hypnos status                    Show Hypnos rail GPIO states"));
  Serial.println(F("\nI2C / SENSORS"));
  Serial.println(F("  i2c scan [channel]               Scan the root bus or one mux channel (0..7)"));
  Serial.println(F("  i2c recover                      Try to release a stuck I2C bus"));
  Serial.println(F("  mux hello                        Read the mux control register"));
  Serial.println(F("  mux test                         Scan all eight mux channels"));
  Serial.println(F("  mux addr <112..119>              Set the mux address in decimal"));
  Serial.println(F("  mux expect <channel> <address>   Check branch isolation (decimal address)"));
  Serial.println(F("  ms5803 hello [0x76|0x77]         Check the MS5803 and read a sample"));
  Serial.println(F("  ms5803 read [0x76|0x77]          Read pressure and temperature on channel 4"));
  Serial.println(F("  dfgas read <channel> [address]   Read CO in ppm"));
  Serial.println(F("  soil read <channel> [address]    Read STEMMA temperature and capacitance"));
  Serial.println(F("\nCLOCK / SLEEP"));
  Serial.println(F("  rtc show                         Show UTC and Unix seconds"));
  Serial.println(F("  rtc test                         Check RTC time and ticking"));
  Serial.println(F("  rtc alarm                        Test the DS3231 alarm and interrupt"));
  Serial.println(F("  rtc set compile                  Set the RTC from sketch compile time"));
  Serial.println(F("  rtc set 1793836800               Set UTC from Unix seconds"));
  Serial.println(F("  rtc set 2026-11-05T00:00:00Z     Set UTC from an ISO timestamp"));
  Serial.println(F("  sleep                            Sleep for the configured interval"));
  Serial.println(F("  sleep 10s [cycles]               Sleep 10 seconds, optionally 1..5 cycles"));
  Serial.println(F("  sleep 1 min                      Sleep for one minute"));
  Serial.println(F("  sleep set 10s                    Set the session sleep interval"));
  Serial.println(F("  sleep load [file.json]           Load the sleep interval from SD"));
  Serial.println(F("  sleep save 1min                  Save a sleep interval to SD"));
  Serial.println(F("  sleep status                     Show the interval and its source"));
  Serial.println(F("  Armed sleep keeps rails off after reset until the DS3231 alarm."));
  Serial.println(F("\nSD / SPI"));
  Serial.println(F("  sd save                          Save Loom MS5803 JSON and print it back"));
  Serial.println(F("  sd test                          Run the same save and readback as sd save"));
  Serial.println(F("  sd list                          List SD root files, sizes, and folders"));
  Serial.println(F("  sd read [file]                   Print the latest or selected test file"));
  Serial.println(F("  spi loop                         Test SPI with a MOSI-MISO jumper"));
  Serial.println(F("\nLTE / MQTT"));
  Serial.println(F("  lte hello                        Wake the modem if needed and check AT"));
  Serial.println(F("  lte on                           Send the modem power pulse and check AT"));
  Serial.println(F("  lte leds                         Configure and verify the modem LEDs"));
  Serial.println(F("  lte test                         Read modem, SIM, and network status"));
  Serial.println(F("  lte scan                         Scan available mobile networks"));
  Serial.println(F("  lte connect                      Register and open the APN data session"));
  Serial.println(F("  lte ping [host]                  Ping through LTE (default 1.1.1.1)"));
  Serial.println(F("  lte register [auto|att]          Recover network registration if needed"));
  Serial.println(F("  lte credentials                  Show credential configuration status"));
  Serial.println(F("  lte pins                         Show the modem UART pin mapping"));
  Serial.println(F("  lte bridge                       Open the raw AT console (Ctrl+] to exit)"));
  Serial.println(F("  lte quiet                        Release the modem UART"));
  Serial.println(F("  AT...                            Send a single raw modem command"));
  Serial.println(F("  mqtt test                        Publish an SD-backed test and read it back"));
  Serial.println(F("  device                           Show the device name and instance"));
  Serial.println(F("  device name Jolteon              Set the session device name"));
  Serial.println(F("  device instance 1                Set the session device instance"));
  Serial.println(F("  Attach the LTE antenna before RF/network tests."));
  Serial.println(F("\nRGB / ANALOG"));
  Serial.println(F("  rgb on                           Light the NeoPixel with the selected color"));
  Serial.println(F("  rgb off                          Turn the NeoPixel off"));
  Serial.println(F("  rgb color r|g|b|y|c|m|w          Choose a preset color and turn the pixel on"));
  Serial.println(F("  rgb color 255,255,0              Set RGB values (0..255 per component)"));
  Serial.println(F("  rgb test                         Compare A0/A1 before, during, and after pulses"));
  Serial.println(F("  analog <A0|A1|A2> [volts]        Read an input or compare to known volts"));
  Serial.println(F("  analog ref 3.300                 Set the ADC reference estimate"));
  Serial.println(F("  tds read                         Read A0 counts and voltage"));
  Serial.println(F("  teros10 read A1                  Read analog TEROS 10 on the specified pin"));
  Serial.println(F("\nSDI-12"));
  Serial.println(F("  sdi ?!                           Discover the address of one attached sensor"));
  Serial.println(F("  sdi 0I!                          Identify the sensor at address 0"));
  Serial.println(F("  sdi set teros11|teros12 <addr>   Select the model and address for sdi read"));
  Serial.println(F("  sdi read                         Measure with the selected model and address"));
  Serial.println(F("  sdi measure 0                    Measure and print the raw SDI-12 reply"));
  Serial.println(F("  sdi power on|off                 Control the sensor 12V boost"));
  Serial.println(F("============================================================\n"));
}
static loomSDI12::Model sdiModel = loomSDI12::Model::Unknown;
static char sdiAddress[2] = "0";
static void selectSdiModel(const char *model, const char *address) {
  if (model && !strcmp(model,"teros10")) {
    Serial.println(F("TEROS 10 is analog. Use teros10 read <A0|A1|A2>."));
    return;
  }
  if (!model || (strcmp(model,"teros11") && strcmp(model,"teros12")) ||
      !address || strlen(address)!=1 || !isalnum(uint8_t(*address))) {
    Serial.println(F("Use sdi set teros11|teros12 <single address>."));
    return;
  }
  sdiModel = !strcmp(model,"teros11") ? loomSDI12::Model::TER11 : loomSDI12::Model::TER12;
  sdiAddress[0] = *address;
  Serial.print(F("Selected ")); Serial.print(model);
  Serial.print(F(" at address ")); Serial.println(sdiAddress);
}

static void dispatch(char *line) {
  if ((line[0]=='A' || line[0]=='a') && (line[1]=='T' || line[1]=='t')) { at(line); dataReady=false; return; }
  char *save=nullptr; char *a=strtok_r(line," \t",&save),*b=strtok_r(nullptr," \t",&save),*c=strtok_r(nullptr," \t",&save),*d=strtok_r(nullptr," \t",&save),*extra=strtok_r(nullptr," \t",&save);
  if (!a) return;
  if (extra) { Serial.println(F("Too many arguments; RGB numeric colors use commas: 255,255,0.")); return; }
  uint32_t x=0,y=0;
  if (!strcmp(a,"help")) help();
  else if (!strcmp(a,"summary")) summary();
  else if (!strcmp(a,"rails")) railCommand(b,c);
  else if (!strcmp(a,"hypnos") && b) {
    if (!strcmp(b,"enable")) railCommand("on"); else if (!strcmp(b,"disable")) railCommand("off"); else railStatus();
  }
  else if (!strcmp(a,"device")) {
    if (b && !strcmp(b,"name") && c && loomMQTT::validTopic(c,Manager::DEVICE_NAME_SIZE,true)) manager.set_device_name(c);
    else if (b && !strcmp(b,"instance") && unsignedNumber(c,x,2147483647)) manager.set_instance_num(int(x));
    else if (b) { Serial.println(F("device name <name> | device instance <number> | device")); return; }
    Serial.print(F("Device: ")); Serial.print(manager.get_device_name()); Serial.print(F(" instance=")); Serial.println(manager.get_instance_num());
  }
  else if (!strcmp(a,"mux") && b) {
    if (!strcmp(b,"hello")) muxHello(); else if (!strcmp(b,"test")) muxTest();
    else if (!strcmp(b,"addr") && unsignedNumber(c,x,119) && x>=112) { muxAddr=x; results[MUX]=results[MUX_HELLO]=NOT_RUN; }
    else if (!strcmp(b,"expect") && unsignedNumber(c,x,7) && unsignedNumber(d,y,119) && y>=8 && y!=muxAddr) muxExpect(x,y);
    else Serial.println(F("mux hello|test | mux addr <112..119> | mux expect <channel> <address>"));
  }
  else if (!strcmp(a,"ms5803")) {
    if ((b && strcmp(b,"hello") && strcmp(b,"read")) || d || (c && strcmp(c,"0x76") && strcmp(c,"0x77"))) { Serial.println(F("ms5803 hello|read [0x76|0x77]")); return; }
    ms5803Test(c?(!strcmp(c,"0x76")?0x76:0x77):0);
  }
  else if (!strcmp(a,"i2c") && b) {
    if (!strcmp(b,"scan")) {
      if (!c) { if (rootBus(MUX)) scan(nullptr,nullptr); }
      else if (!d && unsignedNumber(c,x,7)) scanBranch(x);
      else Serial.println(F("Use i2c scan [channel 0..7]."));
    }
    else if (!strcmp(b,"recover")) { ready(MUX); Serial.println(bus.recover()?F("I2C bus released."):F("I2C bus still stuck LOW.")); }
  }
  else if (!strcmp(a,"dfgas") || !strcmp(a,"soil") || !strcmp(a,"seesaw")) {
    if (b && !strcmp(b,"read")) muxSensor(!strcmp(a,"dfgas"),c,d);
    else Serial.println(F("Use dfgas read <channel> [address] or soil read <channel> [address]."));
  }
  else if (!strcmp(a,"rtc")) {
    if (!b || !strcmp(b,"show")) { uint32_t epoch; if (ensureRtc() && clockRead(epoch)) printRtcTime(epoch); }
    else if (!strcmp(b,"test")) rtcTest(); else if (!strcmp(b,"alarm")) alarmTest();
    else if (!strcmp(b,"set") && c && !strcmp(c,"compile")) rtcCompile();
    else if (!strcmp(b,"set") && (benchIsoEpoch(c,x) || unsignedNumber(c,x,4102444799ul))) rtcSet(x);
    else Serial.println(F("rtc show|test|alarm | rtc set compile|<Unix seconds>|YYYY-MM-DDTHH:MM:SSZ"));
  }
  else if (!strcmp(a,"sleep")) sleepCommand(b,c,d);
  else if (!strcmp(a,"sd")) {
    if (b && (!strcmp(b,"save") || !strcmp(b,"test") || !strcmp(b,"write"))) sdTest(true);
    else if (b && !strcmp(b,"list")) listSdFiles();
    else if (b && !strcmp(b,"read")) sdTest(false,c); else Serial.println(F("sd list | sd save | sd test | sd read [JLT00000.JSON]"));
  }
  else if (!strcmp(a,"spi") && b && !strcmp(b,"loop")) spiTest();
  else if (!strcmp(a,"lte")) lteCommand(b,c);
  else if (!strcmp(a,"mqtt") && (!b || !strcmp(b,"test"))) mqttTest();
  else if (!strcmp(a,"rgb") || !strcmp(a,"neopixel")) rgbCommand(b,c);
  else if (!strcmp(a,"analog")) {
    float volts;
    if (b && !strcmp(b,"ref") && real(c,volts) && volts>=3.0f && volts<=3.6f) { adcReference=volts; results[ANALOG]=NOT_RUN; Serial.println(F("ADC reference estimate updated.")); }
    else analogTest(b,c);
  }
  else if (!strcmp(a,"tds") && (!b || !strcmp(b,"read")) && !c) {
    Serial.println(F("TDS analog signal on A0 (raw bench reading)."));
    analogTest("A0",nullptr);
  }
  else if (!strcmp(a,"teros10") && b && !strcmp(b,"read") && !d) {
    Serial.println(F("TEROS 10 analog output; power it from its specified excitation supply."));
    analogTest(c,nullptr);
  }
  else if (!strcmp(a,"sdi")) {
    if (b && !strcmp(b,"power") && c && (!strcmp(c,"on") || !strcmp(c,"off"))) { if (!strcmp(c,"on")) ready(SDI); sdiPower(!strcmp(c,"on")); }
    else if (b && !strcmp(b,"set")) selectSdiModel(c,d);
    else if (b && !strcmp(b,"read") && !c) {
      if (sdiModel == loomSDI12::Model::Unknown) Serial.println(F("Select a model first with sdi set teros11|teros12 <address>."));
      else sdiMeasure(sdiAddress,sdiModel);
    }
    else if (b && !strcmp(b,"measure")) sdiMeasure(c);
    else sdiCommand(b);
  }
  else if (!strcmp(a,"baseline")) {
    if ((b && strcmp(b,"nosd")) || c || d) { Serial.println(F("baseline [nosd]")); return; }
    enableHypnos(); muxHello(); muxTest(); ms5803Test(); rtcTest();
    if (b) { output(SD_CS,true); record(SD_READ,INFO,"Optional SD intentionally skipped."); record(SD_WRITE,INFO,"Optional SD intentionally skipped."); }
    else sdTest(false);
    analogTest("A0",nullptr); lteTest(); summary();
  }
  else Serial.println(F("Unknown/incomplete command; help lists commands."));
}
