// Manager must be the first Loom include. LTE data/MQTT remain explicit commands.
#include <Loom_Manager.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Utilities/Loom_MQTTUtils.h>
#include <Hardware/Actuators/Loom_Neopixel/Loom_Neopixel.h>
#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
#include <FlashStorage.h>
#include <ArduinoLowPower.h>
#include <RTCZero.h>
#include <Utilities/Loom_SDI12Utils.h>
#include "Bringup.h"
#include "config.h"
#include "Logic.h"
#include "Bus.h"
#include "NetworkConfig.h"
#include "Registration.h"
#include "PressureChecks.h"
#include "ScanLogic.h"
#include "BenchLogic.h"
#include "BenchSettings.h"
#include "SleepDeadline.h"
#include <stdlib.h>
#include <ctype.h>

static Manager manager(DEVICE_NAME,DEVICE_INSTANCE);
// Hypnos owns rails/RTC. Optional SD diagnostics use the separate console card object.
static Loom_Hypnos hypnos(manager,HYPNOS_VERSION::V3_3,TIME_ZONE::PST,false,false);
static bool rail3On, rail5On, rail12On;
static BenchI2c bus;
static SdFat sd;
static bool serialOpen, uartOpen, rawBridge;
static bool rtcGood, alarmGood;
static bool pulseSent,ledsConfigured,dataReady;
static bool pingPending,pingAnswered;
static long pingRtt=-1;
static char pingLine[224];
static size_t pingUsed;
static uint32_t sleepSeconds=DEFAULT_SLEEP_SECONDS;
static bool sleepConfigTried,sleepConfigLoaded,sleepOverride;
static char sleepConfigFile[64]={};
namespace {
FlashStorage(pendingSleepDeadline, SleepDeadline)
}
static char lastSampleFile[24],testId[80];
static uint32_t sampleSequence;
static Loom_LTE *network;
static bool ensureRtc();
static void ltePulse();
static bool lteHello();
static bool configureLteLeds();
static bool prepareSleepLte();
static bool loadSleepConfig(const char *path=nullptr);
static bool registrationCanCancel, registrationCancelled, lastAtCompleted;
static PendingAtReply pendingReply;
static uint8_t muxAddr=MUX_ADDR_DEFAULT;
static float adcReference=3.3f;
static char input[192], reply[2048];
static size_t inputUsed;
static bool discardLine;
static volatile bool rtcWoke, hypnosWakeIrq;
enum Check : uint8_t { POWER, MUX, CLOCK, ALARM, STANDBY_TEST, SD_READ, SD_WRITE, SPI_LOOP, LTE_AT, SIM, NET, SDI, ANALOG, LTE_HELLO, MUX_HELLO, MS5803_TEST, LTE_SCAN, LTE_LEDS, LTE_PING, MQTT_ACK, MQTT_ECHO, PIXEL_NOISE, CHECK_COUNT };
enum Result : uint8_t { NOT_RUN, PASS, FAIL, BLOCKED, INFO };
static Result results[CHECK_COUNT];
static uint32_t checkedAt[CHECK_COUNT];
static const char *checkNames[]={"Hypnos rail controls","I2C mux", "DS3231 clock", "DS3231 alarm/clear", "Hypnos standby/wake", "SD mount/read", "SD JSON save/readback", "SPI loopback", "SARA-R5 identity", "SIM", "LTE registration", "SDI-12", "Analog known voltage", "LTE hello", "Mux hello", "MS5803 pressure/temperature", "LTE RF/network scan", "Modem GPIO LEDs", "LTE internet ping", "MQTT broker ACK", "MQTT packet readback", "A2 NeoPixel / A0-A1 noise"};
static const char *resultNames[]={"NOT RUN","PASS","FAIL","BLOCKED","INFO"};
static void record(Check check,Result result,const char *detail) {
  results[check]=result; checkedAt[check]=millis();
  Serial.print('['); Serial.print(resultNames[result]); Serial.print("] ");
  Serial.print(checkNames[check]); Serial.print(": "); Serial.println(detail);
}
static void output(uint8_t pin,bool level) {
  digitalWrite(pin,level?HIGH:LOW); pinMode(pin,OUTPUT);
}
static void quietBuses() {
  if (uartOpen) Serial1.end(); uartOpen=false; rawBridge=false;
  SPI.end();
  const uint8_t pins[]={0,1,SDA,SCL,MISO,MOSI,SCK,SD_CS,SDI_DATA};
  for (uint8_t p:pins) { digitalWrite(p,LOW); pinMode(p,INPUT); }
  output(SDI_DIR,false); output(MODEM_PWR,false); output(MODEM_RESET,false);
}
static void setRails(bool on3,bool on5,bool on12) {
  quietBuses();
  output(SENSOR12_EN,false);
  if (!on3 && !on5) hypnos.disable();
  else hypnos.setWakeConfiguration(on3 ? (on5?PR_3V_ON_5V_ON:PR_3V_ON_5V_OFF) : PR_3V_OFF_5V_ON);
  if (on12) output(SENSOR12_EN,true);
  rail3On=on3; rail5On=on5; rail12On=on12;
  delay(600);
}
static void safeOff() { setRails(false,false,false); }
// Protocol commands acquire their rail prerequisites; explicit OFF remains manual.
static bool ready(Check check) {
  if (!rail3On || !rail5On) {
    Serial.println(F("Auto power: enabling Hypnos 3V/5V for this command."));
    // No bus shutdown here: enabling a prerequisite must not discard a pending LTE reply.
    hypnos.setWakeConfiguration(PR_3V_ON_5V_ON);
    rail3On=rail5On=true; delay(600);
  }
  manager.setEnableState(true);
  if (!manager.isInitialized()) manager.initialize();
  if (check==SDI && !rail12On) {
    Serial.println(F("Auto power: enabling the SDI-12 sensor's 12V boost."));
    output(SENSOR12_EN,true); rail12On=true; delay(1500);
  }
  return true;
}
static void summary() {
  Serial.println(F("\nLast recorded test results:"));
  for (uint8_t i=0;i<CHECK_COUNT;++i) {
    Serial.print(resultNames[results[i]]); Serial.print(" | "); Serial.print(checkNames[i]);
    Serial.print(" | awake_ms="); Serial.println(checkedAt[i]);
  }
  Serial.print(F("Commanded rails 3V/5V/12V: ")); Serial.print(rail3On); Serial.print('/'); Serial.print(rail5On); Serial.print('/'); Serial.println(rail12On);
  Serial.println(F("Results cover the named checks. Check Mongo insertion and sensor calibration separately."));
}
static bool real(const char *s,float &v) {
  if (!s || !*s) return false;
  char *end=nullptr; v=strtof(s,&end); return end && !*end && isfinite(v);
}
static void railStatus() {
  Serial.print(F("D5=")); Serial.print(digitalRead(RAIL3)); Serial.print(F(" D6=")); Serial.print(digitalRead(RAIL5));
  Serial.print(F(" A3=")); Serial.println(digitalRead(SENSOR12_EN));
  Serial.println(F("D5 LOW=3VRAIL ON; D6 HIGH=5VRAIL + HIGHVRAIL ON; A3 HIGH=12V ON."));
  Serial.println(F("Always-on nodes: logic=+3V3, boost=5VOUT, modem=VCC_3.8V (TP20)."));
  Serial.println(F("Protocol commands automatically enable Hypnos 3V/5V when needed. rails off remains available."));
}
static void reportRailPins() {
  bool matches=(digitalRead(RAIL3)==(rail3On?LOW:HIGH)) && (digitalRead(RAIL5)==(rail5On?HIGH:LOW)) &&
               (digitalRead(SENSOR12_EN)==(rail12On?HIGH:LOW));
  record(POWER,matches?INFO:FAIL,matches ? "Rail control call returned; GPIO levels match requested state (not a measured voltage)." : "GPIO control readback does not match the requested rail state.");
  railStatus();
}
static void enableHypnos() {
  quietBuses(); output(SENSOR12_EN,false); rail12On=false;
  Serial.println(F("CALL hypnos.enable(true, true), useSD=false"));
  hypnos.enable(true,true);
  rail3On=rail5On=true; delay(600);
  Serial.print(F("Hypnos RTC initialized: ")); Serial.println(hypnos.isRTCInitialized());
  // Full enable() can initialize the RTC over hardware Wire. Give the pins
  // back to the bounded software I2C diagnostic transport afterward.
  Wire.end(); pinMode(SDA,INPUT); pinMode(SCL,INPUT);
  rtcGood=alarmGood=false;
  reportRailPins();
}
static void railCommand(char *action,char *value=nullptr) {
  if (!action || !strcmp(action,"status")) { railStatus(); return; }
  if (!strcmp(action,"off")) { Serial.println(F("CALL hypnos.disable()")); safeOff(); }
  else if (!strcmp(action,"on") || !strcmp(action,"both") || !strcmp(action,"start")) { enableHypnos(); return; }
  else if (!strcmp(action,"3")) { Serial.println(F("CALL hypnos.setWakeConfiguration(PR_3V_ON_5V_OFF)")); setRails(true,false,false); }
  else if (!strcmp(action,"5")) { Serial.println(F("CALL hypnos.setWakeConfiguration(PR_3V_OFF_5V_ON)")); setRails(false,true,false); }
  else if (!strcmp(action,"12")) {
    if (value && strcmp(value,"on") && strcmp(value,"off")) { Serial.println(F("rails 12 [on|off]")); return; }
    rail12On=!value || !strcmp(value,"on"); output(SENSOR12_EN,rail12On); delay(600);
    Serial.println(F("Carrier A3/12V_EN updated; Hypnos 3V/5V states unchanged."));
  }
  else { Serial.println(F("rails on|off|3|5|12 [on|off]|status")); return; }
  reportRailPins();
}
static void hex(uint8_t v) { if (v<16) Serial.print('0'); Serial.print(v,HEX); }
static bool ping(uint8_t addr) { return bus.transfer(addr,nullptr,0,nullptr,0); }
static bool muxSelect(uint8_t mask) { return bus.transfer(muxAddr,&mask,1,nullptr,0); }
static bool muxRead(uint8_t &mask) { return bus.transfer(muxAddr,nullptr,0,&mask,1); }
static bool regRead(uint8_t addr,uint8_t reg,uint8_t *data,size_t n) { return bus.transfer(addr,&reg,1,data,n); }
static bool regWrite(uint8_t addr,uint8_t reg,uint8_t val) { uint8_t b[]={reg,val}; return bus.transfer(addr,b,2,nullptr,0); }
static void muxHello() {
  ready(MUX_HELLO);
  uint8_t mask=0;
  bool ok=muxRead(mask);
  Serial.print(F("Mux 0x")); hex(muxAddr); Serial.print(F(" control mask=0x"));
  if (ok) hex(mask); else Serial.print(F("<no reply>")); Serial.println();
  record(MUX_HELLO,ok?PASS:FAIL,"Read-only control register reply; channel selection unchanged. Use mux test for all channels.");
}
static bool pressureRead(uint8_t address,uint8_t command,uint8_t *bytes,size_t count) {
  return bus.transfer(address,&command,1,nullptr,0) && bus.transfer(address,nullptr,0,bytes,count);
}
static bool pressureRaw(uint8_t address,uint8_t command,uint32_t &value) {
  if (!bus.transfer(address,&command,1,nullptr,0)) return false;
  delay(3); // OSR 512 conversion; pressure=0x42, temperature=0x52.
  uint8_t bytes[3];
  if (!pressureRead(address,0,bytes,sizeof(bytes))) return false;
  value=(uint32_t(bytes[0])<<16)|(uint32_t(bytes[1])<<8)|bytes[2];
  return pressureAdcValid(value);
}
static bool ms5803Test(uint8_t requested=0,bool makePacket=false) {
  ready(MS5803_TEST);
  if (makePacket && !ensureRtc()) return false;
  uint8_t saved=0;
  if (!muxRead(saved)) { record(MS5803_TEST,FAIL,"Cannot read mux control; no sensor test started."); return false; }
  // Restore the previous selection even on an early sensor failure.
  auto finish=[&](bool ok,const char *detail) {
    uint8_t got=0; bool restored=muxSelect(saved) && muxRead(got) && got==saved;
    if (!restored) Serial.println(F("FAIL: previous mux mask could not be restored."));
    record(MS5803_TEST,ok && restored?PASS:FAIL,detail);
    return ok && restored;
  };
  uint8_t mask=uint8_t(1u<<MS5803_MUX_CHANNEL),got=0;
  if (!muxSelect(mask) || !muxRead(got) || got!=mask) return finish(false,"Cannot select/read back MS5803 mux channel 4.");
  bool low=ping(0x76),lowFault=bus.fault;
  bool high=ping(0x77),highFault=bus.fault;
  if (lowFault || highFault || (!low && !high) || (!requested && low && high)) {
    return finish(false,low && high ? "Both addresses ACK; select ms5803 read 0x76 or 0x77 explicitly." : "No MS5803 ACK or bus timeout on channel 4 (0x76/0x77).");
  }
  uint8_t address=requested?requested:(low?0x76:0x77);
  if ((address==0x76 && !low) || (address==0x77 && !high)) return finish(false,"Requested sensor address did not ACK.");
  Serial.print(F("MS5803-02BA channel 4 address=0x")); hex(address); Serial.println();
  uint8_t reset=0x1e;
  if (!bus.transfer(address,&reset,1,nullptr,0)) return finish(false,"MS5803 reset command did not ACK.");
  delay(5);
  uint16_t prom[8]={}; bool promRead=true;
  for (uint8_t i=0;i<8 && promRead;++i) {
    uint8_t b[2]; promRead=pressureRead(address,uint8_t(0xa0+2*i),b,2);
    if (promRead) prom[i]=uint16_t(b[0])<<8|b[1];
  }
  if (!promRead || !pressurePromValid(prom)) return finish(false,"MS5803 calibration PROM read/CRC4 failed or coefficients empty.");
  Serial.println(F("MS5803 PROM CRC4 OK; calling Loom_MS5803 initialize/measure."));
  float temperature=0,pressure=0; bool initialized=false;
    // useMux=true avoids registering a temporary object with Manager. This
    // console owns selection; Loom uses its normal hardware Wire transport.
    Loom_MS5803 sensor(manager,address,true);
    Wire.begin(); Wire.setClock(100000);
    sensor.initialize();
    if (sensor.moduleInitialized) sensor.measure();
    initialized=sensor.moduleInitialized;
    temperature=sensor.getTemperature(); pressure=sensor.getPressure();
    Wire.end(); pinMode(SDA,INPUT); pinMode(SCL,INPUT);
  // The legacy driver can reuse bytes on a short read. Require independent
  // complete, non-empty fresh D1/D2 reads before accepting its getters.
  uint32_t d1=0,d2=0;
  bool rawOk=pressureRaw(address,0x42,d1) && pressureRaw(address,0x52,d2);
  Serial.print(F("MS5803 D1=")); Serial.print(d1); Serial.print(F(" D2=")); Serial.println(d2);
  Serial.print(F("Loom temperature C=")); Serial.print(temperature,3);
  Serial.print(F(" pressure mbar=")); Serial.println(pressure,3);
  bool good=initialized && rawOk && isfinite(temperature) && isfinite(pressure) && pressure>0;
  if (good && makePacket) {
    Wire.begin(); Wire.setClock(100000);
    manager.package(); // Hypnos adds its real timestamp; no synthetic packet schema.
    sensor.package(); // Same MS5803 module keys as an ordinary Loom measurement.
    Wire.end(); pinMode(SDA,INPUT); pinMode(SCL,INPUT);
    snprintf(testId,sizeof(testId),"%s-%08lX-%08lX-%lu",manager.get_serial_num(),
             (unsigned long)millis(),(unsigned long)micros(),(unsigned long)++sampleSequence);
    manager.addData("Bringup","TestId",testId);
    good=manager.isPacketValid();
  }
  return finish(good,
         "PROM CRC4, fresh raw ADC reads and Loom MS5803-02BA readings checked. Compare with a reference for calibration; previous mux mask restored when possible.");
}
static bool rootBus(Check check) {
  if (!ready(check)) return false;
  if (!digitalRead(SDA) || !digitalRead(SCL)) { record(check,FAIL,"I2C line stuck LOW. Use i2c recover, or power down and isolate a branch; no scan performed."); return false; }
  if (!muxSelect(0)) { record(check,FAIL,"Mux did not ACK deselect. Confirm address straps (default 0x71), +3V3, RESET pull-up and bus pull-ups."); return false; }
  return true;
}
static void scan(bool *found,const bool *upstream) {
  for (uint8_t a=8;a<=0x77;++a) {
    bool ack=ping(a); if (found) found[a]=ack;
    if (ack) { Serial.print("  0x"); hex(a); if (upstream && upstream[a]) Serial.print(F(" upstream")); Serial.println(); }
    if (bus.fault) { Serial.println(F("Bus electrical timeout; scan stopped.")); return; }
  }
}
static void muxTest() {
  if (!rootBus(MUX)) return;
  bool root[128]={}; Serial.println(F("Root, all channels OFF:")); scan(root,nullptr);
  bool ok=!bus.fault;
  for (uint8_t ch=0;ch<8 && ok;++ch) {
    uint8_t mask=uint8_t(1u<<ch), got=0;
    Serial.print(F("Mux channel ")); Serial.println(ch);
    ok=muxSelect(mask) && muxRead(got) && got==mask;
    if (ok) { scan(nullptr,root); ok=!bus.fault; }
    if (!muxSelect(0)) ok=false;
  }
  uint8_t off=255; ok=muxSelect(0) && muxRead(off) && off==0 && ok;
  record(MUX,ok?PASS:FAIL,"One-hot control readback for 8 channels and final OFF. ACK lists require comparison with fitted sensors; register readback alone does not prove physical channel isolation.");
}
static void muxExpect(uint8_t channel,uint8_t address) {
  if (!rootBus(MUX)) return;
  bool upstream=ping(address); bool selected=muxSelect(uint8_t(1u<<channel));
  bool present=selected && ping(address); bool off=muxSelect(0);
  bool absent=off && !ping(address) && !bus.fault;
  record(MUX,!selected || !off ? FAIL : upstream ? BLOCKED : present && absent ? PASS : FAIL,
         "Expected device ACK on selected branch and NACK when isolated. Upstream/duplicate addresses cannot establish isolation with this check.");
}
static bool clockRead(uint32_t &epoch,uint8_t *status=nullptr) {
  uint8_t r[7],s=0;
  if (!regRead(RTC_ADDR,0,r,7) || !regRead(RTC_ADDR,15,&s,1) || !rtcEpoch(r,epoch)) return false;
  if (status) *status=s; return true;
}
static bool ensureRtc() {
  if (!rootBus(CLOCK)) return false;
  if (!hypnos.isRTCInitialized()) {
    Serial.println(F("RTC: initialize from sketch compile time when lost/older; preserve a newer clock."));
    Wire.begin(); hypnos.enable(true,true);
    Wire.end(); pinMode(SDA,INPUT); pinMode(SCL,INPUT);
  }
  uint32_t epoch=0; uint8_t status=0;
  if (!hypnos.isRTCInitialized() || !clockRead(epoch,&status) || (status&128)) {
    rtcGood=alarmGood=false;
    record(CLOCK,FAIL,"RTC unavailable. Use rtc set compile, rtc set <Unix UTC>, or rtc set YYYY-MM-DDTHH:MM:SSZ."); return false;
  }
  return true;
}
static void printRtcTime(uint32_t epoch) {
  DateTime utc(epoch); char text[24];
  snprintf(text,sizeof(text),"%04u-%02u-%02uT%02u:%02u:%02uZ",utc.year(),utc.month(),utc.day(),utc.hour(),utc.minute(),utc.second());
  Serial.print(F("RTC UTC: ")); Serial.print(text); Serial.print(F(" | Unix seconds: ")); Serial.println(epoch);
}
static void rtcCompile() {
  if (!rootBus(CLOCK)) return;
  Wire.begin(); bool ok=hypnos.setRTCFromCompileTime();
  Wire.end(); pinMode(SDA,INPUT); pinMode(SCL,INPUT);
  rtcGood=alarmGood=false;
  uint32_t epoch=0; ok=clockRead(epoch) && ok;
  if (ok) printRtcTime(epoch);
  record(CLOCK,ok?INFO:FAIL,"Explicit sketch compile-time seed (approximate upload time); run rtc test to verify ticking.");
}
static void rtcTest() {
  rtcGood=alarmGood=false;
  if (!ensureRtc()) return;
  uint32_t a=0,b=0; uint8_t status=0; rtcGood=false;
  if (!clockRead(a,&status)) { record(CLOCK,FAIL,"Invalid/unsupported RTC calendar (24-hour, 2000-2099 required) or I2C read failure."); return; }
  printRtcTime(a); Serial.print(F("Oscillator-stop flag: ")); Serial.println(bool(status&128));
  delay(2100);
  bool ok=clockRead(b) && b>=a && b-a>=2 && b-a<=3 && !(status&128);
  rtcGood=ok; record(CLOCK,ok?PASS:FAIL,"Calendar/tick and oscillator-stop flag checked. Use rtc set <UTC epoch> explicitly if time is invalid; accuracy/backup retention need longer bench tests.");
}
static void rtcSet(uint32_t epoch) {
  if (!rootBus(CLOCK)) return;
  // Establish the SDK RTC state first, so a subsequent default initialization
  // does not overwrite this deliberate manual timestamp with compile time.
  (void)ensureRtc();
  uint8_t bytes[8]={},status;
  if (!epochRtc(epoch,bytes+1) || !regRead(RTC_ADDR,15,&status,1)) { record(CLOCK,FAIL,"RTC epoch/status invalid."); return; }
  bool ok=bus.transfer(RTC_ADDR,bytes,8,nullptr,0) && regWrite(RTC_ADDR,15,status&0x7f);
  uint32_t verified=0; ok=clockRead(verified) && verified>=epoch && verified<=epoch+1 && ok;
  if (ok) printRtcTime(verified);
  rtcGood=alarmGood=false;
  record(CLOCK,ok?INFO:FAIL,"Explicit time write and OSF clear; run rtc test to verify ticking.");
}
static uint8_t savedAlarm[9];
static bool saveAlarm() { return regRead(RTC_ADDR,7,savedAlarm,sizeof(savedAlarm)); }
static bool restoreAlarm() {
  detachInterrupt(RTC_INT);
  uint8_t bytes[8]={7}; memcpy(bytes+1,savedAlarm,7);
  // Restore Alarm1/Alarm2 registers and control. Do not reassert latched flags.
  uint8_t status=0;
  bool ok=regWrite(RTC_ADDR,14,savedAlarm[7]&uint8_t(~3u));
  ok=bus.transfer(RTC_ADDR,bytes,8,nullptr,0) && ok;
  const bool statusRead=regRead(RTC_ADDR,15,&status,1);
  if (statusRead) ok=regWrite(RTC_ADDR,15,status&uint8_t(~3u)) && ok;
  else ok=false;
  ok=regWrite(RTC_ADDR,14,savedAlarm[7]) && ok;
  return ok;
}
static bool armAlarm(uint32_t seconds,uint32_t &start) {
  uint8_t status,control,r[7];
  if (!clockRead(start,&status) || (status&128) || !epochRtc(start+seconds,r) || !regRead(RTC_ADDR,14,&control,1)) {
    Serial.println(F("Alarm arm failed: RTC read/OSF/calendar/control check.")); return false;
  }
  // Disable both alarm interrupts, clear stale flags, replace A1 using exact
  // second/minute/hour/date fields, then enable only Alarm1 in interrupt mode.
  uint8_t b[]={7,r[0],r[1],r[2],r[4]};
  bool ok=regWrite(RTC_ADDR,14,control&uint8_t(~3u)) && regWrite(RTC_ADDR,15,status&uint8_t(~3u)) &&
          bus.transfer(RTC_ADDR,b,5,nullptr,0);
  pinMode(RTC_INT,INPUT_PULLUP);
  ok=ok && digitalRead(RTC_INT) && regWrite(RTC_ADDR,14,uint8_t((control&0x78)|5));
  uint8_t actual[4]={},configured=0;
  bool readback=regRead(RTC_ADDR,7,actual,4) && regRead(RTC_ADDR,14,&configured,1);
  bool matches=readback && !memcmp(actual,b+1,4) && (configured&7)==5;
  Serial.print(F("Alarm arm | readback=")); Serial.print(matches);
  Serial.print(F(" | control=0x")); hex(configured);
  Serial.print(F(" | D12=")); Serial.println(digitalRead(RTC_INT)?F("HIGH"):F("LOW"));
  return ok && matches && digitalRead(RTC_INT);
}
static void rtcIsr() {
  if (digitalRead(RTC_INT)!=LOW) return; // Reject a stale/mux-transition EIC flag.
  rtcWoke=true; detachInterrupt(RTC_INT);
}
static bool awaitAlarm(uint32_t seconds) {
  uint32_t start=0;
  if (!armAlarm(seconds,start)) return false;
  // A detached EIC channel can retain a flag. Clear only D12's mapped EXTINT3
  // before accepting a fresh alarm, rather than treating a stale IRQ as a wake.
  rtcWoke=false;
  const uint32_t priorMask=__get_PRIMASK(); __disable_irq();
  attachInterrupt(RTC_INT,rtcIsr,LOW);
  delayMicroseconds(200); // Let the EIC input synchronize after the pin-mux change.
  if (digitalRead(RTC_INT)==HIGH) {
    EIC->INTFLAG.reg=1ul<<g_APinDescription[RTC_INT].ulExtInt;
    NVIC_ClearPendingIRQ(EIC_IRQn);
  }
  __set_PRIMASK(priorMask);
  uint32_t t=millis();
  while (!rtcWoke && uint32_t(millis()-t)<(seconds+3)*1000) delay(1);
  detachInterrupt(RTC_INT);
  uint8_t status=0; uint32_t end=0;
  const bool pinLow=!digitalRead(RTC_INT);
  // Read these even if the MCU interrupt was missed; that distinction locates
  // the fault. Never replace unread status with zero on an I2C failure.
  const bool read=clockRead(end,&status);
  const bool clear=read && regWrite(RTC_ADDR,15,status&uint8_t(~3u));
  delay(20); const bool released=digitalRead(RTC_INT);
  Serial.print(F("Alarm result | requested s=")); Serial.print(seconds);
  Serial.print(F(" | MCU IRQ=")); Serial.print(rtcWoke); Serial.print(F(" | D12 LOW=")); Serial.print(pinLow);
  Serial.print(F(" | RTC read=")); Serial.print(read); Serial.print(F(" | A1F=")); Serial.print(bool(status&1));
  Serial.print(F(" | status=0x")); hex(status);
  if (read) { Serial.print(F(" | elapsed s=")); Serial.print(end-start); }
  Serial.print(F(" | clear=")); Serial.print(clear); Serial.print(F(" | D12 released=")); Serial.println(released);
  return rtcWoke && pinLow && read && (status&1) && end>=start+seconds && end<=start+seconds+2 && clear && released;
}
static void alarmTest() {
  if (!rootBus(ALARM)) return;
  if (!rtcGood) rtcTest();
  if (!rtcGood) { record(ALARM,FAIL,"RTC clock test failed; inspect the RTC result."); return; }
  if (!saveAlarm()) { record(ALARM,FAIL,"Cannot save alarm configuration."); return; }
  alarmGood=false;
  bool first=awaitAlarm(3), second=awaitAlarm(5);
  bool restored=restoreAlarm(); alarmGood=first && second && restored;
  Serial.print(F("Alarm diagnostic | 3s=")); Serial.print(first);
  Serial.print(F(" | 5s=")); Serial.print(second); Serial.print(F(" | restore=")); Serial.println(restored);
  record(ALARM,alarmGood?PASS:FAIL,"Two fresh alarms, latched A1F, physical LOW and HIGH after clearing, timed replacement, and configuration restoration. Alarm flags are consumed.");
}
static void hypnosWake() {
  if (__get_IPSR()!=0 && digitalRead(RTC_INT)!=LOW) return;
  rtcWoke=true;
  // The SDK may also invoke the callback from its awake overrun path.
  hypnosWakeIrq=__get_IPSR()!=0;
  hypnos.wakeup();
}

