#include "Loom_TippingBucket.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TippingBucket::Loom_TippingBucket(Manager& man, float inchesPerTip) : Module("TippingBucket"), manInst(&man), inchesPerTip(inchesPerTip) {
    module_address = COUNTER_ADDRESS;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TippingBucket::Loom_TippingBucket(Manager& man, int pin, float inchesPerTip) : Module("TippingBucket"), manInst(&man), interruptPin(pin), inchesPerTip(inchesPerTip) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::initialize() {
    Wire.begin();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::measure() {
    /* First check if we are actually meant to be reading the values from our I2C device, this shouldn't occur if we are using interrupts */

    unsigned long lastTipCount = tipCount;
    if(module_address != -1){
        Wire.requestFrom(COUNTER_ADDRESS, 3, false);
        unsigned int byte1 = Wire.read();
        unsigned int byte2 = Wire.read();
        unsigned int byte3 = Wire.read();
        Wire.endTransmission();
        
        // Possible bit hack for counting by 2?
        tipCount = 0;
        tipCount |= (byte1 << 16) + (byte2 << 8) + byte3;
    }
    else{
        /* Do some interrupt stuff */
    }

    /* If we are using the hypnos we want to check the RTC to calculate how many tips occurred in the last hour*/
    if(hypnosInst != nullptr){
        if(times.size() <= 1){
            times.push_front(hypnosInst->getCurrentTime().unixtime());          // Push the start time onto the front of the queue
            tips.push_front(tipCount);                                          // Push the current number of tips onto the front of the queue
        }
        else{
            // If it has been more than one hour since the oldest tip, we want to remove the oldest data
            if(times.front() >= times.back() + ONE_HOUR_UNIX){
                times.pop_back();
                tips.pop_back();
            }

            // Always push new data in
            times.push_front(hypnosInst->getCurrentTime().unixtime());          
            tips.push_front(tipCount);
        }

        // Get the initial number of tips at the start of the hour
        int oldest = tips.back();
        
        // Set the hourly tips back to zero so we can re-calculate it with the new data
        hourlyTips = 0;

        // Loop over all elements in the queue, accumulate the amount in the last hour and subtract the oldest tip value to get the change in the last hour
        for(int i = 0; i < tips.size(); i++){
            hourlyTips += tips[i] - oldest;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::package() {
    JsonObject json = manInst->get_data_object(getModuleName());
    json["Tips"] = tipCount;

    if(hypnosInst != nullptr)
        json["Hourly_Tips"] = hourlyTips;
        json["Hourly_Rainfall(in)"] = tipsToInches(hourlyTips);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_TippingBucket::tipsToInches(unsigned long tips) {
    return tips * inchesPerTip;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////