#pragma once
#include "Logic.h"
#include <stdio.h>

struct NetworkCredentials { const char *apn,*user,*password; int authentication; };
enum class RegistrationResult : uint8_t {
  ALREADY_REGISTERED, REGISTERED, NO_AT, WRONG_MODEL, SIM_NOT_READY,
  STATUS_UNREADABLE, INVALID_CREDENTIALS, CONFIG_FAILED, SELECTION_FAILED,
  DENIED, TIMED_OUT, CANCELLED
};
class RegistrationIO {
public:
  virtual ~RegistrationIO() {}
  virtual bool command(const char *text,uint32_t timeout,bool sensitive=false)=0;
  virtual const char *response() const=0;
  virtual bool completed() const=0; // final OK/ERROR received, not just timeout
  virtual bool cancelled() const=0;
  virtual uint32_t now() const=0;
  virtual void pause(uint32_t ms)=0;
  virtual void phase(const char *text,bool canCancel)=0;
};
inline bool registeredStatus(int status) { return status==1 || status==5; }
inline bool registrationSucceeded(RegistrationResult r) {
  return r==RegistrationResult::ALREADY_REGISTERED || r==RegistrationResult::REGISTERED;
}
inline bool credentialsValid(const NetworkCredentials &c) {
  return atFieldSafe(c.apn,100,false) && atFieldSafe(c.user,50) && atFieldSafe(c.password,50) &&
         c.authentication>=0 && c.authentication<=3 &&
         (c.authentication!=0 || (!c.user[0] && !c.password[0]));
}
// Single registration attempt, invoked by an explicit console command. Normal
// diagnostic/baseline probes never call this function or change carrier settings.
inline RegistrationResult registerIfNeeded(RegistrationIO &io,const NetworkCredentials &credentials,bool att) {
  io.phase("Checking AT, model, SIM and existing registration",false);
  if (!io.command("AT",3000)) return RegistrationResult::NO_AT;
  if (!io.command("AT+CGMM",3000) || !strstr(io.response(),"SARA-R5")) return RegistrationResult::WRONG_MODEL;
  if (!io.command("AT+CPIN?",3000) || !lineEquals(io.response(),"+CPIN: READY")) return RegistrationResult::SIM_NOT_READY;
  if (!io.command("AT+CEREG?",3000)) return RegistrationResult::STATUS_UNREADABLE;
  int status=registrationStatus(io.response());
  if (status<0) return RegistrationResult::STATUS_UNREADABLE;
  // An existing home OR roaming attachment is preserved even for 'att'.
  if (registeredStatus(status)) return RegistrationResult::ALREADY_REGISTERED;
  if (!credentialsValid(credentials)) return RegistrationResult::INVALID_CREDENTIALS;
  io.phase("Configuring inactive EPS context 1; MNO profile is preserved",false);
  // R5 initial default bearer settings must be updated while deregistered.
  // This is RF minimum-functionality, not a reset or GPIO power pulse.
  if (!io.command("AT+CFUN=0",15000)) return RegistrationResult::CONFIG_FAILED;
  char command[256];
  snprintf(command,sizeof(command),"AT+CGDCONT=1,\"IP\",\"%s\"",credentials.apn);
  bool configured=io.command(command,5000);
  if (configured) {
    snprintf(command,sizeof(command),"AT+UAUTHREQ=1,%d,\"%s\",\"%s\"",credentials.authentication,credentials.user,credentials.password);
    configured=io.command(command,5000,true);
    memset(command,0,sizeof(command));
  }
  // Restore RF after a completed configuration rejection. Do not overlap a
  // still-pending command with another AT transaction after timeout.
  if (!configured) {
    if (io.completed()) io.command("AT+CFUN=1",15000);
    return RegistrationResult::CONFIG_FAILED;
  }
  if (!io.command("AT+CFUN=1",15000)) return RegistrationResult::CONFIG_FAILED;
  io.phase(att ? "Selecting AT&T 310410 / LTE; Ctrl+] cancels" : "Selecting automatic operator; Ctrl+] cancels",true);
  if (io.cancelled()) return RegistrationResult::CANCELLED;
  bool selected=true;
  if (att) selected=io.command("AT+COPS=1,2,\"310410\",7",180000);
  else {
    if (!io.command("AT+COPS?",3000)) return io.cancelled() ? RegistrationResult::CANCELLED : RegistrationResult::STATUS_UNREADABLE;
    int mode=operatorMode(io.response());
    if (mode<0) return RegistrationResult::STATUS_UNREADABLE;
    // CFUN=1 already triggers registration in automatic mode. COPS=0 would
    // unnecessarily start a new full high-priority PLMN search in that case.
    if (mode!=0) selected=io.command("AT+COPS=0",180000);
  }
  if (io.cancelled()) return RegistrationResult::CANCELLED;
  if (!selected && !io.completed()) return RegistrationResult::TIMED_OUT;
  // A completed COPS error can race successful registration; judge the query,
  // never the COPS command's OK alone. Denial does not trigger repeated scans.
  io.phase("Polling EPS registration (up to 120 s); Ctrl+] cancels",true);
  uint32_t start=io.now();
  do {
    if (io.cancelled()) return RegistrationResult::CANCELLED;
    bool queried=io.command("AT+CEREG?",3000);
    if (io.cancelled()) return RegistrationResult::CANCELLED;
    if (!queried) return io.completed() ? RegistrationResult::STATUS_UNREADABLE : RegistrationResult::TIMED_OUT;
    status=registrationStatus(io.response());
    if (registeredStatus(status)) return RegistrationResult::REGISTERED;
    if (status<0) return RegistrationResult::STATUS_UNREADABLE;
    if (status==3) return RegistrationResult::DENIED;
    if (!selected) return RegistrationResult::SELECTION_FAILED;
    io.pause(2000);
  } while (uint32_t(io.now()-start)<120000);
  return io.cancelled() ? RegistrationResult::CANCELLED : RegistrationResult::TIMED_OUT;
}