static bool storeSleepDeadline(const SleepDeadline &deadline) {
  pendingSleepDeadline.write(deadline);
  const SleepDeadline stored = pendingSleepDeadline.read();
  return memcmp(&deadline, &stored, sizeof(stored)) == 0;
}

static volatile bool resumeGuardExpired;
static void resumeGuardIsr() { resumeGuardExpired = true; }

static bool waitForRetainedAlarm(uint32_t originalSeconds) {
  pinMode(RTC_INT, INPUT_PULLUP);
  delayMicroseconds(200);
  if (digitalRead(RTC_INT) == LOW) return true;

  // This is the Feather's internal RTC. It only bounds the wait if the
  // battery-backed DS3231 lost its alarm. No bus access or rails are needed.
  RTCZero guard;
  resumeGuardExpired = false;
  guard.begin(true);
  guard.attachInterrupt(resumeGuardIsr);
  guard.setAlarmEpoch(guard.getEpoch() + originalSeconds + 3);
  guard.enableAlarm(RTCZero::MATCH_YYMMDDHHMMSS);

  rtcWoke = false;
  LowPower.attachInterruptWakeup(RTC_INT, rtcIsr, LOW);
  USBDevice.detach();
  const uint32_t tick = SysTick->CTRL;
  const uint32_t sleepControl = SCB->SCR;
  SysTick->CTRL = tick & ~SysTick_CTRL_TICKINT_Msk;
  SCB->SCR = sleepControl | SCB_SCR_SLEEPDEEP_Msk;
  for (;;) {
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    const bool done = resumeGuardExpired || digitalRead(RTC_INT) == LOW;
    // A pending IRQ can wake WFI under PRIMASK. This closes the gap between
    // checking the alarm and entering standby.
    if (!done) {
      __DSB();
      __WFI();
    }
    __set_PRIMASK(mask);
    __ISB();
    if (done) break;
  }
  SCB->SCR = sleepControl;
  SysTick->CTRL = tick;
  const bool asserted = digitalRead(RTC_INT) == LOW;
  detachInterrupt(RTC_INT);
  EIC->WAKEUP.reg &= ~(1ul << g_APinDescription[RTC_INT].ulExtInt);
  guard.disableAlarm();
  guard.detachInterrupt();
  USBDevice.attach();
  return asserted;
}

