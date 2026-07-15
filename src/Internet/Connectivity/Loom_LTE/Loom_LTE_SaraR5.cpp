
#include <TinyGsmClientSaraR5.h>

#include "Loom_LTE_TinyGsmAdapter.h"

using SaraR5Adapter = Loom_LTE_TinyGsmAdapter<
    TinyGsmSaraR5, TinyGsmSaraR5::GsmClientSaraR5>;

Loom_LTE_Modem* createSaraR5LteModem(Stream& stream){
    return new SaraR5Adapter(stream);
}

