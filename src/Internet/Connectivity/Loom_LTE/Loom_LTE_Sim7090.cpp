#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <TinyGsmClientSIM7080.h>
LOOM_EXTERNAL_INCLUDE_END

#include "Loom_LTE_TinyGsmAdapter.h"
#include "Utilities/Loom_Sim70xxGnss.h"

// TinyGSM uses its SIM7080 type for the SIM7070/SIM7080/SIM7090 family.
class Sim7090Adapter : public Loom_LTE_TinyGsmAdapter<TinyGsmSim7080,
                                                   TinyGsmSim7080::GsmClientSim7080> {
  public:
    explicit Sim7090Adapter(Stream &stream) : Loom_LTE_TinyGsmAdapter(stream) {}

    bool init() override {
        if (!modem.init()) return false;
        // Feather Serial1 uses TX/RX only. Persisted hardware flow control would
        // otherwise stall subsequent traffic with CTS/RTS disconnected.
        modem.sendAT(GF("+IFC=0,0"));
        return modem.waitResponse() == 1;
    }

    bool poweroffChecked() override {
        modem.sendAT(GF("+CPOWD=1"));
        return modem.waitResponse(10000UL, GF("NORMAL POWER DOWN")) == 1;
    }

    bool gprsConnect(const char *apn, const char *user, const char *pass) override {
        // TinyGSM 0.12.0 can treat its DEACTIVE match index as true. Verify
        // packet attachment and a nonzero application IP before reporting success.
        return modem.gprsConnect(apn, user, pass) && modem.isGprsConnected();
    }

    bool startGNSS() override { return modem.enableGPS(); }
    bool stopGNSS() override { return modem.disableGPS(); }
    bool readGNSS(loomGnss::Fix &fix) override {
        // TinyGSM owns the AT transaction. Parsing the returned CSV avoids its
        // fixed trailing-field count, which differs between SIM70xx firmwares.
        String raw = modem.getGPSraw();
        if (raw.length() == 0 || raw.length() >= 256) return false;
        char text[256];
        raw.toCharArray(text, sizeof(text));
        return loomGnss::parseSim70xx(text, fix);
    }
};

Loom_LTE_Modem *createSim7090LteModem(Stream &stream) { return new Sim7090Adapter(stream); }
