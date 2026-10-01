// SparkFun SEN-15901 reed-switch wind speed: one contact to pin 5, the other to GND.
// Choose an unused interrupt-capable pin; do not share its interrupt with another sensor.
// This example does not support a powered analog-output Adafruit anemometer.
#include <Loom_Manager.h>
#include <Sensors/Loom_ReedAnemometer/Loom_ReedAnemometer.h>

Manager manager("Wind", 1);
// Manufacturer calibration: 1.492 mph per closure/second, converted to metres/second.
Loom_ReedAnemometer wind(manager, 5, 1.492f * 0.44704f);

void setup() {
    manager.beginSerial();
    manager.initialize();
}

void loop() {
    manager.measure(); // Attaches the interrupt for two seconds, then detaches it.
    manager.package();
    manager.display_data();
    manager.pause(5000);
}
