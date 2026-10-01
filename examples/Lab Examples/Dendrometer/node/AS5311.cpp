#include "AS5311.h"
#include <Loom_Manager.h>
#include <limits.h>
#include <math.h>

const int AS5311::DATA_TIMING_US = 12;
// 1000us/bit was the value in the V3 code
// according to the datasheet the chip's limit is 1MHz

const int AS5311::AVERAGE_MEASUREMENTS = 16;

AS5311::AS5311(uint8_t cs_pin, uint8_t clk_pin, uint8_t do_pin)
    : CS_PIN(cs_pin), CLK_PIN(clk_pin), DO_PIN(do_pin) {}

/**
 *  Initialize pins for serial read procedure
 */
void AS5311::initializePins() {
    // initalize pins
    digitalWrite(CS_PIN, HIGH);
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CLK_PIN, HIGH);
    pinMode(CLK_PIN, OUTPUT);
    pinMode(DO_PIN, INPUT);
}

/**
 * deinitialize pins after serial read
 */
void AS5311::deinitializePins() {
    pinMode(CS_PIN, INPUT);
    digitalWrite(CS_PIN, LOW);
    pinMode(CLK_PIN, INPUT);
    digitalWrite(CLK_PIN, LOW);
    pinMode(DO_PIN, INPUT);
}

/**
 *  Returns the serial output from AS5311
 * @return 32 bit value, of which the 18 least signifcant bits contain the sensor data
 */
uint32_t AS5311::bitbang(bool angleData) {
    initializePins();

    if (angleData) {
        digitalWrite(CLK_PIN, HIGH); // write clock high to select the angular position data
    } else {
        digitalWrite(CLK_PIN, LOW); // A low clock selects magnetic field strength data.
    }

    delayMicroseconds(DATA_TIMING_US);
    // select the chip
    digitalWrite(CS_PIN, LOW);
    delayMicroseconds(DATA_TIMING_US);

    // begin the data transfer
    digitalWrite(CLK_PIN, LOW);

    uint32_t data = 0;
    const uint8_t BITS = 18;
    for (int i = 0; i < BITS; i++) {
        delayMicroseconds(DATA_TIMING_US);
        digitalWrite(CLK_PIN, HIGH);

        // don't set clock low on last bit
        if (i < (BITS - 1)) {
            delayMicroseconds(DATA_TIMING_US);
            digitalWrite(CLK_PIN, LOW);
        }

        delayMicroseconds(DATA_TIMING_US);

        auto readval = digitalRead(DO_PIN);
        if (readval == HIGH) {
            data |= 1UL << ((BITS - 1) - i);
        }
    }

    digitalWrite(CS_PIN, HIGH);
    digitalWrite(CLK_PIN, HIGH);
    delayMicroseconds(DATA_TIMING_US);
    deinitializePins();

    return data;
}

/**
 * Determine the magnet alignment status
 * See pages 12 to 15 of the AS5311 datasheet for more information
 * @return magnetStatus enum
 */
bool AS5311::isValidReading(uint32_t data) {
    // Datasheet sections 7.3/7.5: offset compensation must finish, CORDIC overflow invalidates
    // the conversion, and even parity protects all 18 serial bits. Zero is not a ready reading.
    if (data >= (1UL << 18) || (data & (1UL << OCF)) == 0 || (data & (1UL << COF)) != 0) {
        return false;
    }
    // Fold the serial bits together: the low bit is one for odd parity. This fixed-size
    // calculation works with embedded and host compilers without a compiler-specific builtin.
    uint32_t parity = data;
    parity ^= parity >> 16;
    parity ^= parity >> 8;
    parity ^= parity >> 4;
    parity ^= parity >> 2;
    parity ^= parity >> 1;
    return (parity & 1U) == 0;
}

magnetStatus AS5311::getMagnetStatus() {
    uint32_t data = bitbang();

    // invalid data
    if (!isValidReading(data)) {
        return magnetStatus::error;
    }

    // magnetic field out of range
    if (data & (1UL << LIN)) {
        return magnetStatus::red;
    }

    // magnetic field borderline out of range
    if (data & (1 << MAGINC) && data & (1 << MAGDEC)) {
        return magnetStatus::yellow;
    }

    return magnetStatus::green;
}

/**
 * Return the raw sensor binary data
 * @return raw sensor data
 */
uint32_t AS5311::getRawData() { return bitbang(true); }

/**
 * Right shift the raw sensor data to isolate the absolute position component
 * @return 12-bit absolute postion value
 */
uint16_t AS5311::getPosition() {
    const uint32_t data = bitbang(true);
    // A linearity alarm may contain invalid position data; report missing rather than inventing
    // tree motion. Alignment still reports its red/yellow/green diagnostic separately.
    return isValidReading(data) && (data & (1UL << LIN)) == 0
               ? static_cast<uint16_t>(data >> DATAOFFSET)
               : INVALID_READING;
}

