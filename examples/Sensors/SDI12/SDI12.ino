/**
 * This example uses Manager to read each discovered SDI-12 sensor and package its values.
 * Supported models: GS3, TEROS 11/12, TEROS 21, and all four depths of TEROS 54.
 * Every probe on the shared data wire needs its own address. A second factory-default address 0
 * must be changed before joining the bus; software cannot separate two probes at one address.
 * TEROS 21 reports matric potential in kPa. TEROS 54 reports calibrated VWC in m3/m3.
 * TEROS 11/12 keep Loom's legacy VWC field, whose value is the probe's calibrated ADC counts.
 * Use the manufacturer's wiring and excitation voltage; a digital pin is not the probe's supply.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Sensors/SDI12/Loom_SDI12/Loom_SDI12.h>

Manager manager("Device", 1); // Manager handles all loom simplicity

Loom_SDI12 sdi(manager, 11);

void setup() {

    // Start the serial interface
    manager.beginSerial();

    // Defaults wait 1.5 seconds after power-up and honor up to 30 seconds advertised by M!.
    // Increase startup margin before initialize() if the deployment needs it (1000-60000 ms).
    // sdi.setPowerUpDelay(2000);

    // Initialize the manager
    manager.initialize();
}

void loop() {
    // put your main code here, to run repeatedly:

    // Measure the data from the sensors
    manager.measure();

    // Package the data into JSON
    manager.package();

    // Print the JSON document to the Serial monitor
    manager.display_data();

    // Wait for 5 seconds
    manager.pause(5000);
}
