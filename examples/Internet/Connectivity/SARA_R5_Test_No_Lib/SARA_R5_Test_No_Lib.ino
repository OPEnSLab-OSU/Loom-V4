/**
 * Raw SARA-R5 UART bridge with no Loom manager and no TinyGSM.
 * Use this to prove hardware power/UART before debugging the Loom library.
 * Serial Monitor: 115200 baud, Both NL & CR.
 */

#define MODEM_TX_RX_BAUD 115200

#define LTE_PWR_PIN A5
#define LTE_3V3_RAIL_PIN 5
#define LTE_5V_RAIL_PIN 6

// Leave rail control off unless this bare sketch is the only thing powering LTE.
// If the Feather resets during SARA boot, keep this 0 and let the board power path own the rails.
#define LTE_ENABLE_RAIL_CONTROL 0

// OPEnS/Jolteon notes say A5 HIGH for about 1 second, then LOW.
// Flip this to 0 only when the SARA 1.8V V_INT/ref rail never appears.
#define LTE_A5_PULSE_ACTIVE_HIGH 1

#define LTE_PWR_PULSE_MS 1000UL
#define LTE_BOOT_WAIT_MS 10000UL
#define LTE_AUTO_AT_INTERVAL_MS 2000UL

unsigned long lastAT = 0;

void printSaraByte(int b) {
  Serial.print(F("SARA -> HOST byte: 0x"));
  if (b < 16) {
    Serial.print('0');
  }
  Serial.print(b, HEX);
  Serial.print(F(" '"));

  if (b >= 32 && b <= 126) {
    Serial.write((char)b);
  } else if (b == '\r') {
    Serial.print(F("\\r"));
  } else if (b == '\n') {
    Serial.print(F("\\n"));
  } else {
    Serial.print('.');
  }

  Serial.println(F("'"));
}

void pulsePowerPin() {
  pinMode(LTE_PWR_PIN, OUTPUT);

#if LTE_A5_PULSE_ACTIVE_HIGH
  Serial.println(F("Pulsing A5 HIGH for 1000 ms, then LOW idle."));
  digitalWrite(LTE_PWR_PIN, HIGH);
  delay(LTE_PWR_PULSE_MS);
  digitalWrite(LTE_PWR_PIN, LOW);
#else
  Serial.println(F("Pulsing A5 LOW for 1000 ms, then HIGH idle."));
  digitalWrite(LTE_PWR_PIN, LOW);
  delay(LTE_PWR_PULSE_MS);
  digitalWrite(LTE_PWR_PIN, HIGH);
#endif
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println();
  Serial.println(F("Raw SARA-R5 auto-AT passthrough starting."));
  Serial.println(F("Serial Monitor: 115200 baud, Both NL & CR."));
  Serial.println(F("SARA UART: Serial1 @ 115200."));

  pinMode(LED_BUILTIN, OUTPUT);

#if LTE_ENABLE_RAIL_CONTROL
  Serial.println(F("Enabling LTE rails from this sketch."));
  pinMode(LTE_3V3_RAIL_PIN, OUTPUT);
  pinMode(LTE_5V_RAIL_PIN, OUTPUT);
  digitalWrite(LTE_3V3_RAIL_PIN, LOW);
  digitalWrite(LTE_5V_RAIL_PIN, HIGH);
  delay(250);
#else
  Serial.println(F("LTE rail pin control disabled in this sketch."));
#endif

  pulsePowerPin();

  Serial.print(F("Waiting "));
  Serial.print(LTE_BOOT_WAIT_MS);
  Serial.println(F(" ms for SARA-R5 OS."));

  for (uint8_t i = 0; i < (LTE_BOOT_WAIT_MS / 1000UL); i++) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    Serial.print(F("wait second "));
    Serial.println(i + 1);
    delay(1000);
  }

  Serial1.begin(MODEM_TX_RX_BAUD);

  Serial.println(F("Bridge ready. Auto-sending AT every 2 seconds."));
  Serial.println(F("Expected good response: OK."));
}

void loop() {
  if (millis() - lastAT > LTE_AUTO_AT_INTERVAL_MS) {
    lastAT = millis();
    Serial.println(F("HOST -> SARA: AT"));
    Serial1.print("AT\r\n");
  }

  while (Serial1.available()) {
    printSaraByte(Serial1.read());
  }

  while (Serial.available()) {
    Serial1.write(Serial.read());
  }
}
