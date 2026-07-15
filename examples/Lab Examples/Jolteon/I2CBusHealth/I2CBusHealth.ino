#include <Wire.h>

#if defined(PIN_WIRE_SDA)
const uint8_t I2C_SDA_PIN = PIN_WIRE_SDA;
#elif defined(SDA)
const uint8_t I2C_SDA_PIN = SDA;
#else
const uint8_t I2C_SDA_PIN = 20;
#endif

#if defined(PIN_WIRE_SCL)
const uint8_t I2C_SCL_PIN = PIN_WIRE_SCL;
#elif defined(SCL)
const uint8_t I2C_SCL_PIN = SCL;
#else
const uint8_t I2C_SCL_PIN = 21;
#endif

#define RUN_WIRE_ADDRESS_PROBE true
#define RUN_MUX_CHANNEL_PROBE true

const uint8_t MUX_ADDR = 0x70;

void printLineState(const char* label) {
    pinMode(I2C_SDA_PIN, INPUT);
    pinMode(I2C_SCL_PIN, INPUT);
    delay(5);

    Serial.print(label);
    Serial.print(" SDA=");
    Serial.print(digitalRead(I2C_SDA_PIN) == HIGH ? "HIGH" : "LOW");
    Serial.print(" SCL=");
    Serial.println(digitalRead(I2C_SCL_PIN) == HIGH ? "HIGH" : "LOW");
}

bool busLooksIdle() {
    pinMode(I2C_SDA_PIN, INPUT);
    pinMode(I2C_SCL_PIN, INPUT);
    delay(5);
    return digitalRead(I2C_SDA_PIN) == HIGH && digitalRead(I2C_SCL_PIN) == HIGH;
}

void recoverI2CBus() {
    Serial.println("[I2C DEBUG] Attempting manual I2C bus recovery");

    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    delay(5);

    if(digitalRead(I2C_SDA_PIN) == HIGH && digitalRead(I2C_SCL_PIN) == HIGH) {
        Serial.println("[I2C DEBUG] Bus already looks idle before recovery pulses");
        return;
    }

    pinMode(I2C_SCL_PIN, OUTPUT);

    for(int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(10);
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(10);
    }

    // Generate a STOP condition: SDA low while SCL high, then SDA high.
    pinMode(I2C_SDA_PIN, OUTPUT);
    digitalWrite(I2C_SDA_PIN, LOW);
    delayMicroseconds(10);

    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    delayMicroseconds(10);

    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    delay(5);

    printLineState("[I2C DEBUG] After recovery");
}

uint8_t probeAddress(uint8_t addr) {
    Serial.print("[I2C DEBUG] beginTransmission 0x");
    if(addr < 16) Serial.print('0');
    Serial.println(addr, HEX);

    Wire.beginTransmission(addr);

    Serial.print("[I2C DEBUG] endTransmission 0x");
    if(addr < 16) Serial.print('0');
    Serial.println(addr, HEX);

    uint8_t result = Wire.endTransmission();

    Serial.print("[I2C DEBUG] result 0x");
    if(addr < 16) Serial.print('0');
    Serial.print(addr, HEX);
    Serial.print(" = ");
    Serial.println(result);

    return result;
}

void selectMuxPort(uint8_t port) {
    Serial.print("[I2C DEBUG] Selecting mux port ");
    Serial.print(port);
    Serial.print(" mask 0x");
    Serial.println((uint8_t)(1 << port), HEX);

    Wire.beginTransmission(MUX_ADDR);
    Wire.write(1 << port);
    uint8_t result = Wire.endTransmission();

    Serial.print("[I2C DEBUG] mux select result = ");
    Serial.println(result);
}

void disableMuxPorts() {
    Serial.println("[I2C DEBUG] Disabling all mux ports");
    Wire.beginTransmission(MUX_ADDR);
    Wire.write(0x00);
    uint8_t result = Wire.endTransmission();
    Serial.print("[I2C DEBUG] mux disable result = ");
    Serial.println(result);
}

void setup() {
    Serial.begin(115200);

    unsigned long serialStart = millis();
    while(!Serial && millis() - serialStart < 5000) {
        delay(10);
    }

    Serial.println();
    Serial.println("[I2C DEBUG] Root I2C bus health check");
    Serial.print("[I2C DEBUG] SDA pin constant = ");
    Serial.println(I2C_SDA_PIN);
    Serial.print("[I2C DEBUG] SCL pin constant = ");
    Serial.println(I2C_SCL_PIN);

    printLineState("[I2C DEBUG] Before recovery");
    recoverI2CBus();
    printLineState("[I2C DEBUG] Before Wire.begin");

    if(!busLooksIdle()) {
        Serial.println("[I2C DEBUG] STOP: SDA or SCL is still LOW before Wire probing. Unplug downstream sensors, power-cycle, or inspect pullups/shorts.");
        return;
    }

    Wire.begin();
    Wire.setClock(100000);
    delay(50);

    Serial.println("[I2C DEBUG] Wire started at 100 kHz");
    printLineState("[I2C DEBUG] After Wire.begin");

    if(!RUN_WIRE_ADDRESS_PROBE) {
        Serial.println("[I2C DEBUG] RUN_WIRE_ADDRESS_PROBE is false, stopping before Wire.endTransmission tests");
        return;
    }

    Serial.println("[I2C DEBUG] Probing root mux addresses 0x70-0x77");
    for(uint8_t addr = 0x70; addr <= 0x77; addr++) {
        if(!busLooksIdle()) {
            Serial.println("[I2C DEBUG] STOP: bus went non-idle before address probe");
            printLineState("[I2C DEBUG] Non-idle state");
            return;
        }

        uint8_t result = probeAddress(addr);
        if(result == 0) {
            Serial.print("[I2C DEBUG] ACK at root address 0x");
            Serial.println(addr, HEX);
        }
    }

    if(!RUN_MUX_CHANNEL_PROBE) {
        return;
    }

    Serial.println("[I2C DEBUG] Probing mux channels 6 and 7 for SEN66/TSL2591");

    selectMuxPort(6);
    delay(50);
    probeAddress(0x6B);

    selectMuxPort(7);
    delay(50);
    probeAddress(0x29);

    disableMuxPorts();
    Serial.println("[I2C DEBUG] Done");
}

void loop() {
    delay(1000);
}
