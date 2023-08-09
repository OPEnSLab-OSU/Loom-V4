#include "Loom_TippingBucket.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TippingBucket::Loom_TippingBucket(Manager& man) : Module("TippingBucket"), manInst(&man), module_address(COUNTER_ADDRESS) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TippingBucket::Loom_TippingBucket(Manager& man, int pin) : Module("TippingBucket"), manInst(&man) interruptPin(pin), module_address(-1) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::initialize() {
    Wire.begin();

    /* If we are using the hypnos we want to set the start datetime now*/
    if(hypnosInst != nullptr){
        lastHourDT = hypnosInst->getCurrentTime();          // Current date time in the local timezone of which the device was set
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::measure() {
    /* First check if we are actually meant to be reading the values from our I2C device, this shouldn't occur if we are using interrupts */

    unsigned long lastTipCount = tipCount;
    if(module_address != -1){
        Wire.requestFrom(COUNTER_ADDRESS, 3);
        unsigned int byte1 = Wire.read();
        unsigned int byte2 = Wire.read();
        unsigned int byte3 = Wire.read();

        tipCount = byte3 + (byte2 << 8) + (byte1 << 16);
    }
    else{
        /* Do some interrupt stuff*/
    }

    /* If we are using the hypnos we want to check the RTC to calculate how many tips occurred in the last hour*/
    if(hypnosInst != nullptr){
        DateTime oneHourAheadOfLast = lastHourDT + TimeSpan(0, 1, 0, 0);
        if(hypnosInst->getCurrentTime() < oneHourAheadOfLast){
            hourTips += tipCount - lastTipCount;
        }
        else{
            hourTips = 0;
            lastHourDT = hypnosInst->getCurrentTime();
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::package() {
    JsonObject json = manInst->get_data_object(getModuleName());
    json["Tips"] = tipCount;

    if(hypnosInst != nullptr)
        json["Hourly_Tips"] = hourTips;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////