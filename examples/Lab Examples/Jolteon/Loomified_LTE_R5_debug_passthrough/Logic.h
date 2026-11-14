#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

inline bool unsignedNumber(const char *s, uint32_t &out, uint32_t max) {
  if (!s || !*s) return false;
  uint32_t n=0;
  for (;*s;++s) { if (*s<'0' || *s>'9') return false; uint8_t d=uint8_t(*s-'0');
    if (d>max || n>(max-d)/10) return false; n=n*10+d; }
  out=n; return true;
}
inline bool lineEquals(const char *s, const char *line) {
  size_t n=strlen(line);
  while (*s) { const char *end=strchr(s,'\n'); size_t k=end ? size_t(end-s) : strlen(s);
    if (k && s[k-1]=='\r') --k;
    if (k==n && !memcmp(s,line,n)) return true;
    if (!end) break; s=end+1; }
  return false;
}
inline int registrationStatus(const char *s) {
  int found=-1;
  while (s && *s) {
    const char *end=strchr(s,'\n'); if (!end) end=s+strlen(s);
    const char *p=s;
    if (end-p>=7 && !memcmp(p,"+CEREG:",7)) {
      p+=7; while (p<end && *p==' ') ++p;
      if (p<end && *p>='0' && *p<='5') {
        ++p; while (p<end && *p==' ') ++p;
        if (p<end && *p==',') {
          ++p; while (p<end && *p==' ') ++p;
          int stat=-1;
          if (p<end && *p>='0' && *p<='9') { stat=*p++-'0';
            if (p<end && *p>='0' && *p<='9') stat=stat*10+*p++-'0'; }
          while (p<end && *p==' ') ++p;
          if (stat>=0 && stat<=10 && (p==end || *p==',' || (*p=='\r' && p+1==end))) found=stat;
        }
      }
    }
    if (!*end) break; s=end+1;
  }
  return found;
}
inline bool registration(const char *s) {
  int status=registrationStatus(s); return status==1 || status==5;
}
inline int operatorMode(const char *s) {
  while (s && *s) {
    const char *end=strchr(s,'\n'); if (!end) end=s+strlen(s);
    const char *p=s;
    if (end-p>=6 && !memcmp(p,"+COPS:",6)) {
      p+=6; while (p<end && *p==' ') ++p;
      if (p<end && *p>='0' && *p<='4') {
        int mode=*p++-'0'; while (p<end && *p==' ') ++p;
        if (p==end || *p==',' || (*p=='\r' && p+1==end)) return mode;
      }
    }
    if (!*end) break; s=end+1;
  }
  return -1;
}
inline bool atFieldSafe(const char *s,size_t maximum,bool allowEmpty=true) {
  if (!s || (!allowEmpty && !*s)) return false;
  size_t n=0;
  for (;*s;++s) if (++n>maximum || *s<32 || *s>126 || *s=='"' || *s=='\\') return false;
  return true;
}
struct PendingAtReply {
  bool active=false,hidden=false;
  char line[64]={}; size_t used=0; bool overflow=false;
  void begin(const char *reply,bool privateResponse) {
    active=true; hidden=privateResponse; used=0; overflow=false;
    const char *tail=strrchr(reply,'\n'); tail=tail ? tail+1 : reply;
    while (*tail && used+1<sizeof(line)) line[used++]=*tail++;
    overflow=bool(*tail); line[used]=0;
  }
  bool feed(int ch) {
    if (!active) return false;
    if (ch=='\n') {
      line[used]=0;
      bool done=!overflow && (lineEquals(line,"OK") || lineEquals(line,"ERROR") ||
          !strncmp(line,"+CME ERROR:",11) || !strncmp(line,"+CMS ERROR:",11));
      used=0; overflow=false;
      if (done) active=hidden=false;
      return done;
    }
    if (!overflow && used+1<sizeof(line)) line[used++]=char(ch); else overflow=true;
    return false;
  }
};
inline uint8_t bcd(uint8_t n) { return uint8_t((n/10)*16+n%10); }
inline bool unbcd(uint8_t b, uint8_t max, uint8_t &n) {
  n=uint8_t((b>>4)*10+(b&15)); return (b&15)<10 && (b>>4)<10 && n<=max;
}
inline bool leap(uint16_t y) { return y%4==0 && (y%100!=0 || y%400==0); }
inline uint8_t monthDays(uint16_t y,uint8_t m) {
  const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  return m>=1 && m<=12 ? uint8_t(days[m-1]+(m==2 && leap(y))) : 0;
}
inline bool rtcEpoch(const uint8_t *r,uint32_t &epoch) {
  uint8_t s,m,h,dow,d,mo,y;
  if (!unbcd(r[0],59,s) || !unbcd(r[1],59,m) || !unbcd(r[2]&0x3f,23,h) || (r[2]&0xc0) ||
      !unbcd(r[3],7,dow) || !dow || !unbcd(r[4],31,d) || !unbcd(r[5],12,mo) || !mo ||
      !unbcd(r[6],99,y) || !d || d>monthDays(2000+y,mo)) return false;
  uint32_t days=0; for (uint16_t yr=2000;yr<2000+y;++yr) days+=leap(yr)?366:365;
  for (uint8_t mon=1;mon<mo;++mon) days+=monthDays(2000+y,mon);
  epoch=946684800ul+(days+d-1)*86400ul+h*3600ul+m*60ul+s; return true;
}
inline bool epochRtc(uint32_t epoch,uint8_t *r) {
  if (epoch<946684800ul || epoch>=4102444800ul) return false;
  uint32_t t=epoch-946684800ul, days=t/86400; uint16_t y=2000; uint8_t m=1;
  r[0]=bcd(t%60); r[1]=bcd((t/60)%60); r[2]=bcd((t/3600)%24);
  r[3]=bcd((days+6)%7+1); // 2000-01-01 was Saturday, 1=Sunday.
  while (days>=uint32_t(leap(y)?366:365)) { days-=leap(y)?366:365; ++y; }
  while (days>=monthDays(y,m)) { days-=monthDays(y,m); ++m; }
  r[4]=bcd(uint8_t(days+1)); r[5]=bcd(m); r[6]=bcd(uint8_t(y-2000)); return true;
}
inline bool sdiMeasurement(const char *s,uint16_t &seconds,uint8_t &values) {
  if (!s || strlen(s)!=7 || s[5]!='\r' || s[6]!='\n' || s[4]<'1' || s[4]>'9') return false;
  seconds=0;
  for (uint8_t i=1;i<=3;++i) { if (s[i]<'0' || s[i]>'9') return false; seconds=uint16_t(seconds*10+s[i]-'0'); }
  values=uint8_t(s[4]-'0'); return true;
}
inline int sdiValueCount(const char *s) {
  if (!s || !*s) return -1;
  const char *p=s+1; int count=0;
  while (*p && *p!='\r') {
    if (*p!='+' && *p!='-') return -1; ++p;
    uint8_t digits=0; bool dot=false;
    while ((*p>='0' && *p<='9') || *p=='.') {
      if (*p=='.') { if (dot) return -1; dot=true; } else ++digits;
      if (digits>7) return -1; ++p;
    }
    if (!digits) return -1; ++count;
  }
  return *p=='\r' && p[1]=='\n' && !p[2] ? count : -1;
}