static const __FlashStringHelper *resumeSleepOnBoot() {
  const SleepDeadline deadline = pendingSleepDeadline.read();
  if (!deadline.valid(MAX_SLEEP_SECONDS)) return nullptr;

  const __FlashStringHelper *result = F("Saved sleep: D12 alarm timed out. Rails stayed off.");
  if (waitForRetainedAlarm(deadline.wakeAt - deadline.armedAt)) {
    // D12 is already asserted. Bring up the I2C pull-ups only now, then
    // verify the saved deadline and acknowledge Alarm 1 without setting time.
    output(RAIL3, LOW);
    delay(600);
    uint32_t now = 0;
    uint8_t status = 0, control = 0;
    const bool due = muxSelect(0) && clockRead(now, &status) && deadline.alarmDue(now, status);
    if (due && regRead(RTC_ADDR, 14, &control, 1) &&
        (control & 0x05) == 0x05 &&
        regWrite(RTC_ADDR, 14, control & ~1u) &&
        regWrite(RTC_ADDR, 15, (status | 0x82u) & ~1u)) {
      setRails(deadline.rails & SleepDeadline::rail3, deadline.rails & SleepDeadline::rail5,
               deadline.rails & SleepDeadline::rail12);
      result = F("Saved sleep: DS3231 alarm verified. Prior rails restored.");
    } else {
      safeOff();
      result = F("Saved sleep: D12 asserted but RTC alarm verification failed. Rails are off.");
    }
  }
  if (!storeSleepDeadline({})) return F("Saved sleep handled, but flash clear failed.");
  return result;
}

