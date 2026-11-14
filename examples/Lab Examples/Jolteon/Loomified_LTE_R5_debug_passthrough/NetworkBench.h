#pragma once
// Explicit network tests share the real Loom LTE and MongoDB clients.
static bool prepareSleepLte() {
  if (!network) network=new Loom_LTE(manager,networkApn(),NETWORK_USER,NETWORK_PASS,A5,OPENS,A4,LTE_MODEM::SARA_R5);
  if (!network) return false;
  Serial.println(F("Sleep: shutting down LTE through Loom CPWROFF before arming the RTC (3.8V LDO remains on)."));
  const bool off=network->shutdownForSleep();
  if (!off) Serial.println(F("LTE still answers after shutdown; staying awake so shutdown can be inspected."));
  else Serial.println(F("LTE shutdown complete; modem status/network LEDs should be off during sleep."));
  dataReady=ledsConfigured=false;
  return off;
}
static void ltePulse() {
  ready(LTE_AT);
  if (pendingReply.active || pingPending) { Serial.println(F("Wait for the pending modem response before pulsing power.")); return; }
  if (uartOpen) Serial1.end(); uartOpen=false;
  pinMode(0,INPUT); pinMode(1,INPUT);
  output(MODEM_PWR,true); delay(MODEM_PULSE_MS); output(MODEM_PWR,false);
  pulseSent=true; ledsConfigured=dataReady=false;
  Serial.println(F("LTE power-on: A5 HIGH 1.2 s, then LOW. Waiting 10 s for modem boot."));
  delay(10000); Serial1.begin(MODEM_BAUD); uartOpen=true;
}
static bool configureLteLeds() {
  if (ledsConfigured) return true;
  if (!at("AT+UGPIOC?",10000)) { record(LTE_LEDS,FAIL,"Cannot read SARA GPIO configuration."); return false; }
  // SARA UGPIOC IDs are physical pad numbers: GPIO1=16, GPIO2=23.
  int net=benchGpioMode(reply,16),status=benchGpioMode(reply,23);
  if (net<0 || status<0) { record(LTE_LEDS,FAIL,"GPIO1/16 or GPIO2/23 missing from the modem reply; no GPIO writes made."); return false; }
  uint8_t move[16]; unsigned moves=0;
  for (unsigned pin=0;pin<96;++pin) {
    int mode=benchGpioMode(reply,pin);
    if ((mode==2 && pin!=16) || (mode==10 && pin!=23)) { if (moves>=sizeof(move)) return false; move[moves++]=uint8_t(pin); }
  }
  char command[40];
  for (unsigned i=0;i<moves;++i) {
    snprintf(command,sizeof(command),"AT+UGPIOC=%u,255",move[i]);
    if (!at(command,10000)) { record(LTE_LEDS,FAIL,"Could not move a duplicate status function to the wired LED pin."); return false; }
  }
  if ((net!=2 && !at("AT+UGPIOC=16,2",10000)) || (status!=10 && !at("AT+UGPIOC=23,10",10000))) {
    record(LTE_LEDS,FAIL,"LED configuration rejected; inspect modem error. No reset issued."); return false;
  }
  ledsConfigured=at("AT+UGPIOC?",10000) && benchGpioMode(reply,16)==2 && benchGpioMode(reply,23)==10;
  record(LTE_LEDS,ledsConfigured?PASS:FAIL,"Readback: GPIO1/pad16=network status (mode 2); GPIO2/pad23=module status (mode 10). Matching stored settings are preserved.");
  return ledsConfigured;
}
static bool connectData() {
  if (!lteHello() || pendingReply.active || pingPending) return false;
  if (!credentialsValid(credentials())) { Serial.println(F("Invalid APN credentials in arduino_secrets.h.")); return false; }
  if (!at("AT+CPIN?",10000) || !lineEquals(reply,"+CPIN: READY")) { record(SIM,BLOCKED,"SIM must be READY for LTE data; no unlock guesses made."); return false; }
  uint8_t mode[2]={};
  if (!at("AT+CFUN?") || (!scanNumbers(reply,"+CFUN:",mode,2) && !scanNumbers(reply,"+CFUN:",mode,1))) return false;
  if (mode[0]!=1) {
    Serial.println(F("Enabling normal RF mode for this explicit connection test (LTE antenna attached)."));
    if (!at("AT+CFUN=1",180000)) return false;
  }
  if (!network) network=new Loom_LTE(manager,networkApn(),NETWORK_USER,NETWORK_PASS,A5,OPENS,A4,LTE_MODEM::SARA_R5);
  if (!network) return false;
  Serial.println(F("LTE: verifying registration and APN data session with Loom; no second PWR_ON pulse."));
  dataReady=dataReady?network->prepareConnection():network->initializeFromAwake();
  uartOpen=true;
  record(NET,dataReady?PASS:FAIL,"Loom cellular registration/data-session result. MQTT is a separate command.");
  return dataReady;
}
static void ltePing(const char *host) {
  if (!host) host="1.1.1.1";
  if (!benchHost(host)) { record(LTE_PING,BLOCKED,"lte ping [IPv4-or-hostname], maximum 100 letters/digits/dots/hyphens."); return; }
  if (!connectData()) { record(LTE_PING,BLOCKED,"Need a registered LTE data session before IP ping."); return; }
  char command[160]; snprintf(command,sizeof(command),"AT+UPING=\"%s\",1,32,5000,64",host);
  Serial.print(F("Ping through LTE to internet host: ")); Serial.println(host);
  if (!at(command,10000)) { record(LTE_PING,FAIL,"Modem rejected/timed out accepting ping. Terminal OK alone never counts as an echo reply."); return; }
  pingPending=true; pingAnswered=false; pingRtt=-1; pingUsed=0;
  for (const char *p=reply;*p;++p) pingByte(*p);
  uint32_t started=millis(); registrationCancelled=false; registrationCanCancel=true;
  while (pingPending && uint32_t(millis()-started)<20000 && !cancelRegistration()) {
    while (Serial1.available()) modemByte(Serial1.read()); delay(1);
  }
  registrationCanCancel=false; registrationCancelled=false;
  Serial.print(F("Ping RTT milliseconds: ")); Serial.println(pingRtt);
  record(LTE_PING,pingAnswered?PASS:FAIL,pingAnswered ?
    "Received an actual ICMP echo reply through the LTE data session. No MQTT used." :
    "No valid echo reply. Carrier/host ICMP filtering can cause this; any pending ping result must arrive before another modem command.");
}
static MqttClient *echoClient;
static const char *echoTopic;
static size_t echoLength;
static bool echoMatched;
static void receiveTestPacket(int length) {
  if (!echoClient) return;
  bool matches=echoTopic && echoClient->messageTopic()==echoTopic && length==int(echoLength) && !echoClient->messageRetain();
  size_t used=0; uint32_t start=millis();
  while (used<size_t(length) && uint32_t(millis()-start)<10000) {
    if (!echoClient->available()) { delay(1); continue; }
    int value=echoClient->read(); if (value<0) break;
    if (used>=echoLength || value!=uint8_t(reply[used])) matches=false;
    ++used;
  }
  if (used!=size_t(length)) matches=false;
  if (matches) { echoMatched=true; Serial.println(F("MQTT readback: received the exact current test packet.")); }
}
static void mqttTest() {
  if (!mountSd()) { record(MQTT_ACK,BLOCKED,"MQTT test needs mqtt_creds.json on SD."); return; }
  // Validate/load credentials before making a cellular connection. Never print credential values.
  StaticJsonDocument<1536> config;
  if (!readConfigJson(MQTT_CONFIG_FILE,config)) { record(MQTT_ACK,BLOCKED,"Missing/invalid mqtt_creds.json. Existing file preserved."); return; }
  if (!connectData()) { record(MQTT_ACK,BLOCKED,"LTE data session unavailable."); return; }
  static Loom_MongoDB mongo(manager,*network);
  char *owned=static_cast<char *>(malloc(measureJson(config)+1));
  if (!owned) { record(MQTT_ACK,FAIL,"No RAM for MQTT configuration."); return; }
  serializeJson(config,owned,measureJson(config)+1);
  mongo.loadConfigFromJSON(owned); // Loom owns/frees the input and validates standard broker/database/project fields.
  if (!mongo.moduleInitialized) { record(MQTT_ACK,FAIL,"Loom rejected MQTT configuration; credentials were not printed."); return; }
  mongo.setMaxRetries(1);
  if (!makeSample() || !saveSampleFile(false)) { record(MQTT_ACK,BLOCKED,"Could not acquire and preserve the test measurement on SD."); return; }
  const char *topic=mongo.getPublishTopic();
  if (!topic || !mongo.connectForDiagnostics()) { record(MQTT_ACK,FAIL,"Could not connect to the configured broker/topic."); return; }
  MqttClient &client=mongo.getMQTTClient();
  echoClient=&client; echoTopic=topic; echoLength=strlen(reply); echoMatched=false;
  client.onMessage(receiveTestPacket);
  bool subscribed=client.subscribe(topic,1)==1;
  if (!subscribed) Serial.println(F("Broker declined readback subscription; publish acknowledgement will still be tested."));
  bool accepted=mongo.publish();
  record(MQTT_ACK,accepted?PASS:FAIL,"QoS 1 broker acknowledgement of the actual Loom packet; Mongo storage is not internally verified.");
  uint32_t started=millis();
  while (accepted && subscribed && !echoMatched && client.connected() && uint32_t(millis()-started)<15000) { client.poll(); delay(5); }
  record(MQTT_ECHO,echoMatched?PASS:subscribed && accepted?FAIL:INFO,"Exact non-retained MQTT packet readback. Subscription permission/routing affects this result; it is separate from Mongo insertion.");
  client.onMessage(nullptr);
  if (subscribed && client.connected()) client.unsubscribe(topic);
  client.stop(); echoClient=nullptr; echoTopic=nullptr;
  Serial.print(F("Device: ")); Serial.print(manager.get_device_name()); Serial.println(manager.get_instance_num());
  Serial.print(F("MQTT topic: ")); Serial.println(topic);
  Serial.print(F("Test ID: ")); Serial.println(testId);
  Serial.print(F("Measured UTC: ")); Serial.println(manager.getDocument()["timestamp"]["time_utc"].as<const char *>());
  Serial.print(F("Saved JSON: ")); Serial.println(lastSampleFile);
  Serial.println(F("Check MongoDB manually for this device and TestId. This firmware cannot confirm the database insert."));
}
