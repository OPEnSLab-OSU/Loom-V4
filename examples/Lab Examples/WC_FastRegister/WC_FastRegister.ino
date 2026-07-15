/**
 * Automatically Register the LTE board to the standard AT&T network with the CODE: 310410
 * 
 * @author Will Richards
 */ 

/** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Before doing anything make sure to uncomment one of the following to enable the specific carrier !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!**/
//#define USE_ROGERS
//#define USE_ATT

#include <SparkFun_LTE_Shield_Arduino_Library.h>

// Enables power up of module when connect to a Hypnos board
#define USING_HYPNOS true
#define LTEShieldSerial Serial1

// Create a LTE_Shield object to be used throughout the sketch:
LTE_Shield lte;


// Network operator can be set to either:
// MNO_ATT -- AT&T 
// MNO_ROGERS -- Rogers

#if defined(USE_ATT)
  const mobile_network_operator_t MOBILE_NETWORK_OPERATOR = MNO_ATT;
#elif defined(USE_ROGERS)
  const mobile_network_operator_t MOBILE_NETWORK_OPERATOR = MNO_ROGERS;
#endif

const String MOBILE_NETWORK_STRINGS[] = {"Default", "SIM_ICCD", "AT&T", "VERIZON", 
  "TELSTRA", "T-Mobile", "CT", "Rogers"};

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

#define DEBUG_PASSTHROUGH_ENABLED

void setup() {
  int opsAvailable;
  struct operator_stats op;
  String currentOperator = "";
  bool newConnection = true;

  Serial.begin(9600);
  while (!Serial) ; // For boards with built-in USB

  #if (!defined(USE_ROGERS) and !defined(USE_ATT))
  while(1){
    Serial.println("You didn't specify USE_ATT or USE_ROGERS at the beginning of the INO....hanging");
  }
  #endif

  if(USING_HYPNOS){
    // Turn the board on
    powerUp();
  }
  

  Serial.println(F("Initializing the LTE Shield..."));
  Serial.println(F("...this may take ~25 seconds if the shield is off."));
  Serial.println(F("...it may take ~5 seconds if it just turned on."));
  
  // Open a serial connection with the LTE board
  if ( lte.begin(LTEShieldSerial, 9600) ) {
    Serial.println(F("LTE Shield connected!\r\n"));
  } else {
    Serial.println("Unable to initialize the shield.");
    while(1) ;
  }

  // First check to see if we're already connected to an operator:
  if (lte.getOperator(&currentOperator) == LTE_SHIELD_SUCCESS) {
    Serial.print(F("Already connected to: "));
    Serial.println(currentOperator);
    // If already connected provide the option to type y to connect to new operator
    Serial.println(F("Press y to connect to a new operator, or any other key to continue.\r\n"));
    while (!Serial.available()) ;
    if (Serial.read() != 'y') {
      newConnection = false;
    }
    while (Serial.available()) Serial.read();
  }

  if (newConnection) {
    // Set MNO to either Verizon, T-Mobile, AT&T, Telstra, etc.
    // This will narrow the operator options during our scan later
    Serial.println(F("Setting mobile-network operator"));
    if (lte.setNetwork(MOBILE_NETWORK_OPERATOR)) {
      Serial.print(F("Set mobile network operator to "));

      // Use ternary to pick the right name
      Serial.println(MOBILE_NETWORK_STRINGS[(MOBILE_NETWORK_OPERATOR != 43) ? MOBILE_NETWORK_OPERATOR : 7] + "\r\n");
    } else {
      Serial.println(F("Error setting MNO. Try cycling power to the shield/Arduino."));
      while (1) ;
    }
    
    // Set the APN -- Access Point Name -- e.g. "hologram"
    Serial.println(F("Setting APN..."));
    if (lte.setAPN(APN) == LTE_SHIELD_SUCCESS) {
      Serial.println(F("APN successfully set.\r\n"));
    } else {
      Serial.println(F("Error setting APN. Try cycling power to the shield/Arduino."));
      while (1) ;
    }

    // Manually create an operator that matches our either or standard AT&T operator (US) or Rogers (CA)
    /**
     * Operator Format (* indicate what should always be used)
     * <stat> Number 
     *    • 0: unknown
     *    • 1: available*
     *    • 2: current
     *    • 3: forbidden
     * 
     * 
     * <shortOp & longOp> These aren't really used to register the sim
     * <numOp>  - This is the most important it tells the board what network we will be connecting to, this number can be made by combining the MCC and MNC into one number, list here https://mcc-mnc-list.com/list
     * <Act> Indicates the radio access technology:
     *    • 0: GSM
     *    • 3: GSM/GPRS with EDGE availability
     *    • 7: LTE*
     *    • 8: EC-GSM-IoT (A/Gb mode)
     *    • 9: E-UTRAN (NB-S1 mode)
     *
     */
    if(MOBILE_NETWORK_OPERATOR == 43){
      op.stat = 1;
      op.shortOp = "Rogers";
      op.longOp = "Rogers Wireless";
      op.numOp = 302320;
      op.act = 7;
    }else{
      op.stat = 1;
      op.shortOp = "AT&T";
      op.longOp = "AT&T";
      op.numOp = 310410;
      op.act = 7;
    }
    

    if (lte.registerOperator(op) == LTE_SHIELD_SUCCESS) {
      Serial.println("Network " + op.longOp + " registered\r\n");

      // At the very end print connection information
      printInfo();
    } else {
      Serial.println(F("Error connecting to operator. Reset and try again, or try another network."));
    }
  }
}