static void sleepTest(uint32_t seconds,uint8_t cycles) {
  if (pendingReply.active || pingPending) { record(STANDBY_TEST,BLOCKED,"Finish the pending modem command/ping before sleep."); return; }
  if (seconds<3 || seconds>MAX_SLEEP_SECONDS) { record(STANDBY_TEST,BLOCKED,"Sleep duration must be 3 seconds through 24 hours."); return; }
  if (!ensureRtc()) return;
  const bool restoreUart=uartOpen,restore3=rail3On,restore5=rail5On,restore12=rail12On;
  const uint32_t restoreRails = (restore3 ? SleepDeadline::rail3 : 0) |
                                (restore5 ? SleepDeadline::rail5 : 0) |
                                (restore12 ? SleepDeadline::rail12 : 0);
  if (!saveAlarm()) { record(STANDBY_TEST,FAIL,"Cannot save RTC alarm configuration."); return; }
  const POWERRAIL_CONFIG priorSleep=hypnos.getSleepConfiguration();
  hypnos.setSleepConfiguration(PR_3V_OFF_5V_OFF);
  // Follow examples/Hypnos/Hypnos_Sleep. Hardware Wire belongs to the SDK
  // throughout this sequence; the software I2C diagnostic bus is idle.
  Wire.begin();
  bool ok=hypnos.registerInterrupt(hypnosWake,RTC_INT,SLEEP,LOW);
  for (uint8_t cycle=0;cycle<cycles && ok;++cycle) {
    DateTime start,deadline,end;
    output(SENSOR12_EN,false);
    if (!prepareSleepLte()) { ok=false; break; }
    ok=hypnos.tryGetCurrentTime(start); if (!ok) break;
    rtcWoke=hypnosWakeIrq=false;
    hypnos.setInterruptDuration(TimeSpan(int32_t(seconds)));
    ok=hypnos.getScheduledWakeTime(deadline) && hypnos.reattachRTCInterrupt();
    if (!ok) { Serial.println(F("Hypnos could not schedule/attach the RTC wake.")); break; }
    if (!storeSleepDeadline(SleepDeadline::make(deadline.unixtime()-seconds, deadline.unixtime(), restoreRails))) {
      Serial.println(F("Could not save the wake deadline in flash."));
      (void)storeSleepDeadline({});
      ok=false; break;
    }
    Serial.print(F("Hypnos sleep cycle ")); Serial.print(cycle+1); Serial.print(F(" | requested seconds=")); Serial.println(seconds);
    Serial.println(F("CALL hypnos.sleep(); ISR calls hypnos.wakeup(). USB may reconnect."));
    Serial.flush();
    hypnos.sleep();
    const bool cleared=storeSleepDeadline({});
    const bool read=hypnos.tryGetCurrentTime(end);
    uint32_t wait=millis(); while (!Serial && uint32_t(millis()-wait)<4000) delay(10);
    Serial.print(F("Hypnos returned | callback=")); Serial.print(rtcWoke);
    Serial.print(F(" | IRQ callback=")); Serial.print(hypnosWakeIrq);
    Serial.print(F(" | RTC read=")); Serial.print(read);
    Serial.print(F(" | INT pin=")); Serial.print(digitalRead(RTC_INT)?F("HIGH"):F("LOW"));
    if (read) { Serial.print(F(" | elapsed RTC seconds=")); Serial.print(end.unixtime()-start.unixtime()); }
    Serial.println();
    if (!cleared) Serial.println(F("Could not clear the saved wake deadline."));
    ok=cleared && read && rtcWoke && hypnosWakeIrq && end.unixtime()>=deadline.unixtime() && digitalRead(RTC_INT);
  }
  Wire.end(); pinMode(SDA,INPUT); pinMode(SCL,INPUT);
  ok=restoreAlarm() && ok;
  hypnos.setSleepConfiguration(priorSleep);
  setRails(restore3,restore5,restore12);
  if (restoreUart) { Serial1.begin(MODEM_BAUD); uartOpen=true; }
  dataReady=false;
  record(STANDBY_TEST,ok?PASS:FAIL,"Basic Loom Hypnos sleep/wakeup callback and RTC timing checked; SDK owns alarms, standby and power. Standby current needs separate measurement. Prior rails/UART restored.");
}
static bool mountSd() {
  if (!ready(SD_READ)) return false;
  output(SD_CS,true);
  bool ok=sd.begin(SD_CS,SD_SCK_MHZ(4));
  if (!ok) { output(SD_CS,true); SPI.end(); }
  record(SD_READ,ok?PASS:FAIL,ok ? "Hardware Feather SPI, D11 select; FAT16/FAT32 mounted (no formatting)." :
         "No card, SD wiring/power fault or unsupported filesystem. SD is optional; other tests continue. Use baseline nosd to skip this probe.");
  if (ok) { Serial.print(F("Card sectors=")); Serial.println(sd.card()->cardSize()); }
  return ok;
}
#include "StorageBench.h"
static void spiTest() {
  if (!ready(SPI_LOOP)) return;
  output(SD_CS,true); SPI.begin(); bool ok=true;
  const uint32_t rates[]={100000,1000000,4000000};
  const uint8_t modes[]={SPI_MODE0,SPI_MODE1,SPI_MODE2,SPI_MODE3};
  // Only run after inserting MOSI-MISO jumper at SPI1 with all slaves deselected.
  for (uint32_t hz:rates) for (uint8_t mode:modes) {
    SPI.beginTransaction(SPISettings(hz,MSBFIRST,mode)); uint16_t errors=0;
    for (uint16_t i=0;i<512;++i) { uint8_t tx=uint8_t(i*73u+19u); if (SPI.transfer(tx)!=tx) ++errors; }
    SPI.endTransaction(); Serial.print(F("SPI Hz=")); Serial.print(hz); Serial.print(F(" mode=")); Serial.print(mode); Serial.print(F(" mismatches=")); Serial.println(errors); ok=ok && !errors;
  }
  record(SPI_LOOP,ok?PASS:FAIL,"12 hardware SPI speed/mode combinations with MOSI-MISO jumper, 512 bytes each. Remove jumper before SD tests. This does not validate the older J18 SPI routes.");
}
static bool modemAlreadyAwake() {
  // The modem's unswitched supply survives a Feather reset/reflash. Probe AT
  // before toggling PWR_ON so a healthy running modem is adopted in place.
  uint32_t start=millis();
  while (Serial1.available() && uint32_t(millis()-start)<50) Serial1.read();
  Serial.println(F("LTE first use: checking AT before a power pulse."));
  Serial1.print("AT\r");
  PendingAtReply probe; probe.begin("",false); start=millis();
  while (uint32_t(millis()-start)<1500) {
    for (uint8_t batch=0;batch<64 && Serial1.available();++batch) {
      if (probe.feed(Serial1.read())) return lineEquals(probe.line,"OK");
    }
    delay(1);
  }
  return false;
}
static bool lteOpen() {
  if (!ready(LTE_AT)) return false;
  if (!uartOpen) { Serial1.begin(MODEM_BAUD); uartOpen=true; }
  if (!pulseSent) {
    if (modemAlreadyAwake()) {
      pulseSent=true;
      Serial.println(F("LTE already replied OK; keeping it awake without a PWR_ON pulse."));
    } else ltePulse();
  }
  return true;
}
static bool cancelRegistration() {
  if (registrationCanCancel) for (uint8_t i=0;i<64 && Serial.available();++i) {
    if (Serial.read()==29) registrationCancelled=true;
  }
  return registrationCancelled;
}
static void pingByte(int ch) {
  if (!pingPending) return;
  if (ch=='\n') {
    pingLine[pingUsed]=0;
    if (!strncmp(pingLine,"+UUPING:",8) || !strncmp(pingLine,"+UUPINGER:",10)) {
      pingAnswered=benchPingReply(pingLine,pingRtt) && pingRtt>=0; pingPending=false;
    }
    pingUsed=0;
  } else if (pingUsed+1<sizeof(pingLine)) pingLine[pingUsed++]=char(ch);
}
static void modemByte(int ch) {
  bool hidden=pendingReply.active && pendingReply.hidden;
  if (!hidden) Serial.write(uint8_t(ch));
  pingByte(ch);
  if (pendingReply.feed(ch)) Serial.println(F("\nPrevious modem command completed; AT console available again."));
}
static bool at(const char *cmd,uint32_t timeout=3000,bool sensitive=false,CopsScanCounter *scan=nullptr) {
  lastAtCompleted=false;
  if (!lteOpen()) return false;
  if (pingPending) { Serial.println(F("Previous ping result still pending; wait for its modem response.")); return false; }
  if (pendingReply.active) { Serial.println(F("BLOCKED: previous AT command still pending. Wait for its terminal response; do not overlap commands.")); return false; }
  uint32_t drain=millis(); while (Serial1.available() && uint32_t(millis()-drain)<50) Serial1.read();
  Serial.print(F("HOST> ")); Serial.println(sensitive?"<authentication command hidden>":cmd); Serial1.print(cmd); Serial1.print('\r');
  if (scan) Serial.println(F("MODEM> (live scan output)"));
  size_t used=0; bool overflow=false,terminalOk=false; uint32_t start=millis(); reply[0]=0;
  PendingAtReply transaction; transaction.begin("",sensitive);
  while (uint32_t(millis()-start)<timeout) {
    for (uint8_t batch=0;batch<64 && Serial1.available();++batch) { int c=Serial1.read();
      if (scan) { Serial.write(uint8_t(c)); scan->feed(c); }
      if (used+1<sizeof(reply)) reply[used++]=char(c); else overflow=true;
      if (transaction.feed(c)) { lastAtCompleted=true; terminalOk=lineEquals(transaction.line,"OK"); break; } }
    reply[used]=0;
    if (lastAtCompleted) break;
    if (cancelRegistration()) break;
    delay(1);
  }
  if (scan) { Serial.println(); if (!lastAtCompleted) Serial.println(F("Scan wait ended before a terminal modem reply.")); }
  else { Serial.print(F("MODEM> ")); Serial.println(sensitive?"<authentication response hidden>":used?reply:"<timeout>"); }
  // A silent bare AT probe may simply mean a missing/off device; allow retry.
  // Keep late-reply tracking for all other transactions, including authentication.
  bool silentProbe=!strcmp(cmd,"AT") && !used && !registrationCanCancel;
  if (!lastAtCompleted && !silentProbe) pendingReply=transaction;
  bool ok=lastAtCompleted && (!overflow || scan) && terminalOk;
  if (sensitive) memset(reply,0,sizeof(reply));
  return ok;
}
class BenchRegistrationIO : public RegistrationIO {
  bool command(const char *text,uint32_t timeout,bool sensitive=false) override { return at(text,timeout,sensitive); }
  const char *response() const override { return reply; }
  bool completed() const override { return lastAtCompleted; }
  bool cancelled() const override { return registrationCancelled; }
  uint32_t now() const override { return millis(); }
  void pause(uint32_t ms) override {
    uint32_t start=millis(); while (uint32_t(millis()-start)<ms && !cancelRegistration()) delay(5);
  }
  void phase(const char *text,bool canCancel) override { registrationCanCancel=canCancel; Serial.println(text); if (canCancel) cancelRegistration(); }
};
static NetworkCredentials credentials() { return {networkApn(),NETWORK_USER,NETWORK_PASS,NETWORK_AUTH}; }
static void credentialStatus() {
  Serial.print(F("APN: ")); Serial.println(networkApn());
  Serial.println(NETWORK_NAME[0]?F("Source: NETWORK_NAME in arduino_secrets.h"):F("Source: hologram fallback; local NETWORK_NAME is empty."));
  Serial.print(F("Username configured: ")); Serial.println(bool(NETWORK_USER[0]));
  Serial.print(F("Password configured: ")); Serial.println(bool(NETWORK_PASS[0]));
  Serial.print(F("Authentication type: ")); Serial.println(NETWORK_AUTH);
  Serial.println(F("APN authentication is separate from SIM PIN and network registration. No PIN guesses are made."));
}
static void lteRegister(bool att) {
  if (!lteHello()) return;
  if (pendingReply.active) { Serial.println(F("Wait for previous modem command's terminal response first.")); return; }
  registrationCancelled=false; registrationCanCancel=false;
  credentialStatus();
  Serial.println(F("Registration recovery runs only if not already registered; it may change APN/operator settings and enable RF. Ctrl+] cancels during network search."));
  BenchRegistrationIO io; RegistrationResult result=registerIfNeeded(io,credentials(),att);
  registrationCanCancel=false;
  const char *details[]={
    "Already registered home/roaming. Existing APN, operator and MNO profile left unchanged.",
    "CEREG confirms home/roaming after conditional APN/operator recovery. Packet delivery is a separate test.",
    "AT readiness failed; use lte pulse if the modem is off, or inspect UART/power.",
    "SARA-R5 identity not confirmed; no carrier changes made.",
    "SIM not READY (missing, locked or busy). Resolve CPIN/account first; no unlock attempts made.",
    "Registration query unavailable/malformed; registration cannot be inferred.",
    "APN/authentication fields invalid or too long. Inspect arduino_secrets.h; no credential values printed.",
    "EPS context/RF configuration failed; inspect terminal errors and AT+CFUN? before retrying. No reset or GPIO pulse issued.",
    "Operator selection rejected and CEREG still unregistered; use automatic selection or inspect account/coverage/profile.",
    "Network denied registration. Check SIM activation/plan, roaming, signal and selected operator; no automatic retry loop.",
    "Registration deadline or AT timeout reached. A pending command blocks new AT until its final response arrives.",
    "Registration search cancelled. RF/operator changes already sent are retained; pending AT must finish before another command."
  };
  if (result==RegistrationResult::NO_AT || result==RegistrationResult::WRONG_MODEL) {
    record(LTE_AT,FAIL,details[uint8_t(result)]); record(SIM,BLOCKED,"Need SARA-R5 AT/model readiness before judging SIM state.");
  }
  else { record(LTE_AT,PASS,"AT and SARA-R5 identity verified before registration handling.");
    record(SIM,result==RegistrationResult::SIM_NOT_READY?BLOCKED:PASS,"CPIN readiness checked; SIM authentication is separate from carrier attachment."); }
  bool blocked=result==RegistrationResult::NO_AT || result==RegistrationResult::WRONG_MODEL || result==RegistrationResult::SIM_NOT_READY ||
               result==RegistrationResult::INVALID_CREDENTIALS || result==RegistrationResult::CANCELLED;
  record(NET,registrationSucceeded(result)?PASS:blocked?BLOCKED:FAIL,details[uint8_t(result)]);
  if (!pendingReply.active && result!=RegistrationResult::CANCELLED) { at("AT+COPS?"); at("AT+UMNOPROF?"); at("AT+CGATT?"); at("AT+CGPADDR=1"); }
  registrationCancelled=false;
}
static bool lteHello() {
  if (pendingReply.active) { record(LTE_HELLO,BLOCKED,"Previous AT command still pending; wait for its terminal reply."); return false; }
  bool ok=at("AT");
  record(LTE_HELLO,ok?PASS:FAIL,ok ? "Modem replied OK to AT; awake and UART responsive." : "No terminal OK to AT. Check modem power/UART; use lte pulse explicitly if needed.");
  if (ok) configureLteLeds();
  return ok;
}
static bool lteSignal() {
  uint8_t values[6]={};
  bool csq=at("AT+CSQ");
  if (pendingReply.active) return false;
  bool cesq=at("AT+CESQ");
  bool decoded=cesq && scanNumbers(reply,"+CESQ:",values,6) &&
               (values[4]<=34 || values[4]==255) && (values[5]<=97 || values[5]==255);
  if (decoded) {
    Serial.print(F("LTE RSRP: "));
    if (values[5]==255) Serial.println(F("unknown / no serving-cell measurement"));
    else if (!values[5]) Serial.println(F("below -140 dBm"));
    else if (values[5]==97) Serial.println(F("-44 dBm or stronger"));
    else { Serial.print(int(values[5])-141); Serial.print(F(" to <")); Serial.print(int(values[5])-140); Serial.println(F(" dBm")); }
    Serial.print(F("LTE RSRQ: "));
    if (values[4]==255) Serial.println(F("unknown / no serving-cell measurement"));
    else if (!values[4]) Serial.println(F("below -19.5 dB"));
    else if (values[4]==34) Serial.println(F("-3 dB or better"));
    else { Serial.print(-20.0f+values[4]*0.5f,1); Serial.print(F(" to <")); Serial.print(-19.5f+values[4]*0.5f,1); Serial.println(F(" dB")); }
  } else Serial.println(F("Signal measurement unavailable/malformed; a network scan can still find operators."));
  return csq && decoded;
}
static void lteScan() {
  if (pendingReply.active) { record(LTE_SCAN,BLOCKED,"Previous AT command still pending; wait for its terminal response."); return; }
  Serial.println(F("Connect the LTE antenna before RF testing. Scan results show reception, not antenna continuity or calibration."));
  if (!lteHello()) { record(LTE_SCAN,BLOCKED,"Modem did not answer AT; use lte on to run the power-on pulse."); return; }
  uint8_t mode[2]={};
  if (!at("AT+CFUN?") ||
      (!scanNumbers(reply,"+CFUN:",mode,2) && !scanNumbers(reply,"+CFUN:",mode,1))) {
    record(LTE_SCAN,FAIL,"Cannot read modem RF functionality; no scan started."); return;
  }
  if (mode[0]!=1) {
    record(LTE_SCAN,BLOCKED,"RF is not in full functionality. With the antenna attached, explicitly send AT+CFUN=1 and retry. Scan does not change RF mode."); return;
  }
  lteSignal();
  if (pendingReply.active) { record(LTE_SCAN,FAIL,"Signal query timed out; waiting for its terminal reply before another AT command."); return; }
  bool simReady=at("AT+CPIN?") && lineEquals(reply,"+CPIN: READY");
  if (!simReady) { record(LTE_SCAN,BLOCKED,"Signal queries shown above; the operator-list scan requires a READY SIM. No PIN guesses or registration changes made."); return; }
  CopsScanCounter scan;
  Serial.println(F("Searching operators with AT+COPS=? (up to 180 s). This can interrupt service. Ctrl+] ends the console wait; a pending modem scan must finish before another AT command."));
  registrationCancelled=false; registrationCanCancel=true;
  bool ok=at("AT+COPS=?",180000,false,&scan);
  registrationCanCancel=false;
  bool cancelled=registrationCancelled; registrationCancelled=false;
  Serial.print(F("Complete operator entries observed: ")); Serial.println(scan.operators);
  if (cancelled) { record(LTE_SCAN,INFO,"Scan wait cancelled. No operator/APN settings changed; wait for any pending modem response before retrying."); return; }
  if (!ok) { record(LTE_SCAN,FAIL,"Scan ERROR/timeout is not an antenna diagnosis: check coverage, SIM and RF availability. Wait for any pending reply before retrying."); return; }
  record(LTE_SCAN,scan.operators?PASS:INFO,scan.operators ?
         "Operator signals received. This supports RF reception, but does not prove antenna attachment, matching, transmit performance or registration." :
         "Scan completed with no valid operator entries. Check antenna, coverage, supported bands and RF state; this alone does not prove an antenna fault.");
  lteSignal();
}
static void lteTest() {
  bool ok=lteHello() && at("AT+CGMM") && strstr(reply,"SARA-R5");
  record(LTE_AT,ok?PASS:FAIL,"AT terminal OK plus SARA-R5 model prefix. No automatic power pulse, reset, operator selection or registration change.");
  if (!ok) { record(SIM,BLOCKED,"Need model/AT first."); record(NET,BLOCKED,"Need model/AT first."); return; }
  at("AT+CGMR"); bool sim=at("AT+CPIN?") && lineEquals(reply,"+CPIN: READY");
  record(SIM,sim?PASS:BLOCKED,"SIM state only; no PIN attempts.");
  at("AT+CSQ"); at("AT+CESQ"); bool registered=at("AT+CEREG?") && registration(reply);
  record(NET,registered?PASS:INFO,"Existing registration state. This does not prove packet data or remote delivery.");
  if (!registered) Serial.println(F("Use lte register for conditional automatic recovery, or lte register att for AT&T 310410. Existing registration is always preserved."));
  at("AT+CGATT?"); at("AT+CGDCONT?"); at("AT+CGACT?"); at("AT+UGPS?");
}
#include "NetworkBench.h"
static void lteCommand(char *a,char *b) {
  if (a && !strcmp(a,"hello")) { lteHello(); return; }
  if (a && !strcmp(a,"scan")) { lteScan(); return; }
  if (!a || !strcmp(a,"test")) { lteTest(); return; }
  if (!strcmp(a,"credentials")) { credentialStatus(); return; }
  if (!strcmp(a,"register")) {
    if (b && strcmp(b,"auto") && strcmp(b,"att")) { Serial.println(F("lte register [auto|att]")); return; }
    lteRegister(b && !strcmp(b,"att")); return;
  }
  if (!strcmp(a,"quiet")) { if (uartOpen) Serial1.end(); uartOpen=false; rawBridge=false; pinMode(0,INPUT); pinMode(1,INPUT); return; }
  if (!strcmp(a,"pins")) { Serial.println(F("D1 TX -> U4 B1/A1 -> SARA TXD pin12 INPUT; SARA RXD pin13 OUTPUT -> U4 A2/B2 -> D0 RX. SARA uses DCE names.")); return; }
  if (!ready(LTE_AT)) return;
  if (!strcmp(a,"pulse") || !strcmp(a,"on")) { ltePulse(); lteHello(); return; }
  if (!strcmp(a,"leds")) { ledsConfigured=false; if (lteOpen() && at("AT")) configureLteLeds(); return; }
  if (!strcmp(a,"connect") || !strcmp(a,"loom")) { connectData(); return; }
  if (!strcmp(a,"ping")) { ltePing(b); return; }
  if (!strcmp(a,"mqtt")) { mqttTest(); return; }
  if (!strcmp(a,"bridge") && lteOpen()) { if (pendingReply.active) { Serial.println(F("Wait for the pending AT command to complete first.")); return; } rawBridge=true; Serial.println(F("Raw UART bridge, Ctrl+] exits. AT commands may alter modem/network settings.")); return; }
  Serial.println(F("lte hello | on | leds | test | scan | connect | ping [host] | mqtt | register [auto|att] | credentials | quiet | pins | bridge"));
}
static void analogTest(const char *name,const char *known) {
  if (!ready(ANALOG)) return;
  if (!name || strlen(name)!=2 || name[0]!='A' || name[1]<'0' || name[1]>'2') { Serial.println(F("analog A0|A1|A2 [known volts], 0..3.3V ONLY; A3/A4/A5 and D9 are controls/data.")); return; }
  uint8_t pin=uint8_t(A0+name[1]-'0'); pinMode(pin,INPUT);
  analogReadResolution(12); analogReference(AR_DEFAULT); analogRead(pin);
  uint16_t lo=4095,hi=0; uint32_t sum=0;
  for (uint8_t i=0;i<64;++i) { uint16_t v=analogRead(pin); if (v<lo) lo=v; if (v>hi) hi=v; sum+=v; delay(2); }
  float volts=(sum/64.0f)*adcReference/4095.0f, expected;
  Serial.print(name); Serial.print(F(" min/max/mean=")); Serial.print(lo); Serial.print('/'); Serial.print(hi); Serial.print('/'); Serial.print(sum/64.0f,2); Serial.print(F(" estimated volts=")); Serial.println(volts,4);
  if (!known) record(ANALOG,INFO,"64 samples only; enter known voltage for comparison. Do not attach raw 5/12V rails to ADC.");
  else if (!real(known,expected) || expected<0 || expected>adcReference) record(ANALOG,BLOCKED,"Invalid known voltage.");
  else record(ANALOG,fabsf(volts-expected)<=0.05f && (hi-lo)*adcReference/4095.0f<=0.05f?PASS:FAIL,"Known input comparison within 50 mV and peak-to-peak <=50 mV; calibrate reference, test GND/midscale/fullscale on every analog channel.");
}
static void sdiReceive() { pinMode(SDI_DATA,INPUT); output(SDI_DIR,false); }
static void sdiPower(bool enabled) {
  sdiReceive(); output(SENSOR12_EN,enabled); rail12On=enabled;
  delay(enabled?1500:20);
}
static constexpr uint32_t SDI_BIT_US = 833;

