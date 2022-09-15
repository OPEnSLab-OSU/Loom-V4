/*
  Register your LTE Shield/SIM combo on a mobile network operator
  By: Jim Lindblom
  SparkFun Electronics
  Date: November 19, 2018
  License: This code is public domain but you buy me a beer if you use this 
  and we meet someday (Beerware license).
  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14997

  This example demonstrates how to initialize your Cat M1/NB-IoT shield, and
  connect it to a mobile network operator (Verizon, AT&T, T-Mobile, etc.).

  Before beginning, you may need to adjust the mobile network operator (MNO)
  setting on line 45. See comments above that line to help select either
  Verizon, T-Mobile, AT&T or others.

  You may also need to set an APN on line 51 -- e.g. "hologram"
  
  Hardware Connections:
  Attach the SparkFun LTE Cat M1/NB-IoT Shield to your Arduino
  Power the shield with your Arduino -- ensure the PWR_SEL switch is in
    the "ARDUINO" position.
*/

//Click here to get the library: http://librarymanager/All#SparkFun_LTE_Shield_Arduino_Library
#include <SparkFun_LTE_Shield_Arduino_Library.h>


// Create a LTE_Shield object to be used throughout the sketch:
LTE_Shield lte;

// To support multiple architectures, serial ports are abstracted here.
// By default, they'll support AVR's like the Arduino Uno and Redboard
// For example, on a SAMD21 board SerialMonitor can be changed to SerialUSB
// and LTEShieldSerial can be set to Serial1 (hardware serial port on 0/1)
#define SerialMonitor Serial
#define LTEShieldSerial Serial1
void printInfo();

// Network operator can be set to either:
// MNO_SW_DEFAULT -- DEFAULT
// MNO_ATT -- AT&T 
// MNO_VERIZON -- Verizon
// MNO_TELSTRA -- Telstra
// MNO_TMO -- T-Mobile
const mobile_network_operator_t MOBILE_NETWORK_OPERATOR = MNO_ATT;
const String MOBILE_NETWORK_STRINGS[] = {"Default", "SIM_ICCD", "AT&T", "VERIZON", 
  "TELSTRA", "T-Mobile", "CT"};

// Map registration status messages to more readable strings
String registrationString[] = {
  "Not registered",                         // 0
  "Registered, home",                       // 1
  "Searching for operator",                 // 2
  "Registration denied",                    // 3
  "Registration unknown",                   // 4
  "Registered, roaming",                   // 5
  "Registered, home (SMS only)",            // 6
  "Registered, roaming (SMS only)",         // 7
  "Registered, home, CSFB not preferred",   // 8
  "Registered, roaming, CSFB not prefered"  // 9
};

// APN -- Access Point Name. Gateway between GPRS MNO
// and another computer network. E.g. "hologram
const String APN = "hologram";

// This defines the size of the ops struct array. Be careful making
// this much bigger than ~5 on an Arduino Uno. To narrow the operator
// list, set MOBILE_NETWORK_OPERATOR to AT&T, Verizeon etc. instead
// of MNO_SW_DEFAULT.
#define MAX_OPERATORS 5

#define DEBUG_PASSTHROUGH_ENABLED

