#include "Loom_LTE.h"
#include "Logger.h"
#include "Utilities/Loom_GnssUtils.h"

namespace {
// u-blox SARA-R5 AT manual: UGPS and UGRMC response times are below 10 seconds.
// Allow one second of host margin, without extending this to the time-to-first-fix.
constexpr uint32_t GNSS_REPLY_MS = 11000;
bool readGnssReply(loomGnss::Fix *fix = nullptr) {
    return loomGnss::readReply(SerialAT, fix, millis, []() { delay(1); }, GNSS_REPLY_MS);
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::startGNSS() {
    FUNCTION_START;
    if (!isSaraR5() || !powerMayBeOn || !moduleInitialized) {
        WARNING(F("GNSS requires an initialized, powered SARA-R5 with an attached receiver."));
        return false;
    }
    if (gnssState == GnssState::RUNNING) {
        return true;
    }
    if (gnssState == GnssState::UNKNOWN) {
        WARNING(F("GNSS state is uncertain; stop it or power-cycle the modem before retrying."));
        return false;
    }
    LoomWatchdogPause watchdogPause;
    discardModemInput();
    SerialAT.print(F("AT+UGRMC=1\r\n")); // Cache RMC snapshots without enabling unsolicited output.
    if (!readGnssReply()) {
        return false;
    }
    gnssState = GnssState::UNKNOWN;       // A lost OK cannot prove that the power command failed.
    SerialAT.print(F("AT+UGPS=1,0\r\n")); // No aiding: this path never starts CellLocate/AssistNow.
    if (!readGnssReply()) {
        (void)stopGNSS(); // Best-effort cleanup, with uncertain state retained on failed stop.
        return false;
    }
    gnssState = GnssState::RUNNING;
    FUNCTION_END;
    return true; // Receiver started; this does not mean it already has a satellite fix.
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::stopGNSS() {
    FUNCTION_START;
    if (gnssState == GnssState::OFF) {
        return true;
    }
    if (!isSaraR5() || !powerMayBeOn) {
        return false;
    }
    LoomWatchdogPause watchdogPause;
    discardModemInput();
    SerialAT.print(F("AT+UGPS=0\r\n"));
    if (!readGnssReply()) {
        gnssState = GnssState::UNKNOWN;
        return false;
    }
    gnssState = GnssState::OFF;
    FUNCTION_END;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::readGNSS(loomGnss::Fix &fix) {
    FUNCTION_START;
    if (!isSaraR5() || !powerMayBeOn || !moduleInitialized || gnssState != GnssState::RUNNING) {
        return false;
    }
    LoomWatchdogPause watchdogPause;
    discardModemInput();
    SerialAT.print(F("AT+UGRMC?\r\n"));
    const bool received = readGnssReply(&fix);
    FUNCTION_END;
    return received;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
