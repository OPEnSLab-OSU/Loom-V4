#pragma once
#include "Logic.h"
#include <stdio.h>
#include <stdlib.h>

inline bool benchDuration(const char *value,const char *unit,uint32_t &seconds) {
  if (!value || !*value) return false;
  const char *end=value; while (*end>='0' && *end<='9') ++end;
  size_t length=size_t(end-value); if (!length || length>5) return false;
  char number[6]={}; memcpy(number,value,length);
  uint32_t n=0; if (!unsignedNumber(number,n,86400) || !n || (*end && unit)) return false;
  const char *suffix=*end?end:unit?unit:"s";
  uint32_t scale=0;
  if (!strcmp(suffix,"s") || !strcmp(suffix,"sec") || !strcmp(suffix,"secs") || !strcmp(suffix,"second") || !strcmp(suffix,"seconds")) scale=1;
  if (!strcmp(suffix,"m") || !strcmp(suffix,"min") || !strcmp(suffix,"mins") || !strcmp(suffix,"minute") || !strcmp(suffix,"minutes")) scale=60;
  if (!strcmp(suffix,"h") || !strcmp(suffix,"hr") || !strcmp(suffix,"hour") || !strcmp(suffix,"hours")) scale=3600;
  if (!strcmp(suffix,"d") || !strcmp(suffix,"day") || !strcmp(suffix,"days")) scale=86400;
  if (!scale || n>86400/scale) return false;
  seconds=n*scale; return true;
}
inline bool benchIsoEpoch(const char *text,uint32_t &epoch) {
  if (!text || strlen(text)!=20) return false;
  uint8_t digits[14]; size_t count=0;
  for (size_t i=0;i<20;++i) {
    char expected=i==4 || i==7?'-':i==10?'T':i==13 || i==16?':':i==19?'Z':0;
    if (expected) { if (text[i]!=expected) return false; }
    else { if (text[i]<'0' || text[i]>'9') return false; digits[count++]=uint8_t(text[i]-'0'); }
  }
  unsigned year=digits[0]*1000+digits[1]*100+digits[2]*10+digits[3];
  if (year<2000 || year>2099) return false;
  uint8_t r[]={bcd(uint8_t(digits[12]*10+digits[13])),bcd(uint8_t(digits[10]*10+digits[11])),
    bcd(uint8_t(digits[8]*10+digits[9])),1,bcd(uint8_t(digits[6]*10+digits[7])),bcd(uint8_t(digits[4]*10+digits[5])),bcd(uint8_t(year-2000))};
  return rtcEpoch(r,epoch);
}
inline int benchGpioMode(const char *reply,unsigned wanted) {
  bool section=false;
  while (reply && *reply) {
    const char *end=strchr(reply,'\n'); if (!end) end=reply+strlen(reply);
    const char *p=reply;
    if (end-p>=7 && !memcmp(p,"+UGPIOC",7)) {
      if (end-p<8 || p[7]!=':') return -1;
      section=true; p+=8;
    }
    if (section && end-p<40) {
      char line[40]={}; memcpy(line,p,size_t(end-p));
      unsigned id=0,mode=0; int used=0;
      if (sscanf(line," %u , %u %n",&id,&mode,&used)==2 && !line[used] && id==wanted && mode<=255) return int(mode);
    }
    if (!*end) break; reply=end+1;
  }
  return -1;
}
inline bool benchHost(const char *host) {
  if (!host || !*host || strlen(host)>100) return false;
  for (const char *p=host;*p;++p) if (!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='.' || *p=='-')) return false;
  return true;
}
inline bool benchPingReply(const char *line,long &rtt) {
  if (!line || strncmp(line,"+UUPING:",8)) return false;
  unsigned sequence=0,bytes=0;
  if (sscanf(line,"+UUPING: %u , %u ,",&sequence,&bytes)!=2 || sequence!=1 || bytes!=32) return false;
  // Six CSV fields; hostname/IP must be quoted. Empty hostname is valid for a numeric target.
  const char *p=line+8; unsigned commas=0; bool quoted=false; unsigned quotes=0;
  const char *last=nullptr;
  for (;*p && *p!='\r' && *p!='\n';++p) {
    if (*p=='"') { quoted=!quoted; ++quotes; }
    else if (*p==',' && !quoted) { ++commas; last=p+1; }
  }
  if (quoted || quotes!=4 || commas!=5 || !last) return false;
  char *end=nullptr; rtt=strtol(last,&end,10);
  if (end==last || rtt < -2 || rtt>60000) return false;
  while (*end==' ' || *end=='\r' || *end=='\n') ++end;
  return !*end;
}
inline bool benchRgb(const char *text,uint8_t &red,uint8_t &green,uint8_t &blue) {
  if (!text) return false;
  if (strlen(text)==1) {
    red=green=blue=0;
    switch (*text) {
      case 'r': red=255; return true; case 'g': green=255; return true; case 'b': blue=255; return true;
      case 'y': red=green=255; return true; case 'c': green=blue=255; return true;
      case 'm': red=blue=255; return true; case 'w': red=green=blue=255; return true;
      default: return false;
    }
  }
  uint8_t values[3]; const char *p=text;
  for (unsigned i=0;i<3;++i) {
    uint32_t n=0; unsigned digits=0;
    while (*p>='0' && *p<='9') { if (++digits>3) return false; n=n*10+unsigned(*p++-'0'); }
    if (!digits || n>255) return false; values[i]=uint8_t(n);
    if (i<2 && *p++!=',') return false;
  }
  if (*p) return false;
  red=values[0]; green=values[1]; blue=values[2]; return true;
}
struct BenchStats {
  uint32_t count=0; uint16_t low=4095,high=0; double mean=0,m2=0;
  void add(uint16_t value) {
    if (value<low) low=value; if (value>high) high=value;
    ++count; double delta=value-mean; mean+=delta/count; m2+=delta*(value-mean);
  }
  double deviation() const { return count>1?sqrt(m2/(count-1)):0; }
};
