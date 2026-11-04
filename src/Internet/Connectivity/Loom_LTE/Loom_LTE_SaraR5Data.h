#pragma once
#include <stddef.h>

// R5 uses the CID 1 that Loom's setPdpContext() explicitly configures. Do not
// rediscover it by comparing CGDCONT APNs: networks may append .mnc...mcc...gprs.
// UBX-19047455 sections 14.6/14.7: set profile APN BEFORE mapping it to the CID.
// The bundled TinyGSM R5 routine omits that step and parses CGDCONT unsafely.
inline bool loomSaraR5QuotedValue(const char *text,size_t limit,bool required=false) {
    if (!text) return !required;
    size_t length=0;
    for (;text[length];++length) {
        const unsigned char ch=text[length];
        if (length>=limit || ch<32 || ch>126 || ch=='"' || ch=='\\') return false;
    }
    return !required || length>0;
}

// Returns the failed stage (never credentials), or nullptr on success.
// Templated only to exercise this actual AT sequence with a transcript in host tests.
template<class Modem>
const char *loomSaraR5OpenData(Modem &modem,const char *apn,const char *user,const char *pass) {
    if (!loomSaraR5QuotedValue(apn,99,true) || !loomSaraR5QuotedValue(user,50) ||
        !loomSaraR5QuotedValue(pass,50)) return "invalid APN/authentication fields";
    modem.sendAT(F("+CMEE=2"));
    if (modem.waitResponse(3000L)!=1) return "enable verbose errors";

    // Deactivate the internal profile; ERROR is expected when already inactive.
    // A timeout is different: never send a new command over an unfinished action.
    modem.sendAT(F("+UPSDA=0,4"));
    if (modem.waitResponse(180000L)==0) return "deactivate internal profile (timeout)";
    if ((user && *user) || (pass && *pass)) {
        // R5 does not support UPSD user/password parameters. UAUTHREQ requires
        // a defined, inactive CID; configure it before attaching/activating.
        modem.sendAT(F("+CGACT=0,1"));
        if (modem.waitResponse(180000L)!=1) return "deactivate CID 1 for authentication";
        modem.sendAT(F("+UAUTHREQ=1,3,\""),user?user:"",F("\",\""),pass?pass:"",F("\""));
        if (modem.waitResponse(10000L)!=1) return "configure CID 1 authentication";
    }
    modem.sendAT(F("+CGATT=1"));
    if (modem.waitResponse(180000L)!=1) return "packet-service attach";
    modem.sendAT(F("+UPSD=0,1,\""),apn,F("\""));
    if (modem.waitResponse(10000L)!=1) return "set internal profile APN";
    // Loom configures CGDCONT CID 1 as IP (IPv4). Match that protocol before activation.
    modem.sendAT(F("+UPSD=0,0,0"));
    if (modem.waitResponse(10000L)!=1) return "set internal profile IPv4";
    modem.sendAT(F("+UPSD=0,100,1"));
    if (modem.waitResponse(10000L)!=1) return "map internal profile to CID 1";
    modem.sendAT(F("+UPSDA=0,3"));
    if (modem.waitResponse(180000L)!=1) return "activate internal profile";
    return nullptr;
}