void loop() {
  // Loop won't do much besides provide a debugging interface.
  // Pass serial data from Arduino to shield and vice-versa
#ifdef DEBUG_PASSTHROUGH_ENABLED
  if (LTEShieldSerial.available()) {
    Serial.write((char) LTEShieldSerial.read());
  }
  if (Serial.available()) {
    LTEShieldSerial.write((char) Serial.read());
  }
#endif
}

void printInfo() {
  String currentApn = "";
  IPAddress ip(0, 0, 0, 0);
  String currentOperator = "";

  Serial.println(F("Connection info:"));

  // APN Connection info: APN name and IP
  if (lte.getAPN(&currentApn, &ip) == LTE_SHIELD_SUCCESS) {
    Serial.println("APN: " + String(currentApn));
    Serial.print("IP: ");
    Serial.println(ip);
  }

  // Operator name or number
  if (lte.getOperator(&currentOperator) == LTE_SHIELD_SUCCESS) {
    Serial.print(F("Operator: "));
    Serial.println(currentOperator);
  }

  // Received signal strength
  Serial.println("RSSI: " + String(lte.rssi()));
  Serial.println();

  // Check if the LTE board is actually registered to the network
  int regStatus = lte.registration();

  if ((regStatus >= 0) && (regStatus <= 9)) {
    Serial.println("Network registration: " + registrationString[regStatus]);
  }
  if (regStatus > 0) {
    Serial.println(F("All set. Go to the next example!"));
  }
}

#if(USING_HYPNOS == true)
  /**
   * Turn the LTE Board On
   */ 
  void powerUp(){
    // Turn on the LTE Board, Manually power the hypnos rails on
    Serial.println("Powering up LTE Board, this will take about 10 seconds...");
    pinMode(5, OUTPUT);                     // 3.3v power rail
    pinMode(6, OUTPUT);                     // 5v power rail
    pinMode(A5, OUTPUT);                    // LTE Power Pin
    digitalWrite(5, LOW);                   // Enable the 3.3v Rail
    digitalWrite(6, HIGH);                  // Enable the 5v Rail
    digitalWrite(A5, LOW);                  // Request that the board power on
    delay(10000);
    Serial.println("Board power up complete!");
  }
#endif