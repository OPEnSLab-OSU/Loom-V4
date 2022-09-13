/**
 * K30 Example code
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_K30/Loom_K30.h>

Manager manager("Device", 1);

// Reads the battery voltage
// Manger Instance, Address Enable Warmup, Value Multiplier
//Loom_K30 k30(manager, 0x68, true, 1);
Uart kSerial(&sercom1, 12, 11, SERCOM_RX_PAD_3, UART_TX_PAD_0);

// Serial Interface for the K30
Loom_K30 k30(manager, 12, 11, true, 1);

void setup() {

  // Start the serial interface and wait for the user to open the serial monitor
  manager.beginSerial();

  //Assign pins 10 & 11 SERCOM functionality
  pinPeripheral(11, PIO_SERCOM);
  pinPeripheral(12, PIO_SERCOM);

  k30.setSerial(kSerial);

  // Initialize the manager
  manager.initialize();

  // Measure the data from the sensors
  manager.measure();

  // Package the data into JSON
  manager.package();

  // Print the JSON document to the Serial monitor
  manager.display_data();

}

void loop() {
  // put your main code here, to run repeatedly:

}

void SERCOM1_Handler(){
  kSerial.IrqHandler();
}