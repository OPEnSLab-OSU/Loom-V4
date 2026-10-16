#include "Loom_WindVane.h"

namespace {
//Nominal 12-bit ADC readings from North clockwise, based on the 10k pull-down.
const uint16_t DIRECTION_ADC_VALUES[] = {
    955, 2475, 2255, 3766, 3730, 3836, 3362, 3595,
    2950, 3121, 1580, 1700, 320, 790, 550, 1290
};
const uint16_t MAX_ADC_DEVIATION = 25;

// 360 / 16 positions. A float so the multiplication below gives fractional degrees.
const float DEGREES_PER_DIRECTION = 22.5f;
}
 
Loom_WindVane::Loom_WindVane(Manager& man, uint8_t pin)
    : Module("Loom_WindVane"), manInst(&man), directionPin(pin) {
    manInst->registerModule(this);
}
 
void Loom_WindVane::initialize() {
    analogReadResolution(12);
    pinMode(directionPin, INPUT);
    moduleInitialized = true;
}
 
void Loom_WindVane::measure() {
    //Don't touch the hardware if setup never happened.
    if (!moduleInitialized) {
        return;
    }
 
    // analogRead returns int; cast down to uint16_t for the member. The static_cast
    // makes the narrowing explicit and avoids compiler warnings. 12-bit values always fit.
    rawADC = static_cast<uint16_t>(analogRead(directionPin));
 
    uint8_t closestDirection = 0;
 
    // Start at the largest possible value so the first comparison always replaces it.
    uint16_t smallestDeviation = UINT16_MAX;
 
    for (uint8_t direction = 0;
         direction < sizeof(DIRECTION_ADC_VALUES) / sizeof(DIRECTION_ADC_VALUES[0]);
         ++direction) { 
        const uint16_t expected = DIRECTION_ADC_VALUES[direction];
 
        // Absolute difference done by hand with a ternary. These are
        // unsigned, so "rawADC - expected" when rawADC < expected would wrap around to
        // a huge number (e.g. 5 - 10 = 65531) rather than going negative. Always
        // subtracting the smaller from the larger avoids that.
        const uint16_t deviation =
            rawADC > expected ? rawADC - expected : expected - rawADC;
 
        // Strict "<" means that on an exact tie, the earlier (lower-index) direction wins.
        if (deviation < smallestDeviation) {
            smallestDeviation = deviation;
            closestDirection = direction;
        }
    }
 
    // Accept the match only if it was close enough. The loop always finds *some*
    // closest entry, even for garbage input, so this is what separates a real
    // reading from noise or a fault.
    directionValid = smallestDeviation <= MAX_ADC_DEVIATION;
 
    // Index -> degrees (index 0 = North = 0, index 4 = East = 90, and so on).
    // -1 sentinel when invalid so a bad reading is never mistaken for a real heading.
    windDirectionDegrees = directionValid
        ? closestDirection * DEGREES_PER_DIRECTION
        : -1.0f;
}
 
void Loom_WindVane::package() {
    if (!moduleInitialized) {
        return;
    }
 
    JsonObject json = manInst->get_data_object(getModuleName());
 
    json["wind_direction_degrees"] = windDirectionDegrees;  // -1 if invalid
    json["wind_direction_adc"] = rawADC;                    // raw value, useful for calibration
    json["wind_direction_valid"] = directionValid;          // lets consumers filter out bad readings
}