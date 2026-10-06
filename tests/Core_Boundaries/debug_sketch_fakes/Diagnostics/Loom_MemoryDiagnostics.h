#pragma once
#define F(value) value
struct Loom_MemoryDiagnostics {
    void beginCycle() { ++cycleCalls; }
    void checkpoint(const char *phase, int &document, int batch) {
        ++serialCalls;
        serialPhase = phase;
        assert(document == 42 && batch == 72);
    }
};
