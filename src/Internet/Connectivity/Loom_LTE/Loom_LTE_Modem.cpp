#include "Loom_LTE_Modem.h"

Loom_LTE_Modem *createSaraR4LteModem(Stream &stream);
Loom_LTE_Modem *createSaraR5LteModem(Stream &stream);
Loom_LTE_Modem *createSim7090LteModem(Stream &stream);

Loom_LTE_Modem *createLteModem(LTE_MODEM type, Stream &stream) {
    if (type == LTE_MODEM::SIM7090) {
        return createSim7090LteModem(stream);
    }
    if (type == LTE_MODEM::SARA_R5) {
        return createSaraR5LteModem(stream);
    }
    return createSaraR4LteModem(stream);
}
