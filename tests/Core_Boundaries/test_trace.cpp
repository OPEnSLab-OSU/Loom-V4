#define LOOM_ENABLE_TRACE 1
#define LOOM_TRACE_LINKER_HEAP_HOOKS 1
#define ARDUINO_ARCH_SAMD 1
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "../../src/Diagnostics/Loom_Trace.cpp"
#include "../../src/Diagnostics/Loom_TraceHeap.cpp"

namespace {
bool failAllocation = false;
void *allocationPointer = reinterpret_cast<void *>(uintptr_t(0x20004000));
size_t occurrences(const std::string &text, const char *needle) {
    size_t count = 0;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += strlen(needle);
    }
    return count;
}
} // namespace

extern "C" {
struct _reent {};
_reent reentrantState;
void *__real__malloc_r(_reent *, size_t) { return failAllocation ? nullptr : allocationPointer; }
void *__real__calloc_r(_reent *, size_t, size_t) {
    return failAllocation ? nullptr : allocationPointer;
}
void *__real__realloc_r(_reent *, void *, size_t) {
    return failAllocation ? nullptr : allocationPointer;
}
void __real__free_r(_reent *, void *) {}
void *__real_malloc(size_t size) { return __wrap__malloc_r(&reentrantState, size); }
void *__real_calloc(size_t count, size_t size) {
    return __wrap__calloc_r(&reentrantState, count, size);
}
void *__real_realloc(void *previous, size_t size) {
    return __wrap__realloc_r(&reentrantState, previous, size);
}
void __real_free(void *pointer) { __wrap__free_r(&reentrantState, pointer); }
}