/**
 * Right shift the raw sensor data to isolate the field strength component
 * @return 12-bit magnetic field strength value
 */
uint16_t AS5311::getFieldStrength() {
    const uint32_t data = bitbang(false);
    return isValidReading(data) ? static_cast<uint16_t>(data >> DATAOFFSET) : INVALID_READING;
}

/**
 * Takes multiple position measurements and average them
 * @return averaged 12-bit absolute position value
 */
uint16_t AS5311::getFilteredPosition() {
    const uint16_t first = getPosition();
    if (first == INVALID_READING) {
        return INVALID_READING;
    }
    int32_t sum = first;
    for (int i = 1; i < AVERAGE_MEASUREMENTS; ++i) {
        const uint16_t position = getPosition();
        if (position == INVALID_READING) {
            return INVALID_READING; // Never average a bad serial word into a plausible position.
        }
        // 4095 and 0 are neighbours. Move every reading onto the same turn before averaging;
        // the ordinary mean would instead report a false half-range displacement near zero.
        int32_t difference = static_cast<int32_t>(position) - first;
        if (difference > 2048) {
            difference -= 4096;
        } else if (difference < -2048) {
            difference += 4096;
        }
        sum += static_cast<int32_t>(first) + difference;
    }
    int32_t average = sum / AVERAGE_MEASUREMENTS;
    if (average < 0) {
        average += 4096;
    } else if (average >= 4096) {
        average -= 4096;
    }
    return static_cast<uint16_t>(average);
}

/**
 * Record the data from the magnet sensor, process it, and add it to the manager's packet.
 */
void AS5311::measure(Manager &manager) {
    int filteredPosition = (int)getFilteredPosition();
    int rawPosition = (int)getPosition();

    recordMagnetStatus(manager);
    const uint16_t fieldStrength = getFieldStrength();
    // Keep the existing keys/order. -1 marks missing nonnegative readings; invalid displacement
    // is NaN (serialized as JSON null), because negative displacement can be a real measurement.
    manager.addData("AS5311", "mag", fieldStrength == INVALID_READING ? -1 : fieldStrength);
    manager.addData("AS5311", "pos_raw", rawPosition == INVALID_READING ? -1 : rawPosition);
    manager.addData("AS5311", "pos_avg",
                    filteredPosition == INVALID_READING ? -1 : filteredPosition);
    manager.addData("displacement", "um", measureDisplacement(rawPosition));
}

/**
 * Calculate the displacement of the magnet given a position.
 * Keeps a persistent count of sensor range overflows
 * Moving the sensor too much (about 1mm) in between calls to this function will result in invalid
 * data.
 */
float AS5311::measureDisplacement(int pos) {
    static const int WRAP_THRESHOLD = 2048;
    static const int TICKS = 4096;                   // 2^12 == 4096   see datasheet page 10
    static const float POLE_PAIR_LENGTH_UM = 2000.0; // 2mm == 2000um
    static const float UM_PER_TICK = POLE_PAIR_LENGTH_UM / TICKS;

    if (pos < 0 || pos >= TICKS) {
        return NAN; // Leave the displacement baseline and turn count untouched on a bad read.
    }
    if (initialPosition == -1) { // initial position has not been measured
        initialPosition = pos;
        lastPosition = pos;
    }
    int magnetPosition = pos;

    int difference = magnetPosition - lastPosition;
    if (difference > WRAP_THRESHOLD || difference < -WRAP_THRESHOLD) {
        if ((difference < 0 && overflows == INT_MAX) || (difference > 0 && overflows == INT_MIN)) {
            return NAN; // Do not allow the signed turn counter to overflow.
        }
        if (difference < 0) { // high to low overflow
            overflows += 1;
        } else { // low to high overflow
            overflows -= 1;
        }
    }
    lastPosition = magnetPosition;

    return ((magnetPosition - initialPosition) * UM_PER_TICK) + overflows * POLE_PAIR_LENGTH_UM;
}

/**
 * Record the alignment status of the magnet sensor
 */
void AS5311::recordMagnetStatus(Manager &manager) {
    magnetStatus status = getMagnetStatus();
    switch (status) {
    case magnetStatus::red:
        manager.addData("AS5311", "Alignment", "Red");
        break;
    case magnetStatus::yellow:
        manager.addData("AS5311", "Alignment", "Yellow");
        break;
    case magnetStatus::green:
        manager.addData("AS5311", "Alignment", "Green");
        break;
    case magnetStatus::error: // fall through
    default:
        manager.addData("AS5311", "Alignment", "Error");
        break;
    }
}