static bool bitWait(uint32_t deadline) {
  while (int32_t(micros()-deadline)<0) {}
  return uint32_t(micros()-deadline)<200;
}

static const char *sdiSend(const char *command, bool sendBreak) {
  if (digitalRead(SDI_DATA)) return "Data is HIGH before transmit. Expected LOW in receive mode.";
  // Release U7's A output before driving D9. Hold the data line LOW
  // while changing direction so a retry does not generate another break.
  output(SDI_DIR,true);
  output(SDI_DATA,false);
  if (sendBreak) {
    digitalWrite(SDI_DATA,HIGH); delay(13);
    digitalWrite(SDI_DATA,LOW); delay(9);
  }
  for (const char *p=command; *p; ++p) {
    const uint8_t byte = uint8_t(*p);
    uint8_t parity = 0;
    uint32_t tick = micros();
    digitalWrite(SDI_DATA,HIGH);
    for (uint8_t bit=0; bit<7; ++bit) {
      tick += SDI_BIT_US;
      if (!bitWait(tick)) return "Transmit timing overrun.";
      const bool one = (byte >> bit) & 1;
      parity ^= uint8_t(one);
      digitalWrite(SDI_DATA,!one);
    }
    tick += SDI_BIT_US;
    if (!bitWait(tick)) return "Transmit parity timing overrun.";
    digitalWrite(SDI_DATA,!parity);
    tick += SDI_BIT_US;
    if (!bitWait(tick)) return "Transmit stop-bit timing overrun.";
    digitalWrite(SDI_DATA,LOW);
    if (!bitWait(tick+SDI_BIT_US)) return "Transmit frame timing overrun.";
  }
  return nullptr;
}

