#pragma once

#include <Arduino.h>
#include <MemoryFree.h>

#include "Loom_Manager.h"

#if defined(__arm__)
extern "C" char *sbrk(int increment);
#endif

/**
 * Low-overhead memory telemetry for long-running SAMD deployment tests.
 *
 * Important: the reported gap is the distance from the current program break
 * (sbrk(0)) to a local stack marker. It is not the total free heap and cannot
 * see reusable holes below the program break. The program-break value is
 * included so long-term heap high-water growth can be distinguished from
 * call-stack variation.
 *
 * This helper intentionally does not probe the allocator with malloc/free.
 * Doing so immediately before MQTT could create the contiguous block under
 * test and mask a fragmentation failure.
 */
class Loom_MemoryDiagnostics {
  public:
    void beginCycle() { cycle++; }

    void checkpoint(const __FlashStringHelper *phase, DynamicJsonDocument &document,
                    int currentBatch) {
        char stackMarker;
        const uintptr_t stackAddress = reinterpret_cast<uintptr_t>(&stackMarker);
        const uintptr_t currentBreak = reinterpret_cast<uintptr_t>(sbrk(0));

        latestGap = static_cast<int32_t>(stackAddress - currentBreak);
        latestBreak = static_cast<uint32_t>(currentBreak);

        if (!hasSample || latestGap < minimumGap)
            minimumGap = latestGap;

        const int32_t delta = hasSample ? latestGap - previousGap : 0;
        previousGap = latestGap;
        hasSample = true;

        Serial.print(F("[MEM] c="));
        Serial.print(cycle);
        Serial.print(F(" ms="));
        Serial.print(millis());
        Serial.print(F(" phase="));
        Serial.print(phase);
        Serial.print(F(" gap="));
        Serial.print(latestGap);
        Serial.print(F(" min="));
        Serial.print(minimumGap);
        Serial.print(F(" delta="));
        Serial.print(delta);
        Serial.print(F(" brk=0x"));
        Serial.print(latestBreak, HEX);
        Serial.print(F(" json="));
        Serial.print(document.memoryUsage());
        Serial.print('/');
        Serial.print(document.capacity());
        Serial.print(F(" ovf="));
        Serial.print(document.overflowed() ? 1 : 0);
        Serial.print(F(" batch="));
        Serial.println(currentBatch);
    }

    /**
     * Persist the latest checkpoint in the normal sensor packet. This adds no
     * extra SD open/write cycle: Hypnos records it with the next normal log.
     * Short keys keep the diagnostic addition small on a 2000-byte JSON pool.
     */
    void addToPacket(Manager &manager, int currentBatch) {
        DynamicJsonDocument &document = manager.getDocument();
        const uint32_t jsonUsedBeforeDiagnostics =
            static_cast<uint32_t>(document.memoryUsage());
        const bool overflowedBeforeDiagnostics = document.overflowed();

        manager.addData("Memory", "cycle", cycle);
        manager.addData("Memory", "gap", latestGap);
        manager.addData("Memory", "min_ckpt", minimumGap);
        manager.addData("Memory", "brk", latestBreak);
        manager.addData("Memory", "json", jsonUsedBeforeDiagnostics);
        manager.addData("Memory", "ovf", overflowedBeforeDiagnostics ? 1 : 0);
        manager.addData("Memory", "batch", currentBatch);

        if (document.overflowed() && !overflowedBeforeDiagnostics)
            Serial.println(F("[MEM] WARNING: diagnostics overflowed the JSON document"));
    }

  private:
    uint32_t cycle = 0;
    int32_t latestGap = 0;
    int32_t minimumGap = 0;
    int32_t previousGap = 0;
    uint32_t latestBreak = 0;
    bool hasSample = false;
};
