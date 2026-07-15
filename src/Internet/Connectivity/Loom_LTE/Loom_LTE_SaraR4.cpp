
#include <TinyGsmClientSaraR4.h>

#include "Loom_LTE_TinyGsmAdapter.h"

using SaraR4Adapter = Loom_LTE_TinyGsmAdapter<
    TinyGsmSaraR4, TinyGsmSaraR4::GsmClientSaraR4>;

Loom_LTE_Modem* createSaraR4LteModem(Stream& stream){
    return new SaraR4Adapter(stream);
}