static const char *sdiReadFrame(const char *command, char *response, size_t capacity, bool &silent) {
  const uint32_t started = micros();
  size_t used = 0;
  while (uint32_t(micros()-started)<1500000) {
    const uint32_t waitStarted = micros();
    while (!digitalRead(SDI_DATA)) {
      if (uint32_t(micros()-waitStarted) >= (used ? 10000u : 20000u)) {
        silent = used == 0;
        return silent ? "No response start bit." : "Reply ended before CRLF.";
      }
    }
    uint32_t tick = micros() + SDI_BIT_US/2;
    if (!bitWait(tick)) return "Start-bit sampling overrun.";
    if (!digitalRead(SDI_DATA)) return "Invalid start bit.";
    uint8_t byte = 0, parity = 0;
    for (uint8_t bit=0; bit<7; ++bit) {
      tick += SDI_BIT_US;
      if (!bitWait(tick)) return "Data-bit sampling overrun.";
      const bool one = !digitalRead(SDI_DATA);
      byte |= uint8_t(one) << bit;
      parity ^= uint8_t(one);
    }
    tick += SDI_BIT_US;
    if (!bitWait(tick)) return "Parity sampling overrun.";
    if (bool(!digitalRead(SDI_DATA)) != bool(parity)) return "Even-parity error.";
    tick += SDI_BIT_US;
    if (!bitWait(tick)) return "Stop-bit sampling overrun.";
    if (digitalRead(SDI_DATA)) return "Framing error: stop bit was HIGH.";
    if (used+1 >= capacity) return "Reply buffer overflow.";
    if ((byte<32 && byte!='\r' && byte!='\n') || byte>126) return "Unexpected reply character.";
    response[used++] = char(byte);
    response[used] = 0;
    if (byte == '\n') {
      if (used<3 || response[used-2]!='\r') return "Reply did not end in CRLF.";
      const bool addressOk = command[0]=='?' ? isalnum(uint8_t(response[0])) : response[0]==command[0];
      return addressOk ? nullptr : "Reply address did not match.";
    }
    // The stop bit was sampled at its midpoint. Look for the next start
    // immediately so its edge is captured even with back-to-back bytes.
  }
  return "Reply deadline exceeded.";
}

