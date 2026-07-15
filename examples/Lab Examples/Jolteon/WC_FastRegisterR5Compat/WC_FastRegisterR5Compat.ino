/**
 * Low-level SARA-R4/R5 test using a lightly modified SparkFun LTE Shield library.
 *
 * Serial Monitor:
 *   - 115200 baud
 *   - Both NL & CR
 *
 * This keeps the old OPEnS/Hypnos power-up shape visible and then uses the
 * shield library only after a raw AT sanity check. If raw AT does not return OK,
 * the problem is still power, UART routing, or level shifting, not APN/network.
 */

#define USE_ATT
// #define USE_ROGERS

// Comment this out to test the older SARA-R4 / R410M board.
#define USE_SARA_R5

#include <SparkFun_LTE_Shield_Arduino_Library.h>

#define USING_HYPNOS true
#define LTEShieldSerial Serial1

#if defined(USE_SARA_R5)
#define LTE_MODEM_BAUD 115200
#else
#define LTE_MODEM_BAUD 9600
#endif

#define LTE_3V3_RAIL_PIN 5
#define LTE_5V_RAIL_PIN 6
#define LTE_PWR_PIN A5

#if defined(USE_SARA_R5)
#define POWER_PULSE_ACTIVE_HIGH true
#else
#define POWER_PULSE_ACTIVE_HIGH false
#endif
#define POWER_PULSE_MS 1000UL
#define BOOT_WAIT_MS 10000UL

#define DEBUG_PASSTHROUGH_ENABLED

LTE_Shield lte;

#if defined(USE_ATT)
const mobile_network_operator_t MOBILE_NETWORK_OPERATOR = MNO_ATT;
#elif defined(USE_ROGERS) && defined(USE_SARA_R5)
const mobile_network_operator_t MOBILE_NETWORK_OPERATOR = MNO_GLOBAL;
#elif defined(USE_ROGERS)
const mobile_network_operator_t MOBILE_NETWORK_OPERATOR = MNO_ROGERS;
#endif

const String MOBILE_NETWORK_STRINGS[] = {
  "Default",
  "SIM_ICCD",
  "AT&T",
  "VERIZON",
  "TELSTRA",
  "T-Mobile",
  "CT",
  "Global/Rogers"
};

String registrationString[] = {
  "Not registered",
  "Registered, home",
  "Searching for operator",
  "Registration denied",
  "Registration unknown",
  "Registered, roaming",
  "Registered, home (SMS only)",
  "Registered, roaming (SMS only)",
  "Registered, home, CSFB not preferred",
  "Registered, roaming, CSFB not preferred"
};

const String APN = "hologram";

static bool rawAT(const char *command, unsigned long timeoutMs) {
  while (LTEShieldSerial.available()) {
    LTEShieldSerial.read();
  }

  Serial.print(F("HOST -> SARA: "));
  Serial.println(command);

  LTEShieldSerial.print(command);
#if defined(USE_SARA_R5)
  LTEShieldSerial.print("\r\n");
#else
  LTEShieldSerial.print("\r");
#endif

  unsigned long start = millis();
  String response = "";

  while ((millis() - start) < timeoutMs) {
    while (LTEShieldSerial.available()) {
      char c = (char)LTEShieldSerial.read();
      Serial.write(c);
      response += c;
      start = millis();
    }

    if (response.indexOf("OK") >= 0) {
      Serial.println(F("\nRaw AT status: OK seen."));
      return true;
    }

    if (response.indexOf("ERROR") >= 0) {
      Serial.println(F("\nRaw AT status: ERROR seen."));
      return false;
    }
  }

  Serial.println(F("Raw AT status: no response."));
  return false;
}

