#pragma once

#include <Arduino.h>
#include <malloc.h>

#include "Loom_Manager.h"

#if defined(__arm__)
extern "C" char *sbrk(int increment);
#endif

/**
 * Low-overhead memory telemetry for long-running SAMD deployment tests.
 *
 * The reported gap is the distance from the current program break (sbrk(0)) to a local stack
 * marker. newlib-nano's mallinfo() adds allocator state without probing malloc/free: free blocks,
 * bytes held in holes below the top chunk, and the top free chunk that can grow into the gap.
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

        const struct mallinfo heapInfo = mallinfo();
        const int32_t topFree = heapInfo.keepcost > 0 ? heapInfo.keepcost : 0;
        latestHeapFree = heapInfo.fordblks > 0 ? heapInfo.fordblks : 0;
        latestFragmentedFree = latestHeapFree > topFree ? latestHeapFree - topFree : 0;
        latestFreeChunks = heapInfo.ordblks > 0 ? heapInfo.ordblks : 0;
        // This is the useful allocation ceiling before allowing for future stack growth: the
        // allocator's top free chunk plus as-yet-unclaimed SRAM below the current stack marker.
        latestContiguous = latestGap + topFree;

        if (!hasSample || latestGap < minimumGap)
            minimumGap = latestGap;
        if (!hasSample || latestContiguous < minimumContiguous)
            minimumContiguous = latestContiguous;

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
        Serial.print(F(" contig="));
        Serial.print(latestContiguous);
        Serial.print(F(" min_contig="));
        Serial.print(minimumContiguous);
        Serial.print(F(" heap_free="));
        Serial.print(latestHeapFree);
        Serial.print(F(" frag="));
        Serial.print(latestFragmentedFree);
        Serial.print(F(" holes="));
        Serial.print(latestFreeChunks);
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
        manager.addData("Memory", "contig", latestContiguous);
        manager.addData("Memory", "min_contig", minimumContiguous);
        manager.addData("Memory", "frag", latestFragmentedFree);
        manager.addData("Memory", "holes", latestFreeChunks);
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
    int32_t latestContiguous = 0;
    int32_t minimumContiguous = 0;
    int32_t latestHeapFree = 0;
    int32_t latestFragmentedFree = 0;
    int32_t latestFreeChunks = 0;
    uint32_t latestBreak = 0;
    bool hasSample = false;
};
