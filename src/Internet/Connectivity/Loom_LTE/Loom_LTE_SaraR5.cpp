#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <TinyGsmClientSaraR5.h>
LOOM_EXTERNAL_INCLUDE_END

#include "Loom_LTE_TinyGsmAdapter.h"

using SaraR5Adapter = Loom_LTE_TinyGsmAdapter<TinyGsmSaraR5, TinyGsmSaraR5::GsmClientSaraR5>;

Loom_LTE_Modem *createSaraR5LteModem(Stream &stream) { return new SaraR5Adapter(stream); }
