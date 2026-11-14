#pragma once
// Included after mountSd(): these commands share the console's SD and Manager objects.
static bool readConfigJson(const char *path,JsonDocument &doc) {
  if (!benchRootFile(path)) return false;
  SdFile file; if (!file.open(path,O_RDONLY)) return false;
  size_t length=file.fileSize(); char text[1025];
  bool ok=length>0 && length<sizeof(text) && file.read(text,length)==int(length);
  file.close(); if (!ok || memchr(text,0,length)) return false;
  text[length]=0; const char *json=text;
  if (length>=3 && uint8_t(text[0])==0xef && uint8_t(text[1])==0xbb && uint8_t(text[2])==0xbf) json+=3;
  return !deserializeJson(doc,json) && doc.is<JsonObject>() && !doc.overflowed();
}
static void sleepStatus() {
  Serial.print(F("Sleep interval: ")); Serial.print(sleepSeconds); Serial.print(F(" seconds | source: "));
  Serial.println(sleepOverride?"session override":sleepConfigLoaded?sleepConfigFile:sleepConfigTried?"default (no valid SD config loaded)":"SD config on first sleep (10 s fallback)");
}
static bool loadSleepConfig(const char *path) {
  sleepConfigTried=true;
  if (!mountSd()) { Serial.println(F("Sleep: SD unavailable; keeping the current interval.")); return false; }
  const char *chosen=path;
  if (!chosen) {
    const char *names[]={HYPNOS_CONFIG_FILE,"hypnos_config.json","hypnos.json","hypnosConfig.json"};
    for (const char *name:names) if (sd.exists(name)) { chosen=name; break; }
  }
  if (!chosen || !benchRootFile(chosen)) { Serial.println(F("No Hypnos config found. Use sleep load <filename.json>, sleep set 10s, or sleep save 1min.")); return false; }
  StaticJsonDocument<1536> doc; uint32_t seconds=0;
  if (!readConfigJson(chosen,doc) || !benchSleepSeconds(doc.as<JsonObjectConst>(),seconds)) {
    Serial.println(F("Invalid Hypnos sleep config; expected days/hours/minutes/seconds or nested SleepInterval, totaling 3 s..24 h. Current interval retained.")); return false;
  }
  if (doc.containsKey("timezone") && (!doc["timezone"].is<const char *>() || !hypnos.setTimezone(doc["timezone"].as<const char *>()))) {
    Serial.println(F("Invalid Hypnos timezone; config not applied.")); return false;
  }
  sleepSeconds=seconds; sleepOverride=false; sleepConfigLoaded=true;
  strncpy(sleepConfigFile,chosen,sizeof(sleepConfigFile)-1);
  Serial.print(F("Loaded Hypnos config: ")); Serial.println(sleepConfigFile); sleepStatus(); return true;
}
static bool saveSleepConfig() {
  if (!mountSd()) return false;
  StaticJsonDocument<1536> doc;
  bool existed=sd.exists(sleepConfigFile);
  if (existed && !readConfigJson(sleepConfigFile,doc)) { Serial.println(F("Existing config is unreadable; preserving it.")); return false; }
  if (!existed) { doc.to<JsonObject>(); doc["timezone"]="PST"; }
  JsonObject interval=doc.as<JsonObject>();
  if (!interval.containsKey("days") && !interval.containsKey("hours") && !interval.containsKey("minutes") && !interval.containsKey("seconds")) {
    bool found=false;
    for (const char *key:{"SleepInterval","sleepInterval","sleep_interval"}) if (doc[key].is<JsonObject>()) { interval=doc[key].as<JsonObject>(); found=true; break; }
    if (!found) interval=doc.createNestedObject("SleepInterval");
  }
  for (const char *key:{"days","hours","minutes","seconds"}) interval.remove(key);
  interval["seconds"]=sleepSeconds;
  if (doc.overflowed()) return false;
  char temp[16],backup[16]; bool unique=false;
  for (unsigned i=0;i<1000;++i) {
    snprintf(temp,sizeof(temp),"HYP%05u.TMP",i); snprintf(backup,sizeof(backup),"HYP%05u.BAK",i);
    if (!sd.exists(temp) && !sd.exists(backup)) { unique=true; break; }
  }
  if (!unique) return false;
  SdFile file; if (!file.open(temp,O_WRONLY|O_CREAT|O_EXCL)) return false;
  bool ok=serializeJson(doc,file)==measureJson(doc); ok=file.sync() && ok; ok=file.close() && ok;
  uint32_t verify=0;
  ok=readConfigJson(temp,doc) && benchSleepSeconds(doc.as<JsonObjectConst>(),verify) && verify==sleepSeconds && ok;
  if (!ok) { Serial.println(F("Config verification failed; original retained and temporary file left for inspection.")); return false; }
  if (existed && !sd.rename(sleepConfigFile,backup)) return false;
  if (!sd.rename(temp,sleepConfigFile)) { if (existed) sd.rename(backup,sleepConfigFile); return false; }
  if (existed) { Serial.print(F("Previous config retained as ")); Serial.println(backup); }
  sleepOverride=false; sleepConfigTried=sleepConfigLoaded=true; Serial.print(F("Saved and verified ")); Serial.println(sleepConfigFile); sleepStatus(); return true;
}
static bool makeSample() {
  manager.getDocument().clear(); testId[0]=0;
  if (!ms5803Test(0,true)) return false;
  if (!manager.getDocument()["timestamp"]["time_utc"].is<const char *>()) { Serial.println(F("Packet has no verified RTC timestamp; sample not saved/sent.")); return false; }
  size_t length=measureJson(manager.getDocument());
  if (!manager.isPacketValid() || length+1>=sizeof(reply)) {
    Serial.print(F("Packet serialization rejected | JSON bytes=")); Serial.print(length);
    Serial.print(F(" | complete=")); Serial.println(manager.isPacketValid()); return false;
  }
  const size_t written=serializeJson(manager.getDocument(),reply,sizeof(reply));
  if (written!=length) { Serial.print(F("Packet serialization mismatch | expected=")); Serial.print(length); Serial.print(F(" | written=")); Serial.println(written); }
  return written==length;
}
static bool printSampleFile(const char *path,const char *expected=nullptr) {
  if (!benchSampleName(path)) { Serial.println(F("Use a JLTnnnnn.JSON test file; configuration/credential files are not printed.")); return false; }
  SdFile file; if (!file.open(path,O_RDONLY) || file.fileSize()>sizeof(reply)) return false;
  size_t count=0,length=expected?strlen(expected):0; bool ok=true;
  Serial.print(F("--- SD readback: ")); Serial.print(path); Serial.println(F(" ---"));
  while (file.available()) {
    int ch=file.read(); if (ch<0) { ok=false; break; }
    Serial.write(uint8_t(ch));
    if (expected && (count<length ? ch!=expected[count] : count!=length || ch!='\n')) ok=false;
    ++count;
  }
  ok=file.close() && ok;
  Serial.println(); Serial.println(F("--- End SD readback ---"));
  return ok && (!expected || count==length+1);
}
static bool saveSampleFile(bool acquire=true) {
  if (!mountSd() || (acquire && !makeSample())) { record(SD_WRITE,FAIL,"Need a mounted SD and a valid MS5803/RTC packet."); return false; }
  if (!manager.isPacketValid() || !reply[0]) return false;
  SdFile file; bool created=false;
  for (unsigned i=0;i<1000;++i) {
    snprintf(lastSampleFile,sizeof(lastSampleFile),"JLT%05u.JSON",i);
    if (!sd.exists(lastSampleFile)) { created=file.open(lastSampleFile,O_WRONLY|O_CREAT|O_EXCL); break; }
  }
  if (!created) { record(SD_WRITE,FAIL,"Cannot create a new exclusive test file."); return false; }
  size_t length=strlen(reply);
  bool ok=file.write(reinterpret_cast<const uint8_t *>(reply),length)==length && file.write('\n')==1;
  ok=file.sync() && ok; ok=file.close() && ok;
  Serial.print(F("Saved test packet: ")); Serial.println(lastSampleFile);
  ok=printSampleFile(lastSampleFile,reply) && ok;
  record(SD_WRITE,ok?PASS:FAIL,"JSON retained on SD; reopened contents printed and compared byte-for-byte with the packaged sample."); return ok;
}
static bool listSdFiles(char *latest=nullptr) {
  if (!mountSd()) return false;
  if (latest) latest[0]=0;
  SdFile dir,entry; char name[64];
  bool ok=dir.openRoot(sd.vol()); unsigned count=0; uint32_t start=millis();
  Serial.println(F("--- SD root directory (name | bytes; directories end in /) ---"));
  while (ok && count<256 && uint32_t(millis()-start)<4000 && entry.openNext(&dir,O_RDONLY)) {
    entry.getName(name,sizeof(name)); Serial.print(name);
    if (entry.isDir()) Serial.println('/');
    else { Serial.print(F(" | ")); Serial.println(entry.fileSize()); }
    if (latest && !entry.isDir() && benchSampleName(name) && strcmp(name,latest)>0) strcpy(latest,name);
    entry.close(); ++count;
  }
  dir.close();
  Serial.print(F("Entries listed: ")); Serial.println(count);
  if (ok && !count) Serial.println(F("Root directory is empty."));
  if (count>=256 || uint32_t(millis()-start)>=4000) Serial.println(F("Listing stopped at the 256-entry / 4-second limit; more entries may remain."));
  if (!ok) Serial.println(F("Cannot open SD root directory."));
  return ok;
}
static void sdTest(bool write,const char *selected=nullptr) {
  if (write) { saveSampleFile(); return; }
  char latest[24]={}; bool ok=listSdFiles(latest);
  if (!ok) return;
  const char *path=selected?selected:lastSampleFile[0]?lastSampleFile:latest;
  if (*path) ok=printSampleFile(path) && ok;
  else Serial.println(F("No JLT test packet found. Run sd save to create and verify one."));
  record(SD_READ,ok?PASS:FAIL,"Directory and selected/latest retained JSON test file read. SD remains optional for other protocols.");
}
