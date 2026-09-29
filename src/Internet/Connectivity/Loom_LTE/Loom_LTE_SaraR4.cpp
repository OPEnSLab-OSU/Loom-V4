#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <TinyGsmClientSaraR4.h>
LOOM_EXTERNAL_INCLUDE_END

#include "Loom_LTE_TinyGsmAdapter.h"

using SaraR4Adapter = Loom_LTE_TinyGsmAdapter<TinyGsmSaraR4, TinyGsmSaraR4::GsmClientSaraR4>;

Loom_LTE_Modem *createSaraR4LteModem(Stream &stream) { return new SaraR4Adapter(stream); }
