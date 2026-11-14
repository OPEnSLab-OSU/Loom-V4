#pragma once
#include <ArduinoJson.h>
#include <stdint.h>
#include <string.h>
#include <initializer_list>

inline JsonObjectConst benchInterval(JsonObjectConst config) {
  if (config.containsKey("days") || config.containsKey("hours") || config.containsKey("minutes") || config.containsKey("seconds")) return config;
  for (const char *key:{"SleepInterval","sleepInterval","sleep_interval"})
    if (config[key].is<JsonObjectConst>()) return config[key].as<JsonObjectConst>();
  return JsonObjectConst();
}
inline bool benchSleepSeconds(JsonObjectConst config,uint32_t &seconds) {
  JsonObjectConst interval=benchInterval(config); if (interval.isNull()) return false;
  const char *keys[]={"days","hours","minutes","seconds"};
  const uint32_t scale[]={86400,3600,60,1}; uint32_t total=0;
  for (uint8_t i=0;i<4;++i) if (interval.containsKey(keys[i])) {
    if (!interval[keys[i]].is<uint32_t>()) return false;
    uint32_t n=interval[keys[i]].as<uint32_t>();
    if (n>(86400-total)/scale[i]) return false; total+=n*scale[i];
  }
  if (total<3) return false; seconds=total; return true;
}
inline bool benchRootFile(const char *name) {
  if (!name || !*name || strlen(name)>63 || !strcmp(name,".") || !strcmp(name,"..")) return false;
  for (const char *p=name;*p;++p) if (!((*p>='A' && *p<='Z') || (*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='_' || *p=='-' || *p=='.')) return false;
  return true;
}
inline bool benchSampleName(const char *name) {
  if (!name || strlen(name)!=13 || strncmp(name,"JLT",3) || strcmp(name+8,".JSON")) return false;
  for (uint8_t i=3;i<8;++i) if (name[i]<'0' || name[i]>'9') return false;
  return true;
}