void setup() {
  struct operator_stats op;
  String currentOperator = "";
  bool newConnection = true;

  Serial.begin(115200);
  while (!Serial) {}

#if (!defined(USE_ROGERS) && !defined(USE_ATT))
  while (1) {
    Serial.println(F("You didn't specify USE_ATT or USE_ROGERS at the beginning of the INO."));
    delay(1000);
  }
#endif

  Serial.println();
#if defined(USE_SARA_R5)
  Serial.println(F("SARA-R5 low-level SparkFun-library debug starting."));
  Serial.println(F("Mode: SARA-R5 / R510S. SARA UART=115200, registration uses CEREG."));
  lte.setSaraR5Mode(true);
#else
  Serial.println(F("SARA-R4 low-level SparkFun-library debug starting."));
  Serial.println(F("Mode: SARA-R4 / R410M. SARA UART=9600, registration uses CREG."));
  lte.setSaraR5Mode(false);
#endif
  Serial.println(F("Expected hardware: A5=PWR_ON, Serial1=LTE UART."));

#if USING_HYPNOS
  powerUp();
#endif

  Serial.print(F("Opening raw Serial1 at "));
  Serial.print(LTE_MODEM_BAUD);
  Serial.println(F(" before library begin()."));
  LTEShieldSerial.begin(LTE_MODEM_BAUD);
  delay(250);

  Serial.println(F("Raw AT sanity check before library init."));
  bool atWorks = rawAT("AT", 3000);

  if (!atWorks) {
    Serial.println(F("No raw AT response. Staying in passthrough mode."));
    Serial.println(F("Meaning: this is still before SIM/APN/network. Check 3.8V, 1.8V V_INT/ref, TX/RX, level shifter, and Serial1 routing."));
    Serial.println(F("Type AT manually now if you want to keep probing."));
    return;
  }

  Serial.println(F("Initializing modified SparkFun LTE shield library..."));
  if (lte.begin(LTEShieldSerial, LTE_MODEM_BAUD)) {
    Serial.println(F("LTE shield connected through library."));
  } else {
    Serial.println(F("Unable to initialize the LTE shield through the library."));
    Serial.println(F("Raw AT worked, so this is now likely an AT-command/library compatibility issue."));
    lte.printRawCommand("", 3000);
    lte.printRawCommand("+CGMI", 3000);
    lte.printRawCommand("+CGMM", 3000);
    lte.printRawCommand("+CGMR", 3000);
    return;
  }

  lte.printRawCommand("+CPIN?", 3000);
  lte.printRawCommand("+CSQ", 3000);
#if defined(USE_SARA_R5)
  lte.printRawCommand("+CEREG?", 3000);
#else
  lte.printRawCommand("+CREG?", 3000);
#endif
  lte.printRawCommand("+CGDCONT?", 3000);

  if (lte.getOperator(&currentOperator) == LTE_SHIELD_SUCCESS) {
    Serial.print(F("Already connected to: "));
    Serial.println(currentOperator);
    Serial.println(F("Press y to force a new operator registration, or any other key to continue."));

    unsigned long waitStart = millis();
    while (!Serial.available() && (millis() - waitStart < 10000UL)) {}

    if (Serial.available() && Serial.read() != 'y') {
      newConnection = false;
    }
    while (Serial.available()) Serial.read();
  }

  if (newConnection) {
    Serial.println(F("Setting mobile-network operator profile..."));
    if (lte.setNetwork(MOBILE_NETWORK_OPERATOR)) {
      Serial.print(F("Set mobile network operator to "));
#if defined(USE_ATT)
      Serial.println(MOBILE_NETWORK_STRINGS[2]);
#else
      Serial.println(MOBILE_NETWORK_STRINGS[7]);
#endif
    } else {
      Serial.println(F("Error setting MNO profile. Continuing to APN/register diagnostics."));
      lte.printRawCommand("+UMNOPROF?", 3000);
    }

    Serial.println(F("Setting APN..."));
    if (lte.setAPN(APN, 1, LTE_Shield::PDP_TYPE_IP) == LTE_SHIELD_SUCCESS) {
      Serial.println(F("APN successfully set."));
    } else {
      Serial.println(F("Error setting APN."));
      lte.printRawCommand("+CGDCONT?", 3000);
    }

#if defined(USE_ROGERS)
    op.stat = 1;
    op.shortOp = "Rogers";
    op.longOp = "Rogers Wireless";
    op.numOp = 302320;
    op.act = 7;
#else
    op.stat = 1;
    op.shortOp = "AT&T";
    op.longOp = "AT&T";
    op.numOp = 310410;
    op.act = 7;
#endif

    Serial.println(F("Registering operator manually..."));
    if (lte.registerOperator(op) == LTE_SHIELD_SUCCESS) {
      Serial.println("Network " + op.longOp + " registered");
      printInfo();
    } else {
      Serial.println(F("Error connecting to operator."));
      lte.printRawCommand("+COPS?", 3000);
#if defined(USE_SARA_R5)
      lte.printRawCommand("+CEREG?", 3000);
#else
      lte.printRawCommand("+CREG?", 3000);
#endif
      lte.printRawCommand("+CEER", 3000);
    }
  }

  Serial.println(F("Setup done. Passthrough remains enabled."));
}