void setup() {
  int opsAvailable;
  struct operator_stats op;
  String currentOperator = "";
  bool newConnection = true;

  // Turn on the LTE Board
  pinMode(5, OUTPUT);                     // 3.3v power rail
  pinMode(6, OUTPUT);                     // 5v power rail
  pinMode(A5, OUTPUT);
  digitalWrite(5, LOW);
  digitalWrite(6, HIGH);
  digitalWrite(A5, LOW);
  delay(10000);
  

  SerialMonitor.begin(9600);
  while (!SerialMonitor) ; // For boards with built-in USB

  SerialMonitor.println(F("Initializing the LTE Shield..."));
  SerialMonitor.println(F("...this may take ~25 seconds if the shield is off."));
  SerialMonitor.println(F("...it may take ~5 seconds if it just turned on."));
  
  // Call lte.begin and pass it your Serial/SoftwareSerial object to 
  // communicate with the LTE Shield.
  // Note: If you're using an Arduino with a dedicated hardware serial
  // port, you may instead slide "Serial" into this begin call.
  if ( lte.begin(LTEShieldSerial, 9600) ) {
    SerialMonitor.println(F("LTE Shield connected!\r\n"));
  } else {
    SerialMonitor.println("Unable to initialize the shield.");
    while(1) ;
  }

  // First check to see if we're already connected to an operator:
  if (lte.getOperator(&currentOperator) == LTE_SHIELD_SUCCESS) {
    SerialMonitor.print(F("Already connected to: "));
    SerialMonitor.println(currentOperator);
    // If already connected provide the option to type y to connect to new operator
    SerialMonitor.println(F("Press y to connect to a new operator, or any other key to continue.\r\n"));
    while (!SerialMonitor.available()) ;
    if (SerialMonitor.read() != 'y') {
      newConnection = false;
    }
    while (SerialMonitor.available()) SerialMonitor.read();
  }

  if (newConnection) {
    // Set MNO to either Verizon, T-Mobile, AT&T, Telstra, etc.
    // This will narrow the operator options during our scan later
    SerialMonitor.println(F("Setting mobile-network operator"));
    if (lte.setNetwork(MOBILE_NETWORK_OPERATOR)) {
      SerialMonitor.print(F("Set mobile network operator to "));
      SerialMonitor.println(MOBILE_NETWORK_STRINGS[MOBILE_NETWORK_OPERATOR] + "\r\n");
    } else {
      SerialMonitor.println(F("Error setting MNO. Try cycling power to the shield/Arduino."));
      while (1) ;
    }
    
    // Set the APN -- Access Point Name -- e.g. "hologram"
    SerialMonitor.println(F("Setting APN..."));
    if (lte.setAPN(APN) == LTE_SHIELD_SUCCESS) {
      SerialMonitor.println(F("APN successfully set.\r\n"));
    } else {
      SerialMonitor.println(F("Error setting APN. Try cycling power to the shield/Arduino."));
      while (1) ;
    }

    // AT&T Operator
    op.stat = 1;
    op.shortOp = "AT&T";
    op.longOp = "AT&T";
    op.numOp = 310410;
    op.act = 7;

    if (lte.registerOperator(op) == LTE_SHIELD_SUCCESS) {
      SerialMonitor.println("Network " + op.longOp + " registered\r\n");
      // At the very end print connection information
      printInfo();
    } else {
      SerialMonitor.println(F("Error connecting to operator. Reset and try again, or try another network."));
    }
  }
}

void loop() {
  // Loop won't do much besides provide a debugging interface.
  // Pass serial data from Arduino to shield and vice-versa
#ifdef DEBUG_PASSTHROUGH_ENABLED
  if (LTEShieldSerial.available()) {
    SerialMonitor.write((char) LTEShieldSerial.read());
  }
  if (SerialMonitor.available()) {
    LTEShieldSerial.write((char) SerialMonitor.read());
  }
#endif
}

void printInfo() {
  String currentApn = "";
  IPAddress ip(0, 0, 0, 0);
  String currentOperator = "";

  SerialMonitor.println(F("Connection info:"));
  // APN Connection info: APN name and IP
  if (lte.getAPN(&currentApn, &ip) == LTE_SHIELD_SUCCESS) {
    SerialMonitor.println("APN: " + String(currentApn));
    SerialMonitor.print("IP: ");
    SerialMonitor.println(ip);
  }

  // Operator name or number
  if (lte.getOperator(&currentOperator) == LTE_SHIELD_SUCCESS) {
    SerialMonitor.print(F("Operator: "));
    SerialMonitor.println(currentOperator);
  }

  // Received signal strength
  SerialMonitor.println("RSSI: " + String(lte.rssi()));
  SerialMonitor.println();

  int regStatus = lte.registration();
  if ((regStatus >= 0) && (regStatus <= 9)) {
    SerialMonitor.println("Network registration: " + registrationString[regStatus]);
  }
  if (regStatus > 0) {
    SerialMonitor.println(F("All set. Go to the next example!"));
  }

  
}

void serialWait() {
  while (!SerialMonitor.available()) ;
  while (SerialMonitor.available()) SerialMonitor.read();
}
