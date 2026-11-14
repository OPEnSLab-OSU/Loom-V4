#pragma once
#include "arduino_secrets.h"
#ifndef NETWORK_NAME
#define NETWORK_NAME ""
#endif
#ifndef NETWORK_USER
#define NETWORK_USER ""
#endif
#ifndef NETWORK_PASS
#define NETWORK_PASS ""
#endif
// Existing example uses Hologram. Empty local APN keeps that default.
inline const char *networkApn() { return NETWORK_NAME[0] ? NETWORK_NAME : "hologram"; }
// SARA-R5: 0=none, 1=PAP, 2=CHAP, 3=automatic. Credentials are optional.
#ifndef NETWORK_AUTH
#define NETWORK_AUTH ((NETWORK_USER[0] || NETWORK_PASS[0]) ? 3 : 0)
#endif
