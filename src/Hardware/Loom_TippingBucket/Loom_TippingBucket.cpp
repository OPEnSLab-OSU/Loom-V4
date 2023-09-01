#include "Loom_TippingBucket.h"

int Loom_TippingBucket::volatileTipCount = 0;
int Loom_TippingBucket::wasTip = false;

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TippingBucket::Loom_TippingBucket(Manager& man, COUNTER_TYPE type, float inchesPerTip = 0.01) : Module("TippingBucket"), manInst(&man), inchesPerTip(inchesPerTip) {
    if(type == COUNTER_TYPE::I2C)
        module_address = COUNTER_ADDRESS;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::initialize() {
    if(module_address != -1)
        Wire.begin();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::power_down() {
    if(module_address == -1){
        attachInterrupt(interruptPin, callbackFunc, FALLING);
        attachInterrupt(interruptPin, callbackFunc, FALLING);
    }
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

        /* Loop over the last hour to accumlate the number of tips that occured within the last hour and we want to subtract the current value minus the last to get the difference and add that*/
        hourlyTips += tips[0];
        for(int i = 1; i < tips.size(); i++){
            hourlyTips += tips[i] - tips[i-1];
        }

        hourlyTips -= oldest;
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