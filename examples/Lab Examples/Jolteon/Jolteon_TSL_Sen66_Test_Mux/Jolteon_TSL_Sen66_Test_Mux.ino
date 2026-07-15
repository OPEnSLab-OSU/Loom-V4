/**
 * TSL2591 + SEN66 behind a TCA9548A mux.
 *
 * Wiring:
 * SEN66   on mux channel 6, I2C address 0x6B
 * TSL2591 on mux channel 7, I2C address 0x29
 *
 * Channel 5 is disabled because this board has 5V pullups on SDA5/SCL5.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>
#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>

#include <vector>
#include <Wire.h>

#define RUN_MUX_PREFLIGHT true
#define VERBOSE_MUX_SCAN false

Manager manager("Device", 1);

std::vector<byte> muxSensorAddresses = {
    0x6B, // SEN66
    0x29  // TSL2591
};

Loom_Multiplexer mux(manager, muxSensorAddresses);

void setup() {
    manager.beginSerial();

    Serial.println();
    Serial.println(F("[SKETCH DEBUG] Booted TSL2591 + SEN66 mux sketch"));

    Wire.begin();
    Wire.setClock(100000);
    Serial.println(F("[SKETCH DEBUG] Wire started at 100 kHz"));

    mux.setDebug(true);
    mux.setScanDebug(VERBOSE_MUX_SCAN);

    // Prevent this board from ever selecting the 5V-pulled I2C mux channel.
    mux.disablePort(5);

    // Optional, but faster and safer for this board: only scan the two populated safe ports.
    mux.useOnlyPorts({6, 7});

    // These options are used when the mux auto-loads the matching sensors.
    mux.setSEN66Options(
        true,  // package PM values
        true   // package particle number concentration values
    );

    mux.setTSL2591Options(
        TSL2591_GAIN_MED,
        TSL2591_INTEGRATIONTIME_100MS
    );

    if(RUN_MUX_PREFLIGHT){
        Serial.println(F("[SKETCH DEBUG] Running mux preflight scan before Loom manager.initialize()"));
        mux.debugScan();
        Serial.println(F("[SKETCH DEBUG] Finished mux preflight scan"));
    }

    Serial.println(F("[SKETCH DEBUG] Calling manager.initialize()"));
    manager.initialize();
    Serial.println(F("[SKETCH DEBUG] manager.initialize() returned"));
}

void loop() {
    Serial.println(F("[SKETCH DEBUG] Starting measure/package/display loop"));

    manager.measure();
    Serial.println(F("[SKETCH DEBUG] manager.measure() returned"));

    manager.package();
    Serial.println(F("[SKETCH DEBUG] manager.package() returned"));

    manager.display_data();
    Serial.println(F("[SKETCH DEBUG] manager.display_data() returned"));

    // SEN66 averages 10 one-second samples internally, so this loop is already slow.
    manager.pause(5000);
}
