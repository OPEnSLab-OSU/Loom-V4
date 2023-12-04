#include "Loom_ACS712.h"

Loom_ACS712::Loom_ACS712(Manager &man, int port, int scaleFactor, int ACSOffset) : Module("ACS712"), manInst(&man), analogPort(port), mVperAmp(scaleFactor), offset(ACSOffset)
{
    analogReadResolution(12);

    rawVal = 0;
    amps = 0.0;
    voltage = 0.0;

    module_address = port;

    manInst->registerModule(this);
}

void Loom_ACS712::measure()
{
    rawVal = analogRead(analogPort);
    //rawVal = analogToMV(analogRead(analogPort));
    voltage = ((rawVal / 4) / 1024.0) * 5000;
    amps = ((voltage - offset) / mVperAmp);

    return;
}

void Loom_ACS712::package()
{
    JsonObject json = manInst->get_data_object(getModuleName());
    json["RawValue"] = rawVal;
    json["Amps"] = amps;
    json["Voltage"] = voltage;

    return;
}
