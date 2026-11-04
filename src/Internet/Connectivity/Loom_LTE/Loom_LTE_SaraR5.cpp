#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <TinyGsmClientSaraR5.h>
LOOM_EXTERNAL_INCLUDE_END

#include "Loom_LTE_TinyGsmAdapter.h"
#include "Loom_LTE_SaraR5Data.h"
#include "Logger.h"

class SaraR5Adapter : public Loom_LTE_TinyGsmAdapter<TinyGsmSaraR5, TinyGsmSaraR5::GsmClientSaraR5> {
  public:
    explicit SaraR5Adapter(Stream &stream) : Loom_LTE_TinyGsmAdapter(stream) {}
    bool gprsConnect(const char *apn,const char *user,const char *pass) override {
        const char *failed=loomSaraR5OpenData(modem,apn,user,pass);
        if (failed) { ERRORF("SARA-R5 data setup failed at: %s",failed); return false; }
        // Activation OK alone is insufficient: require the actual internal IP.
        const IPAddress address=modem.localIP();
        if (address==IPAddress(0,0,0,0)) { ERROR(F("SARA-R5 internal profile has no IPv4 address.")); return false; }
        LOG(F("SARA-R5 internal IPv4 profile active on CID 1."));
        return true;
    }
};

Loom_LTE_Modem *createSaraR5LteModem(Stream &stream) { return new SaraR5Adapter(stream); }
