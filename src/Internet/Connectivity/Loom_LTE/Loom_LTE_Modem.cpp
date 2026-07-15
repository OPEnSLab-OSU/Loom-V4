#include "Loom_LTE_Modem.h"

Loom_LTE_Modem* createSaraR4LteModem(Stream& stream);
Loom_LTE_Modem* createSaraR5LteModem(Stream& stream);

Loom_LTE_Modem* createLteModem(bool saraR5, Stream& stream){
    if(saraR5)
        return createSaraR5LteModem(stream);
    return createSaraR4LteModem(stream);
}