static bool sdiExchange(const char *command, char *response, size_t capacity) {
  sdiReceive();
  const char *error = nullptr;
  uint8_t attempts = 0;
  bool silent = false;
  // With no reply, retry 65 ms after the last stop bit without a new break.
  // The third send is after the full 100 ms wake window, even for ?! or a!.
  constexpr uint8_t attemptsPerBreak = 3, breakCount = 3;
  constexpr uint32_t retryDelayUs = 65000;
  for (uint8_t attempt=0; attempt<attemptsPerBreak*breakCount; ++attempt) {
    const bool sendBreak = attempt % attemptsPerBreak == 0;
    if (attempt && sendBreak) delay(25);
    ++attempts;
    response[0] = 0;
    silent = false;
    error = sdiSend(command,sendBreak);
    const uint32_t txEnded = micros();
    sdiReceive();
    if (error) break;
    error = sdiReadFrame(command,response,capacity,silent);
    if (!error || !silent) break;
    if (attempt+1<attemptsPerBreak*breakCount) {
      const uint32_t elapsed = micros()-txEnded;
      if (elapsed<retryDelayUs) delay((retryDelayUs-elapsed+999)/1000);
    }
  }
  if (error || attempts>1) {
    Serial.print(F("SDI ")); Serial.print(command);
    Serial.print(F(" attempts=")); Serial.print(attempts);
    if (error) {
      Serial.print(F(" | ")); Serial.print(error);
      if (*response) {
        Serial.print(F(" | reply bytes:"));
        for (const char *p=response; *p; ++p) { Serial.print(' '); hex(uint8_t(*p)); }
      }
    }
    Serial.println();
  }
  return !error;
}
static void sdiCommand(const char *cmd) {
  if (!ready(SDI)) return;
  if (!cmd || strlen(cmd)>32 || strlen(cmd)<2 || cmd[strlen(cmd)-1]!='!') { Serial.println(F("sdi <ASCII command ending !>, e.g. sdi 0I! or sdi 0M! then sdi 0D0!")); return; }
  for (const char *p=cmd;*p;++p) if (*p<33 || *p>126) { Serial.println(F("Invalid SDI command.")); return; }
  char response[128]; bool ok=sdiExchange(cmd,response,sizeof(response));
  if (ok) { Serial.print(F("SDI response: ")); Serial.print(response); }
  record(SDI,ok?PASS:FAIL,ok ? "Address and complete 1200-baud 7E1 reply verified." : "Command exchange failed. See the SDI line above.");
}
static void sdiMeasure(const char *address, loomSDI12::Model model = loomSDI12::Model::Unknown) {
  if (!ready(SDI)) return;
  if (!address || strlen(address)!=1 || !isalnum(uint8_t(*address))) { record(SDI,BLOCKED,"sdi measure <single address 0-9,a-z,A-Z>"); return; }
  char cmd[]={*address,'I','!',0},response[128];
  const uint8_t modelValues = loomSDI12::valueCount(model);
  if (modelValues) {
    if (!sdiExchange(cmd,response,sizeof(response))) {
      record(SDI,FAIL,"Sensor did not return a valid identification reply."); return;
    }
    Serial.print(F("SDI I response: ")); Serial.print(response);
    if (strlen(response)<22 || loomSDI12::identifyModel(response)!=model) {
      record(SDI,FAIL,"SDI identification did not match the selected TEROS model. Use sdi <address>I! to inspect.");
      return;
    }
    delay(10);
  }
  cmd[1]='M';
  uint16_t seconds=0; uint8_t expected=0;
  if (!sdiExchange(cmd,response,sizeof(response))) {
    record(SDI,FAIL,"Measurement command exchange failed. See the SDI line above."); return;
  }
  Serial.print(F("SDI M response: ")); Serial.print(response);
  if (!sdiMeasurement(response,seconds,expected)) {
    record(SDI,FAIL,"Expected atttn CRLF with a nonzero value count."); return;
  }
  bool ok = true;
  if (modelValues && expected!=modelValues) {
    record(SDI,FAIL,"Reported measurement count does not match the selected TEROS model."); return;
  }
  if (seconds>60) { record(SDI,BLOCKED,"Sensor requested >60 seconds; use manual aDn! retrieval after the advertised delay."); return; }
  Serial.print(F("Waiting advertised measurement interval: ")); Serial.println(seconds);
  for (uint16_t i=0;i<seconds;++i) { delay(1000); Serial.print('.'); } Serial.println();
  delay(10);
  uint8_t total=0;
  float values[9]={};
  for (uint8_t block=0;block<10 && total<expected;++block) {
    char dataCmd[]={*address,'D',char('0'+block),'!',0};
    if (!sdiExchange(dataCmd,response,sizeof(response))) { ok=false; break; }
    Serial.print(F("SDI D response: ")); Serial.print(response);
    int count=sdiValueCount(response);
    if (count<=0 || total+count>expected) { ok=false; break; }
    if (modelValues) {
      const char *p=response+1;
      for (int i=0;i<count;++i) {
        char *end=nullptr;
        values[total+i]=strtof(p,&end);
        p=end;
      }
    }
    total=uint8_t(total+count);
    delay(10);
  }
  if (ok && total==expected && modelValues) {
    Serial.print(F("TEROS water-content counts=")); Serial.print(values[0],3);
    Serial.print(F(" temperature C=")); Serial.print(values[1],3);
    if (model==loomSDI12::Model::TER12) { Serial.print(F(" bulk EC uS/cm=")); Serial.print(values[2],3); }
    Serial.println();
  }
  record(SDI,ok && total==expected?PASS:FAIL,"aM! advertised wait followed by aD0!..aD9!, matching address, strict signed decimal syntax and exact reported value count. Compare raw values with a reference; no CRC variant or calibration certification.");
}
#include "SensorBench.h"
#include "PixelBench.h"
#include "CommandsBench.h"
void bringupBegin() {
  strncpy(sleepConfigFile,HYPNOS_CONFIG_FILE,sizeof(sleepConfigFile)-1);
  safeOff(); Serial.begin(115200);
  const __FlashStringHelper *resumeResult = resumeSleepOnBoot();
  hypnos.setCompileTime(__DATE__,__TIME__,TIME_ZONE::PST);
  uint32_t start=millis(); while (!Serial && uint32_t(millis()-start)<3000) delay(10);
  if (resumeResult) Serial.println(resumeResult);
  record(POWER,INFO,"Startup complete. Current rail states below.");
  help(); railStatus(); serialOpen=bool(Serial);
}
void bringupLoop() {
  bool open=bool(Serial); if (open && !serialOpen) { help(); railStatus(); } serialOpen=open;
  if (rawBridge) {
    for (uint8_t i=0;i<64 && Serial.available();++i) { int ch=Serial.read();
      if (ch==29) { rawBridge=false; dataReady=false; ledsConfigured=false; Serial.println(F("\nConsole resumed.")); break; }
      Serial1.write(uint8_t(ch)); }
    for (uint8_t i=0;i<64 && Serial1.available() && Serial.availableForWrite();++i) Serial.write(Serial1.read());
    return;
  }
  // Show unsolicited modem output without hiding it from the operator.
  if (uartOpen) for (uint8_t i=0;i<64 && Serial1.available() && Serial.availableForWrite();++i) modemByte(Serial1.read());
  for (uint8_t i=0;i<64 && Serial.available();++i) {
    int ch=Serial.read();
    if (ch=='\r' || ch=='\n') { if (discardLine) Serial.println(F("Overlong command discarded."));
      else if (inputUsed) { input[inputUsed]=0; dispatch(input); }
      inputUsed=0; discardLine=false;
    } else if (ch==8 || ch==127) { if (inputUsed) --inputUsed; }
    else if (ch>=32 && ch<=126 && !discardLine) { if (inputUsed+1<sizeof(input)) input[inputUsed++]=char(ch); else discardLine=true; }
  }
}
