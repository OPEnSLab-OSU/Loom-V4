#include "Loom_TippingBucket.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TippingBucket::Loom_TippingBucket(Manager &man, COUNTER_TYPE type, float inchesPerTip)
    : Module("TippingBucket"), manInst(&man), inchesPerTip(inchesPerTip) {
    for (size_t i = 0; i < HISTORY_BUCKET_COUNT; ++i) {
        tipHistory[i].minute = INVALID_MINUTE;
        tipHistory[i].tips = 0;
    }

    if (type == COUNTER_TYPE::I2C)
        module_address = COUNTER_ADDRESS;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::initialize() {
    if (module_address != -1)
        Wire.begin();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::measure() {
    /* First check if we are actually meant to be reading the values from our I2C device, this
     * shouldn't occur if we are using interrupts */

    bool counterUpdated = true;
    if (module_address != -1) {
        const uint8_t bytesReceived = Wire.requestFrom(COUNTER_ADDRESS, 3, true);
        if (bytesReceived == 3 && Wire.available() >= 3) {
            const uint32_t byte1 = static_cast<uint8_t>(Wire.read());
            const uint32_t byte2 = static_cast<uint8_t>(Wire.read());
            const uint32_t byte3 = static_cast<uint8_t>(Wire.read());
            tipCount = (byte1 << 16) | (byte2 << 8) | byte3;
        } else {
            while (Wire.available())
                Wire.read();
            ERROR(F("Tipping bucket counter returned an incomplete I2C response"));
            counterUpdated = false;
        }
    }

    unsigned long newTips = 0;
    if (counterUpdated) {
        // A lower counter normally means the external counter restarted. Treat its new value as
        // post-reset tips instead of allowing unsigned subtraction to create billions of tips.
        newTips = tipCount >= lastTipCount ? tipCount - lastTipCount : tipCount;
    }

    /* If we are using the hypnos we want to check the RTC to calculate how many tips occurred in
     * the last hour*/
    if (hypnosInst != nullptr) {
        updateHourlyTips(hypnosInst->getCurrentTime().unixtime(), newTips);
    }

    if (counterUpdated)
        lastTipCount = tipCount;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::updateHourlyTips(uint32_t currentTime, unsigned long newTips) {
    const uint32_t currentMinute = currentTime / 60UL;
    TipHistoryBucket &currentBucket = tipHistory[currentMinute % HISTORY_BUCKET_COUNT];

    if (currentBucket.minute != currentMinute) {
        currentBucket.minute = currentMinute;
        currentBucket.tips = 0;
    }
    currentBucket.tips += newTips;

    hourlyTips = 0;
    for (size_t i = 0; i < HISTORY_BUCKET_COUNT; ++i) {
        TipHistoryBucket &bucket = tipHistory[i];
        const bool inWindow = bucket.minute != INVALID_MINUTE && currentMinute >= bucket.minute &&
                              currentMinute - bucket.minute < HISTORY_BUCKET_COUNT;
        if (inWindow) {
            hourlyTips += bucket.tips;
        } else if (bucket.minute != INVALID_MINUTE) {
            // Also clears future-dated buckets if the RTC is corrected backwards.
            bucket.minute = INVALID_MINUTE;
            bucket.tips = 0;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TippingBucket::package() {
    JsonObject json = manInst->get_data_object(getModuleName());
    json["Tips"] = tipCount;
    json["Total_Rainfall(in)"] = tipsToInches(tipCount);

    if (hypnosInst != nullptr)
        json["Hourly_Tips"] = hourlyTips;
    json["Hourly_Rainfall(in)"] = tipsToInches(hourlyTips);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_TippingBucket::tipsToInches(unsigned long tips) { return tips * inchesPerTip; }
//////////////////////////////////////////////////////////////////////////////////////////////////////
