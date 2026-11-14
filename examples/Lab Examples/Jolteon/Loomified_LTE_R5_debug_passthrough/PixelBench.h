#pragma once
// Detached Loom actuator: initialize only when a pixel command acquires power.
// Ports 0/1 remain disabled so A0/A1 stay analog inputs.
static Loom_Neopixel rgb(false,false,true,NEO_GRB);
static bool rgbStarted,rgbEnabled;
static uint32_t rgbColor=0xffffff;
static bool rgbBegin() {
  ready(PIXEL_NOISE);
  if (!rgbStarted) {
    if (!rgb.configure_port(2,NEOPIXEL_PIN,NEOPIXEL_COUNT,NEOPIXEL_BRIGHTNESS)) {
      record(PIXEL_NOISE,FAIL,"Unable to configure Loom NeoPixel port."); return false;
    }
    rgb.initialize(); rgbStarted=true;
  }
  // analog A2 may have selected the ADC since the last RGB command.
  pinMode(NEOPIXEL_PIN,OUTPUT); digitalWrite(NEOPIXEL_PIN,LOW);
  return true;
}
static void rgbFill(uint32_t color) {
  for (uint16_t i=0;i<NEOPIXEL_COUNT;++i)
    rgb.set_color(2,i,(color>>16)&255,(color>>8)&255,color&255,false);
}
static void rgbShow() {
  if (!rgbBegin()) return;
  rgbFill(rgbEnabled?rgbColor:0); rgb.show(2);
  Serial.print(F("RGB pin=")); Serial.print(NEOPIXEL_PIN); Serial.print(rgbEnabled?F(" ON | R,G,B="):F(" OFF | stored R,G,B="));
  Serial.print((rgbColor>>16)&255); Serial.print(','); Serial.print((rgbColor>>8)&255); Serial.print(','); Serial.println(rgbColor&255);
}
static bool adcFrameSample(uint8_t pin,bool frame,uint16_t &value) {
  analogRead(pin); // Select and settle the channel/reference with the normal core route.
  while (ADC->STATUS.bit.SYNCBUSY) {}
  const uint16_t prior=ADC->CTRLB.reg;
  ADC->CTRLB.reg=prior|ADC_CTRLB_FREERUN;
  while (ADC->STATUS.bit.SYNCBUSY) {}
  ADC->CTRLA.bit.ENABLE=1; while (ADC->STATUS.bit.SYNCBUSY) {}
  ADC->INTFLAG.reg=ADC_INTFLAG_RESRDY; ADC->SWTRIG.bit.START=1;
  // ADC runs in hardware throughout the bit-banged frame. Read the most recent
  // completed conversion at frame end; this is not a scope capture of every edge.
  if (frame) rgb.show(2); else delayMicroseconds(30*NEOPIXEL_COUNT);
  uint32_t began=micros();
  while (!ADC->INTFLAG.bit.RESRDY && uint32_t(micros()-began)<2000) {}
  bool ok=ADC->INTFLAG.bit.RESRDY; value=ADC->RESULT.reg;
  ADC->CTRLA.bit.ENABLE=0; while (ADC->STATUS.bit.SYNCBUSY) {}
  ADC->INTFLAG.reg=ADC_INTFLAG_RESRDY|ADC_INTFLAG_OVERRUN;
  ADC->CTRLB.reg=prior; while (ADC->STATUS.bit.SYNCBUSY) {}
  return ok;
}
static void printNoise(const char *phase,const char *pin,const BenchStats &stats) {
  const double scale=adcReference*1000.0/4095.0;
  Serial.print(phase); Serial.print(" | "); Serial.print(pin); Serial.print(F(" | samples=")); Serial.print(stats.count);
  Serial.print(F(" | average mV=")); Serial.print(stats.mean*scale,3);
  Serial.print(F(" | min mV=")); Serial.print(stats.low*scale,3);
  Serial.print(F(" | max mV=")); Serial.print(stats.high*scale,3);
  Serial.print(F(" | peak-to-peak mV=")); Serial.print((stats.high-stats.low)*scale,3);
  Serial.print(F(" | standard deviation mV=")); Serial.print(stats.deviation()*scale,3);
  Serial.print(F(" | min/max counts=")); Serial.print(stats.low); Serial.print('/'); Serial.println(stats.high);
}
static void rgbNoiseTest() {
  if (!rgbBegin()) return;
  analogReadResolution(12); analogReference(AR_DEFAULT);
  const bool restoreEnabled=rgbEnabled; const uint32_t restoreColor=rgbColor;
  BenchStats stats[3][2]; bool ok=true;
  Serial.println(F("RGB noise test: connect A0/A1 to known stable inputs or the intended divider/reference circuit. Floating inputs do not establish circuit noise."));
  Serial.println(F("5 seconds each: OFF baseline BEFORE / changing RGB frames / OFF baseline AFTER (about 15 seconds total)."));
  Serial.println(F("ADC free-runs during each frame; latest completed conversion is read at frame end. A0/A1 sampled alternately; baseline data pin stays quiet."));
  const char *names[]={"BASELINE BEFORE / OFF","PULSING / data active","BASELINE AFTER / OFF"};
  const uint32_t colors[]={0,0xff0000,0x00ff00,0x0000ff,0xffffff};
  for (unsigned phase=0;phase<3 && ok;++phase) {
    Serial.print(F("Sampling: ")); Serial.println(names[phase]);
    rgbFill(0); rgb.show(2); delay(100);
    const uint32_t started=millis(); uint32_t frame=0;
    while (uint32_t(millis()-started)<5000 && ok) {
      for (unsigned channel=0;channel<2 && ok;++channel) {
        if (phase==1) rgbFill(colors[frame++%5]);
        while (!rgb.can_show(2)) delayMicroseconds(1);
        uint16_t reading=0; ok=adcFrameSample(channel?A1:A0,phase==1,reading);
        if (ok) stats[phase][channel].add(reading);
      }
    }
  }
  rgbEnabled=restoreEnabled; rgbColor=restoreColor; rgbShow();
  if (!ok) { record(PIXEL_NOISE,FAIL,"ADC conversion timed out. RGB state restored."); return; }
  for (unsigned phase=0;phase<3;++phase) for (unsigned channel=0;channel<2;++channel) printNoise(names[phase],channel?"A1":"A0",stats[phase][channel]);
  for (unsigned channel=0;channel<2;++channel) {
    Serial.print(channel?F("A1 pulsing minus BEFORE average mV: "):F("A0 pulsing minus BEFORE average mV: "));
    Serial.println((stats[1][channel].mean-stats[0][channel].mean)*adcReference*1000.0/4095.0,3);
    Serial.print(channel?F("A1 AFTER minus BEFORE average mV: "):F("A0 AFTER minus BEFORE average mV: "));
    Serial.println((stats[2][channel].mean-stats[0][channel].mean)*adcReference*1000.0/4095.0,3);
  }
  record(PIXEL_NOISE,INFO,"Comparative ADC statistics recorded. Judge against the intended input circuit/noise budget; visually confirm pixel colors. Previous RGB state restored.");
}
static void rgbCommand(const char *action,const char *value) {
  if (!action || !strcmp(action,"test")) { rgbNoiseTest(); return; }
  if (!strcmp(action,"on")) rgbEnabled=true;
  else if (!strcmp(action,"off")) rgbEnabled=false;
  else if (!strcmp(action,"color")) {
    uint8_t r,g,b;
    if (!benchRgb(value,r,g,b)) { Serial.println(F("rgb color r|g|b|y|c|m|w OR rgb color 255,255,0 (0..255 per channel)")); return; }
    rgbColor=(uint32_t(r)<<16)|(uint32_t(g)<<8)|b; rgbEnabled=true;
  } else { Serial.println(F("rgb on | rgb off | rgb color r | rgb color 255,255,0 | rgb test")); return; }
  rgbShow();
}
