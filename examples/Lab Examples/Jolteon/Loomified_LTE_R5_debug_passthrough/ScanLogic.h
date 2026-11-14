#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Strict bounded numeric reply fields; ignore command echoes and unrelated URCs.
inline bool scanNumbers(const char *text,const char *prefix,uint8_t *values,size_t count) {
  size_t length=strlen(prefix);
  for (const char *line=text;line && *line;) {
    const char *end=strchr(line,'\n'); if (!end) end=line+strlen(line);
    if (size_t(end-line)>=length && !memcmp(line,prefix,length)) {
      const char *p=line+length;
      for (size_t i=0;i<count;++i) {
        while (p<end && *p==' ') ++p;
        unsigned value=0; bool digits=false;
        while (p<end && *p>='0' && *p<='9') {
          digits=true; value=value*10+unsigned(*p++-'0'); if (value>255) return false;
        }
        if (!digits) return false;
        values[i]=uint8_t(value); while (p<end && *p==' ') ++p;
        if (i+1<count && (p==end || *p++!=',')) return false;
      }
      if (p<end && *p=='\r') ++p;
      return p==end;
    }
    line=*end?end+1:nullptr;
  }
  return false;
}

// Count complete operator tuples as bytes arrive. No fixed-size COPS line:
// long operator lists are streamed without truncating the diagnostic output.
struct CopsScanCounter {
  uint32_t operators=0;
  uint8_t prefix=0,field=0,status=0,statusDigits=0,plmnDigits=0;
  bool active=false,tuple=false,quoted=false,valid=false,plmnOpened=false,plmnClosed=false;
  void feed(int ch) {
    char c=char(ch);
    if (c=='\n') { prefix=0; active=tuple=quoted=false; return; }
    if (!active) {
      static const char header[]="+COPS:";
      if (prefix<6 && c==header[prefix]) { if (++prefix==6) active=true; }
      else prefix=255; // Only a line beginning +COPS: carries scan results.
      return;
    }
    if (c=='(' && !quoted) {
      tuple=valid=true; field=status=statusDigits=plmnDigits=0;
      plmnOpened=plmnClosed=false; return;
    }
    if (!tuple) return;
    if (c=='"') {
      if (field==0) valid=false;
      if (field==3) {
        if (!quoted) { if (plmnOpened) valid=false; plmnOpened=true; }
        else plmnClosed=true;
      }
      quoted=!quoted; return;
    }
    if (c==',' && !quoted) { if (field<5) ++field; else valid=false; return; }
    if (c==')' && !quoted) {
      if (valid && field>=3 && field<=4 && statusDigits==1 && status<=3 &&
          plmnOpened && plmnClosed && (plmnDigits==5 || plmnDigits==6)) ++operators;
      tuple=false; return;
    }
    if (field==0) {
      if (c>='0' && c<='9') { if (++statusDigits>1) valid=false; status=uint8_t(c-'0'); }
      else if (c!=' ' && c!='\r') valid=false;
    } else if (field==3) {
      if (quoted && c>='0' && c<='9') { if (++plmnDigits>6) valid=false; }
      else if (quoted || (c!=' ' && c!='\r')) valid=false;
    }
  }
};