void loop() {
#ifdef DEBUG_PASSTHROUGH_ENABLED
  while (LTEShieldSerial.available()) {
    Serial.write((char)LTEShieldSerial.read());
  }
  while (Serial.available()) {
    LTEShieldSerial.write((char)Serial.read());
  }
#endif
}

void printInfo() {
  String currentApn = "";
  IPAddress ip(0, 0, 0, 0);
  String currentOperator = "";

  Serial.println(F("Connection info:"));

  if (lte.getAPN(&currentApn, &ip, 1) == LTE_SHIELD_SUCCESS) {
    Serial.println("APN: " + currentApn);
    Serial.print(F("IP: "));
    Serial.println(ip);
  }

  if (lte.getOperator(&currentOperator) == LTE_SHIELD_SUCCESS) {
    Serial.print(F("Operator: "));
    Serial.println(currentOperator);
  }

  Serial.println("RSSI: " + String(lte.rssi()));

  int regStatus = lte.registration();
  if ((regStatus >= 0) && (regStatus <= 9)) {
    Serial.println("Network registration: " + registrationString[regStatus]);
  }

  if (regStatus > 0) {
    Serial.println(F("All set. Go to the next example."));
  }
}

#if USING_HYPNOS
void powerUp() {
  Serial.println(F("Powering up LTE board."));
  Serial.println(F("Enabling Hypnos rails: pin 5 LOW for 3.3V, pin 6 HIGH for 5V."));

  pinMode(LTE_3V3_RAIL_PIN, OUTPUT);
  pinMode(LTE_5V_RAIL_PIN, OUTPUT);
  pinMode(LTE_PWR_PIN, OUTPUT);

  digitalWrite(LTE_3V3_RAIL_PIN, LOW);
  digitalWrite(LTE_5V_RAIL_PIN, HIGH);
  delay(250);

#if defined(USE_SARA_R5)
  Serial.println(F("PWR_ON pulse: A5 HIGH for 1000 ms, then LOW idle."));
  digitalWrite(LTE_PWR_PIN, HIGH);
  delay(POWER_PULSE_MS);
  digitalWrite(LTE_PWR_PIN, LOW);

  Serial.print(F("Waiting for SARA-R5 OS for "));
  Serial.print(BOOT_WAIT_MS);
  Serial.println(F(" ms."));
  delay(BOOT_WAIT_MS);
#else
  Serial.println(F("R4/Hypnos power request: A5 LOW, then wait 10000 ms."));
  digitalWrite(LTE_PWR_PIN, LOW);

  Serial.print(F("Waiting for SARA-R4 OS for "));
  Serial.print(BOOT_WAIT_MS);
  Serial.println(F(" ms."));
  delay(BOOT_WAIT_MS);
#endif
  Serial.println(F("Board power-up delay complete."));
}
#endif