int main() {
    // Exercise the production serializer and public/reentrant wrappers with a fake SD handle.
    {
        SDManager sd;
        Loom_Trace trace;
        assert(trace.begin(sd, true));
        assert(trace.isHeapCaptureEnabled());
        trace.setStorageAvailable(false);
        const unsigned int before = sd.batches;
        for (size_t i = 0; i < Loom_Trace::EVENT_CAPACITY + 2; ++i) {
            __wrap_malloc(8);
        }
        for (size_t i = 0; i < Loom_Trace::EVENT_CAPACITY; ++i) trace.marker("while SD is off");
        assert(!trace.enter("wake.cpp", "void beforeSDRestore()", 3));
        assert(!trace.flush());
        assert(trace.isRecording() && sd.batches == before);
        trace.setStorageAvailable(true);
        assert(trace.flush());
        assert(sd.saved.find("\"kind\":\"lost\"") != std::string::npos);
        assert(sd.saved.find("\"loss_flags\":18") != std::string::npos); // Calls/markers can be lost; heap traffic is intentionally paused.
        assert(sd.saved.find("\"kind\":\"L\"") != std::string::npos);
        assert(sd.perfetto.find("LOST TRACE EVENTS") != std::string::npos);
        std::cout << "PASS SD power-off never drains; wake resumes with explicit loss\n";
    }
    {
        SDManager sd;
        sd.debugName = "WispBench_"; // Custom prefix goes through the production path builder.
        sd.existing.insert("/debug/WispBench_trace_1.ndjson");
        sd.sessionNumber = 2; // Shared SD session selection, not a separate trace counter.
        Loom_Trace trace;
        traceFake::ms = 10;
        traceFake::fraction = 125;
        assert(trace.begin(sd, true));
        assert(std::string(trace.getRecordPath()) == "/debug/WispBench_trace_2.ndjson");
        assert(std::string(trace.getPerfettoPath()) == "/debug/WispBench_trace_2.perfetto.json");
        assert(sd.saved.find("\"session_number\":2") != std::string::npos);
        assert(sd.saved.find("\"heap_hooks\":true") != std::string::npos);
        assert(trace.enter("source.cpp", "void Example::run()", 12, allocationPointer));
        __wrap_malloc(128);
        __wrap_free(allocationPointer);
        failAllocation = true;
        __wrap_realloc(allocationPointer, 1024);
        failAllocation = false;
        __wrap_realloc(allocationPointer, 0);
        trace.label("quote\" slash\\ newline\n", allocationPointer);
        char transient[] = "SHT31_2";
        trace.object(transient, allocationPointer, 0, reinterpret_cast<void *>(uintptr_t(0x20001000)), 2, 0x44);
        transient[0] = 'X'; // Mutating/deleting a module cannot invalidate an unsaved copied name.
        trace.retireObject(allocationPointer);
        trace.memory("After sensor test");
        trace.value("RTC UTC", INT64_C(2200000000), "UTC epoch seconds");
        trace.value("RTC invalid", -1, "seconds");
        trace.leave();
        assert(trace.flush());
        assert(sd.saved.find("SHT31_2") != std::string::npos);
        assert(sd.saved.find("XHT31_2") == std::string::npos);
        assert(sd.perfetto.find("\"ph\":\"B\"") != std::string::npos);
        assert(sd.perfetto.find("\"ph\":\"E\"") != std::string::npos);
        assert(sd.perfetto.find("\"ph\":\"C\"") != std::string::npos);
        assert(sd.perfetto.find("\"Value\":2200000000") != std::string::npos);
        assert(sd.perfetto.find("\"Value\":-1") != std::string::npos);
        assert(sd.perfetto.find("Available RAM estimate bytes") != std::string::npos);
        assert(sd.perfetto.substr(sd.perfetto.size() - 4) == "]}\r\n");
        const char *tempFolder = std::getenv("TEMP");
        if (tempFolder) {
            const std::string prefix = std::string(tempFolder) + "/loom-native-trace-test";
            std::ofstream(prefix + ".perfetto.json", std::ios::binary) << sd.perfetto;
            std::ofstream(prefix + ".ndjson", std::ios::binary) << sd.saved;
        }
        assert(occurrences(sd.saved, "\"kind\":\"A\"") == 1); // No double _malloc_r event.
        assert(occurrences(sd.saved, "\"kind\":\"F\"") == 1);
        assert(occurrences(sd.saved, "\"kind\":\"N\"") == 1);
        assert(occurrences(sd.saved, "\"kind\":\"Z\"") == 1);
        assert(sd.saved.find("quote\\\" slash\\\\ newline\\u000a") != std::string::npos);
        assert(sd.saved.find("\"ts\":10125") != std::string::npos);
        std::cout << "PASS trace serializer, wrappers, source/object fields, escaping\n";
    }
    {
        SDManager sd;
        Loom_Trace trace;
        assert(trace.begin(sd, true));
        assert(trace.flush());
        const unsigned int before = sd.batches;
        for (size_t i = 0; i < 10000; ++i) {
            __wrap_malloc(i);
        }
        assert(sd.batches == before); // An allocator burst must never write to SD.
        trace.marker("after burst"); // A safe boundary resumes the bounded allocation window.
        assert(sd.batches == before + 1);
        assert(sd.saved.find("\"kind\":\"lost\"") == std::string::npos);
        assert(occurrences(sd.saved, "\"kind\":\"A\"") == 16);
        assert(occurrences(sd.saved, "\"kind\":\"L\"") == 1);
        assert(sd.saved.find("\"size\":9984") != std::string::npos);
        assert(sd.perfetto.find("\"Skipped heap events (capture limit)\":9984") != std::string::npos);
        __wrap_free(allocationPointer);
        __wrap_malloc(99);
        assert(trace.flush());
        assert(occurrences(sd.saved, "\"kind\":\"A\"") == 17);
        assert(occurrences(sd.saved, "\"kind\":\"F\"") == 1);
        if (const char *tempFolder = std::getenv("TEMP")) {
            const std::string prefix = std::string(tempFolder) + "/loom-bounded-native-trace-test";
            std::ofstream(prefix + ".ndjson", std::ios::binary) << sd.saved;
            std::ofstream(prefix + ".perfetto.json", std::ios::binary) << sd.perfetto;
        }
        std::cout << "PASS 10,000-event burst capped at 16, zero queue drops/SD in hooks, capture resumes\n";
    }
    {
        SDManager sd;
        Loom_Trace trace;
        assert(trace.begin(sd, true, 4));
        assert(trace.flush());
        for (unsigned int i = 0; i < 100; ++i) __wrap_malloc(1);
        assert(trace.flush());
        assert(occurrences(sd.saved, "\"kind\":\"A\"") == 4);
        assert(sd.saved.find("\"size\":96") != std::string::npos);
        assert(sd.saved.find("\"kind\":\"lost\"") == std::string::npos);
        std::cout << "PASS sketch-selected allocation window limit\n";
    }
    {
        // Regression: a call entry used to fill the final slot before a modem burst.
        SDManager sd;
        Loom_Trace trace;
        assert(trace.begin(sd, true, 23));
        assert(trace.flush());
        for (unsigned int i = 0; i < 22; ++i) trace.marker("queued call diagnostics");
        assert(trace.enter("modem.cpp", "void modemBurst()", 1));
        const unsigned int before = sd.batches;
        for (unsigned int i = 0; i < 10000; ++i) __wrap_malloc(1);
        assert(sd.batches == before);
        trace.leave();
        assert(trace.flush());
        assert(sd.saved.find("\"kind\":\"lost\"") == std::string::npos);
        assert(occurrences(sd.saved, "\"kind\":\"L\"") == 1);
        assert(occurrences(sd.saved, "\"kind\":\"E\"") == 1);
        std::cout << "PASS call entries reserve the heap-pause slot before a full-queue burst\n";
    }
    {
        SDManager sd;
        Loom_Trace trace;
        assert(trace.begin(sd, true));
        assert(trace.flush());
        traceFake::interrupt = 1;
        assert(!trace.enter("isr.cpp", "void interrupt()", 1));
        __wrap_malloc(123);
        traceFake::interrupt = 0;
        assert(trace.flush());
        assert(sd.saved.find("isr.cpp") == std::string::npos);
        assert(sd.saved.find("\"kind\":\"A\"") == std::string::npos);
        std::cout << "PASS interrupt events excluded\n";
    }
    {
        SDManager sd;
        Loom_Trace trace;
        assert(trace.begin(sd, true));
        sd.failPerfetto = true;
        assert(!trace.flush());
        assert(!trace.isRecording());
        assert(sd.saved.find("capture baseline") != std::string::npos);
        std::cout << "PASS companion failure stops capture and preserves NDJSON prefix\n";
    }
    {
        SDManager sd;
        Loom_Trace trace;
        assert(trace.begin(sd, true));
        assert(trace.flush());
        sd.limit = 15;
        trace.marker("failed write");
        assert(!trace.flush());
        const unsigned int attempts = sd.batches;
        assert(!trace.enter("x.cpp", "void later()", 1));
        __wrap_malloc(24);
        assert(!trace.flush());
        assert(sd.batches == attempts);
        assert(!trace.isRecording());
        std::cout << "PASS partial writer failure stops capture without retry loops\n";
    }
    {
        SDManager sd;
        Loom_Trace trace;
        traceFake::ms = UINT32_MAX;
        traceFake::fraction = 500;
        assert(trace.begin(sd));
        assert(!trace.isHeapCaptureEnabled()); // Calls-only sessions must not advertise allocation capture.
        assert(trace.flush());
        traceFake::ms = 0;
        traceFake::fraction = 125;
        trace.marker("after clock rollover");
        assert(trace.flush());
        assert(sd.saved.find("\"ts\":4294967296125") != std::string::npos);
        std::cout << "PASS millis rollover extended, micros fraction preserved\n";
    }
    std::cout << "Trace Event bytes on this host: " << sizeof(Loom_Trace::Event) << '\n';
    for (const char *collision : {"/debug/Wisp_trace_0.ndjson", "/debug/Wisp_trace_0.perfetto.json"}) {
        SDManager sd;
        sd.existing.insert(collision);
        Loom_Trace trace;
        assert(!trace.begin(sd)); // Never silently renumber, append or overwrite an old capture.
        assert(sd.saved.empty() && sd.perfetto.empty());
    }
    assert(allocatorDepth == 0);
}
