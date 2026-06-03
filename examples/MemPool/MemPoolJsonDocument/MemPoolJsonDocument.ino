/**
 * MemPool-backed ArduinoJson document demo.
 *
 * This sketch creates a BasicJsonDocument that uses MemPoolJsonAllocator instead
 * of malloc/free, writes a few values, reads them back, and then lets the document
 * go out of scope so its memory returns to the pool.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <MemoryFree.h>
#include <MemPool.hpp>
#include <MemPoolJson.hpp>

MemPool pool;
bool demoRan = false;

void dumpPoolLine(const char *line, void *ctx) {
    (void)ctx;
    Serial.print(F("  "));
    Serial.println(line);
}

void printPoolStats(const char *label) {
    Serial.print(F("["));
    Serial.print(label);
    Serial.println(F("]"));
    pool.dumpStats(dumpPoolLine, nullptr);
}
void dumpLeaseLine(const char *line, void *ctx) {
    (void)ctx;
    Serial.print(F("  lease: "));
    Serial.println(line);
}

void runJsonDocumentDemo() {
    Serial.println(F("\n--- MemPool JSON document demo start ---"));
    pool.setFreeRamProvider(freeMemory);
    pool.init();
    printPoolStats("after pool.init()");

    {
        Serial.println(F("\nCreating LoomJsonDocument with 512 bytes of JSON capacity..."));
        LoomJsonDocument doc(512, MemPoolJsonAllocator(&pool));
        printPoolStats("after LoomJsonDocument construction");

        Serial.println(F("\nWriting values into the document..."));
        doc["type"] = "mempool-json-test";
        doc["count"] = 3;

        JsonObject payload = doc.createNestedObject("payload");
        payload["message"] = "hello from MemPool";
        payload["ok"] = true;

        Serial.print(F("  doc capacity: "));
        Serial.println(doc.capacity());

        Serial.print(F("  doc memory usage: "));
        Serial.println(doc.memoryUsage());

        Serial.print(F("  serialized JSON: "));
        serializeJson(doc, Serial);
        Serial.println();

        Serial.println(F("\nReading values back from the document..."));
        const char *type = doc["type"] | "(missing)";
        const char *message = doc["payload"]["message"] | "(missing)";
        bool ok = doc["payload"]["ok"] | false;
        int count = doc["count"] | -1;

        Serial.print(F("  type: "));
        Serial.println(type);

        Serial.print(F("  payload.message: "));
        Serial.println(message);

        Serial.print(F("  payload.ok: "));
        Serial.println(ok ? F("true") : F("false"));

        Serial.print(F("  count: "));
        Serial.println(count);

        Serial.println(F("\nActive leases while document is in scope:"));
        size_t active = pool.dumpActiveLeases(dumpLeaseLine, nullptr);
        Serial.print(F("  dump count: "));
        Serial.println(active);

        printPoolStats("before leaving document scope");
        Serial.println(F("\nLeaving document scope now; destructor should release the JSON lease."));
    }

    Serial.println(F("\nDocument scope ended."));
    printPoolStats("after document destruction");

    Serial.println(F("\nActive leases after document destruction:"));
    size_t active = pool.dumpActiveLeases(dumpLeaseLine, nullptr);
    if (active == 0) {
        Serial.println(F("  none"));
    }

    Serial.println(F("--- MemPool JSON document demo complete ---"));
}

void setup() {
    Serial.begin(115200);

    unsigned long start = millis();
    while (!Serial && (millis() - start < 15000)) {
        delay(10);
    }

    runJsonDocumentDemo();
    demoRan = true;
}

void loop() {
    static unsigned long lastPrint = 0;

    if (Serial && (millis() - lastPrint >= 2000)) {
        lastPrint = millis();
        Serial.println(demoRan ? F("[loop] demo complete, board still running")
                               : F("[loop] waiting for demo"));
    }
}
